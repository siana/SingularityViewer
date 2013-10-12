/** 
 * @file llfloatersnapshot.cpp
 * @brief Snapshot preview window, allowing saving, e-mailing, etc.
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

#include "llfloatersnapshot.h"

#include "llfontgl.h"
#include "llsys.h"
#include "llgl.h"
#include "llrender.h"
#include "v3dmath.h"
#include "llmath.h"
#include "lldir.h"
#include "lllocalcliprect.h"
#include "llsdserialize.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llcallbacklist.h"
#include "llcriticaldamp.h"
#include "llfloaterperms.h"
#include "llui.h"
#include "llviewertexteditor.h"
#include "llfocusmgr.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lleconomy.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llviewerstats.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llviewermenufile.h"	// upload_new_resource()
#include "llresourcedata.h"
#include "llfloaterpostcard.h"
#include "llfloaterfeed.h"
#include "llcheckboxctrl.h"
#include "llradiogroup.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "llworld.h"
#include "llagentui.h"
#include "llvoavatar.h"
#include "lluploaddialog.h"

#include "llgl.h"
#include "llglheaders.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagebmp.h"
#include "llimagej2c.h"
#include "llnotificationsutil.h"
#include "llvfile.h"
#include "llvfs.h"

#include "hippogridmanager.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------
S32 LLFloaterSnapshot::sUIWinHeightLong = 625 ;
S32 LLFloaterSnapshot::sUIWinHeightShort = LLFloaterSnapshot::sUIWinHeightLong - 266 ;
S32 LLFloaterSnapshot::sUIWinWidth = 219 ;
S32 const THUMBHEIGHT = 159;

LLSnapshotFloaterView* gSnapshotFloaterView = NULL;

LLFloaterSnapshot* LLFloaterSnapshot::sInstance = NULL;

const F32 AUTO_SNAPSHOT_TIME_DELAY = 1.f;

F32 SHINE_TIME = 0.5f;
F32 SHINE_WIDTH = 0.6f;
F32 SHINE_OPACITY = 0.3f;
F32 FALL_TIME = 0.6f;
S32 BORDER_WIDTH = 6;

const S32 MAX_POSTCARD_DATASIZE = 1024 * 1024; // one megabyte
const S32 MAX_TEXTURE_SIZE = 512 ; //max upload texture size 512 * 512

static std::string snapshotKeepAspectName();

///----------------------------------------------------------------------------
/// Class LLSnapshotLivePreview 
///----------------------------------------------------------------------------
class LLSnapshotLivePreview : public LLView
{
public:
	enum ESnapshotType
	{
		SNAPSHOT_FEED,
		SNAPSHOT_POSTCARD,
		SNAPSHOT_TEXTURE,
		SNAPSHOT_LOCAL
	};

	enum EAspectSizeProblem
	{
		ASPECTSIZE_OK,
		CANNOT_CROP_HORIZONTALLY,
		CANNOT_CROP_VERTICALLY,
		SIZE_TOO_LARGE,
		CANNOT_RESIZE,
		DELAYED,
		NO_RAW_IMAGE,
		ENCODING_FAILED
	};

	U32 typeToMask(ESnapshotType type) const { return 1 << type; }
	void addUsedBy(ESnapshotType type) { mUsedBy |= typeToMask(type); }
	void delUsedBy(ESnapshotType type) { mUsedBy &= ~typeToMask(type); }
	bool isUsedBy(ESnapshotType type) const { return (mUsedBy & typeToMask(type)) != 0; }
	bool isUsed(void) const { return mUsedBy; }
	void addManualOverride(ESnapshotType type) { mManualSizeOverride |= typeToMask(type); }
	bool isOverriddenBy(ESnapshotType type) const { return (mManualSizeOverride & typeToMask(type)) != 0; }

	LLSnapshotLivePreview(const LLRect& rect);
	~LLSnapshotLivePreview();

	/*virtual*/ void draw();
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent);
	
	void setSize(S32 w, S32 h);
	void getSize(S32& w, S32& h) const;
	void setAspect(F32 a);
	F32 getAspect() const;
	void getRawSize(S32& w, S32& h) const;
	S32 getDataSize() const { return mFormattedDataSize; }
	void setMaxImageSize(S32 size) ;
	S32  getMaxImageSize() {return mMaxImageSize ;}
	
	ESnapshotType getSnapshotType() const { return mSnapshotType; }
	LLFloaterSnapshot::ESnapshotFormat getSnapshotFormat() const { return mSnapshotFormat; }
	BOOL getRawSnapshotUpToDate() const;
	BOOL getSnapshotUpToDate() const;
	BOOL isSnapshotActive() { return mSnapshotActive; }
	LLViewerTexture* getThumbnailImage() const { return mThumbnailImage ; }
	S32  getThumbnailWidth() const { return mThumbnailWidth ; }
	S32  getThumbnailHeight() const { return mThumbnailHeight ; }
	BOOL getThumbnailLock() const { return mThumbnailUpdateLock ; }
	BOOL getThumbnailUpToDate() const { return mThumbnailUpToDate ;}
	bool getShowFreezeFrameSnapshot() const { return mShowFreezeFrameSnapshot; }
	LLViewerTexture* getCurrentImage();
	char const* resolutionComboName() const;
	char const* aspectComboName() const;
	
	void setSnapshotType(ESnapshotType type) { mSnapshotType = type; }
	void setSnapshotFormat(LLFloaterSnapshot::ESnapshotFormat type) { mSnapshotFormat = type; }
	void setSnapshotQuality(S32 quality);
	void setSnapshotBufferType(LLFloaterSnapshot* floater, LLViewerWindow::ESnapshotType type);
	void showFreezeFrameSnapshot(bool show);
	void updateSnapshot(BOOL new_snapshot, BOOL new_thumbnail = FALSE, F32 delay = 0.f);
	LLFloaterFeed* getCaptionAndSaveFeed();
	LLFloaterPostcard* savePostcard();
	void saveTexture();
	static void saveTextureDone(LLUUID const& asset_id, void* user_data, S32 status,  LLExtStat ext_status);
	static void saveTextureDone2(bool success, void* user_data);
	void saveLocal();
	void saveStart(int index);
	void saveDone(ESnapshotType type, bool success, int index);
	void close(LLFloaterSnapshot* view);
	void doCloseAfterSave();

	BOOL setThumbnailImageSize();
	void generateThumbnailImage();
	EAspectSizeProblem getAspectSizeProblem(S32& width_out, S32& height_out, bool& crop_vertically_out, S32& crop_offset_out);
	EAspectSizeProblem generateFormattedAndFullscreenPreview(bool delayed_formatted = false);
	void drawPreviewRect(S32 offset_x, S32 offset_y) ;

	// Returns TRUE when snapshot generated, FALSE otherwise.
	static BOOL onIdle(LLSnapshotLivePreview* previewp);

private:
	LLColor4					mColor;

	LLPointer<LLViewerTexture>	mFullScreenPreviewTexture;	// Used to represent the scene when the frame is frozen.
	LLRect						mFullScreenImageRect;		// The portion of the screen that is captured (the window size minus any cropping).
	S32							mWidth;						// The target image width.
	S32							mHeight;					// The target image height.
	BOOL						mFullScreenPreviewTextureNeedsScaling;	// True when the actual data in mFullScreenPreviewTexture is smaller than its size.

	// Temporary data for the fall animation (same as the above).
	LLPointer<LLViewerTexture>	mFallFullScreenPreviewTexture;
	LLRect						mFallFullScreenImageRect;
	S32							mFallWidth;
	S32							mFallHeight;
	BOOL						mFallFullScreenPreviewTextureNeedsScaling;

	S32                         mMaxImageSize;
	F32							mAspectRatio;
	
	// Thumbnail image.
	LLPointer<LLViewerTexture>	mThumbnailImage ;
	LLRect                      mThumbnailPreviewRect ;
	S32                         mThumbnailWidth ;
	S32                         mThumbnailHeight ;
	BOOL                        mThumbnailUpdateLock ;
	BOOL                        mThumbnailUpToDate ;

	// The last raw snapshot image.
	LLPointer<LLImageRaw>		mRawSnapshot;
	S32							mRawSnapshotWidth;
	S32							mRawSnapshotHeight;
	BOOL						mRawSnapshotRenderUI;
	BOOL						mRawSnapshotRenderHUD;
	LLViewerWindow::ESnapshotType mRawSnapshotBufferType;

	LLPointer<LLImageFormatted>	mFormattedImage;
	S32							mFormattedWidth;
	S32							mFormattedHeight;
	S32							mFormattedRawWidth;
	S32							mFormattedRawHeight;
	S32							mFormattedCropOffset;
	bool						mFormattedCropVertically;
	LLFloaterSnapshot::ESnapshotFormat	mFormattedSnapshotFormat;
	S32							mFormattedSnapshotQuality;
	bool						mFormattedUpToDate;

	LLFrameTimer				mSnapshotDelayTimer;
	U32							mUsedBy;
	U32							mManualSizeOverride;
	S32							mShineCountdown;
	LLFrameTimer				mShineAnimTimer;
	F32							mFlashAlpha;
	BOOL						mNeedsFlash;
	LLVector3d					mPosTakenGlobal;
	S32							mSnapshotQuality;
	S32							mFormattedDataSize;
	ESnapshotType				mSnapshotType;
	LLFloaterSnapshot::ESnapshotFormat	mSnapshotFormat;
	BOOL						mShowFreezeFrameSnapshot;
	LLFrameTimer				mFallAnimTimer;
	LLVector3					mCameraPos;
	LLQuaternion				mCameraRot;
	BOOL						mSnapshotActive;
	LLViewerWindow::ESnapshotType mSnapshotBufferType;

	int							mOutstandingCallbacks;
	int							mSaveFailures;
	LLFloaterSnapshot*			mCloseCalled;
	static int					sSnapshotIndex;

public:
	static std::set<LLSnapshotLivePreview*> sList;
	LLFrameTimer				mFormattedDelayTimer;
};

///----------------------------------------------------------------------------
/// Class LLFloaterSnapshot::Impl
///----------------------------------------------------------------------------
class LLFloaterSnapshot::Impl
{
public:
	Impl()
	:	mAvatarPauseHandles(),
		mLastToolset(NULL)
	{
	}
	~Impl()
	{
		//unpause avatars
		mAvatarPauseHandles.clear();
		mQualityMouseUpConnection.disconnect();
	}
	static void onClickDiscard(void* data);
	static void onClickKeep(void* data);
	static void onCommitSave(LLUICtrl* ctrl, void* data);
	static void onClickNewSnapshot(void* data);
	static void onClickFreezeTime(void* data);
	static void onClickAutoSnap(LLUICtrl *ctrl, void* data);
	static void onClickTemporaryImage(LLUICtrl *ctrl, void* data);
	//static void onClickAdvanceSnap(LLUICtrl *ctrl, void* data);
	static void onClickLess(void* data) ;
	static void onClickMore(void* data) ;
	static void onClickUICheck(LLUICtrl *ctrl, void* data);
	static void onClickHUDCheck(LLUICtrl *ctrl, void* data);
	static void onClickKeepOpenCheck(LLUICtrl *ctrl, void* data);
	static void onClickKeepAspect(LLUICtrl* ctrl, void* data);
	static void onCommitQuality(LLUICtrl* ctrl, void* data);
	static void onCommitFeedResolution(LLUICtrl* ctrl, void* data);
	static void onCommitPostcardResolution(LLUICtrl* ctrl, void* data);
	static void onCommitTextureResolution(LLUICtrl* ctrl, void* data);
	static void onCommitLocalResolution(LLUICtrl* ctrl, void* data);
	static void onCommitFeedAspect(LLUICtrl* ctrl, void* data);
	static void onCommitPostcardAspect(LLUICtrl* ctrl, void* data);
	static void onCommitTextureAspect(LLUICtrl* ctrl, void* data);
	static void onCommitLocalAspect(LLUICtrl* ctrl, void* data);
	static void updateResolution(LLUICtrl* ctrl, void* data, bool update_controls = true);
	static void updateAspect(LLUICtrl* ctrl, void* data, bool update_controls = true);
	static void onCommitFreezeTime(LLUICtrl* ctrl, void* data);
	static void onCommitLayerTypes(LLUICtrl* ctrl, void*data);
	static void onCommitSnapshotType(LLUICtrl* ctrl, void* data);
	static void onCommitSnapshotFormat(LLUICtrl* ctrl, void* data);
	static void onCommitCustomResolution(LLUICtrl *ctrl, void* data);
	static void onCommitCustomAspect(LLUICtrl *ctrl, void* data);
	static void onQualityMouseUp(void* data);

	static LLSnapshotLivePreview* getPreviewView(void);
	static void setResolution(LLFloaterSnapshot* floater, const std::string& comboname, bool visible, bool update_controls = true);
	static void setAspect(LLFloaterSnapshot* floater, const std::string& comboname, bool update_controls = true);
	static void storeAspectSetting(LLComboBox* combo, const std::string& comboname);
	static void enforceAspect(LLFloaterSnapshot* floater, F32 new_aspect);
	static void enforceResolution(LLFloaterSnapshot* floater, F32 new_aspect);
	static void updateControls(LLFloaterSnapshot* floater, bool delayed_formatted = false);
	static void resetFeedAndPostcardAspect(LLFloaterSnapshot* floater);
	static void updateLayout(LLFloaterSnapshot* floater);
	static void freezeTime(bool on);
	static void keepAspect(LLFloaterSnapshot* view, bool on, bool force = false);

	static LLHandle<LLView> sPreviewHandle;
	
private:
	static LLSnapshotLivePreview::ESnapshotType getTypeIndex(LLFloaterSnapshot* floater);
	static ESnapshotFormat getFormatIndex(LLFloaterSnapshot* floater);
	static void comboSetCustom(LLFloaterSnapshot *floater, const std::string& comboname);
	static void checkAutoSnapshot(LLSnapshotLivePreview* floater, BOOL update_thumbnail = FALSE);

public:
	std::vector<LLAnimPauseRequest> mAvatarPauseHandles;

	LLToolset*	mLastToolset;
	boost::signals2::connection mQualityMouseUpConnection;
};

//----------------------------------------------------------------------------
// LLSnapshotLivePreview

//static
int LLSnapshotLivePreview::sSnapshotIndex;

void LLSnapshotLivePreview::setSnapshotBufferType(LLFloaterSnapshot* floater, LLViewerWindow::ESnapshotType type)
{
	mSnapshotBufferType = type;
	switch(type)
	{
	  case LLViewerWindow::SNAPSHOT_TYPE_COLOR:
		floater->childSetValue("layer_types", "colors");
		break;
	  case LLViewerWindow::SNAPSHOT_TYPE_DEPTH:
		floater->childSetValue("layer_types", "depth");
		break;
	}
}

BOOL LLSnapshotLivePreview::getRawSnapshotUpToDate() const
{
	return mRawSnapshotRenderUI == gSavedSettings.getBOOL("RenderUIInSnapshot") &&
		mRawSnapshotRenderHUD == gSavedSettings.getBOOL("RenderHUDInSnapshot") &&
		mRawSnapshotBufferType == mSnapshotBufferType;
}

BOOL LLSnapshotLivePreview::getSnapshotUpToDate() const
{
	return mFormattedUpToDate && getRawSnapshotUpToDate();
}

std::set<LLSnapshotLivePreview*> LLSnapshotLivePreview::sList;

