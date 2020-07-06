/** 
 * @file llmotion.cpp
 * @brief Implementation of LLMotion class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "linden_common.h"

#include "llmotion.h"
#include "llcriticaldamp.h"
#include "llmotioncontroller.h"

//<singu>
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// AISyncClientMotion class
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

AISyncKey* AISyncClientMotion::createSyncKey(AISyncKey const* from_key) const
{
  // The const cast is needed because getDuration() is non-const while it should have been.
  AISyncClientMotion* self = const_cast<AISyncClientMotion*>(this);
  // Only synchronize motions with the same duration and loop value.
  return new AISyncKeyMotion(from_key, self->getDuration(), self->getLoop());
}

void AISyncClientMotion::aisync_loading(void)
{
  // Register the motion for (possible) synchronization: this marks the time at which is should have started.
  unregister_client();	// In case it is already registered. Getting here means we are being (re)started now, we need to synchronize with other motions that start now.
  register_client();
}

void AISyncClientMotion::aisync_loaded(void)
{
  AISyncServer* server = this->server();
  if (!server)
  {
	// Already expired without being synchronized (no other motion was started at the same time).
	return;
  }
  AISyncKey const& key = server->key();		// The allocation of this is owned by server.
  // There is no need to resync if there was not another motion started at the same time and the key already expired.
  bool need_resync = !(server->never_synced() && key.expired());
  AISyncKey* new_key = NULL;
  if (need_resync)
  {
	// Create a new key using the old start time.
	new_key = createSyncKey(&key);
  }
  server->remove(this);						// This resets mServer and might even delete server.
  if (need_resync)
  {
	// Add the client to another server (based on the new key). This takes ownership of the key allocation.
	AISyncServerMap::instance().register_client(this, new_key);
  }
}

F32 LLMotion::getRuntime(void) const
{
  llassert(mActive);
  return mController->getAnimTime() - mActivationTimestamp;
}

F32 LLMotion::getAnimTime(void) const
{
  return mController->getAnimTime();
}

F32 LLMotion::syncActivationTime(F32 time)
{
  AISyncServer* server = this->server();
  if (!server)
  {
	register_client();
	server = this->server();
  }
  AISyncServer::client_list_t const& clients = server->getClients();
  if (clients.size() > 1)
  {
	// Look for the client with the smallest runtime.
	AISyncClientMotion* motion_with_smallest_runtime = NULL;
	F32 runtime = 1e10;
	// Run over all motions in this to be synchronized group.
	for (AISyncServer::client_list_t::const_iterator client = clients.begin(); client != clients.end(); ++client)
	{
	  if ((client->mReadyEvents & 2))		// Is this motion active? Motions that aren't loaded yet are not active.
	  {
		// Currently, if event 2 is set then this is an LLMotion.
		llassert(dynamic_cast<AISyncClientMotion*>(client->mClientPtr));
		AISyncClientMotion* motion = static_cast<AISyncClientMotion*>(client->mClientPtr);
		// Deactivated motions should have been deregistered, certainly not have event 2 set.
		llassert(static_cast<LLMotion*>(motion)->isActive());
		if (motion->getRuntime() < runtime)
		{
		  // This is a bit fuzzy since theoretically the runtime of all active motions in the list should be the same.
		  // Just use the smallest value to get rid of some randomness. We might even synchronizing with ourselves
		  // in which case 'time' would be set to a value such that mActivationTimestamp won't change.
		  // In practise however, this list will contain only two clients: this, being inactive, and our partner.
		  runtime = motion->getRuntime();
		  motion_with_smallest_runtime = motion;
		}
	  }
	}
	//-----------------------------------------------------------------------------------------
	// Here is where the actual synchronization takes place.
	// Current we only synchronize looped motions.
	if (getLoop())
	{
	  if (motion_with_smallest_runtime)
	  {
		// Pretend the motion was started in the past at the same time as the other motion(s).
		time = getAnimTime() - runtime;
	  }
	}
	//-----------------------------------------------------------------------------------------
  }

  return time;
}

void AISyncClientMotion::deregistered(void)
{
#ifdef SHOW_ASSERT
  mReadyEvents = 0;
#endif
}
//</singu>

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// LLMotion class
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// LLMotion()
// Class Constructor
//-----------------------------------------------------------------------------
LLMotion::LLMotion(LLUUID const& id, LLMotionController* controller) :
	mStopped(TRUE),
	mActive(FALSE),
	mID(id),
	mController(controller),
	mActivationTimestamp(0.f),
	mStopTimestamp(0.f),
	mSendStopTimestamp(F32_MAX),
	mResidualWeight(0.f),
	mFadeWeight(1.f),
	mDeactivateCallback(NULL),
	mDeactivateCallbackUserData(NULL)
{
	for (S32 i=0; i<3; ++i)
		memset(&mJointSignature[i][0], 0, sizeof(U8) * LL_CHARACTER_MAX_ANIMATED_JOINTS);
}

//-----------------------------------------------------------------------------
// ~LLMotion()
// Class Destructor
//-----------------------------------------------------------------------------
LLMotion::~LLMotion()
{
}

//-----------------------------------------------------------------------------
// fadeOut()
//-----------------------------------------------------------------------------
void LLMotion::fadeOut()
{
	if (mFadeWeight > 0.01f)
	{
		mFadeWeight = lerp(mFadeWeight, 0.f, LLSmoothInterpolation::getInterpolant(0.15f));
	}
	else
	{
		mFadeWeight = 0.f;
	}
}

//-----------------------------------------------------------------------------
// fadeIn()
//-----------------------------------------------------------------------------
void LLMotion::fadeIn()
{
	if (mFadeWeight < 0.99f)
	{
		mFadeWeight = lerp(mFadeWeight, 1.f, LLSmoothInterpolation::getInterpolant(0.15f));
	}
	else
	{
		mFadeWeight = 1.f;
	}
}

//-----------------------------------------------------------------------------
// addJointState()
//-----------------------------------------------------------------------------
void LLMotion::addJointState(const LLPointer<LLJointState>& jointState)
{
	mPose.addJointState(jointState);
	S32 priority = jointState->getPriority();
	if (priority == LLJoint::USE_MOTION_PRIORITY)
	{
		priority = getPriority();	
	}

	U32 usage = jointState->getUsage();

	// for now, usage is everything
    S32 joint_num = jointState->getJoint()->getJointNum();
    if ((joint_num >= (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS) || (joint_num < 0))
    {
        LL_WARNS() << "joint_num " << joint_num << " is outside of legal range [0-" << LL_CHARACTER_MAX_ANIMATED_JOINTS << ") for joint " << jointState->getJoint()->getName() << LL_ENDL;
        return;
    }
	mJointSignature[0][joint_num] = (usage & LLJointState::POS) ? (0xff >> (7 - priority)) : 0;
	mJointSignature[1][joint_num] = (usage & LLJointState::ROT) ? (0xff >> (7 - priority)) : 0;
	mJointSignature[2][joint_num] = (usage & LLJointState::SCALE) ? (0xff >> (7 - priority)) : 0;
}

void LLMotion::setDeactivateCallback( void (*cb)(void *), void* userdata )
{
	mDeactivateCallback = cb;
	mDeactivateCallbackUserData = userdata;
}

//virtual
void LLMotion::setStopTime(F32 time)
{
	mStopTimestamp = time;
	mStopped = TRUE;
}

BOOL LLMotion::isBlending()
{
	return mPose.getWeight() < 1.f;
}

//-----------------------------------------------------------------------------
// activate()
//-----------------------------------------------------------------------------
void LLMotion::activate(F32 time)
{
	mActivationTimestamp = time;
	mStopped = FALSE;
	//<singu>
	if (mController && !mController->syncing_disabled())	// Avoid being registered when syncing is disabled for this motion.
	{
		if (mActive)
		{
			// If the motion is already active then we are being restarted.
			// Unregister it first (if it is registered) so that the call to ready will cause it to be registered
			// and be synchronized with other motions that are started at this moment.
			unregister_client();
		}
		ready(6, 2 | (mController->isHidden() ? 0 : 4));	// Signal that mActivationTimestamp is set/valid (2), and that this server has a visible motion (4) (or not).
	}
	//</singu>
	mActive = TRUE;
	onActivate();
}

//-----------------------------------------------------------------------------
// deactivate()
//-----------------------------------------------------------------------------
void LLMotion::deactivate()
{
	mActive = FALSE;
	mPose.setWeight(0.f);

	//<singu>
	if (server())						// Only when this motion is already registered.
	{
		ready(6, 0);					// Signal that mActivationTimestamp is no longer valid.
		unregister_client();			// No longer running, so no longer a part of this sync group.
	}
	//</singu>

	if (mDeactivateCallback)
	{
		(*mDeactivateCallback)(mDeactivateCallbackUserData);
		mDeactivateCallback = NULL; // only call callback once
		mDeactivateCallbackUserData = NULL;
	}

	onDeactivate();
}

BOOL LLMotion::canDeprecate()
{
	return TRUE;
}

//-----------------------------------------------------------------------------
// AIMaskedMotion
//-----------------------------------------------------------------------------

BOOL AIMaskedMotion::onActivate()
{
	mController->activated(mMaskBit);
	return TRUE;
}

void AIMaskedMotion::onDeactivate()
{
	mController->deactivated(mMaskBit);
}

// End
