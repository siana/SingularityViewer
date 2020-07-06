/** 
 * @file llmotion.h
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

#ifndef LL_LLMOTION_H
#define LL_LLMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include <string>

#include "aisyncclient.h"
#include "llerror.h"
#include "llpose.h"
#include "lluuid.h"

class LLCharacter;
class LLMotionController;

//-----------------------------------------------------------------------------
// class AISync* stuff
//-----------------------------------------------------------------------------

class AISyncKeyMotion : public AISyncKey
{
  private:
	F32 mDuration;
	bool mLoop;

  public:
	AISyncKeyMotion(AISyncKey const* from_key, F32 duration, bool loop) : AISyncKey(from_key), mDuration(duration), mLoop(loop) { }

  // Virtual functions of AISyncKey.
  public:
	/*virtual*/ synckeytype_t getkeytype(void) const
	{
	  // Return a unique identifier for this class, where the low 8 bits represent the syncgroup.
	  return synckeytype_motion;
	}

    /*virtual*/ bool equals(AISyncKey const& key) const
	{
	  switch (key.getkeytype())
	  {
		case synckeytype_motion:
		{
		  // The other key is of the same type.
		  AISyncKeyMotion const& motion_key = static_cast<AISyncKeyMotion const&>(key);
		  return mLoop == motion_key.mLoop && is_approx_equal(mDuration, motion_key.mDuration);
		}
		default:
		  // The keys must be in the same syncgroup.
		  break;
	  }
	  return false;
	}
};

class AISyncClientMotion : public AISyncClient
{
  protected:
	// Make sure that clients that are destroyed are first unregistered.
	// This is needed, for example, when loading fails or when excess motions are being purged.
	/*virtual*/ ~AISyncClientMotion() { unregister_client(); }

	// AISyncClient events.
	/*virtual*/ AISyncKey* createSyncKey(AISyncKey const* from_key = NULL) const;
	/*virtual*/ void event1_ready(void) { }
	/*virtual*/ void event1_not_ready(void) { }
	/*virtual*/ void deregistered(void);

  protected:
	// This is called when the server sent us a message that it wants us to play this animation, but we can't because it isn't fully downloaded yet.
	void aisync_loading(void);
	// This is called when that motion is successfully loaded and it has to be re-registered because now the duration etc is known.
	void aisync_loaded(void);

  public:
	// Virtual functions of AISyncClientMotion.
	// These are defined by classes derived from LLMotion (which is derived from this class).
	virtual BOOL getLoop() = 0;
	virtual F32 getDuration() = 0;
	virtual F32 getAnimTime(void) const  = 0;
	virtual F32 getRuntime(void) const  = 0;
};

//-----------------------------------------------------------------------------
// class LLMotion
//-----------------------------------------------------------------------------
class LLMotion : public AISyncClientMotion
{
	friend class LLMotionController;
	
public:
	enum LLMotionBlendType
	{
		NORMAL_BLEND,
		ADDITIVE_BLEND
	};

	enum LLMotionInitStatus
	{
		STATUS_FAILURE,
		STATUS_SUCCESS,
		STATUS_HOLD
	};

	// Constructor
	LLMotion(LLUUID const& id, LLMotionController* controller);

	// Destructor
	virtual ~LLMotion();

public:
	//-------------------------------------------------------------------------
	// functions to support MotionController and MotionRegistry
	//-------------------------------------------------------------------------

	// get the name of this instance
	const std::string &getName() const { return mName; }

	// set the name of this instance
	void setName(const std::string &name) { mName = name; }

	const LLUUID& getID() const { return mID; }

	// returns the pose associated with the current state of this motion
	virtual LLPose* getPose() { return &mPose;}

	void fadeOut();

	void fadeIn();

	F32 getFadeWeight() const { return mFadeWeight; }

	F32 getStopTime() const { return mStopTimestamp; }

	virtual void setStopTime(F32 time);

	BOOL isStopped() const { return mStopped; }

	void setStopped(BOOL stopped) { mStopped = stopped; }

	BOOL isBlending();

	// Activation functions.
	// It is OK for other classes to activate a motion,
	// but only the controller can deactivate it.
	// Thus, if mActive == TRUE, the motion *may* be on the controllers active list,
	// but if mActive == FALSE, the motion is gauranteed not to be on the active list.
protected:
	// Used by LLMotionController only
	void deactivate();
	BOOL isActive() { return mActive; }
public:
	void activate(F32 time);

