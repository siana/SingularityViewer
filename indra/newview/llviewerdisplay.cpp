/** 
 * @file llviewerdisplay.cpp
 * @brief LLViewerDisplay class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llviewerdisplay.h"

#include "llgl.h"
#include "llrender.h"
#include "llglheaders.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llcoord.h"
#include "llcriticaldamp.h"
#include "lldir.h"
#include "lldynamictexture.h"
#include "lldrawpoolalpha.h"
#include "llfeaturemanager.h"
#include "llfirstuse.h"
#include "llframestats.h"
#include "llhudmanager.h"
#include "llimagebmp.h"
#include "llimagegl.h"
#include "lloctree.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llstartup.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "lltooldraganddrop.h"
#include "lltoolpie.h"
#include "lltracker.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvograss.h"
#include "llworld.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llappviewer.h"
#include "llstartup.h"
#include "llviewershadermgr.h"
#include "llfasttimer.h"
#include "llfloatertools.h"
#include "llviewertexturelist.h"
#include "llfocusmgr.h"
#include "llcubemap.h"
#include "llviewerregion.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolavatar.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llpostprocess.h"
#include "sgmemstat.h"

// [RLVa:KB]
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]

extern bool gShiftFrame;

LLPointer<LLViewerTexture> gDisconnectedImagep = NULL;

// used to toggle renderer back on after teleport
const F32 TELEPORT_RENDER_DELAY = 20.f; // Max time a teleport is allowed to take before we raise the curtain
const F32 TELEPORT_ARRIVAL_DELAY = 2.f; // Time to preload the world before raising the curtain after we've actually already arrived.
const F32 TELEPORT_LOCAL_DELAY = 1.0f; // Delay to prevent teleports after starting an in-sim teleport.
BOOL		 gTeleportDisplay = FALSE;
LLFrameTimer gTeleportDisplayTimer;
LLFrameTimer gTeleportArrivalTimer;
F32			 gSavedDrawDistance = 0.0f;
const F32		RESTORE_GL_TIME = 5.f;	// Wait this long while reloading textures before we raise the curtain

BOOL gForceRenderLandFence = FALSE;
BOOL gDisplaySwapBuffers = FALSE;
BOOL gDepthDirty = FALSE;
BOOL gResizeScreenTexture = FALSE;
BOOL gWindowResized = FALSE;
BOOL gSnapshot = FALSE;

U32 gRecentFrameCount = 0; // number of 'recent' frames
LLFrameTimer gRecentFPSTime;
LLFrameTimer gRecentMemoryTime;

// Rendering stuff
void pre_show_depth_buffer();
void post_show_depth_buffer();
void render_ui(F32 zoom_factor = 1.f, int subfield = 0, bool tiling = false);
void render_hud_attachments();
void render_ui_3d();
void render_ui_2d();
void render_disconnected_background();

void display_startup()
{
	if (   !gViewerWindow->getActive()
		|| !gViewerWindow->getWindow()->getVisible() 
		|| gViewerWindow->getWindow()->getMinimized()
		|| gNoRender )
	{
		return; 
	}

	gPipeline.updateGL();

	// Update images?
	//gImageList.updateImages(0.01f);
	LLTexUnit::sWhiteTexture = LLViewerFetchedTexture::sWhiteImagep->getTexName();

	LLGLSDefault gls_default;

	// Required for HTML update in login screen
	static S32 frame_count = 0;

	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();

	if (frame_count++ > 1) // make sure we have rendered a frame first
	{
		LLViewerDynamicTexture::updateAllInstances();
	}

	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();

	gViewerWindow->updateUI(); // Fix ui flicker.

	gGL.syncContextState();
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	LLGLSUIDefault gls_ui;

	gViewerWindow->setup2DRender();
	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

	gGL.color4f(1,1,1,1);
	gViewerWindow->draw();
	gGL.flush();

	LLVertexBuffer::unbind();

	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();

	gViewerWindow->getWindow()->swapBuffers();
	gGL.syncContextState();
	glClear(GL_DEPTH_BUFFER_BIT);
}

void display_update_camera(bool tiling=false)
{
	LL_PUSH_CALLSTACKS();
	// TODO: cut draw distance down if customizing avatar?
	// TODO: cut draw distance on per-parcel basis?

	// Cut draw distance in half when customizing avatar,
	// but on the viewer only.
	F32 final_far = gAgentCamera.mDrawDistance;
    static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if(freeze_time || tiling) //Don't animate clouds and water if tiling!
	{
		LLViewerCamera::getInstance()->setFar(final_far);
		gViewerWindow->setup3DRender();
		LLWorld::getInstance()->setLandFarClip(final_far);
		return;
	}
	if (CAMERA_MODE_CUSTOMIZE_AVATAR == gAgentCamera.getCameraMode())
	{
		final_far *= 0.5f;
	}
	LLViewerCamera::getInstance()->setFar(final_far);
	gViewerWindow->setup3DRender();
	
	// update all the sky/atmospheric/water settings
	LLWLParamManager::getInstance()->update(LLViewerCamera::getInstance());
	LLWaterParamManager::getInstance()->update(LLViewerCamera::getInstance());

	// Update land visibility too
	LLWorld::getInstance()->setLandFarClip(final_far);
}

// Write some stats to llinfos
void display_stats()
{
	if (gNoRender || !gViewerWindow->getWindow()->getVisible() || !gFocusMgr.getAppHasFocus())
	{
		// Do not keep FPS statistics while yielding cooperatively
		// (i;e. when not running as foreground window)
		gRecentFrameCount = 0;
		gRecentFPSTime.reset();
	}
	F32 fps_log_freq = gSavedSettings.getF32("FPSLogFrequency");
	if (fps_log_freq > 0.f && gRecentFPSTime.getElapsedTimeF32() >= fps_log_freq)
	{
		F32 fps = gRecentFrameCount / fps_log_freq;
		LL_INFOS() << llformat("FPS: %.02f", fps) << LL_ENDL;
		LL_INFOS() << llformat("VBO: %d  glVBO: %d", LLVertexBuffer::sCount, LLVertexBuffer::sGLCount) << LL_ENDL;
#ifdef LL_OCTREE_STATS
		OctreeStats::getInstance()->dump();
#endif
		gRecentFrameCount = 0;
		gRecentFPSTime.reset();
	}
	F32 mem_log_freq = gSavedSettings.getF32("MemoryLogFrequency");
	if (mem_log_freq > 0.f && gRecentMemoryTime.getElapsedTimeF32() >= mem_log_freq)
	{
		gMemoryAllocated = U64Bytes(LLMemory::getCurrentRSS());
		U32Megabytes memory = gMemoryAllocated;
		LL_INFOS() << "MEMORY: " << memory << LL_ENDL;
		LL_INFOS() << "THREADS: "<< LLThread::getCount() << LL_ENDL;
		LL_INFOS() << "MALLOC: " << SGMemStat::getPrintableStat() <<LL_ENDL;
		LLMemory::logMemoryInfo(TRUE) ;
		gRecentMemoryTime.reset();
	}
}

static LLTrace::BlockTimerStatHandle FTM_PICK("Picking");
static LLTrace::BlockTimerStatHandle FTM_RENDER("Render", true);
static LLTrace::BlockTimerStatHandle FTM_UPDATE_SKY("Update Sky");
static LLTrace::BlockTimerStatHandle FTM_UPDATE_TEXTURES("Update Textures");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE("Update Images");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_CLASS("Class");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_BUMP("Bump");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_LIST("List");
static LLTrace::BlockTimerStatHandle FTM_IMAGE_UPDATE_DELETE("Delete");

int sMaxCacheHit = 0;
int sCurCacheHit = 0;

// Paint the display!
void display(BOOL rebuild, F32 zoom_factor, int subfield, BOOL for_snapshot, bool tiling)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER);
	/*if (gViewerWindow->getWindow()->getFullscreen() && (!gViewerWindow->getWindow() || !gViewerWindow->getWindow()->getVisible() ||  !gFocusMgr.getAppHasFocus()))
	{
		return;
	}*/
	gViewerWindow->checkSettings();
	LLVBOPool::deleteReleasedBuffers();

	for (auto avatar : LLCharacter::sInstances)
	{
		LLVOAvatar* avatarp = dynamic_cast<LLVOAvatar*>(avatar);
		if (!avatarp) continue;
		if (avatarp->isDead()) continue;
		avatarp->clearRiggedMatrixCache();
	}
	sCurCacheHit = 0;

	if (gWindowResized)
	{ //skip render on frames where window has been resized
		gGL.flush();
		gGL.syncContextState();
		glClear(GL_COLOR_BUFFER_BIT);
		gViewerWindow->getWindow()->swapBuffers();
		LLPipeline::refreshCachedSettings();
		gPipeline.resizeScreenTexture();
		gResizeScreenTexture = FALSE;
		gWindowResized = FALSE;
		return;
	}

	//Nope
	/*if (LLPipeline::sRenderDeferred)
	{ //hack to make sky show up in deferred snapshots
		for_snapshot = FALSE;
	}*/

	if (LLPipeline::sRenderFrameTest)
	{
		send_agent_pause();
	}

	gSnapshot = for_snapshot;

	LLGLSDefault gls_default;
	LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE, GL_LEQUAL);
	
	LLVertexBuffer::unbind();

	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();
	
	stop_glerror();

	LLGLState<GL_LIGHTING> light_state;
	gPipeline.disableLights(light_state);

	//reset vertex buffers if needed
	gPipeline.doResetVertexBuffers();

	stop_glerror();

	// Don't draw if the window is hidden or minimized.
	// In fact, must explicitly check the minimized state before drawing.
	// Attempting to draw into a minimized window causes a GL error. JC
	if (   !gViewerWindow->getActive()
		|| !gViewerWindow->getWindow()->getVisible() 
		|| gViewerWindow->getWindow()->getMinimized() )
	{
		// Clean up memory the pools may have allocated
		if (rebuild)
		{
			gFrameStats.start(LLFrameStats::REBUILD);
			stop_glerror();
			gPipeline.rebuildPools();
			stop_glerror();
		}

		stop_glerror();
		gViewerWindow->returnEmptyPicks();
		stop_glerror();
		return; 
	}
	
	{
		LL_RECORD_BLOCK_TIME(FTM_PICK);
		LLAppViewer::instance()->pingMainloopTimeout("Display:Pick");
		gViewerWindow->performPick();
	}
	
	LLAppViewer::instance()->pingMainloopTimeout("Display:CheckStates");
	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();
	
	//////////////////////////////////////////////////////////
	//
	// Logic for forcing window updates if we're in drone mode.
	//

	if (gNoRender) 
	{
#if LL_WINDOWS
		static F32 last_update_time = 0.f;
		if ((gFrameTimeSeconds - last_update_time) > 1.f)
		{
			InvalidateRect((HWND)gViewerWindow->getPlatformWindow(), NULL, FALSE);
			last_update_time = gFrameTimeSeconds;
		}
#elif LL_DARWIN
		// MBW -- Do something clever here.
#endif
		// Not actually rendering, don't bother.
		return;
	}


	//
	// Bail out if we're in the startup state and don't want to try to
	// render the world.
	//
	if (LLStartUp::getStartupState() < STATE_STARTED)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Startup");
		display_startup();
		return;
	}

	//LLGLState::verify(FALSE);

	/////////////////////////////////////////////////
	//
	// Update GL Texture statistics (used for discard logic?)
	//

	LLAppViewer::instance()->pingMainloopTimeout("Display:TextureStats");
	gFrameStats.start(LLFrameStats::UPDATE_TEX_STATS);
	stop_glerror();

	LLImageGL::updateStats(gFrameTimeSeconds);
	
	LLVOAvatar::sRenderName = gSavedSettings.getS32("RenderName");
	LLVOAvatar::sRenderGroupTitles = !gSavedSettings.getBOOL("RenderHideGroupTitleAll");
	
	gPipeline.mBackfaceCull = TRUE;
	gFrameCount++;
	gRecentFrameCount++;
	if (gFocusMgr.getAppHasFocus())
	{
		gForegroundFrameCount++;
	}

	//////////////////////////////////////////////////////////
	//
	// Display start screen if we're teleporting, and skip render
	//

	if (gTeleportDisplay)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Teleport");
		const F32 TELEPORT_ARRIVAL_DELAY = 2.f; // Time to preload the world before raising the curtain after we've actually already arrived.

		S32 attach_count = 0;
		if (isAgentAvatarValid())
		{
			attach_count = gAgentAvatarp->getAttachmentCount();
		}
		F32 teleport_save_time = TELEPORT_EXPIRY + TELEPORT_EXPIRY_PER_ATTACHMENT * attach_count;
		F32 teleport_elapsed = gTeleportDisplayTimer.getElapsedTimeF32();
		F32 teleport_percent = teleport_elapsed * (100.f / teleport_save_time);
		if( (gAgent.getTeleportState() != LLAgent::TELEPORT_START) && (teleport_percent > 100.f) )
		{
			// Give up.  Don't keep the UI locked forever.
			gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
			gAgent.setTeleportMessage(std::string());
		}

		static const LLCachedControl<bool> hide_tp_screen("AscentDisableTeleportScreens",false);
		const std::string& message = gAgent.getTeleportMessage();
		switch( gAgent.getTeleportState() )
		{
		case LLAgent::TELEPORT_PENDING:
			gTeleportDisplayTimer.reset();
			if(!hide_tp_screen)
				gViewerWindow->setShowProgress(TRUE);
			gViewerWindow->setProgressPercent(llmin(teleport_percent, 0.0f));
			gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["pending"]);
			gViewerWindow->setProgressString(LLAgent::sTeleportProgressMessages["pending"]);
			break;

		case LLAgent::TELEPORT_START:
			// Transition to REQUESTED.  Viewer has sent some kind
			// of TeleportRequest to the source simulator
			gTeleportDisplayTimer.reset();
			if(!hide_tp_screen)
				gViewerWindow->setShowProgress(TRUE);
			gViewerWindow->setProgressPercent(llmin(teleport_percent, 0.0f));
			gAgent.setTeleportState( LLAgent::TELEPORT_REQUESTED );
			gAgent.setTeleportMessage(
				LLAgent::sTeleportProgressMessages["requesting"]);
			gViewerWindow->setProgressString(LLAgent::sTeleportProgressMessages["requesting"]);
			break;

		case LLAgent::TELEPORT_REQUESTED:
			// Waiting for source simulator to respond
			gViewerWindow->setProgressPercent( llmin(teleport_percent, 37.5f) );
			gViewerWindow->setProgressString(message);
			break;

		case LLAgent::TELEPORT_MOVING:
			// Viewer has received destination location from source simulator
			gViewerWindow->setProgressPercent( llmin(teleport_percent, 75.f) );
			gViewerWindow->setProgressString(message);
			break;

		case LLAgent::TELEPORT_START_ARRIVAL:
			// Transition to ARRIVING.  Viewer has received avatar update, etc., from destination simulator
			gTeleportArrivalTimer.reset();
				gViewerWindow->setProgressCancelButtonVisible(FALSE, LLTrans::getString("Cancel"));
			gViewerWindow->setProgressPercent(75.f);
			gAgent.setTeleportState( LLAgent::TELEPORT_ARRIVING );
			gAgent.setTeleportMessage(
				LLAgent::sTeleportProgressMessages["arriving"]);
			gTextureList.mForceResetTextureStats = TRUE;
			if(!hide_tp_screen)
				gAgentCamera.resetView(TRUE, TRUE);
			break;

		case LLAgent::TELEPORT_ARRIVING:
			// Make the user wait while content "pre-caches"
			{
				F32 arrival_fraction = (gTeleportArrivalTimer.getElapsedTimeF32() / TELEPORT_ARRIVAL_DELAY);
				if( arrival_fraction > 1.f || hide_tp_screen)
				{
					arrival_fraction = 1.f;
					LLFirstUse::useTeleport();
					gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
				}
				gViewerWindow->setProgressCancelButtonVisible(FALSE, LLTrans::getString("Cancel"));
				gViewerWindow->setProgressPercent(  arrival_fraction * 25.f + 75.f);
				gViewerWindow->setProgressString(message);
			}
			break;

		case LLAgent::TELEPORT_LOCAL:
			// Short delay when teleporting in the same sim (progress screen active but not shown - did not
			// fall-through from TELEPORT_START)
			{
				// <edit>
				// is this really needed.... I say no.
				//if( gTeleportDisplayTimer.getElapsedTimeF32() > TELEPORT_LOCAL_DELAY )
				// </edit>
				{
					LLFirstUse::useTeleport();
					gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
				}
			}
			break;

		case LLAgent::TELEPORT_NONE:
			// No teleport in progress
			gViewerWindow->setShowProgress(FALSE);
			gTeleportDisplay = FALSE;
			gTeleportArrivalTimer.reset();
			break;

		default: 
			 break;
		}
	}
    else if(LLAppViewer::instance()->logoutRequestSent())
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Logout");
		F32 percent_done = gLogoutTimer.getElapsedTimeF32() * 100.f / gLogoutMaxTime;
		if (percent_done > 100.f)
		{
			percent_done = 100.f;
		}

		if( LLApp::isExiting() )
		{
			percent_done = 100.f;
		}
		
		gViewerWindow->setProgressPercent( percent_done );
	}
	else
	if (gRestoreGL)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:RestoreGL");
		F32 percent_done = gRestoreGLTimer.getElapsedTimeF32() * 100.f / RESTORE_GL_TIME;
		if( percent_done > 100.f )
		{
			gViewerWindow->setShowProgress(FALSE);
			gRestoreGL = FALSE;
		}
		else
		{

			if( LLApp::isExiting() )
			{
				percent_done = 100.f;
			}
			
			gViewerWindow->setProgressPercent( percent_done );
		}
	}

	// Progressively increase draw distance after TP when required.
	if (gSavedDrawDistance > 0.0f && gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
	{
		if (gTeleportArrivalTimer.getElapsedTimeF32() >=
			(F32)gSavedSettings.getU32("SpeedRezInterval"))
		{
			gTeleportArrivalTimer.reset();
			F32 current = gSavedSettings.getF32("RenderFarClip");
			if (gSavedDrawDistance > current)
			{
				current *= 2.0;
				if (current > gSavedDrawDistance)
				{
					current = gSavedDrawDistance;
				}
				gSavedSettings.setF32("RenderFarClip", current);
			}
			if (current >= gSavedDrawDistance)
			{
				gSavedDrawDistance = 0.0f;
				gSavedSettings.setF32("SavedRenderFarClip", 0.0f);
			}
		}
	}

	//////////////////////////
	//
	// Prepare for the next frame
	//

	/////////////////////////////
	//
	// Update the camera
	//
	//

	LLAppViewer::instance()->pingMainloopTimeout("Display:Camera");
	LLViewerCamera::getInstance()->setZoomParameters(zoom_factor, subfield);
	LLViewerCamera::getInstance()->setNear(MIN_NEAR_PLANE);

	//////////////////////////
	//
	// clear the next buffer
	// (must follow dynamic texture writing since that uses the frame buffer)
	//

	if (gDisconnected)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Disconnected");
		render_ui();
	}
	
	//////////////////////////
	//
	// Set rendering options
	//
	//
	LLAppViewer::instance()->pingMainloopTimeout("Display:RenderSetup");
	stop_glerror();

	///////////////////////////////////////
	//
	// Slam lighting parameters back to our defaults.
	// Note that these are not the same as GL defaults...

	stop_glerror();
	gGL.setAmbientLightColor(LLColor4::white);
	stop_glerror();
			
	/////////////////////////////////////
	//
	// Render
	//
	// Actually push all of our triangles to the screen.
	//

	// do render-to-texture stuff here
	if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES))
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:DynamicTextures");
		LL_RECORD_BLOCK_TIME(FTM_UPDATE_TEXTURES);
		if (LLViewerDynamicTexture::updateAllInstances())
		{
			gGL.setColorMask(true, true);
			gGL.syncContextState();
			glClear(GL_DEPTH_BUFFER_BIT);
		}
	}

	gViewerWindow->setup3DViewport();

	gPipeline.resetFrameStats();	// Reset per-frame statistics.

	if (!gDisconnected)
	{
		LLAppViewer::instance()->pingMainloopTimeout("Display:Update");
		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{ //don't draw hud objects in this frame
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		}

		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES))
		{ //don't draw hud particles in this frame
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
		}

		//upkeep gl name pools
		LLGLNamePool::upkeepPools();
		
		stop_glerror();
		display_update_camera(tiling);
		stop_glerror();
				
		// *TODO: merge these two methods
		LLHUDManager::getInstance()->updateEffects();
		LLHUDObject::updateAll();
		stop_glerror();

		if(!tiling)
		{
			gFrameStats.start(LLFrameStats::UPDATE_GEOM);
			const F32 max_geom_update_time = 0.005f*10.f*gFrameIntervalSeconds; // 50 ms/second update time
			gPipeline.createObjects(max_geom_update_time);
			gPipeline.processPartitionQ();
			gPipeline.updateGeom(max_geom_update_time, *LLViewerCamera::getInstance());
			stop_glerror();
			gPipeline.updateGL();
			stop_glerror();
		}
		
		gFrameStats.start(LLFrameStats::UPDATE_CULL);
		S32 water_clip = 0;
		if ((LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT) > 1) &&
			 (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_WATER) ||
			  gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_VOIDWATER)))
		{
			if (LLViewerCamera::getInstance()->cameraUnderWater())
			{
				water_clip = -1;
			}
			else
			{
				water_clip = 1;
			}
		}

		LLAppViewer::instance()->pingMainloopTimeout("Display:Cull");
		
		//Increment drawable frame counter
		LLDrawable::incrementVisible();

		LLSpatialGroup::sNoDelete = TRUE;
		LLTexUnit::sWhiteTexture = LLViewerFetchedTexture::sWhiteImagep->getTexName();

		/*if (LLPipeline::sUseOcclusion && LLPipeline::sRenderDeferred)
		{ //force occlusion on for all render types if doing deferred render
			LLPipeline::sUseOcclusion = 3;
		}*/

		S32 occlusion = LLPipeline::sUseOcclusion;
		if (gDepthDirty)
		{ //depth buffer is invalid, don't overwrite occlusion state
			LLPipeline::sUseOcclusion = llmin(occlusion, 1);
		}
		gDepthDirty = FALSE;

		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkTextureChannels();
		LLGLStateValidator::checkClientArrays();

		static LLCullResult result;
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater() ? TRUE : FALSE;
		gPipeline.updateCull(*LLViewerCamera::getInstance(), result, water_clip);
		stop_glerror();

		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkTextureChannels();
		LLGLStateValidator::checkClientArrays();

		BOOL to_texture = LLGLSLShader::sNoFixedFunction &&
						LLPipeline::sRenderGlow;

		LLAppViewer::instance()->pingMainloopTimeout("Display:Swap");
		
		{ 

			if (gResizeScreenTexture)
			{
				gResizeScreenTexture = FALSE;
				gPipeline.resizeScreenTexture();
			}

			gGL.setColorMask(true, true);
			glClearColor(0,0,0,0);

			LLGLStateValidator::checkStates();
			LLGLStateValidator::checkTextureChannels();
			LLGLStateValidator::checkClientArrays();

			if (!for_snapshot || LLPipeline::sRenderDeferred)
			{
				if (gFrameCount > 1)
				{ //for some reason, ATI 4800 series will error out if you 
				  //try to generate a shadow before the first frame is through
					gPipeline.generateSunShadow(*LLViewerCamera::getInstance());
				}

				LLVertexBuffer::unbind();

				LLGLStateValidator::checkStates();
				LLGLStateValidator::checkTextureChannels();
				LLGLStateValidator::checkClientArrays();

				const LLMatrix4a saved_proj = glh_get_current_projection();
				const LLMatrix4a saved_mod = glh_get_current_modelview();
				gGL.setViewport(0,0,512,512);
				LLVOAvatar::updateFreezeCounter() ;

				if(!LLPipeline::sMemAllocationThrottled)
				{		
					LLVOAvatar::updateImpostors();
				}

				glh_set_current_projection(saved_proj);
				glh_set_current_modelview(saved_mod);
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.loadMatrix(saved_proj);
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.loadMatrix(saved_mod);
				gViewerWindow->setup3DViewport();

				LLGLStateValidator::checkStates();
				LLGLStateValidator::checkTextureChannels();
				LLGLStateValidator::checkClientArrays();

			}
			gGL.syncContextState();
			glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}

		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkClientArrays();

		//if (!for_snapshot)
		{
			LLAppViewer::instance()->pingMainloopTimeout("Display:Imagery");
			gPipeline.generateWaterReflection(*LLViewerCamera::getInstance());
			gPipeline.renderPhysicsDisplay();
		}

		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkClientArrays();

		//////////////////////////////////////
		//
		// Update images, using the image stats generated during object update/culling
		//
		// Can put objects onto the retextured list.
		//
		// Doing this here gives hardware occlusion queries extra time to complete
		LLAppViewer::instance()->pingMainloopTimeout("Display:UpdateImages");
		LLError::LLCallStacks::clear() ;
		LL_PUSH_CALLSTACKS();
		gFrameStats.start(LLFrameStats::IMAGE_UPDATE);

		{
			LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE);
			
			{
				LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_CLASS);
				LLViewerTexture::updateClass(LLViewerCamera::getInstance()->getVelocityStat()->getMean(),
											LLViewerCamera::getInstance()->getAngularVelocityStat()->getMean());
			}

			
			{
				LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_BUMP);
				gBumpImageList.updateImages();  // must be called before gTextureList version so that it's textures are thrown out first.
			}

			{
				LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_LIST);
				F32 max_image_decode_time = 0.050f*gFrameIntervalSeconds; // 50 ms/second decode time
				max_image_decode_time = llclamp(max_image_decode_time, 0.002f, 0.005f ); // min 2ms/frame, max 5ms/frame)
				gTextureList.updateImages(max_image_decode_time);
			}

			/*{
				LL_RECORD_BLOCK_TIME(FTM_IMAGE_UPDATE_DELETE);
				//remove dead textures from GL
				LLImageGL::deleteDeadTextures();
				stop_glerror();
			}*/
		}
		LL_PUSH_CALLSTACKS();
		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkClientArrays();

		///////////////////////////////////
		//
		// StateSort
		//
		// Responsible for taking visible objects, and adding them to the appropriate draw orders.
		// In the case of alpha objects, z-sorts them first.
		// Also creates special lists for outlines and selected face rendering.
		//
		LLAppViewer::instance()->pingMainloopTimeout("Display:StateSort");
		{
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
			gFrameStats.start(LLFrameStats::STATE_SORT);
			gPipeline.stateSort(*LLViewerCamera::getInstance(), result);
			stop_glerror();
				
			if (rebuild)
			{
				//////////////////////////////////////
				//
				// rebuildPools
				//
				//
				gFrameStats.start(LLFrameStats::REBUILD);
				gPipeline.rebuildPools();
				stop_glerror();
			}
		}

		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkClientArrays();

		LLPipeline::sUseOcclusion = occlusion;

		{
			LLAppViewer::instance()->pingMainloopTimeout("Display:Sky");
			LL_RECORD_BLOCK_TIME(FTM_UPDATE_SKY);	
			gSky.updateSky();
		}

