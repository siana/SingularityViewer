/** 
 * @file llnetmap.cpp
 * @author James Cook
 * @brief Display of surrounding regions, objects, and agents.
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

#include "llnetmap.h"

#include "llmath.h"
#include "llfocusmgr.h"
#include "lllocalcliprect.h"
#include "llmenugl.h"
#include "llrender.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llglheaders.h"

// Viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h" // for gDisconnected
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llcallingcard.h"
#include "llcolorscheme.h"
#include "llfloatermap.h"
#include "llfloaterworldmap.h"
#include "llframetimer.h"
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
#include "llparcel.h"
// [/SL:KB]
#include "lltracker.h"
#include "llsurface.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
// [/SL:KB]
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "llworldmapview.h"		// shared draw code

//<edit>
#include "lfsimfeaturehandler.h"
#include "llfloateravatarlist.h"
#include "llvoavatar.h"
//</edit>

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

using namespace LLOldEvents;
typedef LLMemberListener<LLView> view_listener_t;

class LLScaleMap : public view_listener_t
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};

class LLChatRings : public view_listener_t
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};

class LLCheckChatRings : public view_listener_t
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};

class LLStopTracking : public view_listener_t
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};

class LLIsTracking : public view_listener_t
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};

//moymod - Custom minimap markers :o
class mmsetred : public view_listener_t //moymod
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};
class mmsetgreen : public view_listener_t //moymod
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};
class mmsetblue : public view_listener_t //moymod
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};
class mmsetyellow : public view_listener_t //moymod
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};
class mmsetcustom : public view_listener_t //moymod
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};
class mmsetunmark : public view_listener_t //moymod
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};
class mmenableunmark : public view_listener_t //moymod
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};

// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
class OverlayToggle : public view_listener_t
{
public:
	/*virtual*/ bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata) override final;
};
// [/SL:KB]

const F32 LLNetMap::MAP_SCALE_MIN = 32;
const F32 LLNetMap::MAP_SCALE_MID = 256;
const F32 LLNetMap::MAP_SCALE_MAX = 4096;

const F32 MAP_SCALE_INCREMENT = 16;
const F32 MAP_SCALE_ZOOM_FACTOR = 1.1f;		// Zoom in factor per click of the scroll wheel (10%)
const F32 MAP_MINOR_DIR_THRESHOLD = 0.08f;
const F32 MIN_DOT_RADIUS = 3.5f;
const F32 DOT_SCALE = 0.75f;
const F32 MIN_PICK_SCALE = 2.f;
const S32 MOUSE_DRAG_SLOP = 2;				// How far the mouse needs to move before we think it's a drag

const F32 WIDTH_PIXELS = 2.f;
const S32 CIRCLE_STEPS = 100;

const F64 COARSEUPDATE_MAX_Z = 1020.0f;

std::map<LLUUID, LLVector3d>	LLNetMap::mClosestAgentsToCursor; // <exodus/>
uuid_vec_t						LLNetMap::mClosestAgentsAtLastClick; // <exodus/>

LLNetMap::LLNetMap(const std::string& name) :
	LLPanel(name),
	mScale(128.f),
	mObjectMapTPM(1.f),
	mObjectMapPixels(255.f),
	mPickRadius(gSavedSettings, "ExodusMinimapAreaEffect"), // <exodus/>
	mTargetPan(0.f, 0.f),
	mCurPan(0.f, 0.f),
	mStartPan(0.f, 0.f),
	mMouseDown(0, 0),
	mPanning(false),
//	mUpdateNow( false ),
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	mUpdateObjectImage(false),
	mUpdateParcelImage(false),
// [/SL:KB]
	mObjectImageCenterGlobal( gAgentCamera.getCameraPositionGlobal() ),
	mObjectRawImagep(),
	mObjectImagep(),
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	mParcelImageCenterGlobal( gAgentCamera.getCameraPositionGlobal() ),
	mParcelRawImagep(),
	mParcelImagep(),
// [/SL:KB]
	mClosestAgentToCursor(),
	mPopupMenu(NULL)
{
	setScale(gSavedSettings.getF32("MiniMapScale"));

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_mini_map.xml");
}

LLNetMap::~LLNetMap()
{
	gSavedSettings.setF32("MiniMapScale", mScale);
}

BOOL LLNetMap::postBuild()
{
	mELabel = getChild<LLTextBox>("e_label");
	mNLabel = getChild<LLTextBox>("n_label");
	mWLabel = getChild<LLTextBox>("w_label");
	mSLabel = getChild<LLTextBox>("s_label");
	mNELabel = getChild<LLTextBox>("ne_label");
	mNWLabel = getChild<LLTextBox>("nw_label");
	mSWLabel = getChild<LLTextBox>("sw_label");
	mSELabel = getChild<LLTextBox>("se_label");

	// Register event listeners for popup menu
	(new LLScaleMap())->registerListener(gMenuHolder, "MiniMap.ZoomLevel");
	(new LLChatRings())->registerListener(gMenuHolder, "MiniMap.ChatRings");
	(new LLCheckChatRings())->registerListener(gMenuHolder, "MiniMap.CheckChatRings");
	(new LLStopTracking())->registerListener(gMenuHolder, "StopTracking");
	(new LLIsTracking())->registerListener(gMenuHolder, "IsTracking");
	(new mmsetred())->registerListener(gMenuHolder, "MiniMap.setred");
	(new mmsetgreen())->registerListener(gMenuHolder, "MiniMap.setgreen");
	(new mmsetblue())->registerListener(gMenuHolder, "MiniMap.setblue");
	(new mmsetyellow())->registerListener(gMenuHolder, "MiniMap.setyellow");
	(new mmsetcustom())->registerListener(gMenuHolder, "MiniMap.setcustom");
	(new mmsetunmark())->registerListener(gMenuHolder, "MiniMap.setunmark");
	(new mmenableunmark())->registerListener(gMenuHolder, "MiniMap.enableunmark");
	(new OverlayToggle())->registerListener(gMenuHolder, "Minimap.ToggleOverlay");

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	LLViewerParcelMgr::instance().setCollisionUpdateCallback(boost::bind(&LLNetMap::refreshParcelOverlay, this));
	LLViewerParcelOverlay::setUpdateCallback(boost::bind(&LLNetMap::refreshParcelOverlay, this));
// [/SL:KB]

	updateMinorDirections();

	mPopupMenu = LLUICtrlFactory::getInstance()->buildMenu("menu_mini_map.xml", gMenuHolder);
	if (!mPopupMenu) mPopupMenu = new LLMenuGL(LLStringUtil::null);
	mPopupMenu->setVisible(FALSE);
	return TRUE;
}