LLSnapshotLivePreview::LLSnapshotLivePreview (const LLRect& rect) : 
	LLView(std::string("snapshot_live_preview"), rect, FALSE), 
	mColor(1.f, 0.f, 0.f, 0.5f), 
	mRawSnapshot(NULL),
	mRawSnapshotWidth(0),
	mRawSnapshotHeight(1),
	mRawSnapshotRenderUI(FALSE),
	mRawSnapshotRenderHUD(FALSE),
	mRawSnapshotBufferType(LLViewerWindow::SNAPSHOT_TYPE_COLOR),
	mThumbnailImage(NULL) ,
	mThumbnailWidth(0),
	mThumbnailHeight(0),
	mFormattedImage(NULL),
	mFormattedUpToDate(false),
	mUsedBy(0),
	mManualSizeOverride(0),
	mShineCountdown(0),
	mFlashAlpha(0.f),
	mNeedsFlash(TRUE),
	mSnapshotQuality(gSavedSettings.getS32("SnapshotQuality")),
	mFormattedDataSize(0),
	mSnapshotType((ESnapshotType)gSavedSettings.getS32("LastSnapshotType")),
	mSnapshotFormat(LLFloaterSnapshot::ESnapshotFormat(gSavedSettings.getS32("SnapshotFormat"))),
	mShowFreezeFrameSnapshot(FALSE),
	mCameraPos(LLViewerCamera::getInstance()->getOrigin()),
	mCameraRot(LLViewerCamera::getInstance()->getQuaternion()),
	mSnapshotActive(FALSE),
	mSnapshotBufferType(LLViewerWindow::SNAPSHOT_TYPE_COLOR),
	mCloseCalled(NULL)
{
	DoutEntering(dc::snapshot, "LLSnapshotLivePreview() [" << (void*)this << "]");
	setSnapshotQuality(gSavedSettings.getS32("SnapshotQuality"));
	mSnapshotDelayTimer.start();
	sList.insert(this);
	setFollowsAll();
	mWidth = gViewerWindow->getWindowDisplayWidth();
	mHeight = gViewerWindow->getWindowDisplayHeight();
	mAspectRatio = (F32)mWidth / mHeight;
	mFallWidth = mWidth;
	mFallHeight = mHeight;
	mFullScreenPreviewTextureNeedsScaling = FALSE;
	mFallFullScreenPreviewTextureNeedsScaling = FALSE;

	mMaxImageSize = MAX_SNAPSHOT_IMAGE_SIZE ;
	mThumbnailUpdateLock = FALSE ;
	mThumbnailUpToDate   = FALSE ;
	updateSnapshot(TRUE,TRUE); //To initialize mImageRect to correct values
}

LLSnapshotLivePreview::~LLSnapshotLivePreview()
{
	DoutEntering(dc::snapshot, "~LLSnapshotLivePreview() [" << (void*)this << "]");
	sList.erase(this);
	// Stop callbacks from using this object.
	++sSnapshotIndex;
}

void LLSnapshotLivePreview::setMaxImageSize(S32 size) 
{
	if(size < MAX_SNAPSHOT_IMAGE_SIZE)
	{
		mMaxImageSize = size;
	}
	else
	{
		mMaxImageSize = MAX_SNAPSHOT_IMAGE_SIZE ;
	}
}

LLViewerTexture* LLSnapshotLivePreview::getCurrentImage()
{
	return mFullScreenPreviewTexture;
}

void LLSnapshotLivePreview::showFreezeFrameSnapshot(bool show)
{
	DoutEntering(dc::snapshot, "LLSnapshotLivePreview::showFreezeFrameSnapshot(" << show << ")");
	// If we stop to show the fullscreen "freeze frame" snapshot, play the "fall away" animation.
	if (mShowFreezeFrameSnapshot && !show)
	{
		mFallFullScreenPreviewTexture = mFullScreenPreviewTexture;
		mFallFullScreenPreviewTextureNeedsScaling = mFullScreenPreviewTextureNeedsScaling;
		mFallFullScreenImageRect = mFullScreenImageRect;
		mFallWidth = mFormattedWidth;
		mFallHeight = mFormattedHeight;
		mFallAnimTimer.start();		
	}
	mShowFreezeFrameSnapshot = show;
}

void LLSnapshotLivePreview::updateSnapshot(BOOL new_snapshot, BOOL new_thumbnail, F32 delay) 
{
	DoutEntering(dc::snapshot, "LLSnapshotLivePreview::updateSnapshot(" << new_snapshot << ", " << new_thumbnail << ", " << delay << ")");

	LLRect& rect = mFullScreenImageRect;
	rect.set(0, getRect().getHeight(), getRect().getWidth(), 0);

	S32 window_width = gViewerWindow->getWindowWidthRaw() ;
	S32 window_height = gViewerWindow->getWindowHeightRaw() ;
	F32 window_aspect_ratio = ((F32)window_width) / ((F32)window_height);

	if (mAspectRatio > window_aspect_ratio)
	{
		// trim off top and bottom
		S32 height_diff = llround(getRect().getHeight() - (F32)getRect().getWidth() / mAspectRatio);
		S32 half_height_diff = llround((getRect().getHeight() - (F32)getRect().getWidth() / mAspectRatio) * 0.5);
		rect.mBottom += half_height_diff;
		rect.mTop -= height_diff - half_height_diff;
	}
	else if (mAspectRatio < window_aspect_ratio)
	{
		// trim off left and right
		S32 width_diff = llround(getRect().getWidth() - (F32)getRect().getHeight() * mAspectRatio);
		S32 half_width_diff = llround((getRect().getWidth() - (F32)getRect().getHeight() * mAspectRatio) * 0.5f);
		rect.mLeft += half_width_diff;
		rect.mRight -= width_diff - half_width_diff;
	}

	mShineAnimTimer.stop();
	if (new_snapshot)
	{
		mSnapshotDelayTimer.start(delay);
	}
	if(new_thumbnail)
	{
		mThumbnailUpToDate = FALSE ;
	}
	setThumbnailImageSize();
}

void LLSnapshotLivePreview::setSnapshotQuality(S32 quality)
{
	llclamp(quality, 0, 100);
	if (quality != mSnapshotQuality)
	{
		mSnapshotQuality = quality;
		gSavedSettings.setS32("SnapshotQuality", quality);
	}
}

void LLSnapshotLivePreview::drawPreviewRect(S32 offset_x, S32 offset_y)
{
	// Draw two alpha rectangles to cover areas outside of the snapshot image.
	LLColor4 alpha_color(0.5f, 0.5f, 0.5f, 0.8f);
	if (mThumbnailWidth > mThumbnailPreviewRect.getWidth())
	{
		gl_rect_2d(                      offset_x,                        offset_y,
				   mThumbnailPreviewRect.mLeft  + offset_x, mThumbnailHeight     + offset_y,
				   alpha_color, TRUE);
		gl_rect_2d(mThumbnailPreviewRect.mRight + offset_x,                        offset_y,
				   mThumbnailWidth     + offset_x, mThumbnailHeight     + offset_y,
				   alpha_color, TRUE);
	}
	if (mThumbnailHeight > mThumbnailPreviewRect.getHeight())
	{
		gl_rect_2d(                      offset_x,                        offset_y,
				   mThumbnailWidth     + offset_x, mThumbnailPreviewRect.mBottom + offset_y,
				   alpha_color, TRUE);
		gl_rect_2d(                      offset_x, mThumbnailPreviewRect.mTop    + offset_y,
				   mThumbnailWidth     + offset_x, mThumbnailHeight     + offset_y,
				   alpha_color, TRUE);
	}
	// Draw border around captured part.
	F32 line_width; 
	glGetFloatv(GL_LINE_WIDTH, &line_width);
	glLineWidth(2.0f * line_width);
	gl_rect_2d( mThumbnailPreviewRect.mLeft  + offset_x, mThumbnailPreviewRect.mTop    + offset_y,
				mThumbnailPreviewRect.mRight + offset_x, mThumbnailPreviewRect.mBottom + offset_y,
				LLColor4::black, FALSE);
	glLineWidth(line_width);
}

//called when the frame is frozen.
void LLSnapshotLivePreview::draw()
{
	if (mFullScreenPreviewTexture.notNull() &&
	    mShowFreezeFrameSnapshot)
	{
		LLColor4 bg_color(0.f, 0.f, 0.3f, 0.4f);
		gl_rect_2d(getRect(), bg_color);
		LLRect const& rect = mFullScreenImageRect;
		LLRect shadow_rect = mFullScreenImageRect;
		shadow_rect.stretch(BORDER_WIDTH);
		gl_drop_shadow(shadow_rect.mLeft, shadow_rect.mTop, shadow_rect.mRight, shadow_rect.mBottom, LLColor4(0.f, 0.f, 0.f, mNeedsFlash ? 0.f :0.5f), 10);

		LLColor4 image_color(1.f, 1.f, 1.f, 1.f);
		gGL.color4fv(image_color.mV);
		gGL.getTexUnit(0)->bind(mFullScreenPreviewTexture);
		// calculate UV scale
		F32 uv_width = mFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFormattedWidth / (F32)mFullScreenPreviewTexture->getWidth(), 1.f) : 1.f;
		F32 uv_height = mFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFormattedHeight / (F32)mFullScreenPreviewTexture->getHeight(), 1.f) : 1.f;
		gGL.pushMatrix();
		{
			gGL.translatef((F32)rect.mLeft, (F32)rect.mBottom, 0.f);
			gGL.begin(LLRender::QUADS);
			{
				gGL.texCoord2f(uv_width, uv_height);
				gGL.vertex2i(rect.getWidth(), rect.getHeight() );

				gGL.texCoord2f(0.f, uv_height);
				gGL.vertex2i(0, rect.getHeight() );

				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2i(0, 0);

				gGL.texCoord2f(uv_width, 0.f);
				gGL.vertex2i(rect.getWidth(), 0);
			}
			gGL.end();
		}
		gGL.popMatrix();

		gGL.color4f(1.f, 1.f, 1.f, mFlashAlpha);
		gl_rect_2d(getRect());
		if (mNeedsFlash)
		{
			if (mFlashAlpha < 1.f)
			{
				mFlashAlpha = lerp(mFlashAlpha, 1.f, LLCriticalDamp::getInterpolant(0.02f));
			}
			else
			{
				mNeedsFlash = FALSE;
			}
		}
		else
		{
			mFlashAlpha = lerp(mFlashAlpha, 0.f, LLCriticalDamp::getInterpolant(0.15f));
		}

		if (mShineCountdown > 0)
		{
			mShineCountdown--;
			if (mShineCountdown == 0)
			{
				mShineAnimTimer.start();
			}
		}
		else if (mShineAnimTimer.getStarted())
		{
			//LLDebugVarMessageBox::show("Shine time", &SHINE_TIME, 10.f, 0.1f);
			//LLDebugVarMessageBox::show("Shine width", &SHINE_WIDTH, 2.f, 0.05f);
			//LLDebugVarMessageBox::show("Shine opacity", &SHINE_OPACITY, 1.f, 0.05f);

			F32 shine_interp = llmin(1.f, mShineAnimTimer.getElapsedTimeF32() / SHINE_TIME);
			
			// draw "shine" effect
			LLLocalClipRect clip(getLocalRect());
			{
				// draw diagonal stripe with gradient that passes over screen
				S32 x1 = gViewerWindow->getWindowWidthScaled() * llround((clamp_rescale(shine_interp, 0.f, 1.f, -1.f - SHINE_WIDTH, 1.f)));
				S32 x2 = x1 + llround(gViewerWindow->getWindowWidthScaled() * SHINE_WIDTH);
				S32 x3 = x2 + llround(gViewerWindow->getWindowWidthScaled() * SHINE_WIDTH);
				S32 y1 = 0;
				S32 y2 = gViewerWindow->getWindowHeightScaled();

				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				gGL.begin(LLRender::QUADS);
				{
					gGL.color4f(1.f, 1.f, 1.f, 0.f);
					gGL.vertex2i(x1, y1);
					gGL.vertex2i(x1 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.color4f(1.f, 1.f, 1.f, SHINE_OPACITY);
					gGL.vertex2i(x2 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.vertex2i(x2, y1);

					gGL.color4f(1.f, 1.f, 1.f, SHINE_OPACITY);
					gGL.vertex2i(x2, y1);
					gGL.vertex2i(x2 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.color4f(1.f, 1.f, 1.f, 0.f);
					gGL.vertex2i(x3 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.vertex2i(x3, y1);
				}
				gGL.end();
			}

			// if we're at the end of the animation, stop
			if (shine_interp >= 1.f)
			{
				mShineAnimTimer.stop();
			}
		}
	}

	// draw framing rectangle
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		LLRect outline_rect = mFullScreenImageRect;
		gGL.begin(LLRender::QUADS);
		{
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mLeft, outline_rect.mTop);

			gGL.vertex2i(outline_rect.mLeft, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);

			gGL.vertex2i(outline_rect.mLeft, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mLeft, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);

			gGL.vertex2i(outline_rect.mRight, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);
		}
		gGL.end();
	}

	// draw old image dropping away
	if (mFallAnimTimer.getStarted())
	{
		if (mFallFullScreenPreviewTexture.notNull() && mFallAnimTimer.getElapsedTimeF32() < FALL_TIME)
		{
			F32 fall_interp = mFallAnimTimer.getElapsedTimeF32() / FALL_TIME;
			F32 alpha = clamp_rescale(fall_interp, 0.f, 1.f, 0.8f, 0.4f);
			LLColor4 image_color(1.f, 1.f, 1.f, alpha);
			gGL.color4fv(image_color.mV);
			gGL.getTexUnit(0)->bind(mFallFullScreenPreviewTexture);
			// calculate UV scale
			F32 uv_width = mFallFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFallWidth / (F32)mFallFullScreenPreviewTexture->getWidth(), 1.f) : 1.f;
			F32 uv_height = mFallFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFallHeight / (F32)mFallFullScreenPreviewTexture->getHeight(), 1.f) : 1.f;
			gGL.pushMatrix();
			{
				LLRect const& rect = mFallFullScreenImageRect;
				gGL.translatef((F32)rect.mLeft, (F32)rect.mBottom - llround(getRect().getHeight() * 2.f * (fall_interp * fall_interp)), 0.f);
				gGL.rotatef(-45.f * fall_interp, 0.f, 0.f, 1.f);
				gGL.begin(LLRender::QUADS);
				{
					gGL.texCoord2f(uv_width, uv_height);
					gGL.vertex2i(rect.getWidth(), rect.getHeight() );

					gGL.texCoord2f(0.f, uv_height);
					gGL.vertex2i(0, rect.getHeight() );

					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex2i(0, 0);

					gGL.texCoord2f(uv_width, 0.f);
					gGL.vertex2i(rect.getWidth(), 0);
				}
				gGL.end();
			}
			gGL.popMatrix();
		}
	}
}

/*virtual*/ 
void LLSnapshotLivePreview::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLRect old_rect = getRect();
	LLView::reshape(width, height, called_from_parent);
	if (old_rect.getWidth() != width || old_rect.getHeight() != height)
	{
		updateSnapshot(FALSE, TRUE);
	}
}

