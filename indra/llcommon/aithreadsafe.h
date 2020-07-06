/**
 * @file aithreadsafe.h
 * @brief Implementation of AIThreadSafe, AIReadAccessConst, AIReadAccess and AIWriteAccess.
 *
 * Copyright (c) 2010 - 2013, Aleric Inglewood.
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
 *   31/03/2010
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   14/03/2012
 *   Added AIThreadSafeSingleThread and friends.
 *   Added AIAccessConst (and derived AIAccess from it) to allow read
 *   access to a const AIThreadSafeSimple.
 *
 *   26/01/2013
 *   Added support for LLCondition to AIThreadSafeSimple.
 */

// This file defines wrapper template classes for arbitrary types T
// adding locking to the instance and shielding it from access
// without first being locked.
//
// Locking and getting access works by creating a temporary (local)
// access object that takes the wrapper class as argument. Creating
// the access object obtains the lock, while destructing it releases
// the lock.
//
// There are three types of wrapper classes:
// AIThreadSafe, AIThreadSafeSimple and AIThreadSafeSingleThread.
//
// AIThreadSafe is for use with the access classes:
// AIReadAccessConst, AIReadAccess and AIWriteAccess.
//
// AIThreadSafeSimple is for use with the access classes:
// AIAccessConst and AIAccess.
//
// AIThreadSafeSingleThread is for use with the access classes:
// AISTAccessConst and AISTAccess.
//
// AIReadAccessConst provides read access to a const AIThreadSafe.
// AIReadAccess provides read access to a non-const AIThreadSafe.
// AIWriteAccess provides read/write access to a non-const AIThreadSafe.
//
// AIAccessConst provides read access to a const AIThreadSafeSimple.
// AIAccess provides read/write access to a non-const AIThreadSafeSimple.
//
// AISTAccessConst provides read access to a const AIThreadSafeSingleThread.
// AISTAccess provides read/write access to a non-const AIThreadSafeSingleThread.
//
// Thus, AIThreadSafe is to protect objects with a read/write lock,
// AIThreadSafeSimple is to protect objects with a single mutex,
// and AIThreadSafeSingleThread doesn't do any locking but makes sure
// (in Debug mode) that the wrapped object is only accessed by one thread.
//
// Each wrapper class allows its wrapped object to be constructed
// with arbitrary parameters by using operator new with placement;
// for example, to instantiate a class Foo with read/write locking:
//
// AIThreadSafe<Foo> foo(new (foo.memory()) Foo(param1, param2, ...));
//
// Each wrapper class has a derived class that end on 'DC' (which
// stand for Default Constructed): AIThreadSafeDC, AIThreadSafeSimpleDC
// and AIThreadSafeSingleThreadDC. The default constructors of those
// wrapper classes cause the wrapped instance to be default constructed
// as well. They also provide a general one-parameter constructor.
// For example:
//
// AIThreadSafeDC<Foo> foo;			// Default constructed Foo.
// AIThreadSafeDC<Foo> foo(3.4);	// Foo with one constructor parameter.
//
#ifndef AITHREADSAFE_H
#define AITHREADSAFE_H

#include <new>

#include "llthread.h"
#include "llerror.h"

template<typename T, typename RWLOCK> struct AIReadAccessConst;
template<typename T, typename RWLOCK> struct AIReadAccess;
template<typename T, typename RWLOCK> struct AIWriteAccess;
template<typename T, typename MUTEX> struct AIAccessConst;
template<typename T, typename MUTEX> struct AIAccess;
template<typename T> struct AISTAccessConst;
template<typename T> struct AISTAccess;

// This helper class is needed because offsetof is only allowed on POD types.
template<typename T>
struct AIThreadSafeBitsPOD
{
	// AIThreadSafe is a wrapper around an instance of T.
	// Because T might not have a default constructor, it is constructed
	// 'in place', with placement new, in the memory reserved here.
	//
	// Make sure that the memory that T will be placed in is properly
	// aligned by using an array of long's.
	long mMemory[(sizeof(T) + sizeof(long) - 1) / sizeof(long)];
};

template<typename T>
class AIThreadSafeBits : private AIThreadSafeBitsPOD<T>
{
public:
	// The wrapped objects are constructed in-place with placement new *outside*
	// of this object (by AITHREADSAFE macro(s) or derived classes).
	// However, we are responsible for the destruction of the wrapped object.
	~AIThreadSafeBits() { ptr()->~T(); }