	//<singu>
	// Returns the time that this motion has been running.
	virtual F32 getRuntime(void) const;

	// Return the current time (in seconds since creation of the controller).
	virtual F32 getAnimTime(void) const;

	// This is called when a motion is to be activated, but might need synchronization.
	// It adjusts the start time to match that of other motions in the same synchronization group that were already started.
	F32 syncActivationTime(F32 time);

	// Accessor.
	LLMotionController* getController(void) const { return mController; }
	//</singu>

public:
	//-------------------------------------------------------------------------
	// animation callbacks to be implemented by subclasses
	//-------------------------------------------------------------------------

	// motions must specify whether or not they loop
	virtual BOOL getLoop() = 0;

	// motions must report their total duration
	virtual F32 getDuration() = 0;

	// motions must report their "ease in" duration
	virtual F32 getEaseInDuration() = 0;

	// motions must report their "ease out" duration.
	virtual F32 getEaseOutDuration() = 0;

	// motions must report their priority level
	virtual LLJoint::JointPriority getPriority() = 0;

	// motions must report their blend type
	virtual LLMotionBlendType getBlendType() = 0;

	// called to determine when a motion should be activated/deactivated based on avatar pixel coverage
	virtual F32 getMinPixelArea() = 0;

	// run-time (post constructor) initialization,
	// called after parameters have been set
	// must return true to indicate success and be available for activation
	virtual LLMotionInitStatus onInitialize(LLCharacter *character) = 0;

	// called per time step
	// must return TRUE while it is active, and
	// must return FALSE when the motion is completed.
	virtual BOOL onUpdate(F32 activeTime, U8* joint_mask) = 0;

	// called when a motion is deactivated
	virtual void onDeactivate() = 0;

	// can we crossfade this motion with a new instance when restarted?
	// should ultimately always be TRUE, but lack of emote blending, etc
	// requires this
	virtual BOOL canDeprecate();

	// optional callback routine called when animation deactivated.
	void	setDeactivateCallback( void (*cb)(void *), void* userdata );

protected:
	// called when a motion is activated
	// must return TRUE to indicate success, or else
	// it will be deactivated
	virtual BOOL onActivate() = 0;

	void addJointState(const LLPointer<LLJointState>& jointState);

protected:
	LLPose		mPose;
	BOOL		mStopped;		// motion has been stopped;
	BOOL		mActive;		// motion is on active list (can be stopped or not stopped)

	//-------------------------------------------------------------------------
	// these are set implicitly by the motion controller and
	// may be referenced (read only) in the above handlers.
	//-------------------------------------------------------------------------
	std::string		mName;			// instance name assigned by motion controller
	LLUUID			mID;
	//<singu>
	LLMotionController* mController;
	//</singu>
	
	F32 mActivationTimestamp;	// time when motion was activated
	F32 mStopTimestamp;			// time when motion was told to stop
	F32 mSendStopTimestamp;		// time when simulator should be told to stop this motion
	F32 mResidualWeight;		// blend weight at beginning of stop motion phase
	F32 mFadeWeight;			// for fading in and out based on LOD
	U8	mJointSignature[3][LL_CHARACTER_MAX_ANIMATED_JOINTS];	// signature of which joints are animated at what priority
	void (*mDeactivateCallback)(void* data);
	void* mDeactivateCallbackUserData;
};


//-----------------------------------------------------------------------------
// LLTestMotion
//-----------------------------------------------------------------------------
class LLTestMotion : public LLMotion
{
public:
	LLTestMotion(LLUUID const& id, LLMotionController* controller) : LLMotion(id, controller){}
	~LLTestMotion() {}
	static LLMotion* create(LLUUID const& id, LLMotionController* controller) { return new LLTestMotion(id, controller); }
	BOOL getLoop() { return FALSE; }
	F32 getDuration() { return 0.0f; }
	F32 getEaseInDuration() { return 0.0f; }
	F32 getEaseOutDuration() { return 0.0f; }
	LLJoint::JointPriority getPriority() { return LLJoint::HIGH_PRIORITY; }
	LLMotionBlendType getBlendType() { return NORMAL_BLEND; }
	F32 getMinPixelArea() { return 0.f; }
	