BOOL LLSnapshotLivePreview::setThumbnailImageSize()
{
	if(mWidth < 10 || mHeight < 10)
	{
		return FALSE ;
	}
	S32 window_width = gViewerWindow->getWindowWidthRaw() ;
	S32 window_height = gViewerWindow->getWindowHeightRaw() ;

	F32 window_aspect_ratio = ((F32)window_width) / ((F32)window_height);

	// UI size for thumbnail
	S32 max_width =  THUMBHEIGHT * 4 / 3; // == LLFloaterSnapshot::getUIWinWidth() - 7;
	S32 max_height = THUMBHEIGHT;

	if (window_aspect_ratio > (F32)max_width / max_height)
	{
		// image too wide, shrink to width
		mThumbnailWidth = max_width;
		mThumbnailHeight = llround((F32)max_width / window_aspect_ratio);
	}
	else
	{
		// image too tall, shrink to height
		mThumbnailHeight = max_height;
		mThumbnailWidth = llround((F32)max_height * window_aspect_ratio);
	}
	
	if(mThumbnailWidth > window_width || mThumbnailHeight > window_height)
	{
		return FALSE ;//if the window is too small, ignore thumbnail updating.
	}

	S32 left = 0 , top = mThumbnailHeight, right = mThumbnailWidth, bottom = 0 ;
	F32 ratio = mAspectRatio * window_height / window_width;
	if(ratio > 1.f)
	{
		top = llround(top / ratio);
	}
	else
	{
		right = llround(right * ratio);
	}			
	left = (mThumbnailWidth - right + 1) / 2;
	bottom = (mThumbnailHeight - top + 1) / 2;
	top += bottom ;
	right += left ;
	mThumbnailPreviewRect.set(left - 1, top + 1, right + 1, bottom - 1) ;

	return TRUE ;
}

void LLSnapshotLivePreview::generateThumbnailImage(void)
{	
	if(mThumbnailUpdateLock) //in the process of updating
	{
		return ;
	}
	if(mThumbnailUpToDate)	//already updated
	{
		return ;
	}
	if(mWidth < 10 || mHeight < 10)
	{
		return ;
	}

	////lock updating
	mThumbnailUpdateLock = TRUE ;

	if(!setThumbnailImageSize())
	{
		mThumbnailUpdateLock = FALSE ;
		mThumbnailUpToDate = TRUE ;
		return ;
	}

	Dout(dc::snapshot, "LLSnapshotLivePreview::generateThumbnailImage: Actually making a new thumbnail!");

	mThumbnailImage = NULL;

	S32 w , h ;
	// Set w, h to the nearest power of two that is larger or equal to mThumbnailWidth, mThumbnailHeight (but never more than 1024).
	w = get_lower_power_two(mThumbnailWidth - 1, 512) * 2 ;		// -1 so that w becomes mThumbnailWidth if it is a power of 2, instead of 2 * mThumbnailWidth.
	h = get_lower_power_two(mThumbnailHeight - 1, 512) * 2 ;

	LLPointer<LLImageRaw> raw = new LLImageRaw ;
	if(!gViewerWindow->thumbnailSnapshot(raw,
							w, h,
							gSavedSettings.getBOOL("RenderUIInSnapshot"),
							FALSE,
							mSnapshotBufferType) )								
	{
		raw = NULL ;
	}

	if(raw)
	{
		mThumbnailImage = LLViewerTextureManager::getLocalTexture(raw.get(), FALSE); 		
		mThumbnailUpToDate = TRUE ;
		Dout(dc::snapshot, "thumbnailSnapshot(" << w << ", " << h << ", ...) returns " << raw->getWidth() << ", " << raw->getHeight());
	}

	//unlock updating
	mThumbnailUpdateLock = FALSE ;		
}


// Called often. Checks whether it's time to grab a new snapshot and if so, does it.
// Returns TRUE if new snapshot generated, FALSE otherwise.
//static 
BOOL LLSnapshotLivePreview::onIdle(LLSnapshotLivePreview* previewp)
{
	LLVector3 new_camera_pos = LLViewerCamera::getInstance()->getOrigin();
	LLQuaternion new_camera_rot = LLViewerCamera::getInstance()->getQuaternion();
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (freeze_time && 
		(new_camera_pos != previewp->mCameraPos || dot(new_camera_rot, previewp->mCameraRot) < 0.995f))
	{
		previewp->mCameraPos = new_camera_pos;
		previewp->mCameraRot = new_camera_rot;
		previewp->showFreezeFrameSnapshot(false);
		// request a new snapshot whenever the camera moves, with a time delay
		BOOL autosnap = gSavedSettings.getBOOL("AutoSnapshot");
		previewp->updateSnapshot(
			autosnap, // whether a new snapshot is needed or merely invalidate the existing one
			FALSE, // or if 1st arg is false, whether to produce a new thumbnail image.
			autosnap ? AUTO_SNAPSHOT_TIME_DELAY : 0.f); // shutter delay if 1st arg is true.
	}

	// See if it's time yet to snap the shot and bomb out otherwise.
	previewp->mSnapshotActive = 
		(previewp->mSnapshotDelayTimer.getStarted() && previewp->mSnapshotDelayTimer.hasExpired())
		&& !LLToolCamera::getInstance()->hasMouseCapture(); // don't take snapshots while ALT-zoom active
	if (!previewp->mSnapshotActive)
	{
		if (previewp->mFormattedDelayTimer.getStarted() && previewp->mFormattedDelayTimer.hasExpired())
		{
			previewp->mFormattedDelayTimer.stop();
			LLFloaterSnapshot::updateControls();
		}
		return FALSE;
	}

	// Time to produce a snapshot.

	// Reset Feed and Postcard Aspect to Default.
	LLFloaterSnapshot::resetFeedAndPostcardAspect();

	if (!previewp->mRawSnapshot)
	{
		previewp->mRawSnapshot = new LLImageRaw;
	}

	previewp->setVisible(FALSE);
	previewp->setEnabled(FALSE);
	
	previewp->getWindow()->incBusyCount();

	// Grab the raw image and encode it into desired format.
	Dout(dc::snapshot, "LLSnapshotLivePreview::onIdle: Actually making a new snapshot!");

	// This should never happen, but well. If it's true then that means that the
	// snapshot floater is disabled. Incrementing sSnapshotIndex will cause the
	// callbacks to be ignored, so we better make sure that the floater is enabled.
	if (previewp->mCloseCalled)
	{
		previewp->mCloseCalled->setEnabled(TRUE);
		previewp->mCloseCalled->setVisible(TRUE);
	}
	previewp->sSnapshotIndex++;
	Dout(dc::snapshot, "sSnapshotIndex is now " << previewp->sSnapshotIndex << "; mOutstandingCallbacks reset to 0.");
	previewp->mOutstandingCallbacks = 0;	// There are no outstanding callbacks for THIS snapshot.
	previewp->mSaveFailures = 0;			// There were no upload failures (or attempts for that matter) for this snapshot.

	previewp->mFormattedImage = NULL;
	previewp->mFormattedUpToDate = false;
	previewp->mUsedBy = 0;					// This snapshot wasn't used yet.
	previewp->mManualSizeOverride = 0;
	previewp->mThumbnailUpToDate = FALSE;

	// Remember with what parameters this snapshot was taken.
	previewp->mRawSnapshotRenderUI = gSavedSettings.getBOOL("RenderUIInSnapshot");
	previewp->mRawSnapshotRenderHUD = gSavedSettings.getBOOL("RenderHUDInSnapshot");
	previewp->mRawSnapshotBufferType = previewp->mSnapshotBufferType;
	// Mark that the those values do not represent the current snapshot (yet).
	previewp->mRawSnapshotWidth = 0;
	previewp->mRawSnapshotHeight = 1;	// Avoid division by zero when calculation an aspect in LLSnapshotLivePreview::getAspectSizeProblem.

	if (gViewerWindow->rawRawSnapshot(
							previewp->mRawSnapshot,
							previewp->mWidth,
							previewp->mHeight,
							previewp->mAspectRatio,
							previewp->mRawSnapshotRenderUI,
							FALSE,
							previewp->mRawSnapshotBufferType,
							previewp->getMaxImageSize(), 1.f, true))
	{
		// On success, cache the size of the raw snapshot.
		previewp->mRawSnapshotWidth = previewp->mRawSnapshot->getWidth();
		previewp->mRawSnapshotHeight = previewp->mRawSnapshot->getHeight();
		Dout(dc::snapshot, "Created a new raw snapshot of size " << previewp->mRawSnapshotWidth << "x" << previewp->mRawSnapshotHeight);

		previewp->mPosTakenGlobal = gAgentCamera.getCameraPositionGlobal();
		EAspectSizeProblem ret = previewp->generateFormattedAndFullscreenPreview();
		// If the required size is too large, we might have to scale up one dimension... So that is ok.
		// For example, if the target is 1000x1000 and the aspect is set to 8:1, then the required
		// raw snapshot would be 8000x1000, but since 8000 is too large it would end up being only
		// 6144x768 (since 6144 is the maximum) and we'll have to stretch the 768 to 1000.
		llassert(previewp->mFormattedUpToDate || ret == SIZE_TOO_LARGE || ret == ENCODING_FAILED);
		if (!previewp->mFormattedUpToDate && ret == SIZE_TOO_LARGE)
		{
			// ENCODING_FAILED was already reported.
			LLNotificationsUtil::add("ErrorSizeAspectSnapshot");
		}
    }

	previewp->getWindow()->decBusyCount();

	// Only show fullscreen preview when in freeze frame mode.
	previewp->setVisible(gSavedSettings.getBOOL("FreezeTime"));
	previewp->mSnapshotDelayTimer.stop();
	previewp->mSnapshotActive = FALSE;
	previewp->generateThumbnailImage();

	return TRUE;
}

LLSnapshotLivePreview::EAspectSizeProblem LLSnapshotLivePreview::getAspectSizeProblem(S32& width_out, S32& height_out, bool& crop_vertically_out, S32& crop_offset_out)
{
	S32 const window_width = gViewerWindow->getWindowWidthRaw();
	S32 const window_height = gViewerWindow->getWindowHeightRaw();
	F32 const window_aspect = (F32)window_width / window_height;

	// We need an image with the aspect mAspectRatio (from which mWidth and mHeight were derived).

	// The current aspect ratio of mRawSnapshot. This should be (almost) equal to window_width / window_height,
	// since these values are calculated in rawRawSnapshot with llround(window_width * scale_factor) and
	// llround(window_height * scale_factor) respectively (since we set uncrop = true).
	F32 raw_aspect = (F32)mRawSnapshotWidth / mRawSnapshotHeight;
	// The smaller dimension might have been rounded up to 0.5 up or down. Calculate upper and lower limits.
	F32 lower_raw_aspect = (mRawSnapshotWidth - 0.5) / (mRawSnapshotHeight + 0.5);
	F32 upper_raw_aspect = (mRawSnapshotWidth + 0.5) / (mRawSnapshotHeight - 0.5);
	// Find out if mRawSnapshot was cropped already.
	bool const allow_vertical_crop = window_height * upper_raw_aspect >= window_width;			// mRawSnapshot was cropped horizontally.
	bool const allow_horizontal_crop = window_width / lower_raw_aspect >= window_height;		// mRawSnapshot was cropped vertically.

	if (lower_raw_aspect <= window_aspect && window_aspect <= upper_raw_aspect)	// Was rawRawSnapshot called?
	{
	  // NEW: Since we pass uncrop = true to rawRawSnapshot, these should now always both be true.
	  // Also, mRawSnapshotWidth or mRawSnapshotHeight is calculated from window_aspect to begin
	  // with and a better approximation of raw_aspect for the following calculations is to use this source.
	  llassert(allow_vertical_crop && allow_horizontal_crop);
	  raw_aspect = window_aspect;			// Go back to the source.
	}

	crop_vertically_out = true;			// Prefer this, as it is faster.
	crop_offset_out = 0;				// The number of columns or rows to cut off on the left or bottom.
	// Construct a rectangle size w,h that fits inside mRawSnapshot but has the correct aspect.
	width_out = mRawSnapshotWidth;
	height_out = mRawSnapshotHeight;
	if (mAspectRatio < lower_raw_aspect)
	{
		width_out = llround(width_out * mAspectRatio / raw_aspect);
		if (width_out < mRawSnapshotWidth)
		{
			// Only set this to false when there is actually something to crop.
			crop_vertically_out = false;
			if (!allow_horizontal_crop)
			{
				Dout(dc::snapshot, "NOT up to date: required aspect " << mAspectRatio <<
					" is less than the (lower) raw aspect " << lower_raw_aspect << " which is already vertically cropped.");
				return CANNOT_CROP_HORIZONTALLY;
			}
			crop_offset_out = (mRawSnapshotWidth - width_out) / 2;
		}
	}
	else if (mAspectRatio > upper_raw_aspect)
	{
		height_out = llround(height_out * raw_aspect / mAspectRatio);
		if (height_out < mRawSnapshotHeight)
		{
			if (!allow_vertical_crop)
			{
				Dout(dc::snapshot, "NOT up to date: required aspect " << mAspectRatio <<
					" is larger than the (upper) raw aspect " << upper_raw_aspect << " which is already horizontally cropped.");
				return CANNOT_CROP_VERTICALLY;
			}
			crop_offset_out = (mRawSnapshotHeight - height_out) / 2;
		}
	}
	// Never allow upscaling of the existing snapshot, of course.
	if (mWidth > width_out || mHeight > height_out)
	{
		Dout(dc::snapshot, "NOT up to date: required target size " << mWidth << "x" << mHeight <<
			" is larger than the raw snapshot with size " << width_out << "x" << height_out << "!");
		return SIZE_TOO_LARGE;
	}
	// Do not allow any resizing for target images that are equal or larger than the current window, when the target is the harddisk.
	// If the target size is smaller, than a resize would occur anyway, so in that case we allow resizing whatever we have,
	// unless the aspect is different in which case we have to allow resizing in one direction.
	if (mSnapshotType == SNAPSHOT_LOCAL && (mWidth >= window_width || mHeight >= window_height) && mWidth != width_out && mHeight != height_out)
	{
		Dout(dc::snapshot, "NOT up to date: required target size " << mWidth << "x" << mHeight <<
			" is larger or equal the window size (" << window_width << "x" << window_height << ")"
			" but unequal the the raw snapshot size (" << width_out << "x" << height_out << ")"
			" and target is disk!");
		return CANNOT_RESIZE;
	}
	return ASPECTSIZE_OK;
}