	// Only for use by AITHREADSAFE, see below.
	void* memory() const { return const_cast<long*>(&AIThreadSafeBitsPOD<T>::mMemory[0]); }

	// Cast a T* back to AIThreadSafeBits<T>. This is the inverse of memory().
	template<typename T2>
	  static AIThreadSafeBits<T2>* wrapper_cast(T2* ptr)
	      { return static_cast<AIThreadSafeBits<T2>*>(reinterpret_cast<AIThreadSafeBitsPOD<T2>*>(reinterpret_cast<char*>(ptr) - offsetof(AIThreadSafeBitsPOD<T2>, mMemory[0]))); }
	template<typename T2>
	  static AIThreadSafeBits<T2> const* wrapper_cast(T2 const* ptr)
	      { return static_cast<AIThreadSafeBits<T2> const*>(reinterpret_cast<AIThreadSafeBitsPOD<T2> const*>(reinterpret_cast<char const*>(ptr) - offsetof(AIThreadSafeBitsPOD<T2>, mMemory[0]))); }

protected:
	// Accessors.
	T const* ptr() const { return reinterpret_cast<T const*>(AIThreadSafeBitsPOD<T>::mMemory); }
	T* ptr() { return reinterpret_cast<T*>(AIThreadSafeBitsPOD<T>::mMemory); }
};

/**
 * @brief A wrapper class for objects that need to be accessed by more than one thread, allowing concurrent readers.
 *
 * Use AITHREADSAFE to define instances of any type, and use AIReadAccessConst,
 * AIReadAccess and AIWriteAccess to get access to the instance.
 *
 * For example,
 *
 * <code>
 * class Foo { public: Foo(int, int); };
 *
 * AITHREADSAFE(Foo, foo, (2, 3));
 *
 * AIReadAccess<Foo> foo_r(foo);
 * // Use foo_r-> for read access.
 *
 * AIWriteAccess<Foo> foo_w(foo);
 * // Use foo_w-> for write access.
 * </code>
 *
 * If <code>foo</code> is constant, you have to use <code>AIReadAccessConst<Foo></code>.
 *
 * It is possible to pass access objects to a function that
 * downgrades the access, for example:
 *
 * <code>
 * void readfunc(AIReadAccess const& access);
 *
 * AIWriteAccess<Foo> foo_w(foo);
 * readfunc(foo_w);					// readfunc will perform read access to foo_w.
 * </code>
 *
 * If <code>AIReadAccess</code> is non-const, you can upgrade the access by creating
 * an <code>AIWriteAccess</code> object from it. For example:
 *
 * <code>
 * AIWriteAccess<Foo> foo_w(foo_r);
 * </code>
 *
 * This API is Robust(tm). If you try anything that could result in problems,
 * it simply won't compile. The only mistake you can still easily make is
 * to obtain write access to an object when it is not needed, or to unlock
 * an object in between accesses while the state of the object should be
 * preserved. For example:
 *
 * <code>
 * // This resets foo to point to the first file and then returns that.
 * std::string filename = AIWriteAccess<Foo>(foo)->get_first_filename();
 *
 * // WRONG! The state between calling get_first_filename and get_next_filename should be preserved!
 *
 * AIWriteAccess<Foo> foo_w(foo);	// Wrong. The code below only needs read-access.
 * while (!filename.empty())
 * {
 *     something(filename);
 *     filename = foo_w->next_filename();
 * }
 * </code>
 *
 * Correct would be
 *
 * <code>
 * AIReadAccess<Foo> foo_r(foo);
 * std::string filename = AIWriteAccess<Foo>(foo_r)->get_first_filename();
 * while (!filename.empty())
 * {
 *     something(filename);
 *     filename = foo_r->next_filename();
 * }
 * </code>
 *
 */
template<typename T, typename RWLOCK = AIRWLock>
class AIThreadSafe : public AIThreadSafeBits<T>
{
protected:
	// Only these may access the object (through ptr()).
	friend struct AIReadAccessConst<T, RWLOCK>;
	friend struct AIReadAccess<T, RWLOCK>;
	friend struct AIWriteAccess<T, RWLOCK>;

	// Locking control.
	RWLOCK mRWLock;

