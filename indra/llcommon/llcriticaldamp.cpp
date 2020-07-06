/** 
 * @file llcriticaldamp.cpp
 * @brief Implementation of the critical damping functionality.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include "llcriticaldamp.h"
#include <algorithm>

//-----------------------------------------------------------------------------
// static members
//-----------------------------------------------------------------------------
LLFrameTimer LLSmoothInterpolation::sInternalTimer;
std::vector<LLSmoothInterpolation::Interpolant> LLSmoothInterpolation::sInterpolants;
F32 LLSmoothInterpolation::sTimeDelta;
std::pair<F32, F32> sCachedEntry;

// helper functors
struct LLSmoothInterpolation::CompareTimeConstants
{
	bool operator()(const F32& a, const LLSmoothInterpolation::Interpolant& b) const
	{
		return a < b.mTimeScale;
	}

	bool operator()(const LLSmoothInterpolation::Interpolant& a, const F32& b) const
	{
		return a.mTimeScale < b; // bottom of a is higher than bottom of b
	}

	bool operator()(const LLSmoothInterpolation::Interpolant& a, const LLSmoothInterpolation::Interpolant& b) const
	{
		return a.mTimeScale < b.mTimeScale; // bottom of a is higher than bottom of b
	}
};

//-----------------------------------------------------------------------------
// LLSmoothInterpolation()
//-----------------------------------------------------------------------------
LLSmoothInterpolation::LLSmoothInterpolation()
{
	sTimeDelta = 0.f;
	sCachedEntry = std::pair<F32, F32>(-1.f, -1.f);
}

// static
//-----------------------------------------------------------------------------
// updateInterpolants()
//-----------------------------------------------------------------------------
void LLSmoothInterpolation::updateInterpolants()
{
	sTimeDelta = sInternalTimer.getElapsedTimeAndResetF32();

	for (size_t i = 0; i < sInterpolants.size(); i++)
	{
		Interpolant& interp = sInterpolants[i];
		interp.mInterpolant = calcInterpolant(interp.mTimeScale);
		if (sCachedEntry.first == interp.mTimeScale)
		{
			sCachedEntry.second = interp.mInterpolant;
		}
	}
} 

//-----------------------------------------------------------------------------
// getInterpolant()
//-----------------------------------------------------------------------------
F32 LLSmoothInterpolation::getInterpolant(F32SecondsImplicit time_constant, bool use_cache)
{
	if (time_constant == 0.f)
	{
		return 1.f;
	}

	if (use_cache)
	{
		if (sCachedEntry.first == time_constant)
		{
			return sCachedEntry.second;
		}
		interpolant_vec_t::iterator find_it = std::lower_bound(sInterpolants.begin(), sInterpolants.end(), time_constant.value(), CompareTimeConstants());
		if (find_it != sInterpolants.end() && find_it->mTimeScale == time_constant) 
		{
			sCachedEntry = std::make_pair(time_constant.value(), find_it->mInterpolant);
			return find_it->mInterpolant;
		}
		else
		{
			Interpolant interp;
			interp.mTimeScale = time_constant.value();
			interp.mInterpolant = calcInterpolant(time_constant.value());
			sInterpolants.insert(find_it, interp);
			sCachedEntry = std::make_pair(time_constant.value(), interp.mInterpolant);
			return interp.mInterpolant;
		}
	}
	else
	{
		return calcInterpolant(time_constant.value());
	}
}

//-----------------------------------------------------------------------------
// calcInterpolant()
//-----------------------------------------------------------------------------
F32 LLSmoothInterpolation::calcInterpolant(F32 time_constant)
{
	return llclamp(1.f - powf(2.f, -sTimeDelta / time_constant), 0.f, 1.f);
}