LLSnapshotLivePreview::EAspectSizeProblem LLSnapshotLivePreview::generateFormattedAndFullscreenPreview(bool delayed)
{
	DoutEntering(dc::snapshot, "LLSnapshotLivePreview::generateFormattedAndFullscreenPreview(" << delayed << ")");
	mFormattedUpToDate = false;

	S32 w, h, crop_offset;
	bool crop_vertically;
	EAspectSizeProblem ret = getAspectSizeProblem(w, h, crop_vertically, crop_offset);
	if (ret != ASPECTSIZE_OK)
	{
		// Current raw snapshot cannot be used.
		return ret;
	}

	// Determine the required format.
	LLFloaterSnapshot::ESnapshotFormat format;
	switch (mSnapshotType)
	{
	  case SNAPSHOT_FEED:
		// Feeds hardcoded to always use png.
		format = LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG;
		break;
	  case SNAPSHOT_POSTCARD:
		// Postcards hardcoded to always use jpeg.
		format = LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG;
		break;
	  case SNAPSHOT_TEXTURE:
		format = LLFloaterSnapshot::SNAPSHOT_FORMAT_J2C;
		break;
	  case SNAPSHOT_LOCAL:
		format = mSnapshotFormat;
		break;
	  default:
		format = mSnapshotFormat;
		break;
	}

	if (mFormattedImage &&
		mFormattedWidth == mWidth &&
		mFormattedHeight == mHeight &&
		mFormattedRawWidth == w &&
		mFormattedRawHeight == h &&
		mFormattedCropOffset == crop_offset &&
		mFormattedCropVertically == crop_vertically &&
		mFormattedSnapshotFormat == format &&
		(mFormattedSnapshotQuality == mSnapshotQuality ||
		 format != LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG))
	{
		Dout(dc::snapshot, "Already up to date.");
		mFormattedUpToDate = true;
		return ret;
	}

#ifdef CWDEBUG
	if (!mFormattedImage)
	{
		Dout(dc::snapshot, "mFormattedImage == NULL!");
	}
	else
	{
		if (mFormattedWidth != mWidth)
		  Dout(dc::snapshot, "Target width changed from " << mFormattedWidth << " to " << mWidth);
		if (mFormattedHeight != mHeight)
		  Dout(dc::snapshot, "Target height changed from " << mFormattedHeight << " to " << mHeight);
		if (mFormattedRawWidth != w)
		  Dout(dc::snapshot, "Cropped (raw) width changed from " << mFormattedRawWidth << " to " << w);
		if (mFormattedRawHeight != h)
		  Dout(dc::snapshot, "Cropped (raw) height changed from " << mFormattedRawHeight << " to " << h);
		if (mFormattedCropOffset != crop_offset)
		  Dout(dc::snapshot, "Crop offset changed from " << mFormattedCropOffset << " to " << crop_offset);
		if (mFormattedCropVertically != crop_vertically)
		  Dout(dc::snapshot, "Crop direction changed from " << (mFormattedCropVertically ? "vertical" : "horizontal") << " to " << (crop_vertically ? "vertical" : "horizontal"));
		if (mFormattedSnapshotFormat != format)
		  Dout(dc::snapshot, "Format changed from " << mFormattedSnapshotFormat << " to " << format);
		if (!(mFormattedSnapshotQuality == mSnapshotQuality || format != LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG))
		  Dout(dc::snapshot, "Format is JPEG and quality changed from " << mFormattedSnapshotQuality << " to " << mSnapshotQuality);
	}
#endif

	if (delayed)
	{
		// Just set mFormattedUpToDate.
		Dout(dc::snapshot, "NOT up to date, but delayed. Returning.");
		return DELAYED;
	}

	if (!mRawSnapshot)
	{
		Dout(dc::snapshot, "No raw snapshot exists.");
		return NO_RAW_IMAGE;
	}

	Dout(dc::snapshot, "Generating a new formatted image!");

	mFormattedWidth = mWidth;
	mFormattedHeight = mHeight;
	mFormattedRawWidth = w;
	mFormattedRawHeight = h;
	mFormattedCropOffset = crop_offset;
	mFormattedCropVertically = crop_vertically;
	mFormattedSnapshotFormat = format;
	mFormattedSnapshotQuality = mSnapshotQuality;

	// Served, so no need to call this again.
	mFormattedDelayTimer.stop();

	// A cropped copy of raw snapshot is put in 'scaled', which then is
	// scaled to the target size (or a close power of two in the case
	// of textures) and then encoded to 'formatted' of the same size.
	LLPointer<LLImageRaw> scaled = new LLImageRaw(mRawSnapshot, w, h, crop_offset, crop_vertically);

	// Subsequently, 'formatted' is decoded into 'decoded' again,
	// to serve the full screen preview.
	LLPointer<LLImageRaw> decoded = new LLImageRaw;

	// Scale 'scaled' and allocate the correct image format.
	if (mSnapshotType == SNAPSHOT_TEXTURE)
	{
		// 'scaled' must be a power of two.
		scaled->biasedScaleToPowerOfTwo(mWidth, mHeight, 512);
	}
	else
	{
		// 'scaled' is just the target size.
		scaled->scale(mWidth, mHeight);
	}

	bool lossless = false;
	switch(format)
	{
	  case LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG:
		mFormattedImage = new LLImagePNG; 
		lossless = true;
		break;
	  case LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG:
		mFormattedImage = new LLImageJPEG(mSnapshotQuality); 
		break;
	  case LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP:
		mFormattedImage = new LLImageBMP; 
		lossless = true;
		break;
	  case LLFloaterSnapshot::SNAPSHOT_FORMAT_J2C:
		mFormattedImage = new LLImageJ2C;
		break;
	}

	// Encode the scaled snapshot into 'mFormatted' for save/upload.
	if (mFormattedImage->encode(scaled, 0))
	{
		mFormattedDataSize = mFormattedImage->getDataSize();
		if (!lossless)
		{
			// decoded must be pre-allocated.
			decoded->resize(
				mFormattedImage->getWidth(),
				mFormattedImage->getHeight(),
				mFormattedImage->getComponents());
			mFormattedImage->decode(decoded, 0);
		}
	}
	else
	{
		// Mark formatted image size as unknown.
		mFormattedDataSize = 0;
		// Disable upload/save button.
		mFormattedImage = NULL;
		ret = ENCODING_FAILED;
		LLNotificationsUtil::add("ErrorEncodingSnapshot");
	}

	// Generate mFullScreenPreviewTexture

	// Copy 'decoded' into 'scaled'.
	if (!lossless)		// Otherwise scaled is already "decoded".
	{
		scaled = NULL;	// Free memory before allocating new image.
		scaled = new LLImageRaw(
			decoded->getData(),
			decoded->getWidth(),
			decoded->getHeight(),
			decoded->getComponents());
	}
	
	// Generate mFullScreenPreviewTexture from 'scaled'.
	if (!scaled->isBufferInvalid())
	{
		// Leave original image dimensions, just scale up texture buffer.
		if (scaled->getWidth() > 1024 || scaled->getHeight() > 1024)
		{
			// Go ahead and shrink image to appropriate power of two for display.
			scaled->biasedScaleToPowerOfTwo(1024);
			mFullScreenPreviewTextureNeedsScaling = FALSE;
		}
		else
		{
			// Expand image but keep original image data intact.
			scaled->expandToPowerOfTwo(1024, FALSE);
			mFullScreenPreviewTextureNeedsScaling = TRUE;
		}
		mFullScreenPreviewTexture = LLViewerTextureManager::getLocalTexture(scaled.get(), FALSE);
		LLPointer<LLViewerTexture> curr_preview_texture = mFullScreenPreviewTexture;
		gGL.getTexUnit(0)->bind(curr_preview_texture);
		if (mSnapshotType != SNAPSHOT_TEXTURE)
		{
			curr_preview_texture->setFilteringOption(LLTexUnit::TFO_POINT);
		}
		else
		{
			curr_preview_texture->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
		}
		curr_preview_texture->setAddressMode(LLTexUnit::TAM_CLAMP);

		showFreezeFrameSnapshot(TRUE);

		mShineCountdown = 4; // wait a few frames to avoid animation glitch due to readback this frame
	}

	// Return success if mFormattedImage was succesfully generated.
	mFormattedUpToDate = mFormattedImage;
	return ret;
}

void LLSnapshotLivePreview::setSize(S32 w, S32 h)
{
	mWidth = w;
	mHeight = h;
}

void LLSnapshotLivePreview::getSize(S32& w, S32& h) const
{
	w = mWidth;
	h = mHeight;
}

void LLSnapshotLivePreview::setAspect(F32 a)
{
	mAspectRatio = a;
}

F32 LLSnapshotLivePreview::getAspect() const
{
	return mAspectRatio;
}

void LLSnapshotLivePreview::getRawSize(S32& w, S32& h) const
{
	w = mRawSnapshotWidth;
	h = mRawSnapshotHeight;
}

LLFloaterFeed* LLSnapshotLivePreview::getCaptionAndSaveFeed()
{
	if (mCloseCalled)
	{
		return NULL;
	}

	++mOutstandingCallbacks;
	mSaveFailures = 0;
	addUsedBy(SNAPSHOT_FEED);
	Dout(dc::snapshot, "LLSnapshotLivePreview::getCaptionAndSaveFeed: sSnapshotIndex = " << sSnapshotIndex << "; mOutstandingCallbacks = " << mOutstandingCallbacks << ".");

	if (mFullScreenPreviewTexture.isNull())
	{
		// This should never happen!
		llwarns << "The snapshot image has not been generated!" << llendl;
		saveDone(SNAPSHOT_FEED, false, sSnapshotIndex);
		return NULL;
	}

	// Calculate and pass in image scale in case image data only uses portion of viewerimage buffer.
	F32 uv_width = mFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFormattedWidth / (F32)mFullScreenPreviewTexture->getWidth(), 1.f) : 1.f;
	F32 uv_height = mFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFormattedHeight / (F32)mFullScreenPreviewTexture->getHeight(), 1.f) : 1.f;
	LLVector2 image_scale(uv_width, uv_height);

	LLImagePNG* png = dynamic_cast<LLImagePNG*>(mFormattedImage.get());
	if (!png)
	{
		llwarns << "Formatted image not a PNG" << llendl;
		saveDone(SNAPSHOT_FEED, false, sSnapshotIndex);
		return NULL;
	}

	LLFloaterFeed* floater = LLFloaterFeed::showFromSnapshot(png, mFullScreenPreviewTexture, image_scale, sSnapshotIndex);

	return floater;
}

LLFloaterPostcard* LLSnapshotLivePreview::savePostcard()
{
	if (mCloseCalled)
	{
		return NULL;
	}

	++mOutstandingCallbacks;
	mSaveFailures = 0;
	addUsedBy(SNAPSHOT_POSTCARD);
	Dout(dc::snapshot, "LLSnapshotLivePreview::savePostcard: sSnapshotIndex = " << sSnapshotIndex << "; mOutstandingCallbacks = " << mOutstandingCallbacks << ".");

	if(mFullScreenPreviewTexture.isNull())
	{
		//this should never happen!!
		llwarns << "The snapshot image has not been generated!" << llendl ;
		saveDone(SNAPSHOT_POSTCARD, false, sSnapshotIndex);
		return NULL ;
	}

	// calculate and pass in image scale in case image data only use portion
	// of viewerimage buffer
	// calculate UV scale
	F32 uv_width = mFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFormattedWidth / (F32)mFullScreenPreviewTexture->getWidth(), 1.f) : 1.f;
	F32 uv_height = mFullScreenPreviewTextureNeedsScaling ? llmin((F32)mFormattedHeight / (F32)mFullScreenPreviewTexture->getHeight(), 1.f) : 1.f;
	LLVector2 image_scale(uv_width, uv_height);

	LLImageJPEG* jpg = dynamic_cast<LLImageJPEG*>(mFormattedImage.get());
	if(!jpg)
	{
		llwarns << "Formatted image not a JPEG" << llendl;
		saveDone(SNAPSHOT_POSTCARD, false, sSnapshotIndex);
		return NULL;
	}
	LLFloaterPostcard* floater = LLFloaterPostcard::showFromSnapshot(jpg, mFullScreenPreviewTexture, image_scale, mPosTakenGlobal, sSnapshotIndex);

	return floater;
}

class saveTextureUserData {
public:
	saveTextureUserData(LLSnapshotLivePreview* self, int index, bool temporary) : mSelf(self), mSnapshotIndex(index), mTemporary(temporary) { }
	LLSnapshotLivePreview* mSelf;
	int mSnapshotIndex;
	bool mTemporary;
};

void LLSnapshotLivePreview::saveTexture()
{
	if (mCloseCalled)
	{
		return;
	}

	++mOutstandingCallbacks;
	mSaveFailures = 0;
	addUsedBy(SNAPSHOT_TEXTURE);
	Dout(dc::snapshot, "LLSnapshotLivePreview::saveTexture: sSnapshotIndex = " << sSnapshotIndex << "; mOutstandingCallbacks = " << mOutstandingCallbacks << ".");
	saveStart(sSnapshotIndex);

	// gen a new uuid for this asset
	LLTransactionID tid;
	tid.generate();
	LLAssetID new_asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	LLVFile::writeFile(mFormattedImage->getData(), mFormattedImage->getDataSize(), gVFS, new_asset_id, LLAssetType::AT_TEXTURE);
	std::string pos_string;
	LLAgentUI::buildLocationString(pos_string, LLAgentUI::LOCATION_FORMAT_FULL);
	std::string who_took_it;
	LLAgentUI::buildFullname(who_took_it);
	LLAssetStorage::LLStoreAssetCallback callback = &LLSnapshotLivePreview::saveTextureDone;
	S32 expected_upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
	saveTextureUserData* user_data = new saveTextureUserData(this, sSnapshotIndex, gSavedSettings.getBOOL("TemporaryUpload"));
	if (!upload_new_resource(tid,	// tid
				LLAssetType::AT_TEXTURE,
				"Snapshot : " + pos_string,
				"Taken by " + who_took_it + " at " + pos_string,
				0,
				LLFolderType::FT_SNAPSHOT_CATEGORY,
				LLInventoryType::IT_SNAPSHOT,
				PERM_ALL,  // Note: Snapshots to inventory is a special case of content upload
				LLFloaterPerms::getGroupPerms(), // that is more permissive than other uploads
				LLFloaterPerms::getEveryonePerms(),
				"Snapshot : " + pos_string,
				callback, expected_upload_cost, user_data, &LLSnapshotLivePreview::saveTextureDone2))
	{
		// Something went wrong.
		delete user_data;
		saveDone(SNAPSHOT_TEXTURE, false, sSnapshotIndex);
	}

	gViewerWindow->playSnapshotAnimAndSound();
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_SNAPSHOT_COUNT );
}

void LLSnapshotLivePreview::saveLocal()
{
	if (mCloseCalled)
	{
		return;
	}

	++mOutstandingCallbacks;
	mSaveFailures = 0;
	addUsedBy(SNAPSHOT_LOCAL);
	Dout(dc::snapshot, "LLSnapshotLivePreview::saveLocal: sSnapshotIndex = " << sSnapshotIndex << "; mOutstandingCallbacks = " << mOutstandingCallbacks << ".");
	saveStart(sSnapshotIndex);

	gViewerWindow->saveImageNumbered(mFormattedImage, sSnapshotIndex);	// This calls saveDone() immediately, or later.
}

void LLSnapshotLivePreview::close(LLFloaterSnapshot* view)
{
	DoutEntering(dc::snapshot, "LLSnapshotLivePreview::close(" << (void*)view << ") [mOutstandingCallbacks = " << mOutstandingCallbacks << "]");
	mCloseCalled = view;
	if (!mOutstandingCallbacks)
	{
		doCloseAfterSave();
	}
	else
	{
		view->setVisible(FALSE);
		view->setEnabled(FALSE);
	}
}

void LLSnapshotLivePreview::saveStart(int index)
{
	if (index == sSnapshotIndex && gSavedSettings.getBOOL("CloseSnapshotOnKeep") && gSavedSettings.getBOOL("FreezeTime"))
	{
		// Turn off Freeze Time if we're going to close the floater
		// anyway at the *start* of an upload/save attempt.
		//
		// The disadvantage is that if the upload fails then we lost the Frozen Scene.
		// The user can still retry to upload or save the snapshot using the same size
		// (or smaller) to disk.
		//
		// The advantage is that if for some reason the upload takes a long time, then
		// the user can immediately continue with using the viewer instead of ending
		// up with a frozen (haha) interface.

		LLFloaterSnapshot::Impl::freezeTime(false);
	}
}

