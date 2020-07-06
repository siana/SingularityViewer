/** 
 * @file llkeyframemotion.cpp
 * @brief Implementation of LLKeyframeMotion class.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "linden_common.h"

#include "llmath.h"
#include "llanimationstates.h"
#include "llassetstorage.h"
#include "lldatapacker.h"
#include "llcharacter.h"
#include "llcriticaldamp.h"
#include "lldir.h"
#include "llendianswizzle.h"
#include "llkeyframemotion.h"
#include "llquantize.h"
#include "llvfile.h"
#include "m3math.h"
#include "message.h"
#include <memory>

//-----------------------------------------------------------------------------
// Static Definitions
//-----------------------------------------------------------------------------
LLVFS*				LLKeyframeMotion::sVFS = NULL;
LLKeyframeDataCache::keyframe_data_map_t	LLKeyframeDataCache::sKeyframeDataMap;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static F32 JOINT_LENGTH_K = 0.7f;
static S32 MAX_ITERATIONS = 20;
static S32 MIN_ITERATIONS = 1;
static S32 MIN_ITERATION_COUNT = 2;
static F32 MAX_PIXEL_AREA_CONSTRAINTS = 80000.f;
static F32 MIN_PIXEL_AREA_CONSTRAINTS = 1000.f;
static F32 MIN_ACCELERATION_SQUARED = 0.0005f * 0.0005f;

static F32 MAX_CONSTRAINTS = 10;

//-----------------------------------------------------------------------------
// JointMotionList
//-----------------------------------------------------------------------------
LLKeyframeMotion::JointMotionList::JointMotionList()
	: mDuration(0.f),
	  mLoop(FALSE),
	  mLoopInPoint(0.f),
	  mLoopOutPoint(0.f),
	  mEaseInDuration(0.f),
	  mEaseOutDuration(0.f),
	  mBasePriority(LLJoint::LOW_PRIORITY),
	  mHandPose(LLHandMotion::HAND_POSE_SPREAD),
	  mMaxPriority(LLJoint::LOW_PRIORITY)
{
}

LLKeyframeMotion::JointMotionList::~JointMotionList()
{
	for_each(mConstraints.begin(), mConstraints.end(), DeletePointer());
	for_each(mJointMotionArray.begin(), mJointMotionArray.end(), DeletePointer());
}

//Singu: add parameter 'silent'.
U32 LLKeyframeMotion::JointMotionList::dumpDiagInfo(bool silent) const
{
	S32	total_size = sizeof(JointMotionList);

	for (U32 i = 0; i < getNumJointMotions(); i++)
	{
		LLKeyframeMotion::JointMotion const* joint_motion_p = mJointMotionArray[i];

		if (!silent)
		{
			LL_INFOS() << "\tJoint " << joint_motion_p->mJointName << LL_ENDL;
		}
		if (joint_motion_p->mUsage & LLJointState::SCALE)
		{
			if (!silent)
			{
				LL_INFOS() << "\t" << joint_motion_p->mScaleCurve.mNumKeys << " scale keys at "
				<< joint_motion_p->mScaleCurve.mNumKeys * sizeof(ScaleKey) << " bytes" << LL_ENDL;
			}
			total_size += joint_motion_p->mScaleCurve.mNumKeys * sizeof(ScaleKey);
		}
		if (joint_motion_p->mUsage & LLJointState::ROT)
		{
			if (!silent)
			{
				LL_INFOS() << "\t" << joint_motion_p->mRotationCurve.mNumKeys << " rotation keys at "
				<< joint_motion_p->mRotationCurve.mNumKeys * sizeof(RotationKey) << " bytes" << LL_ENDL;
			}
			total_size += joint_motion_p->mRotationCurve.mNumKeys * sizeof(RotationKey);
		}
		if (joint_motion_p->mUsage & LLJointState::POS)
		{
			if (!silent)
			{
				LL_INFOS() << "\t" << joint_motion_p->mPositionCurve.mNumKeys << " position keys at "
				<< joint_motion_p->mPositionCurve.mNumKeys * sizeof(PositionKey) << " bytes" << LL_ENDL;
			}
			total_size += joint_motion_p->mPositionCurve.mNumKeys * sizeof(PositionKey);
		}
	}
	//Singu: Also add memory used by the constraints.
	S32 constraints_size = mConstraints.size() * sizeof(constraint_list_t::value_type);
	total_size += constraints_size;
	if (!silent)
	{
		LL_INFOS() << "\t" << mConstraints.size() << " constraints at " << constraints_size << " bytes" << LL_ENDL;
		LL_INFOS() << "Size: " << total_size << " bytes" << LL_ENDL;
	}

	return total_size;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// JointMotion class
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// JointMotion::update()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::JointMotion::update(LLJointState* joint_state, F32 time, F32 duration)
{
	// this value being 0 is the cause of https://jira.lindenlab.com/browse/SL-22678 but I haven't 
	// managed to get a stack to see how it got here. Testing for 0 here will stop the crash.
	if ( joint_state == NULL )
	{
		return;
	}

	U32 usage = joint_state->getUsage();

	//-------------------------------------------------------------------------
	// update scale component of joint state
	//-------------------------------------------------------------------------
	if ((usage & LLJointState::SCALE) && mScaleCurve.mNumKeys)
	{
		joint_state->setScale( mScaleCurve.getValue( time, duration ) );
	}

	//-------------------------------------------------------------------------
	// update rotation component of joint state
	//-------------------------------------------------------------------------
	if ((usage & LLJointState::ROT) && mRotationCurve.mNumKeys)
	{
		joint_state->setRotation( mRotationCurve.getValue( time, duration ) );
	}

	//-------------------------------------------------------------------------
	// update position component of joint state
	//-------------------------------------------------------------------------
	if ((usage & LLJointState::POS) && mPositionCurve.mNumKeys)
	{
		joint_state->setPosition( mPositionCurve.getValue( time, duration ) );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// LLKeyframeMotion class
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// LLKeyframeMotion()
// Class Constructor
//-----------------------------------------------------------------------------
LLKeyframeMotion::LLKeyframeMotion(const LLUUID &id, LLMotionController* controller)
	: LLMotion(id, controller),
		mJointMotionList(NULL),
		mPelvisp(NULL),
		mCharacter(NULL),
		mLastSkeletonSerialNum(0),
		mLastUpdateTime(0.f),
		mLastLoopedTime(0.f),
		mAssetStatus(ASSET_UNDEFINED)
{

}


//-----------------------------------------------------------------------------
// ~LLKeyframeMotion()
// Class Destructor
//-----------------------------------------------------------------------------
LLKeyframeMotion::~LLKeyframeMotion()
{
	for_each(mConstraints.begin(), mConstraints.end(), DeletePointer());
}

//-----------------------------------------------------------------------------
// create()
//-----------------------------------------------------------------------------
LLMotion* LLKeyframeMotion::create(LLUUID const& id, LLMotionController* controller)
{
	return new LLKeyframeMotion(id, controller);
}

//-----------------------------------------------------------------------------
// getJointState()
//-----------------------------------------------------------------------------
LLPointer<LLJointState>& LLKeyframeMotion::getJointState(U32 index)
{
	// <edit>
	//llassert_always (index < (S32)mJointStates.size());
	if(index >= (S32)mJointStates.size())
	{
		LL_WARNS() << "LLKeyframeMotion::getJointState: index >= size" << LL_ENDL;
		index = (S32)mJointStates.size() - 1;
	}
	// </edit>
	return mJointStates[index];
}

//-----------------------------------------------------------------------------
// getJoint()
//-----------------------------------------------------------------------------
LLJoint* LLKeyframeMotion::getJoint(U32 index)
{
	// <edit>
	//llassert_always (index < (S32)mJointStates.size());
	if(index >= (S32)mJointStates.size())
		index = (S32)mJointStates.size() - 1;
	// </edit>
	LLJoint* joint = mJointStates[index]->getJoint();
	if(!joint) {
		LL_WARNS_ONCE("Animation") << "Missing joint index:"<< index << " ID:" << mID << " Name:" << mName << LL_ENDL;
	}
	return joint;
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::onInitialize(LLCharacter *character)
//-----------------------------------------------------------------------------
LLMotion::LLMotionInitStatus LLKeyframeMotion::onInitialize(LLCharacter *character)
{
	mCharacter = character;
	
	LLUUID* character_id;

	// asset already loaded?
	switch(mAssetStatus)
	{
	case ASSET_NEEDS_FETCH:
		// request asset
		mAssetStatus = ASSET_FETCHED;

		character_id = new LLUUID(mCharacter->getID());
		gAssetStorage->getAssetData(mID,
						LLAssetType::AT_ANIMATION,
						onLoadComplete,
						(void *)character_id,
						FALSE);

		return STATUS_HOLD;
	case ASSET_FETCHED:
		return STATUS_HOLD;
	case ASSET_FETCH_FAILED:
		LL_WARNS() << "Trying to initialize a motion that failed to be fetched." << LL_ENDL;
		return STATUS_FAILURE;
	case ASSET_LOADED:
		return STATUS_SUCCESS;
	default:
		// we don't know what state the asset is in yet, so keep going
		// check keyframe cache first then static vfs then asset request
		break;
	}

	LLKeyframeMotion::JointMotionListPtr joint_motion_list = LLKeyframeDataCache::getKeyframeData(getID());

	if(joint_motion_list)
	{
		// motion already existed in cache, so grab it
		mJointMotionList = joint_motion_list;

		mJointStates.reserve(mJointMotionList->getNumJointMotions());
		
		// don't forget to allocate joint states
		// set up joint states to point to character joints
		for(U32 i = 0; i < mJointMotionList->getNumJointMotions(); i++)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
			if (LLJoint *joint = mCharacter->getJoint(joint_motion->mJointName))
			{
				LLPointer<LLJointState> joint_state = new LLJointState;
				mJointStates.push_back(joint_state);
				joint_state->setJoint(joint);
				joint_state->setUsage(joint_motion->mUsage);
				joint_state->setPriority(joint_motion->mPriority);
			}
			else
			{
				// add dummy joint state with no associated joint
				mJointStates.push_back(new LLJointState);
			}
		}
		mAssetStatus = ASSET_LOADED;
		setupPose();
		return STATUS_SUCCESS;
	}

	//-------------------------------------------------------------------------
	// Load named file by concatenating the character prefix with the motion name.
	// Load data into a buffer to be parsed.
	//-------------------------------------------------------------------------
	U8 *anim_data;
	S32 anim_file_size;

	if (!sVFS)
	{
		LL_ERRS() << "Must call LLKeyframeMotion::setVFS() first before loading a keyframe file!" << LL_ENDL;
	}

	BOOL success = FALSE;
	LLVFile* anim_file = new LLVFile(sVFS, mID, LLAssetType::AT_ANIMATION);
	if (!anim_file || !anim_file->getSize())
	{
		delete anim_file;
		anim_file = NULL;
		
		// request asset over network on next call to load
		mAssetStatus = ASSET_NEEDS_FETCH;

		return STATUS_HOLD;
	}
	else
	{
		anim_file_size = anim_file->getSize();
		anim_data = new U8[anim_file_size];
		success = anim_file->read(anim_data, anim_file_size);	/*Flawfinder: ignore*/
		delete anim_file;
		anim_file = NULL;
	}

	if (!success)
	{
		LL_WARNS() << "Can't open animation file " << mID << LL_ENDL;
		mAssetStatus = ASSET_FETCH_FAILED;
		return STATUS_FAILURE;
	}

	LL_DEBUGS() << "Loading keyframe data for: " << getName() << ":" << getID() << " (" << anim_file_size << " bytes)" << LL_ENDL;

	LLDataPackerBinaryBuffer dp(anim_data, anim_file_size);

	if (!deserialize(dp, getID()))
	{
		LL_WARNS() << "Failed to decode asset for animation " << getName() << ":" << getID() << LL_ENDL;
		mAssetStatus = ASSET_FETCH_FAILED;
		return STATUS_FAILURE;
	}

	delete []anim_data;

	mAssetStatus = ASSET_LOADED;
	return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// setupPose()
