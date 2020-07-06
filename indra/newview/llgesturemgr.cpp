/** 
 * @file llgesturemgr.cpp
 * @brief Manager for playing gestures on the viewer
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "llgesturemgr.h"

// system
#include <functional>
#include <algorithm>

// library
#include "llanimationstates.h"
#include "llaudioengine.h"
#include "lldatapacker.h"
#include "llinventory.h"
#include "llkeyframemotion.h"
#include "llmultigesture.h"
#include "llnotificationsutil.h"
#include "llstl.h"
#include "llstring.h"	// todo: remove
#include "llvfile.h"
#include "message.h"

// newview
#include "llagent.h"
#include "llchatbar.h"
#include "lldelayedgestureerror.h"
#include "llinventorymodel.h"
#include "llviewermessage.h"
#include "llvoavatarself.h"
#include "llviewerstats.h"
#include "llappearancemgr.h"

#include "chatbar_as_cmdline.h"

#if SHY_MOD //Command handler
#include "shcommandhandler.h"
#endif //shy_mod

void fake_local_chat(std::string msg);

// Longest time, in seconds, to wait for all animations to stop playing
const F32 MAX_WAIT_ANIM_SECS = 30.f;

// Lightweight constructor.
// init() does the heavy lifting.
LLGestureMgr::LLGestureMgr()
:	mValid(false),
	mPlaying(),
	mActive(),
	mLoadingCount(0)
{
	gInventory.addObserver(this);
}


// We own the data for gestures, so clean them up.
LLGestureMgr::~LLGestureMgr()
{
	item_map_t::iterator it;
	for (it = mActive.begin(); it != mActive.end(); ++it)
	{
		LLMultiGesture* gesture = (*it).second;

		delete gesture;
		gesture = nullptr;
	}
	gInventory.removeObserver(this);
}


void LLGestureMgr::init()
{
	// TODO
}

void LLGestureMgr::changed(U32 mask) 
{ 
	LLInventoryFetchItemsObserver::changed(mask);

	if (mask & LLInventoryObserver::GESTURE)
	{
		// If there was a gesture label changed, update all the names in the 
		// active gestures and then notify observers
		if (mask & LLInventoryObserver::LABEL)
		{
			for(item_map_t::iterator it = mActive.begin(); it != mActive.end(); ++it)
			{
				if(it->second)
				{
					LLViewerInventoryItem* item = gInventory.getItem(it->first);
					if(item)
					{
						it->second->mName = item->getName();
					}
				}
			}
			notifyObservers();
		}
		// If there was a gesture added or removed notify observers
		// STRUCTURE denotes that the inventory item has been moved
		// In the case of deleting gesture, it is moved to the trash
		else if(mask & LLInventoryObserver::ADD ||
				mask & LLInventoryObserver::REMOVE ||
				mask & LLInventoryObserver::STRUCTURE)
		{
			notifyObservers();
		}
	}
}


// Use this version when you have the item_id but not the asset_id,
// and you KNOW the inventory is loaded.
void LLGestureMgr::activateGesture(const LLUUID& item_id)
{
	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if (!item) return;
	if (item->getType() != LLAssetType::AT_GESTURE)
		return;

	LLUUID asset_id = item->getAssetUUID();

	mLoadingCount = 1;
	mDeactivateSimilarNames.clear();

	const bool inform_server = true;
	const bool deactivate_similar = false; 
	activateGestureWithAsset(item_id, asset_id, inform_server, deactivate_similar);
}


void LLGestureMgr::activateGestures(LLViewerInventoryItem::item_array_t& items)
{
	mDeactivateSimilarNames.clear();

	// Load up the assets
	mLoadingCount = 0;

	// Inform the database of this change
	LLMessageSystem* msg = gMessageSystem;
	bool start_message = true;

	for (const auto& item : items)
	{
		const auto& id = item->getUUID();
		if (isGestureActive(id))
		{
			continue;
		}

		// Make gesture active and persistent through login sessions.  -Aura 07-12-06
		activateGesture(id);

		++mLoadingCount;

		const auto& asset_id = item->getAssetUUID();

		// Don't inform server, we'll do that in bulk
		const bool no_inform_server = false;
		const bool deactivate_similar = true;
		activateGestureWithAsset(id, asset_id,
								 no_inform_server,
								 deactivate_similar);

		if (start_message)
		{
			msg->newMessage("ActivateGestures");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgent.getSessionID());
			msg->addU32("Flags", 0x0);
			start_message = false;
		}
		
		msg->nextBlock("Data");
		msg->addUUID("ItemID", id);
		msg->addUUID("AssetID", asset_id);
		msg->addU32("GestureFlags", 0x0);

		if (msg->getCurrentSendTotal() > MTUBYTES)
		{
			gAgent.sendReliableMessage();
			start_message = true;
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}
}


struct LLLoadInfo
{
	LLUUID mItemID;
	bool mInformServer;
	bool mDeactivateSimilar;
};

// If inform_server is true, will send a message upstream to update
// the user_gesture_active table.
/**
 * It will load a gesture from remote storage
 */
