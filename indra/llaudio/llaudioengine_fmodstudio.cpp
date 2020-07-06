/** 
 * @file audioengine_fmodstudio.cpp
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
#include "llstreamingaudio_fmodstudio.h"

#include "llaudioengine_fmodstudio.h"
#include "lllistener_fmodstudio.h"

#include "llerror.h"
#include "llmath.h"
#include "llrand.h"

#include "fmod.hpp"
#include "fmod_errors.h"
#include "lldir.h"

#include "sound_ids.h"

#include <bitset>

#if LL_WINDOWS	//Some ugly code to make missing fmod.dll not cause a fatal error.
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <DelayImp.h>
#pragma comment(lib, "delayimp.lib")

bool attemptDelayLoad()
{
	__try
	{
#if defined(_WIN64)
		if( FAILED( __HrLoadAllImportsForDll( "fmod64.dll" ) ) )
			return false;
#else
		if( FAILED( __HrLoadAllImportsForDll( "fmod.dll" ) ) )
			return false;
#endif
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		return false;
	}
	return true;
}
#endif

static bool sVerboseDebugging = false;

FMOD_RESULT F_CALLBACK windDSPCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int *outchannels);

FMOD::ChannelGroup *LLAudioEngine_FMODSTUDIO::mChannelGroups[LLAudioEngine::AUDIO_TYPE_COUNT] = {0};

//This class is designed to keep track of all sound<->channel assocations.
//Used to verify validity of sound and channel pointers, as well as catch cases were sounds
//are released with active channels still attached.
class CFMODSoundChecks
{
	typedef std::map<FMOD::Sound*,std::set<FMOD::Channel*> > active_sounds_t;
	typedef std::set<FMOD::Sound*> dead_sounds_t;
	typedef std::map<FMOD::Channel*,FMOD::Sound*> active_channels_t;
	typedef std::map<FMOD::Channel*,FMOD::Sound*> dead_channels_t;

	active_sounds_t mActiveSounds;
	dead_sounds_t mDeadSounds;
	active_channels_t mActiveChannels;
	dead_channels_t mDeadChannels;
public:
	enum STATUS
	{
		ACTIVE,
		DEAD,
		UNKNOWN
	};
	STATUS getPtrStatus(LLAudioChannel* channel)
	{
		if(!channel)
			return UNKNOWN;
		return getPtrStatus(dynamic_cast<LLAudioChannelFMODSTUDIO*>(channel)->mChannelp);
	}
	STATUS getPtrStatus(LLAudioBuffer* sound)
	{
		if(!sound)
			return UNKNOWN;
		return getPtrStatus(dynamic_cast<LLAudioBufferFMODSTUDIO*>(sound)->mSoundp);
	}
	STATUS getPtrStatus(FMOD::Channel* channel)
	{
		if(!channel)
			return UNKNOWN;
		else if(mActiveChannels.find(channel) != mActiveChannels.end())
			return ACTIVE;
		else if(mDeadChannels.find(channel) != mDeadChannels.end())
			return DEAD;
		return UNKNOWN;
	}
	STATUS getPtrStatus(FMOD::Sound* sound)
	{
		if(!sound)
			return UNKNOWN;
		if(mActiveSounds.find(sound) != mActiveSounds.end())
			return ACTIVE;
		else if(mDeadSounds.find(sound) != mDeadSounds.end())
			return DEAD;
		return UNKNOWN;
	}
	void addNewSound(FMOD::Sound* sound)
	{
		assertActiveState(sound,true,false);

		mDeadSounds.erase(sound);
		mActiveSounds.insert(std::make_pair(sound,std::set<FMOD::Channel*>()));
	}
	void removeSound(FMOD::Sound* sound)
	{
		assertActiveState(sound,true);

		active_sounds_t::const_iterator it = mActiveSounds.find(sound);
		llassert(it != mActiveSounds.end());
		if(it != mActiveSounds.end())
		{
			if(!it->second.empty())
			{
				LL_WARNS("AudioImpl")	<< "Removing sound " << sound << " with attached channels: \n";
				for(std::set<FMOD::Channel*>::iterator it2 = it->second.begin(); it2 != it->second.end();++it2)
				{
					switch(getPtrStatus(*it2))
					{
					case ACTIVE:
						LL_CONT << " Channel " << *it2 << " ACTIVE\n";
						break;
					case DEAD:
						LL_CONT << " Channel " << *it2 << " DEAD\n";
						break;
					default:
						LL_CONT << " Channel " << *it2 << " UNKNOWN\n";
					}
				}
				LL_CONT << LL_ENDL;
			}
			llassert(it->second.empty());
			mDeadSounds.insert(sound);
			mActiveSounds.erase(sound);
		}
	}
	void addNewChannelToSound(FMOD::Sound* sound,FMOD::Channel* channel)
	{
		assertActiveState(sound,true);
		assertActiveState(channel,true,false);

		mActiveSounds[sound].insert(channel);
		mActiveChannels.insert(std::make_pair(channel,sound));
	}
	void removeChannel(FMOD::Channel* channel)
	{
		assertActiveState(channel,true);

		active_channels_t::const_iterator it = mActiveChannels.find(channel);
		llassert(it != mActiveChannels.end());
		if(it != mActiveChannels.end())
		{
#ifdef SHOW_ASSERT
			STATUS status = getPtrStatus(it->second);
			llassert(status != DEAD);
			llassert(status != UNKNOWN);
#endif

			active_sounds_t::iterator it2 = mActiveSounds.find(it->second);
			llassert(it2 != mActiveSounds.end());
			if(it2 != mActiveSounds.end())
			{
				it2->second.erase(channel);
			}
			mDeadChannels.insert(*it);
			mActiveChannels.erase(channel);
		}
	}

	template <typename T>
	void assertActiveState(T ptr, bool try_log=false, bool active=true)
	{
#ifndef SHOW_ASSERT
		if(try_log && sVerboseDebugging)
#endif
		{
			CFMODSoundChecks::STATUS chan = getPtrStatus(ptr);
			if(try_log && sVerboseDebugging)
			{
				if(active)
				{
					if(chan == CFMODSoundChecks::DEAD)
						LL_WARNS("AudioImpl") << __FUNCTION__ << ": Using unexpectedly dead " << typeid(T*).name() << " " << ptr << LL_ENDL;
					else if(chan == CFMODSoundChecks::UNKNOWN)
						LL_WARNS("AudioImpl") << __FUNCTION__ << ": Using unexpectedly unknown " << typeid(T*).name() << " " << ptr << LL_ENDL;
				}
				else if(chan == CFMODSoundChecks::ACTIVE)
						LL_WARNS("AudioImpl") << __FUNCTION__ << ": Using unexpectedly active " << typeid(T*).name() << " " << ptr << LL_ENDL;
			}
			llassert( active == (chan == CFMODSoundChecks::ACTIVE) );
		}
	}
} gSoundCheck;

LLAudioEngine_FMODSTUDIO::LLAudioEngine_FMODSTUDIO(bool enable_profiler, bool verbose_debugging)
	: mInited(false)
	, mWindGen(NULL)
	, mWindDSPDesc(NULL)
	, mWindDSP(NULL)
	, mSystem(NULL)
	, mEnableProfiler(enable_profiler)
{
	sVerboseDebugging = verbose_debugging;
}

LLAudioEngine_FMODSTUDIO::~LLAudioEngine_FMODSTUDIO()
{
}

inline bool Check_FMOD_Error(FMOD_RESULT result, const char *string)
{
	if(result == FMOD_OK)
		return false;
	LL_WARNS("AudioImpl") << string << " Error: " << FMOD_ErrorString(result) << LL_ENDL;
	return true;
}

bool LLAudioEngine_FMODSTUDIO::init(const S32 num_channels, void* userdata)
{
	LL_WARNS("AudioImpl") << "BARKBARKBARK" << LL_ENDL;
#if 0 //LL_WINDOWS
	if(!attemptDelayLoad())
		return false;
#endif

	U32 version = 0;

	FMOD_RESULT result;

	LL_DEBUGS("AppInit") << "LLAudioEngine_FMODSTUDIO::init() initializing FMOD" << LL_ENDL;

	result = FMOD::System_Create(&mSystem);
	if(Check_FMOD_Error(result, "FMOD::System_Create"))
		return false;

	//will call LLAudioEngine_FMODSTUDIO::allocateListener, which needs a valid mSystem pointer.
	LLAudioEngine::init(num_channels, userdata);	
	
	result = mSystem->getVersion(&version);
	Check_FMOD_Error(result, "FMOD::System::getVersion");

	if (version < FMOD_VERSION)
	{
		LL_WARNS("AppInit") << "Error : You are using the wrong FMOD Studio version (" << version
			<< ")!  You should be using FMOD Studio" << FMOD_VERSION << LL_ENDL;
	}

	// In this case, all sounds, PLUS wind and stream will be software.
	result = mSystem->setSoftwareChannels(num_channels + 2);
	Check_FMOD_Error(result,"FMOD::System::setSoftwareChannels");

	U32 fmod_flags = FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED | FMOD_INIT_THREAD_UNSAFE;
	if(mEnableProfiler)
	{
		fmod_flags |= FMOD_INIT_PROFILE_ENABLE;
	}

#if LL_LINUX
	bool audio_ok = false;

	if (!audio_ok)
	{
		if (NULL == getenv("LL_BAD_FMOD_PULSEAUDIO")) /*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying PulseAudio audio output..." << LL_ENDL;
			if((result = mSystem->setOutput(FMOD_OUTPUTTYPE_PULSEAUDIO)) == FMOD_OK &&
				(result = mSystem->init(num_channels + 2, fmod_flags, 0)) == FMOD_OK)
			{
				LL_DEBUGS("AppInit") << "PulseAudio output initialized OKAY"	<< LL_ENDL;
				audio_ok = true;
			}
			else 
			{
				Check_FMOD_Error(result, "PulseAudio audio output FAILED to initialize");
			}
		} 
		else 
		{
			LL_DEBUGS("AppInit") << "PulseAudio audio output SKIPPED" << LL_ENDL;
		}	
	}
	if (!audio_ok)
	{
		if (NULL == getenv("LL_BAD_FMOD_ALSA"))		/*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying ALSA audio output..." << LL_ENDL;
			if((result = mSystem->setOutput(FMOD_OUTPUTTYPE_ALSA)) == FMOD_OK &&
			    (result = mSystem->init(num_channels + 2, fmod_flags, 0)) == FMOD_OK)
			{
				LL_DEBUGS("AppInit") << "ALSA audio output initialized OKAY" << LL_ENDL;
				audio_ok = true;
			} 
			else 
			{
				Check_FMOD_Error(result, "ALSA audio output FAILED to initialize");
			}
		} 
		else 
		{
			LL_DEBUGS("AppInit") << "ALSA audio output SKIPPED" << LL_ENDL;
		}
	}
	if (!audio_ok)
	{
		LL_WARNS("AppInit") << "Overall audio init failure." << LL_ENDL;
		return false;
	}

	// We're interested in logging which output method we
	// ended up with, for QA purposes.
	FMOD_OUTPUTTYPE output_type;
	if(!Check_FMOD_Error(mSystem->getOutput(&output_type), "FMOD::System::getOutput"))
	{
		switch (output_type)
		{
			case FMOD_OUTPUTTYPE_NOSOUND: 
				LL_INFOS("AppInit") << "Audio output: NoSound" << LL_ENDL; break;
			case FMOD_OUTPUTTYPE_PULSEAUDIO:	
				LL_INFOS("AppInit") << "Audio output: PulseAudio" << LL_ENDL; break;
			case FMOD_OUTPUTTYPE_ALSA: 
				LL_INFOS("AppInit") << "Audio output: ALSA" << LL_ENDL; break;
			default:
				LL_INFOS("AppInit") << "Audio output: Unknown!" << LL_ENDL; break;
		};
	}
#else // LL_LINUX

	// initialize the FMOD engine
	result = mSystem->init( num_channels + 2, fmod_flags, 0);
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		/*
		Ok, the speaker mode selected isn't supported by this soundcard. Switch it
		back to stereo...
		*/
		result = mSystem->setSoftwareFormat(44100, FMOD_SPEAKERMODE_STEREO, 0);
		Check_FMOD_Error(result,"Error falling back to stereo mode");
		/*
		... and re-init.
		*/
		result = mSystem->init( num_channels + 2, fmod_flags, 0);
	}
	if(Check_FMOD_Error(result, "Error initializing FMOD Studio"))
		return false;