	// For use by AIThreadSafeDC
	AIThreadSafe(void) { }
	MUTEX_POOL(AIThreadSafe(LLAPRPool& parent) : mRWLock(parent){ })

public:
	// Only for use by AITHREADSAFE, see below.
	AIThreadSafe(T* object)
	{
#if !LL_WINDOWS
		llassert(object == AIThreadSafeBits<T>::ptr());
#endif
	}

#if LL_DEBUG
	// Can only be locked when there still exists an AIAccess object that
	// references this object and will access it upon destruction.
	// If the assertion fails, make sure that such AIAccess object is
	// destructed before the deletion of this object.
	~AIThreadSafe() { llassert(!mRWLock.isLocked()); }
#endif
};

/**
 * @brief Instantiate an static, global or local object of a given type wrapped in AIThreadSafe, using an arbitrary constructor.
 *
 * For example, instead of doing
 *
 * <code>
 * Foo foo(x, y);
 * static Bar bar(0.1, true);
 * </code>
 *
 * One can instantiate a thread-safe instance with
 *
 * <code>
 * AITHREADSAFE(Foo, foo, (x, y));
 * static AITHREADSAFE(Bar, bar, (0.1, true));
 * </code>
 *
 * Note: This macro does not allow to allocate such object on the heap.
 *       If that is needed, have a look at AIThreadSafeDC.
 */
#define AITHREADSAFE(type, var, paramlist) AIThreadSafe<type> var(new (var.memory()) type paramlist)

/**
 * @brief A wrapper class for objects that need to be accessed by more than one thread.
 *
 * This class is the same as an AIThreadSafe wrapper, except that it can only
 * be used for default constructed objects, or constructed with one parameter.
 *
 * For example, instead of
 *
 * <code>
 * Foo foo;
 * Bar bar(3);
 * </code>
 *
 * One would use
 *
 * <code>
 * AIThreadSafeDC<Foo> foo;
 * AIThreadSafeDC<Bar> bar(3);
 * </code>
 *
 * The advantage over AITHREADSAFE is that this object can be allocated with
 * new on the heap. For example:
 *
 * <code>
 * AIThreadSafeDC<Foo>* ptr = new AIThreadSafeDC<Foo>;
 * </code>
 *
 * which is not possible with AITHREADSAFE.
 */
template<typename T, typename RWLOCK = AIRWLock>
class AIThreadSafeDC : public AIThreadSafe<T, RWLOCK>
{
public:
	// Construct a wrapper around a default constructed object.
	AIThreadSafeDC(void) { new (AIThreadSafe<T, RWLOCK>::ptr()) T; }
	// Allow an arbitrary parameter to be passed for construction.
	template<typename T2> AIThreadSafeDC(T2 const& val) { new (AIThreadSafe<T, RWLOCK>::ptr()) T(val); }
};

/**
 * @brief Read lock object and provide read access.
 */
template<typename T, typename RWLOCK = AIRWLock>
struct AIReadAccessConst
{
	//! Internal enum for the lock-type of the AI*Access object.
	enum state_type
	{
		readlocked,			//!< A AIReadAccessConst or AIReadAccess.
		read2writelocked,	//!< A AIWriteAccess constructed from a AIReadAccess.
		writelocked,		//!< A AIWriteAccess constructed from a AIThreadSafe.
		write2writelocked	//!< A AIWriteAccess constructed from (the AIReadAccess base class of) a AIWriteAccess.
	};

	//! Construct a AIReadAccessConst from a constant AIThreadSafe.
	AIReadAccessConst(AIThreadSafe<T, RWLOCK> const& wrapper, bool high_priority = false)
		: mWrapper(const_cast<AIThreadSafe<T, RWLOCK>&>(wrapper)),
		  mState(readlocked)
		{
			mWrapper.mRWLock.rdlock(high_priority);
		}

	//! Destruct the AI*Access object.
	// These should never be dynamically allocated, so there is no need to make this virtual.
	~AIReadAccessConst()
		{
			if (mState == readlocked)
				mWrapper.mRWLock.rdunlock();
			else if (mState == writelocked)
				mWrapper.mRWLock.wrunlock();
			else if (mState == read2writelocked)
				mWrapper.mRWLock.wr2rdlock();
		}

	//! Access the underlaying object for read access.
	T const* operator->() const { return mWrapper.ptr(); }