void LLGestureMgr::activateGestureWithAsset(const LLUUID& item_id,
												const LLUUID& asset_id,
												bool inform_server,
												bool deactivate_similar)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);

	if( !gAssetStorage )
	{
		LL_WARNS() << "LLGestureMgr::activateGestureWithAsset without valid gAssetStorage" << LL_ENDL;
		return;
	}
	// If gesture is already active, nothing to do.
	if (isGestureActive(base_item_id))
	{
		LL_WARNS() << "Tried to loadGesture twice " << base_item_id << LL_ENDL;
		return;
	}

//	if (asset_id.isNull())
//	{
//		LL_WARNS() << "loadGesture() - gesture has no asset" << LL_ENDL;
//		return;
//	}

	// For now, put nullptr into the item map.  We'll build a gesture
	// class object when the asset data arrives.
	mActive[base_item_id] = nullptr;

	// Copy the UUID
	if (asset_id.notNull())
	{
		LLLoadInfo* info = new LLLoadInfo;
		info->mItemID = base_item_id;
		info->mInformServer = inform_server;
		info->mDeactivateSimilar = deactivate_similar;

		const bool high_priority = true;
		gAssetStorage->getAssetData(asset_id,
									LLAssetType::AT_GESTURE,
									onLoadComplete,
									(void*)info,
									high_priority);
	}
	else
	{
		notifyObservers();
	}
}


void LLGestureMgr::deactivateGesture(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	const auto& it = mActive.find(base_item_id);
	if (it == mActive.end())
	{
		LL_WARNS() << "deactivateGesture for inactive gesture " << base_item_id << LL_ENDL;
		return;
	}

	// mActive owns this gesture pointer, so clean up memory.
	LLMultiGesture* gesture = (*it).second;

	// Can be nullptr gestures in the map
	if (gesture)
	{
		stopGesture(gesture);

		delete gesture;
		gesture = nullptr;
	}

	mActive.erase(it);
	gInventory.addChangedMask(LLInventoryObserver::LABEL, base_item_id);

	// Inform the database of this change
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("DeactivateGestures");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->addU32("Flags", 0x0);
	
	msg->nextBlock("Data");
	msg->addUUID("ItemID", base_item_id);
	msg->addU32("GestureFlags", 0x0);

	gAgent.sendReliableMessage();

	LLAppearanceMgr::instance().removeCOFItemLinks(base_item_id);

	notifyObservers();
}


void LLGestureMgr::deactivateSimilarGestures(const LLMultiGesture* in, const LLUUID& in_item_id)
{
	const LLUUID& base_in_item_id = gInventory.getLinkedItemID(in_item_id);

	// Inform database of the change
	LLMessageSystem* msg = gMessageSystem;
	bool start_message = true;

	// Deactivate all gestures that match
	for (auto it = mActive.begin(), end = mActive.end(); it != end; )
	{
		const LLUUID& item_id = (*it).first;
		LLMultiGesture* gest = (*it).second;

		// Don't deactivate the gesture we are looking for duplicates of
		// (for replaceGesture)
		if (!gest || item_id == base_in_item_id) 
		{
			// legal, can have null pointers in list
			++it;
		}
		else if ((!gest->mTrigger.empty() && gest->mTrigger == in->mTrigger)
				 || (gest->mKey != KEY_NONE && gest->mKey == in->mKey && gest->mMask == in->mMask))
		{
			stopGesture(gest);

			delete gest;
			gest = nullptr;

			it = mActive.erase(it);
			end = mActive.end();
			gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

			if (start_message)
			{
				msg->newMessage("DeactivateGestures");
				msg->nextBlock("AgentData");
				msg->addUUID("AgentID", gAgentID);
				msg->addUUID("SessionID", gAgent.getSessionID());
				msg->addU32("Flags", 0x0);
				start_message = false;
			}
	
			msg->nextBlock("Data");
			msg->addUUID("ItemID", item_id);
			msg->addU32("GestureFlags", 0x0);

			if (msg->getCurrentSendTotal() > MTUBYTES)
			{
				gAgent.sendReliableMessage();
				start_message = true;
			}

			// Add to the list of names for the user.
			if (const auto& item = gInventory.getItem(item_id))
				mDeactivateSimilarNames += item->getName() + '\n';
		}
		else
		{
			++it;
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}

	// *TODO: We call notify observers in stopGesture above, should we pass a flag to only do that here?
	notifyObservers();
}


bool LLGestureMgr::isGestureActive(const LLUUID& item_id) const
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	const auto& it = mActive.find(base_item_id);
	return (it != mActive.end());
}