#endif

	if (mEnableProfiler)
	{
		Check_FMOD_Error(mSystem->createChannelGroup("None", &mChannelGroups[AUDIO_TYPE_NONE]), "FMOD::System::createChannelGroup");
		Check_FMOD_Error(mSystem->createChannelGroup("SFX", &mChannelGroups[AUDIO_TYPE_SFX]), "FMOD::System::createChannelGroup");
		Check_FMOD_Error(mSystem->createChannelGroup("UI", &mChannelGroups[AUDIO_TYPE_UI]), "FMOD::System::createChannelGroup");
		Check_FMOD_Error(mSystem->createChannelGroup("Ambient", &mChannelGroups[AUDIO_TYPE_AMBIENT]), "FMOD::System::createChannelGroup");
	}

	// set up our favourite FMOD-native streaming audio implementation if none has already been added
	if (!getStreamingAudioImpl()) // no existing implementation added
		setStreamingAudioImpl(new LLStreamingAudio_FMODSTUDIO(mSystem));

	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init() FMOD Studio initialized correctly" << LL_ENDL;

	int r_numbuffers, r_samplerate;
	unsigned int r_bufferlength;
	char r_name[256];
	FMOD_SPEAKERMODE speaker_mode;
	if (!Check_FMOD_Error(mSystem->getDSPBufferSize(&r_bufferlength, &r_numbuffers), "FMOD::System::getDSPBufferSize") &&
		!Check_FMOD_Error(mSystem->getSoftwareFormat(&r_samplerate, &speaker_mode, NULL), "FMOD::System::getSoftwareFormat") &&
		!Check_FMOD_Error(mSystem->getDriverInfo(0, r_name, 255, NULL, NULL, &speaker_mode, NULL), "FMOD::System::getDriverInfo"))
	{
		std::string speaker_mode_str = "unknown";
		switch(speaker_mode)
		{
			#define SPEAKER_MODE_CASE(MODE) case MODE: speaker_mode_str = #MODE; break;
			SPEAKER_MODE_CASE(FMOD_SPEAKERMODE_RAW)
			SPEAKER_MODE_CASE(FMOD_SPEAKERMODE_MONO)
			SPEAKER_MODE_CASE(FMOD_SPEAKERMODE_STEREO)
			SPEAKER_MODE_CASE(FMOD_SPEAKERMODE_QUAD)
			SPEAKER_MODE_CASE(FMOD_SPEAKERMODE_SURROUND)
			SPEAKER_MODE_CASE(FMOD_SPEAKERMODE_5POINT1)
			SPEAKER_MODE_CASE(FMOD_SPEAKERMODE_7POINT1)
			default:;
			#undef SPEAKER_MODE_CASE
		}

		r_name[255] = '\0';
		int latency = 1000.0 * r_bufferlength * r_numbuffers /r_samplerate;

		LL_INFOS("AppInit") << "FMOD device: "<< r_name << "\n"
			<< "Output mode: "<< speaker_mode_str << "\n"
			<< "FMOD Studio parameters: " << r_samplerate << " Hz * " <<" bit\n"
			<< "\tbuffer " << r_bufferlength << " * " << r_numbuffers << " (" << latency <<"ms)" << LL_ENDL;
	}
	else
	{
		LL_WARNS("AppInit") << "Failed to retrieve FMOD device info!" << LL_ENDL;
	}
	mInited = true;

	return true;
}


