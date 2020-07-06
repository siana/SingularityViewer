/** 
 * @file llnetmap.h
 * @brief A little map of the world with network information
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

#ifndef LL_LLNETMAP_H
#define LL_LLNETMAP_H

#include "lfidbearer.h"
#include "llpanel.h"


class LLTextBox;
class LLImageRaw;
class LLViewerTexture;
class LLFloaterMap;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
class LLViewerRegion;
class LLAvatarName;
// [/SL:KB]

class LLNetMap final : public LLPanel, public LFIDBearer
{
public:
	LLNetMap(const std::string& name);
	virtual ~LLNetMap();

	static const F32 MAP_SCALE_MIN;
	static const F32 MAP_SCALE_MID;
	static const F32 MAP_SCALE_MAX;

	/*virtual*/ void	draw();
	/*virtual*/ BOOL	handleScrollWheel(S32 x, S32 y, S32 clicks);
	/*virtual*/ BOOL	handleMouseDown( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL	handleMouseUp( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL	handleHover( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL	handleToolTip( S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen );
	/*virtual*/ void	reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);

	/*virtual*/ BOOL 	postBuild();
	/*virtual*/ BOOL	handleRightMouseDown( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL	handleDoubleClick( S32 x, S32 y, MASK mask );

	LLUUID getStringUUIDSelectedItem() const override final { return mClosestAgentAtLastRightClick; }
	uuid_vec_t getSelectedIDs() const override final { return mClosestAgentsAtLastClick; }

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	void			refreshParcelOverlay() { mUpdateParcelImage = true; }
// [/SL:KB]
	void			setScale( F32 scale );
	void			renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius );

private:
	const LLVector3d& getObjectImageCenterGlobal()	{ return mObjectImageCenterGlobal; }
	void renderPoint(const LLVector3 &pos, const LLColor4U &color, 
					 S32 diameter, S32 relative_height = 0);

	LLVector3		globalPosToView(const LLVector3d& global_pos);
	LLVector3d		viewPosToGlobal(S32 x,S32 y);

	void			drawRing(const F32 radius, LLVector3 pos_map, const LLColor4& color);

	void			drawTracking( const LLVector3d& pos_global,
							const LLColor4& color,
							BOOL draw_arrow = TRUE);

	void			setDirectionPos( LLTextBox* text_box, F32 rotation );
	void			updateMinorDirections();
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	bool			createImage(LLPointer<LLImageRaw>& rawimagep) const;
	void			createObjectImage();
	void			createParcelImage();

	void			renderPropertyLinesForRegion(const LLViewerRegion* pRegion, const LLColor4U& clrOverlay);
// [/SL:KB]
//	void			createObjectImage();

	static bool		outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y, S32 slop);

private:
//	bool			mUpdateNow;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	bool			mUpdateObjectImage;
protected:
	friend class OverlayToggle;
	bool			mUpdateParcelImage;
private:
// [/SL:KB]

	LLTextBox *mELabel = nullptr,
		*mNLabel = nullptr,
		*mWLabel = nullptr,
		*mSLabel = nullptr,
		*mNELabel = nullptr,
		*mNWLabel = nullptr,
		*mSWLabel = nullptr,
		*mSELabel = nullptr;

	F32				mScale;					// Size of a region in pixels
	F32				mPixelsPerMeter;		// world meters to map pixels
	F32				mObjectMapTPM;			// texels per meter on map
	F32				mObjectMapPixels;		// Width of object map in pixels
	F32				mDotRadius;				// Size of avatar markers
	// <exodus>
	LLCachedControl<F32> mPickRadius;		// Size of the rightclick area of affect
	// </exodus>

	bool			mPanning;			// map is being dragged
	LLVector2		mTargetPan;
	LLVector2		mCurPan;
	LLVector2		mStartPan;		// pan offset at start of drag
	LLCoordGL		mMouseDown;			// pointer position at start of drag

	LLVector3d		mObjectImageCenterGlobal;
	LLPointer<LLImageRaw> mObjectRawImagep;
	LLPointer<LLViewerTexture>	mObjectImagep;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	LLVector3d		mParcelImageCenterGlobal;
	LLPointer<LLImageRaw> mParcelRawImagep;
	LLPointer<LLViewerTexture>	mParcelImagep;
// [/SL:KB]

	static std::map<LLUUID, LLVector3d> mClosestAgentsToCursor; // <exodus/>
	static uuid_vec_t					mClosestAgentsAtLastClick; // <exodus/>

	LLUUID			mClosestAgentToCursor;
	LLUUID			mClosestAgentAtLastRightClick;

	BOOL isAgentUnderCursor() const { return mClosestAgentToCursor.notNull(); }
	LLMenuGL*		mPopupMenu;
};


#endif