bool LLGestureMgr::isGesturePlaying(const LLUUID& item_id) const
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	const auto& it = mActive.find(base_item_id);
	if (it == mActive.end()) return false;

	const LLMultiGesture* gesture = (*it).second;
	if (!gesture) return false;

	return gesture->mPlaying;
}

bool LLGestureMgr::isGesturePlaying(const LLMultiGesture* gesture) const
{
	return gesture && gesture->mPlaying;
}

void LLGestureMgr::replaceGesture(const LLUUID& item_id, LLMultiGesture* new_gesture, const LLUUID& asset_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	const auto& it = mActive.find(base_item_id);
	if (it == mActive.end())
	{
		LL_WARNS() << "replaceGesture for inactive gesture " << base_item_id << LL_ENDL;
		return;
	}

	LLMultiGesture* old_gesture = (*it).second;
	stopGesture(old_gesture);

	mActive.erase(base_item_id);

	mActive[base_item_id] = new_gesture;

	delete old_gesture;
	old_gesture = nullptr;

	if (asset_id.notNull())
	{
		mLoadingCount = 1;
		mDeactivateSimilarNames.clear();

		LLLoadInfo* info = new LLLoadInfo;
		info->mItemID = base_item_id;
		info->mInformServer = true;
		info->mDeactivateSimilar = false;

		const bool high_priority = true;
		gAssetStorage->getAssetData(asset_id,
									LLAssetType::AT_GESTURE,
									onLoadComplete,
									(void*)info,
									high_priority);
	}

	notifyObservers();
}

void LLGestureMgr::replaceGesture(const LLUUID& item_id, const LLUUID& new_asset_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);

	auto it = LLGestureMgr::instance().mActive.find(base_item_id);
	if (it == mActive.end())
	{
		LL_WARNS() << "replaceGesture for inactive gesture " << base_item_id << LL_ENDL;
		return;
	}

	// mActive owns this gesture pointer, so clean up memory.
	LLMultiGesture* gesture = (*it).second;
	LLGestureMgr::instance().replaceGesture(base_item_id, gesture, new_asset_id);
}

void LLGestureMgr::playGesture(LLMultiGesture* gesture, bool local)
{
	if (!gesture) return;

	// Reset gesture to first step
	gesture->mCurrentStep = 0;

	// Add to list of playing
	gesture->mPlaying = true;
	gesture->mLocal = local;
	mPlaying.push_back(gesture);

	// Load all needed assets to minimize the delays
	// when gesture is playing.
	for (std::vector<LLGestureStep*>::iterator steps_it = gesture->mSteps.begin();
		 steps_it != gesture->mSteps.end();
		 ++steps_it)
	{
		LLGestureStep* step = *steps_it;
		switch(step->getType())
		{
		case STEP_ANIMATION:
			{
				LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
				const LLUUID& anim_id = anim_step->mAnimAssetID;

				// Don't request the animation if this step stops it or if it is already in Static VFS
				if (!(anim_id.isNull()
					  || anim_step->mFlags & ANIM_FLAG_STOP
					  || gAssetStorage->hasLocalAsset(anim_id, LLAssetType::AT_ANIMATION)))
				{
					//Singu note: Don't attempt to fetch expressions/emotes.
					const char* emote_name = gAnimLibrary.animStateToString(anim_id);
					if(emote_name && strstr(emote_name,"express_")==emote_name)
					{
						break;
					}

					mLoadingAssets.insert(anim_id);

					LLUUID* id = new LLUUID(gAgentID);
					gAssetStorage->getAssetData(anim_id,
									LLAssetType::AT_ANIMATION,
									onAssetLoadComplete,
									(void *)id,
									true);
				}
				break;
			}
		case STEP_SOUND:
			{
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				const LLUUID& sound_id = sound_step->mSoundAssetID;
				if (!(sound_id.isNull()
					  || gAssetStorage->hasLocalAsset(sound_id, LLAssetType::AT_SOUND)))
				{
					mLoadingAssets.insert(sound_id);

					gAssetStorage->getAssetData(sound_id,
									LLAssetType::AT_SOUND,
									onAssetLoadComplete,
									nullptr,
									true);
				}
				break;
			}
		case STEP_CHAT:
		case STEP_WAIT:
		case STEP_EOF:
			{
				break;
			}
		default:
			{
				LL_WARNS() << "Unknown gesture step type: " << step->getType() << LL_ENDL;
			}
		}
	}

	// And get it going
	stepGesture(gesture);

	notifyObservers();
}


