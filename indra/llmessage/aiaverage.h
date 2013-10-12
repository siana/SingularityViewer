/**
 * @file aiaverage.h
 * @brief Definition of class AIAverage
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

#ifndef AIAVERAGE_H
#define AIAVERAGE_H

#include "llpreprocessor.h"
#include "stdtypes.h"	// U32, U64
#include "llthread.h"	// LLMutex
#include <cstddef>      // size_t
#include <vector>

class AIAverage {
  private:
	struct Data {
	  U32 sum;						// Accumulated sum of the 'n' passed to operator()(size_t n, U64 clock_tick) with clock_tick == time.
	  U32 n;						// The number of calls to operator().
	  U64 time;						// The clock_tick as passed to operator()(size_t n, U64 clock_tick) that sum corresponds to.
	};

	U64 mCurrentClock;				// The current (last) time that operator() was called with, or -1 when not initialized.
	int mTail;						// The oldest bucket with still valid data.
	int mCurrentBucket;				// The bucket that corresponds to mCurrentClock.
	size_t mSum;					// The sum of all the 'n' passed to operator()(size_t n, U64 clock_tick) for all passed mNrOfBuckets time units.
	U32 mN;							// The number of calls to operator().
	int const mNrOfBuckets;			// Size of mData.
	std::vector<Data> mData;		// The buckets.

	mutable LLMutex mLock;			// Mutex for all of the above data.

  public:
	AIAverage(int number_of_buckets) : mCurrentClock(~(U64)0), mTail(0), mCurrentBucket(0), mSum(0), mN(0), mNrOfBuckets(number_of_buckets), mData(number_of_buckets)
	{
	  // Fill mData with all zeroes (much faster than adding a constructor to Data).
	  std::memset(&*mData.begin(), 0, number_of_buckets * sizeof(Data));
	}
	size_t addData(U32 n, U64 clock_tick)
	{
	  DoutEntering(dc::curl, "AIAverage::addData(" << n << ", " << clock_tick << ")");
	  mLock.lock();
	  if (LL_UNLIKELY(clock_tick != mCurrentClock))
	  {
		cleanup(clock_tick);
	  }
	  mSum += n;
	  mN += 1;
	  mData[mCurrentBucket].sum += n;
	  mData[mCurrentBucket].n += 1;
	  size_t sum = mSum;
	  mLock.unlock();
	  Dout(dc::curl, "Current sum: " << sum << ", average: " << (sum / mN));
	  return sum;
	}
	size_t truncateData(U64 clock_tick)
	{
	  mLock.lock();
	  if (clock_tick != mCurrentClock)
	  {
		cleanup(clock_tick);
	  }
	  size_t sum = mSum;
	  mLock.unlock();
	  return sum;
	}
	double getAverage(double avg_no_data) const
	{
	  mLock.lock();
	  double avg = mSum;
	  llassert(mN != 0 || mSum == 0);
	  if (LL_UNLIKELY(mN == 0))
		avg = avg_no_data;
	  else
		avg /= mN;
	  mLock.unlock();
	  return avg;
	}

  private:
	void cleanup(U64 clock_tick);
};

#endif // AIAVERAGE
