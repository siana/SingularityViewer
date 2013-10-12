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
	LLStatBar *stat_barp;

	//
	// Viewer advanced stats
	//
	LLStatView *stat_viewp = NULL;

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

	stat_barp = stat_viewp->addStat("FPS", &(LLViewerStats::getInstance()->mFPSStat),
									"DebugStatModeFPS", TRUE, TRUE);
	stat_barp->setUnitLabel(" fps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 60.f;
	stat_barp->mTickSpacing = 3.f;
	stat_barp->mLabelSpacing = 15.f;
	stat_barp->mPrecision = 1;

	stat_barp = stat_viewp->addStat("Bandwidth", &(LLViewerStats::getInstance()->mKBitStat),
									"DebugStatModeBandwidth", TRUE, FALSE);
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 900.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 300.f;

	stat_barp = stat_viewp->addStat("Packet Loss", &(LLViewerStats::getInstance()->mPacketsLostPercentStat), "DebugStatModePacketLoss");
	stat_barp->setUnitLabel(" %");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 5.f;
	stat_barp->mTickSpacing = 1.f;
	stat_barp->mLabelSpacing = 1.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = TRUE;
	stat_barp->mPrecision = 1;

	stat_barp = stat_viewp->addStat("Ping Sim", &(LLViewerStats::getInstance()->mSimPingStat), "DebugStatMode");
	stat_barp->setUnitLabel(" msec");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1000.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;
	
	if(SGMemStat::haveStat()) {
		stat_barp = stat_viewp->addStat("Allocated memory", &(LLViewerStats::getInstance()->mMallocStat), "DebugStatModeMalloc");
		stat_barp->setUnitLabel(" MB");
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 2048.f;
		stat_barp->mTickSpacing = 128.f;
		stat_barp->mLabelSpacing = 512.f;
		stat_barp->mPerSec = FALSE;
		stat_barp->mDisplayMean = FALSE;
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
	LLStatView *render_statviewp = stat_viewp->addStatView(params);

	stat_barp = render_statviewp->addStat("KTris Drawn", &(LLViewerStats::getInstance()->mTrianglesDrawnStat), "DebugStatModeKTrisDrawnFr");
	stat_barp->setUnitLabel("/fr");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 3000.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 500.f;
	stat_barp->mPrecision = 1;
	stat_barp->mPerSec = FALSE;

	stat_barp = render_statviewp->addStat("KTris Drawn", &(LLViewerStats::getInstance()->mTrianglesDrawnStat), "DebugStatModeKTrisDrawnSec");
	stat_barp->setUnitLabel("/sec");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100000.f;
	stat_barp->mTickSpacing = 4000.f;
	stat_barp->mLabelSpacing = 20000.f;
	stat_barp->mPrecision = 1;

	stat_barp = render_statviewp->addStat("Total Objs", &(LLViewerStats::getInstance()->mNumObjectsStat), "DebugStatModeTotalObjs");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 10000.f;
	stat_barp->mTickSpacing = 2500.f;
	stat_barp->mLabelSpacing = 5000.f;
	stat_barp->mPerSec = FALSE;

	stat_barp = render_statviewp->addStat("New Objs", &(LLViewerStats::getInstance()->mNumNewObjectsStat), "DebugStatModeNewObjs");
	stat_barp->setUnitLabel("/sec");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1000.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 500.f;
	stat_barp->mPerSec = TRUE;

	stat_barp = render_statviewp->addStat("Object Cache Hit Rate", &(LLViewerStats::getInstance()->mNumNewObjectsStat), std::string(), false, true);
	stat_barp->setUnitLabel("%");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 20.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;	

	// Texture statistics
	params.name("texture stat view");
	params.show_label(true);
	params.label("Texture");
	params.setting("OpenDebugStatTexture");
	params.rect(rect);
	LLStatView *texture_statviewp = render_statviewp->addStatView(params);

	stat_barp = texture_statviewp->addStat("Cache Hit Rate", &(LLTextureFetch::sCacheHitRate), std::string(), false, true);
	stat_barp->setUnitLabel("%");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 20.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;	

	stat_barp = texture_statviewp->addStat("Cache Read Latency", &(LLTextureFetch::sCacheReadLatency), std::string(), false, true);
	stat_barp->setUnitLabel("msec");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1000.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = texture_statviewp->addStat("Count", &(LLViewerStats::getInstance()->mNumImagesStat), "DebugStatModeTextureCount");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 8000.f;
	stat_barp->mTickSpacing = 2000.f;
	stat_barp->mLabelSpacing = 4000.f;
	stat_barp->mPerSec = FALSE;

	stat_barp = texture_statviewp->addStat("Raw Count", &(LLViewerStats::getInstance()->mNumRawImagesStat), "DebugStatModeRawCount");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 8000.f;
	stat_barp->mTickSpacing = 2000.f;
	stat_barp->mLabelSpacing = 4000.f;
	stat_barp->mPerSec = FALSE;

	stat_barp = texture_statviewp->addStat("GL Mem", &(LLViewerStats::getInstance()->mGLTexMemStat), "DebugStatModeGLMem");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 400.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPrecision = 1;
	stat_barp->mPerSec = FALSE;

	stat_barp = texture_statviewp->addStat("Formatted Mem", &(LLViewerStats::getInstance()->mFormattedMemStat), "DebugStatModeFormattedMem");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 400.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPrecision = 1;
	stat_barp->mPerSec = FALSE;

	stat_barp = texture_statviewp->addStat("Raw Mem", &(LLViewerStats::getInstance()->mRawMemStat), "DebugStatModeRawMem");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 400.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPrecision = 1;
	stat_barp->mPerSec = FALSE;

	stat_barp = texture_statviewp->addStat("Bound Mem", &(LLViewerStats::getInstance()->mGLBoundMemStat), "DebugStatModeBoundMem");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 400.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPrecision = 1;
	stat_barp->mPerSec = FALSE;
	
	// Network statistics
	params.name("network stat view");
	params.show_label(true);
	params.label("Network");
	params.setting("OpenDebugStatNet");
	params.rect(rect);
	LLStatView *net_statviewp = stat_viewp->addStatView(params);

	stat_barp = net_statviewp->addStat("UDP Packets In", &(LLViewerStats::getInstance()->mPacketsInStat), "DebugStatModePacketsIn");
	stat_barp->setUnitLabel("/sec");

	stat_barp = net_statviewp->addStat("UDP Packets Out", &(LLViewerStats::getInstance()->mPacketsOutStat), "DebugStatModePacketsOut");
	stat_barp->setUnitLabel("/sec");

	stat_barp = net_statviewp->addStat("HTTP Textures", &(LLViewerStats::getInstance()->mHTTPTextureKBitStat), "DebugStatModeHTTPTexture");
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = gSavedSettings.getF32("HTTPThrottleBandwidth");
	stat_barp->mMaxBar *= llclamp(2.0 - (stat_barp->mMaxBar - 400.f) / 3600.f, 1.0, 2.0);	// Allow for overshoot (allow more for low bandwidth values).
	stat_barp->mTickSpacing = 1.f;
	while (stat_barp->mTickSpacing < stat_barp->mMaxBar / 8)
	  stat_barp->mTickSpacing *= 2.f;
	stat_barp->mLabelSpacing = 2 * stat_barp->mTickSpacing;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = net_statviewp->addStat("UDP Textures", &(LLViewerStats::getInstance()->mUDPTextureKBitStat), "DebugStatModeUDPTexture");
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1024.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	stat_barp = net_statviewp->addStat("Objects (UDP)", &(LLViewerStats::getInstance()->mObjectKBitStat), "DebugStatModeObjects");
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1024.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	stat_barp = net_statviewp->addStat("Assets (UDP)", &(LLViewerStats::getInstance()->mAssetKBitStat), "DebugStatModeAsset");
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1024.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	stat_barp = net_statviewp->addStat("Layers (UDP)", &(LLViewerStats::getInstance()->mLayersKBitStat), "DebugStatModeLayers");
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1024.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	stat_barp = net_statviewp->addStat("Actual In (UDP)", &(LLViewerStats::getInstance()->mActualInKBitStat),
									   "DebugStatModeActualIn", TRUE, FALSE);
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1024.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	stat_barp = net_statviewp->addStat("Actual Out (UDP)", &(LLViewerStats::getInstance()->mActualOutKBitStat),
									   "DebugStatModeActualOut", TRUE, FALSE);
	stat_barp->setUnitLabel(" kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 512.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	stat_barp = net_statviewp->addStat("VFS Pending Ops", &(LLViewerStats::getInstance()->mVFSPendingOperations),
									   "DebugStatModeVFSPendingOps");
	stat_barp->setUnitLabel(" ");
	stat_barp->mPerSec = FALSE;


	// Simulator stats
	params.name("sim stat view");
	params.show_label(true);
	params.label("Simulator");
	params.setting("OpenDebugStatSim");
	params.rect(rect);
	LLStatView *sim_statviewp = LLUICtrlFactory::create<LLStatView>(params);
	addStatView(sim_statviewp);

	stat_barp = sim_statviewp->addStat("Time Dilation", &(LLViewerStats::getInstance()->mSimTimeDilation), "DebugStatModeTimeDialation");
	stat_barp->mPrecision = 2;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1.f;
	stat_barp->mTickSpacing = 0.25f;
	stat_barp->mLabelSpacing = 0.5f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Sim FPS", &(LLViewerStats::getInstance()->mSimFPS), "DebugStatModeSimFPS");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 200.f;
	stat_barp->mTickSpacing = 20.f;
	stat_barp->mLabelSpacing = 100.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Physics FPS", &(LLViewerStats::getInstance()->mSimPhysicsFPS), "DebugStatModePhysicsFPS");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 66.f;
	stat_barp->mTickSpacing = 33.f;
	stat_barp->mLabelSpacing = 33.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	params.name("phys detail view");
	params.show_label(true);
	params.label("Physics Details");
	params.setting("OpenDebugStatPhysicsDetails");
	params.rect(rect);
	LLStatView *phys_details_viewp = sim_statviewp->addStatView(params);

	stat_barp = phys_details_viewp->addStat("Pinned Objects", &(LLViewerStats::getInstance()->mPhysicsPinnedTasks), "DebugStatModePinnedObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 500.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 40.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = phys_details_viewp->addStat("Low LOD Objects", &(LLViewerStats::getInstance()->mPhysicsLODTasks), "DebugStatModeLowLODObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 500.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 40.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = phys_details_viewp->addStat("Memory Allocated", &(LLViewerStats::getInstance()->mPhysicsMemoryAllocated), "DebugStatModeMemoryAllocated");
	stat_barp->setUnitLabel(" MB");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1024.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Agent Updates/Sec", &(LLViewerStats::getInstance()->mSimAgentUPS), "DebugStatModeAgentUpdatesSec");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 25.f;
	stat_barp->mLabelSpacing = 50.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Main Agents", &(LLViewerStats::getInstance()->mSimMainAgents), "DebugStatModeMainAgents");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 80.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 40.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Child Agents", &(LLViewerStats::getInstance()->mSimChildAgents), "DebugStatModeChildAgents");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 5.f;
	stat_barp->mLabelSpacing = 10.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Objects", &(LLViewerStats::getInstance()->mSimObjects), "DebugStatModeSimObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 30000.f;
	stat_barp->mTickSpacing = 5000.f;
	stat_barp->mLabelSpacing = 10000.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Active Objects", &(LLViewerStats::getInstance()->mSimActiveObjects), "DebugStatModeSimActiveObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 800.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Active Scripts", &(LLViewerStats::getInstance()->mSimActiveScripts), "DebugStatModeSimActiveScripts");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 800.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Scripts Run", &(LLViewerStats::getInstance()->mSimPctScriptsRun), std::string(), false, true);
	stat_barp->setUnitLabel("%");
	stat_barp->mPrecision = 3;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;	

	stat_barp = sim_statviewp->addStat("Script Events", &(LLViewerStats::getInstance()->mSimScriptEPS), "DebugStatModeSimScriptEvents");
	stat_barp->setUnitLabel(" eps");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20000.f;
	stat_barp->mTickSpacing = 2500.f;
	stat_barp->mLabelSpacing = 5000.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	params.name("pathfinding view");
	params.show_label(true);
	params.label("Pathfinding Details");
	params.rect(rect);
	LLStatView *pathfinding_viewp = sim_statviewp->addStatView(params);

	stat_barp = pathfinding_viewp->addStat("AI Step Time", &(LLViewerStats::getInstance()->mSimSimAIStepMsec));
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 3;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = render_statviewp->addStat("Skipped Silhouette Steps", &(LLViewerStats::getInstance()->mSimSimSkippedSilhouetteSteps));
	stat_barp->setUnitLabel("/sec");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 45.f;
	stat_barp->mTickSpacing = 4.f;
	stat_barp->mLabelSpacing = 8.f;
	stat_barp->mPrecision = 0;

	stat_barp = pathfinding_viewp->addStat("Characters Updated", &(LLViewerStats::getInstance()->mSimSimPctSteppedCharacters));
	stat_barp->setUnitLabel("%");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = TRUE;

	stat_barp = sim_statviewp->addStat("Packets In", &(LLViewerStats::getInstance()->mSimInPPS), "DebugStatModeSimInPPS");
	stat_barp->setUnitLabel(" pps");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 2000.f;
	stat_barp->mTickSpacing = 250.f;
	stat_barp->mLabelSpacing = 1000.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Packets Out", &(LLViewerStats::getInstance()->mSimOutPPS), "DebugStatModeSimOutPPS");
	stat_barp->setUnitLabel(" pps");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 2000.f;
	stat_barp->mTickSpacing = 250.f;
	stat_barp->mLabelSpacing = 1000.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Pending Downloads", &(LLViewerStats::getInstance()->mSimPendingDownloads), "DebugStatModeSimPendingDownloads");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 800.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Pending Uploads", &(LLViewerStats::getInstance()->mSimPendingUploads), "SimPendingUploads");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 25.f;
	stat_barp->mLabelSpacing = 50.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_statviewp->addStat("Total Unacked Bytes", &(LLViewerStats::getInstance()->mSimTotalUnackedBytes), "DebugStatModeSimTotalUnackedBytes");
	stat_barp->setUnitLabel(" kb");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100000.f;
	stat_barp->mTickSpacing = 25000.f;
	stat_barp->mLabelSpacing = 50000.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	params.name("sim perf view");
	params.show_label(true);
	params.label("Time (ms)");
	params.setting("OpenDebugStatSimTime");
	params.rect(rect);
	LLStatView *sim_time_viewp = sim_statviewp->addStatView(params);

	stat_barp = sim_time_viewp->addStat("Total Frame Time", &(LLViewerStats::getInstance()->mSimFrameMsec), "DebugStatModeSimFrameMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_time_viewp->addStat("Net Time", &(LLViewerStats::getInstance()->mSimNetMsec), "DebugStatModeSimNetMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_time_viewp->addStat("Physics Time", &(LLViewerStats::getInstance()->mSimSimPhysicsMsec), "DebugStatModeSimSimPhysicsMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_time_viewp->addStat("Simulation Time", &(LLViewerStats::getInstance()->mSimSimOtherMsec), "DebugStatModeSimSimOtherMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_time_viewp->addStat("Agent Time", &(LLViewerStats::getInstance()->mSimAgentMsec), "DebugStatModeSimAgentMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_time_viewp->addStat("Images Time", &(LLViewerStats::getInstance()->mSimImagesMsec), "DebugStatModeSimImagesMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_time_viewp->addStat("Script Time", &(LLViewerStats::getInstance()->mSimScriptMsec), "DebugStatModeSimScriptMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	stat_barp = sim_time_viewp->addStat("Spare Time", &(LLViewerStats::getInstance()->mSimSpareMsec), "DebugStatModeSimSpareMsec");
	stat_barp->setUnitLabel("ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = FALSE;
	stat_barp->mDisplayMean = FALSE;

	
	// 2nd level time blocks under 'Details' second
	params.name("sim perf view");
	params.show_label(true);
	params.label("Time (ms)");
	params.setting("OpenDebugStatSimTimeDetails");
	params.rect(rect);
	LLStatView *detailed_time_viewp = sim_time_viewp->addStatView(params);
	{
		stat_barp = detailed_time_viewp->addStat("  Physics Step", &(LLViewerStats::getInstance()->mSimSimPhysicsStepMsec), "DebugStatModeSimSimPhysicsStepMsec");
		stat_barp->setUnitLabel("ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 40.f;
		stat_barp->mTickSpacing = 10.f;
		stat_barp->mLabelSpacing = 20.f;
		stat_barp->mPerSec = FALSE;
		stat_barp->mDisplayMean = FALSE;

		stat_barp = detailed_time_viewp->addStat("  Update Physics Shapes", &(LLViewerStats::getInstance()->mSimSimPhysicsShapeUpdateMsec), "DebugStatModeSimSimPhysicsShapeUpdateMsec");
		stat_barp->setUnitLabel("ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 40.f;
		stat_barp->mTickSpacing = 10.f;
		stat_barp->mLabelSpacing = 20.f;
		stat_barp->mPerSec = FALSE;
		stat_barp->mDisplayMean = FALSE;

		stat_barp = detailed_time_viewp->addStat("  Physics Other", &(LLViewerStats::getInstance()->mSimSimPhysicsOtherMsec), "DebugStatModeSimSimPhysicsOtherMsec");
		stat_barp->setUnitLabel("ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 40.f;
		stat_barp->mTickSpacing = 10.f;
		stat_barp->mLabelSpacing = 20.f;
		stat_barp->mPerSec = FALSE;
		stat_barp->mDisplayMean = FALSE;

		stat_barp = detailed_time_viewp->addStat("  Sleep Time", &(LLViewerStats::getInstance()->mSimSleepMsec), "DebugStatModeSimSleepMsec");
		stat_barp->setUnitLabel("ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 40.f;
		stat_barp->mTickSpacing = 10.f;
		stat_barp->mLabelSpacing = 20.f;
		stat_barp->mPerSec = FALSE;
		stat_barp->mDisplayMean = FALSE;

		stat_barp = detailed_time_viewp->addStat("  Pump IO", &(LLViewerStats::getInstance()->mSimPumpIOMsec), "DebugStatModeSimPumpIOMsec");
		stat_barp->setUnitLabel("ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 40.f;
		stat_barp->mTickSpacing = 10.f;
		stat_barp->mLabelSpacing = 20.f;
		stat_barp->mPerSec = FALSE;
		stat_barp->mDisplayMean = FALSE;
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
