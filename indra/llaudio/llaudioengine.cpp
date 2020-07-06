 /** 
 * @file audioengine.cpp
 * @brief implementation of LLAudioEngine class abstracting the Open
 * AL audio support
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llaudioengine.h"
#include "llstreamingaudio.h"

#include "llerror.h"
#include "llmath.h"

#include "sound_ids.h"  // temporary hack for min/max distances

#include "llvfs.h"
#include "lldir.h"
#include "llaudiodecodemgr.h"
#include "llassetstorage.h"

#include <queue>
#include <boost/pool/pool_alloc.hpp>


// necessary for grabbing sounds from sim (implemented in viewer)	
extern void request_sound(const LLUUID &sound_guid);

LLAudioEngine* gAudiop = NULL;

int gSoundHistoryPruneCounter = 0;

//
// LLAudioEngine implementation
//


LLAudioEngine::LLAudioEngine()
{
	setDefaults();
}


LLAudioEngine::~LLAudioEngine()
{
}

LLStreamingAudioInterface* LLAudioEngine::getStreamingAudioImpl()
{
	return mStreamingAudioImpl;
}

void LLAudioEngine::setStreamingAudioImpl(LLStreamingAudioInterface *impl)
{
	mStreamingAudioImpl = impl;
}

void LLAudioEngine::setDefaults()
{
	mMaxWindGain = 1.f;

	mListenerp = NULL;

	mMuted = false;
	mUserData = NULL;

	mLastStatus = 0;

	mNumChannels = 0;
	mEnableWind = false;

	S32 i;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		mChannels[i] = NULL;
	}
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		mBuffers[i] = NULL;
	}

	mMasterGain = 1.f;
	// Setting mInternalGain to an out of range value fixes the issue reported in STORM-830.
	// There is an edge case in setMasterGain during startup which prevents setInternalGain from 
	// being called if the master volume setting and mInternalGain both equal 0, so using -1 forces
	// the if statement in setMasterGain to execute when the viewer starts up.
	mInternalGain = -1.f;
	mNextWindUpdate = 0.f;

	mStreamingAudioImpl = NULL;

	for (U32 i = 0; i < LLAudioEngine::AUDIO_TYPE_COUNT; i++)
		mSecondaryGain[i] = 1.0f;

	mCurrentTransfer = NULL;

	mAllowLargeSounds = false;
}


bool LLAudioEngine::init(const S32 num_channels, void* userdata)
{
	setDefaults();

	mNumChannels = num_channels;
	mUserData = userdata;

	// Initialize the decode manager
	gAudioDecodeMgrp = new LLAudioDecodeMgr;

	LL_INFOS("AudioEngine") << "LLAudioEngine::init() AudioEngine successfully initialized" << LL_ENDL;

	return true;
}


void LLAudioEngine::shutdown()
{
	// Clean up decode manager
	delete gAudioDecodeMgrp;
	gAudioDecodeMgrp = NULL;

	// Clean up wind source
	cleanupWind();

	// Clean up audio sources
	source_map::iterator iter_src;
	for (iter_src = mAllSources.begin(); iter_src != mAllSources.end(); iter_src++)
	{
		delete iter_src->second;
	}


	// Clean up audio data
	data_map::iterator iter_data;
	for (iter_data = mAllData.begin(); iter_data != mAllData.end(); iter_data++)
	{
		delete iter_data->second;
	}


	// Clean up channels
	S32 i;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		delete mChannels[i];
		mChannels[i] = NULL;
	}

	// Clean up buffers
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		delete mBuffers[i];
		mBuffers[i] = NULL;
	}

	delete mStreamingAudioImpl;
	mStreamingAudioImpl = NULL;
}


// virtual
void LLAudioEngine::startInternetStream(const std::string& url)
{
	if (mStreamingAudioImpl)
		mStreamingAudioImpl->start(url);
}


// virtual
void LLAudioEngine::stopInternetStream()
{
	if (mStreamingAudioImpl)
		mStreamingAudioImpl->stop();
}

// virtual
void LLAudioEngine::pauseInternetStream(int pause)
{
	if (mStreamingAudioImpl)
		mStreamingAudioImpl->pause(pause);
}

// virtual
void LLAudioEngine::updateInternetStream()
{
	if (mStreamingAudioImpl)
		mStreamingAudioImpl->update();
}

// virtual
LLAudioEngine::LLAudioPlayState LLAudioEngine::isInternetStreamPlaying()
{
	if (mStreamingAudioImpl)
		return (LLAudioEngine::LLAudioPlayState) mStreamingAudioImpl->isPlaying();

	return LLAudioEngine::AUDIO_STOPPED; // Stopped
}


// virtual
void LLAudioEngine::setInternetStreamGain(F32 vol)
{
	if (mStreamingAudioImpl)
		mStreamingAudioImpl->setGain(vol);
}

// virtual
std::string LLAudioEngine::getInternetStreamURL()
{
	if (mStreamingAudioImpl)
		return mStreamingAudioImpl->getURL();
	else return std::string();
}

void LLAudioEngine::checkStates()
{
#ifdef SHOW_ASSERT
	for (S32 i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i])
		{
			bool buf_has_ref = false;
			for (S32 j = 0; j < MAX_CHANNELS; j++)
			{
				if (mChannels[j])
				{
					if(mChannels[j]->mCurrentBufferp == mBuffers[i])
						buf_has_ref = true;
				}
			}
			if(buf_has_ref)
				llassert(mBuffers[i]->mInUse);
		}
	}
#endif //SHOW_ASSERT
}

void LLAudioEngine::updateChannels()
{
	S32 i;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (mChannels[i])
		{
			mChannels[i]->updateBuffer();
			mChannels[i]->update3DPosition();
			mChannels[i]->updateLoop();
#ifdef SHOW_ASSERT
			if(mChannels[i]->getSource())
				llassert(mChannels[i]->mCurrentBufferp == mChannels[i]->getSource()->getCurrentBuffer());
			if(mChannels[i]->mCurrentBufferp)
			{
				bool found_buffer = false;
				for (S32 j = 0; j < MAX_BUFFERS; j++)
				{
					if (mBuffers[j])
					{
						if(mChannels[i]->mCurrentBufferp == mBuffers[j])
							found_buffer = true;
					}
				}
				llassert(found_buffer);
			}
#endif //SHOW_ASSERT
		}
	}
	checkStates();
}

struct SourcePriorityComparator
{
	bool operator() (const LLAudioSource* lhs, const LLAudioSource* rhs) const
	{
		if(rhs->getPriority() != lhs->getPriority())
			return rhs->getPriority() > lhs->getPriority();
		else
			return ((rhs->isSyncMaster() && !lhs->isSyncMaster()) || (rhs->isSyncSlave() && (!lhs->isSyncMaster() && !lhs->isSyncSlave())));
	}
};

static const F32 default_max_decode_time = .002f; // 2 ms
void LLAudioEngine::idle(F32 max_decode_time)
{
	if (max_decode_time <= 0.f)
	{
		max_decode_time = default_max_decode_time;
	}
	
	// "Update" all of our audio sources, clean up dead ones.
	// Primarily does position updating, cleanup of unused audio sources.
	// Also does regeneration of the current priority of each audio source.

	S32 i;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i])
		{
			mBuffers[i]->mInUse = false;
		}
	}

	//Iterate down all queued sources. Remove finished ones, sort active ones by priority. Find highest priority 'master' source if present.
	LLAudioSource *sync_masterp = NULL;	//Highest priority syncmaster
	LLAudioSource *sync_slavep = NULL;	//Highest priority syncslave

	//Priority queue that will be filled with soundsources that have a high likeleyhood of being able to start [re]playing this idle tick.
	//Sort order:  Primary: priority. Secondary: syncmaster. Tertiary: syncslave
	std::priority_queue<LLAudioSource*,std::vector<LLAudioSource*,boost::pool_allocator<LLAudioSource*> >,SourcePriorityComparator> queue;

	//Have to put syncslaves into a temporary list until the syncmaster state is determined.
	//If the syncmaster might be started, or just looped, insert all pending/looping syncslaves into the priority queue.
	//If the there is no active syncmaster, then stop any currently looping syncslaves and add none to the priority queue.
	//This is necessary as any looping soundsources to be stopped, MUST be stopped before iterating down the priority queue.
	static std::vector<LLAudioSource*> slave_list;	
	slave_list.clear();

	//Iterate over all sources. Update their decode or 'done' state, as well as their priorities.
	//Also add sources that might be able to start playing to the priority queue.
	//Only sources without channels should be added to priority queue.
	//Syncslaves must put into the slave list instead of the priority queue.
	for (source_map::iterator iter = mAllSources.begin(); iter != mAllSources.end();)
	{
		LLAudioSource *sourcep = iter->second;

		//Load/decode/queue pop
		sourcep->update();

		//Done after update, as failure to load might mark source as corrupt, which causes isDone to return true.
		if (sourcep->isDone())		
		{
			// The source is done playing, clean it up.
			delete sourcep;
			mAllSources.erase(iter++);
			continue;
		}

		// Increment iter here (it is not used anymore), so we can use continue below to move on to the next source.
		++iter;

		if(!sourcep->isLoop() && sourcep->mPlayedOnce && (!sourcep->mChannelp || !sourcep->mChannelp->isPlaying()))
		{
			continue;
		}

		LLAudioData *adp = sourcep->getCurrentData();
		//If there is no current data at all, or if it hasn't loaded, we must skip this source.
		if (!adp || !adp->getBuffer())
		{
			continue;
		}

		sourcep->updatePriority();	//Calculates current priority. 1.f=ambient. 0.f=muted. Otherwise based off of distance.

		if (sourcep->getPriority() < F_APPROXIMATELY_ZERO)
		{
			// Muted, or nothing queued, or too far, so we don't care.
			continue;
		}
		else if(sourcep->isSyncMaster())
		{
			if(!sync_masterp || sourcep->getPriority() > sync_masterp->getPriority())
			{
				if(sync_masterp && !sync_masterp->getChannel())
					queue.push(sync_masterp);	//Add lower-priority soundmaster to the queue as a normal sound.	
				sync_masterp = sourcep;
				//Don't add master to the queue yet.
				//Add it after highest-priority sound slave is found so we can outrank its priority.
				continue;	
			}
			//Else fall through like a normal sound.
		}
		else if(sourcep->isSyncSlave())
		{
			if(!sync_slavep || sourcep->getPriority() > sync_slavep->getPriority())
			{
				sync_slavep = sourcep;
			}
			//Don't add to the priority queue quite yet. Best primary syncmaster candidate may not have been determined yet.
			slave_list.push_back(sourcep);
			continue;
		}
		if(sourcep->getChannel())
		{
			//If this is just a regular sound and is currently playing then do nothing special.
			continue;
		}
		//Add to our queue. Highest priority will be at the front.
		queue.push(sourcep);		
	}

	// Do this BEFORE we update the channels
	// Update the channels to sync up with any changes that the source made,
	// such as changing what sound was playing.
	updateChannels();

	//Loop over all syncsaves. If no syncmaster, stop looping ones, else add ones that need [re]started to the priority queue.
	//Normally there are zero to one syncslaves, so this loop isn't typically expensive.
	for(std::vector<LLAudioSource*>::iterator iter=slave_list.begin();iter!=slave_list.end();++iter)
	{
		if(!sync_masterp)
		{
			//Stop any looping syncslaves. I'm not sure this behavior is correct, but letting looping syncslaves run
			//off on their own seems wrong. The lsl documentation is unclear.
			if((*iter)->getChannel() && (*iter)->isLoop())
			{
				(*iter)->getChannel()->cleanup();
			}
		}
		//If the syncmaster already has a channel and hasn't looped, then we can skip syncslaves this idle tick (there's nothing to do).
		else if((!sync_masterp->getChannel() || sync_masterp->getChannel()->mLoopedThisFrame))
		{
			//Add syncslaves that we might want to [re]start.
			if(!(*iter)->getChannel() || (*iter)->isLoop())
				queue.push((*iter));		
		}
	}

	if(sync_masterp)	
	{
		//Syncmaster must have priority greater than or equal to priority of highest priority syncslave.
		//The case of slave priority equaling master priority is handled in the comparator (SourcePriorityComparator).
		if(sync_slavep)
			sync_masterp->setPriority(sync_slavep->getPriority());
		if(!sync_masterp->getChannel())
		{
			queue.push(sync_masterp);
		}
	}

	// Dispatch all soundsources.
	bool syncmaster_started = sync_masterp && sync_masterp->getChannel() && sync_masterp->getChannel()->mLoopedThisFrame;
	while(!queue.empty())
	{
		LLAudioSource *sourcep = queue.top();
		queue.pop();

		//If the syncmaster hasn't just started playing, or hasn't just looped then skip this soundslave,
		//as there's nothing to do with it yet.
		if (sourcep->isSyncSlave() && !syncmaster_started)
		{
			continue;
		}

		LLAudioChannel *channelp = sourcep->getChannel();

		if (!channelp)
		{
			//No more channels. We can break here, as in theory, any lower priority sounds should have had their 
			//channel already stolen. There should be nothing playing, nor playable, when iterating beyond this point.
			if(!(channelp = getFreeChannel(sourcep->getPriority())))
			{
				break;
			}

			//If this is the primary syncmaster that we just started, then [re]start syncslaves further down in the priority queue.
			//Due to sorting and priority fudgery, the syncmaster is ALWAYS before any syncslaves in this loop.
			syncmaster_started |= (sourcep == sync_masterp);

			//setSource calls updateBuffer and update3DPosition, and resets the source mAgeTimer
			channelp->setSource(sourcep);	
		}

		if(sourcep->isSyncSlave())
			channelp->playSynced(sync_masterp->getChannel());
		else
			channelp->play();
	}

	// Sync up everything that the audio engine needs done.
	commitDeferredChanges();
	
	// Flush unused buffers that are stale enough
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i])
		{
			if (!mBuffers[i]->mInUse && mBuffers[i]->mLastUseTimer.getElapsedTimeF32() > 30.f)
			{
				LL_DEBUGS("AudioEngine") << "Flushing unused buffer!" << LL_ENDL;
				mBuffers[i]->mAudioDatap->mBufferp = NULL;
				delete mBuffers[i];
				mBuffers[i] = NULL;
			}
		}
	}


	// Clear all of the looped flags for the channels
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (mChannels[i])
		{
			mChannels[i]->mLoopedThisFrame = false;
		}
	}

	// Decode audio files
	gAudioDecodeMgrp->processQueue(max_decode_time);
	
	// Just call here every frame. It makes little sense to call elsewhere,
	// as it's throttled to one active preloading loading sound at a time anyhow
	startNextTransfer();

	updateInternetStream();
}

void LLAudioEngine::enableWind(bool enable)
{
	if (enable && (!mEnableWind))
	{
		mEnableWind = initWind();
	}
	else if (mEnableWind && (!enable))
	{
		mEnableWind = false;
		cleanupWind();
	}
}


LLAudioBuffer * LLAudioEngine::getFreeBuffer()
{
	//checkStates();	//Fails

	S32 i;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (!mBuffers[i])
		{
			mBuffers[i] = createBuffer();
			return mBuffers[i];
		}
	}

	//checkStates();	// Fails

	// Grab the oldest unused buffer
	F32 max_age = -1.f;
	S32 buffer_id = -1;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i])
		{
			if (!mBuffers[i]->mInUse)
			{
				if (mBuffers[i]->mLastUseTimer.getElapsedTimeF32() > max_age)
				{
					max_age = mBuffers[i]->mLastUseTimer.getElapsedTimeF32();
					buffer_id = i;
				}
			}
		}
	}

	//checkStates();	//Fails

	if (buffer_id >= 0)
	{
		LL_DEBUGS("AudioEngine") << "Taking over unused buffer! max_age=" << max_age << LL_ENDL;
		mBuffers[buffer_id]->mAudioDatap->mBufferp = NULL;
		for (U32 i = 0; i < MAX_CHANNELS; i++)
		{
			LLAudioChannel* channelp = mChannels[i];
			if(channelp && channelp->mCurrentBufferp == mBuffers[buffer_id])
			{
				channelp->cleanup();
				llassert(channelp->mCurrentBufferp == NULL);
			}
		}
		delete mBuffers[buffer_id];
		mBuffers[buffer_id] = createBuffer();
		return mBuffers[buffer_id];
	}

	//checkStates();	//Fails
	return NULL;
}


LLAudioChannel * LLAudioEngine::getFreeChannel(const F32 priority)
{
	S32 i;
	for (i = 0; i < mNumChannels; i++)
	{
		if (!mChannels[i])
		{
			// No channel allocated here, use it.
			mChannels[i] = createChannel();
			return mChannels[i];
		}
		else
		{
			// Channel is allocated but not playing right now, use it.
			if (mChannels[i]->isFree())
			{
				LL_DEBUGS("AudioEngine") << "Replacing unused channel" << LL_ENDL;
				return mChannels[i];
			}
		}
	}

	// All channels used, check priorities.
	// Find channel with lowest priority and see if we want to replace it.
	F32 min_priority = 10000.f;
	LLAudioChannel *min_channelp = NULL;

	for (i = 0; i < mNumChannels; i++)
	{
		LLAudioChannel *channelp = mChannels[i];
		LLAudioSource *sourcep = channelp->getSource();
		if (sourcep->getPriority() < min_priority)
		{
			min_channelp = channelp;
			min_priority = sourcep->getPriority();
		}
	}

	if (!min_channelp || min_priority >= priority)
	{
		// All playing channels have higher priority, return.
		return NULL;
	}

	LL_DEBUGS("AudioEngine") << "Flushing min channel" << LL_ENDL;
	// Flush the minimum priority channel, and return it.
	min_channelp->cleanup();
	return min_channelp;
}


void LLAudioEngine::cleanupBuffer(LLAudioBuffer *bufferp)
{
	S32 i;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i] == bufferp)
		{
			delete mBuffers[i];
			mBuffers[i] = NULL;
		}
	}
}


bool LLAudioEngine::preloadSound(const LLUUID &uuid)
{
	if(uuid.isNull())
		return false;

	if(getAudioData(uuid)->getLoadState() >= LLAudioData::STATE_LOAD_DECODING)
	{
		// This means that we do have a local copy, and we're working on decoding it.
		return true;
	}

	LL_INFOS("AudioEngine") << "Preloading system sound " << uuid << LL_ENDL;
	mPreloadSystemList.push_back(uuid);

	// At some point we need to have the audio/asset system check the static VFS
	// before it goes off and fetches stuff from the server.
	//LL_INFOS("AudioEngine") << "Used internal preload for non-local sound" << LL_ENDL;
	return true;
}


bool LLAudioEngine::isWindEnabled()
{
	return mEnableWind;
}


void LLAudioEngine::setMuted(bool muted)
{
	if (muted != mMuted)
	{
		mMuted = muted;
		setMasterGain(mMasterGain);
	}
	enableWind(!mMuted);
}

void LLAudioEngine::setMasterGain(const F32 gain)
{
	mMasterGain = gain;
	F32 internal_gain = getMuted() ? 0.f : gain;
	if (internal_gain != mInternalGain)
	{
		mInternalGain = internal_gain;
		setInternalGain(mInternalGain);
	}
}

F32 LLAudioEngine::getMasterGain()
{
	return mMasterGain;
}

void LLAudioEngine::setSecondaryGain(S32 type, F32 gain)
{
	llassert(type < LLAudioEngine::AUDIO_TYPE_COUNT);
	
	mSecondaryGain[type] = gain;
}

F32 LLAudioEngine::getSecondaryGain(S32 type)
{
	return mSecondaryGain[type];
}

F32 LLAudioEngine::getInternetStreamGain()
{
	if (mStreamingAudioImpl)
		return mStreamingAudioImpl->getGain();
	else
		return 1.0f;
}

void LLAudioEngine::setMaxWindGain(F32 gain)
{
	mMaxWindGain = gain;
}


F64 LLAudioEngine::mapWindVecToGain(LLVector3 wind_vec)
{
	F64 gain = 0.0;
	
	gain = wind_vec.magVec();

	if (gain)
	{
		if (gain > 20)
		{
			gain = 20;
		}
		gain = gain/20.0;
	}

	return (gain);
} 


F64 LLAudioEngine::mapWindVecToPitch(LLVector3 wind_vec)
{
	LLVector3 listen_right;
	F64 theta;
  
	// Wind frame is in listener-relative coordinates
	LLVector3 norm_wind = wind_vec;
	norm_wind.normVec();
	listen_right.setVec(1.0,0.0,0.0);

	// measure angle between wind vec and listener right axis (on 0,PI)
	theta = acos(norm_wind * listen_right);

	// put it on 0, 1
	theta /= F_PI;					

	// put it on [0, 0.5, 0]
	if (theta > 0.5) theta = 1.0-theta;
	if (theta < 0) theta = 0;

	return (theta);
}


F64 LLAudioEngine::mapWindVecToPan(LLVector3 wind_vec)
{
	LLVector3 listen_right;
	F64 theta;
  
	// Wind frame is in listener-relative coordinates
	listen_right.setVec(1.0,0.0,0.0);

	LLVector3 norm_wind = wind_vec;
	norm_wind.normVec();

	// measure angle between wind vec and listener right axis (on 0,PI)
	theta = acos(norm_wind * listen_right);

	// put it on 0, 1
	theta /= F_PI;					

	return (theta);
}


void LLAudioEngine::triggerSound(const LLUUID &audio_uuid, const LLUUID& owner_id, const F32 gain,
								  // <edit>
								 //const S32 type, const LLVector3d &pos_global)
								 const S32 type, const LLVector3d &pos_global, const LLUUID source_object)
								 // </edit>
{
	if(!mListenerp)
		return;
	// Create a new source (since this can't be associated with an existing source.
	//LL_INFOS("AudioEngine") << "Localized: " << audio_uuid << LL_ENDL;

	if (mMuted || gain < FLT_EPSILON*2)
	{
		return;
	}

	LLUUID source_id;
	source_id.generate();

	// <edit>
	//LLAudioSource *asp = new LLAudioSource(source_id, owner_id, gain, type);
	LLAudioSource *asp = new LLAudioSource(source_id, owner_id, gain, type, source_object, true);
	// </edit>
	gAudiop->addAudioSource(asp);
	if (pos_global.isExactlyZero())
	{
		asp->setAmbient(true);
	}
	else
	{
		asp->setPositionGlobal(pos_global);
	}
	asp->updatePriority();
	asp->play(audio_uuid);
}


void LLAudioEngine::setListenerPos(LLVector3 aVec)
{
	if (mListenerp)
	{
		mListenerp->setPosition(aVec);
	}
}


LLVector3 LLAudioEngine::getListenerPos()
{
	if (mListenerp)
	{
		return mListenerp->getPosition();  
	}
	else
	{
		return(LLVector3::zero);
	}
}


void LLAudioEngine::setListenerVelocity(LLVector3 aVec)
{
	if (mListenerp)
	{
		mListenerp->setVelocity(aVec);
	}
}


void LLAudioEngine::translateListener(LLVector3 aVec)
{
	if (mListenerp)
	{
		mListenerp->translate(aVec);
	}
}


void LLAudioEngine::orientListener(LLVector3 up, LLVector3 at)
{
	if (mListenerp)
	{
		mListenerp->orient(up, at);  
	}
}


void LLAudioEngine::setListener(LLVector3 pos, LLVector3 vel, LLVector3 up, LLVector3 at)
{
	if(!mListenerp)
	{
		allocateListener();
	}
	mListenerp->set(pos,vel,up,at);  
}


void LLAudioEngine::setDopplerFactor(F32 factor)
{
	if (mListenerp)
	{
		mListenerp->setDopplerFactor(factor);  
	}
}


F32 LLAudioEngine::getDopplerFactor()
{
	if (mListenerp)
	{
		return mListenerp->getDopplerFactor();
	}
	else
	{
		return 0.f;
	}
}


void LLAudioEngine::setRolloffFactor(F32 factor)
{
	if (mListenerp)
	{
		mListenerp->setRolloffFactor(factor);  
	}
}


F32 LLAudioEngine::getRolloffFactor()
{
	if (mListenerp)
	{
		return mListenerp->getRolloffFactor();  
	}
	else
	{
		return 0.f;
	}
}


void LLAudioEngine::commitDeferredChanges()
{
	if(mListenerp)
	{
		mListenerp->commitDeferredChanges();  
	}
}


LLAudioSource * LLAudioEngine::findAudioSource(const LLUUID &source_id)
{
	source_map::iterator iter;
	iter = mAllSources.find(source_id);

	if (iter == mAllSources.end())
	{
		return NULL;
	}
	else
	{
		return iter->second;
	}
}


LLAudioData * LLAudioEngine::getAudioData(const LLUUID &audio_uuid)
{
	data_map::iterator iter;
	iter = mAllData.find(audio_uuid);
	if (iter == mAllData.end())
	{
		// Create the new audio data
		LLAudioData *adp = new LLAudioData(audio_uuid);
		mAllData[audio_uuid] = adp;
		return adp;
	}
	else
	{
		return iter->second;
	}
}

 void LLAudioEngine::removeAudioData(LLUUID &audio_uuid)
 {
	if(audio_uuid.isNull())
		return;
	data_map::iterator iter = mAllData.find(audio_uuid);
	if(iter != mAllData.end())
 	{

		for (source_map::iterator iter2 = mAllSources.begin(); iter2 != mAllSources.end();)
		{
			LLAudioSource *sourcep = iter2->second;
			if(	sourcep && sourcep->getCurrentData() && sourcep->getCurrentData()->getID() == audio_uuid )
			{
				LLAudioChannel* chan=sourcep->getChannel();
				if(chan)
				{
					LL_DEBUGS("AudioEngine") << "removeAudioData" << LL_ENDL;
					chan->cleanup();
				}
				delete sourcep;
				mAllSources.erase(iter2++);
			}
			else
				++iter2;
		}
		if(iter->second) //Shouldn't be null, but playing it safe.
		{
			LLAudioBuffer* buf=((LLAudioData*)iter->second)->getBuffer();
			if(buf)
			{
				for (S32 i = 0; i < MAX_BUFFERS; i++)
				{
					if(mBuffers[i] == buf)
						mBuffers[i] = NULL;
				}
				delete buf;
			}
			delete iter->second;
		}
		mAllData.erase(iter);
 	}
 }
void LLAudioEngine::addAudioSource(LLAudioSource *asp)
{
	mAllSources[asp->getID()] = asp;
}


void LLAudioEngine::cleanupAudioSource(LLAudioSource *asp)
{
	source_map::iterator iter;
	iter = mAllSources.find(asp->getID());
	if (iter == mAllSources.end())
	{
		LL_WARNS("AudioEngine") << "Cleaning up unknown audio source!" << LL_ENDL;
		return;
	}
	else
	{
		LL_DEBUGS("AudioEngine") << "Cleaning up audio sources for "<< asp->getID() <<LL_ENDL;
		delete asp;
		mAllSources.erase(iter);
	}
}


bool LLAudioEngine::hasDecodedFile(const LLUUID &uuid)
{
	std::string uuid_str;
	uuid.toString(uuid_str);

	std::string wav_path;
	wav_path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_str);
	wav_path += ".dsf";

	if (gDirUtilp->fileExists(wav_path))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void LLAudioEngine::startNextTransfer()
{
	//LL_INFOS("AudioEngine") << "LLAudioEngine::startNextTransfer()" << LL_ENDL;
	if (getMuted())
	{
		return;
	}
	else if(mCurrentTransferTimer.getElapsedTimeF32() <= .1f)
	{
		return;
	}
	else if(mCurrentTransfer && mCurrentTransfer->isInPreload())
	{
		//Keep updating until it either errors out or completes.
		mCurrentTransfer->updateLoadState();	
		return;
	}
	else
	{
		mCurrentTransfer = NULL;
	}

	//Technically, mCurrentTransfer could end up pointing to an audiodata object that's already
	//being transmitted/decoded if such was spawned via needing it for playback immediately.
	//This will effectively block us from choosing a lower priority audiodata object until the
	//immediate ones are done, but it's not a real problem.

	// Get the ID for the next asset that we want to transfer.
	// Pick one in the following order:
	S32 i;
	LLAudioSource *asp = NULL;
	LLAudioData *adp = NULL;
	LLAudioData *cur_adp = NULL;
	data_map::iterator data_iter;

	// Check all channels for currently playing sounds.
	F32 max_pri = -1.f;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (!mChannels[i])
		{
			continue;
		}

		asp = mChannels[i]->getSource();
		if (!asp)
		{
			continue;
		}
		if (asp->getPriority() <= max_pri)
		{
			continue;
		}

		if (asp->getPriority() <= max_pri)
		{
			continue;
		}

		adp = asp->getCurrentData();
		if (!adp)
		{
			continue;
		}

		if (adp->isInPreload())
		{
			max_pri = asp->getPriority();
			cur_adp = adp;
		}
	}

	// Check all channels for currently queued sounds.
	if (!cur_adp)
	{
		max_pri = -1.f;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			if (!mChannels[i])
			{
				continue;
			}

			LLAudioSource *asp;
			asp = mChannels[i]->getSource();
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}

			adp = asp->getQueuedData();
			if (!adp)
			{
				continue;
			}

			if (adp->isInPreload())
			{
				max_pri = asp->getPriority();
				cur_adp = adp;
			}
		}
	}

	// Check all live channels for other sounds (preloads).
	if (!cur_adp)
	{
		max_pri = -1.f;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			if (!mChannels[i])
			{
				continue;
			}

			LLAudioSource *asp;
			asp = mChannels[i]->getSource();
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}


			for (data_iter = asp->mPreloadMap.begin(); data_iter != asp->mPreloadMap.end(); data_iter++)
			{
				LLAudioData *adp = data_iter->second;
				if (!adp)
				{
					continue;
				}

				if (adp->isInPreload())
				{
					max_pri = asp->getPriority();
					cur_adp = adp;
				}
			}
		}
	}

	// Check all sources
	if (!cur_adp)
	{
		max_pri = -1.f;
		source_map::iterator source_iter;
		for (source_iter = mAllSources.begin(); source_iter != mAllSources.end(); source_iter++)
		{
			asp = source_iter->second;
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}

			adp = asp->getCurrentData();
			if (adp && adp->isInPreload())
			{
				max_pri = asp->getPriority();
				cur_adp = adp;
				continue;
			}

			adp = asp->getQueuedData();
			if (adp && adp->isInPreload())
			{
				max_pri = asp->getPriority();
				cur_adp = adp;
				continue;
			}

			for (data_iter = asp->mPreloadMap.begin(); data_iter != asp->mPreloadMap.end(); data_iter++)
			{
				LLAudioData *adp = data_iter->second;
				if (!adp)
				{
					continue;
				}

				if (adp->isInPreload())
				{
					max_pri = asp->getPriority();
					cur_adp = adp;
					break;
				}
			}
		}
	}

	if (!cur_adp)
	{
		while(!mPreloadSystemList.empty())
		{
			adp = getAudioData(mPreloadSystemList.front());
			mPreloadSystemList.pop_front();
			if(adp->isInPreload())
			{
				cur_adp = adp;
				break;
			}
		}
	}
	else if(cur_adp)
	{
		std::list<LLUUID>::iterator it = std::find(mPreloadSystemList.begin(),mPreloadSystemList.end(),cur_adp->getID());
		if(it != mPreloadSystemList.end())
			mPreloadSystemList.erase(it);
	}

	if (cur_adp)
	{
		mCurrentTransfer = cur_adp;
		mCurrentTransferTimer.reset();
		mCurrentTransfer->updateLoadState();
	}
	else
	{
		//LL_INFOS("AudioEngine") << "No pending transfers?" << LL_ENDL;
	}
}


// static
void LLAudioEngine::assetCallback(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type, void *user_data, S32 result_code, LLExtStat ext_status)
{
	if(!gAudiop)
		return;

	LLAudioData *adp = gAudiop->getAudioData(uuid);

	if (result_code)
	{
		LL_INFOS("AudioEngine") << "Boom, error in audio file transfer: " << LLAssetStorage::getErrorString( result_code ) << " (" << result_code << ")" << LL_ENDL;
		// Need to mark data as bad to avoid constant rerequests.
		
		if (adp)
		{	
			adp->setLoadState(LLAudioData::STATE_LOAD_ERROR);
		}
	}
	else
	{
		if (!adp)
        {
			// Should never happen
			LL_WARNS("AudioEngine") << "Got asset callback without audio data for " << uuid << LL_ENDL;
        }
		else
		{
			// LL_INFOS("AudioEngine") << "Got asset callback with good audio data for " << uuid << ", making decode request" << LL_ENDL;
			adp->setLoadState(LLAudioData::STATE_LOAD_REQ_DECODE);
			//Immediate decode.
			adp->updateLoadState();
		}
	}
}


//
// LLAudioSource implementation
//


// <edit>
//LLAudioSource::LLAudioSource(const LLUUID& id, const LLUUID& owner_id, const F32 gain, const S32 type)
LLAudioSource::LLAudioSource(const LLUUID& id, const LLUUID& owner_id, const F32 gain, const S32 type, const LLUUID source_id, const bool isTrigger)
// </edit>
:	mID(id),
	mOwnerID(owner_id),
	mPriority(0.f),
	mGain(gain),
	mSourceMuted(false),
	mAmbient(false),
	mLoop(false),
	mSyncMaster(false),
	mSyncSlave(false),
	mQueueSounds(false),
	mPlayedOnce(false),
	mCorrupted(false),
	mType(type),
	// <edit>
	mSourceID(source_id),
	mIsTrigger(isTrigger),
	// </edit>
	mChannelp(NULL),
	mCurrentDatap(NULL),
	mQueuedDatap(NULL)
{
	// <edit>
	mLogID.generate();
	// </edit>
}


LLAudioSource::~LLAudioSource()
{
	// <edit>
	// Record destruction of LLAudioSource object.
	if(mType != LLAudioEngine::AUDIO_TYPE_UI)
		logSoundStop(mLogID, true);
	// </edit>
	if (mChannelp)
	{
		// Stop playback of this sound
		mChannelp->cleanup();
	}
}


void LLAudioSource::setChannel(LLAudioChannel *channelp)
{
	if (channelp == mChannelp)
	{
		return;
	}

	//Either this channel just finished playing, or just started. Either way, reset the age timer.
	mAgeTimer.reset();	

	// <edit>
	if (!channelp)
	{
		if(mType != LLAudioEngine::AUDIO_TYPE_UI)
			logSoundStop(mLogID, false);
	}
	// </edit>

	mChannelp = channelp;
}

void LLAudioSource::update()
{
	if(mCorrupted)
	{
		return ; //no need to update
	}

	// If data is queued up and we aren't playing it, shuffle it to current and try to load it.
	if(isQueueSounds() && mPlayedOnce && mQueuedDatap && !mChannelp)	
	{
		mCurrentDatap = mQueuedDatap;
		mQueuedDatap = NULL;

		//Make sure this source looks like its brand new again to prevent removal.
		mPlayedOnce = false;
		mAgeTimer.reset();	
	}

	LLAudioData *adp = getCurrentData();
	if (adp && !adp->getBuffer())
	{
		if(adp->getLoadState() == LLAudioData::STATE_LOAD_ERROR)
		{
			LL_WARNS("AudioEngine") << "Marking LLAudioSource corrupted for " << adp->getID() << LL_ENDL;
			mCorrupted = true ;
		}
		else if(adp->getLoadState() == LLAudioData::STATE_LOAD_READY)
		{
			// Update the audio buffer first - load a sound if we have it.
			// Note that this could potentially cause us to waste time updating buffers
			// for sounds that actually aren't playing, although this should be mitigated
			// by the fact that we limit the number of buffers, and we flush buffers based
			// on priority.
			adp->load();	//If it fails, just try again next update.
		}
		else
		{
			//The sound wasn't preloaded yet... so we must kick off the process.
			adp->updateLoadState();
		}
	}
}

void LLAudioSource::updatePriority()
{
	if (isAmbient())
	{
		setPriority(1.f);
	}
	else if (isMuted())
	{
		setPriority(0.f);
	}
	else
	{
		// Priority is based on distance
		LLVector3 dist_vec;
		dist_vec.setVec(getPositionGlobal());
		dist_vec -= gAudiop->getListenerPos();
		F32 dist_squared = llmax(1.f, dist_vec.magVecSquared());

		setPriority(mGain / dist_squared);
	}
}

bool LLAudioSource::play(const LLUUID &audio_uuid)
{
	// Special abuse of play(); don't play a sound, but kill it.
	if (audio_uuid.isNull())
	{
		if (getChannel())
		{
			llassert(this == getChannel()->getSource());
			getChannel()->cleanup();
			if (!isMuted())
			{
				mCurrentDatap = NULL;
			}
		}
		return false;
	}

	// <edit>
	if(mType != LLAudioEngine::AUDIO_TYPE_UI) //&& mSourceID.notNull())
		logSoundPlay(this, audio_uuid);
	// </edit>

	// Reset our age timeout if someone attempts to play the source.
	mAgeTimer.reset();

	LLAudioData *adp = gAudiop->getAudioData(audio_uuid);

	if (isQueueSounds())
	{
		if(mQueuedDatap)
		{
			// We already have a sound queued up. Ignore new request.
			return false;
		}
		else if (adp == mCurrentDatap && isLoop())
		{
			// No point in queuing the same sound if
			// we're looping.
			return true;
		}
		else if(mCurrentDatap)
		{
			mQueuedDatap = adp;
			return true;
		}
	}
	else if(mCurrentDatap == adp)	//Desired sound did not change. Just re-play it.
	{
		if(getChannel() && getChannel()->isPlaying())
			getChannel()->play();
		return true;
	}
	else							//Desired sound did change. Release the old channel if set.
	{
		if(getChannel())
			getChannel()->cleanup();
		mPlayedOnce = false;		//Reset the played flag so the new sound is actually started up.
	}

	mCurrentDatap = adp;

	return true;
}


bool LLAudioSource::isDone() const
{
	static const F32 MAX_AGE = 60.f;
	static const F32 MAX_UNPLAYED_AGE = 15.f;
	static const F32 MAX_MUTED_AGE = 11.f;
	if(mCorrupted)
	{
		// If we decode bad data then just kill this source entirely.
		return true;
	}
	else if (isLoop())
	{
		// Looped sources never die on their own.
		return false;
	}
	else if (hasPendingPreloads())
	{
		// If there are pending preload requests then keep it alive.
		return false;
	}
	else if (mQueuedDatap)
	{
		// Don't kill this sound if we've got something queued up to play.
		return false;
	}
	else if(mPlayedOnce && (!mChannelp || !mChannelp->isPlaying()))
	{
		// This is a single-play source and it already did its thing.
		return true;
	}

	F32 elapsed = mAgeTimer.getElapsedTimeF32();

	if (!mChannelp)
	{
		LLAudioData* adp = mCurrentDatap;

		//Still decoding.
		if(adp && adp->isInPreload())
			return false;

		// We don't have a channel assigned, and it's been
		// over 15 seconds since we tried to play it.  Don't bother.
		return (elapsed > (mSourceMuted ? MAX_MUTED_AGE : MAX_UNPLAYED_AGE));
	}
	else if (mChannelp->isPlaying())
	{
		// Arbitarily cut off non-looped sounds when they're old.
		return elapsed > MAX_AGE;
	}
	else if(!isSyncSlave())
	{
		// The sound isn't playing back after 15 seconds, kill it.
		// This might happen if all channels are in use and this source is low-priority
		return elapsed > MAX_UNPLAYED_AGE;
	}

	return false;
}


void LLAudioSource::preload(const LLUUID &audio_id)
{
	if(audio_id.notNull())
	{
		// Add it to the preload list.
		mPreloadMap[audio_id] = gAudiop->getAudioData(audio_id);
	}
}


bool LLAudioSource::hasPendingPreloads() const
{
	// Check to see if we've got any preloads on deck for this source
	data_map::const_iterator iter;
	for (iter = mPreloadMap.begin(); iter != mPreloadMap.end(); iter++)
	{
		LLAudioData *adp = iter->second;
		// note: a bad UUID will forever be !hasDecodedData()
		// but also !hasValidData(), hence the check for hasValidData()
		if (!adp)
		{
			continue;
		}
		if (adp->isInPreload())
		{
			// This source is still waiting for a preload
			return true;
		}
	}

	return false;
}


LLAudioData * LLAudioSource::getCurrentData()
{
	return mCurrentDatap;
}

LLAudioData * LLAudioSource::getQueuedData()
{
	return mQueuedDatap;
}

LLAudioBuffer * LLAudioSource::getCurrentBuffer()
{
	if (!mCurrentDatap)
	{
		return NULL;
	}

	return mCurrentDatap->getBuffer();
}




//
// LLAudioChannel implementation
//


LLAudioChannel::LLAudioChannel() :
	mCurrentSourcep(NULL),
	mCurrentBufferp(NULL),
	mLoopedThisFrame(false),
	mSecondaryGain(1.0f)
{
}

LLAudioChannel::~LLAudioChannel()
{
	llassert(mCurrentBufferp == NULL);
	
	// Need to disconnect any sources which are using this channel.
	//LL_INFOS("AudioEngine") << "Cleaning up audio channel" << LL_ENDL;
	cleanup();
}

void LLAudioChannel::cleanup()
{
	if(mCurrentSourcep)
		mCurrentSourcep->setChannel(NULL);
	mCurrentBufferp = NULL;
	mCurrentSourcep = NULL;
}


void LLAudioChannel::setSource(LLAudioSource *sourcep)
{
	llassert_always(sourcep);
	llassert_always(!mCurrentSourcep);

	mCurrentSourcep = sourcep;
	mCurrentSourcep->setChannel(this);
	
	updateBuffer();
	update3DPosition();
}


bool LLAudioChannel::updateBuffer()
{
	if (!mCurrentSourcep)
	{
		// This channel isn't associated with any source, nothing
		// to be updated
		return false;
	}

	// Initialize the channel's gain setting for this sound.
	if(gAudiop)
	{
		setSecondaryGain(gAudiop->getSecondaryGain(mCurrentSourcep->getType()));
	}

	LLAudioBuffer *bufferp = mCurrentSourcep->getCurrentBuffer();

	//This buffer is still in use, so mark it as such, and reset the use timer.
	if (bufferp)
	{
		bufferp->mLastUseTimer.reset();
		bufferp->mInUse = true;
	}

	//Return true if the buffer changed and is not null.
	return bufferp != mCurrentBufferp && !!(mCurrentBufferp = bufferp);
}




//
// LLAudioData implementation
//


LLAudioData::LLAudioData(const LLUUID &uuid) :
	mID(uuid),
	mBufferp(NULL),
	mLoadState(STATE_LOAD_ERROR)
{
	if (uuid.isNull())
	{
		// This is a null sound.
		return;
	}

	if(gAudiop->hasDecodedFile(getID()))
		mLoadState = STATE_LOAD_READY;
	else if(gAssetStorage && gAssetStorage->hasLocalAsset(getID(), LLAssetType::AT_SOUND))
		mLoadState = STATE_LOAD_REQ_DECODE;
	else
		mLoadState = STATE_LOAD_REQ_FETCH;
}

void LLAudioData::updateLoadState()
{
	if(mLoadState == STATE_LOAD_REQ_DECODE && gAudioDecodeMgrp)
	{
		if(	gAudioDecodeMgrp->addDecodeRequest(getID()) )
		{
			setLoadState(STATE_LOAD_DECODING);
			LL_DEBUGS("AudioEngine") << "Decoding asset data for: " << getID() << LL_ENDL;
		}
		else
		{
			setLoadState(STATE_LOAD_ERROR);
		}
	}
	else if(mLoadState == STATE_LOAD_REQ_FETCH && gAssetStorage && gAssetStorage->isUpstreamOK())
	{
		LL_DEBUGS("AudioEngine") << "Fetching asset data for: " << getID() << LL_ENDL;
		setLoadState(STATE_LOAD_FETCHING);

		gAssetStorage->getAssetData(getID(), LLAssetType::AT_SOUND, LLAudioEngine::assetCallback, NULL);
	}
}

//return false when the audio file is corrupted.
bool LLAudioData::load()
{
	// For now, just assume we're going to use one buffer per audiodata.
	if (mBufferp)
	{
		// We already have this sound in a buffer, don't do anything.
		LL_INFOS("AudioEngine") << "Already have a buffer for this sound, don't bother loading!" << LL_ENDL;
		return true;
	}
	
	mBufferp = gAudiop->getFreeBuffer();
	if (!mBufferp)
	{
		// No free buffers, abort.
		LL_DEBUGS("AudioEngine") << "Not able to allocate a new audio buffer, aborting." << LL_ENDL;
		return false;
	}

	std::string uuid_str;
	std::string wav_path;
	mID.toString(uuid_str);
	wav_path= gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_str) + ".dsf";

	if (!mBufferp->loadWAV(wav_path))
	{
		// Hrm.  Right now, let's unset the buffer, since it's empty.
		gAudiop->cleanupBuffer(mBufferp);
		mBufferp = NULL;

		return false;
	}
	mBufferp->mAudioDatap = this;
	return true;
}

// <edit>
std::map<LLUUID, LLSoundHistoryItem> gSoundHistory;

// static
void logSoundPlay(LLAudioSource* audio_source, LLUUID const& assetid)
{
	LLSoundHistoryItem item;
	item.mID = audio_source->getLogID();
	item.mAudioSource = audio_source;
	item.mPosition = audio_source->getPositionGlobal();
	item.mType = audio_source->getType();
	item.mAssetID = assetid;
	item.mOwnerID = audio_source->getOwnerID();
	item.mSourceID = audio_source->getSourceID();
	item.mPlaying = true;
	item.mTimeStarted = LLTimer::getElapsedSeconds();
	item.mTimeStopped = F64_MAX;
	item.mIsTrigger = audio_source->getIsTrigger();
	item.mIsLooped = audio_source->isLoop();

	item.mReviewed = false;
	item.mReviewedCollision = false;

	gSoundHistory[item.mID] = item;
}

//static 
void logSoundStop(LLUUID const& id, bool destructed)
{
	std::map<LLUUID, LLSoundHistoryItem>::iterator iter = gSoundHistory.find(id);
	if(iter != gSoundHistory.end() && iter->second.mAudioSource)
	{
		iter->second.mPlaying = false;
		iter->second.mTimeStopped = LLTimer::getElapsedSeconds();
		if (destructed)
			iter->second.mAudioSource = NULL;
		pruneSoundLog();
	}
}

//static
void pruneSoundLog()
{
	if(++gSoundHistoryPruneCounter >= 64)
	{
		gSoundHistoryPruneCounter = 0;
		while(gSoundHistory.size() > 256)
		{
			std::map<LLUUID, LLSoundHistoryItem>::iterator iter = gSoundHistory.begin();
			std::map<LLUUID, LLSoundHistoryItem>::iterator end = gSoundHistory.end();
			U64 lowest_time = (*iter).second.mTimeStopped;
			LLUUID lowest_id = (*iter).first;
			for( ; iter != end; ++iter)
			{
				if((*iter).second.mTimeStopped < lowest_time)
				{
					lowest_time = (*iter).second.mTimeStopped;
					lowest_id = (*iter).first;
				}
			}
			gSoundHistory.erase(lowest_id);
		}
	}
}
// </edit>
