/** 
 * @file audioengine_FMODEX.cpp
 * @brief Implementation of LLAudioEngine class abstracting the audio support as a FMOD 3D implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llstreamingaudio.h"
#include "llstreamingaudio_fmodex.h"

#include "llaudioengine_fmodex.h"
#include "lllistener_fmodex.h"

#include "llerror.h"
#include "llmath.h"
#include "llrand.h"

#include "fmod.hpp"
#include "fmod_errors.h"
#include "lldir.h"
#include "llapr.h"

#include "sound_ids.h"

#if LL_WINDOWS	//Some ugly code to make missing fmodex.dll not cause a fatal error.
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <DelayImp.h>
#pragma comment(lib, "delayimp.lib")

bool attemptDelayLoad()
{
	__try
	{
		if( FAILED( __HrLoadAllImportsForDll( "fmodex.dll" ) ) )
			return false;
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		return false;
	}
	return true;
}
#endif

FMOD_RESULT F_CALLBACK windCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int outchannels);

LLAudioEngine_FMODEX::LLAudioEngine_FMODEX(bool enable_profiler)
{
	mInited = false;
	mWindGen = NULL;
	mWindDSP = NULL;
	mSystem = NULL;
	mEnableProfiler = enable_profiler;
}


LLAudioEngine_FMODEX::~LLAudioEngine_FMODEX()
{
}


inline bool Check_FMOD_Error(FMOD_RESULT result, const char *string)
{
	if(result == FMOD_OK)
		return false;
	llwarns << string << " Error: " << FMOD_ErrorString(result) << llendl;
	return true;
}

bool LLAudioEngine_FMODEX::init(const S32 num_channels, void* userdata)
{

#if LL_WINDOWS
	if(!attemptDelayLoad())
		return false;
#endif

	U32 version;
	FMOD_RESULT result;
	int numdrivers;
	FMOD_SPEAKERMODE speakermode;
	FMOD_CAPS caps;
	char name[256];

	LL_DEBUGS("AppInit") << "LLAudioEngine_FMODEX::init() initializing FMOD" << LL_ENDL;

	result = FMOD::System_Create(&mSystem);
	if(Check_FMOD_Error(result, "FMOD::System_Create"))
		return false;

	//will call LLAudioEngine_FMODEX::allocateListener, which needs a valid mSystem pointer.
	LLAudioEngine::init(num_channels, userdata);	
	
	result = mSystem->getVersion(&version);
	Check_FMOD_Error(result, "FMOD::System::getVersion");

	if (version < FMOD_VERSION)
	{
		LL_WARNS("AppInit") << "Error : You are using the wrong FMOD version (" << version
			<< ")!  You should be using FMOD " << FMOD_VERSION << LL_ENDL;
	}

#if LL_WINDOWS
	//Is this block applicable to linux?
	{
		result = mSystem->getNumDrivers(&numdrivers);
		Check_FMOD_Error(result, "FMOD::System::getNumDrivers");
		if (numdrivers == 0)
		{
			result = mSystem->setOutput(FMOD_OUTPUTTYPE_NOSOUND);
			Check_FMOD_Error(result, "FMOD::System::setOutput");
		}
		else
		{
			result = mSystem->getDriverCaps(0, &caps, 0, &speakermode);
			Check_FMOD_Error(result,"FMOD::System::getDriverCaps");
			/*
			Set the user selected speaker mode.
			*/
			result = mSystem->setSpeakerMode(speakermode);
			Check_FMOD_Error(result, "FMOD::System::setSpeakerMode");
			if (caps & FMOD_CAPS_HARDWARE_EMULATED)
			{
				/*
				The user has the 'Acceleration' slider set to off! This is really bad
				for latency! You might want to warn the user about this.
				*/
				result = mSystem->setDSPBufferSize(1024, 10);
				Check_FMOD_Error(result, "FMOD::System::setDSPBufferSize");
			}
			result = mSystem->getDriverInfo(0, name, 256, 0);
			Check_FMOD_Error(result, "FMOD::System::getDriverInfo");

			if (strstr(name, "SigmaTel"))
			{
				/*
				Sigmatel sound devices crackle for some reason if the format is PCM 16bit.
				PCM floating point output seems to solve it.
				*/
				result = mSystem->setSoftwareFormat(48000, FMOD_SOUND_FORMAT_PCMFLOAT, 0,0, FMOD_DSP_RESAMPLER_LINEAR);
				Check_FMOD_Error(result,"FMOD::System::setSoftwareFormat");
			}
		}
	}