//		if(gUseWireframe)
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.1.3b) | Modified: RLVa-1.1.3b
		if ( (gUseWireframe) && ((!rlv_handler_t::isEnabled()) || (!gRlvAttachmentLocks.hasLockedHUD())) )
// [/RLVa:KB]
		{
			glClearColor(0.5f, 0.5f, 0.5f, 0.f);
			gGL.syncContextState();
			glClear(GL_COLOR_BUFFER_BIT);
			gGL.setPolygonMode(LLRender::PF_FRONT_AND_BACK, LLRender::PM_LINE);
		}

		LLVBOPool::deleteReleasedBuffers();

		LLAppViewer::instance()->pingMainloopTimeout("Display:RenderStart");
		
		//// render frontmost floater opaque for occlusion culling purposes
		//LLFloater* frontmost_floaterp = gFloaterView->getFrontmost();
		//// assumes frontmost floater with focus is opaque
		//if (frontmost_floaterp && gFocusMgr.childHasKeyboardFocus(frontmost_floaterp))
		//{
		//	gGL.matrixMode(LLRender::MM_MODELVIEW);
		//	gGL.pushMatrix();
		//	{
		//		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		//		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
		//		gGL.loadIdentity();

		//		LLRect floater_rect = frontmost_floaterp->calcScreenRect();
		//		// deflate by one pixel so rounding errors don't occlude outside of floater extents
		//		floater_rect.stretch(-1);
		//		LLRectf floater_3d_rect((F32)floater_rect.mLeft / (F32)gViewerWindow->getWindowWidthScaled(), 
		//								(F32)floater_rect.mTop / (F32)gViewerWindow->getWindowHeightScaled(),
		//								(F32)floater_rect.mRight / (F32)gViewerWindow->getWindowWidthScaled(),
		//								(F32)floater_rect.mBottom / (F32)gViewerWindow->getWindowHeightScaled());
		//		floater_3d_rect.translate(-0.5f, -0.5f);
		//		gGL.translatef(0.f, 0.f, -LLViewerCamera::getInstance()->getNear());
		//		gGL.scalef(LLViewerCamera::getInstance()->getNear() * LLViewerCamera::getInstance()->getAspect() / sinf(LLViewerCamera::getInstance()->getView()), LLViewerCamera::getInstance()->getNear() / sinf(LLViewerCamera::getInstance()->getView()), 1.f);
		//		gGL.color4fv(LLColor4::white.mV);
		//		gGL.begin(LLVertexBuffer::QUADS);
		//		{
		//			gGL.vertex3f(floater_3d_rect.mLeft, floater_3d_rect.mBottom, 0.f);
		//			gGL.vertex3f(floater_3d_rect.mLeft, floater_3d_rect.mTop, 0.f);
		//			gGL.vertex3f(floater_3d_rect.mRight, floater_3d_rect.mTop, 0.f);
		//			gGL.vertex3f(floater_3d_rect.mRight, floater_3d_rect.mBottom, 0.f);
		//		}
		//		gGL.end();
		//		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		//	}
		//	gGL.popMatrix();
		//}

		LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater() ? TRUE : FALSE;
		
		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkClientArrays();

		stop_glerror();

		if (to_texture)
		{
			gGL.setColorMask(true, true);

			if (LLPipeline::sRenderDeferred)
			{
				gPipeline.mDeferredScreen.bindTarget();
				glClearColor(1,0,1,1);
				gPipeline.mDeferredScreen.clear();
			}
			else
			{
				gPipeline.mScreen.bindTarget();
				if (LLPipeline::sUnderWaterRender)
				{
					const LLColor4 &col = LLDrawPoolWater::sWaterFogColor;
					glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
				}
				gPipeline.mScreen.clear();
			}
			
			gGL.setColorMask(true, false);
		}
		
		LLAppViewer::instance()->pingMainloopTimeout("Display:RenderGeom");
		
		if (!(LLAppViewer::instance()->logoutRequestSent() && LLAppViewer::instance()->hasSavedFinalSnapshot())
				&& !gRestoreGL)
		{
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
			
			static LLCachedControl<bool> render_ui_occlusion("RenderUIOcclusion", false);
			if(render_ui_occlusion && LLGLSLShader::sNoFixedFunction)
			{
				LLFloater* floaterp = gFloaterView->getFrontmost();
				if(floaterp && floaterp->getVisible() && floaterp->isBackgroundVisible() && floaterp->isBackgroundOpaque())
				{	
					LLGLDepthTest depth(GL_TRUE, GL_TRUE);
					gGL.setColorMask(false, false);
					gOcclusionProgram.bind();

					LLRect rect = floaterp->calcScreenRect();
					rect.stretch(-1);
					gGL.matrixMode(LLRender::MM_PROJECTION);
					gGL.pushMatrix();
					gGL.loadIdentity();
					gGL.ortho(0.0f, gViewerWindow->getWindowWidth(), 0.0f, gViewerWindow->getWindowHeight(), 0.f, 1.0f);
					gGL.matrixMode(LLRender::MM_MODELVIEW);
					gGL.pushMatrix();
					gGL.loadIdentity();
					gGL.color4fv( LLColor4::white.mV );
					gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
					gGL.begin( LLRender::TRIANGLE_STRIP );
						gGL.vertex3f(rect.mLeft, rect.mTop,0.f);
						gGL.vertex3f(rect.mLeft, rect.mBottom,0.f);
						gGL.vertex3f(rect.mRight, rect.mTop, 0.f);
						gGL.vertex3f(rect.mRight, rect.mBottom,0.f);
					gGL.end();

					gGL.matrixMode(LLRender::MM_PROJECTION);
					gGL.popMatrix();
					gGL.matrixMode(LLRender::MM_MODELVIEW);
					gGL.popMatrix();
					gOcclusionProgram.unbind();
				}
			}

			static LLCachedControl<bool> render_depth_pre_pass("RenderDepthPrePass", false);
			if (render_depth_pre_pass && LLGLSLShader::sNoFixedFunction)
			{
				LLGLDepthTest depth(GL_TRUE, GL_TRUE);
				LLGLEnable<GL_CULL_FACE> cull_face;
				gGL.setColorMask(false, false);
				
				U32 types[] = { 
					LLRenderPass::PASS_SIMPLE, 
					LLRenderPass::PASS_FULLBRIGHT, 
					LLRenderPass::PASS_SHINY 
				};

				U32 num_types = LL_ARRAY_SIZE(types);
				gOcclusionProgram.bind();
				for (U32 i = 0; i < num_types; i++)
				{
					gPipeline.renderObjects(types[i], LLVertexBuffer::MAP_VERTEX, FALSE);
				}
				gOcclusionProgram.unbind();
			}

			gGL.setColorMask(true, false);
			LLGLEnable<GL_LIGHTING> lighting;
			LLGLEnable<GL_NORMALIZE> normalize;
			if (LLPipeline::sRenderDeferred)
			{
				gPipeline.renderGeomDeferred(*LLViewerCamera::getInstance());
			}
			else
			{
				gPipeline.renderGeom(*LLViewerCamera::getInstance(), TRUE);
			}
			
			gGL.setColorMask(true, true);

			//store this frame's modelview matrix for use
			//when rendering next frame's occlusion queries
			gGLPreviousModelView = gGLLastModelView;
			gGLLastModelView = gGLModelView;
			gGLLastProjection = gGLProjection;
			stop_glerror();
		}

		//Reversed this. disabling a texunit sets its index as current.. randomly breaking LLRender::matrixMode(U32 mode). Make sure unit0 is the 'current' unit.
		for (S32 i = gGLManager.mNumTextureImageUnits-1; i >= 0; --i)
		{ //dummy cleanup of any currently bound textures
			if (gGL.getTexUnit((U32)i)->getCurrType() != LLTexUnit::TT_NONE)
			{
				gGL.getTexUnit((U32)i)->unbind(gGL.getTexUnit((U32)i)->getCurrType());
				gGL.getTexUnit((U32)i)->disable();
			}
		}

		LLAppViewer::instance()->pingMainloopTimeout("Display:RenderFlush");		
		
		if (to_texture)
		{
			if (LLPipeline::sRenderDeferred)
			{
				gPipeline.mDeferredScreen.flush();
				if(gPipeline.mDeferredScreen.getFBO())
				{
					LLRenderTarget::copyContentsToFramebuffer(gPipeline.mDeferredScreen, 0, 0, gPipeline.mDeferredScreen.getWidth(), 
															  gPipeline.mDeferredScreen.getHeight(), 0, 0, 
															  gPipeline.mDeferredScreen.getWidth(), 
															  gPipeline.mDeferredScreen.getHeight(), 
															  GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				}
			}
			else
			{
				gPipeline.mScreen.flush();
				if(gPipeline.mScreen.getFBO())
				{				
					LLRenderTarget::copyContentsToFramebuffer(gPipeline.mScreen, 0, 0, gPipeline.mScreen.getWidth(), 
															  gPipeline.mScreen.getHeight(), 0, 0, 
															  gPipeline.mScreen.getWidth(), 
															  gPipeline.mScreen.getHeight(), 
															  GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				}
			}
		}
		//gGL.flush();

		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.renderDeferredLighting();
		}

		LLPipeline::sUnderWaterRender = FALSE;

		LLAppViewer::instance()->pingMainloopTimeout("Display:RenderUI");
		if (!for_snapshot || LLPipeline::sRenderDeferred)
		{
			LL_RECORD_BLOCK_TIME(FTM_RENDER_UI);
			gFrameStats.start(LLFrameStats::RENDER_UI);
			render_ui();
		}


		LLSpatialGroup::sNoDelete = FALSE;
		gPipeline.clearReferences();

		gPipeline.rebuildGroups();
	}

	LLAppViewer::instance()->pingMainloopTimeout("Display:FrameStats");
	
	gFrameStats.start(LLFrameStats::MISC_END);
	stop_glerror();

	if (LLPipeline::sRenderFrameTest)
	{
		send_agent_resume();
		LLPipeline::sRenderFrameTest = FALSE;
	}

	display_stats();
				
	LLAppViewer::instance()->pingMainloopTimeout("Display:Done");

	gShiftFrame = false;

	LLVBOPool::deleteReleasedBuffers();
}

