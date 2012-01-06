/** 
 * @file streamingaudio_fmod.cpp
 * @brief LLStreamingAudio_FMODEX implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
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

#include "llmath.h"

#include "fmod.hpp"
#include "fmod_errors.h"

#include "llstreamingaudio_fmodex.h"


class LLAudioStreamManagerFMODEX
{
public:
	LLAudioStreamManagerFMODEX(FMOD::System *system, const std::string& url);
	FMOD::Channel* startStream();
	bool stopStream(); // Returns true if the stream was successfully stopped.
	bool ready();

	const std::string& getURL() 	{ return mInternetStreamURL; }

	FMOD_OPENSTATE getOpenState();
protected:
	FMOD::System* mSystem;
	FMOD::Channel* mStreamChannel;
	FMOD::Sound* mInternetStream;
	bool mReady;

	std::string mInternetStreamURL;
};



//---------------------------------------------------------------------------
// Internet Streaming
//---------------------------------------------------------------------------
LLStreamingAudio_FMODEX::LLStreamingAudio_FMODEX(FMOD::System *system) :
	mSystem(system),
	mCurrentInternetStreamp(NULL),
	mFMODInternetStreamChannelp(NULL),
	mGain(1.0f),
	mMetaData(NULL)
{
	// Number of milliseconds of audio to buffer for the audio card.
	// Must be larger than the usual Second Life frame stutter time.
	mSystem->setStreamBufferSize(200, FMOD_TIMEUNIT_MS);

	// Here's where we set the size of the network buffer and some buffering 
	// parameters.  In this case we want a network buffer of 16k, we want it 
	// to prebuffer 40% of that when we first connect, and we want it 
	// to rebuffer 80% of that whenever we encounter a buffer underrun.

	// Leave the net buffer properties at the default.
	//FSOUND_Stream_Net_SetBufferProperties(20000, 40, 80);
}


LLStreamingAudio_FMODEX::~LLStreamingAudio_FMODEX()
{
	// nothing interesting/safe to do.
}


void LLStreamingAudio_FMODEX::start(const std::string& url)
{
	//if (!mInited)
	//{
	//	llwarns << "startInternetStream before audio initialized" << llendl;
	//	return;
	//}

	// "stop" stream but don't clear url, etc. in case url == mInternetStreamURL
	stop();

	if (!url.empty())
	{
		llinfos << "Starting internet stream: " << url << llendl;
		mCurrentInternetStreamp = new LLAudioStreamManagerFMODEX(mSystem,url);
		mURL = url;
		mMetaData = new LLSD;
	}
	else
	{
		llinfos << "Set internet stream to null" << llendl;
		mURL.clear();
	}
}


void LLStreamingAudio_FMODEX::update()
{
	// Kill dead internet streams, if possible
	std::list<LLAudioStreamManagerFMODEX *>::iterator iter;
	for (iter = mDeadStreams.begin(); iter != mDeadStreams.end();)
	{
		LLAudioStreamManagerFMODEX *streamp = *iter;
		if (streamp->stopStream())
		{
			llinfos << "Closed dead stream" << llendl;
			delete streamp;
			mDeadStreams.erase(iter++);
		}
		else
		{
			iter++;
		}
	}

	// Don't do anything if there are no streams playing
	if (!mCurrentInternetStreamp)
	{
		return;
	}

	FMOD_OPENSTATE open_state = mCurrentInternetStreamp->getOpenState();

	if (open_state == FMOD_OPENSTATE_READY)
	{
		// Stream is live

		// start the stream if it's ready
		if (!mFMODInternetStreamChannelp &&
			(mFMODInternetStreamChannelp = mCurrentInternetStreamp->startStream()))
		{
			// Reset volume to previously set volume
			setGain(getGain());
			mFMODInternetStreamChannelp->setPaused(false);
		}
	}
	else if(open_state == FMOD_OPENSTATE_ERROR)
	{
		stop();
		return;
	}

	if(mFMODInternetStreamChannelp)
	{
		if(!mMetaData)
			mMetaData = new LLSD;

		FMOD::Sound *sound = NULL;
		
		if(mFMODInternetStreamChannelp->getCurrentSound(&sound) == FMOD_OK && sound)
		{
			FMOD_TAG tag;
			S32 tagcount, dirtytagcount;
			if(sound->getNumTags(&tagcount, &dirtytagcount) == FMOD_OK && dirtytagcount)
			{
				mMetaData->clear();

				for(S32 i = 0; i < tagcount; ++i)
				{
					if(sound->getTag(NULL, i, &tag)!=FMOD_OK)
						continue;
					std::string name = tag.name;
					switch(tag.type)	//Crappy tag translate table.
					{
					case(FMOD_TAGTYPE_ID3V2):
						if(name == "TIT2") name = "TITLE";
						else if(name == "TPE1") name = "ARTIST";
						break;
					case(FMOD_TAGTYPE_ASF):
						if(name == "Title") name = "TITLE";
						else if(name == "WM/AlbumArtist") name = "ARTIST";
						break;
					default:
						break;
					}
					switch(tag.datatype)
					{
						case(FMOD_TAGDATATYPE_INT):
							(*mMetaData)[name]=*(LLSD::Integer*)(tag.data);
							llinfos << tag.name << ": " << *(int*)(tag.data) << llendl;
							break;
						case(FMOD_TAGDATATYPE_FLOAT):
							(*mMetaData)[name]=*(LLSD::Float*)(tag.data);
							llinfos << tag.name << ": " << *(float*)(tag.data) << llendl;
							break;
						case(FMOD_TAGDATATYPE_STRING):
						{
							std::string out = rawstr_to_utf8(std::string((char*)tag.data,tag.datalen));
							(*mMetaData)[name]=out;
							llinfos << tag.name << ": " << out << llendl;
						}
							break;
						case(FMOD_TAGDATATYPE_STRING_UTF16):
						{
							std::string out((char*)tag.data,tag.datalen);
							(*mMetaData)[std::string(tag.name)]=out;
							llinfos << tag.name << ": " << out << llendl;
						}
							break;
						case(FMOD_TAGDATATYPE_STRING_UTF16BE):
						{
							std::string out((char*)tag.data,tag.datalen);
							U16* buf = (U16*)out.c_str();
							for(U32 j = 0; j < out.size()/2; ++j)
								(((buf[j] & 0xff)<<8) | ((buf[j] & 0xff00)>>8));
							(*mMetaData)[std::string(tag.name)]=out;
							llinfos << tag.name << ": " << out << llendl;
						}
						default:
							break;
					}
				}
			}
		}
	}
}

void LLStreamingAudio_FMODEX::stop()
{
	if(mMetaData)
	{
		delete mMetaData;
		mMetaData = NULL;
	}
	if (mFMODInternetStreamChannelp)
	{
		mFMODInternetStreamChannelp->setPaused(true);
		mFMODInternetStreamChannelp->setPriority(0);
		mFMODInternetStreamChannelp = NULL;
	}

	if (mCurrentInternetStreamp)
	{
		llinfos << "Stopping internet stream: " << mCurrentInternetStreamp->getURL() << llendl;
		if (mCurrentInternetStreamp->stopStream())
		{
			delete mCurrentInternetStreamp;
		}
		else
		{
			llwarns << "Pushing stream to dead list: " << mCurrentInternetStreamp->getURL() << llendl;
			mDeadStreams.push_back(mCurrentInternetStreamp);
		}
		mCurrentInternetStreamp = NULL;
		//mURL.clear();
	}
}

void LLStreamingAudio_FMODEX::pause(int pauseopt)
{
	if (pauseopt < 0)
	{
		pauseopt = mCurrentInternetStreamp ? 1 : 0;
	}

	if (pauseopt)
	{
		if (mCurrentInternetStreamp)
		{
			stop();
		}
	}
	else
	{
		start(getURL());
	}
}


// A stream is "playing" if it has been requested to start.  That
// doesn't necessarily mean audio is coming out of the speakers.
int LLStreamingAudio_FMODEX::isPlaying()
{
	if (mCurrentInternetStreamp)
	{
		return 1; // Active and playing
	}
	else if (!mURL.empty())
	{
		return 2; // "Paused"
	}
	else
	{
		return 0;
	}
}


F32 LLStreamingAudio_FMODEX::getGain()
{
	return mGain;
}


std::string LLStreamingAudio_FMODEX::getURL()
{
	return mURL;
}


void LLStreamingAudio_FMODEX::setGain(F32 vol)
{
	mGain = vol;

	if (mFMODInternetStreamChannelp)
	{
		vol = llclamp(vol * vol, 0.f, 1.f);	//should vol be squared here?

		mFMODInternetStreamChannelp->setVolume(vol);
	}
}


///////////////////////////////////////////////////////
// manager of possibly-multiple internet audio streams

LLAudioStreamManagerFMODEX::LLAudioStreamManagerFMODEX(FMOD::System *system, const std::string& url) :
	mSystem(system),
	mStreamChannel(NULL),
	mInternetStream(NULL),
	mReady(false)
{
	mInternetStreamURL = url;

	FMOD_CREATESOUNDEXINFO exinfo;
	memset(&exinfo,0,sizeof(exinfo));
	exinfo.cbsize = sizeof(exinfo);
	exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_MPEG;	//Hint to speed up loading.

	FMOD_RESULT result = mSystem->createStream(url.c_str(), FMOD_2D | FMOD_NONBLOCKING, &exinfo, &mInternetStream);

	if (result!= FMOD_OK)
	{
		llwarns << "Couldn't open fmod stream, error "
			<< FMOD_ErrorString(result)
			<< llendl;
		mReady = false;
		return;
	}

	mReady = true;
}

FMOD::Channel *LLAudioStreamManagerFMODEX::startStream()
{
	// We need a live and opened stream before we try and play it.
	if (!mInternetStream || getOpenState() != FMOD_OPENSTATE_READY)
	{
		llwarns << "No internet stream to start playing!" << llendl;
		return NULL;
	}

	if(mStreamChannel)
		return mStreamChannel;	//Already have a channel for this stream.

	mSystem->playSound(FMOD_CHANNEL_FREE, mInternetStream, true, &mStreamChannel);
	return mStreamChannel;
}

bool LLAudioStreamManagerFMODEX::stopStream()
{
	if (mInternetStream)
	{


		bool close = true;
		switch (getOpenState())
		{
		case FMOD_OPENSTATE_CONNECTING:
			close = false;
			break;
		/*case FSOUND_STREAM_NET_NOTCONNECTED:
		case FSOUND_STREAM_NET_BUFFERING:
		case FSOUND_STREAM_NET_READY:
		case FSOUND_STREAM_NET_ERROR:*/
		default:
			close = true;
		}

		if (close)
		{
			mInternetStream->release();
			mStreamChannel = NULL;
			mInternetStream = NULL;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return true;
	}
}

FMOD_OPENSTATE LLAudioStreamManagerFMODEX::getOpenState()
{
	FMOD_OPENSTATE state;
	mInternetStream->getOpenState(&state,NULL,NULL,NULL);
	return state;
}
