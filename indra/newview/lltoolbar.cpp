/** 
 * @file lltoolbar.cpp
 * @author James Cook, Richard Nelson
 * @brief Large friendly buttons at bottom of screen.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "lltoolbar.h"

#include "llflyoutbutton.h"
#include "llscrolllistitem.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llfloaterchat.h"
#include "llfloaterchatterbox.h"
#include "llfloaterfriends.h"
#include "llfloaterinventory.h"
#include "llfloatermute.h"
#include "llimpanel.h"
#include "llimview.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "llviewerparcelmgr.h"
#include "llvoavatarself.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

#if LL_DARWIN

#include "llresizehandle.h"
#include "llviewerwindow.h"

// This class draws like an LLResizeHandle but has no interactivity.
// It's just there to provide a cue to the user that the lower right corner of the window functions as a resize handle.
class LLFakeResizeHandle : public LLResizeHandle
{
public:
	LLFakeResizeHandle(const LLResizeHandle::Params& p) : LLResizeHandle(p) {}

	virtual BOOL handleHover(S32 x, S32 y, MASK mask) { return false; }
	virtual BOOL handleMouseDown(S32 x, S32 y, MASK mask) { return false; }
	virtual BOOL handleMouseUp(S32 x, S32 y, MASK mask) { return false; }
	virtual void reshape(S32 width, S32 height, BOOL called_from_parent)
	{
		// Only when running in windowed mode on the Mac, leave room for a resize widget on the right edge of the bar.
		if (gViewerWindow->getWindow()->getFullscreen())
			return setVisible(false);

		setVisible(true);
		const F32 wide(gViewerWindow->getWindowWidth() + 2);
		setRect(LLRect(wide - RESIZE_HANDLE_WIDTH, RESIZE_HANDLE_HEIGHT, wide, 0));
	}
};

#endif // LL_DARWIN

//
// Globals
//

LLToolBar *gToolBar = NULL;
S32 TOOL_BAR_HEIGHT = 20;

//
// Statics
//
F32	LLToolBar::sInventoryAutoOpenTime = 1.f;

//
// Functions
//
void show_floater(const std::string& floater_name);
void show_inv_floater(const LLSD& userdata, const std::string& field);
void show_web_floater(const std::string& type);

LLToolBar::LLToolBar()
:	LLLayoutPanel()
{
	setIsChrome(TRUE);
	setFocusRoot(TRUE);
	mCommitCallbackRegistrar.add("ShowFloater", boost::bind(show_floater, _2));
	mCommitCallbackRegistrar.add("ShowInvFloater.ID", boost::bind(show_inv_floater, _2, "id"));
	mCommitCallbackRegistrar.add("ShowInvFloater.Name", boost::bind(show_inv_floater, _2, "name"));
	mCommitCallbackRegistrar.add("ShowInvFloater.Type", boost::bind(show_inv_floater, _2, "type"));
	mCommitCallbackRegistrar.add("ShowWebFloater", boost::bind(show_web_floater, _2));
}


BOOL LLToolBar::postBuild()
{
	mCommunicateBtn.connect(this, "communicate_btn");
	mCommunicateBtn->setCommitCallback(boost::bind(&LLToolBar::onClickCommunicate, this, _2));
	mFlyBtn.connect(this, "fly_btn");
	mBuildBtn.connect(this, "build_btn");
	mMapBtn.connect(this, "map_btn");
	mRadarBtn.connect(this, "radar_btn");
	mInventoryBtn.connect(this, "inventory_btn");

#if LL_DARWIN
	LLResizeHandle::Params p;
	p.rect(LLRect(0, 0, RESIZE_HANDLE_WIDTH, RESIZE_HANDLE_HEIGHT));
	p.name(std::string(""));
	p.min_width(RESIZE_HANDLE_WIDTH);
	p.min_height(RESIZE_HANDLE_HEIGHT);
	p.corner(LLResizeHandle::RIGHT_BOTTOM);
	addChildInBack(new LLFakeResizeHandle(p));
	reshape(getRect().getWidth(), getRect().getHeight());
#endif // LL_DARWIN

	return TRUE;
}

LLToolBar::~LLToolBar()
{
	// LLView destructor cleans up children
}


BOOL LLToolBar::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,
									 EAcceptance* accept,
									 std::string& tooltip_msg)
{
	LLButton* inventory_btn = mInventoryBtn;
	if (!inventory_btn || !inventory_btn->getVisible()) return FALSE;

	LLPanelMainInventory* active_inventory = LLPanelMainInventory::getActiveInventory();

	if (active_inventory && active_inventory->getVisible())
	{
		mInventoryAutoOpenTimer.stop();
	}
	else if (inventory_btn->getRect().pointInRect(x, y))
	{
		if (mInventoryAutoOpenTimer.getStarted())
		{
			if (!(active_inventory && active_inventory->getVisible()) && 
			mInventoryAutoOpenTimer.getElapsedTimeF32() > sInventoryAutoOpenTime)
			{
				LLPanelMainInventory::showAgentInventory();
			}
		}
		else
		{
			mInventoryAutoOpenTimer.start();
		}
	}

	return LLPanel::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
}


// Per-frame updates of visibility
void LLToolBar::refresh()
{
	static const LLCachedControl<bool> show_toolbar("ShowToolBar", true);
	bool show = show_toolbar;
	if (show && gAgentCamera.cameraMouselook())
	{
		static const LLCachedControl<bool> hidden("LiruMouselookHidesToolbar");
		show = !hidden;
	}
	setVisible(show);
	if (!show) return; // Everything below this point manipulates visible UI, anyway

	updateCommunicateList();

	if (!isAgentAvatarValid()) return;

	static const LLCachedControl<bool> continue_flying_on_unsit("LiruContinueFlyingOnUnsit");
	mFlyBtn->setEnabled((gAgent.canFly() || gAgent.getFlying()) && (continue_flying_on_unsit || !gAgentAvatarp->isSitting()));
	static const LLCachedControl<bool> ascent_build_always_enabled("AscentBuildAlwaysEnabled", true);
	mBuildBtn->setEnabled(ascent_build_always_enabled || LLViewerParcelMgr::getInstance()->allowAgentBuild());

	// Check to see if we're in build mode
	// And not just clicking on a scripted object
	bool build_mode = LLToolMgr::getInstance()->inEdit() && !LLToolGrab::getInstance()->getHideBuildHighlight();
	static LLCachedControl<bool> build_btn_state("BuildBtnState",false);
	if (build_btn_state != build_mode)
		build_btn_state = build_mode;

// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-10 (RLVa-1.0.0g)
	// Called per-frame so this really can't be slow
	if (rlv_handler_t::isEnabled())
	{
		// If we're rez-restricted, we can still edit => allow build floater
		// If we're edit-restricted, we can still rez => allow build floater
		mBuildBtn->setEnabled(!(gRlvHandler.hasBehaviour(RLV_BHVR_REZ) && gRlvHandler.hasBehaviour(RLV_BHVR_EDIT)));

		mMapBtn->setEnabled(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWWORLDMAP));
		mRadarBtn->setEnabled(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWMINIMAP));
		mInventoryBtn->setEnabled(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV));
	}
// [/RLVa:KB]
}

void bold_if_equal(const LLFloater* f1, const LLFloater* f2, LLScrollListItem* itemp)
{
	if (f1 != f2) return;
	static_cast<LLScrollListText*>(itemp->getColumn(0))->setFontStyle(LLFontGL::BOLD);
}

void LLToolBar::updateCommunicateList()
{
	if (!mCommunicateBtn->getVisible()) return;

	LLSD selected = mCommunicateBtn->getValue();

	mCommunicateBtn->removeall();

	const LLFloater* frontmost_floater = LLFloaterChatterBox::getInstance()->getActiveFloater();
	bold_if_equal(LLFloaterMyFriends::getInstance(), frontmost_floater, mCommunicateBtn->add(LLFloaterMyFriends::getInstance()->getShortTitle(), LLSD("contacts"), ADD_TOP));
	bold_if_equal(LLFloaterChat::getInstance(), frontmost_floater, mCommunicateBtn->add(LLFloaterChat::getInstance()->getShortTitle(), LLSD("local chat"), ADD_TOP));
	mCommunicateBtn->addSeparator(ADD_TOP);
	static const auto redock = getString("Redock Windows");
	mCommunicateBtn->add(redock, LLSD("redock"), ADD_TOP);
	mCommunicateBtn->addSeparator(ADD_TOP);
	bold_if_equal(LLFloaterMute::getInstance(), frontmost_floater, mCommunicateBtn->add(LLFloaterMute::getInstance()->getShortTitle(), LLSD("mute list"), ADD_TOP));

	if (gIMMgr->getIMFloaterHandles().size() > 0) mCommunicateBtn->addSeparator(ADD_TOP);
	for(const auto& handle : gIMMgr->getIMFloaterHandles())
	{
		if (LLFloaterIMPanel* im_floaterp = (LLFloaterIMPanel*)handle.get())
		{
			const S32 count = im_floaterp->getNumUnreadMessages();
			std::string floater_title;
			if (count > 0) floater_title = '*';
			floater_title.append(im_floaterp->getShortTitle());
			static const LLCachedControl<bool> show_counts("ShowUnreadIMsCounts", true);
			if (show_counts && count > 0)
			{
				floater_title += " - ";
				if (count > 1)
				{
					LLStringUtil::format_map_t args;
					args["COUNT"] = llformat("%d", count);
					static LLUIString ims = getString("IMs");
					ims.setArgList(args);
					floater_title += ims.getString();
				}
				else
				{
					static const auto im = getString("IM");
					floater_title += im;
				}
			}
			bold_if_equal(im_floaterp, frontmost_floater, mCommunicateBtn->add(floater_title, im_floaterp->getSessionID(), ADD_TOP));
		}
	}

	static const LLCachedControl<bool> show_comm("ShowCommunicate", true);
	mCommunicateBtn->setToggleState(show_comm);
	if (!selected.isUndefined()) mCommunicateBtn->setValue(selected);
}


// static
void LLToolBar::onClickCommunicate(const LLSD& selected_option)
{
	if (selected_option.asString() == "contacts")
	{
		LLFloaterMyFriends::showInstance();
	}
	else if (selected_option.asString() == "local chat")
	{
		LLFloaterChat::showInstance();
	}
	else if (selected_option.asString() == "redock")
	{
		LLFloaterChatterBox::getInstance()->addFloater(LLFloaterMyFriends::getInstance(), FALSE);
		LLFloaterChatterBox::getInstance()->addFloater(LLFloaterChat::getInstance(), FALSE);
		LLUUID session_to_show;
		
		std::set<LLHandle<LLFloater> >::const_iterator floater_handle_it;
		for(floater_handle_it = gIMMgr->getIMFloaterHandles().begin(); floater_handle_it != gIMMgr->getIMFloaterHandles().end(); ++floater_handle_it)
		{
			LLFloater* im_floaterp = floater_handle_it->get();
			if (im_floaterp)
			{
				if (im_floaterp->isFrontmost())
				{
					session_to_show = ((LLFloaterIMPanel*)im_floaterp)->getSessionID();
				}
				LLFloaterChatterBox::getInstance()->addFloater(im_floaterp, FALSE);
			}
		}

		LLFloaterChatterBox::showInstance(session_to_show);
	}
	else if (selected_option.asString() == "mute list")
	{
		LLFloaterMute::showInstance();
	}
	else if (selected_option.isUndefined()) // user just clicked the communicate button, treat as toggle
	{
		if (LLFloaterChatterBox::getInstance()->getFloaterCount() == 0)
		{
			LLFloaterMyFriends::toggleInstance();
		}
		else
		{
			LLFloaterChatterBox::toggleInstance();
		}
	}
	else // otherwise selection_option is a specific IM session id
	{
		LLFloaterChatterBox::showInstance(selected_option);
	}
}