	//! Access the underlaying object for read access.
	T const& operator*() const { return *mWrapper.ptr(); }

protected:
	//! Constructor used by AIReadAccess.
	AIReadAccessConst(AIThreadSafe<T, RWLOCK>& wrapper, state_type state)
		: mWrapper(wrapper), mState(state)
		{ }

	AIThreadSafe<T, RWLOCK>& mWrapper;	//!< Reference to the object that we provide access to.
	state_type const mState;	//!< The lock state that mWrapper is in.

private:
	// Disallow copy constructing directly.
	AIReadAccessConst(AIReadAccessConst const&);
};

/**
 * @brief Read lock object and provide read access, with possible promotion to write access.
 */
template<typename T, typename RWLOCK = AIRWLock>
struct AIReadAccess : public AIReadAccessConst<T, RWLOCK>
{
	typedef typename AIReadAccessConst<T, RWLOCK>::state_type state_type;
	using AIReadAccessConst<T, RWLOCK>::readlocked;

	//! Construct a AIReadAccess from a non-constant AIThreadSafe.
	AIReadAccess(AIThreadSafe<T, RWLOCK>& wrapper, bool high_priority = false) : AIReadAccessConst<T, RWLOCK>(wrapper, readlocked) { this->mWrapper.mRWLock.rdlock(high_priority); }

protected:
	//! Constructor used by AIWriteAccess.
	AIReadAccess(AIThreadSafe<T, RWLOCK>& wrapper, state_type state) : AIReadAccessConst<T, RWLOCK>(wrapper, state) { }

	friend struct AIWriteAccess<T, RWLOCK>;
};

/**
 * @brief Write lock object and provide read/write access.
 */
template<typename T, typename RWLOCK = AIRWLock>
struct AIWriteAccess : public AIReadAccess<T, RWLOCK>
{
	using AIReadAccessConst<T, RWLOCK>::readlocked;
	using AIReadAccessConst<T, RWLOCK>::read2writelocked;
	using AIReadAccessConst<T, RWLOCK>::writelocked;
	using AIReadAccessConst<T, RWLOCK>::write2writelocked;

	//! Construct a AIWriteAccess from a non-constant AIThreadSafe.
	AIWriteAccess(AIThreadSafe<T, RWLOCK>& wrapper) : AIReadAccess<T, RWLOCK>(wrapper, writelocked) { this->mWrapper.mRWLock.wrlock();}

	//! Promote read access to write access.
	explicit AIWriteAccess(AIReadAccess<T, RWLOCK>& access)
		: AIReadAccess<T, RWLOCK>(access.mWrapper, (access.mState == readlocked) ? read2writelocked : write2writelocked)
		{
			if (this->mState == read2writelocked)
			{
				this->mWrapper.mRWLock.rd2wrlock();
			}
		}

	//! Access the underlaying object for (read and) write access.
	T* operator->() const { return this->mWrapper.ptr(); }

	//! Access the underlaying object for (read and) write access.
	T& operator*() const { return *this->mWrapper.ptr(); }
};

/**
 * @brief A wrapper class for objects that need to be accessed by more than one thread.
 *
 * Use AITHREADSAFESIMPLE to define instances of any type, and use AIAccess
 * to get access to the instance.
 *
 * For example,
 *
 * <code>
 * class Foo { public: Foo(int, int); };
 *
 * AITHREADSAFESIMPLE(Foo, foo, (2, 3));
 *
 * AIAccess<Foo> foo_w(foo);
 * // Use foo_w-> for read and write access.
 * </code>
 *
 * See also AIThreadSafe
 */
template<typename T, typename MUTEX = LLMutex>
class AIThreadSafeSimple : public AIThreadSafeBits<T>
{
protected:
	// Only this one may access the object (through ptr()).
	friend struct AIAccessConst<T, MUTEX>;
	friend struct AIAccess<T, MUTEX>;

	// Locking control.
	MUTEX mMutex;

	friend struct AIRegisteredStateMachinesList;
	// For use by AIThreadSafeSimpleDC and AIRegisteredStateMachinesList.
	AIThreadSafeSimple(void) { }
	MUTEX_POOL(AIThreadSafeSimple(LLAPRPool& parent) : mMutex(parent) { })

public:
	// Only for use by AITHREADSAFESIMPLE, see below.
	AIThreadSafeSimple(T* object) { llassert(object == AIThreadSafeBits<T>::ptr()); }

#if LL_DEBUG
	// Can only be locked when there still exists an AIAccess object that
	// references this object and will access it upon destruction.
	// If the assertion fails, make sure that such AIAccess object is
	// destructed before the deletion of this object.
	~AIThreadSafeSimple() { llassert(!mMutex.isLocked()); }
#endif
};