#endif //LL_WINDOWS

	// In this case, all sounds, PLUS wind and stream will be software.
	result = mSystem->setSoftwareChannels(num_channels+2);
	Check_FMOD_Error(result,"FMOD::System::setSoftwareChannels");

	U32 fmod_flags = FMOD_INIT_NORMAL;
	if(mEnableProfiler)
		fmod_flags |= FMOD_INIT_ENABLE_PROFILE;

#if LL_LINUX
	// If we don't set an output method, Linux FMOD always
	// decides on OSS and fails otherwise.  So we'll manually
	// try ESD, then OSS, then ALSA.
	// Why this order?  See SL-13250, but in short, OSS emulated
	// on top of ALSA is ironically more reliable than raw ALSA.
	// Ack, and ESD has more reliable failure modes - but has worse
	// latency - than all of them, so wins for now.
	bool audio_ok = false;

	if (!audio_ok)
	{
		if (NULL == getenv("LL_BAD_FMODEX_ESD")) /*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying ESD audio output..." << LL_ENDL;
			if(mSystem->SetOutput(FMOD_OUTPUTTYPE_ESD) == FMOD_OK &&
				(result = mSystem->init(num_channels, fmod_flags, 0)) == FMOD_OK)
			{
				LL_DEBUGS("AppInit") << "ESD audio output initialized OKAY"	<< LL_ENDL;
				audio_ok = true;
			}
			else 
			{
				Check_FMOD_Error(result, "ESD audio output FAILED to initialize");
			}
		} 
		else 
		{
			LL_DEBUGS("AppInit") << "ESD audio output SKIPPED" << LL_ENDL;
		}	
	}
	if (!audio_ok)
	{
		if (NULL == getenv("LL_BAD_FMODEX_OSS")) 	 /*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying OSS audio output..." << LL_ENDL;
			if(mSystem->SetOutput(FMOD_OUTPUTTYPE_OSS) == FMOD_OK &&
			   (result = mSystem->init(num_channels, fmod_flags, 0)) == FMOD_OK)
			{
				LL_DEBUGS("AppInit") << "OSS audio output initialized OKAY" << LL_ENDL;
				audio_ok = true;
			} 
			else 
			{
				Check_FMOD_Error(result, "OSS audio output FAILED to initialize" << LL_ENDL;
			}
		} 
		else 
		{
			LL_DEBUGS("AppInit") << "OSS audio output SKIPPED" << LL_ENDL;
		}
	}
	if (!audio_ok)
	{
		if (NULL == getenv("LL_BAD_FMODEX_ALSA"))		/*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying ALSA audio output..." << LL_ENDL;
			if(mSystem->SetOutput(FMOD_OUTPUTTYPE_ALSA) &&
			   (result = mSystem->init(num_channels, fmod_flags, 0)) == FMOD_OK)
			{
				LL_DEBUGS("AppInit") << "ALSA audio output initialized OKAY" << LL_ENDL;
				audio_ok = true;
			}
			else
			{
				Check_FMOD_Error(result, "ALSA audio output FAILED to initialize");
			}
		} else 
		{
			LL_DEBUGS("AppInit") << "OSS audio output SKIPPED" << LL_ENDL;
		}
	}
	if (!audio_ok)
	{
		LL_WARNS("AppInit") << "Overall audio init failure." << LL_ENDL;
		return false;
	}

	// On Linux, FMOD causes a SIGPIPE for some netstream error
	// conditions (an FMOD bug); ignore SIGPIPE so it doesn't crash us.
	// NOW FIXED in FMOD 3.x since 2006-10-01.
	//signal(SIGPIPE, SIG_IGN);

	// We're interested in logging which output method we
	// ended up with, for QA purposes.
	FMOD_OUTPUTTYPE output_type;
	mSystem->getOutput(output_type);
	switch (output_type)
	{
		case FSOUND_OUTPUT_NOSOUND: 
			LL_DEBUGS("AppInit") << "Audio output: NoSound" << LL_ENDL; break;
		case FSOUND_OUTPUT_OSS:	
			LL_DEBUGS("AppInit") << "Audio output: OSS" << LL_ENDL; break;
		case FSOUND_OUTPUT_ESD:	
			LL_DEBUGS("AppInit") << "Audio output: ESD" << LL_ENDL; break;
		case FSOUND_OUTPUT_ALSA: 
			LL_DEBUGS("AppInit") << "Audio output: ALSA" << LL_ENDL; break;
		default:
			LL_INFOS("AppInit") << "Audio output: Unknown!" << LL_ENDL; break;
	};
#else // LL_LINUX

	// initialize the FMOD engine
	result = mSystem->init( num_channels + 2, fmod_flags, 0);
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		/*
		Ok, the speaker mode selected isn't supported by this soundcard. Switch it
		back to stereo...
		*/
		result = mSystem->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
		Check_FMOD_Error(result,"Error falling back to stereo mode");
		/*
		... and re-init.
		*/
		result = mSystem->init(100, FMOD_INIT_NORMAL, 0);
	}
	if(Check_FMOD_Error(result, "Error initializing FMOD"))
		return false;
