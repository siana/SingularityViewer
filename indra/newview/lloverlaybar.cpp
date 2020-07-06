/** 
 * @file lloverlaybar.cpp
 * @brief LLOverlayBar class implementation
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

// Temporary buttons that appear at the bottom of the screen when you
// are in a mode.

#include "llviewerprecompiledheaders.h"

#include "lloverlaybar.h"

#include "aoremotectrl.h"
#include "llaudioengine.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llchatbar.h"
#include "llfloaterchatterbox.h"
#include "llmediaremotectrl.h"
#include "llpanelaudiovolume.h"
#include "llparcel.h"
#include "llviewertexturelist.h"
#include "llviewerjoystick.h"
#include "llviewermedia.h"
#include "llviewermenu.h"	// handle_reset_view()
#include "llviewermedia.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "lluictrlfactory.h"
#include "llvoiceclient.h"
#include "llvoavatarself.h"
#include "llvoiceremotectrl.h"
#include "llselectmgr.h"
#include "wlfPanel_AdvSettings.h"
#include "llpanelnearbymedia.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

#include <boost/range/adaptor/reversed.hpp>

//
// Globals
//

LLOverlayBar *gOverlayBar = NULL;

//
// Functions
//



void* LLOverlayBar::createMediaRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mMediaRemote =  new LLMediaRemoteCtrl ();
	return self->mMediaRemote;
}

void* LLOverlayBar::createVoiceRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mVoiceRemote = new LLVoiceRemoteCtrl(std::string("voice_remote"));
	return self->mVoiceRemote;
}

void* LLOverlayBar::createAdvSettings(void* userdata)
{
	return wlfPanel_AdvSettings::getInstance();
}

void* LLOverlayBar::createAORemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mAORemote = new AORemoteCtrl();
	return self->mAORemote;
}

void* LLOverlayBar::createChatBar(void* userdata)
{
	gChatBar = new LLChatBar();
	return gChatBar;
}

LLOverlayBar::LLOverlayBar()
	:	LLLayoutPanel(),
		mMediaRemote(NULL),
		mVoiceRemote(NULL),
		mAORemote(NULL),
		mMusicState(STOPPED),
		mOriginalIMLabel("")
{
	setMouseOpaque(FALSE);
	setIsChrome(TRUE);
	
	mBuilt = false;

	LLCallbackMap::map_t factory_map;
	factory_map["media_remote"] = LLCallbackMap(LLOverlayBar::createMediaRemote, this);
	factory_map["voice_remote"] = LLCallbackMap(LLOverlayBar::createVoiceRemote, this);
	factory_map["Adv_Settings"] = LLCallbackMap(LLOverlayBar::createAdvSettings, this);
	factory_map["ao_remote"] = LLCallbackMap(LLOverlayBar::createAORemote, this);
	factory_map["chat_bar"] = LLCallbackMap(LLOverlayBar::createChatBar, this);
	
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_overlaybar.xml", &factory_map);
}

bool LLOverlayBar::updateAdvSettingsPopup(const LLSD &data)
{
	bool wfl_adv_settings_popup = data.asBoolean();
	wlfPanel_AdvSettings::updateClass();
	LLLayoutPanel* layout_panel = dynamic_cast<LLLayoutPanel*>((LLPanel*)mAdvSettingsContainer);
	if(layout_panel)
	{
		((LLLayoutStack*)layout_panel->getParent())->collapsePanel(layout_panel,!wfl_adv_settings_popup);
		if(wfl_adv_settings_popup)
			layout_panel->setTargetDim(layout_panel->getChild<LLView>("Adv_Settings")->getBoundingRect().getWidth());
	}
	
	return true;
}

bool LLOverlayBar::updateChatVisible(const LLSD &data)
{
	mChatBar->getParent()->setVisible(data.asBoolean());
	return true;
}

bool LLOverlayBar::updateAORemoteVisible(const LLSD &data)
{
	mAORemoteContainer->setVisible(data.asBoolean());
	return true;
}

bool updateNearbyMediaFloater(const LLSD &data)
{
	LLFloaterNearbyMedia::updateClass();
	return true;
}


BOOL LLOverlayBar::postBuild()
{
	childSetAction("New IM",onClickIMReceived,this);
	childSetAction("Set Not Busy",onClickSetNotBusy,this);
	childSetAction("Mouselook",onClickMouselook,this);
	childSetAction("Stand Up",onClickStandUp,this);
	childSetAction("Cancel TP",onClickCancelTP,this);
 	childSetAction("Flycam",onClickFlycam,this);

	mCancelBtn = getChild<LLButton>("Cancel TP");
	setFocusRoot(TRUE);
	mBuilt = true;

	mUnreadCountStringPlural = getString("unread_count_string_plural");

	mChatbarAndButtons.connect(this,"chatbar_and_buttons");
	mNewIM.connect(this,"New IM");
	mNotBusy.connect(this,"Set Not Busy");
	mMouseLook.connect(this,"Mouselook");
	mStandUp.connect(this,"Stand Up");
	mFlyCam.connect(this,"Flycam");
	mChatBar.connect(this,"chat_bar");
	mVoiceRemoteContainer.connect(this,"voice_remote_container");
	mStateManagementContainer.connect(this,"state_management_buttons_container");
	mAORemoteContainer.connect(this,"ao_remote_container");
	mAdvSettingsContainer.connect(this,"AdvSettings_container");
	mMediaRemoteContainer.connect(this,"media_remote_container");

	updateAdvSettingsPopup(gSavedSettings.getBOOL("wlfAdvSettingsPopup"));
	updateChatVisible(gSavedSettings.getBOOL("ChatVisible"));
	updateAORemoteVisible(gSavedSettings.getBOOL("EnableAORemote"));

	mOriginalIMLabel = mNewIM->getLabelSelected();

	layoutButtons();

	gSavedSettings.getControl("wlfAdvSettingsPopup")->getSignal()->connect(boost::bind(&LLOverlayBar::updateAdvSettingsPopup,this,_2));
	gSavedSettings.getControl("ChatVisible")->getSignal()->connect(boost::bind(&LLOverlayBar::updateChatVisible,this,_2));
	gSavedSettings.getControl("EnableAORemote")->getSignal()->connect(boost::bind(&LLOverlayBar::updateAORemoteVisible,this,_2));
	gSavedSettings.getControl("ShowNearbyMediaFloater")->getSignal()->connect(boost::bind(&updateNearbyMediaFloater,_2));

	mAORemoteContainer->setVisible(gSavedSettings.getBOOL("EnableAORemote"));



	return TRUE;
}

LLOverlayBar::~LLOverlayBar()
{
	// LLView destructor cleans up children
}

// virtual
void LLOverlayBar::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	S32 delta_width = width - getRect().getWidth();
	S32 delta_height = height - getRect().getHeight();

	if (!delta_width && !delta_height && !sForceReshape)
		return;

	LLView::reshape(width, height, called_from_parent);

	if (mBuilt) 
	{
		layoutButtons();
	}
}

void LLOverlayBar::layoutButtons()
{
	if (mStateManagementContainer->getVisible())
	{
		U32 button_count = 0;
		const child_list_t& view_list = *(mStateManagementContainer->getChildList());
		for (LLView* viewp : view_list)
		{
			if(!viewp->getEnabled())
				continue;
			++button_count;
		}
		const S32 MAX_BAR_WIDTH = 600;
		S32 bar_width = llclamp(mStateManagementContainer->getRect().getWidth(), 0, MAX_BAR_WIDTH);

		// calculate button widths
		const S32 MAX_BUTTON_WIDTH = 150;

		static LLCachedControl<S32> status_bar_pad("StatusBarPad",10);
		S32 segment_width = llclamp(lltrunc((F32)(bar_width) / (F32)button_count), 0, MAX_BUTTON_WIDTH);
		S32 btn_width = segment_width - status_bar_pad;

		// Evenly space all buttons, starting from left
		S32 left = 0;
		S32 bottom = 1;

		for (LLView* viewp : boost::adaptors::reverse(view_list))
		{
			if(!viewp->getEnabled())
				continue;
			LLRect r = viewp->getRect();
			//if(dynamic_cast<LLButton*>(viewp))
			//	new_width = llclamp(new_width,0,MAX_BUTTON_WIDTH);
			r.setOriginAndSize(left, bottom, btn_width, r.getHeight());
			viewp->setRect(r);
			left += segment_width;
		}
	}
}

LLButton* LLOverlayBar::updateButtonVisiblity(LLButton* button, bool visible)
{
	if (button && (bool)button->getVisible() != visible)
	{
		button->setVisible(visible);
		sendChildToFront(button);
		moveChildToBackOfTabGroup(button);
	}
	return button;
}

// Per-frame updates of visibility
void LLOverlayBar::refresh()
{
	bool buttons_changed = FALSE;

	int unread_count(gIMMgr->getIMUnreadCount());
	static const LLCachedControl<bool> per_conversation("NewIMsPerConversation");
	static const LLCachedControl<bool> reset_count("NewIMsPerConversationReset");
	if (per_conversation && (!reset_count || unread_count) && !LLFloaterChatterBox::instanceVisible())
	{
		unread_count = 0;
		for(std::set<LLHandle<LLFloater> >::const_iterator it = gIMMgr->getIMFloaterHandles().begin(); it != gIMMgr->getIMFloaterHandles().end(); ++it)
			if (LLFloaterIMPanel* im_floater = static_cast<LLFloaterIMPanel*>(it->get()))
				if (im_floater->getParent() != gFloaterView && im_floater->getNumUnreadMessages()) // Only count docked IMs
					++unread_count;
	}
	if (LLButton* button = updateButtonVisiblity(mNewIM, unread_count))
	{
		if (unread_count > 0)
		{
			if (unread_count > 1)
			{
				std::stringstream ss;
				ss << unread_count << " " << mUnreadCountStringPlural;
				button->setLabel(ss.str());
			}
			else
			{
				button->setLabel("1 " + mOriginalIMLabel);
			}
		}
		buttons_changed = true;
	}
	buttons_changed |= updateButtonVisiblity(mNotBusy, gAgent.isDoNotDisturb()) != NULL;
	buttons_changed |= updateButtonVisiblity(mFlyCam,LLViewerJoystick::getInstance()->getOverrideCamera()) != NULL;
	buttons_changed |= updateButtonVisiblity(mMouseLook,gAgent.isControlBlocked(CONTROL_ML_LBUTTON_DOWN_INDEX)||gAgent.isControlBlocked(CONTROL_ML_LBUTTON_UP_INDEX)) != NULL;
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
//  buttons_changed |= updateButtonVisiblity("Stand Up", isAgentAvatarValid() && gAgentAvatarp->isSitting()) != NULL;
	buttons_changed |= updateButtonVisiblity(mStandUp,isAgentAvatarValid() && gAgentAvatarp->isSitting() && !gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) != NULL;
// [/RLVa:KB]
	buttons_changed |= updateButtonVisiblity(mCancelBtn,(gAgent.getTeleportState() >= LLAgent::TELEPORT_START) &&	(gAgent.getTeleportState() <= LLAgent::TELEPORT_MOVING)) != NULL;

	moveChildToBackOfTabGroup(mAORemote);
	moveChildToBackOfTabGroup(mMediaRemote);
	moveChildToBackOfTabGroup(mVoiceRemote);

	// turn off the whole bar in mouselook
	static BOOL last_mouselook = FALSE;

	BOOL in_mouselook = gAgentCamera.cameraMouselook();

	if(last_mouselook != in_mouselook)
	{
		last_mouselook = in_mouselook;

		static const LLCachedControl<bool> enable_ao_remote("EnableAORemote", true);
		mMediaRemoteContainer->setVisible(!in_mouselook);
		mVoiceRemoteContainer->setVisible(!in_mouselook && LLVoiceClient::getInstance()->voiceEnabled());
		mAdvSettingsContainer->setVisible(!in_mouselook);
		mAORemoteContainer->setVisible(!in_mouselook && enable_ao_remote);
		mStateManagementContainer->setVisible(!in_mouselook);
	}
	if(!in_mouselook)
		mVoiceRemoteContainer->setVisible(LLVoiceClient::getInstance()->voiceEnabled());

	if (buttons_changed)
	{
		layoutButtons();
	}
}

//-----------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------

// static
void LLOverlayBar::onClickIMReceived(void*)
{
	gIMMgr->setFloaterOpen(TRUE);
}


// static
void LLOverlayBar::onClickSetNotBusy(void*)
{
	gAgent.setDoNotDisturb(false);
}


// static
void LLOverlayBar::onClickFlycam(void*)
{
	LLViewerJoystick::getInstance()->toggleFlycam();
}

// static
void LLOverlayBar::onClickResetView(void* data)
{
	handle_reset_view();
}

//static
void LLOverlayBar::onClickMouselook(void*)
{
	gAgentCamera.changeCameraToMouselook();
}

//static
void LLOverlayBar::onClickStandUp(void*)
{
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) && (gAgentAvatarp) && (gAgentAvatarp->isSitting()) )
	{
		return;
	}
// [/RLVa:KB]

	LLSelectMgr::getInstance()->deselectAllForStandingUp();
	gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
}

//static
void LLOverlayBar::onClickCancelTP(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	self->setCancelTPButtonVisible(FALSE, std::string("Cancel TP"));
	gAgent.teleportCancel();
	LL_INFOS() << "trying to cancel teleport" << LL_ENDL;
}

void LLOverlayBar::setCancelTPButtonVisible(BOOL b, const std::string& label)
{
	mCancelBtn->setVisible( b );
//	mCancelBtn->setEnabled( b );
	mCancelBtn->setLabelSelected(label);
	mCancelBtn->setLabelUnselected(label);
}


////////////////////////////////////////////////////////////////////////////////
void LLOverlayBar::audioFilterPlay()
{
	if (gOverlayBar && gOverlayBar->mMusicState != PLAYING)
	{
		gOverlayBar->mMusicState = PLAYING;
	}
}

void LLOverlayBar::audioFilterStop()
{
	if (gOverlayBar && gOverlayBar->mMusicState != STOPPED)
	{
		gOverlayBar->mMusicState = STOPPED;
	}
}

////////////////////////////////////////////////////////////////////////////////
// static media helpers
// *TODO: Move this into an audio manager abstraction
//static
void LLOverlayBar::mediaStop(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	LLViewerParcelMedia::stop();
}
//static
void LLOverlayBar::toggleMediaPlay(void*)
{
	if (!gOverlayBar)
	{
		return;
	}

	
	if (LLViewerParcelMedia::getStatus() == LLViewerMediaImpl::MEDIA_PAUSED)
	{
		LLViewerParcelMedia::start();
	}
	else if(LLViewerParcelMedia::getStatus() == LLViewerMediaImpl::MEDIA_PLAYING)
	{
		LLViewerParcelMedia::pause();
	}
	else
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (parcel)
		{
			LLViewerParcelMedia::play(parcel);
		}
	}
}

//static
void LLOverlayBar::toggleMusicPlay(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	
	if (gOverlayBar->mMusicState != PLAYING)
	{
		gOverlayBar->mMusicState = PLAYING; // desired state
		if (gAudiop)
		{
			LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			if ( parcel )
			{
				// this doesn't work properly when crossing parcel boundaries - even when the 
				// stream is stopped, it doesn't return the right thing - commenting out for now.
	// 			if ( gAudiop->isInternetStreamPlaying() == 0 )
				{
					LLViewerParcelMedia::playStreamingMusic(parcel);
				}
			}
		}
	}
	//else
	//{
	//	gOverlayBar->mMusicState = PAUSED; // desired state
	//	if (gAudiop)
	//	{
	//		gAudiop->pauseInternetStream(1);
	//	}
	//}
	else
	{
		gOverlayBar->mMusicState = STOPPED; // desired state
		if (gAudiop)
		{
			gAudiop->stopInternetStream();
		}
	}
}

