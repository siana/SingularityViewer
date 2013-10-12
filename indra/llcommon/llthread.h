/** 
 * @file llthread.h
 * @brief Base classes for thread, mutex and condition handling.
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

#ifndef LL_LLTHREAD_H
#define LL_LLTHREAD_H

#if LL_GNUC
// Needed for is_main_thread() when compiling with optimization (relwithdebinfo).
// It doesn't hurt to just always specify it though.
#pragma interface
#endif

#include "llapp.h"
#include "llapr.h"
#include "llmemory.h"
#include "apr_thread_cond.h"
#include "llaprpool.h"
#include "llatomic.h"
#include "aithreadid.h"

class LLThread;
class LLMutex;
class LLCondition;

class LL_COMMON_API LLThreadLocalDataMember
{
public:
	virtual ~LLThreadLocalDataMember() { };
};

class LL_COMMON_API LLThreadLocalData
{
private:
	static apr_threadkey_t* sThreadLocalDataKey;

public:
	// Thread-local memory pool.
	LLAPRRootPool mRootPool;
	LLVolatileAPRPool mVolatileAPRPool;
	LLThreadLocalDataMember* mCurlMultiHandle;	// Initialized by AICurlMultiHandle::getInstance
	char* mCurlErrorBuffer;						// NULL, or pointing to a buffer used by libcurl.
	std::string mName;							// "main thread", or a copy of LLThread::mName.

	static void init(void);
	static void destroy(void* thread_local_data);
	static void create(LLThread* pthread);
	static LLThreadLocalData& tldata(void);

private:
	LLThreadLocalData(char const* name);
	~LLThreadLocalData();
};

// Print to llerrs if the current thread is not the main thread.
LL_COMMON_API void assert_main_thread();

class LL_COMMON_API LLThread
{
private:
	static U32 sIDIter;
	static LLAtomicS32	sCount;
	static LLAtomicS32	sRunning;

public:
	typedef enum e_thread_status
	{
		STOPPED = 0,	// The thread is not running.  Not started, or has exited its run function
		RUNNING = 1,	// The thread is currently running
		QUITTING= 2 	// Someone wants this thread to quit
	} EThreadStatus;

	LLThread(std::string const& name);
	virtual ~LLThread(); // Warning!  You almost NEVER want to destroy a thread unless it's in the STOPPED state.
	virtual void shutdown(); // stops the thread
	
	bool isQuitting() const { return (QUITTING == mStatus); }
	bool isStopped() const { return (STOPPED == mStatus); }
	
	static S32 getCount() { return sCount; }	
	static S32 getRunning() { return sRunning; }
	static void yield(); // Static because it can be called by the main thread, which doesn't have an LLThread data structure.

public:
	// PAUSE / RESUME functionality. See source code for important usage notes.
	// Called from MAIN THREAD.
	void pause();
	void unpause();
	bool isPaused() { return isStopped() || mPaused; }
	
	// Cause the thread to wake up and check its condition
	void wake();

	// Same as above, but to be used when the condition is already locked.
	void wakeLocked();

	// Called from run() (CHILD THREAD). Pause the thread if requested until unpaused.
	void checkPause();

	// this kicks off the apr thread
	void start(void);

	// Can be used to tell the thread we're not interested anymore and it should abort.
	void setQuitting();
	
	// Return thread-local data for the current thread.
	static LLThreadLocalData& tldata(void) { return LLThreadLocalData::tldata(); }

private:
	bool				mPaused;
	
	// static function passed to APR thread creation routine
	static void *APR_THREAD_FUNC staticRun(apr_thread_t *apr_threadp, void *datap);

protected:
	std::string			mName;
	LLCondition*		mRunCondition;

	apr_thread_t		*mAPRThreadp;
	volatile EThreadStatus		mStatus;
	
	friend void LLThreadLocalData::create(LLThread* threadp);
	LLThreadLocalData*  mThreadLocalData;

	// virtual function overridden by subclass -- this will be called when the thread runs
	virtual void run(void) = 0; 
	
	// This class is completely done (called from THREAD!).
	virtual void terminated(void) { mStatus = STOPPED; }

	// virtual predicate function -- returns true if the thread should wake up, false if it should sleep.
	virtual bool runCondition(void);

	// Lock/Unlock Run Condition -- use around modification of any variable used in runCondition()
	inline void lockData();
	inline void unlockData();
	
	// This is the predicate that decides whether the thread should sleep.  
	// It should only be called with mRunCondition locked, since the virtual runCondition() function may need to access
	// data structures that are thread-unsafe.
	bool shouldSleep(void) { return (mStatus == RUNNING) && (isPaused() || (!runCondition())); }

	// To avoid spurious signals (and the associated context switches) when the condition may or may not have changed, you can do the following:
	// mRunCondition->lock();
	// if(!shouldSleep())
	//     mRunCondition->signal();
	// mRunCondition->unlock();
};

#ifdef SHOW_ASSERT
#define ASSERT_SINGLE_THREAD do { static AIThreadID first_thread_id; llassert(first_thread_id.equals_current_thread()); } while(0)
#else
#define ASSERT_SINGLE_THREAD do { } while(0)
#endif

//============================================================================

#define MUTEX_DEBUG (LL_DEBUG || LL_RELEASE_WITH_DEBUG_INFO)

#ifdef MUTEX_DEBUG
// We really shouldn't be using recursive locks. Make sure of that in debug mode.
#define MUTEX_FLAG APR_THREAD_MUTEX_UNNESTED
#else
// Use the fastest platform-optimal lock behavior (can be recursive or non-recursive).
#define MUTEX_FLAG APR_THREAD_MUTEX_DEFAULT
#endif

class LL_COMMON_API LLMutexBase
{
public:
	LLMutexBase() ;
	
	void lock();		// blocks
	void unlock();
	// Returns true if lock was obtained successfully.
	bool tryLock();

	// Returns true if a call to lock() would block (returns false if self-locked()).
	bool isLocked() const;

	// Returns true if locked by this thread.
	bool isSelfLocked() const;

protected:
	// mAPRMutexp is initialized and uninitialized in the derived class.
	apr_thread_mutex_t* mAPRMutexp;
	mutable U32			mCount;
	mutable AIThreadID	mLockingThread;

private:
	// Disallow copy construction and assignment.
	LLMutexBase(LLMutexBase const&);
	LLMutexBase& operator=(LLMutexBase const&);
};

class LL_COMMON_API LLMutex : public LLMutexBase
{
public:
	LLMutex(LLAPRPool& parent = LLThread::tldata().mRootPool) : mPool(parent)
	{
		apr_thread_mutex_create(&mAPRMutexp, MUTEX_FLAG, mPool());
	}
	~LLMutex()
	{
		//this assertion erroneously triggers whenever an LLCondition is destroyed
		//llassert(!isLocked()); // better not be locked!
		apr_thread_mutex_destroy(mAPRMutexp);
		mAPRMutexp = NULL;
	}

protected:
	LLAPRPool mPool;
};

#if APR_HAS_THREADS
// No need to use a root pool in this case.
typedef LLMutex LLMutexRootPool;
#else  // APR_HAS_THREADS
class LL_COMMON_API LLMutexRootPool : public LLMutexBase
{
public:
	LLMutexRootPool(void)
	{
		apr_thread_mutex_create(&mAPRMutexp, MUTEX_FLAG, mRootPool());
	}
	~LLMutexRootPool()
	{
#if APR_POOL_DEBUG
		// It is allowed to destruct root pools from a different thread.
		mRootPool.grab_ownership();
#endif
		llassert(!isLocked()); // better not be locked!
		apr_thread_mutex_destroy(mAPRMutexp);
		mAPRMutexp = NULL;
	}

protected:
	LLAPRRootPool mRootPool;
};
#endif // APR_HAS_THREADS

// Actually a condition/mutex pair (since each condition needs to be associated with a mutex).
class LL_COMMON_API LLCondition : public LLMutex
{
public:
	LLCondition(LLAPRPool& parent = LLThread::tldata().mRootPool);
	~LLCondition();
	
	void wait();		// blocks
	void signal();
	void broadcast();
	
protected:
	apr_thread_cond_t *mAPRCondp;
};

class LL_COMMON_API LLMutexLock
{
public:
	LLMutexLock(LLMutexBase* mutex)
	{
		mMutex = mutex;
		if(mMutex) mMutex->lock();
	}
	~LLMutexLock()
	{
		if(mMutex) mMutex->unlock();
	}
private:
	LLMutexBase* mMutex;
};

class LL_COMMON_API AIRWLock
{
public:
	AIRWLock(LLAPRPool& parent = LLThread::tldata().mRootPool) :
		mWriterWaitingMutex(parent), mNoHoldersCondition(parent), mHoldersCount(0), mWriterIsWaiting(false) { }

private:
	LLMutex mWriterWaitingMutex;		//!< This mutex is locked while some writer is waiting for access.
	LLCondition mNoHoldersCondition;	//!< Access control for mHoldersCount. Condition true when there are no more holders.
	int mHoldersCount;					//!< Number of readers or -1 if a writer locked this object.
	// This is volatile because we read it outside the critical area of mWriterWaitingMutex, at [1].
	// That means that other threads can change it while we are already in the (inlined) function rdlock.
	// Without volatile, the following assembly would fail:
	// register x = mWriterIsWaiting;
	// /* some thread changes mWriterIsWaiting */
	// if (x ...
	// However, because the function is fuzzy to begin with (we don't mind that this race
	// condition exists) it would work fine without volatile. So, basically it's just here
	// out of principle ;).	-- Aleric
    bool volatile mWriterIsWaiting;		//!< True when there is a writer waiting for write access.