#endif

	// set up our favourite FMOD-native streaming audio implementation if none has already been added
	if (!getStreamingAudioImpl()) // no existing implementation added
		setStreamingAudioImpl(new LLStreamingAudio_FMODEX(mSystem));

	LL_DEBUGS("AppInit") << "LLAudioEngine_FMODEX::init() FMOD initialized correctly" << LL_ENDL;

	mInited = true;

	return true;
}


std::string LLAudioEngine_FMODEX::getDriverName(bool verbose)
{
	llassert_always(mSystem);
	if (verbose)
	{
		U32 version;
		if(!Check_FMOD_Error(mSystem->getVersion(&version), "FMOD::System::getVersion"))
		{
			return llformat("FMOD version %1x.%02x.%02x", version >> 16, version >> 8 & 0x000000FF, version & 0x000000FF);
		}
	}
	return "FMOD";
}


void LLAudioEngine_FMODEX::allocateListener(void)
{	
	mListenerp = (LLListener *) new LLListener_FMODEX(mSystem);
	if (!mListenerp)
	{
		llwarns << "Listener creation failed" << llendl;
	}
}


void LLAudioEngine_FMODEX::shutdown()
{
	stopInternetStream();

	LLAudioEngine::shutdown();
	
	llinfos << "LLAudioEngine_FMODEX::shutdown() closing FMOD" << llendl;
	mSystem->close();
	mSystem->release();
	llinfos << "LLAudioEngine_FMODEX::shutdown() done closing FMOD" << llendl;

	delete mListenerp;
	mListenerp = NULL;
}


LLAudioBuffer * LLAudioEngine_FMODEX::createBuffer()
{
	return new LLAudioBufferFMODEX(mSystem);
}


LLAudioChannel * LLAudioEngine_FMODEX::createChannel()
{
	return new LLAudioChannelFMODEX(mSystem);
}

bool LLAudioEngine_FMODEX::initWind()
{
	mNextWindUpdate = 0.0;

	if (!mWindDSP)
	{
		FMOD_DSP_DESCRIPTION dspdesc;
		memset(&dspdesc, 0, sizeof(FMOD_DSP_DESCRIPTION));	//Set everything to zero
		strncpy(dspdesc.name,"Wind Unit", sizeof(dspdesc.name));	//Set name to "Wind Unit"
		dspdesc.read = &windCallback; //Assign callback.
		if(Check_FMOD_Error(mSystem->createDSP(&dspdesc, &mWindDSP), "FMOD::createDSP"))
			return false;

		if(mWindGen)
			delete mWindGen;
	
		float frequency = 44100;
		mWindDSP->getDefaults(&frequency,0,0,0);
		mWindGen = new LLWindGen<MIXBUFFERFORMAT>((U32)frequency);
		mWindDSP->setUserData((void*)mWindGen);
	}

	if (mWindDSP)
	{
		mSystem->playDSP(FMOD_CHANNEL_FREE, mWindDSP, false, 0);
		return true;
	}
	return false;
}