void render_hud_attachments()
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
		
	const LLMatrix4a saved_proj = glh_get_current_projection();
	const LLMatrix4a saved_mod = glh_get_current_modelview();

	// clamp target zoom level to reasonable values
//	gAgentCamera.mHUDTargetZoom = llclamp(gAgentCamera.mHUDTargetZoom, 0.1f, 1.f);
// [RLVa:KB] - Checked: 2010-08-22 (RLVa-1.2.1a) | Modified: RLVa-1.0.0c
	gAgentCamera.mHUDTargetZoom = llclamp(gAgentCamera.mHUDTargetZoom, (!gRlvAttachmentLocks.hasLockedHUD()) ? 0.1f : 0.85f, 1.f);
// [/RLVa:KB]
	// smoothly interpolate current zoom level
	gAgentCamera.mHUDCurZoom = lerp(gAgentCamera.mHUDCurZoom, gAgentCamera.mHUDTargetZoom, LLSmoothInterpolation::getInterpolant(0.03f));

	if (LLPipeline::sShowHUDAttachments && !gDisconnected && setup_hud_matrices())
	{
		LLPipeline::sRenderingHUDs = TRUE;
		LLCamera hud_cam = *LLViewerCamera::getInstance();
		hud_cam.setOrigin(-1.f,0,0);
		hud_cam.setAxes(LLVector3(1,0,0), LLVector3(0,1,0), LLVector3(0,0,1));
		LLViewerCamera::updateFrustumPlanes(hud_cam, TRUE);

		static const LLCachedControl<bool> render_hud_particles("RenderHUDParticles");
		bool render_particles = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES) && render_hud_particles;
		
		//only render hud objects
		gPipeline.pushRenderTypeMask();
		
		// turn off everything
		gPipeline.clearAllRenderTypes();
		// turn on HUD
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		// turn on HUD particles
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);

		// if particles are off, turn off hud-particles as well
		if (!render_particles)
		{
			// turn back off HUD particles
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
		}

		bool has_ui = gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*) LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}

		S32 use_occlusion = LLPipeline::sUseOcclusion;
		LLPipeline::sUseOcclusion = 0;
		
		//cull, sort, and render hud objects
		static LLCullResult result;
		LLSpatialGroup::sNoDelete = TRUE;

		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		gPipeline.updateCull(hud_cam, result);

		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_SIMPLE);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_VOLUME);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_MATERIAL);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_SHINY);
		
		gPipeline.stateSort(hud_cam, result);

		gPipeline.renderGeom(hud_cam);

		LLSpatialGroup::sNoDelete = FALSE;
		//gPipeline.clearReferences();

		render_hud_elements();

		//restore type mask
		gPipeline.popRenderTypeMask();

		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*) LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}
		LLPipeline::sUseOcclusion = use_occlusion;
		LLPipeline::sRenderingHUDs = FALSE;
	}
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
	
	glh_set_current_projection(saved_proj);
	glh_set_current_modelview(saved_mod);
}

