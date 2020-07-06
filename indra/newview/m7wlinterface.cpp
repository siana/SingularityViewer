/* Copyright (C) 2012 Siana Gearz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#include "llviewerprecompiledheaders.h"

#include "meta7windlight.h"
#include "llenvmanager.h"
#include "llwaterparammanager.h"
#include "llwlparammanager.h"
#include "message_prehash.h"

#include "m7wlinterface.h"

M7WindlightInterface::M7WindlightInterface()
:mHasOverride(false)
{
}

void M7WindlightInterface::receiveMessage(LLMessageSystem* msg)
{
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_ParamList);
	for (S32 i = 0; i < count; ++i)
	{
		// our param is binary data)
		S32 size = msg->getSizeFast(_PREHASH_ParamList, i, _PREHASH_Parameter);
		if (size >= 0)
		{
			mHasOverride = true;
			char buf[250];
			msg->getBinaryDataFast(
			_PREHASH_ParamList, _PREHASH_Parameter,
			buf, size, i, 249);

#if 0
			std::ostringstream wldump;
			char hex []= "0123456789abcdefRRRR";
			for (int i = 0; i<250; ++i){
				wldump << "\\x"  << hex[((U8)buf[i]&0xF0)>>4] << hex[(U8)buf[i]&0x0F];
			}
			LL_INFOS() << "Received LightShare data: " << wldump.str() << LL_ENDL;
#endif
			char default_windlight[] = "\x00\x00\x80\x40\x00\x00\x18\x42\x00\x00\x80\x42\x00\x00\x80\x40\x00\x00\x80\x3e\x00\x00\x00\x40\x00\x00\x00\x40\x00\x00\x00\x40\xcd\xcc\xcc\x3e\x00\x00\x00\x3f\x8f\xc2\xf5\x3c\xcd\xcc\x4c\x3e\x0a\xd7\x23\x3d\x66\x66\x86\x3f\x3d\x0a\xd7\xbe\x7b\x14\x8e\x3f\xe1\x7a\x94\xbf\x82\x2d\xed\x49\x9a\x6c\xf6\x1c\xcb\x89\x6d\xf5\x4f\x42\xcd\xf4\x00\x00\x80\x3e\x00\x00\x80\x3e\x0a\xd7\xa3\x3e\x0a\xd7\xa3\x3e\x5c\x8f\x42\x3e\x8f\xc2\xf5\x3d\xae\x47\x61\x3e\x5c\x8f\xc2\x3e\x5c\x8f\xc2\x3e\x33\x33\x33\x3f\xec\x51\x38\x3e\xcd\xcc\x4c\x3f\x8f\xc2\x75\x3e\xb8\x1e\x85\x3e\x9a\x99\x99\x3e\x9a\x99\x99\x3e\xd3\x4d\xa2\x3e\x33\x33\xb3\x3e\x33\x33\xb3\x3e\x33\x33\xb3\x3e\x33\x33\xb3\x3e\x00\x00\x00\x00\xcd\xcc\xcc\x3d\x00\x00\xe0\x3f\x00\x00\x80\x3f\x00\x00\x00\x00\x85\xeb\xd1\x3e\x85\xeb\xd1\x3e\x85\xeb\xd1\x3e\x85\xeb\xd1\x3e\x00\x00\x80\x3f\x14\xae\x07\x3f\x00\x00\x80\x3f\x71\x3d\x8a\x3e\x3d\x0a\xd7\x3e\x00\x00\x80\x3f\x14\xae\x07\x3f\x8f\xc2\xf5\x3d\xcd\xcc\x4c\x3e\x0a\xd7\x23\x3c\x45\x06\x00";
			if(!memcmp(default_windlight, buf, sizeof(default_windlight)))
			{
				LL_INFOS() << "LightShare matches default" << LL_ENDL;
				receiveReset();
				return;
			}

			LLWaterParamManager::getInstance()->getParamSet("Default", mWater);

			Meta7WindlightPacket* wl = (Meta7WindlightPacket*)buf;
			mWater.set("waterFogColor", wl->waterColor.red / 256.f, wl->waterColor.green / 256.f, wl->waterColor.blue / 256.f);
			mWater.set("waterFogDensity", pow(2.0f, wl->waterFogDensityExponent));
			mWater.set("underWaterFogMod", wl->underwaterFogModifier);
			mWater.set("normScale", wl->reflectionWaveletScale.X,wl->reflectionWaveletScale.Y,wl->reflectionWaveletScale.Z);
			mWater.set("fresnelScale", wl->fresnelScale);
			mWater.set("fresnelOffset", wl->fresnelOffset);
			mWater.set("scaleAbove", wl->refractScaleAbove);
			mWater.set("scaleBelow", wl->refractScaleBelow);
			mWater.set("blurMultiplier", wl->blurMultiplier);
			mWater.set("wave1Dir", wl->littleWaveDirection.X, wl->littleWaveDirection.Y);
			mWater.set("wave2Dir", wl->bigWaveDirection.X, wl->bigWaveDirection.Y);

			std::string out = llformat(
			"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			(U8)(wl->normalMapTexture[0]),
			(U8)(wl->normalMapTexture[1]),
			(U8)(wl->normalMapTexture[2]),
			(U8)(wl->normalMapTexture[3]),
			(U8)(wl->normalMapTexture[4]),
			(U8)(wl->normalMapTexture[5]),
			(U8)(wl->normalMapTexture[6]),
			(U8)(wl->normalMapTexture[7]),
			(U8)(wl->normalMapTexture[8]),
			(U8)(wl->normalMapTexture[9]),
			(U8)(wl->normalMapTexture[10]),
			(U8)(wl->normalMapTexture[11]),
			(U8)(wl->normalMapTexture[12]),
			(U8)(wl->normalMapTexture[13]),
			(U8)(wl->normalMapTexture[14]),
			(U8)(wl->normalMapTexture[15]));

			mNormalMapTexture.set(out);
			LLWaterParamManager::getInstance()->mCurParams = mWater;
			LLWaterParamManager::getInstance()->setNormalMapID(mNormalMapTexture);
			LLWaterParamManager::getInstance()->propagateParameters();

			LLWLParamManager::getInstance()->mAnimator.deactivate();

			LLWLParamManager::getInstance()->getParamSet(LLWLParamKey("Default", LLWLParamKey::SCOPE_LOCAL), mWindlight);

			mWindlight.setSunAngle(F_TWO_PI * wl->sunMoonPosiiton);
			mWindlight.setEastAngle(F_TWO_PI * wl->eastAngle);
			mWindlight.set("sunlight_color", wl->sunMoonColor.red * 3.0f, wl->sunMoonColor.green * 3.0f, wl->sunMoonColor.blue * 3.0f, wl->sunMoonColor.alpha * 3.0f);
			mWindlight.set("ambient", wl->ambient.red * 3.0f, wl->ambient.green * 3.0f, wl->ambient.blue * 3.0f, wl->ambient.alpha * 3.0f);
			mWindlight.set("blue_horizon", wl->horizon.red * 2.0f, wl->horizon.green *2.0f, wl->horizon.blue * 2.0f, wl->horizon.alpha * 2.0f);
			mWindlight.set("blue_density", wl->blueDensity.red * 2.0f, wl->blueDensity.green * 2.0f, wl->blueDensity.blue * 2.0f, wl->blueDensity.alpha * 2.0f);
			mWindlight.set("haze_horizon", wl->hazeHorizon);
			mWindlight.set("haze_density", wl->hazeDensity);
			mWindlight.set("cloud_shadow", wl->cloudCoverage);
			mWindlight.set("density_multiplier", wl->densityMultiplier / 1000.0f);
			mWindlight.set("distance_multiplier", wl->distanceMultiplier);
			mWindlight.set("max_y",(F32)wl->maxAltitude);
			mWindlight.set("cloud_color", wl->cloudColor.red, wl->cloudColor.green, wl->cloudColor.blue, wl->cloudColor.alpha);
			mWindlight.set("cloud_pos_density1", wl->cloudXYDensity.X, wl->cloudXYDensity.Y, wl->cloudXYDensity.Z);
			mWindlight.set("cloud_pos_density2", wl->cloudDetailXYDensity.X, wl->cloudDetailXYDensity.Y, wl->cloudDetailXYDensity.Z);
			mWindlight.set("cloud_scale", wl->cloudScale);
			mWindlight.set("gamma", wl->sceneGamma);
			mWindlight.set("glow",(2 - wl->sunGlowSize) * 20 , 0.f, -wl->sunGlowFocus * 5);
			mWindlight.setCloudScrollX(wl->cloudScrollX + 10.0f);
			mWindlight.setCloudScrollY(wl->cloudScrollY + 10.0f);
			mWindlight.setEnableCloudScrollX(!wl->cloudScrollXLock);
			mWindlight.setEnableCloudScrollY(!wl->cloudScrollYLock);
			mWindlight.setStarBrightness(wl->starBrightness);

			LLWLParamManager::getInstance()->mCurParams = mWindlight;
			LLWLParamManager::getInstance()->propagateParameters();
		}
	}
}

void M7WindlightInterface::receiveReset()
{
	LL_INFOS() << "Received LightShare reset" << LL_ENDL;
	mHasOverride = false;
	LLEnvManagerNew::getInstance()->usePrefs();
}
