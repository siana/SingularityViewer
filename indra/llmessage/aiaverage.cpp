/**
 * @file aiaverage.cpp
 * @brief Implementation of AIAverage
 *
 * Copyright (c) 2013, Aleric Inglewood.
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
 *   11/04/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "sys.h"
#include "aiaverage.h"
#include "llerror.h"	// llassert

void AIAverage::cleanup(U64 clock_tick)
{
  // This expression can fail because the curl thread caches the time in
  // sTime_10ms for the duration of an entire loop. Therefore, the time can
  // go into the next 40ms and a texture fetch worker thread might call
  // cleanup() with that time, setting mCurrentClock to a value (one)
  // larger than sTime_10ms / 4. Next, the curl thread can continue to call
  // this function with the smaller value; in that case just add the new
  // data to the current bucket.
  //
  // Or, this is just the one-time initialization that happens the first
  // time this is called. In that case initialize just mCurrentClock:
  // the rest is already initialized upon construction.
  if (LL_LIKELY(clock_tick > mCurrentClock))
  {
	// Advance to the next bucket.
	++mCurrentBucket;
	mCurrentBucket %= mNrOfBuckets;
	// Initialize the new bucket.
	mData[mCurrentBucket].time = clock_tick;
	// Clean up old buckets.
	U64 old_time = clock_tick - mNrOfBuckets;
	if (LL_UNLIKELY(mTail == mCurrentBucket) ||		// Extremely unlikely: only happens when data was added EVERY clock tick for the past mNrOfBuckets clock ticks.
		mData[mTail].time <= old_time)
	{
	  do
	  {
		mSum -= mData[mTail].sum;
		mN -= mData[mTail].n;
		mData[mTail].sum = 0;
		mData[mTail].n = 0;
		++mTail;
		if (LL_UNLIKELY(mTail == mNrOfBuckets))
		{
		  mTail = 0;
		}
	  }
	  while (mData[mTail].time <= old_time);
	}
	// This was set to zero when mTail passed this point (likely not this call, but a few calls ago).
	llassert(mData[mCurrentBucket].sum == 0 &&
	         mData[mCurrentBucket].n == 0);
  }
  mCurrentClock = clock_tick;
  return;
}