void LLAudioEngine_FMODEX::cleanupWind()
{
	if (mWindDSP)
	{
		mWindDSP->remove();
		mWindDSP->release();
		mWindDSP = NULL;
	}

	delete mWindGen;
	mWindGen = NULL;
}


//-----------------------------------------------------------------------
void LLAudioEngine_FMODEX::updateWind(LLVector3 wind_vec, F32 camera_height_above_water)
{
	LLVector3 wind_pos;
	F64 pitch;
	F64 center_freq;

	if (!mEnableWind)
	{
		return;
	}
	
	if (mWindUpdateTimer.checkExpirationAndReset(LL_WIND_UPDATE_INTERVAL))
	{
		
		// wind comes in as Linden coordinate (+X = forward, +Y = left, +Z = up)
		// need to convert this to the conventional orientation DS3D and OpenAL use
		// where +X = right, +Y = up, +Z = backwards

		wind_vec.setVec(-wind_vec.mV[1], wind_vec.mV[2], -wind_vec.mV[0]);

		// cerr << "Wind update" << endl;

		pitch = 1.0 + mapWindVecToPitch(wind_vec);
		center_freq = 80.0 * pow(pitch,2.5*(mapWindVecToGain(wind_vec)+1.0));
		
		mWindGen->mTargetFreq = (F32)center_freq;
		mWindGen->mTargetGain = (F32)mapWindVecToGain(wind_vec) * mMaxWindGain;
		mWindGen->mTargetPanGainR = (F32)mapWindVecToPan(wind_vec);
  	}
}

//-----------------------------------------------------------------------
void LLAudioEngine_FMODEX::setInternalGain(F32 gain)
{
	if (!mInited)
	{
		return;
	}

	gain = llclamp( gain, 0.0f, 1.0f );

	FMOD::ChannelGroup *master_group;
	mSystem->getMasterChannelGroup(&master_group);

	master_group->setVolume(gain);

	LLStreamingAudioInterface *saimpl = getStreamingAudioImpl();
	if ( saimpl )
	{
		// fmod likes its streaming audio channel gain re-asserted after
		// master volume change.
		saimpl->setGain(saimpl->getGain());
	}
}

//
// LLAudioChannelFMODEX implementation
//

LLAudioChannelFMODEX::LLAudioChannelFMODEX(FMOD::System *system) : LLAudioChannel(), mSystemp(system), mChannelp(NULL), mLastSamplePos(0)
{
}


LLAudioChannelFMODEX::~LLAudioChannelFMODEX()
{
	cleanup();
}