// Convenience function that looks up the item_id for you.
void LLGestureMgr::playGesture(const LLUUID& item_id, bool local)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);

	item_map_t::iterator it = mActive.find(base_item_id);
	if (it == mActive.end()) return;

	LLMultiGesture* gesture = (*it).second;
	if (!gesture) return;

	playGesture(gesture);
}


// Iterates through space delimited tokens in string, triggering any gestures found.
// Generates a revised string that has the found tokens replaced by their replacement strings
// and (as a minor side effect) has multiple spaces in a row replaced by single spaces.
bool LLGestureMgr::triggerAndReviseString(const std::string &utf8str, std::string* revised_string)
{
	std::string tokenized = utf8str;

	bool found_gestures = false;
	bool first_token = true;

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");
	tokenizer tokens(tokenized, sep);
	tokenizer::iterator token_iter;

	for( token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
	{
		const char* cur_token = token_iter->c_str();
		LLMultiGesture* gesture = nullptr;

		// Only pay attention to the first gesture in the string.
		if( !found_gestures )
		{
			// collect gestures that match
			std::vector <LLMultiGesture *> matching;
			for (const auto& pair : mActive)
			{
				gesture = pair.second;

				// Gesture asset data might not have arrived yet
				if (!gesture) continue;
				
				if (LLStringUtil::compareInsensitive(gesture->mTrigger, cur_token) == 0)
				{
					matching.push_back(gesture);
				}
				
				gesture = nullptr;
			}

			
			if (matching.size() > 0)
			{
				// choose one at random
				{
					S32 random = ll_rand(matching.size());

					gesture = matching[random];
					
					playGesture(gesture);

					if (revised_string && !gesture->mReplaceText.empty())
					{
						if (!first_token) revised_string->push_back(' ');

						// Don't muck with the user's capitalization if we don't have to.
						revised_string->append(LLStringUtil::compareInsensitive(cur_token, gesture->mReplaceText) == 0 ?
							cur_token : gesture->mReplaceText);
					}
					found_gestures = true;
				}
			}
		}
		
		if (!gesture && revised_string)
		{
			// This token doesn't match a gesture.  Pass it through to the output.
			if (!first_token) revised_string->push_back(' ');
			revised_string->append(cur_token);
		}

		first_token = false;
		gesture = nullptr;
	}
	return found_gestures;
}


bool LLGestureMgr::triggerGesture(KEY key, MASK mask)
{
	if (mActive.empty()) return false;

	std::vector<LLMultiGesture*> matching;

	// collect matching gestures
	for (const auto& pair : mActive)
	{
		LLMultiGesture* gesture = pair.second;

		// asset data might not have arrived yet
		if (!gesture) continue;

		if (gesture->mKey == key
			&& gesture->mMask == mask)
		{
			matching.push_back(gesture);
		}
	}

	// choose one and play it
	const auto& count = matching.size();
	if (count > 0)
	{
		playGesture(matching[ll_rand(count)]);
		return true;
	}
	return false;
}


S32 LLGestureMgr::getPlayingCount() const
{
	return mPlaying.size();
}


void LLGestureMgr::update()
{
	bool notify = false;
	for (auto it = mPlaying.begin(), end = mPlaying.end(); it != end;)
	{
		auto& gesture = *it;
		stepGesture(gesture);
		if (!gesture->mPlaying)
		{
			// Delete the completed gestures that want deletion
			if (gesture->mDoneCallback)
			{
				gesture->mDoneCallback(gesture);

				// callback might have deleted gesture, can't
				// rely on this pointer any more
				gesture = nullptr;
			}

			// And take done gesture out of the playing list
			it = mPlaying.erase(it);
			end = mPlaying.end();
			notify = true;
		}
		else ++it;
	}

	// Something finished playing
	if (notify) notifyObservers();
}


// Run all steps until you're either done or hit a wait.
void LLGestureMgr::stepGesture(LLMultiGesture* gesture)
{
	if (!gesture)
	{
		return;
	}
	if (!isAgentAvatarValid() || hasLoadingAssets(gesture)) return;

	// Of the ones that started playing, have any stopped?

	const auto& signaled(gAgentAvatarp->mSignaledAnimations);
	const auto& signaled_end(signaled.end());
	for (auto gest_it = gesture->mPlayingAnimIDs.begin(), end = gesture->mPlayingAnimIDs.end(); gest_it != end;)
	{
		if (gesture->mLocal)
		{
			// Local, erase if no longer playing (or gone)
			LLMotion* motion = gAgentAvatarp->findMotion(*gest_it);
			if (!motion || motion->isStopped())
				gest_it = gesture->mPlayingAnimIDs.erase(gest_it);
			else ++gest_it;
		}
		// look in signaled animations (simulator's view of what is
		// currently playing.
		else if (signaled.find(*gest_it) != signaled_end)
		{
			++gest_it;
		}
		else
		{
			// not found, so not currently playing or scheduled to play
			// delete from the triggered set
			gest_it = gesture->mPlayingAnimIDs.erase(gest_it);
		}
	}

	// Of all the animations that we asked the sim to start for us,
	// pick up the ones that have actually started.
	for (auto gest_it = gesture->mRequestedAnimIDs.begin(), end = gesture->mRequestedAnimIDs.end(); gest_it != end;)
	{
		if (signaled.find(*gest_it) != signaled_end)
		{
			// Hooray, this animation has started playing!
			// Copy into playing.
			gesture->mPlayingAnimIDs.insert(*gest_it);
			gest_it = gesture->mRequestedAnimIDs.erase(gest_it);
		}
		else
		{
			// nope, not playing yet
			++gest_it;
		}
	}

	// Run the current steps
	bool waiting = false;
	while (!waiting && gesture->mPlaying)
	{
		// Get the current step, if there is one.
		// Otherwise enter the waiting at end state.
		LLGestureStep* step = nullptr;
		if (gesture->mCurrentStep < (S32)gesture->mSteps.size())
		{
			step = gesture->mSteps[gesture->mCurrentStep];
			llassert(step != nullptr);
		}
		else
		{
			// step stays null, we're off the end
			gesture->mWaitingAtEnd = true;
		}


		// If we're waiting at the end, wait for all gestures to stop
		// playing.
		// TODO: Wait for all sounds to complete as well.
		if (gesture->mWaitingAtEnd)
		{
			// Neither do we have any pending requests, nor are they
			// still playing.
			if ((gesture->mRequestedAnimIDs.empty()
				&& gesture->mPlayingAnimIDs.empty()))
			{
				// all animations are done playing
				gesture->mWaitingAtEnd = false;
				gesture->mPlaying = false;
			}
			else
			{
				waiting = true;
			}
			continue;
		}

		// If we're waiting on our animations to stop, poll for
		// completion.
		if (gesture->mWaitingAnimations)
		{
			// Neither do we have any pending requests, nor are they
			// still playing.
			if ((gesture->mRequestedAnimIDs.empty()
				&& gesture->mPlayingAnimIDs.empty()))
			{
				// all animations are done playing
				gesture->mWaitingAnimations = false;
				++gesture->mCurrentStep;
			}
			else if (gesture->mWaitTimer.getElapsedTimeF32() > MAX_WAIT_ANIM_SECS)
			{
				// we've waited too long for an animation
				LL_INFOS() << "Waited too long for animations to stop, continuing gesture."
					<< LL_ENDL;
				gesture->mWaitingAnimations = false;
				++gesture->mCurrentStep;
			}
			else
			{
				waiting = true;
			}
			continue;
		}

		// If we're waiting a fixed amount of time, check for timer
		// expiration.
		if (gesture->mWaitingTimer)
		{
			// We're waiting for a certain amount of time to pass
			LLGestureStepWait* wait_step = (LLGestureStepWait*)step;

			F32 elapsed = gesture->mWaitTimer.getElapsedTimeF32();
			if (elapsed > wait_step->mWaitSeconds)
			{
				// wait is done, continue execution
				gesture->mWaitingTimer = false;
				++gesture->mCurrentStep;
			}
			else
			{
				// we're waiting, so execution is done for now
				waiting = true;
			}
			continue;
		}

		// Not waiting, do normal execution
		runStep(gesture, step);
	}
}


void LLGestureMgr::runStep(LLMultiGesture* gesture, LLGestureStep* step)
{
	switch(step->getType())
	{
	case STEP_ANIMATION:
		{
			LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
			if (anim_step->mAnimAssetID.isNull())
			{
				++gesture->mCurrentStep;
			}

			if (anim_step->mFlags & ANIM_FLAG_STOP)
			{
				if (gesture->mLocal)
				{
					gAgentAvatarp->stopMotion(anim_step->mAnimAssetID);
				}
				else
				{
					gAgent.sendAnimationRequest(anim_step->mAnimAssetID, ANIM_REQUEST_STOP);
					// remove it from our request set in case we just requested it
					auto set_it = gesture->mRequestedAnimIDs.find(anim_step->mAnimAssetID);
					if (set_it != gesture->mRequestedAnimIDs.end())
					{
						gesture->mRequestedAnimIDs.erase(set_it);
					}
				}
			}
			else
			{
				if (gesture->mLocal)
				{
					gAgentAvatarp->startMotion(anim_step->mAnimAssetID);
					// Indicate we're playing this animation now.
					gesture->mPlayingAnimIDs.insert(anim_step->mAnimAssetID);
				}
				else
				{
					gAgent.sendAnimationRequest(anim_step->mAnimAssetID, ANIM_REQUEST_START);
					// Indicate that we've requested this animation to play as
					// part of this gesture (but it won't start playing for at
					// least one round-trip to simulator).
					gesture->mRequestedAnimIDs.insert(anim_step->mAnimAssetID);
				}
			}
			++gesture->mCurrentStep;
			break;
		}
	case STEP_SOUND:
		{
			LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
			const LLUUID& sound_id = sound_step->mSoundAssetID;
			constexpr F32 volume = 1.f;
			if (gesture->mLocal)
				gAudiop->triggerSound(sound_id, gAgentID, volume, LLAudioEngine::AUDIO_TYPE_UI, gAgent.getPositionGlobal());
			else
				send_sound_trigger(sound_id, volume);
			++gesture->mCurrentStep;
			break;
		}
	case STEP_CHAT:
		{
			LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
			const std::string& chat_text = chat_step->mChatText;
			// Don't animate the nodding, as this might not blend with
			// other playing animations.

			constexpr bool animate = false;
			if (cmd_line_chat(chat_text, CHAT_TYPE_NORMAL))
			{
#if SHY_MOD //Command handler
				if(!SHCommandHandler::handleCommand(true, chat_text, gAgentID, gAgentAvatarp))//returns true if handled
#endif //shy_mod
				gesture->mLocal ? fake_local_chat(chat_text) : gChatBar->sendChatFromViewer(chat_text, CHAT_TYPE_NORMAL, animate);
			}
			++gesture->mCurrentStep;
			break;
		}
	case STEP_WAIT:
		{
			LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
			if (wait_step->mFlags & WAIT_FLAG_TIME)
			{
				gesture->mWaitingTimer = true;
				gesture->mWaitTimer.reset();
			}
			else if (wait_step->mFlags & WAIT_FLAG_ALL_ANIM)
			{
				gesture->mWaitingAnimations = true;
				// Use the wait timer as a deadlock breaker for animation
				// waits.
				gesture->mWaitTimer.reset();
			}
			else
			{
				++gesture->mCurrentStep;
			}
			// Don't increment instruction pointer until wait is complete.
			break;
		}
	default:
		{
			break;
		}
	}
}


// static
void LLGestureMgr::onLoadComplete(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLLoadInfo* info = (LLLoadInfo*)user_data;

	const LLUUID item_id = info->mItemID;
	const bool inform_server = info->mInformServer;
	const bool deactivate_similar = info->mDeactivateSimilar;

	delete info;
	info = nullptr;
	LLGestureMgr& self = LLGestureMgr::instance();
	--self.mLoadingCount;

	if (0 == status)
	{
		LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
		S32 size = file.getSize();

		// ensure there's a trailing nullptr so strlen will work.
		std::vector<char> buffer(size+1, '\0');
		file.read((U8*)&buffer[0], size);

		LLMultiGesture* gesture = new LLMultiGesture();

		LLDataPackerAsciiBuffer dp(&buffer[0], size+1);

		if (gesture->deserialize(dp))
		{
			if (deactivate_similar)
			{
				self.deactivateSimilarGestures(gesture, item_id);

				// Display deactivation message if this was the last of the bunch.
				if (self.mLoadingCount == 0
					&& self.mDeactivateSimilarNames.length() > 0)
				{
					// we're done with this set of deactivations
					LLSD args;
					args["NAMES"] = self.mDeactivateSimilarNames;
					LLNotificationsUtil::add("DeactivatedGesturesTrigger", args);
				}
			}

			LLViewerInventoryItem* item = gInventory.getItem(item_id);
			if(item)
			{
				gesture->mName = item->getName();
			}
			else
			{
				// Watch this item and set gesture name when item exists in inventory
				self.setFetchID(item_id);
				self.startFetch();
			}
			self.mActive[item_id] = gesture;

			// Everything has been successful.  Add to the active list.
			gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

			if (inform_server)
			{
				// Inform the database of this change
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessage("ActivateGestures");
				msg->nextBlock("AgentData");
				msg->addUUID("AgentID", gAgent.getID());
				msg->addUUID("SessionID", gAgent.getSessionID());
				msg->addU32("Flags", 0x0);
				
				msg->nextBlock("Data");
				msg->addUUID("ItemID", item_id);
				msg->addUUID("AssetID", asset_uuid);
				msg->addU32("GestureFlags", 0x0);

				gAgent.sendReliableMessage();
			}

			auto i_cb = self.mCallbackMap.find(item_id);
			if(i_cb != self.mCallbackMap.end())
			{
				i_cb->second(gesture);
				self.mCallbackMap.erase(i_cb);
			}

			self.notifyObservers();
		}
		else
		{
			LL_WARNS() << "Unable to load gesture" << LL_ENDL;

			self.mActive.erase(item_id);
			
			delete gesture;
			gesture = nullptr;
		}
	}
	else
	{
		LLViewerStats::getInstance()->incStat( LLViewerStats::ST_DOWNLOAD_FAILED );

		if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
			LL_ERR_FILE_EMPTY == status)
		{
			LLDelayedGestureError::gestureMissing( item_id );
		}
		else
		{
			LLDelayedGestureError::gestureFailedToLoad( item_id );
		}

		LL_WARNS() << "Problem loading gesture: " << status << LL_ENDL;
		
		LLGestureMgr::instance().mActive.erase(item_id);			
	}
}