void LLNetMap::setScale( F32 scale )
{
	scale = llclamp(scale, MAP_SCALE_MIN, MAP_SCALE_MAX);
	mCurPan *= scale / mScale;
	mScale = scale;

	if (mObjectImagep.notNull())
	{
		F32 width = (F32)(getRect().getWidth());
		F32 height = (F32)(getRect().getHeight());
		F32 diameter = sqrt(width * width + height * height);
		F32 region_widths = diameter / mScale;
// <FS:CR> Aurora Sim
		//F32 meters = region_widths * LLWorld::getInstance()->getRegionWidthInMeters();
		F32 meters = region_widths * REGION_WIDTH_METERS;
// </FS:CR> Aurora Sim
		F32 num_pixels = (F32)mObjectImagep->getWidth();
		mObjectMapTPM = num_pixels / meters;
		mObjectMapPixels = diameter;
	}

	mPixelsPerMeter = mScale / REGION_WIDTH_METERS;
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	mUpdateObjectImage = true;
	mUpdateParcelImage = true;
// [/SL:KB]
//	mUpdateNow = true;
}


///////////////////////////////////////////////////////////////////////////////////
std::size_t hash_value(const LLUUID& uuid)
{
	return (std::size_t)uuid.getCRC32();
}
boost::unordered_map<const LLUUID,LLColor4> mm_MarkerColors;
const LLColor4* mm_getMarkerColor(const LLUUID& id, bool mark_only)
{
	if (!mark_only) // They're trying to get the color and they're not the minimap or the radar mark
	{
		static const LLCachedControl<bool> use_marked_color("LiruUseMarkedColor");
		if (!use_marked_color) return nullptr;
	}
	auto it = mm_MarkerColors.find(id);
	return it == mm_MarkerColors.end() ? nullptr : &it->second;
}
const LLColor4* mm_getMarkerColor(const LLUUID& id) { return mm_getMarkerColor(id, false); }

bool mm_getMarkerColor(const LLUUID& id, LLColor4& color)
{
	auto c = mm_getMarkerColor(id);
	if (c) color = *c;
	return c;
}

void mm_clearMark(const LLUUID& id)
{
	mm_MarkerColors.erase(id);
}

void mm_setcolor(LLUUID key,LLColor4 col)
{
	mm_MarkerColors[key] = col;
}
void LLNetMap::draw()
{
	LLViewerRegion* region = gAgent.getRegion();

	if (region == NULL) return;

 	static LLFrameTimer map_timer;
	static LLUIColor map_track_color = gTrackColor;
	static const LLCachedControl<LLColor4> map_frustum_color(gColors, "NetMapFrustum", LLColor4::white);
	static const LLCachedControl<LLColor4> map_frustum_rotating_color(gColors, "NetMapFrustumRotating", LLColor4::white);
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-08-17 (Catznip-3.3.0)
	static const LLCachedControl<LLColor4> map_property_line("MiniMapPropertyLine", LLColor4::white);
// [/SL:KB]
	static const LLCachedControl<LLColor4> map_whisper_ring_color("MiniMapWhisperRingColor", LLColor4(0.f,1.f,0.f,0.5f));
	static const LLCachedControl<LLColor4> map_chat_ring_color("MiniMapChatRingColor", LLColor4(0.f,0.f,1.f,0.5f));
	static const LLCachedControl<LLColor4> map_shout_ring_color("MiniMapShoutRingColor", LLColor4(1.f,0.f,0.f,0.5f));

	if (mObjectImagep.isNull())
	{
		createObjectImage();
	}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	if (mParcelImagep.isNull())
	{
		createParcelImage();
	}
// [/SL:KB]

	static const LLUICachedControl<S32> center("MiniMapCenter");
	if (center)
	{
		mCurPan = lerp(mCurPan, mTargetPan, LLSmoothInterpolation::getInterpolant(0.1f));
	}

	// Prepare a scissor region
	F32 rotation = 0;

	gGL.pushUIMatrix();

	{
		LLLocalClipRect clip(getLocalRect());
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

			// Draw background rectangle.
			if(isBackgroundVisible())
			{
				LLColor4 background_color = isBackgroundOpaque() ? getBackgroundColor().mV : getTransparentColor().mV;
				gGL.color4fv( background_color.mV );
				gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0);
			}
		}

		// Region 0,0 is in the middle.
		S32 center_sw_left = getRect().getWidth() / 2 + llfloor(mCurPan.mV[VX]);
		S32 center_sw_bottom = getRect().getHeight() / 2 + llfloor(mCurPan.mV[VY]);

		gGL.pushUIMatrix();
		gGL.translateUI( (F32) center_sw_left, (F32) center_sw_bottom, 0.f);

		static LLCachedControl<bool> rotate_map("MiniMapRotate", true);
		if (rotate_map)
		{
			// Rotate subsequent draws to agent rotation.
			rotation = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
			LLQuaternion rot(rotation, LLVector3(0.f, 0.f, 1.f));
			gGL.rotateUI(rot);
		}

		// Figure out where agent is.
		static const LLCachedControl<LLColor4> this_region_color(gColors, "NetMapThisRegion");
		static const LLCachedControl<LLColor4> live_region_color(gColors, "NetMapLiveRegion");
		static const LLCachedControl<LLColor4> dead_region_color(gColors, "NetMapDeadRegion");
// <FS:CR> Aurora Sim
		//S32 region_width = ll_round(LLWorld::getInstance()->getRegionWidthInMeters());
		S32 region_width = REGION_WIDTH_UNITS;
// </FS:CR> Aurora Sim

		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
			 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* regionp = *iter;

			// Find x and y position relative to camera's center.
			LLVector3 origin_agent = regionp->getOriginAgent();
			LLVector3 rel_region_pos = origin_agent - gAgentCamera.getCameraPositionAgent();
			F32 relative_x = (rel_region_pos.mV[0] / region_width) * mScale;
			F32 relative_y = (rel_region_pos.mV[1] / region_width) * mScale;

			const F32 real_width(regionp->getWidth());
			// Background region rectangle.
			F32 bottom =	relative_y;
			F32 left =		relative_x;
// <FS:CR> Aurora Sim
			//F32 top =		bottom + mScale ;
			//F32 right =		left + mScale ;
			F32 top =		bottom + (real_width / REGION_WIDTH_METERS) * mScale ;
			F32 right =		left + (real_width / REGION_WIDTH_METERS) * mScale ;
// </FS:CR> Aurora Sim

			if (regionp == region) gGL.color4fv(this_region_color().mV);
			else if (!regionp->isAlive()) gGL.color4fv(dead_region_color().mV);
			else gGL.color4fv(live_region_color().mV);

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
			static LLCachedControl<bool> s_fUseWorldMapTextures(gSavedSettings, "MiniMapWorldMapTextures");
			bool fRenderTerrain = true;

			if (s_fUseWorldMapTextures)
			{
				const LLViewerRegion::tex_matrix_t& tiles(regionp->getWorldMapTiles());

				for (S32 i(0), scaled_width((S32)real_width/region_width), square_width(scaled_width*scaled_width); i < square_width; ++i)
				{
					const F32 y(i / (F32)scaled_width);
					const F32 x(i - y*scaled_width);
					const F32 local_left(left + x*mScale);
					const F32 local_right(local_left + mScale);
					const F32 local_bottom(bottom + y*mScale);
					const F32 local_top(local_bottom + mScale);
					LLViewerTexture* pRegionImage = tiles[x*scaled_width+y];
					if (pRegionImage && pRegionImage->hasGLTexture())
					{
						gGL.getTexUnit(0)->bind(pRegionImage);
						gGL.begin(LLRender::TRIANGLE_STRIP);
							gGL.texCoord2f(0.f, 1.f);
							gGL.vertex2f(local_left, local_top);
							gGL.texCoord2f(0.f, 0.f);
							gGL.vertex2f(local_left, local_bottom);
							gGL.texCoord2f(1.f, 1.f);
							gGL.vertex2f(local_right, local_top);
							gGL.texCoord2f(1.f, 0.f);
							gGL.vertex2f(local_right, local_bottom);
						gGL.end();

						pRegionImage->setBoostLevel(LLViewerTexture::BOOST_MAP_VISIBLE);
						fRenderTerrain = false;
					}
				}
			}