bool LLAudioChannelFMODEX::updateBuffer()
{
	if (LLAudioChannel::updateBuffer())
	{
		// Base class update returned true, which means that we need to actually
		// set up the channel for a different buffer.

		LLAudioBufferFMODEX *bufferp = (LLAudioBufferFMODEX *)mCurrentSourcep->getCurrentBuffer();

		// Grab the FMOD sample associated with the buffer
		FMOD::Sound *soundp = bufferp->getSound();
		if (!soundp)
		{
			// This is bad, there should ALWAYS be a sound associated with a legit
			// buffer.
			llerrs << "No FMOD sound!" << llendl;
			return false;
		}


		// Actually play the sound.  Start it off paused so we can do all the necessary
		// setup.
		if(!mChannelp)
		{
			FMOD_RESULT result = getSystem()->playSound(FMOD_CHANNEL_FREE, soundp, true, &mChannelp);
			Check_FMOD_Error(result, "FMOD::System::playSound");
		}

		//llinfos << "Setting up channel " << std::hex << mChannelID << std::dec << llendl;
	}

	// If we have a source for the channel, we need to update its gain.
	if (mCurrentSourcep)
	{
		// SJB: warnings can spam and hurt framerate, disabling
		FMOD_RESULT result;

		result = mChannelp->setVolume(getSecondaryGain() * mCurrentSourcep->getGain());
		//Check_FMOD_Error(result, "FMOD::Channel::setVolume");

		result = mChannelp->setMode(mCurrentSourcep->isLoop() ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
		/*if(Check_FMOD_Error(result, "FMOD::Channel::setMode"))
		{
			S32 index;
			mChannelp->getIndex(&index);
 			llwarns << "Channel " << index << "Source ID: " << mCurrentSourcep->getID()
 					<< " at " << mCurrentSourcep->getPositionGlobal() << llendl;		
		}*/
	}

	return true;
}


void LLAudioChannelFMODEX::update3DPosition()
{
	if (!mChannelp)
	{
		// We're not actually a live channel (i.e., we're not playing back anything)
		return;
	}

	LLAudioBufferFMODEX  *bufferp = (LLAudioBufferFMODEX  *)mCurrentBufferp;
	if (!bufferp)
	{
		// We don't have a buffer associated with us (should really have been picked up
		// by the above if.
		return;
	}

	if (mCurrentSourcep->isAmbient())
	{
		// Ambient sound, don't need to do any positional updates.
		set3DMode(false);
	}
	else
	{
		// Localized sound.  Update the position and velocity of the sound.
		set3DMode(true);

		LLVector3 float_pos;
		float_pos.setVec(mCurrentSourcep->getPositionGlobal());
		FMOD_RESULT result = mChannelp->set3DAttributes((FMOD_VECTOR*)float_pos.mV, (FMOD_VECTOR*)mCurrentSourcep->getVelocity().mV);
		Check_FMOD_Error(result, "FMOD::Channel::set3DAttributes");
	}
}


void LLAudioChannelFMODEX::updateLoop()
{
	if (!mChannelp)
	{
		// May want to clear up the loop/sample counters.
		return;
	}

	//
	// Hack:  We keep track of whether we looped or not by seeing when the
	// sample position looks like it's going backwards.  Not reliable; may
	// yield false negatives.
	//
	U32 cur_pos;
	mChannelp->getPosition(&cur_pos,FMOD_TIMEUNIT_PCMBYTES);

	if (cur_pos < (U32)mLastSamplePos)
	{
		mLoopedThisFrame = true;
	}
	mLastSamplePos = cur_pos;
}


void LLAudioChannelFMODEX::cleanup()
{
	if (!mChannelp)
	{
		//llinfos << "Aborting cleanup with no channel handle." << llendl;
		return;
	}

	//llinfos << "Cleaning up channel: " << mChannelID << llendl;
	Check_FMOD_Error(mChannelp->stop(),"FMOD::Channel::stop");

	mCurrentBufferp = NULL;
	mChannelp = NULL;
}


void LLAudioChannelFMODEX::play()
{
	if (!mChannelp)
	{
		llwarns << "Playing without a channel handle, aborting" << llendl;
		return;
	}

	Check_FMOD_Error(mChannelp->setPaused(false), "FMOD::Channel::pause");

	getSource()->setPlayedOnce(true);
}


void LLAudioChannelFMODEX::playSynced(LLAudioChannel *channelp)
{
	LLAudioChannelFMODEX *fmod_channelp = (LLAudioChannelFMODEX*)channelp;
	if (!(fmod_channelp->mChannelp && mChannelp))
	{
		// Don't have channels allocated to both the master and the slave
		return;
	}

	U32 cur_pos;
	if(Check_FMOD_Error(mChannelp->getPosition(&cur_pos,FMOD_TIMEUNIT_PCMBYTES), "Unable to retrieve current position"))
		return;

	cur_pos %= mCurrentBufferp->getLength();
	
	// Try to match the position of our sync master
	Check_FMOD_Error(mChannelp->setPosition(cur_pos,FMOD_TIMEUNIT_PCMBYTES),"Unable to set current position");

	// Start us playing
	play();
}


bool LLAudioChannelFMODEX::isPlaying()
{
	if (!mChannelp)
	{
		return false;
	}

	bool paused, playing;
	mChannelp->getPaused(&paused);
	mChannelp->isPlaying(&playing);
	return !paused && playing;
}


//
// LLAudioChannelFMODEX implementation
//


LLAudioBufferFMODEX::LLAudioBufferFMODEX(FMOD::System *system) : mSystemp(system), mSoundp(NULL)
{
}


LLAudioBufferFMODEX::~LLAudioBufferFMODEX()
{
	if(mSoundp)
	{
		mSoundp->release();
		mSoundp = NULL;
	}
}


bool LLAudioBufferFMODEX::loadWAV(const std::string& filename)
{
	// Try to open a wav file from disk.  This will eventually go away, as we don't
	// really want to block doing this.
	if (filename.empty())
	{
		// invalid filename, abort.
		return false;
	}

	if (!LLAPRFile::isExist(filename, LL_APR_RPB))
	{
		// File not found, abort.
		return false;
	}
	
	if (mSoundp)
	{
		// If there's already something loaded in this buffer, clean it up.
		mSoundp->release();
		mSoundp = NULL;
	}

	FMOD_MODE base_mode = FMOD_LOOP_NORMAL | FMOD_SOFTWARE;
	FMOD_CREATESOUNDEXINFO exinfo;
	memset(&exinfo,0,sizeof(exinfo));
	exinfo.cbsize = sizeof(exinfo);
	exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_WAV;	//Hint to speed up loading.
	// Load up the wav file into an fmod sample
#if LL_WINDOWS
	FMOD_RESULT result = getSystem()->createSound((const char*)utf8str_to_utf16str(filename).c_str(), base_mode | FMOD_UNICODE, &exinfo, &mSoundp);
#else
	FMOD_RESULT result = getSystem()->createSound(filename.c_str(), base_mode, &exinfo, &mSoundp);
#endif

	if (result != FMOD_OK)
	{
		// We failed to load the file for some reason.
		llwarns << "Could not load data '" << filename << "': " << FMOD_ErrorString(result) << llendl;

		//
		// If we EVER want to load wav files provided by end users, we need
		// to rethink this!
		//
		// file is probably corrupt - remove it.
		LLFile::remove(filename);
		return false;
	}

	// Everything went well, return true
	return true;
}


U32 LLAudioBufferFMODEX::getLength()
{
	if (!mSoundp)
	{
		return 0;
	}

	U32 length;
	mSoundp->getLength(&length, FMOD_TIMEUNIT_PCMBYTES);
	return length;
}


void LLAudioChannelFMODEX::set3DMode(bool use3d)
{
	FMOD_MODE current_mode;
	if(mChannelp->getMode(&current_mode) != FMOD_OK)
		return;
	FMOD_MODE new_mode = current_mode;	
	new_mode &= ~(use3d ? FMOD_2D : FMOD_3D);
	new_mode |= use3d ? FMOD_3D : FMOD_2D;

	if(current_mode != new_mode)
	{
		mChannelp->setMode(new_mode);
	}
}


FMOD_RESULT F_CALLBACK windCallback(FMOD_DSP_STATE *dsp_state, float *originalbuffer, float *newbuffer, unsigned int length, int inchannels, int outchannels)
{
	// originalbuffer = fmod's original mixbuffer.
	// newbuffer = the buffer passed from the previous DSP unit.
	// length = length in samples at this mix time.
	// userdata = user parameter passed through in FSOUND_DSP_Create.
	
	LLWindGen<LLAudioEngine_FMODEX::MIXBUFFERFORMAT> *windgen;
	FMOD::DSP *thisdsp = (FMOD::DSP *)dsp_state->instance;

	thisdsp->getUserData((void **)&windgen);
	S32 channels, configwidth, configheight;
	thisdsp->getInfo(0, 0, &channels, &configwidth, &configheight);
	
	windgen->windGenerate((LLAudioEngine_FMODEX::MIXBUFFERFORMAT *)newbuffer, length);

	return FMOD_OK;
}