LLRect get_whole_screen_region()
{
	LLRect whole_screen = gViewerWindow->getWorldViewRectScaled();
	
	// apply camera zoom transform (for high res screenshots)
	F32 zoom_factor = LLViewerCamera::getInstance()->getZoomFactor();
	S16 sub_region = LLViewerCamera::getInstance()->getZoomSubRegion();
	if (zoom_factor > 1.f)
	{
		S32 num_horizontal_tiles = llceil(zoom_factor);
		S32 tile_width = ll_round((F32)gViewerWindow->getWorldViewWidthScaled() / zoom_factor);
		S32 tile_height = ll_round((F32)gViewerWindow->getWorldViewHeightScaled() / zoom_factor);
		int tile_y = sub_region / num_horizontal_tiles;
		int tile_x = sub_region - (tile_y * num_horizontal_tiles);
			
		whole_screen.setLeftTopAndSize(tile_x * tile_width, gViewerWindow->getWorldViewHeightScaled() - (tile_y * tile_height), tile_width, tile_height);
	}
	return whole_screen;
}

bool get_hud_matrices(const LLRect& screen_region, LLMatrix4a &proj, LLMatrix4a &model)
{
	if (isAgentAvatarValid() && gAgentAvatarp->hasHUDAttachment())
	{
		F32 zoom_level = gAgentCamera.mHUDCurZoom;
		LLBBox hud_bbox = gAgentAvatarp->getHUDBBox();
		
		F32 hud_depth = llmax(1.f, hud_bbox.getExtentLocal().mV[VX] * 1.1f);
		proj = gGL.genOrtho(-0.5f * LLViewerCamera::getInstance()->getAspect(), 0.5f * LLViewerCamera::getInstance()->getAspect(), -0.5f, 0.5f, 0.f, hud_depth);
		proj.getRow<2>().copyComponent<2>(LLVector4a(-0.01f));

		F32 aspect_ratio = LLViewerCamera::getInstance()->getAspect();
		
		F32 scale_x = (F32)gViewerWindow->getWorldViewWidthScaled() / (F32)screen_region.getWidth();
		F32 scale_y = (F32)gViewerWindow->getWorldViewHeightScaled() / (F32)screen_region.getHeight();

		proj.applyTranslation_affine(
			clamp_rescale((F32)(screen_region.getCenterX() - screen_region.mLeft), 0.f, (F32)gViewerWindow->getWorldViewWidthScaled(), 0.5f * scale_x * aspect_ratio, -0.5f * scale_x * aspect_ratio),
			clamp_rescale((F32)(screen_region.getCenterY() - screen_region.mBottom), 0.f, (F32)gViewerWindow->getWorldViewHeightScaled(), 0.5f * scale_y, -0.5f * scale_y),
			0.f);
		proj.applyScale_affine(scale_x, scale_y, 1.f);

		model = OGL_TO_CFR_ROTATION;
		model.applyTranslation_affine(LLVector3(-hud_bbox.getCenterLocal().mV[VX] + (hud_depth * 0.5f), 0.f, 0.f));
		model.applyScale_affine(zoom_level);

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

bool get_hud_matrices(LLMatrix4a &proj, LLMatrix4a &model)
{
	LLRect whole_screen = get_whole_screen_region();
	return get_hud_matrices(whole_screen, proj, model);
}

BOOL setup_hud_matrices()
{
	LLRect whole_screen = get_whole_screen_region();
	return setup_hud_matrices(whole_screen);
}

BOOL setup_hud_matrices(const LLRect& screen_region)
{
	LLMatrix4a proj, model;
	bool result = get_hud_matrices(screen_region, proj, model);
	if (!result) return result;

	// set up transform to keep HUD objects in front of camera
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.loadMatrix(proj);
	glh_set_current_projection(proj);
	
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.loadMatrix(model);
	glh_set_current_modelview(model);
	return TRUE;
}

static LLTrace::BlockTimerStatHandle FTM_SWAP("Swap");

void render_ui(F32 zoom_factor, int subfield, bool tiling)
{
	LLGLStateValidator::checkStates();
	
	const LLMatrix4a saved_view = glh_get_current_modelview();

	if (!gSnapshot)
	{
		gGL.pushMatrix();
		gGL.loadMatrix(gGLLastModelView);
		glh_set_current_modelview(gGLLastModelView);
	}
	
	{
		BOOL to_texture = LLGLSLShader::sNoFixedFunction &&
							LLPipeline::sRenderGlow;

		if (to_texture)
		{
			gPipeline.renderBloom(gSnapshot, zoom_factor, subfield, tiling);
		}

		if (LLGLSLShader::sNoFixedFunction)
		{
			LLPostProcess::getInstance()->renderEffects(gViewerWindow->getWindowDisplayWidth(), gViewerWindow->getWindowDisplayHeight());
		}
		
		render_hud_elements();
		render_hud_attachments();
	}

	LLGLSDefault gls_default;
	LLGLSUIDefault gls_ui;

	{
		gGL.color4f(1,1,1,1);
		if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
		{
			LL_RECORD_BLOCK_TIME(FTM_RENDER_UI);

			if (!gDisconnected)
			{
				render_ui_3d();
				LLGLStateValidator::checkStates();
			}
			else
			{
				render_disconnected_background();
			}

			render_ui_2d();
			LLGLStateValidator::checkStates();
		}
		gGL.flush();

		{
			gViewerWindow->setup2DRender();
			gViewerWindow->updateDebugText();
			gViewerWindow->drawDebugText();
		}

		LLVertexBuffer::unbind();
	}

	if (!gSnapshot)
	{
		glh_set_current_modelview(saved_view);
		gGL.popMatrix();
	}

	if (gDisplaySwapBuffers)
	{
		LL_RECORD_BLOCK_TIME(FTM_SWAP);
		gViewerWindow->getWindow()->swapBuffers();
	}
	gDisplaySwapBuffers = TRUE;
}

void renderCoordinateAxes()
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.0f, 0.0f, 0.0f);   // i direction = X-Axis = red
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(2.0f, 0.0f, 0.0f);
		gGL.vertex3f(3.0f, 0.0f, 0.0f);
		gGL.vertex3f(5.0f, 0.0f, 0.0f);
		gGL.vertex3f(6.0f, 0.0f, 0.0f);
		gGL.vertex3f(8.0f, 0.0f, 0.0f);
		// Make an X
		gGL.vertex3f(11.0f, 1.0f, 1.0f);
		gGL.vertex3f(11.0f, -1.0f, -1.0f);
		gGL.vertex3f(11.0f, 1.0f, -1.0f);
		gGL.vertex3f(11.0f, -1.0f, 1.0f);

		gGL.color3f(0.0f, 1.0f, 0.0f);   // j direction = Y-Axis = green
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(0.0f, 2.0f, 0.0f);
		gGL.vertex3f(0.0f, 3.0f, 0.0f);
		gGL.vertex3f(0.0f, 5.0f, 0.0f);
		gGL.vertex3f(0.0f, 6.0f, 0.0f);
		gGL.vertex3f(0.0f, 8.0f, 0.0f);
		// Make a Y
		gGL.vertex3f(1.0f, 11.0f, 1.0f);
		gGL.vertex3f(0.0f, 11.0f, 0.0f);
		gGL.vertex3f(-1.0f, 11.0f, 1.0f);
		gGL.vertex3f(0.0f, 11.0f, 0.0f);
		gGL.vertex3f(0.0f, 11.0f, 0.0f);
		gGL.vertex3f(0.0f, 11.0f, -1.0f);

		gGL.color3f(0.0f, 0.0f, 1.0f);   // Z-Axis = blue
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(0.0f, 0.0f, 2.0f);
		gGL.vertex3f(0.0f, 0.0f, 3.0f);
		gGL.vertex3f(0.0f, 0.0f, 5.0f);
		gGL.vertex3f(0.0f, 0.0f, 6.0f);
		gGL.vertex3f(0.0f, 0.0f, 8.0f);
		// Make a Z
		gGL.vertex3f(-1.0f, 1.0f, 11.0f);
		gGL.vertex3f(1.0f, 1.0f, 11.0f);
		gGL.vertex3f(1.0f, 1.0f, 11.0f);
		gGL.vertex3f(-1.0f, -1.0f, 11.0f);
		gGL.vertex3f(-1.0f, -1.0f, 11.0f);
		gGL.vertex3f(1.0f, -1.0f, 11.0f);
	gGL.end();
}