/**
 * @brief Instantiate an static, global or local object of a given type wrapped in AIThreadSafeSimple, using an arbitrary constructor.
 *
 * For example, instead of doing
 *
 * <code>
 * Foo foo(x, y);
 * static Bar bar(0.1, true);
 * </code>
 *
 * One can instantiate a thread-safe instance with
 *
 * <code>
 * AITHREADSAFESIMPLE(Foo, foo, (x, y));
 * static AITHREADSAFESIMPLE(Bar, bar, (0.1, true));
 * </code>
 *
 * Note: This macro does not allow to allocate such object on the heap.
 *       If that is needed, have a look at AIThreadSafeSimpleDC.
 */
#define AITHREADSAFESIMPLE(type, var, paramlist) AIThreadSafeSimple<type> var(new (var.memory()) type paramlist)
#define AITHREADSAFESIMPLECONDITION(type, var, paramlist) AIThreadSafeSimple<type, LLCondition> var(new (var.memory()) type paramlist)

/**
 * @brief A wrapper class for objects that need to be accessed by more than one thread.
 *
 * This class is the same as an AIThreadSafeSimple wrapper, except that it can only
 * be used for default constructed objects, or constructed with one parameter.
 *
 * For example, instead of
 *
 * <code>
 * Foo foo;
 * Bar bar(0.1);
 * </code>
 *
 * One would use
 *
 * <code>
 * AIThreadSafeSimpleDC<Foo> foo;
 * AIThreadSafeSimpleDC<Bar> bar(0.1);
 * </code>
 *
 * The advantage over AITHREADSAFESIMPLE is that this object can be allocated with
 * new on the heap. For example:
 *
 * <code>
 * AIThreadSafeSimpleDC<Foo>* ptr = new AIThreadSafeSimpleDC<Foo>;
 * </code>
 *
 * which is not possible with AITHREADSAFESIMPLE.
 */
template<typename T, typename MUTEX = LLMutex>
class AIThreadSafeSimpleDC : public AIThreadSafeSimple<T, MUTEX>
{
public:
	// Construct a wrapper around a default constructed object.
	AIThreadSafeSimpleDC(void) { new (AIThreadSafeSimple<T, MUTEX>::ptr()) T; }
	// Allow an arbitrary parameter to be passed for construction.
	template<typename T2> AIThreadSafeSimpleDC(T2 const& val) { new (AIThreadSafeSimple<T, MUTEX>::ptr()) T(val); }

protected:
	// For use by AIThreadSafeSimpleDCRootPool
	AIThreadSafeSimpleDC(LLAPRRootPool& parent) : AIThreadSafeSimple<T, MUTEX>(MUTEX_POOL(parent)) { new (AIThreadSafeSimple<T, MUTEX>::ptr()) T; }
};

// Helper class for AIThreadSafeSimpleDCRootPool to assure initialization of
// the root pool before constructing AIThreadSafeSimpleDC.
class AIThreadSafeSimpleDCRootPool_pbase
{
protected:
	LLAPRRootPool mRootPool;

private:
	template<typename T> friend class AIThreadSafeSimpleDCRootPool;
	AIThreadSafeSimpleDCRootPool_pbase(void) { }
};

/**
 * @brief A wrapper class for objects that need to be accessed by more than one thread.
 *
 * The same as AIThreadSafeSimpleDC except that this class creates its own LLAPRRootPool
 * for the internally used mutexes and condition, instead of using the current threads
 * root pool. The advantage of this is that it can be used for objects that need to
 * be accessed from the destructors of global objects (after main). The disadvantage
 * is that it's less efficient to use your own root pool, therefore its use should be
 * restricted to those cases where it is absolutely necessary.
 */
template<typename T>
class AIThreadSafeSimpleDCRootPool : private AIThreadSafeSimpleDCRootPool_pbase, public AIThreadSafeSimpleDC<T>
{
public:
	// Construct a wrapper around a default constructed object, using memory allocated
	// from the operating system for the internal APR objects (mutexes and conditional),
	// as opposed to allocated from the current threads root pool.
	AIThreadSafeSimpleDCRootPool(void) :
		AIThreadSafeSimpleDCRootPool_pbase(),
		AIThreadSafeSimpleDC<T>(MUTEX_POOL(mRootPool)) { }
};