// [/SL:KB]

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
			if (fRenderTerrain)
			{
// [/SL:KB]
			// Draw using texture.
			gGL.getTexUnit(0)->bind(regionp->getLand().getSTexture());
			gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(left, top);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(left, bottom);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(right, top);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(right, bottom);
			gGL.end();

			// Draw water
			gGL.setAlphaRejectSettings(LLRender::CF_GREATER, ABOVE_WATERLINE_ALPHA / 255.f);
			{
				if (regionp->getLand().getWaterTexture())
				{
					gGL.getTexUnit(0)->bind(regionp->getLand().getWaterTexture());
					gGL.begin(LLRender::TRIANGLE_STRIP);
						gGL.texCoord2f(0.f, 1.f);
						gGL.vertex2f(left, top);
						gGL.texCoord2f(0.f, 0.f);
						gGL.vertex2f(left, bottom);
						gGL.texCoord2f(1.f, 1.f);
						gGL.vertex2f(right, top);
						gGL.texCoord2f(1.f, 0.f);
						gGL.vertex2f(right, bottom);
					gGL.end();
				}
			}
			gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
			}
// [/SL:KB]
		}

		// Redraw object layer periodically
//		if (mUpdateNow || (map_timer.getElapsedTimeF32() > 0.5f))
//		{
//			mUpdateNow = false;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
		// Locate the center
		LLVector3 posCenter = globalPosToView(gAgentCamera.getCameraPositionGlobal());
		posCenter.mV[VX] -= mCurPan.mV[VX];
		posCenter.mV[VY] -= mCurPan.mV[VY];
		posCenter.mV[VZ] = 0.f;
		LLVector3d posCenterGlobal = viewPosToGlobal(llfloor(posCenter.mV[VX]), llfloor(posCenter.mV[VY]));

		static LLCachedControl<bool> s_fShowObjects(gSavedSettings, "ShowMiniMapObjects") ;
		if ( (s_fShowObjects) && ((mUpdateObjectImage) || ((map_timer.getElapsedTimeF32() > 0.5f))) )
		{
			mUpdateObjectImage = false;
// [/SL:KB]

//			// Locate the centre of the object layer, accounting for panning
//			LLVector3 new_center = globalPosToView(gAgentCamera.getCameraPositionGlobal());
//			new_center.mV[VX] -= mCurPan.mV[VX];
//			new_center.mV[VY] -= mCurPan.mV[VY];
//			new_center.mV[VZ] = 0.f;
//			mObjectImageCenterGlobal = viewPosToGlobal(llfloor(new_center.mV[VX]), llfloor(new_center.mV[VY]));
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
			mObjectImageCenterGlobal = posCenterGlobal;
// [/SL:KB]

			// Create the base texture.
			U8 *default_texture = mObjectRawImagep->getData();
			memset( default_texture, 0, mObjectImagep->getWidth() * mObjectImagep->getHeight() * mObjectImagep->getComponents() );

			// Draw objects
			gObjectList.renderObjectsForMap(*this);

			mObjectImagep->setSubImage(mObjectRawImagep, 0, 0, mObjectImagep->getWidth(), mObjectImagep->getHeight());
			map_timer.reset();
		}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
		static LLCachedControl<bool> s_fShowPropertyLines(gSavedSettings, "MiniMapPropertyLines") ;
		if ( (s_fShowPropertyLines) && ((mUpdateParcelImage) || (dist_vec_squared2D(mParcelImageCenterGlobal, posCenterGlobal) > 9.0f)) )
		{
			mUpdateParcelImage = false;
			mParcelImageCenterGlobal = posCenterGlobal;

			U8* pTextureData = mParcelRawImagep->getData();
			memset(pTextureData, 0, mParcelImagep->getWidth() * mParcelImagep->getHeight() * mParcelImagep->getComponents());

			// Process each region
			for (LLWorld::region_list_t::const_iterator itRegion = LLWorld::getInstance()->getRegionList().begin();
					itRegion != LLWorld::getInstance()->getRegionList().end(); ++itRegion)
			{
				const LLViewerRegion* pRegion = *itRegion; LLColor4U clrOverlay;
				if (pRegion->isAlive())
					clrOverlay = map_property_line.get();
				else
					clrOverlay = LLColor4U(255, 128, 128, 255);
				renderPropertyLinesForRegion(pRegion, clrOverlay);
			}

			mParcelImagep->setSubImage(mParcelRawImagep, 0, 0, mParcelImagep->getWidth(), mParcelImagep->getHeight());
		}
// [/SL:KB]

		LLVector3 map_center_agent = gAgent.getPosAgentFromGlobal(mObjectImageCenterGlobal);
		LLVector3 camera_position = gAgentCamera.getCameraPositionAgent();
		map_center_agent -= camera_position;
		map_center_agent.mV[VX] *= mScale/region_width;
		map_center_agent.mV[VY] *= mScale/region_width;

//		gGL.getTexUnit(0)->bind(mObjectImagep);
		F32 image_half_width = 0.5f*mObjectMapPixels;
		F32 image_half_height = 0.5f*mObjectMapPixels;

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
		if (s_fShowObjects)
		{
			gGL.getTexUnit(0)->bind(mObjectImagep);
// [/SL:KB]
		gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, image_half_height + map_center_agent.mV[VY]);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2f(image_half_width + map_center_agent.mV[VX], image_half_height + map_center_agent.mV[VY]);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(image_half_width + map_center_agent.mV[VX], map_center_agent.mV[VY] - image_half_height);
		gGL.end();
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
		}
// [/SL:KB]

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
		if (s_fShowPropertyLines)
		{
			map_center_agent = gAgent.getPosAgentFromGlobal(mParcelImageCenterGlobal) - camera_position;
			map_center_agent.mV[VX] *= mScale / region_width;
			map_center_agent.mV[VY] *= mScale / region_width;

			gGL.getTexUnit(0)->bind(mParcelImagep);
			gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, image_half_height + map_center_agent.mV[VY]);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], image_half_height + map_center_agent.mV[VY]);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], map_center_agent.mV[VY] - image_half_height);
			gGL.end();
		}