// static
void LLGestureMgr::onAssetLoadComplete(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLGestureMgr& self = LLGestureMgr::instance();

	// Complete the asset loading process depending on the type and
	// remove the asset id from pending downloads list.
	switch(type)
	{
	case LLAssetType::AT_ANIMATION:
		{
			LLKeyframeMotion::onLoadComplete(vfs, asset_uuid, type, user_data, status, ext_status);

			self.mLoadingAssets.erase(asset_uuid);

			break;
		}
	case LLAssetType::AT_SOUND:
		{
			LLAudioEngine::assetCallback(vfs, asset_uuid, type, user_data, status, ext_status);

			self.mLoadingAssets.erase(asset_uuid);

			break;
		}
	default:
		{
			LL_WARNS() << "Unexpected asset type: " << type << LL_ENDL;

			// We don't want to return from this callback without
			// an animation or sound callback being fired
			// and *user_data handled to avoid memory leaks.
			llassert(type == LLAssetType::AT_ANIMATION || type == LLAssetType::AT_SOUND);
		}
	}
}

// static
bool LLGestureMgr::hasLoadingAssets(LLMultiGesture* gesture)
{
	LLGestureMgr& self = LLGestureMgr::instance();

	for (auto& step : gesture->mSteps)
	{
		switch(step->getType())
		{
		case STEP_ANIMATION:
			{
				LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
				const LLUUID& anim_id = anim_step->mAnimAssetID;

				if (!(anim_id.isNull()
					  || anim_step->mFlags & ANIM_FLAG_STOP
					  || self.mLoadingAssets.find(anim_id) == self.mLoadingAssets.end()))
				{
					return true;
				}
				break;
			}
		case STEP_SOUND:
			{
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				const LLUUID& sound_id = sound_step->mSoundAssetID;

				if (!(sound_id.isNull()
					  || self.mLoadingAssets.find(sound_id) == self.mLoadingAssets.end()))
				{
					return true;
				}
				break;
			}
		case STEP_CHAT:
		case STEP_WAIT:
		case STEP_EOF:
			{
				break;
			}
		default:
			{
				LL_WARNS() << "Unknown gesture step type: " << step->getType() << LL_ENDL;
			}
		}
	}

	return false;
}