std::string LLAudioEngine_FMODSTUDIO::getDriverName(bool verbose)
{
	llassert_always(mSystem);
	if (verbose)
	{
		U32 version;
		if(!Check_FMOD_Error(mSystem->getVersion(&version), "FMOD::System::getVersion"))
		{
			return llformat("FMOD Studio %1x.%02x.%02x", version >> 16, version >> 8 & 0x000000FF, version & 0x000000FF);
		}
	}
	return "FMOD Studio";
}


void LLAudioEngine_FMODSTUDIO::allocateListener(void)
{	
	mListenerp = (LLListener *) new LLListener_FMODSTUDIO(mSystem);
	if (!mListenerp)
	{
		LL_WARNS("AudioImpl") << "Listener creation failed" << LL_ENDL;
	}
}


void LLAudioEngine_FMODSTUDIO::shutdown()
{
	LL_INFOS("AudioImpl") << "About to LLAudioEngine::shutdown()" << LL_ENDL;
	LLAudioEngine::shutdown();
	
	LL_INFOS("AudioImpl") << "LLAudioEngine_FMODSTUDIO::shutdown() closing FMOD Studio" << LL_ENDL;
	if ( mSystem ) // speculative fix for MAINT-2657
	{
		LL_INFOS("AudioImpl") << "LLAudioEngine_FMODSTUDIO::shutdown() Requesting FMOD Studio system closure" << LL_ENDL;
		Check_FMOD_Error(mSystem->close(), "FMOD::System::close");
		LL_INFOS("AudioImpl") << "LLAudioEngine_FMODSTUDIO::shutdown() Requesting FMOD Studio system release" << LL_ENDL;
		Check_FMOD_Error(mSystem->release(), "FMOD::System::release");
	}
	LL_INFOS("AudioImpl") << "LLAudioEngine_FMODSTUDIO::shutdown() done closing FMOD Studio" << LL_ENDL;

	delete mListenerp;
	mListenerp = NULL;
}


