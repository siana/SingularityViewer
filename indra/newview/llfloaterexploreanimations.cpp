// <edit>

#include "llviewerprecompiledheaders.h"
#include "llfloaterexploreanimations.h"
#include "lluictrlfactory.h"
#include "llscrolllistctrl.h"
#include "llagentdata.h"
#include "llfloaterbvhpreview.h"
#include "llvoavatar.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "lltoolmgr.h"
// <stuff for jelly roll>
#include "llfloatertools.h"
#include "llselectmgr.h"
// </stuff for jelly roll>

std::map< LLUUID, std::list< LLAnimHistoryItem > > LLFloaterExploreAnimations::animHistory;

LLAnimHistoryItem::LLAnimHistoryItem(LLUUID assetid, bool playing)
:	mAssetID(assetid)
,	mPlaying(playing)
,	mTimeStarted(LLTimer::getElapsedSeconds())
,	mTimeStopped(playing ? 0.f : mTimeStarted)
{}
bool LLAnimHistoryItem::setPlaying(bool playing)
{
	if (mPlaying == playing) return false;
	mPlaying = playing;
	playing ? mTimeStarted : mTimeStopped = LLTimer::getElapsedSeconds();
	return true;
}

void LLFloaterExploreAnimations::show()
{
	LLViewerObject* avatar = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
	const LLUUID& id(avatar ? avatar->getID() : gAgentID);
	if (LLFloaterExploreAnimations* f = getInstance(id))
		if (avatar)	f->open();
		else		f->close();
	else
		(new LLFloaterExploreAnimations(id))->open();
}

LLFloaterExploreAnimations::LLFloaterExploreAnimations(const LLUUID avatarid)
:	LLInstanceTracker<LLFloaterExploreAnimations, LLUUID>(avatarid)
,	mAnimPreview(256, 256)
{
	mLastMouseX = 0;
	mLastMouseY = 0;

	mAnimPreview.setZoom(2.0f);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_explore_animations.xml");
}

LLFloaterExploreAnimations::~LLFloaterExploreAnimations()
{
}

BOOL LLFloaterExploreAnimations::postBuild()
{
	getChild<LLUICtrl>("anim_list")->setCommitCallback(boost::bind(&LLFloaterExploreAnimations::onSelectAnimation, this));
	LLRect r = getRect();
	mPreviewRect.set(r.getWidth() - 266, r.getHeight() - 25, r.getWidth() - 10, r.getHeight() - 256);
	update();
	return TRUE;
}

void LLFloaterExploreAnimations::update()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("anim_list");
	LLUUID selection = list->getCurrentID();
	list->clearRows(); // do this differently probably

	std::list<LLAnimHistoryItem> history = animHistory[getKey()];
	std::list<LLAnimHistoryItem>::iterator end = history.end();
	for(std::list<LLAnimHistoryItem>::iterator iter = history.begin(); iter != end; ++iter)
	{
		LLAnimHistoryItem item = (*iter);

		LLSD element;
		element["id"] = item.mAssetID;

		LLSD& name_column = element["columns"][0];
		name_column["column"] = "name";
		name_column["value"] = item.mAssetID.asString();

		LLSD& info_column = element["columns"][1];
		info_column["column"] = "info";
		if(item.mPlaying)
			info_column["value"] = "Playing";
		else
			info_column["value"] = llformat("%.1f min ago", (LLTimer::getElapsedSeconds() - item.mTimeStopped) / 60.f);

		list->addElement(element, ADD_BOTTOM);
	}

	list->selectByID(selection);
}

void LLFloaterExploreAnimations::draw()
{
	LLFloater::draw();

	LLRect r = getRect();
	mPreviewRect.set(r.getWidth() - 266, r.getHeight() - 25, r.getWidth() - 10, r.getHeight() - 256);

	gGL.color3f(1.f, 1.f, 1.f);

	gGL.getTexUnit(0)->bind(&mAnimPreview);

	gGL.begin( LLRender::TRIANGLE_STRIP );
	{
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex2i(mPreviewRect.mLeft, mPreviewRect.mTop);
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2i(mPreviewRect.mLeft, mPreviewRect.mBottom);
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex2i(mPreviewRect.mRight, mPreviewRect.mTop);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex2i(mPreviewRect.mRight, mPreviewRect.mBottom);
	}
	gGL.end();

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	//LLVOAvatar* avatarp = mAnimPreview.getDummyAvatar();
	//if (!avatarp->areAnimationsPaused())
	//{
	//	mAnimPreview.requestUpdate();
	//}
}

