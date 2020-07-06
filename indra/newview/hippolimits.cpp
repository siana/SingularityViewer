#include "llviewerprecompiledheaders.h"

#include "hippolimits.h"

#include "hippogridmanager.h"

#include "llerror.h"

#include "llviewercontrol.h"		// gSavedSettings

HippoLimits *gHippoLimits = 0;


HippoLimits::HippoLimits()
{
	setLimits();
}


void HippoLimits::setLimits()
{
	if (gHippoGridManager->getConnectedGrid()->getPlatform() == HippoGridInfo::PLATFORM_SECONDLIFE) {
		setSecondLifeLimits();
	} else if (gHippoGridManager->getConnectedGrid()->getPlatform() == HippoGridInfo::PLATFORM_WHITECORE) {
		setWhiteCoreLimits();
	} else {
		setOpenSimLimits();
	}
}

void HippoLimits::setOpenSimLimits()
{
	mMaxPrimScale = 8192.0f;
	mMaxHeight = 10000.0f;
	mMinPrimScale = 0.001f;
	if (gHippoGridManager->getConnectedGrid()->isRenderCompat()) {
		LL_INFOS() << "Using rendering compatible OpenSim limits." << LL_ENDL;
		mMinHoleSize = 0.05f;
		mMaxHollow = 0.95f;
	} else {
		LL_INFOS() << "Using Hippo OpenSim limits." << LL_ENDL;
		mMinHoleSize = 0.01f;
		mMaxHollow = 0.99f;
	}
}

void HippoLimits::setWhiteCoreLimits()
{
	mMaxPrimScale = 8192.0f;
	mMinPrimScale = 0.001f;
	mMaxHeight = 10000.0f;
	mMinHoleSize = 0.001f;
	mMaxHollow = 99.0f;
}

void HippoLimits::setSecondLifeLimits()
{
	LL_INFOS() << "Using Second Life limits." << LL_ENDL;

	mMinPrimScale = 0.01f;
	mMaxHeight = 4096.0f;
	mMinHoleSize = 0.05f;
	mMaxHollow = 0.95f;
	mMaxPrimScale = 64.0f;
}