//-----------------------------------------------------------------------------
BOOL LLKeyframeMotion::setupPose()
{
	// add all valid joint states to the pose
	for (U32 jm=0; jm<mJointMotionList->getNumJointMotions(); jm++)
	{
		LLPointer<LLJointState> joint_state = getJointState(jm);
		if ( joint_state->getJoint() )
		{
			addJointState( joint_state );
		}
	}

	// initialize joint constraints
	for (JointMotionList::constraint_list_t::iterator iter = mJointMotionList->mConstraints.begin();
		 iter != mJointMotionList->mConstraints.end(); ++iter)
	{
		JointConstraintSharedData* shared_constraintp = *iter;
		JointConstraint* constraintp = new JointConstraint(shared_constraintp);
		initializeConstraint(constraintp);
		mConstraints.push_front(constraintp);
	}

	if (mJointMotionList->mConstraints.size())
	{
		mPelvisp = mCharacter->getJoint("mPelvis");
		if (!mPelvisp)
		{
			return FALSE;
		}
	}

	// setup loop keys
	setLoopIn(mJointMotionList->mLoopInPoint);
	setLoopOut(mJointMotionList->mLoopOutPoint);

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::onActivate()
//-----------------------------------------------------------------------------
BOOL LLKeyframeMotion::onActivate()
{
	// If the keyframe anim has an associated emote, trigger it. 
	if( mJointMotionList->mEmoteName.length() > 0 )
	{
		LLUUID emote_anim_id = gAnimLibrary.stringToAnimState(mJointMotionList->mEmoteName);
		// don't start emote if already active to avoid recursion
		if (!mCharacter->isMotionActive(emote_anim_id))
		{
			mCharacter->startMotion( emote_anim_id );
		}
	}

	mLastLoopedTime = 0.f;

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::onUpdate()
//-----------------------------------------------------------------------------
BOOL LLKeyframeMotion::onUpdate(F32 time, U8* joint_mask)
{
	// llassert(time >= 0.f);		// This will fire
	time = llmax(0.f, time);

	if (mJointMotionList->mLoop)
	{
		if (mJointMotionList->mDuration == 0.0f)
		{
			time = 0.f;
			mLastLoopedTime = 0.0f;
		}
		else if (mStopped)
		{
			mLastLoopedTime = llmin(mJointMotionList->mDuration, mLastLoopedTime + time - mLastUpdateTime);
		}
		else if (time > mJointMotionList->mLoopOutPoint)
		{
			if ((mJointMotionList->mLoopOutPoint - mJointMotionList->mLoopInPoint) == 0.f)
			{
				mLastLoopedTime = mJointMotionList->mLoopOutPoint;
			}
			else
			{
				mLastLoopedTime = mJointMotionList->mLoopInPoint + 
					fmod(time - mJointMotionList->mLoopOutPoint, 
					mJointMotionList->mLoopOutPoint - mJointMotionList->mLoopInPoint);
			}
		}
		else
		{
			mLastLoopedTime = time;
		}
	}
	else
	{
		mLastLoopedTime = time;
	}

	applyKeyframes(mLastLoopedTime);

	applyConstraints(mLastLoopedTime, joint_mask);

	mLastUpdateTime = time;

	return mLastLoopedTime <= mJointMotionList->mDuration;
}

//-----------------------------------------------------------------------------
// applyKeyframes()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::applyKeyframes(F32 time)
{
	llassert_always (mJointMotionList->getNumJointMotions() <= mJointStates.size());
	for (U32 i=0; i<mJointMotionList->getNumJointMotions(); i++)
	{
		mJointMotionList->getJointMotion(i)->update(mJointStates[i],
													  time, 
													  mJointMotionList->mDuration );
	}

	LLJoint::JointPriority* pose_priority = (LLJoint::JointPriority* )mCharacter->getAnimationData("Hand Pose Priority");
	if (pose_priority)
	{
		if (mJointMotionList->mMaxPriority >= *pose_priority)
		{
			mCharacter->setAnimationData("Hand Pose", &mJointMotionList->mHandPose);
			mCharacter->setAnimationData("Hand Pose Priority", &mJointMotionList->mMaxPriority);
		}
	}
	else
	{
		mCharacter->setAnimationData("Hand Pose", &mJointMotionList->mHandPose);
		mCharacter->setAnimationData("Hand Pose Priority", &mJointMotionList->mMaxPriority);
	}
}

//-----------------------------------------------------------------------------
// applyConstraints()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::applyConstraints(F32 time, U8* joint_mask)
{
	//TODO: investigate replacing spring simulation with critically damped motion

	// re-init constraints if skeleton has changed
	if (mCharacter->getSkeletonSerialNum() != mLastSkeletonSerialNum)
	{
		mLastSkeletonSerialNum = mCharacter->getSkeletonSerialNum();
		for (constraint_list_t::iterator iter = mConstraints.begin();
			 iter != mConstraints.end(); ++iter)
		{
			JointConstraint* constraintp = *iter;
			initializeConstraint(constraintp);
		}
	}

	// apply constraints
	for (constraint_list_t::iterator iter = mConstraints.begin();
		 iter != mConstraints.end(); ++iter)
	{
		JointConstraint* constraintp = *iter;
		applyConstraint(constraintp, time, joint_mask);
	}
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::onDeactivate()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::onDeactivate()
{
	for (constraint_list_t::iterator iter = mConstraints.begin();
		 iter != mConstraints.end(); ++iter)
	{
		JointConstraint* constraintp = *iter;
		deactivateConstraint(constraintp);
	}
}

//-----------------------------------------------------------------------------
// setStopTime()
//-----------------------------------------------------------------------------
//
// Consider a looping animation of 20 frames, where the loop in point is at 3 frames
// and the loop out point at 16 frames:
//
// The first 3 frames of the animation would be the "loop in" animation.
// The last 4 frames of the animation would be the "loop out" animation.
// Frames 4 through 15 would be the looping animation frames.
//
// If the animation would not be looping, all frames would just be played once sequentially:
//
// mActivationTimestamp -.
//                       v
//						 0  3         15 16  20
//						 |  |           \|   |
//						 ---------------------
//                       <-->                    <-- mLoopInPoint (relative to mActivationTimestamp)
//                       <-------------->        <-- mLoopOutPoint (relative to mActivationTimestamp)
//                       <----mDuration------>
//
// When looping the animation would repeat frames 3 to 16 (loop) a few times, for example:
//
// 0  3         15 3         15 3         15 3         15 16  20
// |  |  loop 1   \|  loop 2   \|  loop 3   \|  loop 4   \|   |
// ------------------------------------------------------------
//LOOP^                                             ^      LOOP
// IN |                                      <----->|      OUT
//  start_loop_time          loop_fraction_time-'  time
//
// The time at which the animation is started corresponds to frame 0 and is stored
// in mActivationTimestamp (in seconds since character creation).
//
// If setStopTime() is called with a time somewhere inside loop 4,
// then 'loop_fraction_time' is the time from the beginning of
// loop 4 till 'time'. Thus 'time - loop_fraction_time' is the first
// frame of loop 4, and '(time - loop_fraction_time) +
// (mJointMotionList->mDuration - mJointMotionList->mLoopInPoint)'
// would correspond to frame 20.
//
void LLKeyframeMotion::setStopTime(F32 time)
{
	LLMotion::setStopTime(time);

	if (mJointMotionList->mLoop && mJointMotionList->mLoopOutPoint != mJointMotionList->mDuration)
	{
		F32 start_loop_time = mActivationTimestamp + mJointMotionList->mLoopInPoint;
		F32 loop_fraction_time;
		if (mJointMotionList->mLoopOutPoint == mJointMotionList->mLoopInPoint)
		{
			loop_fraction_time = 0.f;
		}
		else
		{
			loop_fraction_time = fmod(time - start_loop_time, 
				mJointMotionList->mLoopOutPoint - mJointMotionList->mLoopInPoint);
		}
		// This sets mStopTimestamp to the time that corresponds to the end of the animation (ie, frame 20 in the above example)
		// minus the ease out duration, so that the animation eases out during the loop out and finishes exactly at the end.
		mStopTimestamp = llmax(time, 
			(time - loop_fraction_time) + (mJointMotionList->mDuration - mJointMotionList->mLoopInPoint) - getEaseOutDuration());
	}
}

//-----------------------------------------------------------------------------
// initializeConstraint()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::initializeConstraint(JointConstraint* constraint)
{
	JointConstraintSharedData *shared_data = constraint->mSharedData;

	S32 joint_num;
	LLVector3 source_pos = mCharacter->getVolumePos(shared_data->mSourceConstraintVolume, shared_data->mSourceConstraintOffset);
	LLJoint* cur_joint = getJoint(shared_data->mJointStateIndices[0]);
	if ( !cur_joint )
	{
		return;
	}
	
	F32 source_pos_offset = dist_vec(source_pos, cur_joint->getWorldPosition());

	constraint->mTotalLength = constraint->mJointLengths[0] = dist_vec(cur_joint->getParent()->getWorldPosition(), source_pos);
	
	// grab joint lengths
	for (joint_num = 1; joint_num < shared_data->mChainLength; joint_num++)
	{
		cur_joint = getJointState(shared_data->mJointStateIndices[joint_num])->getJoint();
		if (!cur_joint)
		{
			return;
		}
		constraint->mJointLengths[joint_num] = dist_vec(cur_joint->getWorldPosition(), cur_joint->getParent()->getWorldPosition());
		constraint->mTotalLength += constraint->mJointLengths[joint_num];
	}

	// store fraction of total chain length so we know how to shear the entire chain towards the goal position
	for (joint_num = 1; joint_num < shared_data->mChainLength; joint_num++)
	{
		constraint->mJointLengthFractions[joint_num] = constraint->mJointLengths[joint_num] / constraint->mTotalLength;
	}

	// add last step in chain, from final joint to constraint position
	constraint->mTotalLength += source_pos_offset;

	constraint->mSourceVolume = mCharacter->findCollisionVolume(shared_data->mSourceConstraintVolume);
	constraint->mTargetVolume = mCharacter->findCollisionVolume(shared_data->mTargetConstraintVolume);
}

//-----------------------------------------------------------------------------
// activateConstraint()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::activateConstraint(JointConstraint* constraint)
{
	JointConstraintSharedData *shared_data = constraint->mSharedData;
	constraint->mActive = TRUE;
	S32 joint_num;

	// grab ground position if we need to
	if (shared_data->mConstraintTargetType == CONSTRAINT_TARGET_TYPE_GROUND)
	{
		LLVector3 source_pos = mCharacter->getVolumePos(shared_data->mSourceConstraintVolume, shared_data->mSourceConstraintOffset);
		LLVector3 ground_pos_agent;
		mCharacter->getGround(source_pos, ground_pos_agent, constraint->mGroundNorm);
		constraint->mGroundPos = mCharacter->getPosGlobalFromAgent(ground_pos_agent + shared_data->mTargetConstraintOffset);
	}

	for (joint_num = 1; joint_num < shared_data->mChainLength; joint_num++)
	{
		LLJoint* cur_joint = getJoint(shared_data->mJointStateIndices[joint_num]);
		if ( !cur_joint )
		{
			return;
		}
		constraint->mPositions[joint_num] = (cur_joint->getWorldPosition() - mPelvisp->getWorldPosition()) * ~mPelvisp->getWorldRotation();
	}

	constraint->mWeight = 1.f;
}

//-----------------------------------------------------------------------------
// deactivateConstraint()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::deactivateConstraint(JointConstraint *constraintp)
{
	if (constraintp->mSourceVolume)
	{
		constraintp->mSourceVolume->mUpdateXform = FALSE;
	}

	if (constraintp->mSharedData->mConstraintTargetType != CONSTRAINT_TARGET_TYPE_GROUND)
	{
		if (constraintp->mTargetVolume)
		{
			constraintp->mTargetVolume->mUpdateXform = FALSE;
		}
	}
	constraintp->mActive = FALSE;
}

//-----------------------------------------------------------------------------
// applyConstraint()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::applyConstraint(JointConstraint* constraint, F32 time, U8* joint_mask)
{
	JointConstraintSharedData *shared_data = constraint->mSharedData;
	if (!shared_data) return;

	LLVector3		positions[MAX_CHAIN_LENGTH];
	const F32*		joint_lengths = constraint->mJointLengths;
	LLVector3		velocities[MAX_CHAIN_LENGTH - 1];
	LLQuaternion	old_rots[MAX_CHAIN_LENGTH];
	S32				joint_num;

	if (time < shared_data->mEaseInStartTime)
	{
		return;
	}

	if (time > shared_data->mEaseOutStopTime)
	{
		if (constraint->mActive)
		{
			deactivateConstraint(constraint);
		}
		return;
	}

	if (!constraint->mActive || time < shared_data->mEaseInStopTime)
	{
		activateConstraint(constraint);
	}

	LLJoint* root_joint = getJoint(shared_data->mJointStateIndices[shared_data->mChainLength]);
	if (! root_joint) 
	{
		return;
	}
	
	LLVector3 root_pos = root_joint->getWorldPosition();
//	LLQuaternion root_rot = 
	root_joint->getParent()->getWorldRotation();
//	LLQuaternion inv_root_rot = ~root_rot;

//	LLVector3 current_source_pos = mCharacter->getVolumePos(shared_data->mSourceConstraintVolume, shared_data->mSourceConstraintOffset);

	//apply underlying keyframe animation to get nominal "kinematic" joint positions
	for (joint_num = 0; joint_num <= shared_data->mChainLength; joint_num++)
	{
		LLJoint* cur_joint = getJoint(shared_data->mJointStateIndices[joint_num]);
		if (!cur_joint)
		{
			return;
		}
		
		if (joint_mask[cur_joint->getJointNum()] >= (0xff >> (7 - getPriority())))
		{
			// skip constraint
			return;
		}
		old_rots[joint_num] = cur_joint->getRotation();
		cur_joint->setRotation(getJointState(shared_data->mJointStateIndices[joint_num])->getRotation());
	}


	LLVector3 keyframe_source_pos = mCharacter->getVolumePos(shared_data->mSourceConstraintVolume, shared_data->mSourceConstraintOffset);
	LLVector3 target_pos;

	switch(shared_data->mConstraintTargetType)
	{
	case CONSTRAINT_TARGET_TYPE_GROUND:
		target_pos = mCharacter->getPosAgentFromGlobal(constraint->mGroundPos);
		//target_pos += mCharacter->getHoverOffset();
//		LL_INFOS() << "Target Pos " << constraint->mGroundPos << " on " << mCharacter->findCollisionVolume(shared_data->mSourceConstraintVolume)->getName() << LL_ENDL;
		break;
	case CONSTRAINT_TARGET_TYPE_BODY:
		target_pos = mCharacter->getVolumePos(shared_data->mTargetConstraintVolume, shared_data->mTargetConstraintOffset);
		break;
	default:
		break;
	}
	
	LLVector3 norm;
	LLJoint *source_jointp = NULL;
	LLJoint *target_jointp = NULL;

	if (shared_data->mConstraintType == CONSTRAINT_TYPE_PLANE)
	{
		switch(shared_data->mConstraintTargetType)
		{
		case CONSTRAINT_TARGET_TYPE_GROUND:
			norm = constraint->mGroundNorm;
			break;
		case CONSTRAINT_TARGET_TYPE_BODY:
			target_jointp = mCharacter->findCollisionVolume(shared_data->mTargetConstraintVolume);
			if (target_jointp)
			{
				// *FIX: do proper normal calculation for stretched
				// spheres (inverse transpose)
				norm = target_pos - target_jointp->getWorldPosition();
			}

			if (norm.isExactlyZero())
			{
				source_jointp = mCharacter->findCollisionVolume(shared_data->mSourceConstraintVolume);
				norm = -1.f * shared_data->mSourceConstraintOffset;
				if (source_jointp)	
				{
					norm = norm * source_jointp->getWorldRotation();
				}
			}
			norm.normVec();
			break;
		default:
			norm.clearVec();
			break;
		}
		
		target_pos = keyframe_source_pos + (norm * ((target_pos - keyframe_source_pos) * norm));
	}

	if (constraint->mSharedData->mChainLength != 0 &&
		dist_vec_squared(root_pos, target_pos) * 0.95f > constraint->mTotalLength * constraint->mTotalLength)
	{
		constraint->mWeight = LLSmoothInterpolation::lerp(constraint->mWeight, 0.f, 0.1f);
	}
	else
	{
		constraint->mWeight = LLSmoothInterpolation::lerp(constraint->mWeight, 1.f, 0.3f);
	}

	F32 weight = constraint->mWeight * ((shared_data->mEaseOutStopTime == 0.f) ? 1.f : 
		llmin(clamp_rescale(time, shared_data->mEaseInStartTime, shared_data->mEaseInStopTime, 0.f, 1.f), 
		clamp_rescale(time, shared_data->mEaseOutStartTime, shared_data->mEaseOutStopTime, 1.f, 0.f)));

	LLVector3 source_to_target = target_pos - keyframe_source_pos;
	
	S32 max_iteration_count = ll_pos_round(clamp_rescale(
										  mCharacter->getPixelArea(),
										  MAX_PIXEL_AREA_CONSTRAINTS,
										  MIN_PIXEL_AREA_CONSTRAINTS,
										  (F32)MAX_ITERATIONS,
										  (F32)MIN_ITERATIONS));

	if (shared_data->mChainLength)
	{
		LLJoint* end_joint = getJoint(shared_data->mJointStateIndices[0]);
		
		if (!end_joint)
		{
			return;
		}
		
		LLQuaternion end_rot = end_joint->getWorldRotation();

		// slam start and end of chain to the proper positions (rest of chain stays put)
		positions[0] = lerp(keyframe_source_pos, target_pos, weight);
		positions[shared_data->mChainLength] = root_pos;

		// grab keyframe-specified positions of joints	
		for (joint_num = 1; joint_num < shared_data->mChainLength; joint_num++)
		{
			LLJoint* cur_joint = getJoint(shared_data->mJointStateIndices[joint_num]);
			
			if (!cur_joint)
			{
				return;
			}
			
			LLVector3 kinematic_position = cur_joint->getWorldPosition() + 
				(source_to_target * constraint->mJointLengthFractions[joint_num]);

			// convert intermediate joint positions to world coordinates
			positions[joint_num] = ( constraint->mPositions[joint_num] * mPelvisp->getWorldRotation()) + mPelvisp->getWorldPosition();
			F32 time_constant = 1.f / clamp_rescale(constraint->mFixupDistanceRMS, 0.f, 0.5f, 0.2f, 8.f);
//			LL_INFOS() << "Interpolant " << LLSmoothInterpolation::getInterpolant(time_constant, FALSE) << " and fixup distance " << constraint->mFixupDistanceRMS << " on " << mCharacter->findCollisionVolume(shared_data->mSourceConstraintVolume)->getName() << LL_ENDL;
			positions[joint_num] = lerp(positions[joint_num], kinematic_position, 
				LLSmoothInterpolation::getInterpolant(time_constant, FALSE));
		}

		S32 iteration_count;
		for (iteration_count = 0; iteration_count < max_iteration_count; iteration_count++)
		{
			S32 num_joints_finished = 0;
			for (joint_num = 1; joint_num < shared_data->mChainLength; joint_num++)
			{
				// constraint to child
				LLVector3 acceleration = (positions[joint_num - 1] - positions[joint_num]) * 
					(dist_vec(positions[joint_num], positions[joint_num - 1]) - joint_lengths[joint_num - 1]) * JOINT_LENGTH_K;
				// constraint to parent
				acceleration  += (positions[joint_num + 1] - positions[joint_num]) * 
					(dist_vec(positions[joint_num + 1], positions[joint_num]) - joint_lengths[joint_num]) * JOINT_LENGTH_K;

				if (acceleration.magVecSquared() < MIN_ACCELERATION_SQUARED)
				{
					num_joints_finished++;
				}

				velocities[joint_num - 1] = velocities[joint_num - 1] * 0.7f;
				positions[joint_num] += velocities[joint_num - 1] + (acceleration * 0.5f);
				velocities[joint_num - 1] += acceleration;
			}

			if ((iteration_count >= MIN_ITERATION_COUNT) && 
				(num_joints_finished == shared_data->mChainLength - 1))
			{
//				LL_INFOS() << iteration_count << " iterations on " << 
//					mCharacter->findCollisionVolume(shared_data->mSourceConstraintVolume)->getName() << LL_ENDL;
				break;
			}
		}

		for (joint_num = shared_data->mChainLength; joint_num > 0; joint_num--)
		{
			LLJoint* cur_joint = getJoint(shared_data->mJointStateIndices[joint_num]);
			
			if (!cur_joint)
			{
				return;
			}
			LLJoint* child_joint = getJoint(shared_data->mJointStateIndices[joint_num - 1]);
			if (!child_joint)
			{
				return;
			}
			
			LLQuaternion parent_rot = cur_joint->getParent()->getWorldRotation();

			LLQuaternion cur_rot = cur_joint->getWorldRotation();
			LLQuaternion fixup_rot;
			
			LLVector3 target_at = positions[joint_num - 1] - positions[joint_num];
			LLVector3 current_at;

			// at bottom of chain, use point on collision volume, not joint position
			if (joint_num == 1)
			{
				current_at = mCharacter->getVolumePos(shared_data->mSourceConstraintVolume, shared_data->mSourceConstraintOffset) -
					cur_joint->getWorldPosition();
			}
			else
			{
				current_at = child_joint->getPosition() * cur_rot;
			}
			fixup_rot.shortestArc(current_at, target_at);

			LLQuaternion target_rot = cur_rot * fixup_rot;
			target_rot = target_rot * ~parent_rot;

			if (weight != 1.f)
			{
				LLQuaternion cur_rot = getJointState(shared_data->mJointStateIndices[joint_num])->getRotation();
				target_rot = nlerp(weight, cur_rot, target_rot);
			}

			getJointState(shared_data->mJointStateIndices[joint_num])->setRotation(target_rot);
			cur_joint->setRotation(target_rot);
		}

		LLQuaternion end_local_rot = end_rot * ~end_joint->getParent()->getWorldRotation();

		if (weight == 1.f)
		{
			getJointState(shared_data->mJointStateIndices[0])->setRotation(end_local_rot);
		}
		else
		{
			LLQuaternion cur_rot = getJointState(shared_data->mJointStateIndices[0])->getRotation();
			getJointState(shared_data->mJointStateIndices[0])->setRotation(nlerp(weight, cur_rot, end_local_rot));
		}

		// save simulated positions in pelvis-space and calculate total fixup distance
		constraint->mFixupDistanceRMS = 0.f;
		F32 delta_time = llmax(0.02f, llabs(time - mLastUpdateTime));
		for (joint_num = 1; joint_num < shared_data->mChainLength; joint_num++)
		{
			LLVector3 new_pos = (positions[joint_num] - mPelvisp->getWorldPosition()) * ~mPelvisp->getWorldRotation();
			constraint->mFixupDistanceRMS += dist_vec_squared(new_pos, constraint->mPositions[joint_num]) / delta_time;
			constraint->mPositions[joint_num] = new_pos;
		}
		constraint->mFixupDistanceRMS *= 1.f / (constraint->mTotalLength * (F32)(shared_data->mChainLength - 1));
		constraint->mFixupDistanceRMS = (F32) sqrt(constraint->mFixupDistanceRMS);

		//reset old joint rots
		for (joint_num = 0; joint_num <= shared_data->mChainLength; joint_num++)
		{
			LLJoint* cur_joint = getJoint(shared_data->mJointStateIndices[joint_num]);
			if (!cur_joint)
			{
				return;
			}

			cur_joint->setRotation(old_rots[joint_num]);
		}
	}
	// simple positional constraint (pelvis only)
	else if (getJointState(shared_data->mJointStateIndices[0])->getUsage() & LLJointState::POS)
	{
		LLVector3 delta = source_to_target * weight;
		LLPointer<LLJointState> current_joint_state = getJointState(shared_data->mJointStateIndices[0]);
		if (current_joint_state->getJoint())
		{
			LLQuaternion parent_rot = current_joint_state->getJoint()->getParent()->getWorldRotation();
			delta = delta * ~parent_rot;
			current_joint_state->setPosition(current_joint_state->getJoint()->getPosition() + delta);
		}
	}
}

//-----------------------------------------------------------------------------
// deserialize()
//-----------------------------------------------------------------------------
BOOL LLKeyframeMotion::deserialize(LLDataPacker& dp, const LLUUID& asset_id)
{
	BOOL old_version = FALSE;

	//<singu>

	// First add a new LLKeyframeMotion::JointMotionList to the cache, then assign a pointer
	// to that to mJointMotionList. In LLs code the cache is never deleted again. Now it is
	// is deleted when the last mJointMotionList pointer is destructed.
	//
	// It is possible that we get here for an already added animation, because animations can
	// be requested multiple times (we get here from LLKeyframeMotion::onLoadComplete) when
	// the animation was still downloading from a previous request for another LLMotionController
	// object (avatar). In that case we just overwrite the old data while decoding it again.
	mJointMotionList = LLKeyframeDataCache::getKeyframeData(getID());
	bool singu_new_joint_motion_list = !mJointMotionList;
	if (singu_new_joint_motion_list)
	{
		// Create a new JointMotionList.
		mJointMotionList = LLKeyframeDataCache::createKeyframeData(getID());
	}

	//</singu>

	//-------------------------------------------------------------------------
	// get base priority
	//-------------------------------------------------------------------------
	S32 temp_priority;
	U16 version;
	U16 sub_version;

	if (!dp.unpackU16(version, "version"))
	{
		LL_WARNS() << "can't read version number for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (!dp.unpackU16(sub_version, "sub_version"))
	{
		LL_WARNS() << "can't read sub version number for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (version == 0 && sub_version == 1)
	{
		old_version = TRUE;
	}
	else if (version != KEYFRAME_MOTION_VERSION || sub_version != KEYFRAME_MOTION_SUBVERSION)
	{
#if LL_RELEASE
		LL_WARNS() << "Bad animation version " << version << "." << sub_version 
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
#else
		LL_ERRS() << "Bad animation version " << version << "." << sub_version
                  << " for animation " << asset_id << LL_ENDL;
#endif
	}

	if (!dp.unpackS32(temp_priority, "base_priority"))
	{
		LL_WARNS() << "can't read animation base_priority"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}
	mJointMotionList->mBasePriority = (LLJoint::JointPriority) temp_priority;

	if (mJointMotionList->mBasePriority >= LLJoint::ADDITIVE_PRIORITY)
	{
		mJointMotionList->mBasePriority = (LLJoint::JointPriority)((S32)LLJoint::ADDITIVE_PRIORITY-1);
		mJointMotionList->mMaxPriority = mJointMotionList->mBasePriority;
	}
	else if (mJointMotionList->mBasePriority < LLJoint::USE_MOTION_PRIORITY)
	{
		LL_WARNS() << "bad animation base_priority " << mJointMotionList->mBasePriority
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// get duration
	//-------------------------------------------------------------------------
	if (!dp.unpackF32(mJointMotionList->mDuration, "duration"))
	{
		LL_WARNS() << "can't read duration"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}
	
	if (mJointMotionList->mDuration > MAX_ANIM_DURATION ||
	    !std::isfinite(mJointMotionList->mDuration))
	{
		LL_WARNS() << "invalid animation duration"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// get emote (optional)
	//-------------------------------------------------------------------------
	if (!dp.unpackString(mJointMotionList->mEmoteName, "emote_name"))
	{
		LL_WARNS() << "can't read optional_emote_animation"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if(mJointMotionList->mEmoteName==mID.asString())
	{
		LL_WARNS() << "Malformed animation mEmoteName==mID"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// get loop
	//-------------------------------------------------------------------------
	if (!dp.unpackF32(mJointMotionList->mLoopInPoint, "loop_in_point") ||
	    !std::isfinite(mJointMotionList->mLoopInPoint))
	{
		LL_WARNS() << "can't read loop point"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (!dp.unpackF32(mJointMotionList->mLoopOutPoint, "loop_out_point") ||
	    !std::isfinite(mJointMotionList->mLoopOutPoint))
	{
		LL_WARNS() << "can't read loop point"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (!dp.unpackS32(mJointMotionList->mLoop, "loop"))
	{
		LL_WARNS() << "can't read loop"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// get easeIn and easeOut
	//-------------------------------------------------------------------------
	if (!dp.unpackF32(mJointMotionList->mEaseInDuration, "ease_in_duration") ||
	    !std::isfinite(mJointMotionList->mEaseInDuration))
	{
		LL_WARNS() << "can't read easeIn"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (!dp.unpackF32(mJointMotionList->mEaseOutDuration, "ease_out_duration") ||
	    !std::isfinite(mJointMotionList->mEaseOutDuration))
	{
		LL_WARNS() << "can't read easeOut"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// get hand pose
	//-------------------------------------------------------------------------
	U32 word;
	if (!dp.unpackU32(word, "hand_pose"))
	{
		LL_WARNS() << "can't read hand pose"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}
	
	if(word > LLHandMotion::NUM_HAND_POSES)
	{
		LL_WARNS() << "invalid LLHandMotion::eHandPose index: " << word
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}
	
	mJointMotionList->mHandPose = (LLHandMotion::eHandPose)word;

	//-------------------------------------------------------------------------
	// get number of joint motions
	//-------------------------------------------------------------------------
	U32 num_motions = 0;
	if (!dp.unpackU32(num_motions, "num_joints"))
	{
		LL_WARNS() << "can't read number of joints"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (num_motions == 0)
	{
		LL_WARNS() << "no joints"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}
	else if (num_motions > LL_CHARACTER_MAX_ANIMATED_JOINTS)
	{
		LL_WARNS() << "too many joints"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (singu_new_joint_motion_list)
	{
		mJointMotionList->mJointMotionArray.reserve(num_motions);
	}
	mJointStates.clear();
	mJointStates.reserve(num_motions);

	//-------------------------------------------------------------------------
	// initialize joint motions
	//-------------------------------------------------------------------------

	for(U32 i=0; i<num_motions; ++i)
	{
		JointMotion* joint_motion = new JointMotion;
		std::unique_ptr<JointMotion> watcher(joint_motion);
		if (singu_new_joint_motion_list)
		{
			// Pass ownership to mJointMotionList.
			mJointMotionList->mJointMotionArray.push_back(watcher.release());
		}
		
		std::string joint_name;
		if (!dp.unpackString(joint_name, "joint_name"))
		{
			LL_WARNS() << "can't read joint name"
                       << " for animation " << asset_id << LL_ENDL;
			return FALSE;
		}

		if (joint_name == "mScreen" || joint_name == "mRoot")
		{
			LL_WARNS() << "attempted to animate special " << joint_name << " joint"
                       << " for animation " << asset_id << LL_ENDL;
			return FALSE;
		}
				
		//---------------------------------------------------------------------
		// find the corresponding joint
		//---------------------------------------------------------------------
		LLJoint *joint = mCharacter->getJoint( joint_name );
		if (joint)
		{
			S32 joint_num = joint->getJointNum();
//			LL_INFOS() << "  joint: " << joint_name << LL_ENDL;
			if ((joint_num >= (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS) || (joint_num < 0))
			{
                LL_WARNS() << "Joint will be omitted from animation: joint_num " << joint_num 
                           << " is outside of legal range [0-"
                           << LL_CHARACTER_MAX_ANIMATED_JOINTS << ") for joint " << joint->getName()
                           << " for animation " << asset_id << LL_ENDL;
                joint = NULL;
            }
		}
		else
		{
			// <edit>
			int sz = joint_name.size();
			int i = 0;
			while (i < sz)
			{
				if ('\a' == joint_name[i])
				{
					joint_name.replace(i, 1, " ");
				}
				i++;
			}
			// </edit>
			LL_WARNS() << "invalid joint name: " << joint_name
                       << " for animation " << asset_id << LL_ENDL;
			//return FALSE;
		}

		joint_motion->mJointName = joint_name;
		
		LLPointer<LLJointState> joint_state = new LLJointState;
		mJointStates.push_back(joint_state);
		joint_state->setJoint( joint ); // note: can accept NULL
		joint_state->setUsage( 0 );

		//---------------------------------------------------------------------
		// get joint priority
		//---------------------------------------------------------------------
		S32 joint_priority;
		if (!dp.unpackS32(joint_priority, "joint_priority"))
		{
			LL_WARNS() << "can't read joint priority."
                       << " for animation " << asset_id << LL_ENDL;
			return FALSE;
		}

		if (joint_priority < LLJoint::USE_MOTION_PRIORITY)
		{
			LL_WARNS() << "joint priority unknown - too low."
                       << " for animation " << asset_id << LL_ENDL;
			return FALSE;
		}
		
		joint_motion->mPriority = (LLJoint::JointPriority)joint_priority;
		if (joint_priority != LLJoint::USE_MOTION_PRIORITY &&
		    joint_priority > mJointMotionList->mMaxPriority)
		{
			mJointMotionList->mMaxPriority = (LLJoint::JointPriority)joint_priority;
		}

		joint_state->setPriority((LLJoint::JointPriority)joint_priority);

		//---------------------------------------------------------------------
		// scan rotation curve header
		//---------------------------------------------------------------------
		if (!dp.unpackS32(joint_motion->mRotationCurve.mNumKeys, "num_rot_keys") || joint_motion->mRotationCurve.mNumKeys < 0)
		{
			LL_WARNS() << "can't read number of rotation keys"
                       << " for animation " << asset_id << LL_ENDL;
			return FALSE;
		}

		joint_motion->mRotationCurve.mInterpolationType = IT_LINEAR;
		if (joint_motion->mRotationCurve.mNumKeys != 0)
		{
			joint_state->setUsage(joint_state->getUsage() | LLJointState::ROT );
		}

		//---------------------------------------------------------------------
		// scan rotation curve keys
		//---------------------------------------------------------------------
		RotationCurve *rCurve = &joint_motion->mRotationCurve;

		for (S32 k = 0; k < joint_motion->mRotationCurve.mNumKeys; k++)
		{
			F32 time;
			U16 time_short;

			if (old_version)
			{
				if (!dp.unpackF32(time, "time") ||
				    !std::isfinite(time))
				{
					LL_WARNS() << "can't read rotation key (" << k << ")"
                               << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}

			}
			else
			{
				if (!dp.unpackU16(time_short, "time"))
				{
					LL_WARNS() << "can't read rotation key (" << k << ")"
                               << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}

				time = U16_to_F32(time_short, 0.f, mJointMotionList->mDuration);
				
				if (time < 0 || time > mJointMotionList->mDuration)
				{
					LL_WARNS() << "invalid frame time"
                               << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}
			}
			
			RotationKey rot_key;
			rot_key.mTime = time;
			LLVector3 rot_angles;
			U16 x, y, z;

			BOOL success = TRUE;

			if (old_version)
			{
				success = dp.unpackVector3(rot_angles, "rot_angles") && rot_angles.isFinite();

				LLQuaternion::Order ro = StringToOrder("ZYX");
				rot_key.mValue = mayaQ(rot_angles.mV[VX], rot_angles.mV[VY], rot_angles.mV[VZ], ro);
			}
			else
			{
				success &= dp.unpackU16(x, "rot_angle_x");
				success &= dp.unpackU16(y, "rot_angle_y");
				success &= dp.unpackU16(z, "rot_angle_z");

				LLVector3 rot_vec;
				rot_vec.mV[VX] = U16_to_F32(x, -1.f, 1.f);
				rot_vec.mV[VY] = U16_to_F32(y, -1.f, 1.f);
				rot_vec.mV[VZ] = U16_to_F32(z, -1.f, 1.f);
				rot_key.mValue.unpackFromVector3(rot_vec);
			}

			if( !(rot_key.mValue.isFinite()) )
			{
				LL_WARNS() << "non-finite angle in rotation key"
                           << " for animation " << asset_id << LL_ENDL;
				success = FALSE;
			}
			
			if (!success)
			{
				LL_WARNS() << "can't read rotation key (" << k << ")"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			rCurve->mKeys.emplace_back(time, rot_key);
		}

		std::sort(rCurve->mKeys.begin(), rCurve->mKeys.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		//---------------------------------------------------------------------
		// scan position curve header
		//---------------------------------------------------------------------
		if (!dp.unpackS32(joint_motion->mPositionCurve.mNumKeys, "num_pos_keys") || joint_motion->mPositionCurve.mNumKeys < 0)
		{
			LL_WARNS() << "can't read number of position keys"
                       << " for animation " << asset_id << LL_ENDL;
			return FALSE;
		}

		joint_motion->mPositionCurve.mInterpolationType = IT_LINEAR;
		if (joint_motion->mPositionCurve.mNumKeys != 0)
		{
			joint_state->setUsage(joint_state->getUsage() | LLJointState::POS );
		}

		//---------------------------------------------------------------------
		// scan position curve keys
		//---------------------------------------------------------------------
		PositionCurve *pCurve = &joint_motion->mPositionCurve;
		BOOL is_pelvis = joint_motion->mJointName == "mPelvis";
		for (S32 k = 0; k < joint_motion->mPositionCurve.mNumKeys; k++)
		{
			U16 time_short;
			PositionKey pos_key;

			if (old_version)
			{
				if (!dp.unpackF32(pos_key.mTime, "time") ||
				    !std::isfinite(pos_key.mTime))
				{
					LL_WARNS() << "can't read position key (" << k << ")"
                               << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}
			}
			else
			{
				if (!dp.unpackU16(time_short, "time"))
				{
					LL_WARNS() << "can't read position key (" << k << ")"
                               << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}

				pos_key.mTime = U16_to_F32(time_short, 0.f, mJointMotionList->mDuration);
			}

			BOOL success = TRUE;

			if (old_version)
			{
				success = dp.unpackVector3(pos_key.mValue, "pos");
                
                //MAINT-6162
                pos_key.mValue.mV[VX] = llclamp( pos_key.mValue.mV[VX], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
                pos_key.mValue.mV[VY] = llclamp( pos_key.mValue.mV[VY], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
                pos_key.mValue.mV[VZ] = llclamp( pos_key.mValue.mV[VZ], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
                
			}
			else
			{
				U16 x, y, z;

				success &= dp.unpackU16(x, "pos_x");
				success &= dp.unpackU16(y, "pos_y");
				success &= dp.unpackU16(z, "pos_z");

				pos_key.mValue.mV[VX] = U16_to_F32(x, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
				pos_key.mValue.mV[VY] = U16_to_F32(y, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
				pos_key.mValue.mV[VZ] = U16_to_F32(z, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			}
			
			if( !(pos_key.mValue.isFinite()) )
			{
				LL_WARNS() << "non-finite position in key"
                           << " for animation " << asset_id << LL_ENDL;
				success = FALSE;
			}
			
			if (!success)
			{
				LL_WARNS() << "can't read position key (" << k << ")"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}
			
			pCurve->mKeys.emplace_back(pos_key.mTime, pos_key);

			if (is_pelvis)
			{
				mJointMotionList->mPelvisBBox.addPoint(pos_key.mValue);
			}

		}

		std::sort(pCurve->mKeys.begin(), pCurve->mKeys.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		joint_motion->mUsage = joint_state->getUsage();
	}

	//-------------------------------------------------------------------------
	// get number of constraints
	//-------------------------------------------------------------------------
	S32 num_constraints = 0;
	if (!dp.unpackS32(num_constraints, "num_constraints"))
	{
		LL_WARNS() << "can't read number of constraints"
                   << " for animation " << asset_id << LL_ENDL;
		return FALSE;
	}

	if (num_constraints > MAX_CONSTRAINTS || num_constraints < 0)
	{
		LL_WARNS() << "Bad number of constraints... ignoring: " << num_constraints
                   << " for animation " << asset_id << LL_ENDL;
	}
	else
	{
		//-------------------------------------------------------------------------
		// get constraints
		//-------------------------------------------------------------------------
		std::string str;
		for(S32 i = 0; i < num_constraints; ++i)
		{
			// read in constraint data
			auto constraintp = new JointConstraintSharedData;
			std::unique_ptr<JointConstraintSharedData> watcher(constraintp);
			U8 byte = 0;

			if (!dp.unpackU8(byte, "chain_length"))
			{
				LL_WARNS() << "can't read constraint chain length"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}
			constraintp->mChainLength = (S32) byte;

			if((U32)constraintp->mChainLength > mJointMotionList->getNumJointMotions())
			{
				LL_WARNS() << "invalid constraint chain length"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if (!dp.unpackU8(byte, "constraint_type"))
			{
				LL_WARNS() << "can't read constraint type"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}
			
			if( byte >= NUM_CONSTRAINT_TYPES )
			{
				LL_WARNS() << "invalid constraint type"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}
			constraintp->mConstraintType = (EConstraintType)byte;

			const S32 BIN_DATA_LENGTH = 16;
			U8 bin_data[BIN_DATA_LENGTH+1];
			if (!dp.unpackBinaryDataFixed(bin_data, BIN_DATA_LENGTH, "source_volume"))
			{
				LL_WARNS() << "can't read source volume name"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			bin_data[BIN_DATA_LENGTH] = 0; // Ensure null termination
			str = (char*)bin_data;
			constraintp->mSourceConstraintVolume = mCharacter->getCollisionVolumeID(str);
			if (constraintp->mSourceConstraintVolume == -1)
			{
				LL_WARNS() << "not a valid source constraint volume " << str
						   << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if (!dp.unpackVector3(constraintp->mSourceConstraintOffset, "source_offset"))
			{
				LL_WARNS() << "can't read constraint source offset"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}
			
			if( !(constraintp->mSourceConstraintOffset.isFinite()) )
			{
				LL_WARNS() << "non-finite constraint source offset"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}
			
			if (!dp.unpackBinaryDataFixed(bin_data, BIN_DATA_LENGTH, "target_volume"))
			{
				LL_WARNS() << "can't read target volume name"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			bin_data[BIN_DATA_LENGTH] = 0; // Ensure null termination
			str = (char*)bin_data;
			if (str == "GROUND")
			{
				// constrain to ground
				constraintp->mConstraintTargetType = CONSTRAINT_TARGET_TYPE_GROUND;
			}
			else
			{
				constraintp->mConstraintTargetType = CONSTRAINT_TARGET_TYPE_BODY;
				constraintp->mTargetConstraintVolume = mCharacter->getCollisionVolumeID(str);
				if (constraintp->mTargetConstraintVolume == -1)
				{
					LL_WARNS() << "not a valid target constraint volume " << str
							   << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}
			}

			if (!dp.unpackVector3(constraintp->mTargetConstraintOffset, "target_offset"))
			{
				LL_WARNS() << "can't read constraint target offset"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if( !(constraintp->mTargetConstraintOffset.isFinite()) )
			{
				LL_WARNS() << "non-finite constraint target offset"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}
			
			if (!dp.unpackVector3(constraintp->mTargetConstraintDir, "target_dir"))
			{
				LL_WARNS() << "can't read constraint target direction"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if( !(constraintp->mTargetConstraintDir.isFinite()) )
			{
				LL_WARNS() << "non-finite constraint target direction"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if (!constraintp->mTargetConstraintDir.isExactlyZero())
			{
				constraintp->mUseTargetOffset = TRUE;
	//			constraintp->mTargetConstraintDir *= constraintp->mSourceConstraintOffset.magVec();
			}

			if (!dp.unpackF32(constraintp->mEaseInStartTime, "ease_in_start") || !std::isfinite(constraintp->mEaseInStartTime))
			{
				LL_WARNS() << "can't read constraint ease in start time"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if (!dp.unpackF32(constraintp->mEaseInStopTime, "ease_in_stop") || !std::isfinite(constraintp->mEaseInStopTime))
			{
				LL_WARNS() << "can't read constraint ease in stop time"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if (!dp.unpackF32(constraintp->mEaseOutStartTime, "ease_out_start") || !std::isfinite(constraintp->mEaseOutStartTime))
			{
				LL_WARNS() << "can't read constraint ease out start time"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			if (!dp.unpackF32(constraintp->mEaseOutStopTime, "ease_out_stop") || !std::isfinite(constraintp->mEaseOutStopTime))
			{
				LL_WARNS() << "can't read constraint ease out stop time"
                           << " for animation " << asset_id << LL_ENDL;
				return FALSE;
			}

			constraintp->mJointStateIndices = new S32[constraintp->mChainLength + 1]; // note: mChainLength is size-limited - comes from a byte
			
			if (singu_new_joint_motion_list)
			{
				mJointMotionList->mConstraints.push_front(watcher.release());
			}
			
			LLJoint* joint = mCharacter->findCollisionVolume(constraintp->mSourceConstraintVolume);
			// get joint to which this collision volume is attached
			if (!joint)
			{
				return FALSE;
			}
			for (S32 i = 0; i < constraintp->mChainLength + 1; i++)
			{
				LLJoint* parent = joint->getParent();
				if (!parent)
				{
					LL_WARNS() << "Joint with no parent: " << joint->getName()
                               << " Emote: " << mJointMotionList->mEmoteName
                               << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}
				joint = parent;
				constraintp->mJointStateIndices[i] = -1;
				for (U32 j = 0; j < mJointMotionList->getNumJointMotions(); j++)
				{
					LLJoint* constraint_joint = getJoint(j);
					
					if ( !constraint_joint )
					{
						LL_WARNS() << "Invalid joint " << j
                                   << " for animation " << asset_id << LL_ENDL;
						return FALSE;
					}
					
					if(constraint_joint == joint)
					{
						constraintp->mJointStateIndices[i] = (S32)j;
						break;
					}
				}
				if (constraintp->mJointStateIndices[i] < 0 )
				{
					LL_WARNS() << "No joint index for constraint " << i
                               << " for animation " << asset_id << LL_ENDL;
					return FALSE;
				}
			}
		}
	}

	mAssetStatus = ASSET_LOADED;

	setupPose();

	return TRUE;
}

//-----------------------------------------------------------------------------
// serialize()
//-----------------------------------------------------------------------------
BOOL LLKeyframeMotion::serialize(LLDataPacker& dp) const
{
	BOOL success = TRUE;

	LL_DEBUGS("BVH") << "serializing" << LL_ENDL;

	success &= dp.packU16(KEYFRAME_MOTION_VERSION, "version");
	success &= dp.packU16(KEYFRAME_MOTION_SUBVERSION, "sub_version");
	success &= dp.packS32(mJointMotionList->mBasePriority, "base_priority");
	success &= dp.packF32(mJointMotionList->mDuration, "duration");
	success &= dp.packString(mJointMotionList->mEmoteName, "emote_name");
	success &= dp.packF32(mJointMotionList->mLoopInPoint, "loop_in_point");
	success &= dp.packF32(mJointMotionList->mLoopOutPoint, "loop_out_point");
	success &= dp.packS32(mJointMotionList->mLoop, "loop");
	success &= dp.packF32(mJointMotionList->mEaseInDuration, "ease_in_duration");
	success &= dp.packF32(mJointMotionList->mEaseOutDuration, "ease_out_duration");
	success &= dp.packU32(mJointMotionList->mHandPose, "hand_pose");
	success &= dp.packU32(mJointMotionList->getNumJointMotions(), "num_joints");

    LL_DEBUGS("BVH") << "version " << KEYFRAME_MOTION_VERSION << LL_ENDL;
    LL_DEBUGS("BVH") << "sub_version " << KEYFRAME_MOTION_SUBVERSION << LL_ENDL;
    LL_DEBUGS("BVH") << "base_priority " << mJointMotionList->mBasePriority << LL_ENDL;
	LL_DEBUGS("BVH") << "duration " << mJointMotionList->mDuration << LL_ENDL;
	LL_DEBUGS("BVH") << "emote_name " << mJointMotionList->mEmoteName << LL_ENDL;
	LL_DEBUGS("BVH") << "loop_in_point " << mJointMotionList->mLoopInPoint << LL_ENDL;
	LL_DEBUGS("BVH") << "loop_out_point " << mJointMotionList->mLoopOutPoint << LL_ENDL;
	LL_DEBUGS("BVH") << "loop " << mJointMotionList->mLoop << LL_ENDL;
	LL_DEBUGS("BVH") << "ease_in_duration " << mJointMotionList->mEaseInDuration << LL_ENDL;
	LL_DEBUGS("BVH") << "ease_out_duration " << mJointMotionList->mEaseOutDuration << LL_ENDL;
	LL_DEBUGS("BVH") << "hand_pose " << mJointMotionList->mHandPose << LL_ENDL;
	LL_DEBUGS("BVH") << "num_joints " << mJointMotionList->getNumJointMotions() << LL_ENDL;

	for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); i++)
	{
		JointMotion* joint_motionp = mJointMotionList->getJointMotion(i);
		success &= dp.packString(joint_motionp->mJointName, "joint_name");
		success &= dp.packS32(joint_motionp->mPriority, "joint_priority");
		success &= dp.packS32(joint_motionp->mRotationCurve.mNumKeys, "num_rot_keys");

		LL_DEBUGS("BVH") << "Joint " << joint_motionp->mJointName << LL_ENDL;
		for (RotationCurve::key_map_t::iterator iter = joint_motionp->mRotationCurve.mKeys.begin();
			 iter != joint_motionp->mRotationCurve.mKeys.end(); ++iter)
		{
			RotationKey& rot_key = iter->second;
			U16 time_short = F32_to_U16(rot_key.mTime, 0.f, mJointMotionList->mDuration);
			success &= dp.packU16(time_short, "time");

			LLVector3 rot_angles = rot_key.mValue.packToVector3();
			
			U16 x, y, z;
			rot_angles.quantize16(-1.f, 1.f, -1.f, 1.f);
			x = F32_to_U16(rot_angles.mV[VX], -1.f, 1.f);
			y = F32_to_U16(rot_angles.mV[VY], -1.f, 1.f);
			z = F32_to_U16(rot_angles.mV[VZ], -1.f, 1.f);
			success &= dp.packU16(x, "rot_angle_x");
			success &= dp.packU16(y, "rot_angle_y");
			success &= dp.packU16(z, "rot_angle_z");

			LL_DEBUGS("BVH") << "  rot: t " << rot_key.mTime << " angles " << rot_angles.mV[VX] <<","<< rot_angles.mV[VY] <<","<< rot_angles.mV[VZ] << LL_ENDL;
		}

		success &= dp.packS32(joint_motionp->mPositionCurve.mNumKeys, "num_pos_keys");
		for (PositionCurve::key_map_t::iterator iter = joint_motionp->mPositionCurve.mKeys.begin();
			 iter != joint_motionp->mPositionCurve.mKeys.end(); ++iter)
		{
			PositionKey& pos_key = iter->second;
			U16 time_short = F32_to_U16(pos_key.mTime, 0.f, mJointMotionList->mDuration);
			success &= dp.packU16(time_short, "time");

			U16 x, y, z;
			pos_key.mValue.quantize16(-LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			x = F32_to_U16(pos_key.mValue.mV[VX], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			y = F32_to_U16(pos_key.mValue.mV[VY], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			z = F32_to_U16(pos_key.mValue.mV[VZ], -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
			success &= dp.packU16(x, "pos_x");
			success &= dp.packU16(y, "pos_y");
			success &= dp.packU16(z, "pos_z");

			LL_DEBUGS("BVH") << "  pos: t " << pos_key.mTime << " pos " << pos_key.mValue.mV[VX] <<","<< pos_key.mValue.mV[VY] <<","<< pos_key.mValue.mV[VZ] << LL_ENDL;
		}
	}	

	success &= dp.packS32(mJointMotionList->mConstraints.size(), "num_constraints");
    LL_DEBUGS("BVH") << "num_constraints " << mJointMotionList->mConstraints.size() << LL_ENDL;
	for (JointMotionList::constraint_list_t::const_iterator iter = mJointMotionList->mConstraints.begin();
		 iter != mJointMotionList->mConstraints.end(); ++iter)
	{
		JointConstraintSharedData* shared_constraintp = *iter;
		success &= dp.packU8(shared_constraintp->mChainLength, "chain_length");
		success &= dp.packU8(shared_constraintp->mConstraintType, "constraint_type");
		char source_volume[16]; /* Flawfinder: ignore */
		snprintf(source_volume, sizeof(source_volume), "%s",	/* Flawfinder: ignore */
				 mCharacter->findCollisionVolume(shared_constraintp->mSourceConstraintVolume)->getName().c_str()); 
        
		success &= dp.packBinaryDataFixed((U8*)source_volume, 16, "source_volume");
		success &= dp.packVector3(shared_constraintp->mSourceConstraintOffset, "source_offset");
		char target_volume[16];	/* Flawfinder: ignore */
		if (shared_constraintp->mConstraintTargetType == CONSTRAINT_TARGET_TYPE_GROUND)
		{
			snprintf(target_volume,sizeof(target_volume), "%s", "GROUND");	/* Flawfinder: ignore */
		}
		else
		{
			snprintf(target_volume, sizeof(target_volume),"%s", /* Flawfinder: ignore */
					 mCharacter->findCollisionVolume(shared_constraintp->mTargetConstraintVolume)->getName().c_str());	
		}
		success &= dp.packBinaryDataFixed((U8*)target_volume, 16, "target_volume");
		success &= dp.packVector3(shared_constraintp->mTargetConstraintOffset, "target_offset");
		success &= dp.packVector3(shared_constraintp->mTargetConstraintDir, "target_dir");
		success &= dp.packF32(shared_constraintp->mEaseInStartTime, "ease_in_start");
		success &= dp.packF32(shared_constraintp->mEaseInStopTime, "ease_in_stop");
		success &= dp.packF32(shared_constraintp->mEaseOutStartTime, "ease_out_start");
		success &= dp.packF32(shared_constraintp->mEaseOutStopTime, "ease_out_stop");

        LL_DEBUGS("BVH") << "  chain_length " << shared_constraintp->mChainLength << LL_ENDL;
        LL_DEBUGS("BVH") << "  constraint_type " << (S32)shared_constraintp->mConstraintType << LL_ENDL;
        LL_DEBUGS("BVH") << "  source_volume " << source_volume << LL_ENDL;
        LL_DEBUGS("BVH") << "  source_offset " << shared_constraintp->mSourceConstraintOffset << LL_ENDL;
        LL_DEBUGS("BVH") << "  target_volume " << target_volume << LL_ENDL;
        LL_DEBUGS("BVH") << "  target_offset " << shared_constraintp->mTargetConstraintOffset << LL_ENDL;
        LL_DEBUGS("BVH") << "  target_dir " << shared_constraintp->mTargetConstraintDir << LL_ENDL;
        LL_DEBUGS("BVH") << "  ease_in_start " << shared_constraintp->mEaseInStartTime << LL_ENDL;
        LL_DEBUGS("BVH") << "  ease_in_stop " << shared_constraintp->mEaseInStopTime << LL_ENDL;
        LL_DEBUGS("BVH") << "  ease_out_start " << shared_constraintp->mEaseOutStartTime << LL_ENDL;
        LL_DEBUGS("BVH") << "  ease_out_stop " << shared_constraintp->mEaseOutStopTime << LL_ENDL;
	}

	return success;
}

//-----------------------------------------------------------------------------
// getFileSize()
//-----------------------------------------------------------------------------
U32	LLKeyframeMotion::getFileSize()
{
	// serialize into a dummy buffer to calculate required size
	LLDataPackerBinaryBuffer dp;
	serialize(dp);

	return dp.getCurrentSize();
}

//-----------------------------------------------------------------------------
// getPelvisBBox()
//-----------------------------------------------------------------------------
const LLBBoxLocal &LLKeyframeMotion::getPelvisBBox()
{
	return mJointMotionList->mPelvisBBox;
}

//-----------------------------------------------------------------------------
// setPriority()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::setPriority(S32 priority)
{
	if (mJointMotionList) 
	{
		S32 priority_delta = priority - mJointMotionList->mBasePriority;
		mJointMotionList->mBasePriority = (LLJoint::JointPriority)priority;
		mJointMotionList->mMaxPriority = mJointMotionList->mBasePriority;

		for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); i++)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);			
			joint_motion->mPriority = (LLJoint::JointPriority)llclamp(
				(S32)joint_motion->mPriority + priority_delta,
				(S32)LLJoint::LOW_PRIORITY, 
				(S32)LLJoint::HIGHEST_PRIORITY);
			getJointState(i)->setPriority(joint_motion->mPriority);
		}
	}
}

//-----------------------------------------------------------------------------
// setEmote()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::setEmote(const LLUUID& emote_id)
{
	const char* emote_name = gAnimLibrary.animStateToString(emote_id);
	if (emote_name)
	{
		mJointMotionList->mEmoteName = emote_name;
	}
	else
	{
		mJointMotionList->mEmoteName.clear();
	}
}

//-----------------------------------------------------------------------------
// setEaseIn()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::setEaseIn(F32 ease_in)
{
	if (mJointMotionList)
	{
		mJointMotionList->mEaseInDuration = llmax(ease_in, 0.f);
	}
}

//-----------------------------------------------------------------------------
// setEaseOut()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::setEaseOut(F32 ease_in)
{
	if (mJointMotionList)
	{
		mJointMotionList->mEaseOutDuration = llmax(ease_in, 0.f);
	}
}


//-----------------------------------------------------------------------------
// flushKeyframeCache()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::flushKeyframeCache()
{
	// TODO: Make this safe to do
// 	LLKeyframeDataCache::clear();
}

//-----------------------------------------------------------------------------
// setLoop()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::setLoop(BOOL loop)
{
	if (mJointMotionList) 
	{
		mJointMotionList->mLoop = loop; 
		mSendStopTimestamp = F32_MAX;
	}
}


//-----------------------------------------------------------------------------
// setLoopIn()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::setLoopIn(F32 in_point)
{
	if (mJointMotionList)
	{
		mJointMotionList->mLoopInPoint = in_point; 
		
		// set up loop keys
		for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); i++)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
			
			PositionCurve* pos_curve = &joint_motion->mPositionCurve;
			RotationCurve* rot_curve = &joint_motion->mRotationCurve;
			ScaleCurve* scale_curve = &joint_motion->mScaleCurve;
			
			pos_curve->mLoopInKey.mTime = mJointMotionList->mLoopInPoint;
			rot_curve->mLoopInKey.mTime = mJointMotionList->mLoopInPoint;
			scale_curve->mLoopInKey.mTime = mJointMotionList->mLoopInPoint;

			pos_curve->mLoopInKey.mValue = pos_curve->getValue(mJointMotionList->mLoopInPoint, mJointMotionList->mDuration);
			rot_curve->mLoopInKey.mValue = rot_curve->getValue(mJointMotionList->mLoopInPoint, mJointMotionList->mDuration);
			scale_curve->mLoopInKey.mValue = scale_curve->getValue(mJointMotionList->mLoopInPoint, mJointMotionList->mDuration);
		}
	}
}

//-----------------------------------------------------------------------------
// setLoopOut()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::setLoopOut(F32 out_point)
{
	if (mJointMotionList)
	{
		mJointMotionList->mLoopOutPoint = out_point; 
		
		// set up loop keys
		for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); i++)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
			
			PositionCurve* pos_curve = &joint_motion->mPositionCurve;
			RotationCurve* rot_curve = &joint_motion->mRotationCurve;
			ScaleCurve* scale_curve = &joint_motion->mScaleCurve;
			
			pos_curve->mLoopOutKey.mTime = mJointMotionList->mLoopOutPoint;
			rot_curve->mLoopOutKey.mTime = mJointMotionList->mLoopOutPoint;
			scale_curve->mLoopOutKey.mTime = mJointMotionList->mLoopOutPoint;

			pos_curve->mLoopOutKey.mValue = pos_curve->getValue(mJointMotionList->mLoopOutPoint, mJointMotionList->mDuration);
			rot_curve->mLoopOutKey.mValue = rot_curve->getValue(mJointMotionList->mLoopOutPoint, mJointMotionList->mDuration);
			scale_curve->mLoopOutKey.mValue = scale_curve->getValue(mJointMotionList->mLoopOutPoint, mJointMotionList->mDuration);
		}
	}
}

//-----------------------------------------------------------------------------
// onLoadComplete()
//-----------------------------------------------------------------------------
void LLKeyframeMotion::onLoadComplete(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLUUID* id = (LLUUID*)user_data;
		
	std::vector<LLCharacter* >::iterator char_iter = LLCharacter::sInstances.begin();

	while(char_iter != LLCharacter::sInstances.end() &&
			(*char_iter)->getID() != *id)
	{
		++char_iter;
	}
	
	delete id;

	if (char_iter == LLCharacter::sInstances.end())
	{
		return;
	}

	LLCharacter* character = *char_iter;

	// look for an existing instance of this motion
	LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*> (character->findMotion(asset_uuid));
	if (motionp)
	{
		if (0 == status)
		{
			if (motionp->mAssetStatus == ASSET_LOADED)
			{
				// asset already loaded
				return;
			}
			LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
			S32 size = file.getSize();
			
			U8* buffer = new U8[size];
			file.read((U8*)buffer, size);	/*Flawfinder: ignore*/

			LL_DEBUGS("Animation") << "Loading keyframe data for: " << motionp->getName() << ":" << motionp->getID() << " (" << size << " bytes)" << LL_ENDL;
			
			LLDataPackerBinaryBuffer dp(buffer, size);
			if (motionp->deserialize(dp, asset_uuid))
			{
				motionp->mAssetStatus = ASSET_LOADED;
			}
			else
			{
				LL_WARNS() << "Failed to decode asset for animation " << motionp->getName() << ":" << motionp->getID() << LL_ENDL;
				motionp->mAssetStatus = ASSET_FETCH_FAILED;
			}
			
			delete[] buffer;
		}
		else
		{
			LL_WARNS() << "Failed to load asset for animation " << motionp->getName() << ":" << motionp->getID() << LL_ENDL;
			motionp->mAssetStatus = ASSET_FETCH_FAILED;
		}
	}
	else
	{
		LL_WARNS() << "No existing motion for asset data. UUID: " << asset_uuid << LL_ENDL;
	}
}

//--------------------------------------------------------------------
// LLKeyframeDataCache::dumpDiagInfo()
//--------------------------------------------------------------------
// <singu>
// quiet = 0 : print everything in detail.
//         1 : print the UUIDs of all animations in the cache and the total memory usage.
//         2 : only print the total memory usage.
// </singu>
void LLKeyframeDataCache::dumpDiagInfo(int quiet)
{
	// keep track of totals
	U32 total_size = 0;

	char buf[1024];		/* Flawfinder: ignore */

	if (quiet < 2)
	{
		LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;
		LL_INFOS() << "       Global Motion Table (DEBUG only)" << LL_ENDL;		
		LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;
	}

	// print each loaded mesh, and it's memory usage
	for (keyframe_data_map_t::iterator map_it = sKeyframeDataMap.begin();
		 map_it != sKeyframeDataMap.end(); ++map_it)
	{
		U32 joint_motion_kb;

		LLKeyframeMotion::JointMotionList const* motion_list_p = map_it->get();

		if (quiet < 2)
		{
			LL_INFOS() << "Motion: " << map_it->key() << LL_ENDL;
		}

		if (motion_list_p)
		{
			joint_motion_kb = motion_list_p->dumpDiagInfo(quiet);
			total_size += joint_motion_kb;
		}
	}

	if (quiet < 2)
	{
		LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;
	}
	LL_INFOS() << "Motions\tTotal Size" << LL_ENDL;
	snprintf(buf, sizeof(buf), "%d\t\t%d bytes", (S32)sKeyframeDataMap.size(), total_size );		/* Flawfinder: ignore */
	LL_INFOS() << buf << LL_ENDL;
	if (quiet < 2)
	{
		LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;
	}
}


//--------------------------------------------------------------------
// LLKeyframeDataCache::createKeyframeData()
//--------------------------------------------------------------------
//<singu> This function replaces LLKeyframeDataCache::addKeyframeData and was rewritten to fix a memory leak (aka, the usage of AICachedPointer).
LLKeyframeMotion::JointMotionListPtr LLKeyframeDataCache::createKeyframeData(LLUUID const& id)
{
	std::pair<keyframe_data_map_t::iterator, bool> result =
		sKeyframeDataMap.insert(AICachedPointer<LLUUID, LLKeyframeMotion::JointMotionList>(id, new LLKeyframeMotion::JointMotionList, &sKeyframeDataMap));
	llassert(result.second);	// id may not already exist in the cache.
	return &*result.first;		// Construct and return a JointMotionListPt from a pointer to the actually inserted AICachedPointer.
}
//</singu>

//--------------------------------------------------------------------
// LLKeyframeDataCache::removeKeyframeData()
//--------------------------------------------------------------------
void LLKeyframeDataCache::removeKeyframeData(const LLUUID& id)
{
	keyframe_data_map_t::iterator found_data = sKeyframeDataMap.find(id);
	if (found_data != sKeyframeDataMap.end())
	{
		sKeyframeDataMap.erase(found_data);
	}
}

//--------------------------------------------------------------------
// LLKeyframeDataCache::getKeyframeData()
//--------------------------------------------------------------------
LLKeyframeMotion::JointMotionListPtr LLKeyframeDataCache::getKeyframeData(const LLUUID& id)
{
	keyframe_data_map_t::iterator found_data = sKeyframeDataMap.find(id);
	if (found_data == sKeyframeDataMap.end())
	{
		return NULL;
	}
	return &*found_data;			// Construct and return a JointMotionListPt from a pointer to the found AICachedPointer.
}

//--------------------------------------------------------------------
// ~LLKeyframeDataCache::LLKeyframeDataCache()
//--------------------------------------------------------------------
LLKeyframeDataCache::~LLKeyframeDataCache()
{
	clear();
}

//-----------------------------------------------------------------------------
// clear()
//-----------------------------------------------------------------------------
void LLKeyframeDataCache::clear()
{
	sKeyframeDataMap.clear();
}

//-----------------------------------------------------------------------------
// JointConstraint()
//-----------------------------------------------------------------------------
LLKeyframeMotion::JointConstraint::JointConstraint(JointConstraintSharedData* shared_data) : mSharedData(shared_data) 
{
	mWeight = 0.f;
	mTotalLength = 0.f;
	mActive = FALSE;
	mSourceVolume = NULL;
	mTargetVolume = NULL;
	mFixupDistanceRMS = 0.f;

	int i;
	for (i=0; i<MAX_CHAIN_LENGTH; ++i)
	{
		mJointLengths[i] = 0.f;
		mJointLengthFractions[i] = 0.f;
	}
}

//-----------------------------------------------------------------------------
// ~JointConstraint()
//-----------------------------------------------------------------------------
LLKeyframeMotion::JointConstraint::~JointConstraint()
{
}

// End
