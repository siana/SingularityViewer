/** 
 * @file llframetimer.h
 * @brief A lightweight timer that measures seconds and is only
 * updated once per frame.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#ifndef LL_LLFRAMETIMER_H
#define LL_LLFRAMETIMER_H

/**
 * *NOTE: Because of limitations on linux which we do not really have
 * time to explore, the total time is derived from the frame time
 * and is recsynchronized on every frame.
 */

#include "lltimer.h"
#include "timing.h"
#include <apr_thread_mutex.h>
#ifdef SHOW_ASSERT
#include "aithreadid.h"			// is_main_thread()
#endif

class LL_COMMON_API LLFrameTimer
{
public:
	// Create an LLFrameTimer and start it. After creation it is running and in the state expired (hasExpired will return true).
	LLFrameTimer(void) : mExpiry(0), mRunning(true), mPaused(false) { if (!sGlobalMutex) global_initialization(); setAge(0.0); }

	// Atomic reads of static variables.

	// Return the number of seconds since the start of the application.
	static F64 getElapsedSeconds(void)
	{
		// Loses msec precision after ~4.5 hours...
		apr_thread_mutex_lock(sGlobalMutex);
		F64 res = sFrameTime;
		apr_thread_mutex_unlock(sGlobalMutex);
		return res;
	} 

	// Return a low precision usec since epoch.
	static U64 getTotalTime(void)
	{
		// sTotalTime is only accessed by the main thread, so no locking is necessary.
		llassert(is_main_thread());
		//apr_thread_mutex_lock(sGlobalMutex);
		U64 res = sTotalTime;
		//apr_thread_mutex_unlock(sGlobalMutex);
		llassert(res);
		return res;
	}

	// Return a low precision seconds since epoch.
	static F64 getTotalSeconds(void)
	{
		// sTotalSeconds is only accessed by the main thread, so no locking is necessary.
		llassert(is_main_thread());
		//apr_thread_mutex_lock(sGlobalMutex);
		F64 res = sTotalSeconds;
		//apr_thread_mutex_unlock(sGlobalMutex);
		return res;
	}

	// Return current frame number (the number of frames since application start).
	static U32 getFrameCount(void)
	{
		// sFrameCount is only accessed by the main thread, so no locking is necessary.
		llassert(is_main_thread());
		//apr_thread_mutex_lock(sGlobalMutex);
		U32 res = sFrameCount;
		//apr_thread_mutex_unlock(sGlobalMutex);
		return res;
	}

	// Call this method once per frame to update the current frame time.
	// This is actually called at some other times as well.
	static void updateFrameTime(void);

	// Call this method once, and only once, per frame to update the current frame count and sFrameDeltaTime.
	static void updateFrameTimeAndCount(void);

	// Return duration of last frame in seconds.
	static F32 getFrameDeltaTimeF32(void);

	// Return seconds since the current frame started
	static F32 getCurrentFrameTime(void);

	// MANIPULATORS

	void reset(F32 expiration = 0.f)				// Same as start() but leaves mRunning off when called after stop().
	{
		llassert(!mPaused);
		mStartTime = getElapsedSeconds();
		mExpiry = mStartTime + expiration;
	}

	void start(F32 expiration = 0.f)				// Reset and (re)start with expiration.
	{
		reset(expiration);
		mRunning = true;					// Start, if not already started.
	}

	void stop(void)									// Stop running.
	{
		llassert(!mPaused);
		mRunning = false;
	}

	void resetWithExpiry(F32 expiration)			{ reset(); setTimerExpirySec(expiration); }
	void pause();									// Mark elapsed time so far.
	void unpause();									// Move 'start' time in order to decrement time between pause and unpause from ElapsedTime.

	void setTimerExpirySec(F32 expiration)			{ llassert(!mPaused); mExpiry = mStartTime + expiration; }

	void setExpiryAt(F64 seconds_since_epoch);
	bool checkExpirationAndReset(F32 expiration);	// Returns true when expired. Only resets if expired.
	F32 getElapsedTimeAndResetF32(void);
	void setAge(const F64 age)						{ llassert(!mPaused); mStartTime = getElapsedSeconds() - age; }

	// ACCESSORS
	bool hasExpired() const							{ return getElapsedSeconds() >= mExpiry; }
	F32  getElapsedTimeF32() const					{ llassert(mRunning); return mPaused ? (F32)mStartTime : (F32)(getElapsedSeconds() - mStartTime); }
	bool getStarted() const							{ return mRunning; }

	// return the seconds since epoch when this timer will expire.
	F64 expiresAt() const;

public:
	// Do one-time initialization of the static members.
	static void global_initialization(void);

protected:
	//
	// Application constants
	//

	// Application start in microseconds since epoch.
	static U64 const sStartTotalTime;	

	// 
	// Global data.
	//

	// More than one thread are accessing (some of) these variables, therefore we need locking.
	static apr_thread_mutex_t* sGlobalMutex;

	// Current time in seconds since application start, updated together with sTotalTime.
	static F64 sFrameTime;

	// Microseconds between last two calls to updateFrameTimeAndCount (time between last two frames).
	static U64 sFrameDeltaTime;

	// Current time in microseconds since epoch, updated at least once per frame.
	static U64 sTotalTime;			

	// Previous (frame) time in microseconds since epoch, updated once per frame.
	static U64 sPrevTotalTime;

	// Current time in seconds since epoch, updated together with sTotalTime.
	static F64 sTotalSeconds;

	// Current frame number (number of frames since application start).
	static S32 sFrameCount;

	//
	// Member data
	//

	// When not paused (mPaused is false): number of seconds since application start,
	// otherwise this value is equal to the accumulated run time (ElapsedTime).
	// Set equal to sFrameTime when reset.
	F64 mStartTime;

	// Timer expires when sFrameTime reaches this value (in seconds since application start).
	F64 mExpiry;

	// True when running, merely a boolean return by getStarted(). The timer always runs.
	bool mRunning;

	// True when accumulating ElapsedTime. If false mStartTime has a different meaning
	// and really unpause() should be called before anything else.
	bool mPaused;
};

// Glue code for Havok (or anything else that doesn't want the full .h files)
extern F32 getCurrentFrameTime();

#endif  // LL_LLFRAMETIMER_H
