/**
 * @file llwlparamset.cpp
 * @brief Implementation for the LLWLParamSet class.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llwlparamset.h"
#include "llwlanimator.h"

#include "llfloaterwindlight.h"
#include "llwlparammanager.h"
#include "lluictrlfactory.h"
#include "llsliderctrl.h"

#include <llgl.h>

#include <sstream>

static LLStaticHashedString sStarBrightness("star_brightness");
static LLStaticHashedString sPresetNum("preset_num");
static LLStaticHashedString sSunAngle("sun_angle");
static LLStaticHashedString sEastAngle("east_angle");
static LLStaticHashedString sEnableCloudScroll("enable_cloud_scroll");
static LLStaticHashedString sCloudScrollRate("cloud_scroll_rate");
static LLStaticHashedString sLightNorm("lightnorm");
static LLStaticHashedString sCloudDensity("cloud_pos_density1");
static LLStaticHashedString sCloudScale("cloud_scale");
static LLStaticHashedString sCloudShadow("cloud_shadow");
static LLStaticHashedString sDensityMultiplier("density_multiplier");
static LLStaticHashedString sDistanceMultiplier("distance_multiplier");
static LLStaticHashedString sHazeDensity("haze_density");
static LLStaticHashedString sHazeHorizon("haze_horizon");
static LLStaticHashedString sMaxY("max_y");

LLWLParamSet::LLWLParamSet(void) :
	mName("Unnamed Preset"),
	mCloudScrollXOffset(0.f), mCloudScrollYOffset(0.f)	
{}

static LLTrace::BlockTimerStatHandle FTM_WL_PARAM_UPDATE("WL Param Update");

void LLWLParamSet::update(LLGLSLShader * shader) const 
{	
	LL_RECORD_BLOCK_TIME(FTM_WL_PARAM_UPDATE);
	LLSD::map_const_iterator i = mParamValues.beginMap();
	std::vector<LLStaticHashedString>::const_iterator n = mParamHashedNames.begin();
	for(;(i != mParamValues.endMap()) && (n != mParamHashedNames.end());++i, n++)
	{
		const LLStaticHashedString& param = *n;
		
		// check that our pre-hashed names are still tracking the mParamValues map correctly
		//
		llassert(param.String() == i->first);

		if (param == sStarBrightness || param == sPresetNum || param == sSunAngle ||
			param == sEastAngle || param == sEnableCloudScroll ||
			param == sCloudScrollRate || param == sLightNorm ) 
		{
			continue;
		}
		else if (param == sCloudDensity)
		{
			LLVector4 val;
			val.mV[0] = F32(i->second[0].asReal()) + mCloudScrollXOffset;
			val.mV[1] = F32(i->second[1].asReal()) + mCloudScrollYOffset;
			val.mV[2] = (F32) i->second[2].asReal();
			val.mV[3] = (F32) i->second[3].asReal();

			stop_glerror();
			shader->uniform4fv(param, 1, val.mV);
			stop_glerror();
		}
		else // param is the uniform name
		{
			// handle all the different cases
			if (i->second.isArray())
			{
				stop_glerror();
				// Switch statement here breaks msbuild for some reason
				if (i->second.size() == 4)
				{
					LLVector4 val(
						i->second[0].asFloat(),
						i->second[1].asFloat(),
						i->second[2].asFloat(),
						i->second[3].asFloat()
					);
					shader->uniform4fv(param, 1, val.mV);
				}
				else if (i->second.size() == 3)
				{
					shader->uniform3f(param,
						i->second[0].asFloat(),
						i->second[1].asFloat(),
						i->second[2].asFloat());
				}
				else if (i->second.size() == 2)
				{
					shader->uniform2f(param,
						i->second[0].asFloat(),
						i->second[1].asFloat());
				}
				else if (i->second.size() == 1)
				{
					shader->uniform1f(param, i->second[0].asFloat());
				}
				stop_glerror();
			} 
			else if (i->second.isReal())
			{
				stop_glerror();
				shader->uniform1f(param, i->second.asFloat());
				stop_glerror();
			} 
			else if (i->second.isInteger())
			{
				stop_glerror();
				shader->uniform1i(param, i->second.asInteger());
				stop_glerror();
			} 
			else if (i->second.isBoolean())
			{
				stop_glerror();
				shader->uniform1i(param, i->second.asBoolean() ? 1 : 0);
				stop_glerror();
			}
		}
	}
}

void LLWLParamSet::set(const std::string& paramName, float x) 
{
	// handle case where no array
	if(mParamValues.isUndefined() || mParamValues[paramName].isReal())
	{
		mParamValues[paramName] = x;
	} 
	
	// handle array
	else if(mParamValues[paramName].isArray() &&
			mParamValues[paramName][0].isReal())
	{
		mParamValues[paramName][0] = x;
	}
}

void LLWLParamSet::set(const std::string& paramName, float x, float y)
{
	mParamValues[paramName][0] = x;
	mParamValues[paramName][1] = y;
}

void LLWLParamSet::set(const std::string& paramName, float x, float y, float z) 
{
	mParamValues[paramName][0] = x;
	mParamValues[paramName][1] = y;
	mParamValues[paramName][2] = z;
}

void LLWLParamSet::set(const std::string& paramName, float x, float y, float z, float w) 
{
	mParamValues[paramName][0] = x;
	mParamValues[paramName][1] = y;
	mParamValues[paramName][2] = z;
	mParamValues[paramName][3] = w;
}

void LLWLParamSet::set(const std::string& paramName, const float * val) 
{
	mParamValues[paramName][0] = val[0];
	mParamValues[paramName][1] = val[1];
	mParamValues[paramName][2] = val[2];
	mParamValues[paramName][3] = val[3];
}

void LLWLParamSet::set(const std::string& paramName, const LLVector4 & val) 
{
	mParamValues[paramName][0] = val.mV[0];
	mParamValues[paramName][1] = val.mV[1];
	mParamValues[paramName][2] = val.mV[2];
	mParamValues[paramName][3] = val.mV[3];
}

void LLWLParamSet::set(const std::string& paramName, const LLColor4 & val) 
{
	mParamValues[paramName][0] = val.mV[0];
	mParamValues[paramName][1] = val.mV[1];
	mParamValues[paramName][2] = val.mV[2];
	mParamValues[paramName][3] = val.mV[3];
}

LLVector4 LLWLParamSet::getVector(const std::string& paramName, bool& error) 
{
	// test to see if right type
	LLSD cur_val = mParamValues.get(paramName);
	if (!cur_val.isArray()) 
	{
		error = true;
		return LLVector4(0,0,0,0);
	}
	
	LLVector4 val;
	val.mV[0] = (F32) cur_val[0].asReal();
	val.mV[1] = (F32) cur_val[1].asReal();
	val.mV[2] = (F32) cur_val[2].asReal();
	val.mV[3] = (F32) cur_val[3].asReal();
	
	error = false;
	return val;
}

F32 LLWLParamSet::getFloat(const std::string& paramName, bool& error) 
{
	// test to see if right type
	LLSD cur_val = mParamValues.get(paramName);
	if (cur_val.isArray() && cur_val.size() != 0) 
	{
		error = false;
		return (F32) cur_val[0].asReal();	
	}
	
	if(cur_val.isReal())
	{
		error = false;
		return (F32) cur_val.asReal();
	}
	
	error = true;
	return 0;
}

void LLWLParamSet::setSunAngle(float val) 
{
	// keep range 0 - 2pi
	if(val > F_TWO_PI || val < 0)
	{
		F32 num = val / F_TWO_PI;
		num -= floor(num);
		val = F_TWO_PI * num;
	}

	set("sun_angle", val);
}


void LLWLParamSet::setEastAngle(float val) 
{
	// keep range 0 - 2pi
	if(val > F_TWO_PI || val < 0)
	{
		F32 num = val / F_TWO_PI;
		num -= floor(num);
		val = F_TWO_PI * num;
	}

	set("east_angle", val);
}

void LLWLParamSet::mix(LLWLParamSet& src, LLWLParamSet& dest, F32 weight)
{
	// set up the iterators

	// keep cloud positions and coverage the same
	/// TODO masking will do this later
	F32 cloudPos1X = (F32) mParamValues["cloud_pos_density1"][0].asReal();
	F32 cloudPos1Y = (F32) mParamValues["cloud_pos_density1"][1].asReal();
	F32 cloudPos2X = (F32) mParamValues["cloud_pos_density2"][0].asReal();
	F32 cloudPos2Y = (F32) mParamValues["cloud_pos_density2"][1].asReal();
	F32 cloudCover = (F32) mParamValues["cloud_shadow"].asReal();

	LLSD srcVal;
	LLSD destVal;

	// Iterate through values
	for(LLSD::map_iterator iter = mParamValues.beginMap(); iter != mParamValues.endMap(); ++iter)
	{
		// If param exists in both src and dest, set the holder variables, otherwise skip
		if(src.mParamValues.has(iter->first) && dest.mParamValues.has(iter->first))
		{
			srcVal = src.mParamValues[iter->first];
			destVal = dest.mParamValues[iter->first];
		}
		else
		{
			continue;
		}
		
		if(iter->second.isReal())									// If it's a real, interpolate directly
		{
			iter->second = srcVal.asReal() + ((destVal.asReal() - srcVal.asReal()) * weight);
		}
		else if(iter->second.isArray() && iter->second[0].isReal()	// If it's an array of reals, loop through the reals and interpolate on those
				&& iter->second.size() == srcVal.size() && iter->second.size() == destVal.size())
		{
			// Actually do interpolation: old value + (difference in values * factor)
			for(int i=0; i < iter->second.size(); ++i) 
			{
				// iter->second[i] = (1.f-weight)*(F32)srcVal[i].asReal() + weight*(F32)destVal[i].asReal();	// old way of doing it -- equivalent but one more operation
				iter->second[i] = srcVal[i].asReal() + ((destVal[i].asReal() - srcVal[i].asReal()) * weight);
			}
		}
		else														// Else, skip
		{
			continue;
		}		
	}

	// now mix the extra parameters
	setStarBrightness((1 - weight) * (F32) src.getStarBrightness()
		+ weight * (F32) dest.getStarBrightness());

	llassert(src.getSunAngle() >= - F_PI && 
					src.getSunAngle() <= 3 * F_PI);
	llassert(dest.getSunAngle() >= - F_PI && 
					dest.getSunAngle() <= 3 * F_PI);
	llassert(src.getEastAngle() >= 0 && 
					src.getEastAngle() <= 4 * F_PI);
	llassert(dest.getEastAngle() >= 0 && 
					dest.getEastAngle() <= 4 * F_PI);

	// sun angle and east angle require some handling to make sure
	// they go in circles.  Yes quaternions would work better.
	F32 srcSunAngle = src.getSunAngle();
	F32 destSunAngle = dest.getSunAngle();
	F32 srcEastAngle = src.getEastAngle();
	F32 destEastAngle = dest.getEastAngle();
	
	if(fabsf(srcSunAngle - destSunAngle) > F_PI) 
	{
		if(srcSunAngle > destSunAngle) 
		{
			destSunAngle += 2 * F_PI;
		} 
		else 
		{
			srcSunAngle += 2 * F_PI;
		}
	}

	if(fabsf(srcEastAngle - destEastAngle) > F_PI) 
	{
		if(srcEastAngle > destEastAngle) 
		{
			destEastAngle += 2 * F_PI;
		} 
		else 
		{
			srcEastAngle += 2 * F_PI;
		}
	}

	setSunAngle((1 - weight) * srcSunAngle + weight * destSunAngle);
	setEastAngle((1 - weight) * srcEastAngle + weight * destEastAngle);
	
	// now setup the sun properly

	// reset those cloud positions
	set("cloud_pos_density1", cloudPos1X, cloudPos1Y);
	set("cloud_pos_density2", cloudPos2X, cloudPos2Y);
	set("cloud_shadow", cloudCover);
}

void LLWLParamSet::updateCloudScrolling(void) 
{
	static LLTimer s_cloud_timer;

	F64 delta_t = s_cloud_timer.getElapsedTimeAndResetF64();

	if(getEnableCloudScrollX())
	{
		mCloudScrollXOffset += F32(delta_t * (getCloudScrollX() - 10.f) / 100.f);
	}
	if(getEnableCloudScrollY())
	{
		mCloudScrollYOffset += F32(delta_t * (getCloudScrollY() - 10.f) / 100.f);
	}
}

void LLWLParamSet::updateHashedNames()
{
	mParamHashedNames.clear();
	// Iterate through values
	for(LLSD::map_iterator iter = mParamValues.beginMap(); iter != mParamValues.endMap(); ++iter)
	{
		LLStaticHashedString param(iter->first);
		mParamHashedNames.push_back(param);
		if (iter->second.isArray() && (param == sCloudScale || param == sCloudShadow ||
			param == sDensityMultiplier || param == sDistanceMultiplier ||
			param == sHazeDensity || param == sHazeHorizon ||
			param == sMaxY))
		{
			// Params are incorrect in the XML files. These SHOULD be F32, not arrays.
			iter->second.assign(iter->second[0]);
		}
	}
}