void LLGestureMgr::stopGesture(LLMultiGesture* gesture)
{
	if (!gesture) return;

	// Stop any animations that this gesture is currently playing
	for (const auto& anim_id : gesture->mRequestedAnimIDs)
	{
		gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_STOP);
	}
	gesture->mRequestedAnimIDs.clear();

	for (const auto& anim_id : gesture->mPlayingAnimIDs)
	{
		if (gesture->mLocal)
			gAgentAvatarp->stopMotion(anim_id, true);
		else
			gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_STOP);
	}
	gesture->mPlayingAnimIDs.clear();

	mPlaying.erase(std::remove(mPlaying.begin(), mPlaying.end(), gesture), mPlaying.end());

	gesture->reset();

	if (gesture->mDoneCallback)
	{
		gesture->mDoneCallback(gesture);

		// callback might have deleted gesture, can't
		// rely on this pointer any more
		gesture = nullptr;
	}

	notifyObservers();
}


void LLGestureMgr::stopGesture(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	auto it = mActive.find(base_item_id);
	if (it == mActive.end()) return;

	LLMultiGesture* gesture = (*it).second;
	if (!gesture) return;

	stopGesture(gesture);
}


void LLGestureMgr::addObserver(LLGestureManagerObserver* observer)
{
	mObservers.push_back(observer);
}