// [/SL:KB]

		gGL.popUIMatrix();

		// Mouse pointer in local coordinates
		S32 local_mouse_x;
		S32 local_mouse_y;

		LLUI::getMousePositionLocal(this, &local_mouse_x, &local_mouse_y);

		F32 min_pick_dist = mDotRadius * mPickRadius;

		mClosestAgentToCursor.setNull();
		mClosestAgentsToCursor.clear();

		F32 closest_dist_squared = F32_MAX;
		F32 min_pick_dist_squared = (mDotRadius * MIN_PICK_SCALE) * (mDotRadius * MIN_PICK_SCALE);

		LLVector3 pos_map;
// [RLVa:KB] - Version: 1.23.4 | Alternate: Snowglobe-1.2.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
		bool show_friends = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES);
// [/RLVa:KB]
		LLWorld::pos_map_t positions;

		LLWorld::getInstance()->getAvatars(&positions, gAgentCamera.getCameraPositionGlobal());

		// Get the selected ids from radar now, as they are loop invariant
		uuid_vec_t gmSelected;
		static const LLCachedControl<bool> radar_tracking_circle("MiniMapRadarTrackingCircles");
		if (radar_tracking_circle && LLFloaterAvatarList::instanceVisible())
			gmSelected = LLFloaterAvatarList::instance().getSelectedIDs();

		// Draw avatars
		for(const auto& pair : positions)
		{
			const LLUUID& uuid = pair.first;
			static const LLCachedControl<LLColor4>	standard_color("MapAvatar",LLColor4(0.f,1.f,0.f,1.f));
			LLColor4 color = standard_color;
			// TODO: it'd be very cool to draw these in sorted order from lowest Z to highest.
			// just be careful to sort the avatar IDs along with the positions. -MG
			const LLVector3d& position = pair.second;
			pos_map = globalPosToView(position);
			if (position.mdV[VZ] == 0.f || position.mdV[VZ] == COARSEUPDATE_MAX_Z)
			{
				pos_map.mV[VZ] = 16000.f;
			}

			if (dist_vec(LLVector2(pos_map.mV[VX], pos_map.mV[VY]), LLVector2(local_mouse_x, local_mouse_y)) < min_pick_dist)
			{
				mClosestAgentsToCursor[uuid] = position;
				static const LLCachedControl<LLColor4> map_avatar_rollover_color(gSavedSettings, "ExodusMapRolloverColor", LLColor4::cyan);
				color = map_avatar_rollover_color;
			}
			else if (auto mark_color = mm_getMarkerColor(uuid, true))
			{
				color = *mark_color;
			}
			else
			{
				bool getColorFor(const LLUUID & id, LLViewerRegion * parent_estate, LLColor4 & color, bool name_restricted = false);
				getColorFor(uuid, LLWorld::getInstance()->getRegionFromPosGlobal(position), color, !show_friends);
			}

			LLWorldMapView::drawAvatar(
				pos_map.mV[VX], pos_map.mV[VY],
				color,
				pos_map.mV[VZ], mDotRadius);

			if (!gmSelected.empty())
			if (uuid.notNull())
			{
				bool selected = false;
				uuid_vec_t::iterator sel_iter = gmSelected.begin();

				for (; sel_iter != gmSelected.end(); sel_iter++)
				{
					if(*sel_iter == uuid)
					{
						selected = true;
						break;
					}
				}

				if (selected)
				{
					if( (pos_map.mV[VX] < 0) ||
						(pos_map.mV[VY] < 0) ||
						(pos_map.mV[VX] >= getRect().getWidth()) ||
						(pos_map.mV[VY] >= getRect().getHeight()) )
					{
						S32 x = ll_round( pos_map.mV[VX] );
						S32 y = ll_round( pos_map.mV[VY] );

						LLWorldMapView::drawTrackingCircle( getRect(), x, y, color, 1, 10);
					}
					else LLWorldMapView::drawTrackingDot(pos_map.mV[VX],pos_map.mV[VY],color,0.f);
				}
			}

			F32 dist_to_cursor_squared = dist_vec_squared(LLVector2(pos_map.mV[VX], pos_map.mV[VY]), LLVector2(local_mouse_x,local_mouse_y));
			if (dist_to_cursor_squared < min_pick_dist_squared && dist_to_cursor_squared < closest_dist_squared)
			{
				closest_dist_squared = dist_to_cursor_squared;
				mClosestAgentToCursor = uuid;
			}
		}

		// Draw dot for autopilot target
		if (gAgent.getAutoPilot())
		{
			drawTracking( gAgent.getAutoPilotTargetGlobal(), map_track_color );
		}
		else
		{
			LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
			if (  LLTracker::TRACKING_AVATAR == tracking_status )
			{
				drawTracking( LLAvatarTracker::instance().getGlobalPos(), map_track_color );
			} 
			else if ( LLTracker::TRACKING_LANDMARK == tracking_status ||
					LLTracker::TRACKING_LOCATION == tracking_status )
			{
				drawTracking( LLTracker::getTrackedPositionGlobal(), map_track_color );
			}
		}

		pos_map = globalPosToView(gAgent.getPositionGlobal());
		S32 dot_width = ll_round(mDotRadius * 2.f);
		LLUIImagePtr you = LLWorldMapView::sAvatarYouLargeImage;

		if (you)
		{
			you->draw(ll_round(pos_map.mV[VX] - mDotRadius),
					  ll_round(pos_map.mV[VY] - mDotRadius),
					  dot_width,
					  dot_width);

			F32	dist_to_cursor_squared = dist_vec_squared(LLVector2(pos_map.mV[VX], pos_map.mV[VY]),
										  LLVector2(local_mouse_x,local_mouse_y));

			if (dist_to_cursor_squared < min_pick_dist_squared && dist_to_cursor_squared < closest_dist_squared)
			{
				mClosestAgentToCursor = gAgent.getID();
			}
		}

		// Draw chat range ring(s)
		static LLCachedControl<bool> whisper_ring("MiniMapWhisperRing");
		if(whisper_ring)
			drawRing(LFSimFeatureHandler::getInstance()->whisperRange(), pos_map, map_whisper_ring_color);
		static LLCachedControl<bool> chat_ring("MiniMapChatRing");
		if(chat_ring)
			drawRing(LFSimFeatureHandler::getInstance()->sayRange(), pos_map, map_chat_ring_color);
		static LLCachedControl<bool> shout_ring("MiniMapShoutRing");
		if(shout_ring)
			drawRing(LFSimFeatureHandler::getInstance()->shoutRange(), pos_map, map_shout_ring_color);

		// Draw frustum
// <FS:CR> Aurora Sim
		//F32 meters_to_pixels = mScale/ LLWorld::getInstance()->getRegionWidthInMeters();
		F32 meters_to_pixels = mScale/ REGION_WIDTH_METERS;