/**
 * @brief Write lock object and provide read access.
 */
template<typename T, typename MUTEX = LLMutex>
struct AIAccessConst
{
	//! Construct a AIAccessConst from a constant AIThreadSafeSimple.
	AIAccessConst(AIThreadSafeSimple<T, MUTEX> const& wrapper) : mWrapper(const_cast<AIThreadSafeSimple<T, MUTEX>&>(wrapper))
	{
	  this->mWrapper.mMutex.lock();
	}

	//! Access the underlaying object for (read and) write access.
	T const* operator->() const { return this->mWrapper.ptr(); }

	//! Access the underlaying object for (read and) write access.
	T const& operator*() const { return *this->mWrapper.ptr(); }

	~AIAccessConst()
	{
	  this->mWrapper.mMutex.unlock();
	}

	// If MUTEX is an LLCondition, then this can be used to wait for a signal.
	void wait() { this->mWrapper.mMutex.wait(); }
	// If MUTEX is a LLCondition then this can be used to wake up the waiting thread.
	void signal() { this->mWrapper.mMutex.signal(); }

protected:
	AIThreadSafeSimple<T, MUTEX>& mWrapper;		//!< Reference to the object that we provide access to.

private:
	// Disallow copy constructing directly.
	AIAccessConst(AIAccessConst const&);
};

/**
 * @brief Write lock object and provide read/write access.
 */
template<typename T, typename MUTEX = LLMutex>
struct AIAccess : public AIAccessConst<T, MUTEX>
{
	//! Construct a AIAccess from a non-constant AIThreadSafeSimple.
	AIAccess(AIThreadSafeSimple<T, MUTEX>& wrapper) : AIAccessConst<T, MUTEX>(wrapper) { }

	//! Access the underlaying object for (read and) write access.
	T* operator->() const { return this->mWrapper.ptr(); }

	//! Access the underlaying object for (read and) write access.
	T& operator*() const { return *this->mWrapper.ptr(); }
};

/**
 * @brief A wrapper class for objects that should only be accessed by a single thread.
 *
 * Use AITHREADSAFESINGLETHREAD to define instances of any type, and use AISTAccess
 * to get access to the instance.
 *
 * For example,
 *
 * <code>
 * class Foo { public: Foo(int, int); };
 *
 * AITHREADSAFESINGLETHREAD(Foo, foo, (2, 3));
 *
 * AISTAccess<Foo> foo_w(foo);
 * // Use foo_w-> for read and write access.
 * </code>
 */
template<typename T>
class AIThreadSafeSingleThread : public AIThreadSafeBits<T>
{
protected:
	// Only these one may access the object (through ptr()).
	friend struct AISTAccessConst<T>;
	friend struct AISTAccess<T>;

	// For use by AIThreadSafeSingleThreadDC.
	AIThreadSafeSingleThread(void)
#ifdef LL_DEBUG
	  : mAccessed(false)
#endif
	{ }

#ifdef LL_DEBUG
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
#endif

public:
	// Only for use by AITHREADSAFESINGLETHREAD, see below.
	AIThreadSafeSingleThread(T* object)
#ifdef LL_DEBUG
	  : mAccessed(false)
#endif
	{
	  llassert(object == AIThreadSafeBits<T>::ptr());
	}

private:
	// Disallow copying or assignments.
	AIThreadSafeSingleThread(AIThreadSafeSingleThread const&);
	void operator=(AIThreadSafeSingleThread const&);
};

