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
	} else {
		setOpenSimLimits();
	}
}


void HippoLimits::setOpenSimLimits()
{
	mMaxAgentGroups = gHippoGridManager->getConnectedGrid()->getMaxAgentGroups();
	if (mMaxAgentGroups < 0) mMaxAgentGroups = 50;
	mMaxPrimScale = 256.0f;
	mMaxHeight = 10000.0f;
	if (gHippoGridManager->getConnectedGrid()->isRenderCompat()) {
		llinfos << "Using rendering compatible OpenSim limits." << llendl;
		mMinHoleSize = 0.05f;
		mMaxHollow = 0.95f;
	} else {
		llinfos << "Using Hippo OpenSim limits." << llendl;
		mMinHoleSize = 0.01f;
		mMaxHollow = 0.99f;
	}
}

void HippoLimits::setSecondLifeLimits()
{
	llinfos << "Using Second Life limits." << llendl;
	
	if (gHippoGridManager->getConnectedGrid())
	
	//KC: new server defined max groups
	mMaxAgentGroups = gHippoGridManager->getConnectedGrid()->getMaxAgentGroups();
	if (mMaxAgentGroups <= 0)
	{
		mMaxAgentGroups = DEFAULT_MAX_AGENT_GROUPS;
	}
	
	mMaxHeight = 4096.0f;
	mMinHoleSize = 0.05f;
	mMaxHollow = 0.95f;
	mMaxPrimScale = 64.0f;
}