void LLSnapshotLivePreview::saveDone(ESnapshotType type, bool success, int index)
{
	DoutEntering(dc::snapshot, "LLSnapshotLivePreview::saveDone(" << type << ", " << success << ", " << index << ")");

	if (sSnapshotIndex != index)
	{
		Dout(dc::snapshot, "sSnapshotIndex (" << sSnapshotIndex << ") != index (" << index << ")");

		// The snapshot was already replaced, so this callback has nothing to do with us anymore.
		if (!success)
		{
			llwarns << "Permanent failure to upload or save snapshot" << llendl;
		}
		return;
	}

	--mOutstandingCallbacks;
	Dout(dc::snapshot, "sSnapshotIndex = " << sSnapshotIndex << "; mOutstandingCallbacks = " << mOutstandingCallbacks << ".");
	if (!success)
	{
		++mSaveFailures;
		// Enable Upload button.
		delUsedBy(type);
		LLFloaterSnapshot::updateControls();
	}
	if (!mOutstandingCallbacks)
	{
		doCloseAfterSave();
	}
}

// This callback is only used for the *legacy* LLViewerAssetStorage::storeAssetData
// (when the cap NewFileAgentInventory is not available) and temporaries.
// See upload_new_resource.
//static
void LLSnapshotLivePreview::saveTextureDone(LLUUID const& asset_id, void* user_data, S32 status, LLExtStat ext_status)
{
	LLResourceData* resource_data = (LLResourceData*)user_data;

	bool success = status == LL_ERR_NOERR;
	if (!success)
	{
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotificationsUtil::add("UploadSnapshotFail", args);
	}
	saveTextureUserData* data = (saveTextureUserData*)resource_data->mUserData;
	bool temporary = data->mTemporary;
	data->mSelf->saveDone(SNAPSHOT_TEXTURE, success, data->mSnapshotIndex);
	delete data;

	// Call the default call back.
	LLAssetStorage::LLStoreAssetCallback asset_callback = temporary ? &temp_upload_callback : &upload_done_callback;
	(*asset_callback)(asset_id, user_data, status, ext_status);
}

// This callback used when the capability NewFileAgentInventory is available and it wasn't a temporary.
// See upload_new_resource.
//static
void LLSnapshotLivePreview::saveTextureDone2(bool success, void* user_data)
{
	saveTextureUserData* data = (saveTextureUserData*)user_data;
	data->mSelf->saveDone(SNAPSHOT_TEXTURE, success, data->mSnapshotIndex);
	delete data;
}

void LLSnapshotLivePreview::doCloseAfterSave()
{
	if (!mCloseCalled)
	{
		return;
	}
	if (!mSaveFailures && gSavedSettings.getBOOL("CloseSnapshotOnKeep"))
	{
		// Relinquish image memory.
		mFormattedImage = NULL;
		mFormattedUpToDate = false;
		mFormattedDataSize = 0;
		updateSnapshot(FALSE, FALSE);
		mCloseCalled->close();
	}
	else
	{
		mCloseCalled->setEnabled(TRUE);
		mCloseCalled->setVisible(TRUE);
		gFloaterView->bringToFront(mCloseCalled);
		mCloseCalled = NULL;
	}
}

//----------------------------------------------------------------------------
// LLFloaterSnapshot::Impl

// static
LLHandle<LLView> LLFloaterSnapshot::Impl::sPreviewHandle;

// static
LLSnapshotLivePreview* LLFloaterSnapshot::Impl::getPreviewView(void)
{
	LLSnapshotLivePreview* previewp = (LLSnapshotLivePreview*)sPreviewHandle.get();
	return previewp;
}

// static
LLSnapshotLivePreview::ESnapshotType LLFloaterSnapshot::Impl::getTypeIndex(LLFloaterSnapshot* floater)
{
	LLSnapshotLivePreview::ESnapshotType index = LLSnapshotLivePreview::SNAPSHOT_FEED;
	LLSD value = floater->childGetValue("snapshot_type_radio");
	const std::string id = value.asString();
	if (id == "feed")
		index = LLSnapshotLivePreview::SNAPSHOT_FEED;
	else if (id == "postcard")
		index = LLSnapshotLivePreview::SNAPSHOT_POSTCARD;
	else if (id == "texture")
		index = LLSnapshotLivePreview::SNAPSHOT_TEXTURE;
	else if (id == "local")
		index = LLSnapshotLivePreview::SNAPSHOT_LOCAL;
	return index;
}


// static
LLFloaterSnapshot::ESnapshotFormat LLFloaterSnapshot::Impl::getFormatIndex(LLFloaterSnapshot* floater)
{
	ESnapshotFormat index = SNAPSHOT_FORMAT_PNG;
	LLSD value = floater->childGetValue("local_format_combo");
	const std::string id = value.asString();
	if (id == "PNG")
		index = SNAPSHOT_FORMAT_PNG;
	else if (id == "JPEG")
		index = SNAPSHOT_FORMAT_JPEG;
	else if (id == "BMP")
		index = SNAPSHOT_FORMAT_BMP;
	return index;
}

// static
void LLFloaterSnapshot::Impl::setResolution(LLFloaterSnapshot* floater, const std::string& comboname, bool visible, bool update_controls)
{
	LLComboBox* combo = floater->getChild<LLComboBox>(comboname);
	combo->setVisible(visible);
	updateResolution(combo, floater, update_controls);	// to sync spinners with combo
}

// static
void LLFloaterSnapshot::Impl::setAspect(LLFloaterSnapshot* floater, const std::string& comboname, bool update_controls)
{
	LLComboBox* combo = floater->getChild<LLComboBox>(comboname);
	combo->setVisible(TRUE);
	updateAspect(combo, floater, update_controls); 		// to sync spinner with combo
}

//static
void LLFloaterSnapshot::Impl::resetFeedAndPostcardAspect(LLFloaterSnapshot* floaterp)
{
	floaterp->getChild<LLComboBox>("feed_aspect_combo")->setCurrentByIndex(0);		// Default
	gSavedSettings.setS32("SnapshotFeedLastAspect", 0);

	floaterp->getChild<LLComboBox>("postcard_aspect_combo")->setCurrentByIndex(0);	// Default
	gSavedSettings.setS32("SnapshotPostcardLastAspect", 0);
}

//static 
void LLFloaterSnapshot::Impl::updateLayout(LLFloaterSnapshot* floaterp)
{
	S32 delta_height = 0;
	if (!gSavedSettings.getBOOL("AdvanceSnapshot"))
	{
		floaterp->getChild<LLComboBox>("feed_size_combo")->setCurrentByIndex(2);		// 500x375 (4:3)
		gSavedSettings.setS32("SnapshotFeedLastResolution", 2);

		floaterp->getChild<LLComboBox>("postcard_size_combo")->setCurrentByIndex(0);	// Current window
		gSavedSettings.setS32("SnapshotPostcardLastResolution", 0);

		resetFeedAndPostcardAspect(floaterp);

		floaterp->getChild<LLComboBox>("texture_size_combo")->setCurrentByIndex(0);		// 512x512 (most likely result for current window).
		gSavedSettings.setS32("SnapshotTextureLastResolution", 0);
		floaterp->getChild<LLComboBox>("texture_aspect_combo")->setCurrentByIndex(0);	// Current window
		gSavedSettings.setS32("SnapshotTextureLastAspect", 0);

		floaterp->getChild<LLComboBox>("local_size_combo")->setCurrentByIndex(0);		// Current window
		gSavedSettings.setS32("SnapshotLocalLastResolution", 0);
		floaterp->getChild<LLComboBox>("local_aspect_combo")->setCurrentByIndex(0);		// Current window
		gSavedSettings.setS32("SnapshotLocalLastAspect", 0);

		updateControls(floaterp);

		delta_height = floaterp->getUIWinHeightShort() - floaterp->getUIWinHeightLong();
	}
	floaterp->reshape(floaterp->getRect().getWidth(), floaterp->getUIWinHeightLong() + delta_height);
}

//static 
void LLFloaterSnapshot::Impl::freezeTime(bool on)
{
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (on)
	{
		// Stop all mouse events at fullscreen preview layer.
		gSnapshotFloaterView->setMouseOpaque(TRUE);

		// can see and interact with fullscreen preview now
		if (previewp)
		{
			previewp->setEnabled(TRUE);
			previewp->setVisible(TRUE);
		}

		// Freeze all avatars.
		for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
			iter != LLCharacter::sInstances.end(); ++iter)
		{
			sInstance->impl.mAvatarPauseHandles.push_back((*iter)->requestPause());
		}

		// Freeze everything else.
		gSavedSettings.setBOOL("FreezeTime", TRUE);

		if (LLToolMgr::getInstance()->getCurrentToolset() != gCameraToolset)
		{
			sInstance->impl.mLastToolset = LLToolMgr::getInstance()->getCurrentToolset();
			LLToolMgr::getInstance()->setCurrentToolset(gCameraToolset);
		}

		// Make sure the floater keeps focus so that pressing ESC stops Freeze Time mode.
		gFocusMgr.setDefaultKeyboardFocus(sInstance);
	}
	else if (gSavedSettings.getBOOL("FreezeTime")) // turning off freeze frame mode
	{
		// Restore default keyboard focus.
		gFocusMgr.restoreDefaultKeyboardFocus(sInstance);

		gSnapshotFloaterView->setMouseOpaque(FALSE);

		if (previewp)
		{
			previewp->setVisible(FALSE);
			previewp->setEnabled(FALSE);
		}

		// Thaw everything except avatars.
		gSavedSettings.setBOOL("FreezeTime", FALSE);

		// Thaw all avatars.
		LLVOAvatar* avatarp;
		for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin(); iter != LLCharacter::sInstances.end(); ++iter)
		{
			avatarp = static_cast<LLVOAvatar*>(*iter);
			avatarp->resetFreezeTime();
		}
		sInstance->impl.mAvatarPauseHandles.clear();

		// restore last tool (e.g. pie menu, etc)
		if (sInstance->impl.mLastToolset)
		{
			LLToolMgr::getInstance()->setCurrentToolset(sInstance->impl.mLastToolset);
		}
	}
}

// This is the main function that keeps all the GUI controls in sync with the saved settings.
// It should be called anytime a setting is changed that could affect the controls.
// No other methods should be changing any of the controls directly except for helpers called by this method.
// The basic pattern for programmatically changing the GUI settings is to first set the
// appropriate saved settings and then call this method to sync the GUI with them.
// static
void LLFloaterSnapshot::Impl::updateControls(LLFloaterSnapshot* floater, bool delayed_formatted)
{
	DoutEntering(dc::snapshot, "LLFloaterSnapshot::Impl::updateControls()");
	std::string fee = gHippoGridManager->getConnectedGrid()->getUploadFee();
	if (gSavedSettings.getBOOL("TemporaryUpload"))
	{
		fee = fee.substr(0, fee.find_first_of("0123456789")) + "0";
	}
	floater->childSetLabelArg("upload_btn", "[UPLOADFEE]", fee);

	LLSnapshotLivePreview::ESnapshotType shot_type = (LLSnapshotLivePreview::ESnapshotType)gSavedSettings.getS32("LastSnapshotType");
	LLRadioGroup* snapshot_type_radio = floater->getChild<LLRadioGroup>("snapshot_type_radio");
	if (snapshot_type_radio) 
	{
		snapshot_type_radio->setSelectedIndex(shot_type);

		const child_list_t *childs = snapshot_type_radio->getChildList();
		if (childs) 
		{
			child_list_t::const_iterator it, end=childs->end();
			for (it=childs->begin(); it!=end; ++it) 
			{
				LLRadioCtrl *ctrl = dynamic_cast<LLRadioCtrl*>(*it);
				if (ctrl && (ctrl->getName() == "texture"))
				{
					ctrl->setLabelArg("[UPLOADFEE]", fee);
				}
			}
		}
	}
	ESnapshotFormat shot_format = (ESnapshotFormat)gSavedSettings.getS32("SnapshotFormat");

	floater->childSetVisible("feed_size_combo", FALSE);
	floater->childSetVisible("feed_aspect_combo", FALSE);
	floater->childSetVisible("postcard_size_combo", FALSE);
	floater->childSetVisible("postcard_aspect_combo", FALSE);
	floater->childSetVisible("texture_size_combo", FALSE);
	floater->childSetVisible("texture_aspect_combo", FALSE);
	floater->childSetVisible("local_size_combo", FALSE);
	floater->childSetVisible("local_aspect_combo", FALSE);

	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp)
	{
		LLViewerWindow::ESnapshotType layer_type =
			(shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL) ?
			(LLViewerWindow::ESnapshotType)gSavedSettings.getS32("SnapshotLayerType") :
			LLViewerWindow::SNAPSHOT_TYPE_COLOR;
		// Do this before calling setResolution().
		previewp->setSnapshotBufferType(floater, layer_type);
	}
	BOOL is_advance = gSavedSettings.getBOOL("AdvanceSnapshot");
	switch(shot_type)
	{
	  case LLSnapshotLivePreview::SNAPSHOT_FEED:
		setResolution(floater, "feed_size_combo", is_advance, false);
		break;
	  case LLSnapshotLivePreview::SNAPSHOT_POSTCARD:
		setResolution(floater, "postcard_size_combo", is_advance, false);
		break;
	  case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:
		setResolution(floater, "texture_size_combo", is_advance, false);
		break;
	  case  LLSnapshotLivePreview::SNAPSHOT_LOCAL:
		setResolution(floater, "local_size_combo", is_advance, false);
		break;
	}
	floater->getChild<LLComboBox>("local_format_combo")->selectNthItem(shot_format);

	floater->childSetVisible("upload_btn",			shot_type == LLSnapshotLivePreview::SNAPSHOT_TEXTURE);
	floater->childSetVisible("send_btn",			shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD);
	floater->childSetVisible("feed_btn",			shot_type == LLSnapshotLivePreview::SNAPSHOT_FEED);
	floater->childSetVisible("save_btn",			shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL);
	floater->childSetEnabled("layer_types",			shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL);

	BOOL is_local = shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL;
	BOOL show_slider =
		shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD ||
		(is_local && shot_format == LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG);

	floater->getChild<LLComboBox>(previewp->aspectComboName())->setVisible(is_advance);
	floater->childSetVisible("more_btn", !is_advance); // the only item hidden in advanced mode
	floater->childSetVisible("less_btn",				is_advance);
	floater->childSetVisible("type_label2",				is_advance);
	floater->childSetVisible("keep_aspect",				is_advance);
	floater->childSetVisible("type_label3",				is_advance);
	floater->childSetVisible("format_label",			is_advance && is_local);
	floater->childSetVisible("local_format_combo",		is_advance && is_local);
	floater->childSetVisible("layer_types",				is_advance);
	floater->childSetVisible("layer_type_label",		is_advance);
	floater->childSetVisible("aspect_one_label",		is_advance);
	floater->childSetVisible("snapshot_width",			is_advance);
	floater->childSetVisible("snapshot_height",			is_advance);
	floater->childSetVisible("aspect_ratio",			is_advance);
	floater->childSetVisible("ui_check",				is_advance);
	floater->childSetVisible("hud_check",				is_advance);
	floater->childSetVisible("keep_open_check",			is_advance);
	floater->childSetVisible("freeze_time_check",		is_advance);
	floater->childSetVisible("auto_snapshot_check",		is_advance);
	floater->childSetVisible("image_quality_slider",	is_advance && show_slider);
	floater->childSetVisible("temp_check",				is_advance);

	BOOL got_bytes = previewp && previewp->getDataSize() > 0;
	BOOL is_texture = shot_type == LLSnapshotLivePreview::SNAPSHOT_TEXTURE;

	if (previewp)
	{
		previewp->setSnapshotFormat(shot_format);
		if (previewp->getRawSnapshotUpToDate())
		{
			if (delayed_formatted)
			{
				previewp->mFormattedDelayTimer.start(0.5);
			}
			previewp->generateFormattedAndFullscreenPreview(delayed_formatted);
		}
		else
		{
			previewp->mFormattedDelayTimer.stop();
		}
	}

	LLLocale locale(LLLocale::USER_LOCALE);
	std::string bytes_string;
	if (got_bytes)
	{
		LLResMgr::getInstance()->getIntegerString(bytes_string, (previewp->getDataSize()) >> 10 );
	}
	else
	{
		bytes_string = floater->getString("unknown");
	}
	S32 upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
	floater->childSetLabelArg("texture", "[AMOUNT]", llformat("%d",upload_cost));
	floater->childSetLabelArg("upload_btn", "[AMOUNT]", llformat("%d",upload_cost));
	floater->childSetTextArg("file_size_label", "[SIZE]", bytes_string);
	floater->childSetColor("file_size_label", 
		shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD 
		&& got_bytes
		&& previewp->getDataSize() > MAX_POSTCARD_DATASIZE ? LLColor4::red : gColors.getColor( "LabelTextColor" ));
	std::string target_size_str = gSavedSettings.getBOOL(snapshotKeepAspectName()) ? floater->getString("sourceAR") : floater->getString("targetAR");
	floater->childSetValue("type_label3", target_size_str);

	bool up_to_date = previewp && previewp->getSnapshotUpToDate();
	bool can_upload = up_to_date && !previewp->isUsedBy(shot_type);
	floater->childSetEnabled("feed_btn",   shot_type == LLSnapshotLivePreview::SNAPSHOT_FEED     && can_upload);
	floater->childSetEnabled("send_btn",   shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD && can_upload && previewp->getDataSize() <= MAX_POSTCARD_DATASIZE);
	floater->childSetEnabled("upload_btn", shot_type == LLSnapshotLivePreview::SNAPSHOT_TEXTURE  && can_upload);
	floater->childSetEnabled("save_btn",   shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL    && can_upload);
	floater->childSetEnabled("temp_check", is_advance && is_texture);

	if (previewp)
	{
		previewp->showFreezeFrameSnapshot(up_to_date);
	}
}

