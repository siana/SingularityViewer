/**
 * @file llfloaterstats.cpp
 * @brief Container for statistics view
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



#include "llviewerprecompiledheaders.h"

#include "llfloaterstats.h"
#include "llcontainerview.h"
#include "llfloater.h"
#include "llstatview.h"
#include "llscrollcontainer.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewerstats.h"
#include "pipeline.h"
#include "llviewerobjectlist.h"
#include "llviewertexturelist.h"
#include "lltexturefetch.h"
#include "sgmemstat.h"

const S32 LL_SCROLL_BORDER = 1;

void LLFloaterStats::buildStats()
{
	LLRect rect;

	//
	// Viewer advanced stats
	//
	LLStatView* stat_viewp = NULL;

	//
	// Viewer Basic
	//
	LLStatView::Params params;
	params.name("basic stat view");
	params.show_label(true);
	params.label("Basic");
	params.setting("OpenDebugStatBasic");
	params.rect(rect);

	stat_viewp = LLUICtrlFactory::create<LLStatView>(params);
	addStatView(stat_viewp);

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " fps";
		params.mMinBar = 0.f;
		params.mMaxBar = 60.f;
		params.mTickSpacing = 3.f;
		params.mLabelSpacing = 15.f;
		params.mPrecision = 1;
		stat_viewp->addStat("FPS", &(LLViewerStats::getInstance()->mFPSStat), params, "DebugStatModeFPS", TRUE, TRUE);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = 900.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 300.f;
		stat_viewp->addStat("Bandwidth", &(LLViewerStats::getInstance()->mKBitStat), params, "DebugStatModeBandwidth", TRUE, FALSE);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " %";
		params.mMinBar = 0.f;
		params.mMaxBar = 5.f;
		params.mTickSpacing = 1.f;
		params.mLabelSpacing = 1.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = TRUE;
		params.mDisplayMean = 1;
		stat_viewp->addStat("Packet Loss", &(LLViewerStats::getInstance()->mPacketsLostPercentStat), params, "DebugStatModePacketLoss");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " msec";
		params.mMinBar = 0.f;
		params.mMaxBar = 1000.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		stat_viewp->addStat("Ping Sim", &(LLViewerStats::getInstance()->mSimPingStat), params, "DebugStatMode");
	}

	if (SGMemStat::haveStat()) {
		LLStatBar::Parameters params;
		params.mUnitLabel = " MB";
		params.mMinBar = 0.f;
		params.mMaxBar = 2048.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 512.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		stat_viewp->addStat("Allocated memory", &(LLViewerStats::getInstance()->mMallocStat), params, "DebugStatModeMalloc");
	}

	params.name("advanced stat view");
	params.show_label(true);
	params.label("Advanced");
	params.setting("OpenDebugStatAdvanced");
	params.rect(rect);
	stat_viewp = LLUICtrlFactory::create<LLStatView>(params);
	addStatView(stat_viewp);

	params.name("render stat view");
	params.show_label(true);
	params.label("Render");
	params.setting("OpenDebugStatRender");
	params.rect(rect);

	LLStatView* render_statviewp = stat_viewp->addStatView(params);

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "/fr";
		params.mMinBar = 0.f;
		params.mMaxBar = 3000.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 500.f;
		params.mPrecision = 1;
		params.mPerSec = FALSE;
		render_statviewp->addStat("KTris Drawn", &(LLViewerStats::getInstance()->mTrianglesDrawnStat), params, "DebugStatModeKTrisDrawnFr");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "/sec";
		params.mMinBar = 0.f;
		params.mMaxBar = 100000.f;
		params.mTickSpacing = 4000.f;
		params.mLabelSpacing = 20000.f;
		params.mPrecision = 1;
		render_statviewp->addStat("KTris Drawn", &(LLViewerStats::getInstance()->mTrianglesDrawnStat), params, "DebugStatModeKTrisDrawnSec");
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 10000.f;
		params.mTickSpacing = 2500.f;
		params.mLabelSpacing = 5000.f;
		params.mPerSec = FALSE;
		render_statviewp->addStat("Total Objs", &(LLViewerStats::getInstance()->mNumObjectsStat), params, "DebugStatModeTotalObjs");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "/sec";
		params.mMinBar = 0.f;
		params.mMaxBar = 1000.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 500.f;
		params.mPerSec = TRUE;
		render_statviewp->addStat("New Objs", &(LLViewerStats::getInstance()->mNumNewObjectsStat), params, "DebugStatModeNewObjs");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "%";
		params.mMinBar = 0.f;
		params.mMaxBar = 100.f;
		params.mTickSpacing = 20.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		render_statviewp->addStat("Object Cache Hit Rate", &(LLViewerStats::getInstance()->mNumNewObjectsStat), params, std::string(), false, true);
	}

	// Texture statistics
	params.name("texture stat view");
	params.show_label(true);
	params.label("Texture");
	params.setting("OpenDebugStatTexture");
	params.rect(rect);
	LLStatView* texture_statviewp = render_statviewp->addStatView(params);

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "%";
		params.mMinBar = 0.f;
		params.mMaxBar = 100.f;
		params.mTickSpacing = 20.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		texture_statviewp->addStat("Cache Hit Rate", &(LLTextureFetch::sCacheHitRate), params, std::string(), false, true);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "msec";
		params.mMinBar = 0.f;
		params.mMaxBar = 1000.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		texture_statviewp->addStat("Cache Read Latency", &(LLTextureFetch::sCacheReadLatency), params, std::string(), false, true);
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 8000.f;
		params.mTickSpacing = 2000.f;
		params.mLabelSpacing = 4000.f;
		params.mPerSec = FALSE;
		texture_statviewp->addStat("Count", &(LLViewerStats::getInstance()->mNumImagesStat), params, "DebugStatModeTextureCount");
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 8000.f;
		params.mTickSpacing = 2000.f;
		params.mLabelSpacing = 4000.f;
		params.mPerSec = FALSE;
		texture_statviewp->addStat("Raw Count", &(LLViewerStats::getInstance()->mNumRawImagesStat), params, "DebugStatModeRawCount");
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 400.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPrecision = 1;
		params.mPerSec = FALSE;
		texture_statviewp->addStat("GL Mem", &(LLViewerStats::getInstance()->mGLTexMemStat), params, "DebugStatModeGLMem");
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 400.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPrecision = 1;
		params.mPerSec = FALSE;
		texture_statviewp->addStat("Formatted Mem", &(LLViewerStats::getInstance()->mFormattedMemStat), params, "DebugStatModeFormattedMem");
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 400.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPrecision = 1;
		params.mPerSec = FALSE;
		texture_statviewp->addStat("Raw Mem", &(LLViewerStats::getInstance()->mRawMemStat), params, "DebugStatModeRawMem");
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 400.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPrecision = 1;
		params.mPerSec = FALSE;
		texture_statviewp->addStat("Bound Mem", &(LLViewerStats::getInstance()->mGLBoundMemStat), params, "DebugStatModeBoundMem");
	}

	// Network statistics
	params.name("network stat view");
	params.show_label(true);
	params.label("Network");
	params.setting("OpenDebugStatNet");
	params.rect(rect);
	LLStatView* net_statviewp = stat_viewp->addStatView(params);

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "/sec";
		net_statviewp->addStat("UDP Packets In", &(LLViewerStats::getInstance()->mPacketsInStat), params, "DebugStatModePacketsIn");

	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "/sec";
		net_statviewp->addStat("UDP Packets Out", &(LLViewerStats::getInstance()->mPacketsOutStat), params, "DebugStatModePacketsOut");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = gSavedSettings.getF32("HTTPThrottleBandwidth");
		params.mMaxBar *= llclamp(2.0 - (params.mMaxBar - 400.f) / 3600.f, 1.0, 2.0);	// Allow for overshoot (allow more for low bandwidth values).
		params.mTickSpacing = 1.f;
		while (params.mTickSpacing < params.mMaxBar / 8)
			params.mTickSpacing *= 2.f;
		params.mLabelSpacing = 2 * params.mTickSpacing;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		net_statviewp->addStat("HTTP Textures", &(LLViewerStats::getInstance()->mHTTPTextureKBitStat), params, "DebugStatModeHTTPTexture");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = 1024.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 256.f;
		net_statviewp->addStat("UDP Textures", &(LLViewerStats::getInstance()->mUDPTextureKBitStat), params, "DebugStatModeUDPTexture");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = 1024.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 256.f;
		net_statviewp->addStat("Objects (UDP)", &(LLViewerStats::getInstance()->mObjectKBitStat), params, "DebugStatModeObjects");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = 1024.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 256.f;
		net_statviewp->addStat("Assets (UDP)", &(LLViewerStats::getInstance()->mAssetKBitStat), params, "DebugStatModeAsset");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = 1024.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 256.f;
		net_statviewp->addStat("Layers (UDP)", &(LLViewerStats::getInstance()->mLayersKBitStat), params, "DebugStatModeLayers");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = 1024.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 256.f;
		net_statviewp->addStat("Actual In (UDP)", &(LLViewerStats::getInstance()->mActualInKBitStat), params, "DebugStatModeActualIn", TRUE, FALSE);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kbps";
		params.mMinBar = 0.f;
		params.mMaxBar = 512.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 256.f;
		net_statviewp->addStat("Actual Out (UDP)", &(LLViewerStats::getInstance()->mActualOutKBitStat), params, "DebugStatModeActualOut", TRUE, FALSE);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " ";
		params.mPerSec = FALSE;
		net_statviewp->addStat("VFS Pending Ops", &(LLViewerStats::getInstance()->mVFSPendingOperations), params, "DebugStatModeVFSPendingOps");
	}

	// Simulator stats
	params.name("sim stat view");
	params.show_label(true);
	params.label("Simulator");
	params.setting("OpenDebugStatSim");
	params.rect(rect);
	LLStatView* sim_statviewp = LLUICtrlFactory::create<LLStatView>(params);
	addStatView(sim_statviewp);

	{
		LLStatBar::Parameters params;
		params.mPrecision = 2;
		params.mMinBar = 0.f;
		params.mMaxBar = 1.f;
		params.mTickSpacing = 0.25f;
		params.mLabelSpacing = 0.5f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Time Dilation", &(LLViewerStats::getInstance()->mSimTimeDilation), params, "DebugStatModeTimeDialation");
	}

	{
		LLStatBar::Parameters params;
		params.mMinBar = 0.f;
		params.mMaxBar = 200.f;
		params.mTickSpacing = 20.f;
		params.mLabelSpacing = 100.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Sim FPS", &(LLViewerStats::getInstance()->mSimFPS), params, "DebugStatModeSimFPS");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 66.f;
		params.mTickSpacing = 33.f;
		params.mLabelSpacing = 33.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Physics FPS", &(LLViewerStats::getInstance()->mSimPhysicsFPS), params, "DebugStatModePhysicsFPS");
	}

	params.name("phys detail view");
	params.show_label(true);
	params.label("Physics Details");
	params.setting("OpenDebugStatPhysicsDetails");
	params.rect(rect);
	LLStatView* phys_details_viewp = sim_statviewp->addStatView(params);

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 500.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 40.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		phys_details_viewp->addStat("Pinned Objects", &(LLViewerStats::getInstance()->mPhysicsPinnedTasks), params, "DebugStatModePinnedObjects");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 500.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 40.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		phys_details_viewp->addStat("Low LOD Objects", &(LLViewerStats::getInstance()->mPhysicsLODTasks), params, "DebugStatModeLowLODObjects");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " MB";
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 1024.f;
		params.mTickSpacing = 128.f;
		params.mLabelSpacing = 256.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		phys_details_viewp->addStat("Memory Allocated", &(LLViewerStats::getInstance()->mPhysicsMemoryAllocated), params, "DebugStatModeMemoryAllocated");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 100.f;
		params.mTickSpacing = 25.f;
		params.mLabelSpacing = 50.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Agent Updates/Sec", &(LLViewerStats::getInstance()->mSimAgentUPS), params, "DebugStatModeAgentUpdatesSec");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 80.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 40.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Main Agents", &(LLViewerStats::getInstance()->mSimMainAgents), params, "DebugStatModeMainAgents");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 5.f;
		params.mLabelSpacing = 10.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Child Agents", &(LLViewerStats::getInstance()->mSimChildAgents), params, "DebugStatModeChildAgents");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 30000.f;
		params.mTickSpacing = 5000.f;
		params.mLabelSpacing = 10000.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Objects", &(LLViewerStats::getInstance()->mSimObjects), params, "DebugStatModeSimObjects");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 800.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Active Objects", &(LLViewerStats::getInstance()->mSimActiveObjects), params, "DebugStatModeSimActiveObjects");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 800.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Active Scripts", &(LLViewerStats::getInstance()->mSimActiveScripts), params, "DebugStatModeSimActiveScripts");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "%";
		params.mPrecision = 3;
		params.mMinBar = 0.f;
		params.mMaxBar = 100.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		sim_statviewp->addStat("Scripts Run", &(LLViewerStats::getInstance()->mSimPctScriptsRun), params, std::string(), false, true);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " eps";
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 20000.f;
		params.mTickSpacing = 2500.f;
		params.mLabelSpacing = 5000.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Script Events", &(LLViewerStats::getInstance()->mSimScriptEPS), params, "DebugStatModeSimScriptEvents");
	}

	params.name("pathfinding view");
	params.show_label(true);
	params.label("Pathfinding Details");
	params.rect(rect);
	LLStatView* pathfinding_viewp = sim_statviewp->addStatView(params);

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "ms";
		params.mPrecision = 3;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		pathfinding_viewp->addStat("AI Step Time", &(LLViewerStats::getInstance()->mSimSimAIStepMsec), params);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "/sec";
		params.mMinBar = 0.f;
		params.mMaxBar = 45.f;
		params.mTickSpacing = 4.f;
		params.mLabelSpacing = 8.f;
		params.mPrecision = 0;
		render_statviewp->addStat("Skipped Silhouette Steps", &(LLViewerStats::getInstance()->mSimSimSkippedSilhouetteSteps), params);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "%";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 100.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = TRUE;
		pathfinding_viewp->addStat("Characters Updated", &(LLViewerStats::getInstance()->mSimSimPctSteppedCharacters), params);
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " pps";
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 2000.f;
		params.mTickSpacing = 250.f;
		params.mLabelSpacing = 1000.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Packets In", &(LLViewerStats::getInstance()->mSimInPPS), params, "DebugStatModeSimInPPS");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " pps";
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 2000.f;
		params.mTickSpacing = 250.f;
		params.mLabelSpacing = 1000.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Packets Out", &(LLViewerStats::getInstance()->mSimOutPPS), params, "DebugStatModeSimOutPPS");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 800.f;
		params.mTickSpacing = 100.f;
		params.mLabelSpacing = 200.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Pending Downloads", &(LLViewerStats::getInstance()->mSimPendingDownloads), params, "DebugStatModeSimPendingDownloads");
	}

	{
		LLStatBar::Parameters params;
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 100.f;
		params.mTickSpacing = 25.f;
		params.mLabelSpacing = 50.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Pending Uploads", &(LLViewerStats::getInstance()->mSimPendingUploads), params, "SimPendingUploads");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = " kb";
		params.mPrecision = 0;
		params.mMinBar = 0.f;
		params.mMaxBar = 100000.f;
		params.mTickSpacing = 25000.f;
		params.mLabelSpacing = 50000.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_statviewp->addStat("Total Unacked Bytes", &(LLViewerStats::getInstance()->mSimTotalUnackedBytes), params, "DebugStatModeSimTotalUnackedBytes");
	}

	params.name("sim perf view");
	params.show_label(true);
	params.label("Time (ms)");
	params.setting("OpenDebugStatSimTime");
	params.rect(rect);
	LLStatView* sim_time_viewp = sim_statviewp->addStatView(params);

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Total Frame Time", &(LLViewerStats::getInstance()->mSimFrameMsec), params, "DebugStatModeSimFrameMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Net Time", &(LLViewerStats::getInstance()->mSimNetMsec), params, "DebugStatModeSimNetMsec");
	}

	{
		LLStatBar::Parameters params; 
		params.mUnitLabel = "ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Physics Time", &(LLViewerStats::getInstance()->mSimSimPhysicsMsec), params, "DebugStatModeSimSimPhysicsMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Simulation Time", &(LLViewerStats::getInstance()->mSimSimOtherMsec), params, "DebugStatModeSimSimOtherMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Agent Time", &(LLViewerStats::getInstance()->mSimAgentMsec), params, "DebugStatModeSimAgentMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Images Time", &(LLViewerStats::getInstance()->mSimImagesMsec), params, "DebugStatModeSimImagesMsec");
	}

	{
		LLStatBar::Parameters params; 
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Script Time", &(LLViewerStats::getInstance()->mSimScriptMsec), params, "DebugStatModeSimScriptMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel = "ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		sim_time_viewp->addStat("Spare Time", &(LLViewerStats::getInstance()->mSimSpareMsec), params, "DebugStatModeSimSpareMsec");
	}
	
	// 2nd level time blocks under 'Details' second
	params.name("sim perf view");
	params.show_label(true);
	params.label("Time (ms)");
	params.setting("OpenDebugStatSimTimeDetails");
	params.rect(rect);
	LLStatView *detailed_time_viewp = sim_time_viewp->addStatView(params);

	{
	LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		detailed_time_viewp->addStat("  Physics Step", &(LLViewerStats::getInstance()->mSimSimPhysicsStepMsec), params, "DebugStatModeSimSimPhysicsStepMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		detailed_time_viewp->addStat("  Update Physics Shapes", &(LLViewerStats::getInstance()->mSimSimPhysicsShapeUpdateMsec), params, "DebugStatModeSimSimPhysicsShapeUpdateMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		detailed_time_viewp->addStat("  Physics Other", &(LLViewerStats::getInstance()->mSimSimPhysicsOtherMsec), params, "DebugStatModeSimSimPhysicsOtherMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		detailed_time_viewp->addStat("  Sleep Time", &(LLViewerStats::getInstance()->mSimSleepMsec), params, "DebugStatModeSimSleepMsec");
	}

	{
		LLStatBar::Parameters params;
		params.mUnitLabel ="ms";
		params.mPrecision = 1;
		params.mMinBar = 0.f;
		params.mMaxBar = 40.f;
		params.mTickSpacing = 10.f;
		params.mLabelSpacing = 20.f;
		params.mPerSec = FALSE;
		params.mDisplayMean = FALSE;
		detailed_time_viewp->addStat("  Pump IO", &(LLViewerStats::getInstance()->mSimPumpIOMsec), params, "DebugStatModeSimPumpIOMsec");
	}

	LLRect r = getRect();

	// Reshape based on the parameters we set.
	reshape(r.getWidth(), r.getHeight());
}


LLFloaterStats::LLFloaterStats(const LLSD& val)
	:   LLFloater("floater_stats"),
		mStatsContainer(NULL),
		mScrollContainer(NULL)

{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_statistics.xml", NULL, FALSE);
	
	LLRect stats_rect(0, getRect().getHeight() - LLFLOATER_HEADER_SIZE,
					  getRect().getWidth() - LLFLOATER_CLOSE_BOX_SIZE, 0);

	LLContainerView::Params rvp;
	rvp.name("statistics_view");
	rvp.rect(stats_rect);
	mStatsContainer = LLUICtrlFactory::create<LLContainerView>(rvp);

	LLRect scroll_rect(LL_SCROLL_BORDER, getRect().getHeight() - LLFLOATER_HEADER_SIZE - LL_SCROLL_BORDER,
					   getRect().getWidth() - LL_SCROLL_BORDER, LL_SCROLL_BORDER);
		mScrollContainer = new LLScrollContainer(std::string("statistics_scroll"), scroll_rect, mStatsContainer);
	mScrollContainer->setFollowsAll();
	mScrollContainer->setReserveScrollCorner(TRUE);

	mStatsContainer->setScrollContainer(mScrollContainer);
	
	addChild(mScrollContainer);

	buildStats();
}


LLFloaterStats::~LLFloaterStats()
{
}

void LLFloaterStats::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	if (mStatsContainer)
	{
		LLRect rect = mStatsContainer->getRect();

		mStatsContainer->reshape(rect.getWidth() - 2, rect.getHeight(), TRUE);
	}

	LLFloater::reshape(width, height, called_from_parent);
}


void LLFloaterStats::addStatView(LLStatView* stat)
{
	mStatsContainer->addChildInBack(stat);
}

// virtual
void LLFloaterStats::onOpen()
{
	LLFloater::onOpen();
	gSavedSettings.setBOOL("ShowDebugStats", TRUE);
	reshape(getRect().getWidth(), getRect().getHeight());
}

void LLFloaterStats::onClose(bool app_quitting)
{
	setVisible(FALSE);
	if (!app_quitting)
	{
		gSavedSettings.setBOOL("ShowDebugStats", FALSE);
	}
}
