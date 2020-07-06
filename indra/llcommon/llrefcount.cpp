/** 
 * @file llrefcount.cpp
 * @brief Base class for reference counted objects for use with LLPointer
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
#include "linden_common.h"

#include "llrefcount.h"

#include "llerror.h"

#if LL_REF_COUNT_DEBUG
#include "llthread.h"
#endif

LLRefCount::LLRefCount(const LLRefCount& other) :	
#if LL_REF_COUNT_DEBUG
	mMutex(),
	mCrashAtUnlock(FALSE),
#endif
	mRef(0)
{
}

LLRefCount& LLRefCount::operator=(const LLRefCount&)
{
	// do nothing, since ref count is specific to *this* reference
	return *this;
}

LLRefCount::LLRefCount() :
#if LL_REF_COUNT_DEBUG
	mMutex(),
	mCrashAtUnlock(FALSE),
#endif
	mRef(0)
{
}

LLRefCount::~LLRefCount()
{ 
	if (mRef != 0)
	{
		LL_ERRS() << "deleting non-zero reference" << LL_ENDL;
	}
}

#if LL_REF_COUNT_DEBUG
void LLRefCount::ref() const
{ 
		if(mMutex.isLocked()) 
		{
			mCrashAtUnlock = TRUE ;
			LL_ERRS() << "the mutex is locked by the thread: " << mLockedThreadID 
				<< " Current thread: " << AIThreadID() << LL_ENDL ;
		}

		mMutex.lock() ;
		mLockedThreadID.reset_inline();

		mRef++; 

		if(mCrashAtUnlock)
		{
			while(1); //crash here.
		}
		mMutex.unlock() ;
} 

S32 LLRefCount::unref() const
{
		if(mMutex.isLocked()) 
		{
			mCrashAtUnlock = TRUE ;
			LL_ERRS() << "the mutex is locked by the thread: " << mLockedThreadID 
				<< " Current thread: " << AIThreadID() << LL_ENDL ;
		}

		mMutex.lock() ;
		mLockedThreadID.reset_inline();
		
		llassert(mRef >= 1);
		if (0 == --mRef) 
		{
			if(mCrashAtUnlock)
			{
				while(1); //crash here.
			}
			mMutex.unlock() ;

			delete this; 
			return 0;
		}

		if(mCrashAtUnlock)
		{
			while(1); //crash here.
		}
		mMutex.unlock() ;
		return mRef;
}	
#endif