// static
void LLFloaterSnapshot::Impl::checkAutoSnapshot(LLSnapshotLivePreview* previewp, BOOL update_thumbnail)
{
	if (previewp)
	{
		BOOL autosnap = gSavedSettings.getBOOL("AutoSnapshot");
		previewp->updateSnapshot(autosnap, update_thumbnail, autosnap ? AUTO_SNAPSHOT_TIME_DELAY : 0.f);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickDiscard(void* data)
{
	LLFloaterSnapshot* view = static_cast<LLFloaterSnapshot*>(data);
	if (gSavedSettings.getBOOL("FreezeTime"))
	{
		LLSnapshotLivePreview* previewp = view->impl.getPreviewView();
		if (previewp && previewp->getShowFreezeFrameSnapshot())
			previewp->showFreezeFrameSnapshot(false);
		view->impl.freezeTime(false);
	}
	else
	{
		view->close();
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitFeedResolution(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotFeedLastResolution", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_FEED);
	}
	updateResolution(ctrl, data);
}

// static
void LLFloaterSnapshot::Impl::onCommitPostcardResolution(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotPostcardLastResolution", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_POSTCARD);
	}
	updateResolution(ctrl, data);
}

// static
void LLFloaterSnapshot::Impl::onCommitTextureResolution(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotTextureLastResolution", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_TEXTURE);
	}
	updateResolution(ctrl, data);
}

// static
void LLFloaterSnapshot::Impl::onCommitLocalResolution(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotLocalLastResolution", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_LOCAL);
	}
	updateResolution(ctrl, data);
}