/**
 * @brief A wrapper class for objects that should only be accessed by a single thread.
 *
 * This class is the same as an AIThreadSafeSingleThread wrapper, except that it can only
 * be used for default constructed objects, or constructed with one parameter.
 *
 * For example, instead of
 *
 * <code>
 * Foo foo;
 * Bar bar(0.1);
 * </code>
 *
 * One would use
 *
 * <code>
 * AIThreadSafeSingleThreadDC<Foo> foo;
 * AIThreadSafeSingleThreadDC<Bar> bar(0.1);
 * </code>
 *
 * The advantage over AITHREADSAFESINGLETHREAD is that this object can be allocated with
 * new on the heap. For example:
 *
 * <code>
 * AIThreadSafeSingleThreadDC<Foo>* ptr = new AIThreadSafeSingleThreadDC<Foo>;
 * </code>
 *
 * which is not possible with AITHREADSAFESINGLETHREAD.
 *
 * This class is primarily intended to test if some (member) variable needs locking,
 * during development (in debug mode), and is therefore more flexible in that it
 * automatically converts to the underlying type, can be assigned to and can be
 * written to an ostream, as if it wasn't wrapped at all. This is to reduce the
 * impact on the source code.
 */
template<typename T>
class AIThreadSafeSingleThreadDC : public AIThreadSafeSingleThread<T>
{
public:
	// Construct a wrapper around a default constructed object.
	AIThreadSafeSingleThreadDC(void) { new (AIThreadSafeSingleThread<T>::ptr()) T; }
	// Allow an arbitrary parameter to be passed for construction.
	template<typename T2> AIThreadSafeSingleThreadDC(T2 const& val) { new (AIThreadSafeSingleThread<T>::ptr()) T(val); }

	// Allow assigning with T.
	AIThreadSafeSingleThreadDC& operator=(T const& val) { AIThreadSafeSingleThread<T>::accessed(); *AIThreadSafeSingleThread<T>::ptr() = val; return *this; }
	// Allow writing to an ostream.
	friend std::ostream& operator<<(std::ostream& os, AIThreadSafeSingleThreadDC const& wrapped_val) { wrapped_val.accessed(); return os << *wrapped_val.ptr(); }

	// Automatic conversion to T.
	operator T&(void) { AIThreadSafeSingleThread<T>::accessed(); return *AIThreadSafeSingleThread<T>::ptr(); }
	operator T const&(void) const { AIThreadSafeSingleThread<T>::accessed(); return *AIThreadSafeSingleThread<T>::ptr(); }
};

/**
 * @brief Instantiate a static, global or local object of a given type wrapped in AIThreadSafeSingleThread, using an arbitrary constructor.
 *
 * For example, instead of doing
 *
 * <code>
 * Foo foo(x, y);
 * static Bar bar;
 * </code>
 *
 * One can instantiate a thread-safe instance with
 *
 * <code>
 * AITHREADSAFESINGLETHREAD(Foo, foo, (x, y));
 * static AITHREADSAFESINGLETHREAD(Bar, bar, );
 * </code>
 *
 * Note: This macro does not allow to allocate such object on the heap.
 *       If that is needed, have a look at AIThreadSafeSingleThreadDC.
 */
#define AITHREADSAFESINGLETHREAD(type, var, paramlist) AIThreadSafeSingleThread<type> var(new (var.memory()) type paramlist)

/**
 * @brief Access single threaded object for read access.
 */
template<typename T>
struct AISTAccessConst
{
	//! Construct a AISTAccessConst from a constant AIThreadSafeSingleThread.
	AISTAccessConst(AIThreadSafeSingleThread<T> const& wrapper) : mWrapper(const_cast<AIThreadSafeSingleThread<T>&>(wrapper))
	{
#if LL_DEBUG
	  wrapper.accessed();
#endif
	}

	//! Access the underlaying object for read access.
	T const* operator->() const { return this->mWrapper.ptr(); }

	//! Access the underlaying object for read write access.
	T const& operator*() const { return *this->mWrapper.ptr(); }

protected:
	AIThreadSafeSingleThread<T>& mWrapper;		//!< Reference to the object that we provide access to.

private:
	// Disallow copy constructing directly.
	AISTAccessConst(AISTAccessConst const&);
};

/**
 * @brief Access single threaded object for read/write access.
 */
template<typename T>
struct AISTAccess : public AISTAccessConst<T>
{
	//! Construct a AISTAccess from a non-constant AIThreadSafeSingleThread.
	AISTAccess(AIThreadSafeSingleThread<T>& wrapper) : AISTAccessConst<T>(wrapper)
	{
#if LL_DEBUG
	  wrapper.accessed();
#endif
	}

	//! Access the underlaying object for (read and) write access.
	T* operator->() const { return this->mWrapper.ptr(); }

	//! Access the underlaying object for (read and) write access.
	T& operator*() const { return *this->mWrapper.ptr(); }
};

#endif