// static
void LLFloaterExploreAnimations::processAnim(LLUUID avatarid, LLUUID assetid, bool playing)
{
	std::string asset_str = assetid.asString();
	if(asset_str.find("17132261-c061") != std::string::npos) return; // dog1
	else if(asset_str.find("fea558cb-8b9b") != std::string::npos) return; // dog2
	else if(asset_str.find("50cb5750-0743") != std::string::npos) return; // dog3

	std::list<LLAnimHistoryItem>& history = animHistory[avatarid];
	std::list<LLAnimHistoryItem>::iterator end = history.end();
	for(std::list<LLAnimHistoryItem>::iterator iter = history.begin(); iter != end; ++iter)
	{
		LLAnimHistoryItem& item = (*iter);
		if (item.mAssetID != assetid) continue;
		if (item.setPlaying(playing))
			handleHistoryChange(avatarid);
		return;
	}
	// Trim it
	if (history.size() > 31)
		history.resize(31);

	history.push_back(LLAnimHistoryItem(assetid, playing));
	handleHistoryChange(avatarid);
}

class LLAnimHistoryItemCompare
{
public:
	bool operator() (LLAnimHistoryItem first, LLAnimHistoryItem second)
	{
		if (first.mPlaying)
		{
			if (second.mPlaying)
			{
				return (first.mTimeStarted > second.mTimeStarted);
			}
			else
			{
				return true;
			}
		}
		else if (second.mPlaying)
		{
			return false;
		}
		else
		{
			return (first.mTimeStopped > second.mTimeStopped);
		}
	}
};

// static
void LLFloaterExploreAnimations::handleHistoryChange(LLUUID avatarid)
{
	// Sort it
	animHistory[avatarid].sort(LLAnimHistoryItemCompare());

	// Update floater
	if (LLFloaterExploreAnimations* f = getInstance(avatarid))
		f->update();
}

void LLFloaterExploreAnimations::onSelectAnimation()
{
	mAnimPreview.getDummyAvatar()->deactivateAllMotions();
	mAnimPreview.getDummyAvatar()->startMotion(getChild<LLScrollListCtrl>("anim_list")->getCurrentID(), 0.f);
	mAnimPreview.setZoom(2.0f);
}

//-----------------------------------------------------------------------------
// handleMouseDown()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mPreviewRect.pointInRect(x, y))
	{
		//LL_INFOS() << "lolwut" << LL_ENDL;
		bringToFront( x, y );
		gFocusMgr.setMouseCapture(this);
		gViewerWindow->hideCursor();
		mLastMouseX = x;
		mLastMouseY = y;
		return TRUE;
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleMouseUp()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleMouseUp(S32 x, S32 y, MASK mask)
{
	//LL_INFOS() << "lolwut pear" << LL_ENDL;
	gFocusMgr.setMouseCapture(FALSE);
	gViewerWindow->showCursor();
	return LLFloater::handleMouseUp(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleHover()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleHover(S32 x, S32 y, MASK mask)
{
	MASK local_mask = mask & ~MASK_ALT;

	if (hasMouseCapture())
	{
		if (local_mask == MASK_PAN)
		{
			// pan here
			mAnimPreview.pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
		}
		else if (local_mask == MASK_ORBIT)
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;
			
			mAnimPreview.rotate(yaw_radians, pitch_radians);
		}
		else 
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;
			
			mAnimPreview.rotate(yaw_radians, 0.f);
			mAnimPreview.zoom(zoom_amt);
		}

		mAnimPreview.requestUpdate();

		LLUI::setMousePositionLocal(this, mLastMouseX, mLastMouseY);
	}

	if (!mPreviewRect.pointInRect(x, y))
	{
		return LLFloater::handleHover(x, y, mask);
	}
	else if (local_mask == MASK_ORBIT)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLCAMERA);
	}
	else if (local_mask == MASK_PAN)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLZOOMIN);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// handleScrollWheel()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	mAnimPreview.zoom((F32)clicks * -0.2f);
	mAnimPreview.requestUpdate();
	return TRUE;
}

//-----------------------------------------------------------------------------
// onMouseCaptureLost()
//-----------------------------------------------------------------------------
void LLFloaterExploreAnimations::onMouseCaptureLost()
{
	gViewerWindow->showCursor();
}
// </edit>
