/** 
 * @file llrendersphere.cpp
 * @brief implementation of the LLRenderSphere class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

//	Sphere creates a set of display lists that can then be called to create 
//	a lit sphere at different LOD levels.  You only need one instance of sphere 
//	per viewer - then call the appropriate list.  

#include "linden_common.h"

#include "llrendersphere.h"
#include "llerror.h"

#include "llglheaders.h"

LLRenderSphere gSphere;

void LLRenderSphere::render()
{
	renderGGL();
	gGL.flush();
}

inline LLVector3 polar_to_cart(F32 latitude, F32 longitude)
{
	return LLVector3(sin(F_TWO_PI * latitude) * cos(F_TWO_PI * longitude),
					 sin(F_TWO_PI * latitude) * sin(F_TWO_PI * longitude),
					 cos(F_TWO_PI * latitude));
}


void LLRenderSphere::renderGGL()
{
	S32 const LATITUDE_SLICES = 20;
	S32 const LONGITUDE_SLICES = 30;

	if (mSpherePoints.empty())
	{
		mSpherePoints.resize(LATITUDE_SLICES + 1);
		for (S32 lat_i = 0; lat_i < LATITUDE_SLICES + 1; lat_i++)
		{
			mSpherePoints[lat_i].resize(LONGITUDE_SLICES + 1);
			for (S32 lon_i = 0; lon_i < LONGITUDE_SLICES + 1; lon_i++)
			{
				F32 lat = (F32)lat_i / LATITUDE_SLICES;
				F32 lon = (F32)lon_i / LONGITUDE_SLICES;

				mSpherePoints[lat_i][lon_i] = polar_to_cart(lat, lon);
			}
		}
	}
	
	gGL.begin(LLRender::TRIANGLES);

	for (S32 lat_i = 0; lat_i < LATITUDE_SLICES; lat_i++)
	{
		for (S32 lon_i = 0; lon_i < LONGITUDE_SLICES; lon_i++)
		{
			gGL.vertex3fv(mSpherePoints[lat_i][lon_i].mV);
			gGL.vertex3fv(mSpherePoints[lat_i][lon_i+1].mV);
			gGL.vertex3fv(mSpherePoints[lat_i+1][lon_i].mV);

			gGL.vertex3fv(mSpherePoints[lat_i+1][lon_i].mV);
			gGL.vertex3fv(mSpherePoints[lat_i][lon_i+1].mV);
			gGL.vertex3fv(mSpherePoints[lat_i+1][lon_i+1].mV);
		}
	}
	gGL.end();
}