	LLMotionInitStatus onInitialize(LLCharacter*) { LL_INFOS() << "LLTestMotion::onInitialize()" << LL_ENDL; return STATUS_SUCCESS; }
	BOOL onActivate() { LL_INFOS() << "LLTestMotion::onActivate()" << LL_ENDL; return TRUE; }
	BOOL onUpdate(F32 time, U8* joint_mask) { LL_INFOS() << "LLTestMotion::onUpdate(" << time << ")" << LL_ENDL; return TRUE; }
	void onDeactivate() { LL_INFOS() << "LLTestMotion::onDeactivate()" << LL_ENDL; }
};


//-----------------------------------------------------------------------------
// LLNullMotion
//-----------------------------------------------------------------------------
class LLNullMotion : public LLMotion
{
public:
	LLNullMotion(LLUUID const& id, LLMotionController* controller) : LLMotion(id, controller) {}
	~LLNullMotion() {}
	static LLMotion* create(LLUUID const& id, LLMotionController* controller) { return new LLNullMotion(id, controller); }

	// motions must specify whether or not they loop
	/*virtual*/ BOOL getLoop() { return TRUE; }

	// motions must report their total duration
	/*virtual*/ F32 getDuration() { return 1.f; }

	// motions must report their "ease in" duration
	/*virtual*/ F32 getEaseInDuration() { return 0.f; }

	// motions must report their "ease out" duration.
	/*virtual*/ F32 getEaseOutDuration() { return 0.f; }

	// motions must report their priority level
	/*virtual*/ LLJoint::JointPriority getPriority() { return LLJoint::HIGH_PRIORITY; }

	// motions must report their blend type
	/*virtual*/ LLMotionBlendType getBlendType() { return NORMAL_BLEND; }

	// called to determine when a motion should be activated/deactivated based on avatar pixel coverage
	/*virtual*/ F32 getMinPixelArea() { return 0.f; }

	// run-time (post constructor) initialization,
	// called after parameters have been set
	// must return true to indicate success and be available for activation
	/*virtual*/ LLMotionInitStatus onInitialize(LLCharacter *character) { return STATUS_SUCCESS; }

	// called when a motion is activated
	// must return TRUE to indicate success, or else
	// it will be deactivated
	/*virtual*/ BOOL onActivate() { return TRUE; }

	// called per time step
	// must return TRUE while it is active, and
	// must return FALSE when the motion is completed.
	/*virtual*/ BOOL onUpdate(F32 activeTime, U8* joint_mask) { return TRUE; }

	// called when a motion is deactivated
	/*virtual*/ void onDeactivate() {}
};


//-----------------------------------------------------------------------------
// AIMaskedMotion
//-----------------------------------------------------------------------------

// These motions have a bit assigned in LLMotionController::mActiveMask
// that is set and uset upon activation/deactivation.

// This must be in the same order as ANIM_AGENT_BODY_NOISE_ID through ANIM_AGENT_WALK_ADJUST_ID in llvoavatar.cpp.
U32 const ANIM_AGENT_BODY_NOISE		= 0x001;
U32 const ANIM_AGENT_BREATHE_ROT	= 0x002;
U32 const ANIM_AGENT_PHYSICS_MOTION	= 0x004;
U32 const ANIM_AGENT_EDITING		= 0x008;
U32 const ANIM_AGENT_EYE			= 0x010;
U32 const ANIM_AGENT_FLY_ADJUST		= 0x020;
U32 const ANIM_AGENT_HAND_MOTION	= 0x040;
U32 const ANIM_AGENT_HEAD_ROT		= 0x080;
U32 const ANIM_AGENT_PELVIS_FIX		= 0x100;
U32 const ANIM_AGENT_TARGET			= 0x200;
U32 const ANIM_AGENT_WALK_ADJUST	= 0x400;

class AIMaskedMotion : public LLMotion
{
private:
	U32 mMaskBit;

public:
	AIMaskedMotion(LLUUID const& id, LLMotionController* controller, U32 mask_bit) : LLMotion(id, controller), mMaskBit(mask_bit) { }

	/*virtual*/ BOOL onActivate();
	/*virtual*/ void onDeactivate();

	U32 getMaskBit(void) const { return mMaskBit; }
};

#endif // LL_LLMOTION_H