// </FS:CR> Aurora Sim

		F32 horiz_fov = LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect();
		F32 far_clip_meters = LLViewerCamera::getInstance()->getFar();
		F32 far_clip_pixels = far_clip_meters * meters_to_pixels;

		F32 half_width_meters = far_clip_meters * tan( horiz_fov / 2 );
		F32 half_width_pixels = half_width_meters * meters_to_pixels;
		
		F32 ctr_x = (F32)center_sw_left;
		F32 ctr_y = (F32)center_sw_bottom;


		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLColor4 c = rotate_map ? map_frustum_color() : map_frustum_rotating_color();

		gGL.pushUIMatrix();

		gGL.translateUI(ctr_x, ctr_y, 0);

		// If we don't rotate the map, we have to rotate the frustum.
		if (!rotate_map)
		{
			LLQuaternion rot(atan2(LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY]), LLVector3(0.f, 0.f, -1.f));
			gGL.rotateUI(rot);
		}

		gGL.begin( LLRender::TRIANGLES  );
			gGL.color4fv(c.mV);
			gGL.vertex2f( 0, 0 );
			c.mV[VW] *= .1f;
			gGL.color4fv(c.mV);
			gGL.vertex2f( half_width_pixels, far_clip_pixels );
			gGL.vertex2f( -half_width_pixels, far_clip_pixels );
		gGL.end();

		gGL.popUIMatrix();

		// <exodus> Draw mouse radius
		static const LLCachedControl<LLColor4> map_avatar_rollover_color("ExodusMapRolloverCircleColor");
		gGL.color4fv((map_avatar_rollover_color()).mV);
		// Todo: Detect if over the window and don't render a circle?
		gl_circle_2d(local_mouse_x, local_mouse_y, min_pick_dist, 32, true);
		// </exodus>
	}
	;
	gGL.popUIMatrix();

	// Rotation of 0 means that North is up
	setDirectionPos(mELabel, rotation);
	setDirectionPos(mNLabel, rotation + F_PI_BY_TWO);
	setDirectionPos(mWLabel, rotation + F_PI);
	setDirectionPos(mSLabel, rotation + F_PI + F_PI_BY_TWO);

	setDirectionPos(mNELabel, rotation + F_PI_BY_TWO / 2);
	setDirectionPos(mNWLabel, rotation + F_PI_BY_TWO + F_PI_BY_TWO / 2);
	setDirectionPos(mSWLabel, rotation + F_PI + F_PI_BY_TWO / 2);
	setDirectionPos(mSELabel, rotation + F_PI + F_PI_BY_TWO + F_PI_BY_TWO / 2);

	LLUICtrl::draw();
}

void LLNetMap::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);
	createObjectImage();
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-28 (Catznip-3.3)
	createParcelImage();
// [/SL:KB]
	updateMinorDirections();
}

LLVector3 LLNetMap::globalPosToView(const LLVector3d& global_pos)
{
	LLVector3d relative_pos_global = global_pos - gAgentCamera.getCameraPositionGlobal();
	LLVector3 pos_local;
	pos_local.setVec(relative_pos_global);  // convert to floats from doubles

	pos_local.mV[VX] *= mPixelsPerMeter;
	pos_local.mV[VY] *= mPixelsPerMeter;
	// leave Z component in meters

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		F32 radians = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

	pos_local.mV[VX] += getRect().getWidth() / 2 + mCurPan.mV[VX];
	pos_local.mV[VY] += getRect().getHeight() / 2 + mCurPan.mV[VY];

	return pos_local;
}

void LLNetMap::drawRing(const F32 radius, const LLVector3 pos_map, const LLColor4& color)
{
	F32 radius_pixels = radius * mPixelsPerMeter;

	gGL.pushUIMatrix();
	gGL.translateUI(pos_map.mV[VX], pos_map.mV[VY], 0.f);
	gl_washer_2d(radius_pixels, radius_pixels-WIDTH_PIXELS, CIRCLE_STEPS, color, color);
	gGL.popUIMatrix();
}

void LLNetMap::drawTracking(const LLVector3d& pos_global, const LLColor4& color,
							BOOL draw_arrow )
{
	LLVector3 pos_local = globalPosToView(pos_global);
	if( (pos_local.mV[VX] < 0) ||
		(pos_local.mV[VY] < 0) ||
		(pos_local.mV[VX] >= getRect().getWidth()) ||
		(pos_local.mV[VY] >= getRect().getHeight()) )
	{
		if (draw_arrow)
		{
			S32 x = ll_round( pos_local.mV[VX] );
			S32 y = ll_round( pos_local.mV[VY] );
			LLWorldMapView::drawTrackingCircle( getRect(), x, y, color, 1, 10 );
			LLWorldMapView::drawTrackingArrow( getRect(), x, y, color );
		}
	}
	else
	{
		LLWorldMapView::drawTrackingDot(pos_local.mV[VX], 
										pos_local.mV[VY], 
										color,
										pos_local.mV[VZ]);
	}
}

LLVector3d LLNetMap::viewPosToGlobal( S32 x, S32 y )
{
	x -= ll_round(getRect().getWidth() / 2 + mCurPan.mV[VX]);
	y -= ll_round(getRect().getHeight() / 2 + mCurPan.mV[VY]);

	LLVector3 pos_local( (F32)x, (F32)y, 0 );

	F32 radians = - atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

// <FS:CR> Aurora Sim
	//pos_local *= ( LLWorld::getInstance()->getRegionWidthInMeters() / mScale );
	pos_local *= ( REGION_WIDTH_METERS / mScale );
// </FS:CR> Aurora Sim
	
	LLVector3d pos_global;
	pos_global.setVec( pos_local );
	pos_global += gAgentCamera.getCameraPositionGlobal();

	return pos_global;
}

BOOL LLNetMap::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	// <exodus>
	if (gKeyboard->currentMask(TRUE) & MASK_SHIFT)
	{
		mPickRadius = llclamp(mPickRadius + (2.5f * clicks), 1.f, 64.f);
		return true;
	}
	// </exodus>
	// note that clicks are reversed from what you'd think: i.e. > 0  means zoom out, < 0 means zoom in
	F32 new_scale = mScale * pow(MAP_SCALE_ZOOM_FACTOR, -clicks);
	F32 old_scale = mScale;

	setScale(new_scale);

	static const LLUICachedControl<S32> center("MiniMapCenter");
	if (!center)
	{
		// Adjust pan to center the zoom on the mouse pointer
		LLVector2 zoom_offset;
		zoom_offset.mV[VX] = x - getRect().getWidth() / 2;
		zoom_offset.mV[VY] = y - getRect().getHeight() / 2;
		mCurPan -= zoom_offset * mScale / old_scale - zoom_offset;
	}

	return TRUE;
}