void LLGestureMgr::removeObserver(LLGestureManagerObserver* observer)
{
	const auto& end = mObservers.end();
	auto it = std::find(mObservers.begin(), end, observer);
	if (it != end)
	{
		mObservers.erase(it);
	}
}

// Call this method when it's time to update everyone on a new state.
// Copy the list because an observer could respond by removing itself
// from the list.
void LLGestureMgr::notifyObservers()
{
	LL_DEBUGS() << "LLGestureMgr::notifyObservers" << LL_ENDL;

	for(auto& observer : mObservers)
	{
		observer->changed();
	}
}

bool LLGestureMgr::matchPrefix(const std::string& in_str, std::string* out_str) const
{
	S32 in_len = in_str.length();

#ifdef MATCH_COMMON_CHARS
	std::string rest_of_match;
	std::string buf;
#endif
	for (const auto& pair : mActive)
	{
		const LLMultiGesture* gesture = pair.second;
		if (gesture)
		{
			const std::string& trigger = gesture->getTrigger();
#ifdef MATCH_COMMON_CHARS
			//return whole trigger, if received text equals to it
			if (!LLStringUtil::compareInsensitive(in_str, trigger))
			{
				*out_str = trigger;
				return true;
			}
#else
			if (in_len > (S32)trigger.length()) continue; 	// too short, bail out
#endif

			//return common chars, if more than one trigger matches the prefix
			std::string trigger_trunc = trigger;
			LLStringUtil::truncate(trigger_trunc, in_len);
			if (!LLStringUtil::compareInsensitive(in_str, trigger_trunc))
			{
#ifndef MATCH_COMMON_CHARS
				*out_str = trigger;
				return true;
#else
				if (rest_of_match.empty())
				{
					rest_of_match = trigger.substr(in_str.size());
				}
				std::string cur_rest_of_match = trigger.substr(in_str.size());
				buf.clear();

				for (U32 i = 0; i < rest_of_match.length() && i < cur_rest_of_match.length(); ++i)
				{
					const auto& rest(rest_of_match[i])
					if (rest==cur_rest_of_match[i])
					{
						buf.push_back(rest);
					}
					else
					{
						if (i==0)
						{
							rest_of_match.clear();
						}
						break;
					}
				}
				if (rest_of_match.empty())
				{
					return false;
				}
				if (!buf.empty())
				{
					rest_of_match = buf;
				}
#endif
			}
		}
	}

#ifdef MATCH_COMMON_CHARS
	if (!rest_of_match.empty())
	{
		*out_str = in_str+rest_of_match;
		return true;
	}
#endif

	return false;
}


void LLGestureMgr::getItemIDs(uuid_vec_t* ids) const
{
	for (const auto& pair : mActive)
	{
		ids->push_back(pair.first);
	}
}

void LLGestureMgr::done()
{
	bool notify = false;
	for(const auto& pair : mActive)
	{
		if (pair.second && pair.second->mName.empty())
		{
			LLViewerInventoryItem* item = gInventory.getItem(pair.first);
			if(item)
			{
				pair.second->mName = item->getName();
				notify = true;
			}
		}
	}
	if(notify)
	{
		notifyObservers();
	}
}