void draw_axes() 
{
	LLGLSUIDefault gls_ui;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	// A vertical white line at origin
	LLVector3 v = gAgent.getPositionAgent();
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.0f, 1.0f, 1.0f); 
		gGL.vertex3f(0.0f, 0.0f, 0.0f);
		gGL.vertex3f(0.0f, 0.0f, 40.0f);
	gGL.end();
	// Some coordinate axes
	gGL.pushMatrix();
		gGL.translatef( v.mV[VX], v.mV[VY], v.mV[VZ] );
		renderCoordinateAxes();
	gGL.popMatrix();
}

void render_ui_3d()
{
	LLGLSPipeline gls_pipeline;

	//////////////////////////////////////
	//
	// Render 3D UI elements
	// NOTE: zbuffer is cleared before we get here by LLDrawPoolHUD,
	//		 so 3d elements requiring Z buffer are moved to LLDrawPoolHUD
	//

	/////////////////////////////////////////////////////////////
	//
	// Render 2.5D elements (2D elements in the world)
	// Stuff without z writes
	//

	// Debugging stuff goes before the UI.

	stop_glerror();
	
	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.bind();
	}

	// Coordinate axes
	static const LLCachedControl<bool> show_axes("ShowAxes");
	if (show_axes)
	{
		draw_axes();
	}

	gViewerWindow->renderSelections(FALSE, FALSE, TRUE); // Non HUD call in render_hud_elements
	stop_glerror();
}