public:
	void rdlock(bool high_priority = false)
	{
		// Give a writer a higher priority (kinda fuzzy).
		if (mWriterIsWaiting && !high_priority)	// [1] If there is a writer interested,
		{
			mWriterWaitingMutex.lock();			// [2] then give it precedence and wait here.
			// If we get here then the writer got it's access; mHoldersCount == -1.
			mWriterWaitingMutex.unlock();
		}
		mNoHoldersCondition.lock();				// [3] Get exclusive access to mHoldersCount.
		while (mHoldersCount == -1)				// [4]
		{
		  	mNoHoldersCondition.wait();			// [5] Wait till mHoldersCount is (or just was) 0.
		}
		++mHoldersCount;						// One more reader.
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
	void rdunlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		if (--mHoldersCount == 0)				// Was this the last reader?
		{
			mNoHoldersCondition.signal();		// Tell waiting threads, see [5], [6] and [7].
		}
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
	void wrlock(void)
	{
		mWriterWaitingMutex.lock();				// Block new readers, see [2],
		mWriterIsWaiting = true;				// from this moment on, see [1].
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		while (mHoldersCount != 0)				// Other readers or writers have this lock?
		{
		  	mNoHoldersCondition.wait();			// [6] Wait till mHoldersCount is (or just was) 0.
		}
		mWriterIsWaiting = false;				// Stop checking the lock for new readers, see [1].
		mWriterWaitingMutex.unlock();			// Release blocked readers, they will still hang at [3].
		mHoldersCount = -1;						// We are a writer now (will cause a hang at [5], see [4]).
		mNoHoldersCondition.unlock();			// Release lock on mHolders (readers go from [3] to [5]).
	}
	void wrunlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		mHoldersCount = 0;						// We have no writer anymore.
		mNoHoldersCondition.signal();			// Tell waiting threads, see [5], [6] and [7].
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
	void rd2wrlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount. Blocks new readers at [3].
		if (--mHoldersCount > 0)				// Any other reads left?
		{
			mWriterWaitingMutex.lock();			// Block new readers, see [2],
			mWriterIsWaiting = true;			// from this moment on, see [1].
			while (mHoldersCount != 0)			// Other readers (still) have this lock?
			{
				mNoHoldersCondition.wait();		// [7] Wait till mHoldersCount is (or just was) 0.
			}
			mWriterIsWaiting = false;			// Stop checking the lock for new readers, see [1].
			mWriterWaitingMutex.unlock();		// Release blocked readers, they will still hang at [3].
		}
		mHoldersCount = -1;						// We are a writer now (will cause a hang at [5], see [4]).
		mNoHoldersCondition.unlock();			// Release lock on mHolders (readers go from [3] to [5]).
	}
	void wr2rdlock(void)
	{
		mNoHoldersCondition.lock();				// Get exclusive access to mHoldersCount.
		mHoldersCount = 1;						// Turn writer into a reader.
		mNoHoldersCondition.signal();			// Tell waiting readers, see [5].
		mNoHoldersCondition.unlock();			// Release lock on mHoldersCount.
	}
