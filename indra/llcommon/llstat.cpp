/** 
 * @file llstat.cpp
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

#include "linden_common.h"

#include "llstat.h"
#include "lllivefile.h"
#include "llerrorcontrol.h"
#include "llframetimer.h"
#include "timing.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llstl.h"
#include "u64.h"


// statics
//------------------------------------------------------------------------
LLTimer LLStat::sTimer;
LLFrameTimer LLStat::sFrameTimer;

void LLStat::reset()
{
	mNumValues = 0;
	delete[] mBins;
	mBins      = new ValueEntry[mNumBins];
	mCurBin = mNumBins-1;
	mNextBin = 0;
}

LLStat::LLStat(std::string name, S32 num_bins, BOOL use_frame_timer)
:	mUseFrameTimer(use_frame_timer),
	mNumBins(num_bins),
	mName(name),
	mBins(NULL)
{
	llassert_always(mNumBins > 1);

	reset();

	if (!mName.empty())
	{
		stat_map_t::iterator iter = getStatList().find(mName);
		if (iter != getStatList().end())
			LL_WARNS() << "LLStat with duplicate name: " << mName << LL_ENDL;
		getStatList().insert(std::make_pair(mName, this));
	}
}

LLStat::stat_map_t& LLStat::getStatList()
{
	static LLStat::stat_map_t* stat_list = NULL;
	if(!stat_list)
	{
		stat_list = new LLStat::stat_map_t();
	}
	return *stat_list;
}

LLStat::~LLStat()
{
	delete[] mBins;

	if (!mName.empty())
	{
		// handle multiple entries with the same name
		stat_map_t::iterator iter = getStatList().find(mName);
		while (iter != getStatList().end() && iter->second != this)
			++iter;
		getStatList().erase(iter);
	}
}

void LLStat::start()
{
	if (mUseFrameTimer)
	{
		mBins[mNextBin].mBeginTime = sFrameTimer.getElapsedSeconds();
	}
	else
	{
		mBins[mNextBin].mBeginTime = sTimer.getElapsedTimeF64();
	}
}

void LLStat::addValue(const F32 value)
{
	if (mNumValues < mNumBins)
	{
		mNumValues++;
	}

	// Increment the bin counters.
	mCurBin = (mCurBin+1) % mNumBins;
	mNextBin = (mNextBin+1) % mNumBins;

	mBins[mCurBin].mValue = value;
	if (mUseFrameTimer)
	{
		mBins[mCurBin].mTime = sFrameTimer.getElapsedSeconds();
	}
	else
	{
		mBins[mCurBin].mTime = sTimer.getElapsedTimeF64();
	}
	mBins[mCurBin].mDT = (F32)(mBins[mCurBin].mTime - mBins[mCurBin].mBeginTime);

	// Set the begin time for the next stat segment.
	mBins[mNextBin].mBeginTime = mBins[mCurBin].mTime;
	mBins[mNextBin].mTime = mBins[mCurBin].mTime;
	mBins[mNextBin].mDT = 0.f;
}


F32 LLStat::getMax() const
{
	F32 current_max = getCurrent();
	for (S32 i = 0; i < (S32)mNumValues; i++)
	{
		// Skip the bin we're currently filling.
		if (i == mNextBin)
		{
			continue;
		}
		current_max = llmax(current_max, mBins[i].mValue);
	}

	return current_max;
}

F32 LLStat::getMean() const
{
	F32 current_mean = 0.f; // Don't double-count current.
	S32 samples = 0;
	for (S32 i = 0; i < (S32)mNumValues; i++)
	{
		// Skip the bin we're currently filling.
		if (i == mNextBin)
		{
			continue;
		}
		current_mean += mBins[i].mValue;
		samples++;
	}

	// There will be a wrap error at 2^32. :)
	if (samples != 0)
	{
		current_mean /= samples;
	}
	return current_mean;
}

F32 LLStat::getMin() const
{
	F32 current_min = getCurrent();

	for (S32 i = 0; i < (S32)mNumValues; i++)
	{
		// Skip the bin we're currently filling.
		if (i == mNextBin)
		{
			continue;
		}
		current_min = llmin(current_min, mBins[i].mValue);
	}

	return current_min;
}

F32 LLStat::getPrev(S32 age) const
{
	S32 bin;
	bin = mCurBin - age;

	while (bin < 0)
	{
		bin += mNumBins;
	}

	if (bin == mNextBin)
	{
		// Bogus for bin we're currently working on.
		return 0.f;
	}
	return mBins[bin].mValue;
}

F32 LLStat::getPrevPerSec(S32 age) const
{
	S32 bin;
	bin = mCurBin - age;

	while (bin < 0)
	{
		bin += mNumBins;
	}

	if (bin == mNextBin)
	{
		// Bogus for bin we're currently working on.
		return 0.f;
	}
	return (U32(bin) < mNumValues) ? (mBins[bin].mValue / mBins[bin].mDT) : 0.f;
}

F64 LLStat::getPrevBeginTime(S32 age) const
{
	S32 bin;
	bin = mCurBin - age;

	while (bin < 0)
	{
		bin += mNumBins;
	}

	if (bin == mNextBin)
	{
		// Bogus for bin we're currently working on.
		return 0.f;
	}

	return mBins[bin].mBeginTime;
}

F64 LLStat::getPrevTime(S32 age) const
{
	S32 bin;
	bin = mCurBin - age;

	while (bin < 0)
	{
		bin += mNumBins;
	}

	if (bin == mNextBin)
	{
		// Bogus for bin we're currently working on.
		return 0.f;
	}

	return mBins[bin].mTime;
}


F64 LLStat::getBinTime(S32 bin) const
{
	return mBins[bin].mTime;
}

F32 LLStat::getCurrent() const
{
	return mBins[mCurBin].mValue;
}

F32 LLStat::getCurrentPerSec() const
{
	return (U32(mCurBin) < mNumValues) ? (mBins[mCurBin].mValue / mBins[mCurBin].mDT) : 0.f;
}

F32 LLStat::getCurrentDuration() const
{
	return mBins[mCurBin].mDT;
}

F64 LLStat::getCurrentTime() const
{
	return mBins[mCurBin].mTime;
}

F32 LLStat::getMeanPerSec() const
{
	F32 value = 0.f;
	F32 dt    = 0.f;

	for (S32 i = 0; i < (S32)mNumValues; i++)
	{
		// Skip the bin we're currently filling.
		if (i == mNextBin)
		{
			continue;
		}
		value += mBins[i].mValue;
		dt    += mBins[i].mDT;
	}

	if (dt > 0.f)
	{
		return value/dt;
	}
	else
	{
		return 0.f;
	}
}

F32 LLStat::getMeanDuration() const
{
	F32 dur = 0.0f;
	S32 count = 0;
	for (S32 i=0; i < (S32)mNumValues; i++)
	{
		if (i == mNextBin)
		{
			continue;
		}
		dur += mBins[i].mDT;
		count++;
	}

	if (count > 0)
	{
		dur /= F32(count);
		return dur;
	}
	else
	{
		return 0.f;
	}
}

F32 LLStat::getMaxPerSec() const
{
	F32 value = 0.f;

	for (S32 i = 0; i < (S32)mNumValues; i++)
	{
		// Skip the bin we're currently filling.
		if (i == mNextBin)
		{
			continue;
		}
		value = llmax(value, mBins[i].mValue/mBins[i].mDT);
	}
	return value;
}

F32 LLStat::getMinPerSec() const
{
	F32 value = 0.f;

	for (S32 i = 0; i < (S32)mNumValues; i++)
	{
		// Skip the bin we're currently filling.
		if (i == mNextBin)
		{
			continue;
		}
		value = llmin(value, mBins[i].mValue/mBins[i].mDT);
	}
	return value;
}

U32 LLStat::getNumValues() const
{
	return mNumValues;
}

S32 LLStat::getNumBins() const
{
	return mNumBins;
}

S32 LLStat::getCurBin() const
{
	return mCurBin;
}

S32 LLStat::getNextBin() const
{
	return mNextBin;
}

