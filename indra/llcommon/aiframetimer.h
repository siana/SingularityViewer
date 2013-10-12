/**
 * @file aiframetimer.h
 * @brief Implementation of AIFrameTimer.
 *
 * Copyright (c) 2011, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   05/08/2011
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIFRAMETIMER_H
#define AIFRAMETIMER_H

#include "llframetimer.h"
#include "llthread.h"
#include <boost/signals2.hpp>
#include <set>

class LL_COMMON_API AIFrameTimer
{
  protected:
	typedef boost::signals2::signal<void (void)> signal_type;

  private:
	// Use separate struct for this object because it is non-copyable.
	struct Signal {
		signal_type mSignal;
	};

	// Notes on Thread-Safety
	//
	// This is the type of the objects stored in AIFrameTimer::sTimerList, and as such leans
	// for it's thread-safety on the same lock as is used for that std::multiset as follows.
	// An arbitrary thread can create, insert and initialize this object. Other threads can
	// not access it until that has completed.
	//
	// After creation two threads can access it: the thread that created it (owns the
	// AIFrameTimer object, which has an mHandle that points to this object), or the main
	// thread by finding it in sTimerList.
	//
	// See aiframetimer.cpp for more notes.
	class AIRunningFrameTimer {
	  private:
		F64 mExpire;						// Time at which the timer expires, in seconds since application start (compared to LLFrameTimer::sFrameTime).
		AIFrameTimer* mTimer;				// The actual timer.
		// Can be mutable, since only the mExpire is used for ordering this object in the multiset AIFrameTimer::sTimerList.
		mutable Signal* mCallback;			// Pointer to callback struct, or NULL when the object wasn't added to sTimerList yet.

	  public:
		AIRunningFrameTimer(F64 expiration, AIFrameTimer* timer) : mExpire(LLFrameTimer::getElapsedSeconds() + expiration), mTimer(timer), mCallback(NULL) { }
		~AIRunningFrameTimer() { delete mCallback; }

		// This function is called after the final object was added to sTimerList (where it is initialized in-place).
		void init(signal_type::slot_type const& slot) const
			{
			  // We may only call init() once.
			  llassert(!mCallback);
			  mCallback = new Signal;
			  mCallback->mSignal.connect(slot);
			}

		// Order AIFrameTimer::sTimerList so that the timer that expires first is up front.
		friend bool operator<(AIRunningFrameTimer const& ft1, AIRunningFrameTimer const& ft2) { return ft1.mExpire < ft2.mExpire; }

		void do_callback(void) const { mCallback->mSignal(); }
		F64 expiration(void) const { return mExpire; }
		AIFrameTimer* getTimer(void) const { return mTimer; }

#if LL_DEBUG
		// May not copy this object after it was initialized.
		AIRunningFrameTimer(AIRunningFrameTimer const& running_frame_timer) :
			mExpire(running_frame_timer.mExpire), mTimer(running_frame_timer.mTimer), mCallback(running_frame_timer.mCallback)
			{ llassert(!mCallback); }
#endif
	};

	typedef std::multiset<AIRunningFrameTimer> timer_list_type;

	static LLMutex sMutex;						// Mutex for the two global variables below.
	static timer_list_type sTimerList;			// List with all running timers.
	static F64 sNextExpiration;					// Cache of smallest value in sTimerList.
	friend class LLFrameTimer;					// Access to sNextExpiration.

	class Handle {
	  public:
		timer_list_type::iterator mRunningTimer;	// Points to the running timer, or to sTimerList.end() when not running.
													// Access to this iterator is protected by the AIFrameTimer::sMutex!
		LLMutex mMutex;								// A mutex used to protect us from deletion of the callback object while
													// calling the callback function in the case of simultaneous expiration
													// and cancellation by the thread owning the AIFrameTimer (by calling
													// AIFrameTimer::cancel).

		// Constructor for a not-running timer.
		Handle(void) : mRunningTimer(sTimerList.end()) { }

		// Actual initialization used by AIFrameTimer::create.
		void init(timer_list_type::iterator const& running_timer, signal_type::slot_type const& slot)
			{
			  // Locking AIFrameTimer::sMutex is not neccessary here, because we're creating
			  // the object and no other thread knows of mRunningTimer at this point.
			  mRunningTimer = running_timer;
			  mRunningTimer->init(slot);
			}

	  private:
		// LLMutex has no assignment operator.
	  	Handle& operator=(Handle const&) { return *this; }
	};

	Handle mHandle;

  public:
	// Construct an AIFrameTimer that is not running.
	AIFrameTimer(void) { }

	// Construction of a running AIFrameTimer with expiration time expiration in seconds, and callback slot.
	AIFrameTimer(F64 expiration, signal_type::slot_type const& slot) { create(expiration, slot); }

	// Destructing the AIFrameTimer object terminates the running timer (if still running).
	// Note that cancel() must have returned BEFORE anything is destructed that would disallow the callback function to be called.
	// So, if the AIFrameTimer is a member of an object whose callback function is called then cancel() has
	// to be called manually (or from the destructor of THAT object), before that object is destructed.
	// Cancel may be called multiple times.
	~AIFrameTimer() { cancel(); }

	void create(F64 expiration, signal_type::slot_type const& slot);
	void cancel(void);

	bool isRunning(void) const { bool running; sMutex.lock(); running = mHandle.mRunningTimer != sTimerList.end(); sMutex.unlock(); return running; }

  protected:
	static void handleExpiration(F64 current_frame_time);
};

#endif