LLAudioBuffer * LLAudioEngine_FMODSTUDIO::createBuffer()
{
	return new LLAudioBufferFMODSTUDIO(mSystem);
}


LLAudioChannel * LLAudioEngine_FMODSTUDIO::createChannel()
{
	return new LLAudioChannelFMODSTUDIO(mSystem);
}

bool LLAudioEngine_FMODSTUDIO::initWind()
{
	mNextWindUpdate = 0.0;

	cleanupWind();

	mWindDSPDesc = new FMOD_DSP_DESCRIPTION();
	memset(mWindDSPDesc, 0, sizeof(*mWindDSPDesc));	//Set everything to zero
	mWindDSPDesc->pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;
	strncpy(mWindDSPDesc->name, "Wind Unit", sizeof(mWindDSPDesc->name));	//Set name to "Wind Unit"
	mWindDSPDesc->numoutputbuffers = 1;
	mWindDSPDesc->read = &windDSPCallback; //Assign callback.
	if (Check_FMOD_Error(mSystem->createDSP(mWindDSPDesc, &mWindDSP), "FMOD::createDSP") || !mWindDSP)
		return false;
	
	int frequency = 44100;
	FMOD_SPEAKERMODE mode;
	if (!Check_FMOD_Error(mSystem->getSoftwareFormat(&frequency, &mode, NULL), "FMOD::System::getSoftwareFormat"))
	{
		mWindGen = new LLWindGen<MIXBUFFERFORMAT>((U32)frequency);

		if (!Check_FMOD_Error(mWindDSP->setUserData((void*)mWindGen), "FMOD::DSP::setUserData") &&
			!Check_FMOD_Error(mSystem->getSoftwareFormat(NULL, &mode, NULL), "FMOD::System::getSoftwareFormat") &&
			!Check_FMOD_Error(mWindDSP->setChannelFormat(FMOD_CHANNELMASK_STEREO, 2, mode), "FMOD::DSP::setChannelFormat") &&
			!Check_FMOD_Error(mSystem->playDSP(mWindDSP, NULL, false, 0), "FMOD::System::playDSP"))
			return true;	//Success
	}

	cleanupWind();
	return false;
}