// static
void LLFloaterSnapshot::Impl::onCommitFeedAspect(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotFeedLastAspect", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_FEED);
	}
	updateAspect(ctrl, data);
	LLFloaterSnapshot* floater = (LLFloaterSnapshot*)data;
	if (floater && previewp && gSavedSettings.getBOOL(snapshotKeepAspectName()))
	{
		enforceResolution(floater, previewp->getAspect());
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitPostcardAspect(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotPostcardLastAspect", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_POSTCARD);
	}
	updateAspect(ctrl, data);
	LLFloaterSnapshot* floater = (LLFloaterSnapshot*)data;
	if (floater && previewp && gSavedSettings.getBOOL(snapshotKeepAspectName()))
	{
		enforceResolution(floater, previewp->getAspect());
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitTextureAspect(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotTextureLastAspect", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_TEXTURE);
	}
	updateAspect(ctrl, data);
}

// static
void LLFloaterSnapshot::Impl::onCommitLocalAspect(LLUICtrl* ctrl, void* data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	gSavedSettings.setS32("SnapshotLocalLastAspect", combobox->getCurrentIndex());
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp && previewp->isUsed())
	{
		previewp->addManualOverride(LLSnapshotLivePreview::SNAPSHOT_LOCAL);
	}
	updateAspect(ctrl, data);
	LLFloaterSnapshot* floater = (LLFloaterSnapshot*)data;
	if (floater && previewp && gSavedSettings.getBOOL(snapshotKeepAspectName()))
	{
		enforceResolution(floater, previewp->getAspect());
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitSave(LLUICtrl* ctrl, void* data)
{
	if (ctrl->getValue().asString() == "saveas")
	{
		gViewerWindow->resetSnapshotLoc();
	}
	onClickKeep(data);
}

//static
void LLFloaterSnapshot::saveStart(int index)
{
	LLSnapshotLivePreview* previewp = LLFloaterSnapshot::Impl::getPreviewView();
	if (previewp)
	{
		previewp->saveStart(index);
	}
}

// Called from LLViewerWindow::saveImageNumbered, LLViewerWindow::saveImageNumbered_continued1 and LLViewerWindow::saveImageNumbered_continued2.
//static
void LLFloaterSnapshot::saveLocalDone(bool success, int index)
{
	LLSnapshotLivePreview* previewp = LLFloaterSnapshot::Impl::getPreviewView();
	if (previewp)
	{
		previewp->saveDone(LLSnapshotLivePreview::SNAPSHOT_LOCAL, success, index);
	}
}

//static
void LLFloaterSnapshot::saveFeedDone(bool success, int index)
{
	LLUploadDialog::modalUploadFinished();
	LLSnapshotLivePreview* previewp = LLFloaterSnapshot::Impl::getPreviewView();
	if (previewp)
	{
		previewp->saveDone(LLSnapshotLivePreview::SNAPSHOT_FEED, success, index);
	}
}

//static
void LLFloaterSnapshot::savePostcardDone(bool success, int index)
{
	LLUploadDialog::modalUploadFinished();
	LLSnapshotLivePreview* previewp = LLFloaterSnapshot::Impl::getPreviewView();
	if (previewp)
	{
		previewp->saveDone(LLSnapshotLivePreview::SNAPSHOT_POSTCARD, success, index);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickKeep(void* data)
{
	LLFloaterSnapshot* floater = (LLFloaterSnapshot *)data;
	LLSnapshotLivePreview* previewp = getPreviewView();
	
	if (previewp)
	{
		if (previewp->getSnapshotType() == LLSnapshotLivePreview::SNAPSHOT_FEED)
		{
			LLFloaterFeed* floater = previewp->getCaptionAndSaveFeed();
			if (floater)
			{
				// Make sure that the new floater is in front of gSnapshotFloaterView.
				// So that the structure is:
				// "root" -->
				//     "Snapshot Floater View" -->
				//         "floater_snapshot_profile"
				//         "Snapshot" floater
				//    ["snapshot_live_preview"]
				// and "floater_snapshot_profile" (floater) is a child of "Snapshot Floater View" (gSnapshotFloaterView),
				// and therefore in front of "snapshot_live_preview", if it exists.
				gSnapshotFloaterView->addChild(floater);
			}
		}
		else if (previewp->getSnapshotType() == LLSnapshotLivePreview::SNAPSHOT_POSTCARD)
		{
			LLFloaterPostcard* floater = previewp->savePostcard();
			if (floater)
			{
				// Same as above, but for the "Postcard" floater.
				gSnapshotFloaterView->addChild(floater);
			}
		}
		else if (previewp->getSnapshotType() == LLSnapshotLivePreview::SNAPSHOT_TEXTURE)
		{
			previewp->saveTexture();
		}
		else
		{
			previewp->saveLocal();
		}

		if (gSavedSettings.getBOOL("CloseSnapshotOnKeep"))
		{
			previewp->close(floater);
		}
		else
		{
			checkAutoSnapshot(previewp);
		}

		updateControls(floater);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickNewSnapshot(void* data)
{
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp)
	{
		previewp->updateSnapshot(TRUE);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickFreezeTime(void*)
{
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp)
	{
		freezeTime(true);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickAutoSnap(LLUICtrl *ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	gSavedSettings.setBOOL( "AutoSnapshot", check->get() );
	
	LLFloaterSnapshot* floater = (LLFloaterSnapshot*)data;		
	if (floater)
	{
		checkAutoSnapshot(getPreviewView());
		updateControls(floater);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickTemporaryImage(LLUICtrl *ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		updateControls(view);
	}
}

void LLFloaterSnapshot::Impl::onClickMore(void* data)
{
	gSavedSettings.setBOOL( "AdvanceSnapshot", TRUE );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		view->translate( 0, view->getUIWinHeightShort() - view->getUIWinHeightLong() );
		view->reshape(view->getRect().getWidth(), view->getUIWinHeightLong());
		updateControls(view) ;
		updateLayout(view) ;
		if (getPreviewView())
		{
			getPreviewView()->setThumbnailImageSize() ;
		}
	}
}
void LLFloaterSnapshot::Impl::onClickLess(void* data)
{
	gSavedSettings.setBOOL( "AdvanceSnapshot", FALSE );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		view->translate( 0, view->getUIWinHeightLong() - view->getUIWinHeightShort() );
		view->reshape(view->getRect().getWidth(), view->getUIWinHeightShort());
		updateControls(view) ;
		updateLayout(view) ;
		if (getPreviewView())
		{
			getPreviewView()->setThumbnailImageSize();
		}
	}
}

// static
void LLFloaterSnapshot::Impl::onClickUICheck(LLUICtrl *ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	gSavedSettings.setBOOL( "RenderUIInSnapshot", check->get() );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (view)
	{
		checkAutoSnapshot(getPreviewView(), TRUE);
		updateControls(view);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickHUDCheck(LLUICtrl *ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	gSavedSettings.setBOOL( "RenderHUDInSnapshot", check->get() );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (view)
	{
		checkAutoSnapshot(getPreviewView(), TRUE);
		updateControls(view);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickKeepOpenCheck(LLUICtrl* ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;

	gSavedSettings.setBOOL( "CloseSnapshotOnKeep", !check->get() );
}

// static
void LLFloaterSnapshot::Impl::onClickKeepAspect(LLUICtrl* ctrl, void* data)
{
	LLFloaterSnapshot* view = (LLFloaterSnapshot*)data;
	if (view)
	{
		LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
		keepAspect(view, check->get());
		updateControls(view);
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitQuality(LLUICtrl* ctrl, void* data)
{
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	S32 quality_val = llfloor((F32)slider->getValue().asReal());

	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp)
	{
		previewp->setSnapshotQuality(quality_val);
		checkAutoSnapshot(previewp, TRUE);
	}
}

//static
void LLFloaterSnapshot::Impl::onQualityMouseUp(void* data)
{
	Dout(dc::snapshot, "Calling LLFloaterSnapshot::Impl::QualityMouseUp()");
	LLFloaterSnapshot* view = (LLFloaterSnapshot *)data;
	updateControls(view);
}

// static
void LLFloaterSnapshot::Impl::onCommitFreezeTime(LLUICtrl* ctrl, void* data)
{
	LLCheckBoxCtrl* check_box = (LLCheckBoxCtrl*)ctrl;
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
		
	if (!view || !check_box)
	{
		return;
	}

	gSavedSettings.setBOOL("SnapshotOpenFreezeTime", check_box->get());
}

static std::string lastSnapshotWidthName()
{
	switch(gSavedSettings.getS32("LastSnapshotType"))
	{
	case LLSnapshotLivePreview::SNAPSHOT_FEED:     return "LastSnapshotToFeedWidth";
	case LLSnapshotLivePreview::SNAPSHOT_POSTCARD: return "LastSnapshotToEmailWidth";
	case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:  return "LastSnapshotToInventoryWidth";
	default:                                       return "LastSnapshotToDiskWidth";
	}
}

static std::string lastSnapshotHeightName()
{
	switch(gSavedSettings.getS32("LastSnapshotType"))
	{
	case LLSnapshotLivePreview::SNAPSHOT_FEED:     return "LastSnapshotToFeedHeight";
	case LLSnapshotLivePreview::SNAPSHOT_POSTCARD: return "LastSnapshotToEmailHeight";
	case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:  return "LastSnapshotToInventoryHeight";
	default:                                       return "LastSnapshotToDiskHeight";
	}
}

static std::string lastSnapshotAspectName()
{
	switch(gSavedSettings.getS32("LastSnapshotType"))
	{
	case LLSnapshotLivePreview::SNAPSHOT_FEED:     return "LastSnapshotToFeedAspect";
	case LLSnapshotLivePreview::SNAPSHOT_POSTCARD: return "LastSnapshotToEmailAspect";
	case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:  return "LastSnapshotToInventoryAspect";
	default:                                       return "LastSnapshotToDiskAspect";
	}
}

static std::string snapshotKeepAspectName()
{
	switch(gSavedSettings.getS32("LastSnapshotType"))
	{
	case LLSnapshotLivePreview::SNAPSHOT_FEED:     return "SnapshotFeedKeepAspect";
	case LLSnapshotLivePreview::SNAPSHOT_POSTCARD: return "SnapshotPostcardKeepAspect";
	case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:  return "SnapshotTextureKeepAspect";
	default:                                       return "SnapshotLocalKeepAspect";
	}
}

// static
void LLFloaterSnapshot::Impl::keepAspect(LLFloaterSnapshot* view, bool on, bool force)
{
	DoutEntering(dc::snapshot, "LLFloaterSnapshot::Impl::keepAspect(view, " << on << ", " << force << ")");
	bool cur_on = gSavedSettings.getBOOL(snapshotKeepAspectName());
	if ((!force && cur_on == on) ||
		gSavedSettings.getBOOL("RenderUIInSnapshot") ||
		gSavedSettings.getBOOL("RenderHUDInSnapshot"))
	{
		// No change.
		return;
	}
	view->childSetValue("keep_aspect", on);
	gSavedSettings.setBOOL(snapshotKeepAspectName(), on);
	if (on)
	{
		LLSnapshotLivePreview* previewp = getPreviewView();
		if (previewp)
		{
			S32 w = llround(view->childGetValue("snapshot_width").asReal(), 1.0);
			S32 h = llround(view->childGetValue("snapshot_height").asReal(), 1.0);
			gSavedSettings.setS32(lastSnapshotWidthName(), w);
			gSavedSettings.setS32(lastSnapshotHeightName(), h);
			comboSetCustom(view, previewp->resolutionComboName());
			enforceAspect(view, (F32)w / h);
		}
	}
}

// static
void LLFloaterSnapshot::Impl::updateResolution(LLUICtrl* ctrl, void* data, bool update_controls)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
		
	if (!view || !combobox)
	{
		return;
	}

	LLSnapshotLivePreview* previewp = getPreviewView();

	view->getChild<LLComboBox>("feed_size_combo")->selectNthItem(gSavedSettings.getS32("SnapshotFeedLastResolution"));
	view->getChild<LLComboBox>("postcard_size_combo")->selectNthItem(gSavedSettings.getS32("SnapshotPostcardLastResolution"));
	view->getChild<LLComboBox>("texture_size_combo")->selectNthItem(gSavedSettings.getS32("SnapshotTextureLastResolution"));
	view->getChild<LLComboBox>("local_size_combo")->selectNthItem(gSavedSettings.getS32("SnapshotLocalLastResolution"));

	std::string sdstring = combobox->getSelectedValue();
	LLSD sdres;
	std::stringstream sstream(sdstring);
	LLSDSerialize::fromNotation(sdres, sstream, sdstring.size());
		
	S32 width = sdres[0];
	S32 height = sdres[1];

	if (width != -1 && height != -1)
	{
		// Not "Custom".
		keepAspect(view, false);
	}
	
	if (previewp && combobox->getCurrentIndex() >= 0)
	{
		S32 original_width = 0 , original_height = 0 ;
		previewp->getSize(original_width, original_height) ;
		if (width == 0 || height == 0 || gSavedSettings.getBOOL("RenderUIInSnapshot") || gSavedSettings.getBOOL("RenderHUDInSnapshot"))
		{
			// take resolution from current window size
			previewp->setSize(gViewerWindow->getWindowDisplayWidth(), gViewerWindow->getWindowDisplayHeight());
		}
		else if (width == -1 || height == -1)
		{
			// load last custom value
			previewp->setSize(gSavedSettings.getS32(lastSnapshotWidthName()), gSavedSettings.getS32(lastSnapshotHeightName()));
		}
		else if (height == -2)
		{
		  // The size is actually the source aspect now, encoded in the width.
		  F32 source_aspect = width / 630.f;
		  // Use the size of the current window, cropped to the required aspect.
		  width = llmin(gViewerWindow->getWindowDisplayWidth(), llround(gViewerWindow->getWindowDisplayHeight() * source_aspect));
		  height = llmin(gViewerWindow->getWindowDisplayHeight(), llround(gViewerWindow->getWindowDisplayWidth() / source_aspect));
		  previewp->setSize(width, height);
		}
		else
		{
			// use the resolution from the selected pre-canned drop-down choice
			previewp->setSize(width, height);
		}

		previewp->getSize(width, height);
	
		if(view->childGetValue("snapshot_width").asInteger() != width || view->childGetValue("snapshot_height").asInteger() != height)
		{
			view->childSetValue("snapshot_width", width);
			view->childSetValue("snapshot_height", height);
		}
	}

	// Set whether or not the spinners can be changed.

	LLSpinCtrl* width_spinner = view->getChild<LLSpinCtrl>("snapshot_width");
	LLSpinCtrl* height_spinner = view->getChild<LLSpinCtrl>("snapshot_height");

	if (gSavedSettings.getS32("LastSnapshotType") == LLSnapshotLivePreview::SNAPSHOT_TEXTURE ||
		gSavedSettings.getBOOL("RenderUIInSnapshot") ||
		gSavedSettings.getBOOL("RenderHUDInSnapshot"))
	{
		// Disable without making label gray.
		width_spinner->setAllowEdit(FALSE);
		width_spinner->setIncrement(0);
		height_spinner->setAllowEdit(FALSE);
		height_spinner->setIncrement(0);
	}
	else
	{
		width_spinner->setAllowEdit(TRUE);
		width_spinner->setIncrement(32);
		height_spinner->setAllowEdit(TRUE);
		height_spinner->setIncrement(32);
	}

	if (previewp)
	{
		// In case the aspect is 'Default', need to update aspect (which will call updateControls, if necessary).
		setAspect(view, previewp->aspectComboName(), update_controls);
	}
}

// static
void LLFloaterSnapshot::Impl::updateAspect(LLUICtrl* ctrl, void* data, bool update_controls)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
		
	if (!view || !combobox)
	{
		return;
	}

	LLSnapshotLivePreview* previewp = getPreviewView();

	view->getChild<LLComboBox>("feed_aspect_combo")->selectNthItem(gSavedSettings.getS32("SnapshotFeedLastAspect"));
	view->getChild<LLComboBox>("postcard_aspect_combo")->selectNthItem(gSavedSettings.getS32("SnapshotPostcardLastAspect"));
	view->getChild<LLComboBox>("texture_aspect_combo")->selectNthItem(gSavedSettings.getS32("SnapshotTextureLastAspect"));
	view->getChild<LLComboBox>("local_aspect_combo")->selectNthItem(gSavedSettings.getS32("SnapshotLocalLastAspect"));

	std::string sdstring = combobox->getSelectedValue();
	std::stringstream sstream;
	sstream << sdstring;
	F32 aspect;
	sstream >> aspect;

	if (aspect == -2)		// Default
	{
		S32 width, height;
		previewp->getSize(width, height);
		aspect = (F32)width / height;
		// Turn off "Keep aspect" when aspect is set to Default.
		keepAspect(view, false);
	}
	else if (aspect == -1)	// Custom
	{
		aspect = gSavedSettings.getF32(lastSnapshotAspectName());
	}
	if (aspect == 0)		// Current window
	{
		aspect = (F32)gViewerWindow->getWindowDisplayWidth() / gViewerWindow->getWindowDisplayHeight();
	}

	LLSpinCtrl* aspect_spinner = view->getChild<LLSpinCtrl>("aspect_ratio");
	LLCheckBoxCtrl* keep_aspect = view->getChild<LLCheckBoxCtrl>("keep_aspect");

	// Set whether or not the spinners can be changed.
	if (gSavedSettings.getBOOL("RenderUIInSnapshot") ||
		gSavedSettings.getBOOL("RenderHUDInSnapshot"))
	{
		aspect = (F32)gViewerWindow->getWindowDisplayWidth() / gViewerWindow->getWindowDisplayHeight();
		// Disable without making label gray.
		aspect_spinner->setAllowEdit(FALSE);
		aspect_spinner->setIncrement(0);
		keep_aspect->setEnabled(FALSE);
	}
	else
	{
		aspect_spinner->setAllowEdit(TRUE);
		aspect_spinner->setIncrement(llmax(0.01f, lltrunc(aspect) / 100.0f));
		keep_aspect->setEnabled(TRUE);
	}

	// Sync the spinner and cache value.
	aspect_spinner->set(aspect);
	if (previewp)
	{
		F32 old_aspect = previewp->getAspect();
		previewp->setAspect(aspect);
		if (old_aspect != aspect)
		{
			checkAutoSnapshot(previewp, TRUE);
		}
	}
	if (update_controls)
	{
		// In case getSnapshotUpToDate() changed.
		updateControls(view);
	}
}

// static
void LLFloaterSnapshot::Impl::enforceAspect(LLFloaterSnapshot* floater, F32 new_aspect)
{
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp)
	{
		LLComboBox* combo = floater->getChild<LLComboBox>(previewp->aspectComboName());
		S32 const aspect_custom = combo->getItemCount() - 1;
		for (S32 index = 0; index <= aspect_custom; ++index)
		{
			combo->setCurrentByIndex(index);
			if (index == aspect_custom)
			{
				gSavedSettings.setF32(lastSnapshotAspectName(), new_aspect);
				break;
			}
			std::string sdstring = combo->getSelectedValue();
			std::stringstream sstream;
			sstream << sdstring;
			F32 aspect;
			sstream >> aspect;
			if (aspect == -2)		// Default
			{
				continue;
			}
			if (aspect == 0)		// Current window
			{
				aspect = (F32)gViewerWindow->getWindowDisplayWidth() / gViewerWindow->getWindowDisplayHeight();
			}
			if (llabs(aspect - new_aspect) < 0.0001)
			{
				break;
			}
		}
		storeAspectSetting(combo, previewp->aspectComboName());
		updateAspect(combo, floater, true);
	}
}

void LLFloaterSnapshot::Impl::enforceResolution(LLFloaterSnapshot* floater, F32 new_aspect)
{
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp)
	{
		// Get current size.
		S32 w, h;
		previewp->getSize(w, h);
		// Do all calculation in floating point.
		F32 cw = w;
		F32 ch = h;
		// Get the current raw size.
		previewp->getRawSize(w, h);
		F32 rw = w;
		F32 rh = h;
		// Fit rectangle with aspect new_aspect, around current rectangle, but cropped to raw size.
		F32 nw = llmin(llmax(cw, ch * new_aspect), rw);
		F32 nh = llmin(llmax(ch, cw / new_aspect), rh);
		// Fit rectangle with aspect new_aspect inside that rectangle (in case it was cropped).
		nw = llmin(nw, nh * new_aspect);
		nh = llmin(nh, nw / new_aspect);
		// Round off to nearest integer.
		S32 new_width = llround(nw);
		S32 new_height = llround(nh);

		gSavedSettings.setS32(lastSnapshotWidthName(), new_width);
		gSavedSettings.setS32(lastSnapshotHeightName(), new_height);
		comboSetCustom(floater, previewp->resolutionComboName());
		updateResolution(floater->getChild<LLComboBox>(previewp->resolutionComboName()), floater, false);
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitLayerTypes(LLUICtrl* ctrl, void*data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (view)
	{
		LLComboBox* combobox = (LLComboBox*)ctrl;
		gSavedSettings.setS32("SnapshotLayerType", combobox->getCurrentIndex());
		updateControls(view);
	}
}

//static 
void LLFloaterSnapshot::Impl::onCommitSnapshotType(LLUICtrl* ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		LLSnapshotLivePreview::ESnapshotType snapshot_type = getTypeIndex(view);
		gSavedSettings.setS32("LastSnapshotType", snapshot_type);
		LLSnapshotLivePreview* previewp = getPreviewView();
		if (previewp)
		{
			previewp->setSnapshotType(snapshot_type);
		}
		keepAspect(view, gSavedSettings.getBOOL(snapshotKeepAspectName()), true);
		updateControls(view);
	}
}


//static
void LLFloaterSnapshot::Impl::onCommitSnapshotFormat(LLUICtrl* ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		gSavedSettings.setS32("SnapshotFormat", getFormatIndex(view));
		updateControls(view);
	}
}



// Sets the named size combo to "custom" mode.
// static
void LLFloaterSnapshot::Impl::comboSetCustom(LLFloaterSnapshot* floater, const std::string& comboname)
{
	LLComboBox* combo = floater->getChild<LLComboBox>(comboname);

	combo->setCurrentByIndex(combo->getItemCount() - 1); // "custom" is always the last index
	storeAspectSetting(combo, comboname);
}

// static
void LLFloaterSnapshot::Impl::storeAspectSetting(LLComboBox* combo, const std::string& comboname)
{
	if(comboname == "feed_size_combo") 
	{
		gSavedSettings.setS32("SnapshotFeedLastResolution", combo->getCurrentIndex());
	}
	else if(comboname == "postcard_size_combo") 
	{
		gSavedSettings.setS32("SnapshotPostcardLastResolution", combo->getCurrentIndex());
	}
	else if(comboname == "local_size_combo") 
	{
		gSavedSettings.setS32("SnapshotLocalLastResolution", combo->getCurrentIndex());
	}
	else if(comboname == "feed_aspect_combo")
	{
		gSavedSettings.setS32("SnapshotFeedLastAspect", combo->getCurrentIndex());
	}
	else if(comboname == "postcard_aspect_combo") 
	{
		gSavedSettings.setS32("SnapshotPostcardLastAspect", combo->getCurrentIndex());
	}
	else if(comboname == "texture_aspect_combo") 
	{
		gSavedSettings.setS32("SnapshotTextureLastAspect", combo->getCurrentIndex());
	}
	else if(comboname == "local_aspect_combo") 
	{
		gSavedSettings.setS32("SnapshotLocalLastAspect", combo->getCurrentIndex());
	}
}

//static
void LLFloaterSnapshot::Impl::onCommitCustomResolution(LLUICtrl *ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		LLSpinCtrl* width_spinner = view->getChild<LLSpinCtrl>("snapshot_width");
		LLSpinCtrl* height_spinner = view->getChild<LLSpinCtrl>("snapshot_height");

		S32 w = llround((F32)width_spinner->getValue().asReal(), 1.0f);
		S32 h = llround((F32)height_spinner->getValue().asReal(), 1.0f);

		LLSnapshotLivePreview* previewp = getPreviewView();
		if (previewp)
		{
			S32 curw,curh;
			previewp->getSize(curw, curh);
			
			if (w != curw || h != curh)
			{
				// Enforce multiple of 32 if the step was 32.
				Dout(dc::snapshot, "w = " << w << "; curw = " << curw);
				if (llabs(w - curw) == 32)
				{
					w = (w + 16) & -32;
					Dout(dc::snapshot, "w = (w + 16) & -32 = " << w);
				}
				if (llabs(h - curh) == 32)
				{
					h = (h + 16) & -32;
				}
				if (gSavedSettings.getBOOL(snapshotKeepAspectName()))
				{
					F32 aspect = previewp->getAspect();
					if (h == curh)
					{
						// Width was changed. Change height to keep aspect constant.
						h = llround(w / aspect);
					}
					else
					{
						// Height was changed. Change width to keep aspect constant.
						w = llround(h * aspect);
					}
				}
				width_spinner->forceSetValue(LLSD::Real(w));
				height_spinner->forceSetValue(LLSD::Real(h));
				previewp->setMaxImageSize((S32)((LLSpinCtrl *)ctrl)->getMaxValue()) ;
				previewp->setSize(w,h);
				checkAutoSnapshot(previewp, FALSE);
				previewp->updateSnapshot(FALSE, TRUE);
				comboSetCustom(view, "feed_size_combo");
				comboSetCustom(view, "postcard_size_combo");
				comboSetCustom(view, "local_size_combo");
			}
		}

		gSavedSettings.setS32(lastSnapshotWidthName(), w);
		gSavedSettings.setS32(lastSnapshotHeightName(), h);

		updateControls(view, true);
	}
}

char const* LLSnapshotLivePreview::resolutionComboName() const
{
	char const* result;
	switch(mSnapshotType)
	{
		case SNAPSHOT_FEED:
			result = "feed_size_combo";
			break;
		case SNAPSHOT_POSTCARD:
			result = "postcard_size_combo";
			break;
		case SNAPSHOT_TEXTURE:
			result = "texture_size_combo";
			break;
		case SNAPSHOT_LOCAL:
			result = "local_size_combo";
			break;
		default:
			result= "";
			break;
	}
	return result;
}

char const* LLSnapshotLivePreview::aspectComboName() const
{
	char const* result;
	switch(mSnapshotType)
	{
		case SNAPSHOT_FEED:
			result = "feed_aspect_combo";
			break;
		case SNAPSHOT_POSTCARD:
			result = "postcard_aspect_combo";
			break;
		case SNAPSHOT_TEXTURE:
			result = "texture_aspect_combo";
			break;
		case SNAPSHOT_LOCAL:
			result = "local_aspect_combo";
			break;
		default:
			result= "";
			break;
	}
	return result;
}

//static
void LLFloaterSnapshot::Impl::onCommitCustomAspect(LLUICtrl *ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		F32 a = view->childGetValue("aspect_ratio").asReal();

		LLSnapshotLivePreview* previewp = getPreviewView();
		if (previewp)
		{
			F32 cura = previewp->getAspect();
			
			if (a != cura)
			{
				previewp->setAspect(a);
				previewp->updateSnapshot(FALSE, TRUE);
				comboSetCustom(view, previewp->aspectComboName());
			}
		}

		gSavedSettings.setF32(lastSnapshotAspectName(), a);

		if (gSavedSettings.getBOOL(snapshotKeepAspectName()))
		{
			enforceResolution(view, a);
		}

		updateControls(view, true);
	}
}

///----------------------------------------------------------------------------
/// Class LLFloaterSnapshot
///----------------------------------------------------------------------------

// Default constructor
LLFloaterSnapshot::LLFloaterSnapshot()
	: LLFloater(std::string("Snapshot Floater")),
	  impl (*(new Impl))
{
}

// Destroys the object
LLFloaterSnapshot::~LLFloaterSnapshot()
{
	if (sInstance == this)
	{
		LLView::deleteViewByHandle(Impl::sPreviewHandle);
		Impl::sPreviewHandle = LLHandle<LLView>();
		sInstance = NULL;
	}

	//unfreeze everything else
	gSavedSettings.setBOOL("FreezeTime", FALSE);

	if (impl.mLastToolset)
	{
		LLToolMgr::getInstance()->setCurrentToolset(impl.mLastToolset);
	}

	delete &impl;
}


BOOL LLFloaterSnapshot::postBuild()
{
	childSetCommitCallback("snapshot_type_radio", Impl::onCommitSnapshotType, this);
	childSetCommitCallback("local_format_combo", Impl::onCommitSnapshotFormat, this);
	
	childSetAction("new_snapshot_btn", Impl::onClickNewSnapshot, this);
	childSetAction("freeze_time_btn", Impl::onClickFreezeTime, this);

	childSetAction("more_btn", Impl::onClickMore, this);
	childSetAction("less_btn", Impl::onClickLess, this);

	childSetAction("upload_btn", Impl::onClickKeep, this);
	childSetAction("send_btn", Impl::onClickKeep, this);
	childSetAction("feed_btn", Impl::onClickKeep, this);
	childSetCommitCallback("save_btn", Impl::onCommitSave, this);
	childSetAction("discard_btn", Impl::onClickDiscard, this);

	childSetCommitCallback("image_quality_slider", Impl::onCommitQuality, this);
	childSetValue("image_quality_slider", gSavedSettings.getS32("SnapshotQuality"));
	impl.mQualityMouseUpConnection = getChild<LLSliderCtrl>("image_quality_slider")->setSliderMouseUpCallback(boost::bind(&Impl::onQualityMouseUp, this));

	childSetCommitCallback("snapshot_width", Impl::onCommitCustomResolution, this);
	childSetCommitCallback("snapshot_height", Impl::onCommitCustomResolution, this);

	childSetCommitCallback("aspect_ratio", Impl::onCommitCustomAspect, this);
	childSetCommitCallback("keep_aspect", Impl::onClickKeepAspect, this);

	childSetCommitCallback("ui_check", Impl::onClickUICheck, this);
	childSetValue("ui_check", gSavedSettings.getBOOL("RenderUIInSnapshot"));

	childSetCommitCallback("hud_check", Impl::onClickHUDCheck, this);
	childSetValue("hud_check", gSavedSettings.getBOOL("RenderHUDInSnapshot"));

	childSetCommitCallback("keep_open_check", Impl::onClickKeepOpenCheck, this);
	childSetValue("keep_open_check", !gSavedSettings.getBOOL("CloseSnapshotOnKeep"));

	childSetCommitCallback("layer_types", Impl::onCommitLayerTypes, this);
	childSetValue("layer_types", "colors");
	childSetEnabled("layer_types", FALSE);

	childSetValue("snapshot_width", gSavedSettings.getS32(lastSnapshotWidthName()));
	childSetValue("snapshot_height", gSavedSettings.getS32(lastSnapshotHeightName()));

	childSetValue("freeze_time_check", gSavedSettings.getBOOL("SnapshotOpenFreezeTime"));
	childSetCommitCallback("freeze_time_check", Impl::onCommitFreezeTime, this);

	childSetValue("auto_snapshot_check", gSavedSettings.getBOOL("AutoSnapshot"));
	childSetCommitCallback("auto_snapshot_check", Impl::onClickAutoSnap, this);
	childSetCommitCallback("temp_check", Impl::onClickTemporaryImage, this);

	childSetCommitCallback("feed_size_combo", Impl::onCommitFeedResolution, this);
	childSetCommitCallback("postcard_size_combo", Impl::onCommitPostcardResolution, this);
	childSetCommitCallback("texture_size_combo", Impl::onCommitTextureResolution, this);
	childSetCommitCallback("local_size_combo", Impl::onCommitLocalResolution, this);

	childSetCommitCallback("feed_aspect_combo", Impl::onCommitFeedAspect, this);
	childSetCommitCallback("postcard_aspect_combo", Impl::onCommitPostcardAspect, this);
	childSetCommitCallback("texture_aspect_combo", Impl::onCommitTextureAspect, this);
	childSetCommitCallback("local_aspect_combo", Impl::onCommitLocalAspect, this);

	// create preview window
	LLRect full_screen_rect = sInstance->getRootView()->getRect();
	LLSnapshotLivePreview* previewp = new LLSnapshotLivePreview(full_screen_rect);
	// Make sure preview is below snapshot floater.
	// "root" --> (first child is hit first):
	//   "Snapshot Floater View"
	//   "snapshot_live_preview"
	//   ...
	sInstance->getRootView()->addChild(previewp);				// Note that addChild is actually 'moveChild'.
	sInstance->getRootView()->addChild(gSnapshotFloaterView);	// The last added child becomes the first child; the one up front.

	gSavedSettings.setBOOL("TemporaryUpload",FALSE);
	childSetValue("temp_check",FALSE);

	Impl::sPreviewHandle = previewp->getHandle();

	impl.keepAspect(sInstance, gSavedSettings.getBOOL(snapshotKeepAspectName()), true);
	impl.freezeTime(gSavedSettings.getBOOL("SnapshotOpenFreezeTime"));
	impl.updateControls(this);

	return TRUE;
}

LLRect LLFloaterSnapshot::getThumbnailAreaRect()
{
	// getRect() includes shadows and the title bar, therefore the real corners are as follows:
	return LLRect(1, getRect().getHeight() - 17, getRect().getWidth() - 1, getRect().getHeight() - 17 - THUMBHEIGHT);
}

void LLFloaterSnapshot::draw()
{
	LLSnapshotLivePreview* previewp = impl.getPreviewView();

	if (previewp && (previewp->isSnapshotActive() || previewp->getThumbnailLock()))
	{
		// don't render snapshot window in snapshot, even if "show ui" is turned on
		return;
	}

	LLFloater::draw();

	if (previewp)
	{		
		if(previewp->getThumbnailImage())
		{
			LLRect const thumb_area = getThumbnailAreaRect();
			// The offset needed to center the thumbnail is: center - thumbnailSize/2 =
			S32 offset_x = (thumb_area.mLeft + thumb_area.mRight - previewp->getThumbnailWidth()) / 2;
			S32 offset_y = (thumb_area.mBottom + thumb_area.mTop - previewp->getThumbnailHeight()) / 2;

			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gl_rect_2d(thumb_area, LLColor4::grey4, true);
			gl_draw_scaled_image(offset_x, offset_y, 
					previewp->getThumbnailWidth(), previewp->getThumbnailHeight(), 
					previewp->getThumbnailImage(), LLColor4::white);	

			previewp->drawPreviewRect(offset_x, offset_y) ;
		}
	}
}

void LLFloaterSnapshot::onOpen()
{
	gSavedSettings.setBOOL("SnapshotBtnState", TRUE);
}

void LLFloaterSnapshot::onClose(bool app_quitting)
{
	// Set invisible so it doesn't eat tooltips. JC
	gSnapshotFloaterView->setVisible(FALSE);
	gSnapshotFloaterView->setEnabled(FALSE);
	gSavedSettings.setBOOL("SnapshotBtnState", FALSE);
	impl.freezeTime(false);
	destroy();
}

// static
void LLFloaterSnapshot::show(void*)
{
	if (!sInstance)
	{
		sInstance = new LLFloaterSnapshot();
		LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_snapshot.xml", NULL, FALSE);	// "Snapshot" floater.
		// Make snapshot floater a child of (full screen) gSnapshotFloaterView.
		// So that the structure is:
		// "root" -->
		//     "Snapshot Floater View" -->
		//         "Snapshot" floater
		//        ["floater_snapshot_profile"]
		//        ["Postcard"]
		//    ["snapshot_live_preview"]
		// and the "Snapshot" floater (sInstance) is a child of "Snapshot Floater View" (gSnapshotFloaterView),
		// and therefore in front of "snapshot_live_preview", if it exists.
		gSnapshotFloaterView->addChild(sInstance);

		sInstance->impl.updateLayout(sInstance);
	}
	else // just refresh the snapshot in the existing floater instance (DEV-12255)
	{
		LLSnapshotLivePreview* preview = LLFloaterSnapshot::Impl::getPreviewView();
		if(preview)
		{
			preview->updateSnapshot(TRUE);
		}
	}
	
	sInstance->open();		/* Flawfinder: ignore */
	sInstance->focusFirstItem(FALSE);
	sInstance->setEnabled(TRUE);
	sInstance->setVisible(TRUE);
	gSnapshotFloaterView->setEnabled(TRUE);
	gSnapshotFloaterView->setVisible(TRUE);
	gSnapshotFloaterView->adjustToFitScreen(sInstance, FALSE);
}

void LLFloaterSnapshot::hide(void*)
{
	if (sInstance && !sInstance->isDead())
	{
		sInstance->close();
	}
}

//static 
void LLFloaterSnapshot::update()
{
	BOOL changed = FALSE;
	for (std::set<LLSnapshotLivePreview*>::iterator iter = LLSnapshotLivePreview::sList.begin();
		 iter != LLSnapshotLivePreview::sList.end(); ++iter)
	{
		changed |= LLSnapshotLivePreview::onIdle(*iter);
	}
	if(changed)
	{
		sInstance->impl.updateControls(sInstance);
	}
}

//static 
void LLFloaterSnapshot::updateControls()
{
	Impl::updateControls(sInstance);
}

//static 
void LLFloaterSnapshot::resetFeedAndPostcardAspect()
{
	Impl::resetFeedAndPostcardAspect(sInstance);
}

BOOL LLFloaterSnapshot::handleKeyHere(KEY key, MASK mask)
{
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (freeze_time && key == KEY_ESCAPE && mask == MASK_NONE)
	{
		// Ignore trying to defocus the snapshot floater in Freeze Time mode.
		// However, if we're showing the fullscreen frozen preview, drop out of it;
		// otherwise leave Freeze Time (so the next ESC press DOES defocus
		// the floater and only the fourth will finally reset the cam).
		LLSnapshotLivePreview* previewp = LLFloaterSnapshot::Impl::getPreviewView();
		if (previewp && previewp->getShowFreezeFrameSnapshot())
		{
			previewp->showFreezeFrameSnapshot(false);
		}
		else
		{
			impl.freezeTime(false);
		}
		return TRUE;
	}
	else if (key == 'Q' && mask == MASK_CONTROL)
	{
		// Allow users to quit with ctrl-Q.
		LLSnapshotLivePreview* previewp = LLFloaterSnapshot::Impl::getPreviewView();
		if (previewp && previewp->getShowFreezeFrameSnapshot())
		{
			previewp->showFreezeFrameSnapshot(false);
		}
		impl.freezeTime(false);
		gFocusMgr.removeKeyboardFocusWithoutCallback(gFocusMgr.getKeyboardFocus());
		return FALSE;
	}
	return LLFloater::handleKeyHere(key, mask);
}

BOOL LLFloaterSnapshot::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mask == MASK_NONE)
	{
		LLRect thumb_area = getThumbnailAreaRect();
		if (thumb_area.pointInRect(x, y))
		{
			return TRUE;
		}
	}
	return LLFloater::handleMouseDown(x, y, mask);
}

BOOL LLFloaterSnapshot::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (mask == MASK_NONE)
	{
		// In FreezeTime mode, show preview again (after user pressed ESC) when
		// clicking on the thumbnail area.
		LLRect thumb_area = getThumbnailAreaRect();
		if (thumb_area.pointInRect(x, y))
		{
			impl.updateControls(this);
			return TRUE;
		}
	}
	return LLFloater::handleMouseUp(x, y, mask);
}

///----------------------------------------------------------------------------
/// Class LLSnapshotFloaterView
///----------------------------------------------------------------------------

LLSnapshotFloaterView::LLSnapshotFloaterView( const std::string& name, const LLRect& rect ) : LLFloaterView(name, rect)
{
	setMouseOpaque(FALSE);
	setEnabled(FALSE);
}

LLSnapshotFloaterView::~LLSnapshotFloaterView()
{
}

BOOL LLSnapshotFloaterView::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
	// use default handler when not in freeze-frame mode
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if(!freeze_time)
	{
		return LLFloaterView::handleKey(key, mask, called_from_parent);
	}

	if (called_from_parent)
	{
		// pass all keystrokes down
		LLFloaterView::handleKey(key, mask, called_from_parent);
	}
	else
	{
		// bounce keystrokes back down
		LLFloaterView::handleKey(key, mask, TRUE);
	}
	return TRUE;
}

BOOL LLSnapshotFloaterView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// use default handler when not in freeze-frame mode
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if(!freeze_time)
	{
		return LLFloaterView::handleMouseDown(x, y, mask);
	}
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleMouseDown(x, y, mask) == NULL)
	{
		LLToolMgr::getInstance()->getCurrentTool()->handleMouseDown( x, y, mask );
	}
	return TRUE;
}

BOOL LLSnapshotFloaterView::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// use default handler when not in freeze-frame mode
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if(!freeze_time)
	{
		return LLFloaterView::handleMouseUp(x, y, mask);
	}
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleMouseUp(x, y, mask) == NULL)
	{
		LLToolMgr::getInstance()->getCurrentTool()->handleMouseUp( x, y, mask );
	}
	return TRUE;
}

BOOL LLSnapshotFloaterView::handleHover(S32 x, S32 y, MASK mask)
{
	// use default handler when not in freeze-frame mode
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if(!freeze_time)
	{
		return LLFloaterView::handleHover(x, y, mask);
	}	
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleHover(x, y, mask) == NULL)
	{
		LLToolMgr::getInstance()->getCurrentTool()->handleHover( x, y, mask );
	}
	return TRUE;
}