BOOL LLNetMap::handleToolTip( S32 x, S32 y, std::string& tool_tip, LLRect* sticky_rect_screen )
{
	if (gDisconnected)
	{
		return FALSE;
	}

	LLRect sticky_rect;
	LLViewerRegion*	region = LLWorld::getInstance()->getRegionFromPosGlobal(viewPosToGlobal(x, y));
	if( region )
	{
		// set sticky_rect
		S32 SLOP = 4;
		localPointToScreen(x - SLOP, y - SLOP, &(sticky_rect.mLeft), &(sticky_rect.mBottom));
		sticky_rect.mRight = sticky_rect.mLeft + 2 * SLOP;
		sticky_rect.mTop = sticky_rect.mBottom + 2 * SLOP;

		tool_tip.clear();

		if (region->mMapAvatarIDs.size())
		{
			if (mClosestAgentsToCursor.size())
			{
				bool single_agent(mClosestAgentsToCursor.size() == 1); // Singu note: For old look, only add the count if we have more than one
				if (!single_agent)
					tool_tip.append(llformat("Agents under cursor (%d/%d)\n", mClosestAgentsToCursor.size(), region->mMapAvatarIDs.size() + 1));

				LLVector3d myPosition = gAgent.getPositionGlobal();

				for (const auto& target : mClosestAgentsToCursor)
				{
					const auto& targetUUID = target.first;
					auto targetPosition = target.second;

					std::string fullName;
					if (targetUUID.notNull() && LLAvatarNameCache::getNSName(targetUUID, fullName))
					{
						//tool_tip.append(fullName);
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
						if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS))
							tool_tip.append( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? fullName : RlvStrings::getAnonym(fullName) );
// [/RLVa:KB]

						// <singu> Use the radar for positioning, when possible.
						if (LLFloaterAvatarList::instanceExists())
						{
							if (LLAvatarListEntry* ent = LLFloaterAvatarList::getInstance()->getAvatarEntry(targetUUID))
								targetPosition = ent->getPosition();
						}
						// </singu>

						LLVector3d delta = targetPosition - myPosition;
						F32 distance = (F32)delta.magVec();
						if (single_agent)
							tool_tip.append( llformat("\n(Distance: %.02fm)\n",distance) );
						else
							tool_tip.append(llformat(" (%.02fm)\n", distance));
					}
				}
				tool_tip += '\n';
			}
		}
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a) | Modified: RLVa-0.2.0b
		tool_tip.append((!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC) ? region->getName() : RlvStrings::getString(RLV_STRING_HIDDEN)));
// [/RLVa:KB]
		//tool_tip.append("\n\n" + region->getName());

		tool_tip.append('\n' + region->getHost().getHostName());
		tool_tip.append('\n' + region->getHost().getString());
		tool_tip.append('\n' + getToolTip());
	}
	else
	{
		return LLPanel::handleToolTip(x, y, tool_tip, sticky_rect_screen);
	}
	*sticky_rect_screen = sticky_rect;
	return TRUE;
}


void LLNetMap::setDirectionPos( LLTextBox* text_box, F32 rotation )
{
	// Rotation is in radians.
	// Rotation of 0 means x = 1, y = 0 on the unit circle.

	F32 half_height = (F32)( (getRect().getHeight() - text_box->getRect().getHeight()) / 2);
	F32 half_width  = (F32)( (getRect().getWidth() - text_box->getRect().getWidth()) / 2);
	F32 radius = llmin( half_height, half_width );

	// Inset by a little to account for position display.
	radius -= 8.f;

	text_box->setOrigin(ll_round(half_width  + radius * cos( rotation )),
						ll_round(half_height + radius * sin( rotation )));
}

void LLNetMap::updateMinorDirections()
{
	if (!mNELabel) return;

	// Hide minor directions if they cover too much of the map
	bool show_minors = mNELabel->getRect().getHeight() < MAP_MINOR_DIR_THRESHOLD *
			llmin(getRect().getWidth(), getRect().getHeight());

	mNELabel->setVisible(show_minors);
	mNWLabel->setVisible(show_minors);
	mSWLabel->setVisible(show_minors);
	mSELabel->setVisible(show_minors);
}

void LLNetMap::renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius_meters )
{
	LLVector3 local_pos;
	local_pos.setVec( pos - mObjectImageCenterGlobal );

	// DEV-17370 - megaprims of size > 4096 cause lag.  (go figger.)
	const F32 MAX_RADIUS = 256.0f;
	F32 radius_clamped = llmin(radius_meters, MAX_RADIUS);

	S32 diameter_pixels = ll_round(2 * radius_clamped * mObjectMapTPM);
	renderPoint( local_pos, color, diameter_pixels );
}