void LLAudioEngine_FMODSTUDIO::cleanupWind()
{
	if (mWindDSP)
	{
		FMOD::ChannelGroup* mastergroup = NULL;
		if (!Check_FMOD_Error(mSystem->getMasterChannelGroup(&mastergroup), "FMOD::System::getMasterChannelGroup") && mastergroup)
			Check_FMOD_Error(mastergroup->removeDSP(mWindDSP), "FMOD::ChannelGroup::removeDSP");
		Check_FMOD_Error(mWindDSP->release(), "FMOD::DSP::release");
		mWindDSP = NULL;
	}

	delete mWindDSPDesc;
	mWindDSPDesc = NULL;

	delete mWindGen;
	mWindGen = NULL;
}


//-----------------------------------------------------------------------
void LLAudioEngine_FMODSTUDIO::updateWind(LLVector3 wind_vec, F32 camera_height_above_water)
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
void LLAudioEngine_FMODSTUDIO::setInternalGain(F32 gain)
{
	if (!mInited)
	{
		return;
	}

	gain = llclamp( gain, 0.0f, 1.0f );

	FMOD::ChannelGroup *master_group;
	if(Check_FMOD_Error(mSystem->getMasterChannelGroup(&master_group), "FMOD::System::getMasterChannelGroup"))
		return;
		
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
// LLAudioChannelFMODSTUDIO implementation
//

LLAudioChannelFMODSTUDIO::LLAudioChannelFMODSTUDIO(FMOD::System *system) : LLAudioChannel(), mSystemp(system), mChannelp(NULL), mLastSamplePos(0)
{
}


LLAudioChannelFMODSTUDIO::~LLAudioChannelFMODSTUDIO()
{
	if(sVerboseDebugging)
		LL_DEBUGS("AudioImpl") << "Destructing Audio Channel. mChannelp = " << mChannelp << LL_ENDL;

	cleanup();
}

static FMOD_RESULT F_CALLBACK channel_callback(FMOD_CHANNELCONTROL *channel, FMOD_CHANNELCONTROL_TYPE controltype, FMOD_CHANNELCONTROL_CALLBACK_TYPE callbacktype, void *commanddata1, void *commanddata2)
{
	if (controltype == FMOD_CHANNELCONTROL_CHANNEL &&
		callbacktype == FMOD_CHANNELCONTROL_CALLBACK_END)
	{
		FMOD::Channel* chan = reinterpret_cast<FMOD::Channel*>(channel);
		LLAudioChannelFMODSTUDIO* audio_channel = NULL;
		chan->getUserData((void**)&audio_channel);
		if(audio_channel)
		{
			audio_channel->onRelease();
		}
	}
	return FMOD_OK;
}

void LLAudioChannelFMODSTUDIO::onRelease()
{
	llassert(mChannelp);
	if(sVerboseDebugging)
		LL_DEBUGS("AudioImpl") << "Fmod signaled channel release for channel  " << mChannelp << LL_ENDL;
	gSoundCheck.removeChannel(mChannelp);
	mChannelp = NULL;	//Null out channel here so cleanup doesn't try to redundantly stop it.
	cleanup();
}

bool LLAudioChannelFMODSTUDIO::updateBuffer()
{
	if (LLAudioChannel::updateBuffer())
	{
		// Base class update returned true, which means the channel buffer was changed, and not is null.

		// Grab the FMOD sample associated with the buffer
		FMOD::Sound *soundp = ((LLAudioBufferFMODSTUDIO*)mCurrentBufferp)->getSound();
		if (!soundp)
		{
			// This is bad, there should ALWAYS be a sound associated with a legit
			// buffer.
			LL_ERRS("AudioImpl") << "No FMOD sound!" << LL_ENDL;
			return false;
		}


		// Actually play the sound.  Start it off paused so we can do all the necessary
		// setup.
		if(!mChannelp)
		{
			FMOD_RESULT result = getSystem()->playSound(soundp, NULL, true, &mChannelp);
			Check_FMOD_Error(result, "FMOD::System::playSound");
			if(result == FMOD_OK)
			{
				if(sVerboseDebugging)
					LL_DEBUGS("AudioImpl") << "Created channel " << mChannelp << " for sound " << soundp << LL_ENDL;

				gSoundCheck.addNewChannelToSound(soundp,mChannelp);
				mChannelp->setCallback(&channel_callback);
				mChannelp->setUserData(this);
			}
		}
	}

	// If we have a source for the channel, we need to update its gain.
	if (mCurrentSourcep && mChannelp)
	{
		FMOD_RESULT result;

		gSoundCheck.assertActiveState(this);
		result = mChannelp->setVolume(getSecondaryGain() * mCurrentSourcep->getGain());
		Check_FMOD_Error(result, "FMOD::Channel::setVolume");
		result = mChannelp->setMode(mCurrentSourcep->isLoop() ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
		Check_FMOD_Error(result, "FMOD::Channel::setMode");
	}

	return true;
}


void LLAudioChannelFMODSTUDIO::update3DPosition()
{
	if (!mChannelp)
	{
		// We're not actually a live channel (i.e., we're not playing back anything)
		return;
	}

	LLAudioBufferFMODSTUDIO  *bufferp = (LLAudioBufferFMODSTUDIO  *)mCurrentBufferp;
	if (!bufferp)
	{
		// We don't have a buffer associated with us (should really have been picked up
		// by the above if.
		return;
	}

	gSoundCheck.assertActiveState(this);

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


void LLAudioChannelFMODSTUDIO::updateLoop()
{
	if (!mChannelp)
	{
		// May want to clear up the loop/sample counters.
		return;
	}

	gSoundCheck.assertActiveState(this);

	//
	// Hack:  We keep track of whether we looped or not by seeing when the
	// sample position looks like it's going backwards.  Not reliable; may
	// yield false negatives.
	//
	U32 cur_pos;
	Check_FMOD_Error(mChannelp->getPosition(&cur_pos,FMOD_TIMEUNIT_PCMBYTES),"FMOD::Channel::getPosition");

	if (cur_pos < (U32)mLastSamplePos)
	{
		mLoopedThisFrame = true;
	}
	mLastSamplePos = cur_pos;
}


void LLAudioChannelFMODSTUDIO::cleanup()
{
	LLAudioChannel::cleanup();

	if (!mChannelp)
	{
		llassert(mCurrentBufferp == NULL);
		//LL_INFOS("AudioImpl") << "Aborting cleanup with no channel handle." << LL_ENDL;
		return;
	}

	if(sVerboseDebugging)
		LL_DEBUGS("AudioImpl") << "Stopping channel " << mChannelp << LL_ENDL;

	gSoundCheck.removeChannel(mChannelp);
	mChannelp->setCallback(NULL);
	Check_FMOD_Error(mChannelp->stop(),"FMOD::Channel::stop");

	mChannelp = NULL;
	mLastSamplePos = 0;
}


void LLAudioChannelFMODSTUDIO::play()
{
	if (!mChannelp)
	{
		LL_WARNS("AudioImpl") << "Playing without a channel handle, aborting" << LL_ENDL;
		return;
	}

	gSoundCheck.assertActiveState(this,true);

	bool paused=true;
	Check_FMOD_Error(mChannelp->getPaused(&paused), "FMOD::Channel::getPaused");
	if(!paused)
	{
		Check_FMOD_Error(mChannelp->setPaused(true), "FMOD::Channel::setPaused");
		Check_FMOD_Error(mChannelp->setPosition(0,FMOD_TIMEUNIT_PCMBYTES), "FMOD::Channel::setPosition");
	}
	Check_FMOD_Error(mChannelp->setPaused(false), "FMOD::Channel::setPaused");

	if(sVerboseDebugging)
		LL_DEBUGS("AudioImpl") << "Playing channel " << mChannelp << LL_ENDL;

	getSource()->setPlayedOnce(true);

	if(LLAudioEngine_FMODSTUDIO::mChannelGroups[getSource()->getType()])
		Check_FMOD_Error(mChannelp->setChannelGroup(LLAudioEngine_FMODSTUDIO::mChannelGroups[getSource()->getType()]),"FMOD::Channel::setChannelGroup");
}


void LLAudioChannelFMODSTUDIO::playSynced(LLAudioChannel *channelp)
{
	LLAudioChannelFMODSTUDIO *fmod_channelp = (LLAudioChannelFMODSTUDIO*)channelp;
	if (!(fmod_channelp->mChannelp && mChannelp))
	{
		// Don't have channels allocated to both the master and the slave
		return;
	}

	gSoundCheck.assertActiveState(this,true);

	U32 cur_pos;
	if(Check_FMOD_Error(fmod_channelp->mChannelp->getPosition(&cur_pos,FMOD_TIMEUNIT_PCMBYTES), "Unable to retrieve current position"))
		return;

	cur_pos %= mCurrentBufferp->getLength();
	
	// Try to match the position of our sync master
	Check_FMOD_Error(mChannelp->setPosition(cur_pos,FMOD_TIMEUNIT_PCMBYTES),"Unable to set current position");

	// Start us playing
	play();
}


bool LLAudioChannelFMODSTUDIO::isPlaying()
{
	if (!mChannelp)
	{
		return false;
	}

	gSoundCheck.assertActiveState(this);

	bool paused, playing;
	Check_FMOD_Error(mChannelp->getPaused(&paused),"FMOD::Channel::getPaused");
	Check_FMOD_Error(mChannelp->isPlaying(&playing),"FMOD::Channel::isPlaying");
	return !paused && playing;
}


//
// LLAudioChannelFMODSTUDIO implementation
//


LLAudioBufferFMODSTUDIO::LLAudioBufferFMODSTUDIO(FMOD::System *system) : LLAudioBuffer(), mSystemp(system), mSoundp(NULL)
{
}


LLAudioBufferFMODSTUDIO::~LLAudioBufferFMODSTUDIO()
{
	if(mSoundp)
	{
		if(sVerboseDebugging)
			LL_DEBUGS("AudioImpl") << "Release sound " << mSoundp << LL_ENDL;

		gSoundCheck.removeSound(mSoundp);
		Check_FMOD_Error(mSoundp->release(),"FMOD::Sound::Release");
		mSoundp = NULL;
	}
}


bool LLAudioBufferFMODSTUDIO::loadWAV(const std::string& filename)
{
	// Try to open a wav file from disk.  This will eventually go away, as we don't
	// really want to block doing this.
	if (filename.empty())
	{
		// invalid filename, abort.
		return false;
	}

	if (!gDirUtilp->fileExists(filename))
	{
		// File not found, abort.
		return false;
	}
	
	if (mSoundp)
	{
		gSoundCheck.removeSound(mSoundp);
		// If there's already something loaded in this buffer, clean it up.
		Check_FMOD_Error(mSoundp->release(),"FMOD::Sound::release");
		mSoundp = NULL;
	}

	FMOD_MODE base_mode = FMOD_LOOP_NORMAL;
	FMOD_CREATESOUNDEXINFO exinfo = { };
	exinfo.cbsize = sizeof(exinfo);
	exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_WAV;	//Hint to speed up loading.
	// Load up the wav file into an fmod sample
	FMOD_RESULT result = getSystem()->createSound(filename.c_str(), base_mode, &exinfo, &mSoundp);
	if (result != FMOD_OK)
	{
		// We failed to load the file for some reason.
		LL_WARNS("AudioImpl") << "Could not load data '" << filename << "': " << FMOD_ErrorString(result) << LL_ENDL;

		//
		// If we EVER want to load wav files provided by end users, we need
		// to rethink this!
		//
		// file is probably corrupt - remove it.
		LLFile::remove(filename);
		return false;
	}

	gSoundCheck.addNewSound(mSoundp);

	// Everything went well, return true
	return true;
}


U32 LLAudioBufferFMODSTUDIO::getLength()
{
	if (!mSoundp)
	{
		return 0;
	}

	gSoundCheck.assertActiveState(this);
	U32 length;
	Check_FMOD_Error(mSoundp->getLength(&length, FMOD_TIMEUNIT_PCMBYTES),"FMOD::Sound::getLength");
	return length;
}


void LLAudioChannelFMODSTUDIO::set3DMode(bool use3d)
{
	gSoundCheck.assertActiveState(this);

	FMOD_MODE current_mode;
	if(Check_FMOD_Error(mChannelp->getMode(&current_mode),"FMOD::Channel::getMode"))
		return;
	FMOD_MODE new_mode = current_mode;	
	new_mode &= ~(use3d ? FMOD_2D : FMOD_3D);
	new_mode |= use3d ? FMOD_3D : FMOD_2D;

	if(current_mode != new_mode)
	{
		Check_FMOD_Error(mChannelp->setMode(new_mode),"FMOD::Channel::setMode");
	}
}


FMOD_RESULT F_CALLBACK windDSPCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int *outchannels)
{
	// inbuffer = incomming data.
	// newbuffer = outgoing data. AKA this DSP's output.
	// length = length in samples at this mix time. True buffer size, in bytes, would be (length * sizeof(float) * inchannels).
	// userdata = user-provided data attached this DSP via FMOD::DSP::setUserData.
	
	LLWindGen<LLAudioEngine_FMODSTUDIO::MIXBUFFERFORMAT> *windgen;
	FMOD::DSP *thisdsp = (FMOD::DSP *)dsp_state->instance;

	thisdsp->getUserData((void **)&windgen);
	
	if (windgen)
		windgen->windGenerate((LLAudioEngine_FMODSTUDIO::MIXBUFFERFORMAT *)outbuffer, length);

	return FMOD_OK;
}
