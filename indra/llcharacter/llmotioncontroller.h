/** 
 * @file llmotioncontroller.h
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

#ifndef LL_LLMOTIONCONTROLLER_H
#define LL_LLMOTIONCONTROLLER_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include <string>
#include <map>
#include <deque>

#include "llmotion.h"
#include "llpose.h"
#include "llframetimer.h"
#include "llstatemachine.h"
#include "llstring.h"

//-----------------------------------------------------------------------------
// Class predeclaration
// This is necessary because llcharacter.h includes this file.
//-----------------------------------------------------------------------------
class LLCharacter;
class LLMotionController;
class LLPauseRequestHandle;
typedef LLPointer<LLPauseRequestHandle> LLAnimPauseRequest;

//-----------------------------------------------------------------------------
// LLMotionRegistry
//-----------------------------------------------------------------------------
typedef LLMotion* (*LLMotionConstructor)(LLUUID const& id, LLMotionController*);

class LLMotionRegistry
{
public:
	// Constructor
	LLMotionRegistry();

	// Destructor
	~LLMotionRegistry();

	// adds motion classes to the registry
	// returns true if successfull
	BOOL registerMotion( const LLUUID& id, LLMotionConstructor create);

	// creates a new instance of a named motion
	// returns NULL motion is not registered
	LLMotion* createMotion(LLUUID const& id, LLMotionController* controller);

	// initialization of motion failed, don't try to create this motion again
	void markBad( const LLUUID& id );


protected:
	typedef std::map<LLUUID, LLMotionConstructor> motion_map_t;
	motion_map_t mMotionTable;
};

//-----------------------------------------------------------------------------
// class LLMotionController
//-----------------------------------------------------------------------------
class LLMotionController
{
public:
	typedef std::list<LLMotion*> motion_list_t;
	typedef std::set<LLMotion*> motion_set_t;
	BOOL mIsSelf;
	
public:
	// Constructor
	LLMotionController();

	// Destructor
	virtual ~LLMotionController();

	// set associated character
	// this must be called exactly once by the containing character class.
	// this is generally done in the Character constructor
	void setCharacter( LLCharacter *character );

	// registers a motion with the controller
	// (actually just forwards call to motion registry)
	// returns true if successfull
	BOOL registerMotion( const LLUUID& id, LLMotionConstructor create );

	// creates a motion from the registry
	LLMotion *createMotion( const LLUUID &id );

	// unregisters a motion with the controller
	// (actually just forwards call to motion registry)
	void removeMotion( const LLUUID& id );

	// start motion
	// begins playing the specified motion
	// returns true if successful
	BOOL startMotion( const LLUUID &id, F32 start_offset );

	// stop motion
	// stops a playing motion
	// in reality, it begins the ease out transition phase
	// returns true if successful
	BOOL stopMotionLocally( const LLUUID &id, BOOL stop_immediate );

	// Move motions from loading to loaded
	void updateLoadingMotions();
	
	// update motions
	// invokes the update handlers for each active motion
	// activates sequenced motions
	// deactivates terminated motions`
	void updateMotions(bool force_update = false);

	// minimal update (e.g. while hidden)
	void updateMotionsMinimal();

	void clearBlenders() { mPoseBlender.clearBlenders(); }

	// flush motions
	// releases all motion instances
	void flushAllMotions();

	//Flush is a liar.
	void deactivateAllMotions();	

	//<singu>
	void activated(U32 bit) { mActiveMask |= bit; }
	void deactivated(U32 bit) { mActiveMask &= ~bit; }
	bool isactive(U32 bit) const { return (mActiveMask & bit) != 0; }
	//</singu>

	// pause and continue all motions
	void pauseAllMotions();
	void unpauseAllMotions();
	BOOL isPaused() const { return mPaused; }
	U64 getPausedFrame() const { return mPausedFrame; }
	//<singu>
	void requestPause(std::vector<LLAnimPauseRequest>& avatar_pause_handles);
	void pauseAllSyncedCharacters(std::vector<LLAnimPauseRequest>& avatar_pause_handles);
	//</singu>

	void setTimeStep(F32 step);
    F32 getTimeStep() const { return mTimeStep; }

	void setTimeFactor(F32 time_factor);
	F32 getTimeFactor() const { return mTimeFactor; }

    F32 getAnimTime() const { return mAnimTime; }
    
	motion_list_t& getActiveMotions() { return mActiveMotions; }

	void incMotionCounts(S32& num_motions, S32& num_loading_motions, S32& num_loaded_motions, S32& num_active_motions, S32& num_deprecated_motions);
	
//protected:
	bool isMotionActive( LLMotion *motion );
	bool isMotionLoading( LLMotion *motion );
	LLMotion *findMotion( const LLUUID& id ) const;

	void dumpMotions();

	const LLFrameTimer& getFrameTimer() { return mTimer; }

	static F32	getCurrentTimeFactor()				{ return sCurrentTimeFactor;	};
	static void setCurrentTimeFactor(F32 factor)	{ sCurrentTimeFactor = factor;	};

protected:
	// internal operations act on motion instances directly
	// as there can be duplicate motions per id during blending overlap
	void deleteAllMotions();
	// singu: LLMotion needs access to activateMotionInstance.
public:
	BOOL activateMotionInstance(LLMotion *motion, F32 time);
protected:
	BOOL deactivateMotionInstance(LLMotion *motion);
	void deprecateMotionInstance(LLMotion* motion);
	BOOL stopMotionInstance(LLMotion *motion, BOOL stop_imemdiate);
	void removeMotionInstance(LLMotion* motion);
	void updateRegularMotions();
	void updateAdditiveMotions();
	void resetJointSignatures();
	void updateMotionsByType(LLMotion::LLMotionBlendType motion_type);
	void updateIdleMotion(LLMotion* motionp);
	void updateIdleActiveMotions();
	void purgeExcessMotions();
	void deactivateStoppedMotions();

protected:
	F32					mTimeFactor;			// 1.f for normal speed
	static F32			sCurrentTimeFactor;		// Value to use for initialization
	static LLMotionRegistry	sRegistry;
	LLPoseBlender		mPoseBlender;

	LLCharacter			*mCharacter;

//	Life cycle of an animation:
//
//	Animations are instantiated and immediately put in the mAllMotions map for their entire lifetime.
//	Singu note: that is not true, they are moved to mDeprecatedMotions (often) for the last part of their lifetime.
//	If the animations depend on any asset data, the appropriate data is fetched from the data server,
//	and the animation is put on the mLoadingMotions list.
//	Once an animations is loaded, it will be initialized and put on the mLoadedMotions list.
//	Any animation that is currently playing also sits in the mActiveMotions list.
//	Singu note: animations are only put in mDeprecatedMotions if and while they are playing,
//	therefore animations in mDeprecatedMotions will be (must be) active and in mActiveMotions.

	typedef std::map<LLUUID, LLMotion*> motion_map_t;
	motion_map_t	mAllMotions;

	motion_set_t		mLoadingMotions;
	motion_set_t		mLoadedMotions;
	motion_list_t		mActiveMotions;
	motion_set_t		mDeprecatedMotions;

	//<singu>
	U32					mActiveMask;
	int					mDisableSyncing;			// Set while LLMotion::onActivate (and onDeactivate) are called for this controller.
	bool				mHidden;					// The value of the last call to hidden().
	bool				mHaveVisibleSyncedMotions;	// Set when we are synchronized with one or more motions of a controller that is not hidden.
	//</singu>
	LLFrameTimer		mTimer;
	F32					mPrevTimerElapsed;
	F32					mAnimTime;
	F32					mLastTime;
	BOOL				mHasRunOnce;
	BOOL				mPaused;
	U64					mPausedFrame;
	F32					mTimeStep;
	S32					mTimeStepCount;
	F32					mLastInterp;

	U8					mJointSignature[2][LL_CHARACTER_MAX_ANIMATED_JOINTS];

	//<singu>
public:
	// Internal administration for AISync.
	void disable_syncing(void) { mDisableSyncing += 100; }
	void enable_syncing(void) { mDisableSyncing -= 100; }
	bool syncing_disabled(void) const { return mDisableSyncing >= 100; }

	// Accessors needed for synchronization.
	bool isHidden(void) const { return mHidden; }

	// Called often. Should return false if we still need to keep updating our motions even if we're not visible.
	bool hidden(bool not_visible) { if (mHidden != not_visible) toggle_hidden(); return !mHaveVisibleSyncedMotions; }

private:
	void toggle_hidden(void);
	void refresh_hidden(void);
	void setHaveVisibleSyncedMotions(void) { mHaveVisibleSyncedMotions = true; }
	//</singu>
};

//-----------------------------------------------------------------------------
// Class declaractions
//-----------------------------------------------------------------------------
#include "llcharacter.h"

#endif // LL_LLMOTIONCONTROLLER_H

