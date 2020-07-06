/** 
 * @file llmotioncontroller.cpp
 * @brief Implementation of LLMotionController class.
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

#include "llmotioncontroller.h"
#include "llkeyframemotion.h"
#include "llmath.h"
#include "lltimer.h"
#include "llanimationstates.h"
#include "llstl.h"

const S32 NUM_JOINT_SIGNATURE_STRIDES = LL_CHARACTER_MAX_ANIMATED_JOINTS / 4;
const U32 MAX_MOTION_INSTANCES = 32;

//-----------------------------------------------------------------------------
// Constants and statics
//-----------------------------------------------------------------------------
F32 LLMotionController::sCurrentTimeFactor = 1.f;
LLMotionRegistry LLMotionController::sRegistry;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// LLMotionRegistry class
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// LLMotionRegistry()
// Class Constructor
//-----------------------------------------------------------------------------
LLMotionRegistry::LLMotionRegistry()
{
	
}


//-----------------------------------------------------------------------------
// ~LLMotionRegistry()
// Class Destructor
//-----------------------------------------------------------------------------
LLMotionRegistry::~LLMotionRegistry()
{
	mMotionTable.clear();
}


//-----------------------------------------------------------------------------
// addMotion()
//-----------------------------------------------------------------------------
BOOL LLMotionRegistry::registerMotion( const LLUUID& id, LLMotionConstructor constructor )
{
	//	LL_INFOS() << "Registering motion: " << name << LL_ENDL;
	return mMotionTable.emplace(id, constructor).second;
}

//-----------------------------------------------------------------------------
// markBad()
//-----------------------------------------------------------------------------
void LLMotionRegistry::markBad( const LLUUID& id )
{
	mMotionTable[id] = LLMotionConstructor(NULL);
}

//-----------------------------------------------------------------------------
// createMotion()
//-----------------------------------------------------------------------------
LLMotion* LLMotionRegistry::createMotion(LLUUID const& id, LLMotionController* controller)
{
	LLMotionConstructor constructor = get_if_there(mMotionTable, id, LLMotionConstructor(NULL));
	LLMotion* motion = NULL;

	if ( constructor == NULL )
	{
		// *FIX: need to replace with a better default scheme. RN
		motion = LLKeyframeMotion::create(id, controller);
	}
	else
	{
		motion = constructor(id, controller);
	}

	return motion;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// LLMotionController class
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// LLMotionController()
// Class Constructor
//-----------------------------------------------------------------------------
LLMotionController::LLMotionController()
	: mTimeFactor(sCurrentTimeFactor),
	  mCharacter(NULL),
	  mAnimTime(0.f),
	  mActiveMask(0),
	  mDisableSyncing(0),
	  mHidden(false),
	  mHaveVisibleSyncedMotions(false),
	  mPrevTimerElapsed(0.f),
	  mLastTime(0.0f),
	  mHasRunOnce(FALSE),
	  mPaused(FALSE),
	  mPausedFrame(0),
	  mTimeStep(0.f),
	  mTimeStepCount(0),
	  mLastInterp(0.f),
	  mIsSelf(FALSE)
{
}


//-----------------------------------------------------------------------------
// ~LLMotionController()
// Class Destructor
//-----------------------------------------------------------------------------
LLMotionController::~LLMotionController()
{
	deleteAllMotions();
}

void LLMotionController::incMotionCounts(S32& num_motions, S32& num_loading_motions, S32& num_loaded_motions, S32& num_active_motions, S32& num_deprecated_motions)
{
	num_motions += mAllMotions.size();
	num_loading_motions += mLoadingMotions.size();
	num_loaded_motions += mLoadedMotions.size();
	num_active_motions += mActiveMotions.size();
	num_deprecated_motions += mDeprecatedMotions.size();
}

//-----------------------------------------------------------------------------
// deleteAllMotions()
//-----------------------------------------------------------------------------
void LLMotionController::deleteAllMotions()
{
	mLoadingMotions.clear();
	mLoadedMotions.clear();
	mActiveMotions.clear();
	//<singu>
	mActiveMask = 0;
	for_each(mDeprecatedMotions.begin(), mDeprecatedMotions.end(), DeletePointer());
	mDeprecatedMotions.clear();
	for (motion_map_t::iterator iter = mAllMotions.begin(); iter != mAllMotions.end(); ++iter)
	{
		iter->second->unregister_client();
	}
	//</singu>
	for_each(mAllMotions.begin(), mAllMotions.end(), DeletePairedPointer());
	mAllMotions.clear();
}

//-----------------------------------------------------------------------------
// purgeExcessMotion()
//-----------------------------------------------------------------------------
void LLMotionController::purgeExcessMotions()
{
	//<singu>
	// The old code attempted to remove non-active motions from mDeprecatedMotions,
	// but that is nonsense: there are no motions in mDeprecatedMotions that are not active.
	if (mLoadedMotions.size() <= MAX_MOTION_INSTANCES)
	{
		// Speed up, no need to create motions_to_kill.
		return;
	}
	//</singu>

	uuid_set_t motions_to_kill;

	if (1)	// Singu: leave indentation alone...
	{
		// too many motions active this frame, kill all blenders
		mPoseBlender.clearBlenders();
		for (motion_set_t::iterator loaded_motion_it = mLoadedMotions.begin(); 
			 loaded_motion_it != mLoadedMotions.end(); 
			 ++loaded_motion_it)
		{
			LLMotion* cur_motionp = *loaded_motion_it;
			// motion isn't playing, delete it
			if (!isMotionActive(cur_motionp))
			{
				motions_to_kill.insert(cur_motionp->getID());
			}
		}
	}
	
	// clean up all inactive, loaded motions
	for (auto motion_it = motions_to_kill.begin();
		motion_it != motions_to_kill.end();
		++motion_it)
	{
		// look up the motion again by ID to get canonical instance
		// and kill it only if that one is inactive
		LLUUID motion_id = *motion_it;
		LLMotion* motionp = findMotion(motion_id);
		if (motionp && !isMotionActive(motionp))
		{
			removeMotion(motion_id);
		}
	}

	if (mLoadedMotions.size() > 2*MAX_MOTION_INSTANCES)
	{
		LL_WARNS_ONCE("Animation") << "> " << 2*MAX_MOTION_INSTANCES << " Loaded Motions" << LL_ENDL;
	}
}

//-----------------------------------------------------------------------------
// deactivateStoppedMotions()
//-----------------------------------------------------------------------------
void LLMotionController::deactivateStoppedMotions()
{
	// Since we're hidden, deactivate any stopped motions.
	for (motion_list_t::iterator iter = mActiveMotions.begin();
		 iter != mActiveMotions.end(); )
	{
		motion_list_t::iterator curiter = iter++;
		LLMotion* motionp = *curiter;
		if (motionp->isStopped())
		{
			deactivateMotionInstance(motionp);
		}
	}
}

//-----------------------------------------------------------------------------
// setTimeStep()
//-----------------------------------------------------------------------------
void LLMotionController::setTimeStep(F32 step)
{
	mTimeStep = step;

	if (step != 0.f)
	{
		// make sure timestamps conform to new quantum
		for (motion_list_t::iterator iter = mActiveMotions.begin();
			 iter != mActiveMotions.end(); ++iter)
		{
			LLMotion* motionp = *iter;
			F32 activation_time = motionp->mActivationTimestamp;
			motionp->mActivationTimestamp = (F32)(llfloor(activation_time / step)) * step;
			BOOL stopped = motionp->isStopped();
			motionp->setStopTime((F32)(llfloor(motionp->getStopTime() / step)) * step);
			motionp->setStopped(stopped);
			motionp->mSendStopTimestamp = (F32)llfloor(motionp->mSendStopTimestamp / step) * step;
		}
	}
}

//-----------------------------------------------------------------------------
// setTimeFactor()
//-----------------------------------------------------------------------------
void LLMotionController::setTimeFactor(F32 time_factor)
{ 
	mTimeFactor = time_factor; 
}

//-----------------------------------------------------------------------------
// setCharacter()
//-----------------------------------------------------------------------------
void LLMotionController::setCharacter(LLCharacter *character)
{
	mCharacter = character;
}


//-----------------------------------------------------------------------------
// registerMotion()
//-----------------------------------------------------------------------------
BOOL LLMotionController::registerMotion( const LLUUID& id, LLMotionConstructor constructor )
{
	return sRegistry.registerMotion(id, constructor);
}

//-----------------------------------------------------------------------------
// removeMotion()
//-----------------------------------------------------------------------------
void LLMotionController::removeMotion( const LLUUID& id)
{
	LLMotion* motionp = findMotion(id);
	//<singu>
	// If a motion is erased from mAllMotions, it must be deleted.
	if (motionp)
	{
		mAllMotions.erase(id);
		removeMotionInstance(motionp);
		delete motionp;
	}
	//</singu>
}

// removes instance of a motion from all runtime structures, but does
// not erase entry by ID, as this could be a duplicate instance
// use removeMotion(id) to remove a reference to a given motion by id
// (that will not remove (active) deprecated motions).
void LLMotionController::removeMotionInstance(LLMotion* motionp)
{
	if (motionp)
	{
		llassert(findMotion(motionp->getID()) != motionp);
		mLoadingMotions.erase(motionp);
		mLoadedMotions.erase(motionp);
		mActiveMotions.remove(motionp);
		//<singu>
		// Deactivation moved here. Only delete motionp when it is being removed from mDeprecatedMotions.
		if (motionp->isActive())
		{
			motionp->deactivate();
			// If a motion is deactivated, it must be removed from mDeprecatedMotions if there.
			motion_set_t::iterator found_it = mDeprecatedMotions.find(motionp);
			if (found_it != mDeprecatedMotions.end())
			{
				mDeprecatedMotions.erase(found_it);
				// If a motion is erased from mDeprecatedMotions, it must be deleted.
				delete motionp;
			}
		}
		//</singu>
	}
}

//-----------------------------------------------------------------------------
// createMotion()
//-----------------------------------------------------------------------------
LLMotion* LLMotionController::createMotion( const LLUUID &id )
{
	// do we have an instance of this motion for this character?
	LLMotion *motion = findMotion(id);

	// if not, we need to create one
	if (!motion)
	{
		// look up constructor and create it
		motion = sRegistry.createMotion(id, this);
		if (!motion)
		{
			return NULL;
		}

		// look up name for default motions
		const char* motion_name = gAnimLibrary.animStateToString(id);
		if (motion_name)
		{
			motion->setName(motion_name);
		}

		// initialize the new instance
		LLMotion::LLMotionInitStatus stat = motion->onInitialize(mCharacter);
		switch(stat)
		{
		case LLMotion::STATUS_FAILURE:
			LL_INFOS() << "Motion " << id << " init failed." << LL_ENDL;
			sRegistry.markBad(id);
			delete motion;
			return NULL;
		case LLMotion::STATUS_HOLD:
			mLoadingMotions.insert(motion);
			break;
		case LLMotion::STATUS_SUCCESS:
		    // add motion to our list
		    mLoadedMotions.insert(motion);
			break;
		default:
			LL_ERRS() << "Invalid initialization status" << LL_ENDL;
			break;
		}

		mAllMotions[id] = motion;
	}
	return motion;
}

//-----------------------------------------------------------------------------
// startMotion()
//-----------------------------------------------------------------------------
BOOL LLMotionController::startMotion(const LLUUID &id, F32 start_offset)
{
	// do we have an instance of this motion for this character?
	LLMotion *motion = findMotion(id);

	// motion that is stopping will be allowed to stop but
	// replaced by a new instance of that motion
	if (motion
		&& !mPaused
		&& motion->canDeprecate()
		&& motion->isActive()											// singu: do not deprecate motions that are not active.
		&& motion->getFadeWeight() > 0.01f // not LOD-ed out
		&& (motion->isBlending() || motion->getStopTime() != 0.f))
	{
		deprecateMotionInstance(motion);
		// force creation of new instance
		motion = NULL;
	}

	// create new motion instance
	if (!motion)
	{
		motion = createMotion(id);
	}

	if (!motion)
	{
		return FALSE;
	}
	//if the motion is already active and allows deprecation, then let it keep playing
	else if (motion->canDeprecate() && isMotionActive(motion))
	{	
		return TRUE;
	}

//	LL_INFOS() << "Starting motion " << name << LL_ENDL;
	//<singu>
	F32 start_time = mAnimTime - start_offset;
	if (!mDisableSyncing)
	{
	  start_time = motion->syncActivationTime(start_time);
	}
	++mDisableSyncing;
	//</singu>
	BOOL res = activateMotionInstance(motion, start_time);
	//<singu>
	--mDisableSyncing;
	//</singu>
	return res;
}


//-----------------------------------------------------------------------------
// stopMotionLocally()
//-----------------------------------------------------------------------------
BOOL LLMotionController::stopMotionLocally(const LLUUID &id, BOOL stop_immediate)
{
	// if already inactive, return false
	LLMotion *motion = findMotion(id);
	return stopMotionInstance(motion, stop_immediate||mPaused);
}

BOOL LLMotionController::stopMotionInstance(LLMotion* motion, BOOL stop_immediate)
{
	if (!motion)
	{
		return FALSE;
	}

	// If on active list, stop it
	if (isMotionActive(motion) && !motion->isStopped())
	{
		motion->setStopTime(mAnimTime);
		if (stop_immediate)
		{
			deactivateMotionInstance(motion);
		}
		return TRUE;
	}
	else if (isMotionLoading(motion))
	{
		motion->setStopped(TRUE);
		return TRUE;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// updateRegularMotions()
//-----------------------------------------------------------------------------
void LLMotionController::updateRegularMotions()
{
	updateMotionsByType(LLMotion::NORMAL_BLEND);
}

//-----------------------------------------------------------------------------
// updateAdditiveMotions()
//-----------------------------------------------------------------------------
void LLMotionController::updateAdditiveMotions()
{
	updateMotionsByType(LLMotion::ADDITIVE_BLEND);
}

//-----------------------------------------------------------------------------
// resetJointSignatures()
//-----------------------------------------------------------------------------
void LLMotionController::resetJointSignatures()
{
	memset(&mJointSignature[0][0], 0, sizeof(U8) * LL_CHARACTER_MAX_ANIMATED_JOINTS);
	memset(&mJointSignature[1][0], 0, sizeof(U8) * LL_CHARACTER_MAX_ANIMATED_JOINTS);
}

//-----------------------------------------------------------------------------
// updateIdleMotion()
// minimal updates for active motions
//-----------------------------------------------------------------------------
void LLMotionController::updateIdleMotion(LLMotion* motionp)
{
	if (motionp->isStopped() && mAnimTime > motionp->getStopTime() + motionp->getEaseOutDuration())
	{
		deactivateMotionInstance(motionp);
	}
	else if (motionp->isStopped() && mAnimTime > motionp->getStopTime())
	{
		// is this the first iteration in the ease out phase?
		if (mLastTime <= motionp->getStopTime())
		{
			// store residual weight for this motion
			motionp->mResidualWeight = motionp->getPose()->getWeight();
		}
	}
	else if (mAnimTime > motionp->mSendStopTimestamp)
	{
		// notify character of timed stop event on first iteration past sendstoptimestamp
		// this will only be called when an animation stops itself (runs out of time)
		if (mLastTime <= motionp->mSendStopTimestamp)
		{
			mCharacter->requestStopMotion( motionp );
			stopMotionInstance(motionp, FALSE);
		}
	}
	else if (mAnimTime >= motionp->mActivationTimestamp)
	{
		if (mLastTime < motionp->mActivationTimestamp)
		{
			motionp->mResidualWeight = motionp->getPose()->getWeight();
		}
	}
}

//-----------------------------------------------------------------------------
// updateIdleActiveMotions()
// Call this instead of updateMotionsByType for hidden avatars
//-----------------------------------------------------------------------------
void LLMotionController::updateIdleActiveMotions()
{
	for (motion_list_t::iterator iter = mActiveMotions.begin();
		 iter != mActiveMotions.end(); )
	{
		motion_list_t::iterator curiter = iter++;
		LLMotion* motionp = *curiter;
		updateIdleMotion(motionp);
	}
}

//-----------------------------------------------------------------------------
// updateMotionsByType()
//-----------------------------------------------------------------------------
static LLTrace::BlockTimerStatHandle FTM_MOTION_ON_UPDATE("Motion onUpdate");

void LLMotionController::updateMotionsByType(LLMotion::LLMotionBlendType anim_type)
{
	BOOL update_result = TRUE;
	U8 last_joint_signature[LL_CHARACTER_MAX_ANIMATED_JOINTS] = {0};

	// iterate through active motions in chronological order
	for (motion_list_t::iterator iter = mActiveMotions.begin();
		 iter != mActiveMotions.end(); )
	{
		motion_list_t::iterator curiter = iter++;
		LLMotion* motionp = *curiter;
		if (motionp->getBlendType() != anim_type)
		{
			continue;
		}

		BOOL update_motion = FALSE;

		if (motionp->getPose()->getWeight() < 1.f)
		{
			update_motion = TRUE;
		}
		else
		{
			// NUM_JOINT_SIGNATURE_STRIDES should be multiple of 4
			for (S32 i = 0; i < NUM_JOINT_SIGNATURE_STRIDES; i++)
			{
		 		U32 *current_signature = (U32*)&(mJointSignature[0][i * 4]);
				U32 test_signature = *(U32*)&(motionp->mJointSignature[0][i * 4]);
				
				if ((*current_signature | test_signature) > (*current_signature))
				{
					*current_signature |= test_signature;
					update_motion = TRUE;
				}

				*((U32*)&last_joint_signature[i * 4]) = *(U32*)&(mJointSignature[1][i * 4]);
				current_signature = (U32*)&(mJointSignature[1][i * 4]);
				test_signature = *(U32*)&(motionp->mJointSignature[1][i * 4]);

				if ((*current_signature | test_signature) > (*current_signature))
				{
					*current_signature |= test_signature;
					update_motion = TRUE;
				}
			}
		}

		if (!update_motion)
		{
			updateIdleMotion(motionp);
			continue;
		}

		LLPose *posep = motionp->getPose();

		// only filter by LOD after running every animation at least once (to prime the avatar state)
		if (mHasRunOnce && motionp->getMinPixelArea() > mCharacter->getPixelArea())
		{
			motionp->fadeOut();

			//should we notify the simulator that this motion should be stopped (check even if skipped by LOD logic)
			if (mAnimTime > motionp->mSendStopTimestamp)
			{
				// notify character of timed stop event on first iteration past sendstoptimestamp
				// this will only be called when an animation stops itself (runs out of time)
				if (mLastTime <= motionp->mSendStopTimestamp)
				{
					mCharacter->requestStopMotion( motionp );
					stopMotionInstance(motionp, FALSE);
				}
			}

			if (motionp->getFadeWeight() < 0.01f)
			{
				if (motionp->isStopped() && mAnimTime > motionp->getStopTime() + motionp->getEaseOutDuration())
				{
					posep->setWeight(0.f);
					deactivateMotionInstance(motionp);
				}
				continue;
			}
		}
		else
		{
			motionp->fadeIn();
		}

		//**********************
		// MOTION INACTIVE
		//**********************
		if (motionp->isStopped() && mAnimTime > motionp->getStopTime() + motionp->getEaseOutDuration())
		{
			// this motion has gone on too long, deactivate it
			// did we have a chance to stop it?
			if (mLastTime <= motionp->getStopTime())
			{
				// if not, let's stop it this time through and deactivate it the next

				posep->setWeight(motionp->getFadeWeight());
				motionp->onUpdate(motionp->getStopTime() - motionp->mActivationTimestamp, last_joint_signature);
			}
			else
			{
				posep->setWeight(0.f);
				deactivateMotionInstance(motionp);
				continue;
			}
		}

		//**********************
		// MOTION EASE OUT
		//**********************
		else if (motionp->isStopped() && mAnimTime > motionp->getStopTime())
		{
			// is this the first iteration in the ease out phase?
			if (mLastTime <= motionp->getStopTime())
			{
				// store residual weight for this motion
				motionp->mResidualWeight = motionp->getPose()->getWeight();
			}

			if (motionp->getEaseOutDuration() == 0.f)
			{
				posep->setWeight(0.f);
			}
			else
			{
				posep->setWeight(motionp->getFadeWeight() * motionp->mResidualWeight * cubic_step(1.f - ((mAnimTime - motionp->getStopTime()) / motionp->getEaseOutDuration())));
			}

			// perform motion update
			update_result = motionp->onUpdate(mAnimTime - motionp->mActivationTimestamp, last_joint_signature);
		}

		//**********************
		// MOTION ACTIVE
		//**********************
		else if (mAnimTime > motionp->mActivationTimestamp + motionp->getEaseInDuration())
		{
			posep->setWeight(motionp->getFadeWeight());

			//should we notify the simulator that this motion should be stopped?
			if (mAnimTime > motionp->mSendStopTimestamp)
			{
				// notify character of timed stop event on first iteration past sendstoptimestamp
				// this will only be called when an animation stops itself (runs out of time)
				if (mLastTime <= motionp->mSendStopTimestamp)
				{
					mCharacter->requestStopMotion( motionp );
					stopMotionInstance(motionp, FALSE);
				}
			}

			// perform motion update
			{
				LL_RECORD_BLOCK_TIME(FTM_MOTION_ON_UPDATE);
				update_result = motionp->onUpdate(mAnimTime - motionp->mActivationTimestamp, last_joint_signature);
			}
		}

		//**********************
		// MOTION EASE IN
		//**********************
		else if (mAnimTime >= motionp->mActivationTimestamp)
		{
			if (mLastTime < motionp->mActivationTimestamp)
			{
				motionp->mResidualWeight = motionp->getPose()->getWeight();
			}
			if (motionp->getEaseInDuration() == 0.f)
			{
				posep->setWeight(motionp->getFadeWeight());
			}
			else
			{
				// perform motion update
				posep->setWeight(motionp->getFadeWeight() * motionp->mResidualWeight + (1.f - motionp->mResidualWeight) * cubic_step((mAnimTime - motionp->mActivationTimestamp) / motionp->getEaseInDuration()));
			}
			// perform motion update
			update_result = motionp->onUpdate(mAnimTime - motionp->mActivationTimestamp, last_joint_signature);
		}
		else
		{
			posep->setWeight(0.f);
			update_result = motionp->onUpdate(0.f, last_joint_signature);
		}
		
		// allow motions to deactivate themselves 
		if (!update_result)
		{
			if (!motionp->isStopped() || motionp->getStopTime() > mAnimTime)
			{
				// animation has stopped itself due to internal logic
				// propagate this to the network
				// as not all viewers are guaranteed to have access to the same logic
				mCharacter->requestStopMotion( motionp );
				stopMotionInstance(motionp, FALSE);
			}

		}

		// even if onupdate returns FALSE, add this motion in to the blend one last time
		mPoseBlender.addMotion(motionp);
	}
}

//-----------------------------------------------------------------------------
// updateLoadingMotions()
//-----------------------------------------------------------------------------
void LLMotionController::updateLoadingMotions()
{
	// query pending motions for completion
	for (motion_set_t::iterator iter = mLoadingMotions.begin();
		 iter != mLoadingMotions.end(); )
	{
		motion_set_t::iterator curiter = iter++;
		LLMotion* motionp = *curiter;
		if( !motionp)
		{
			continue; // maybe shouldn't happen but i've seen it -MG
		}
		LLMotion::LLMotionInitStatus status = motionp->onInitialize(mCharacter);
		if (status == LLMotion::STATUS_SUCCESS)
		{
			mLoadingMotions.erase(curiter);
			// add motion to our loaded motion list
			mLoadedMotions.insert(motionp);
			// this motion should be playing
			if (!motionp->isStopped())
			{
				//<singu>
				F32 start_time = mAnimTime;
				if (!mDisableSyncing)
				{
				  motionp->aisync_loaded();
				  start_time = motionp->syncActivationTime(start_time);
				}
				++mDisableSyncing;
				//</singu>
				activateMotionInstance(motionp, start_time);
				//<singu>
				--mDisableSyncing;
				//</singu>
			}
		}
		else if (status == LLMotion::STATUS_FAILURE)
		{
			LL_INFOS() << "Motion " << motionp->getID() << " init failed." << LL_ENDL;
			sRegistry.markBad(motionp->getID());
			mLoadingMotions.erase(curiter);
			// Singu note: a motion in mLoadingMotions will not be in mActiveMotions
			// and therefore not be in mDeprecatedMotions. So, we don't have to
			// check for it's existence there.
			llassert(mDeprecatedMotions.find(motionp) == mDeprecatedMotions.end());
			mAllMotions.erase(motionp->getID());
			//<singu>
			// Make sure we're not registered anymore.
			motionp->unregister_client();
			//</singu>
			delete motionp;
		}
	}
}

//-----------------------------------------------------------------------------
// call updateMotion() or updateMotionsMinimal() every frame
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// updateMotion()
//-----------------------------------------------------------------------------
void LLMotionController::updateMotions(bool force_update)
{
	BOOL use_quantum = (mTimeStep != 0.f);

	// Always update mPrevTimerElapsed
	F32 cur_time = mTimer.getElapsedTimeF32();
	F32 delta_time = cur_time - mPrevTimerElapsed;
	mPrevTimerElapsed = cur_time;
	mLastTime = mAnimTime;

	// Always cap the number of loaded motions
	purgeExcessMotions();
	
	// Update timing info for this time step.
	if (!mPaused)
	{
		F32 update_time = mAnimTime + delta_time * mTimeFactor;
		if (use_quantum)
		{
			F32 time_interval = fmodf(update_time, mTimeStep);

            //<singu>
            // This old code is nonsense.
            //S32 quantum_count = llmax(0, ll_round((update_time - time_interval) / mTimeStep)) + 1;
            // (update_time - time_interval) / mTimeStep is an integer! We need ll_round to get rid of floating point errors, not llfloor.
            // Moreover, just rounding off to the nearest integer with ll_round(update_time / mTimeStep) makes a lot more sense:
            // it is the best we can do to get as close to what we should draw as possible.
            // However, mAnimTime may only be incremented; therefore make sure of that with the llmax.
			S32 quantum_count = llmax(ll_pos_round(update_time / mTimeStep), llceil(mAnimTime / mTimeStep));
            //</singu>
			if (quantum_count == mTimeStepCount)
			{
				// we're still in same time quantum as before, so just interpolate and exit
				if (!mPaused)
				{
					F32 interp = time_interval / mTimeStep;
					mPoseBlender.interpolate(interp - mLastInterp);
					mLastInterp = interp;
				}

				updateLoadingMotions();

				return;
			}
			
			// is calculating a new keyframe pose, make sure the last one gets applied
			mPoseBlender.interpolate(1.f);
			clearBlenders();

			mTimeStepCount = quantum_count;
			mAnimTime = (F32)quantum_count * mTimeStep;
			mLastInterp = 0.f;
		}
		else
		{
			// Singu note: mAnimTime may never be set back in time.
			mAnimTime = llmax(mAnimTime, update_time);
		}
	}

	updateLoadingMotions();

	resetJointSignatures();

	if (mPaused && !force_update)
	{
		updateIdleActiveMotions();
	}
	else
	{
		// update additive motions
		updateAdditiveMotions();
		resetJointSignatures();

		// update all regular motions
		updateRegularMotions();

		if (use_quantum)
		{
			mPoseBlender.blendAndCache(TRUE);
		}
		else
		{
			mPoseBlender.blendAndApply();
		}
	}

	mHasRunOnce = TRUE;
//	LL_INFOS() << "Motion controller time " << motionTimer.getElapsedTimeF32() << LL_ENDL;
}

//-----------------------------------------------------------------------------
// updateMotionsMinimal()
// minimal update (e.g. while hidden)
//-----------------------------------------------------------------------------
void LLMotionController::updateMotionsMinimal()
{
	// Always update mPrevTimerElapsed
	mPrevTimerElapsed = mTimer.getElapsedTimeF32();

	purgeExcessMotions();
	updateLoadingMotions();
	resetJointSignatures();

	deactivateStoppedMotions();

	mHasRunOnce = TRUE;
}

//-----------------------------------------------------------------------------
// activateMotionInstance()
//-----------------------------------------------------------------------------
BOOL LLMotionController::activateMotionInstance(LLMotion *motion, F32 time)
{
	// It's not clear why the getWeight() line seems to be crashing this, but
	// hopefully this fixes it.
	if (motion == NULL || motion->getPose() == NULL)
	{
		return FALSE;	
	}

	if (mLoadingMotions.find(motion) != mLoadingMotions.end())
	{
		//<singu>
		if (!syncing_disabled())
		{
			motion->aisync_loading();
		}
		//</singu>
		// we want to start this motion, but we can't yet, so flag it as started
		motion->setStopped(FALSE);
		// report pending animations as activated
		return TRUE;
	}

	motion->mResidualWeight = motion->getPose()->getWeight();

	// set stop time based on given duration and ease out time
	if (motion->getDuration() != 0.f && !motion->getLoop())
	{
		F32 ease_out_time;
		F32 motion_duration;

		// should we stop at the end of motion duration, or a bit earlier 
		// to allow it to ease out while moving?
		ease_out_time = motion->getEaseOutDuration();

		// is the clock running when the motion is easing in?
		// if not (POSTURE_EASE) then we need to wait that much longer before triggering the stop
		motion_duration = llmax(motion->getDuration() - ease_out_time, 0.f);
		motion->mSendStopTimestamp = time + motion_duration;
	} 
	else
	{
		motion->mSendStopTimestamp = F32_MAX;
	}
	
	if (motion->isActive())
	{
		mActiveMotions.remove(motion);
	}
	mActiveMotions.push_front(motion);

	motion->activate(time);
	motion->onUpdate(0.f, mJointSignature[1]);

	if (mAnimTime >= motion->mSendStopTimestamp)
	{
		motion->setStopTime(motion->mSendStopTimestamp);
		if (motion->mResidualWeight == 0.0f)
		{
			// bit of a hack; if newly activating a motion while easing out, weight should = 1
			motion->mResidualWeight = 1.f;
		}
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// deactivateMotionInstance()
//-----------------------------------------------------------------------------
BOOL LLMotionController::deactivateMotionInstance(LLMotion *motion)
{
	motion_set_t::iterator found_it = mDeprecatedMotions.find(motion);
	if (found_it != mDeprecatedMotions.end())
	{
		// deprecated motions need to be completely excised
		removeMotionInstance(motion);							// singu note: this deactivates motion and removes it from mDeprecatedMotions.
	}
	else
	{
		// for motions that we are keeping, simply remove from active queue
		motion->deactivate();									// singu note: moved here from the top of the function.
		mActiveMotions.remove(motion);
	}

	return TRUE;
}

void LLMotionController::deprecateMotionInstance(LLMotion* motion)
{
	mDeprecatedMotions.insert(motion);

	//fade out deprecated motion
	stopMotionInstance(motion, FALSE);
	//no longer canonical
	mAllMotions.erase(motion->getID());
}

//-----------------------------------------------------------------------------
// isMotionActive()
//-----------------------------------------------------------------------------
bool LLMotionController::isMotionActive(LLMotion *motion)
{
	return (motion && motion->isActive());
}

//-----------------------------------------------------------------------------
// isMotionLoading()
//-----------------------------------------------------------------------------
bool LLMotionController::isMotionLoading(LLMotion* motion)
{
	return (mLoadingMotions.find(motion) != mLoadingMotions.end());
}


//-----------------------------------------------------------------------------
// findMotion()
//-----------------------------------------------------------------------------
LLMotion* LLMotionController::findMotion(const LLUUID& id) const
{
	motion_map_t::const_iterator iter = mAllMotions.find(id);
	if(iter == mAllMotions.end())
	{
		return NULL;
	}
	else
	{
		return iter->second;
	}
}

//-----------------------------------------------------------------------------
// dumpMotions()
//-----------------------------------------------------------------------------
void LLMotionController::dumpMotions()
{
	LL_INFOS() << "=====================================" << LL_ENDL;
	for (motion_map_t::iterator iter = mAllMotions.begin();
		 iter != mAllMotions.end(); iter++)
	{
		LLUUID id = iter->first;
		std::string state_string;
		LLMotion *motion = iter->second;
		if (mLoadingMotions.find(motion) != mLoadingMotions.end())
			state_string += std::string("l");
		if (mLoadedMotions.find(motion) != mLoadedMotions.end())
			state_string += std::string("L");
		if (std::find(mActiveMotions.begin(), mActiveMotions.end(), motion)!=mActiveMotions.end())
			state_string += std::string("A");
		llassert(mDeprecatedMotions.find(motion) == mDeprecatedMotions.end());	// singu: it's impossible that a motion is in mAllMotions and mDeprecatedMotions at the same time.
		LL_INFOS() << gAnimLibrary.animationName(id) << " " << state_string << LL_ENDL;
	}
	//<singu>
	// Also dump the deprecated motions.
	for (motion_set_t::iterator iter = mDeprecatedMotions.begin();
		 iter != mDeprecatedMotions.end(); ++iter)
	{
		std::string state_string;
		LLMotion* motion = *iter;
		LLUUID id = motion->getID();
		llassert(mLoadingMotions.find(motion) == mLoadingMotions.end());
		if (mLoadedMotions.find(motion) != mLoadedMotions.end())
			state_string += std::string("L");
		if (std::find(mActiveMotions.begin(), mActiveMotions.end(), motion)!=mActiveMotions.end())
			state_string += std::string("A");
		state_string += "D";
		LL_INFOS() << gAnimLibrary.animationName(id) << " " << state_string << LL_ENDL;
	}
	//</singu>
}

//-----------------------------------------------------------------------------
// deactivateAllMotions()
//-----------------------------------------------------------------------------
void LLMotionController::deactivateAllMotions()
{
	// Singu note: this must run over mActiveMotions: other motions are not active,
	// and running over mAllMotions will miss the ones in mDeprecatedMotions.
	for (motion_list_t::iterator iter = mActiveMotions.begin(); iter != mActiveMotions.end();)
	{
		deactivateMotionInstance(*iter++);	// This might invalidate iter by erasing it from mActiveMotions.
	}
}


//-----------------------------------------------------------------------------
// flushAllMotions()
//-----------------------------------------------------------------------------
void LLMotionController::flushAllMotions()
{
	std::vector<std::pair<LLUUID,F32> > active_motions;
	active_motions.reserve(mActiveMotions.size());
	for (motion_list_t::iterator iter = mActiveMotions.begin();
		 iter != mActiveMotions.end(); )
	{
		motion_list_t::iterator curiter = iter++;
		LLMotion* motionp = *curiter;
		F32 dtime = mAnimTime - motionp->mActivationTimestamp;
		active_motions.push_back(std::make_pair(motionp->getID(),dtime));
		motionp->deactivate(); // don't call deactivateMotionInstance() because we are going to reactivate it
	}

	// delete all motion instances
	deleteAllMotions();

	// kill current hand pose that was previously called out by
	// keyframe motion
	mCharacter->removeAnimationData("Hand Pose");

	// restart motions
	//<singu>
	// Because we called motionp->deactivate() above, instead of deactivateMotionInstance(),
	// prevent calling AISyncClientMotion::activateInstance in startMotion below.
	disable_syncing();
	//</singu>
	for (std::vector<std::pair<LLUUID,F32> >::iterator iter = active_motions.begin();
		 iter != active_motions.end(); ++iter)
	{
		startMotion(iter->first, iter->second);
	}
	//<singu>
	enable_syncing();
	//</singu>
}

//<singu>
//-----------------------------------------------------------------------------
// toggle_hidden()
//-----------------------------------------------------------------------------
void LLMotionController::toggle_hidden(void)
{
	mHaveVisibleSyncedMotions = mHidden;		// Default is false if we just became invisible (otherwise this value isn't used).
	mHidden = !mHidden;
	synceventset_t const visible = mHidden ? 0 : 4;

	// Run over all motions.
	for (motion_list_t::iterator iter = mActiveMotions.begin(); iter != mActiveMotions.end(); ++iter)
	{
		LLMotion* motionp = *iter;
		AISyncServer* server = motionp->server();
		if (server && !server->never_synced() && motionp->isActive())			// Skip motions that aren't synchronized at all or that are not active.
		{
			bool visible_before = server->events_with_at_least_one_client_ready() & 4;
			server->ready(4, visible, motionp);									// Mark that now we are visible or no longer visible.
			bool visible_after = server->events_with_at_least_one_client_ready() & 4;
			if (visible_after)													// Are there any synchronized motions (left) that ARE visible?
			{
				mHaveVisibleSyncedMotions = true;
			}
			if (visible_before != visible_after)
			{
				// The group as a whole now might need to change whether or not it is animated.
				AISyncServer::client_list_t const& clients = server->getClients();
				for (AISyncServer::client_list_t::const_iterator client = clients.begin(); client != clients.end(); ++client)
				{
					LLMotion* motion = dynamic_cast<LLMotion*>(client->mClientPtr);
					if (!motion)
					{
						continue;
					}
					LLMotionController* controller = motion->getController();
					if (controller == this)
					{
						continue;
					}
					if (visible_after)
					{
					  // Us becoming visible means that all synchronized avatars need to be animated again too.
					  controller->setHaveVisibleSyncedMotions();
					}
					else
					{
					  // Us becoming hidden means that all synchronized avatars might stop animating.
					  controller->refresh_hidden();		// It is extremely unlikely, but harmless, to call this twice on the same controller.
					}
				}
			}
		}
	}
}

void LLMotionController::refresh_hidden(void)
{
	mHaveVisibleSyncedMotions = !mHidden;

	// Run over all motions.
	for (motion_list_t::iterator iter = mActiveMotions.begin(); iter != mActiveMotions.end(); ++iter)
	{
		LLMotion* motionp = *iter;
		AISyncServer* server = motionp->server();
		if (server && !server->never_synced() && motionp->isActive())			// Skip motions that aren't synchronized at all or that are not active.
		{
			bool visible_after = server->events_with_at_least_one_client_ready() & 4;
			if (visible_after)													// Are there any synchronized motions (left) that ARE visible?
			{
				mHaveVisibleSyncedMotions = true;
			}
		}
	}
}

void LLMotionController::pauseAllSyncedCharacters(std::vector<LLAnimPauseRequest>& avatar_pause_handles)
{
	// Run over all motions.
	for (motion_list_t::iterator iter = mActiveMotions.begin(); iter != mActiveMotions.end(); ++iter)
	{
		LLMotion* motionp = *iter;
		AISyncServer* server = motionp->server();
		if (server && !server->never_synced() && motionp->isActive())			// Skip motions that aren't synchronized at all or that are not active.
		{
			// Run over all clients of the found servers.
			AISyncServer::client_list_t const& clients = server->getClients();
			for (AISyncServer::client_list_t::const_iterator client = clients.begin(); client != clients.end(); ++client)
			{
				LLMotion* motion = dynamic_cast<LLMotion*>(client->mClientPtr);
				if (!motion)
				{
					continue;
				}
				LLMotionController* controller = motion->getController();
				if (controller == this)
				{
					continue;
				}
				controller->requestPause(avatar_pause_handles);
			}
		}
	}
}

void LLMotionController::requestPause(std::vector<LLAnimPauseRequest>& avatar_pause_handles)
{
	if (mCharacter)
	{
		mCharacter->requestPause(avatar_pause_handles);
	}
}

//</singu>

//-----------------------------------------------------------------------------
// pause()
//-----------------------------------------------------------------------------
void LLMotionController::pauseAllMotions()
{
	if (!mPaused)
	{
		//LL_INFOS() << "Pausing animations..." << LL_ENDL;
		mPaused = TRUE;
        mPausedFrame = LLFrameTimer::getFrameCount();
	}
	
}

//-----------------------------------------------------------------------------
// unpause()
//-----------------------------------------------------------------------------
void LLMotionController::unpauseAllMotions()
{
	if (mPaused)
	{
		//LL_INFOS() << "Unpausing animations..." << LL_ENDL;
		mPaused = FALSE;
	}
}
// End