extern void check_blend_funcs();
void render_ui_2d()
{
	LLGLSUIDefault gls_ui;

	/////////////////////////////////////////////////////////////
	//
	// Render 2D UI elements that overlay the world (no z compare)

	//  Disable wireframe mode below here, as this is HUD/menus
	gGL.setPolygonMode(LLRender::PF_FRONT_AND_BACK, LLRender::PM_FILL);

	//  Menu overlays, HUD, etc
	gViewerWindow->setup2DRender();

	F32 zoom_factor = LLViewerCamera::getInstance()->getZoomFactor();
	S16 sub_region = LLViewerCamera::getInstance()->getZoomSubRegion();

	if (zoom_factor > 1.f)
	{
		//decompose subregion number to x and y values
		int pos_y = sub_region / llceil(zoom_factor);
		int pos_x = sub_region - (pos_y*llceil(zoom_factor));
		// offset for this tile
		LLFontGL::sCurOrigin.mX -= ll_round((F32)gViewerWindow->getWindowWidthScaled() * (F32)pos_x / zoom_factor);
		LLFontGL::sCurOrigin.mY -= ll_round((F32)gViewerWindow->getWindowHeightScaled() * (F32)pos_y / zoom_factor);
	}

	stop_glerror();
	//gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

	// render outline for HUD
	if (isAgentAvatarValid() && gAgentCamera.mHUDCurZoom < 0.98f)
	{
		gGL.pushMatrix();
		S32 half_width = (gViewerWindow->getWorldViewWidthScaled() / 2);
		S32 half_height = (gViewerWindow->getWorldViewHeightScaled() / 2);
		gGL.scalef(LLUI::getScaleFactor().mV[0], LLUI::getScaleFactor().mV[1], 1.f);
		gGL.translatef((F32)half_width, (F32)half_height, 0.f);
		F32 zoom = gAgentCamera.mHUDCurZoom;
		gGL.scalef(zoom,zoom,1.f);
		gGL.color4fv(LLColor4::white.mV);
		gl_rect_2d(-half_width, half_height, half_width, -half_height, FALSE);
		gGL.popMatrix();
		stop_glerror();
	}
	if(gDebugGL)check_blend_funcs();
	gViewerWindow->draw();
	if(gDebugGL)check_blend_funcs();

	// reset current origin for font rendering, in case of tiling render
	LLFontGL::sCurOrigin.set(0, 0);
}

