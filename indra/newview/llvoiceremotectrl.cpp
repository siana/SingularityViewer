/* @file llvoiceremotectrl.cpp
 * @brief A remote control for voice chat
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llvoiceremotectrl.h"

#include "llagent.h"
#include "llui.h"
#include "llbutton.h"
#include "lluictrlfactory.h"
#include "llvoicechannel.h"
#include "llvoiceclient.h"
#include "llfloateractivespeakers.h"
#include "llfloaterchatterbox.h"
#include "lliconctrl.h"
#include "lloverlaybar.h"
#include "lltextbox.h"

LLVoiceRemoteCtrl::LLVoiceRemoteCtrl (const std::string& name) : LLPanel(name)
{
	setIsChrome(TRUE);

	if (gSavedSettings.getBOOL("ShowVoiceChannelPopup"))
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "panel_voice_remote_expanded.xml");
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "panel_voice_remote.xml");
	}

	setFocusRoot(TRUE);
}

LLVoiceRemoteCtrl::~LLVoiceRemoteCtrl()
{
}

BOOL LLVoiceRemoteCtrl::postBuild()
{
	mTalkBtn = getChild<LLButton>("push_to_talk");
	mTalkBtn->setClickedCallback(boost::bind(&LLVoiceRemoteCtrl::onBtnTalkClicked));
	mTalkBtn->setHeldDownCallback(boost::bind(&LLVoiceRemoteCtrl::onBtnTalkHeld));
	mTalkBtn->setMouseUpCallback(boost::bind(&LLVoiceRemoteCtrl::onBtnTalkReleased));

	mTalkLockBtn = getChild<LLButton>("ptt_lock");
	mTalkLockBtn->setClickedCallback(boost::bind(&LLVoiceRemoteCtrl::onBtnLock,this));

	mSpeakersBtn = getChild<LLButton>("speakers_btn");
	mSpeakersBtn->setClickedCallback(boost::bind(&LLVoiceRemoteCtrl::onClickSpeakers));

	childSetAction("show_channel", onClickPopupBtn, this);
	if (LLButton* end_call_btn = findChild<LLButton>("end_call_btn"))
		end_call_btn->setClickedCallback(boost::bind(&LLVoiceRemoteCtrl::onClickEndCall));

	LLTextBox* text = findChild<LLTextBox>("channel_label");
	if (text)
	{
		text->setUseEllipses(TRUE);
	}

	if (LLButton* voice_channel_bg = findChild<LLButton>("voice_channel_bg"))
		voice_channel_bg->setClickedCallback(boost::bind(&LLVoiceRemoteCtrl::onClickVoiceChannel));

	mVoiceVolIcon.connect(this,"voice_volume");
	mShowChanBtn.connect(this,"show_channel");
	return TRUE;
}

void LLVoiceRemoteCtrl::draw()
{
	BOOL voice_active = FALSE;
	LLVoiceChannel* channelp = LLVoiceChannel::getCurrentVoiceChannel();
	if (channelp)
	{
		voice_active = channelp->isActive();
	}

	mTalkBtn->setEnabled(voice_active);
	mTalkLockBtn->setEnabled(voice_active);

	static LLCachedControl<bool> ptt_currently_enabled("PTTCurrentlyEnabled",false);
	// propagate ptt state to button display,
	if (!mTalkBtn->hasMouseCapture())
	{
		// not in push to talk mode, or push to talk is active means I'm talking
		mTalkBtn->setToggleState(!ptt_currently_enabled || LLVoiceClient::getInstance()->getUserPTTState());
	}
	mSpeakersBtn->setToggleState(LLFloaterActiveSpeakers::instanceVisible(LLSD()));
	mTalkLockBtn->setToggleState(!ptt_currently_enabled);

	std::string talk_blip_image;
	if (LLVoiceClient::getInstance()->getIsSpeaking(gAgent.getID()))
	{
		F32 voice_power = LLVoiceClient::getInstance()->getCurrentPower(gAgent.getID());

		if (voice_power > LLVoiceClient::OVERDRIVEN_POWER_LEVEL)
		{
			talk_blip_image = "icn_voice_ptt-on-lvl3.tga";
		}
		else
		{
			F32 power = LLVoiceClient::getInstance()->getCurrentPower(gAgent.getID());
			S32 icon_image_idx = llmin(2, llfloor((power / LLVoiceClient::OVERDRIVEN_POWER_LEVEL) * 3.f));

			switch(icon_image_idx)
			{
			case 0:
				talk_blip_image = "icn_voice_ptt-on.tga";
				break;
			case 1:
				talk_blip_image = "icn_voice_ptt-on-lvl1.tga";
				break;
			case 2:
				talk_blip_image = "icn_voice_ptt-on-lvl2.tga";
				break;
			}
		}
	}
	else
	{
		talk_blip_image = "icn_voice_ptt-off.tga";
	}

	LLIconCtrl* icon = mVoiceVolIcon;
	if (icon)
	{
		icon->setImage(talk_blip_image);
	}

	LLFloater* voice_floater = LLFloaterChatterBox::getInstance()->getCurrentVoiceFloater();
	LLVoiceChannel* current_channel = LLVoiceChannel::getCurrentVoiceChannel();
	if (!voice_floater) // Maybe it's undocked
	{
		voice_floater = gIMMgr->findFloaterBySession(current_channel->getSessionID());
	}
	std::string active_channel_name;
	if (voice_floater)
	{
		active_channel_name = voice_floater->getShortTitle();
	}

	if (LLButton* end_call_btn = findChild<LLButton>("end_call_btn"))
		end_call_btn->setEnabled(LLVoiceClient::getInstance()->voiceEnabled()
								&& current_channel
								&& current_channel->isActive()
								&& current_channel != LLVoiceChannelProximal::getInstance());

	if (LLTextBox* text = findChild<LLTextBox>("channel_label"))
		text->setValue(active_channel_name);
	LLButton* voice_channel_bg = findChild<LLButton>("voice_channel_bg");
	if (voice_channel_bg) voice_channel_bg->setToolTip(active_channel_name);

	if (current_channel)
	{
		LLIconCtrl* voice_channel_icon = findChild<LLIconCtrl>("voice_channel_icon");
		if (voice_channel_icon && voice_floater)
		{
			voice_channel_icon->setImage(voice_floater->getString("voice_icon"));
		}

		if (voice_channel_bg)
		{
			LLColor4 bg_color;
			if (current_channel->isActive())
			{
				bg_color = lerp(LLColor4::green, LLColor4::white, 0.7f);
			}
			else if (current_channel->getState() == LLVoiceChannel::STATE_ERROR)
			{
				bg_color = lerp(LLColor4::red, LLColor4::white, 0.7f);
			}
			else // active, but not connected
			{
				bg_color = lerp(LLColor4::yellow, LLColor4::white, 0.7f);
			}
			voice_channel_bg->setImageColor(bg_color);
		}
	}

	LLButton* expand_button = mShowChanBtn;
	if (expand_button)
	{
		if (expand_button->getToggleState())
		{
			expand_button->setImageOverlay(std::string("arrow_down.tga"));
		}
		else
		{
			expand_button->setImageOverlay(std::string("arrow_up.tga"));
		}
	}

	LLPanel::draw();
}

void LLVoiceRemoteCtrl::onBtnTalkClicked()
{
	// when in toggle mode, clicking talk button turns mic on/off
	if (gSavedSettings.getBOOL("PushToTalkToggle"))
	{
		LLVoiceClient::getInstance()->toggleUserPTTState();
	}
}

void LLVoiceRemoteCtrl::onBtnTalkHeld()
{
	// when not in toggle mode, holding down talk button turns on mic
	if (!gSavedSettings.getBOOL("PushToTalkToggle"))
	{
		LLVoiceClient::getInstance()->setUserPTTState(true);
	}
}

void LLVoiceRemoteCtrl::onBtnTalkReleased()
{
	// when not in toggle mode, releasing talk button turns off mic
	if (!gSavedSettings.getBOOL("PushToTalkToggle"))
	{
		LLVoiceClient::getInstance()->setUserPTTState(false);
	}
}

void LLVoiceRemoteCtrl::onBtnLock(void* user_data)
{
	LLVoiceRemoteCtrl* remotep = (LLVoiceRemoteCtrl*)user_data;

	gSavedSettings.setBOOL("PTTCurrentlyEnabled", !remotep->mTalkLockBtn->getToggleState());
}

//static
void LLVoiceRemoteCtrl::onClickPopupBtn(void* user_data)
{
	LLVoiceRemoteCtrl* remotep = (LLVoiceRemoteCtrl*)user_data;

	remotep->deleteAllChildren();
	if (gSavedSettings.getBOOL("ShowVoiceChannelPopup"))
	{
		LLUICtrlFactory::getInstance()->buildPanel(remotep, "panel_voice_remote_expanded.xml");
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildPanel(remotep, "panel_voice_remote.xml");
	}
	gOverlayBar->layoutButtons();
}

//static
void LLVoiceRemoteCtrl::onClickEndCall()
{
	LLVoiceChannel* current_channel = LLVoiceChannel::getCurrentVoiceChannel();

	if (current_channel && current_channel != LLVoiceChannelProximal::getInstance())
	{
		current_channel->deactivate();
	}
}


void LLVoiceRemoteCtrl::onClickSpeakers()
{
	LLFloaterActiveSpeakers::toggleInstance(LLSD());
}

//static 
void LLVoiceRemoteCtrl::onClickVoiceChannel()
{
	if (LLFloater* floater = LLFloaterChatterBox::getInstance()->getCurrentVoiceFloater())
	{
		if (LLMultiFloater* mf = floater->getHost()) // Docked
			mf->showFloater(floater);
		else // Probably only local chat
			floater->open();
	}
	else if (LLVoiceChannel* chan = LLVoiceChannel::getCurrentVoiceChannel()) // Detached chat floater
	{
		if (LLFloaterIMPanel* floater = gIMMgr->findFloaterBySession(chan->getSessionID()))
			floater->open();
	}
}