void LLNetMap::renderPoint(const LLVector3 &pos_local, const LLColor4U &color, 
						   S32 diameter, S32 relative_height)
{
	if (diameter <= 0)
	{
		return;
	}

	const S32 image_width = (S32)mObjectImagep->getWidth();
	const S32 image_height = (S32)mObjectImagep->getHeight();

	S32 x_offset = ll_round(pos_local.mV[VX] * mObjectMapTPM + image_width / 2);
	S32 y_offset = ll_round(pos_local.mV[VY] * mObjectMapTPM + image_height / 2);

	if ((x_offset < 0) || (x_offset >= image_width))
	{
		return;
	}
	if ((y_offset < 0) || (y_offset >= image_height))
	{
		return;
	}

	U8 *datap = mObjectRawImagep->getData();

	S32 neg_radius = diameter / 2;
	S32 pos_radius = diameter - neg_radius;
	S32 x, y;

	if (relative_height > 0)
	{
		// ...point above agent
		S32 px, py;

		// vertical line
		px = x_offset;
		for (y = -neg_radius; y < pos_radius; y++)
		{
			py = y_offset + y;
			if ((py < 0) || (py >= image_height))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.mAll;
		}

		// top line
		py = y_offset + pos_radius - 1;
		for (x = -neg_radius; x < pos_radius; x++)
		{
			px = x_offset + x;
			if ((px < 0) || (px >= image_width))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.mAll;
		}
	}
	else
	{
		// ...point level with agent
		for (x = -neg_radius; x < pos_radius; x++)
		{
			S32 p_x = x_offset + x;
			if ((p_x < 0) || (p_x >= image_width))
			{
				continue;
			}

			for (y = -neg_radius; y < pos_radius; y++)
			{
				S32 p_y = y_offset + y;
				if ((p_y < 0) || (p_y >= image_height))
				{
					continue;
				}
				S32 offset = p_x + p_y * image_width;
				((U32*)datap)[offset] = color.mAll;
			}
		}
	}
}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
void LLNetMap::renderPropertyLinesForRegion(const LLViewerRegion* pRegion, const LLColor4U& clrOverlay)
{
	const S32 imgWidth = (S32)mParcelImagep->getWidth();
	const S32 imgHeight = (S32)mParcelImagep->getHeight();

	const LLVector3 originLocal(pRegion->getOriginGlobal() - mParcelImageCenterGlobal);
	const S32 originX = ll_round(originLocal.mV[VX] * mObjectMapTPM + imgWidth / 2);
	const S32 originY = ll_round(originLocal.mV[VY] * mObjectMapTPM + imgHeight / 2);

	U32* pTextureData = (U32*)mParcelRawImagep->getData();

	//
	// Draw the north and east region borders
	//
	const F32 real_width(pRegion->getWidth());
	const S32 borderY = originY + ll_round(real_width * mObjectMapTPM);
	if ( (borderY >= 0) && (borderY < imgHeight) )
	{
		S32 curX = llclamp(originX, 0, imgWidth), endX = llclamp(originX + ll_round(real_width * mObjectMapTPM), 0, imgWidth - 1);
		for (; curX <= endX; curX++)
			pTextureData[borderY * imgWidth + curX] = clrOverlay.mAll;
	}
	const S32 borderX = originX + ll_round(real_width * mObjectMapTPM);
	if ( (borderX >= 0) && (borderX < imgWidth) )
	{
		S32 curY = llclamp(originY, 0, imgHeight), endY = llclamp(originY + ll_round(real_width * mObjectMapTPM), 0, imgHeight - 1);
		for (; curY <= endY; curY++)
			pTextureData[curY * imgWidth + borderX] = clrOverlay.mAll;
	}

	//
	// Render parcel lines
	//
	const F32 GRID_STEP = PARCEL_GRID_STEP_METERS;
	const S32 GRIDS_PER_EDGE = real_width / GRID_STEP;

	const U8* pOwnership = pRegion->getParcelOverlay()->getOwnership();
	const U8* pCollision = (pRegion->getHandle() == LLViewerParcelMgr::instance().getCollisionRegionHandle()) ? LLViewerParcelMgr::instance().getCollisionBitmap() : NULL;
	for (S32 idxRow = 0; idxRow < GRIDS_PER_EDGE; idxRow++)
	{
		for (S32 idxCol = 0; idxCol < GRIDS_PER_EDGE; idxCol++)
		{
			S32 overlay = pOwnership[idxRow * GRIDS_PER_EDGE + idxCol];
			S32 idxCollision = idxRow * GRIDS_PER_EDGE + idxCol;
			bool fForSale = ((overlay & PARCEL_COLOR_MASK) == PARCEL_FOR_SALE);
			bool fCollision = (pCollision) && (pCollision[idxCollision / 8] & (1 << (idxCollision % 8)));
			if ( (!fForSale) && (!fCollision) && (0 == (overlay & (PARCEL_SOUTH_LINE | PARCEL_WEST_LINE))) )
				continue;

			const S32 posX = originX + ll_round(idxCol * GRID_STEP * mObjectMapTPM);
			const S32 posY = originY + ll_round(idxRow * GRID_STEP * mObjectMapTPM);

			static LLCachedControl<bool> s_fForSaleParcels(gSavedSettings, "MiniMapForSaleParcels");
			static LLCachedControl<bool> s_fShowCollisionParcels(gSavedSettings, "MiniMapCollisionParcels");
			if ( ((s_fForSaleParcels) && (fForSale)) || ((s_fShowCollisionParcels) && (fCollision)) )
			{
				S32 curY = llclamp(posY, 0, imgHeight), endY = llclamp(posY + ll_round(GRID_STEP * mObjectMapTPM), 0, imgHeight - 1);
				for (; curY <= endY; curY++)
				{
					S32 curX = llclamp(posX, 0, imgWidth) , endX = llclamp(posX + ll_round(GRID_STEP * mObjectMapTPM), 0, imgWidth - 1);
					for (; curX <= endX; curX++)
					{
						pTextureData[curY * imgWidth + curX] = (fForSale) ? LLColor4U(255, 255, 128, 192).mAll
						                                                  : LLColor4U(255, 128, 128, 192).mAll;
					}
				}
			}
			if (overlay & PARCEL_SOUTH_LINE)
			{
				if ( (posY >= 0) && (posY < imgHeight) )
				{
					S32 curX = llclamp(posX, 0, imgWidth), endX = llclamp(posX + ll_round(GRID_STEP * mObjectMapTPM), 0, imgWidth - 1);
					for (; curX <= endX; curX++)
						pTextureData[posY * imgWidth + curX] = clrOverlay.mAll;
				}
			}
			if (overlay & PARCEL_WEST_LINE)
			{
				if ( (posX >= 0) && (posX < imgWidth) )
				{
					S32 curY = llclamp(posY, 0, imgHeight), endY = llclamp(posY + ll_round(GRID_STEP * mObjectMapTPM), 0, imgHeight - 1);
					for (; curY <= endY; curY++)
						pTextureData[curY * imgWidth + posX] = clrOverlay.mAll;
				}
			}
		}
	}
}
// [/SL:KB]

//void LLNetMap::createObjectImage()
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
bool LLNetMap::createImage(LLPointer<LLImageRaw>& rawimagep) const
// [/SL:KB]
{
	// Find the size of the side of a square that surrounds the circle that surrounds getRect().
	// ... which is, the diagonal of the rect.
	F32 width = (F32)getRect().getWidth();
	F32 height = (F32)getRect().getHeight();
	S32 square_size = ll_round( sqrt(width*width + height*height) );

	// Find the least power of two >= the minimum size.
	const S32 MIN_SIZE = 64;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-28 (Catznip-3.3)
	const S32 MAX_SIZE = 512;
// [/SL:KB]
//	const S32 MAX_SIZE = 256;
	S32 img_size = MIN_SIZE;
	while( (img_size*2 < square_size ) && (img_size < MAX_SIZE) )
	{
		img_size <<= 1;
	}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	if( rawimagep.isNull() || (rawimagep->getWidth() != img_size) || (rawimagep->getHeight() != img_size) )
	{
		rawimagep = new LLImageRaw(img_size, img_size, 4);
		U8* data = rawimagep->getData();
		memset( data, 0, img_size * img_size * 4 );
		return true;
	}
	return false;
// [/SL:KB]
//	if( mObjectImagep.isNull() ||
//		(mObjectImagep->getWidth() != img_size) ||
//		(mObjectImagep->getHeight() != img_size) )
//	{
//		mObjectRawImagep = new LLImageRaw(img_size, img_size, 4);
//		U8* data = mObjectRawImagep->getData();
//		memset( data, 0, img_size * img_size * 4 );
//		mObjectImagep = LLViewerTextureManager::getLocalTexture( mObjectRawImagep.get(), FALSE);
//	}
//	setScale(mScale);
//	mUpdateNow = true;
}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
void LLNetMap::createObjectImage()
{
	if (createImage(mObjectRawImagep))
		mObjectImagep = LLViewerTextureManager::getLocalTexture( mObjectRawImagep.get(), FALSE);
	setScale(mScale);
	mUpdateObjectImage = true;
}

void LLNetMap::createParcelImage()
{
	if (createImage(mParcelRawImagep))
		mParcelImagep = LLViewerTextureManager::getLocalTexture( mParcelRawImagep.get(), FALSE);
	mUpdateParcelImage = true;
}
// [/SL:KB]

BOOL LLNetMap::handleMouseDown( S32 x, S32 y, MASK mask )
{
	if (!(mask & MASK_SHIFT)) return FALSE;

	// Start panning
	gFocusMgr.setMouseCapture(this);

	mStartPan = mCurPan;
	mMouseDown.mX = x;
	mMouseDown.mY = y;
	return TRUE;
}