void render_disconnected_background()
{
	if (!gDisconnectedImagep && gDisconnected)
	{
		LL_INFOS() << "Loading last bitmap..." << LL_ENDL;

		std::string temp_str;
		temp_str = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + SCREEN_LAST_FILENAME;

		LLPointer<LLImageBMP> image_bmp = new LLImageBMP;
		if( !image_bmp->load(temp_str) )
		{
			//LL_INFOS() << "Bitmap load failed" << LL_ENDL;
			return;
		}
		
		LLPointer<LLImageRaw> raw = new LLImageRaw;
		if (!image_bmp->decode(raw, 0.0f))
		{
			LL_INFOS() << "Bitmap decode failed" << LL_ENDL;
			gDisconnectedImagep = NULL;
			return;
		}

		U8 *rawp = raw->getData();
		S32 npixels = (S32)image_bmp->getWidth()*(S32)image_bmp->getHeight();
		for (S32 i = 0; i < npixels; i++)
		{
			S32 sum = 0;
			sum = *rawp + *(rawp+1) + *(rawp+2);
			sum /= 3;
			*rawp = ((S32)sum*6 + *rawp)/7;
			rawp++;
			*rawp = ((S32)sum*6 + *rawp)/7;
			rawp++;
			*rawp = ((S32)sum*6 + *rawp)/7;
			rawp++;
		}

		
		raw->expandToPowerOfTwo();
		gDisconnectedImagep = LLViewerTextureManager::getLocalTexture(raw.get(), FALSE );
		gStartTexture = gDisconnectedImagep;
	}

	// Make sure the progress view always fills the entire window.
	S32 width = gViewerWindow->getWindowWidthScaled();
	S32 height = gViewerWindow->getWindowHeightScaled();

	if (gDisconnectedImagep)
	{
		if (LLGLSLShader::sNoFixedFunction)
		{
			gUIProgram.bind();
		}
		LLGLSUIDefault gls_ui;
		gViewerWindow->setup2DRender();
		gGL.pushMatrix();
		{
			// scale ui to reflect UIScaleFactor
			// this can't be done in setup2DRender because it requires a
			// pushMatrix/popMatrix pair
			const LLVector2& display_scale = gViewerWindow->getDisplayScale();
			gGL.scalef(display_scale.mV[VX], display_scale.mV[VY], 1.f);

			gGL.getTexUnit(0)->bind(gDisconnectedImagep);
			gGL.color4f(1.f, 1.f, 1.f, 1.f);
			gl_rect_2d_simple_tex(width, height);
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		}
		gGL.popMatrix();
		gGL.flush();
		if (LLGLSLShader::sNoFixedFunction)
		{
			gUIProgram.unbind();
		}
	}
}

void display_cleanup()
{
	gDisconnectedImagep = NULL;
}