#if LL_DEBUG
	// Really only intended for debugging purposes:
	bool isLocked(void)
	{
		mNoHoldersCondition.lock();
		bool res = mHoldersCount;
		mNoHoldersCondition.unlock();
		return res;
	}
#endif
};

#if LL_DEBUG
class LL_COMMON_API AINRLock
{
private:
	int read_locked;
	int write_locked;

	mutable bool mAccessed;
	mutable AIThreadID mTheadID;

	void accessed(void) const
	{
	  if (!mAccessed)
	  {
		mAccessed = true;
		mTheadID.reset();
	  }
	  else
	  {
		llassert_always(mTheadID.equals_current_thread());
	  }
	}

public:
	AINRLock(void) : read_locked(false), write_locked(false), mAccessed(false) { }

	bool isLocked() const { return read_locked || write_locked; }

	void rdlock(bool high_priority = false) { accessed(); ++read_locked; }
	void rdunlock() { --read_locked; }
	void wrlock() { llassert(!isLocked()); accessed(); ++write_locked; }
	void wrunlock() { --write_locked; }
	void wr2rdlock() { llassert(false); }
	void rd2wrlock() { llassert(false); }
};
#endif

//============================================================================

void LLThread::lockData()
{
	mRunCondition->lock();
}

void LLThread::unlockData()
{
	mRunCondition->unlock();
}


//============================================================================

// see llmemory.h for LLPointer<> definition

class LL_COMMON_API LLThreadSafeRefCount
{
private:
	LLThreadSafeRefCount(const LLThreadSafeRefCount&); // not implemented
	LLThreadSafeRefCount&operator=(const LLThreadSafeRefCount&); // not implemented

protected:
	virtual ~LLThreadSafeRefCount(); // use unref()
	
public:
	LLThreadSafeRefCount();
	
	void ref()
	{
		mRef++; 
	} 

	void unref()
	{
		llassert(mRef > 0);
		if (!--mRef) delete this;
	}
	S32 getNumRefs() const
	{
		return mRef;
	}

private: 
	LLAtomicS32	mRef; 
};

//============================================================================

// Simple responder for self destructing callbacks
// Pure virtual class
class LL_COMMON_API LLResponder : public LLThreadSafeRefCount
{
protected:
	virtual ~LLResponder();
public:
	virtual void completed(bool success) = 0;
};

//============================================================================

#endif // LL_LLTHREAD_H