BOOL LLNetMap::handleMouseUp( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning)
		{
			// restore mouse cursor
			S32 local_x, local_y;
			local_x = mMouseDown.mX + llfloor(mCurPan.mV[VX] - mStartPan.mV[VX]);
			local_y = mMouseDown.mY + llfloor(mCurPan.mV[VY] - mStartPan.mV[VY]);
			LLRect clip_rect = getRect();
			clip_rect.stretch(-8);
			clip_rect.clipPointToRect(mMouseDown.mX, mMouseDown.mY, local_x, local_y);
			LLUI::setMousePositionLocal(this, local_x, local_y);

			// finish the pan
			mPanning = false;

			mMouseDown.set(0, 0);

			// auto centre
			mTargetPan.setZero();
		}
		gViewerWindow->showCursor();
		gFocusMgr.setMouseCapture(NULL);
		return TRUE;
	}
	return FALSE;
}

// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
bool OverlayToggle::handleEvent(LLPointer<LLEvent> event, const LLSD& sdParam)
{
	// Force an overlay update
	LLFloaterMap::findInstance()->mPanelMap->mUpdateParcelImage = true;
	return true;
}
// [/SL:KB]

BOOL LLNetMap::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	mClosestAgentsAtLastClick.clear();
	mClosestAgentsAtLastClick.reserve(mClosestAgentsToCursor.size());
	for (const auto& pair : mClosestAgentsToCursor)
		mClosestAgentsAtLastClick.push_back(pair.first);
	mClosestAgentAtLastRightClick = mClosestAgentToCursor;
	if (mPopupMenu)
	{
		showMenu(this, mPopupMenu, x, y);
	mPopupMenu->getChildView("avs_menu")->setVisible(!mClosestAgentsAtLastClick.empty() && !
// [RLVa:LF] - 2019
			gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)
// [/RLVa:LF]
		);
	}
	return TRUE;
}

BOOL LLNetMap::handleDoubleClick( S32 x, S32 y, MASK mask )
{
	LLVector3d pos_global = viewPosToGlobal(x, y);

	bool double_click_teleport = gSavedSettings.getBOOL("DoubleClickTeleport");
	bool double_click_show_world_map = gSavedSettings.getBOOL("DoubleClickShowWorldMap");

	bool new_target = false;
	if (double_click_teleport || double_click_show_world_map)
	{
		// If we're not tracking a beacon already, double-click will set one
		if (!LLTracker::isTracking())
		{
			LLFloaterWorldMap* world_map = gFloaterWorldMap;
			if (world_map)
			{
				world_map->trackLocation(pos_global);
				new_target = true;
			}
		}
	}

	if (double_click_teleport)
	{
		// If DoubleClickTeleport is on, double clicking the minimap will teleport there
		gAgent.teleportViaLocationLookAt(pos_global);
	}
	else if (double_click_show_world_map)
	{
		LLFloaterWorldMap::show(new_target);
	}
	return TRUE;
}

// static
bool LLNetMap::outsideSlop( S32 x, S32 y, S32 start_x, S32 start_y, S32 slop )
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return (dx <= -slop || slop <= dx || dy <= -slop || slop <= dy);
}

BOOL LLNetMap::handleHover( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning || outsideSlop(x, y, mMouseDown.mX, mMouseDown.mY, MOUSE_DRAG_SLOP))
		{
			if (!mPanning)
			{
				// just started panning, so hide cursor
				mPanning = true;
				gViewerWindow->hideCursor();
			}

			LLVector2 delta(static_cast<F32>(gViewerWindow->getCurrentMouseDX()),
							static_cast<F32>(gViewerWindow->getCurrentMouseDY()));

			// Set pan to value at start of drag + offset
			mCurPan += delta;
			mTargetPan = mCurPan;

			gViewerWindow->moveCursorToCenter();
		}

		// Doesn't really matter, cursor should be hidden
		gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
	}
	else
	{
		if (mask & MASK_SHIFT)
		{
			// If shift is held, change the cursor to hint that the map can be dragged
			gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
		}
		else 
		{
			gViewerWindow->setCursor( UI_CURSOR_CROSS );
		}
	}
	
	return TRUE;
}

// static
bool LLScaleMap::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	auto self = LLFloaterMap::findInstance()->mPanelMap;

	S32 level = userdata.asInteger();

	switch(level)
	{
	case 0:
		self->setScale(LLNetMap::MAP_SCALE_MIN);
		break;
	case 1:
		self->setScale(LLNetMap::MAP_SCALE_MID);
		break;
	case 2:
		self->setScale(LLNetMap::MAP_SCALE_MAX);
		break;
	default:
		break;
	}

	return true;
}

//moymod - minimap color shit
void markMassAgents(const LLColor4& color)
{
	auto radar = LLFloaterAvatarList::getInstance();
	for (const auto& id : LFIDBearer::getActiveSelectedIDs())
	{
		mm_setcolor(id, color);
		if (auto entry = radar ? radar->getAvatarEntry(id) : nullptr)
			entry->setMarked(true);
	}
}

bool mmsetred::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	markMassAgents(LLColor4::red);
	return true;
}
bool mmsetgreen::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	markMassAgents(LLColor4::green);
	return true;
}
bool mmsetblue::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	markMassAgents(LLColor4::blue);
	return true;
}
bool mmsetyellow::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	markMassAgents(LLColor4::yellow);
	return true;
}
bool mmsetcustom::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	markMassAgents(gSavedSettings.getColor4("MoyMiniMapCustomColor"));
	return true;
}
bool mmsetunmark::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	auto radar = LLFloaterAvatarList::getInstance();
	for (const auto& id : LFIDBearer::getActiveSelectedIDs())
	{
		mm_clearMark(id);
		if (auto entry = radar ? radar->getAvatarEntry(id) : nullptr)
			entry->setMarked(false);
	}
	return true;
}
bool mmenableunmark::handleEvent(LLPointer<LLEvent>, const LLSD& userdata)
{
	bool enabled(false);
	for (const auto& id : LFIDBearer::getActiveSelectedIDs())
		if (enabled = mm_MarkerColors.find(id) != mm_MarkerColors.end())
			break;
	mPtr->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLChatRings::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	auto whisper = gSavedSettings.getControl("MiniMapWhisperRing");
	auto chat = gSavedSettings.getControl("MiniMapChatRing");
	auto shout = gSavedSettings.getControl("MiniMapShoutRing");
	bool all_enabled = whisper->get() && chat->get() && shout->get();

	whisper->set(!all_enabled);
	chat->set(!all_enabled);
	shout->set(!all_enabled);

	return true;
}

bool LLCheckChatRings::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	bool all_enabled = gSavedSettings.getBOOL("MiniMapWhisperRing")
		&& gSavedSettings.getBOOL("MiniMapChatRing")
		&& gSavedSettings.getBOOL("MiniMapShoutRing");

	mPtr->findControl(userdata["control"].asString())->setValue(all_enabled);
	return true;
}

bool LLStopTracking::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLTracker::stopTracking(false);
	return true;
}

bool LLIsTracking::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	mPtr->findControl(userdata["control"].asString())->setValue(LLTracker::isTracking());
	return true;
}

