/** 
 * @file llprefsvoice.cpp
 * @author Richard Nelson
 * @brief Voice chat preferences panel
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llprefsvoice.h"

#include "llcheckboxctrl.h"
#include "llfloatertos.h"
#include "llfocusmgr.h"
#include "llkeyboard.h"
#include "llmodaldialog.h"
#include "llpanelvoicedevicesettings.h"
#include "lltrans.h"
#include "lluictrlfactory.h"


class LLVoiceSetKeyDialog : public LLModalDialog
{
public:
	LLVoiceSetKeyDialog(LLPrefsVoice* parent);
	~LLVoiceSetKeyDialog();

	BOOL handleKeyHere(KEY key, MASK mask);

	static void start(LLPrefsVoice* p) { (new LLVoiceSetKeyDialog(p))->startModal(); }

private:
	LLPrefsVoice* mParent;
};

LLVoiceSetKeyDialog::LLVoiceSetKeyDialog(LLPrefsVoice* parent)
	: LLModalDialog(LLStringUtil::null, 240, 100), mParent(parent)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_select_key.xml");
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("Cancel"))
	{
		ctrl->setCommitCallback(boost::bind(&LLModalDialog::close, this, false));
		ctrl->setFocus(true);
	}

	gFocusMgr.setKeystrokesOnly(TRUE);
}

LLVoiceSetKeyDialog::~LLVoiceSetKeyDialog()
{
}

BOOL LLVoiceSetKeyDialog::handleKeyHere(KEY key, MASK mask)
{
	BOOL result = TRUE;
	
	if (key == 'Q' && mask == MASK_CONTROL)
	{
		result = FALSE;
	}
	else
	{
		mParent->setKey(key);
	}

	close();
	return result;
}

namespace
{
	void* createDevicePanel(void*)
	{
		return new LLPanelVoiceDeviceSettings;
	}
}

//--------------------------------------------------------------------
//LLPrefsVoice
LLPrefsVoice::LLPrefsVoice()
	:	LLPanel(std::string("Voice Chat Panel"))
{
	mFactoryMap["device_settings_panel"] = LLCallbackMap(createDevicePanel, NULL);

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_voice.xml", &getFactoryMap());
}

LLPrefsVoice::~LLPrefsVoice()
{
}

BOOL LLPrefsVoice::postBuild()
{
	getChild<LLUICtrl>("enable_voice_check")->setCommitCallback(boost::bind(&LLPrefsVoice::onCommitEnableVoiceChat, this, _2));
	getChild<LLUICtrl>("set_voice_hotkey_button")->setCommitCallback(boost::bind(LLVoiceSetKeyDialog::start, this));
	getChild<LLUICtrl>("set_voice_middlemouse_button")->setCommitCallback(boost::bind(&LLView::setValue, getChildView("modifier_combo"), "MiddleMouse"));

	getChildView("enable_voice_check")->setValue(!gSavedSettings.getBOOL("CmdLineDisableVoice") && gSavedSettings.getBOOL("EnableVoiceChat"));

	if (LLCheckBoxCtrl* check = findChild<LLCheckBoxCtrl>("enable_multivoice_check"))
	{
		check->setValue(gSavedSettings.getBOOL("VoiceMultiInstance"));
		check->setLabel(getString("multivoice_label", LLTrans::getDefaultArgs()));
	}

	childSetValue("modifier_combo", gSavedSettings.getString("PushToTalkButton"));
	childSetValue("voice_call_friends_only_check", gSavedSettings.getBOOL("VoiceCallsFriendsOnly"));
	childSetValue("auto_disengage_mic_check", gSavedSettings.getBOOL("AutoDisengageMic"));
	childSetValue("push_to_talk_toggle_check", gSavedSettings.getBOOL("PushToTalkToggle"));
	childSetValue("ear_location", gSavedSettings.getS32("VoiceEarLocation"));
	childSetValue("enable_lip_sync_check", gSavedSettings.getBOOL("LipSyncEnabled"));

	return TRUE;
}

void LLPrefsVoice::apply()
{
	gSavedSettings.setString("PushToTalkButton", childGetValue("modifier_combo"));
	gSavedSettings.setBOOL("VoiceCallsFriendsOnly", childGetValue("voice_call_friends_only_check"));
	gSavedSettings.setBOOL("AutoDisengageMic", childGetValue("auto_disengage_mic_check"));
	gSavedSettings.setBOOL("PushToTalkToggle", childGetValue("push_to_talk_toggle_check"));
	gSavedSettings.setS32("VoiceEarLocation", childGetValue("ear_location"));
	gSavedSettings.setBOOL("LipSyncEnabled", childGetValue("enable_lip_sync_check"));
	gSavedSettings.setBOOL("VoiceMultiInstance", childGetValue("enable_multivoice_check"));
	
	if (LLPanelVoiceDeviceSettings* voice_device_settings = findChild<LLPanelVoiceDeviceSettings>("device_settings_panel"))
	{
		voice_device_settings->apply();
	}

	bool enable_voice = childGetValue("enable_voice_check");
	if (enable_voice && !gSavedSettings.getBOOL("VivoxLicenseAccepted"))
	{
		// This window enables voice chat if license is accepted
		auto inst = LLFloaterTOS::show(LLFloaterTOS::TOS_VOICE);
		inst->open();
		inst->center();
	}
	else
	{
		gSavedSettings.setBOOL("EnableVoiceChat", enable_voice);
	}
}

void LLPrefsVoice::cancel()
{
	if (LLPanelVoiceDeviceSettings* voice_device_settings = findChild<LLPanelVoiceDeviceSettings>("device_settings_panel"))
	{
		voice_device_settings->cancel();
	}
}

void LLPrefsVoice::setKey(KEY key)
{
	getChildView("modifier_combo")->setValue(LLKeyboard::stringFromKey(key));
}

void LLPrefsVoice::onCommitEnableVoiceChat(const LLSD& value)
{
	bool enable = value.asBoolean();

	getChildView("modifier_combo")->setEnabled(enable);
	getChildView("push_to_talk_label")->setEnabled(enable);
	getChildView("voice_call_friends_only_check")->setEnabled(enable);
	getChildView("auto_disengage_mic_check")->setEnabled(enable);
	getChildView("push_to_talk_toggle_check")->setEnabled(enable);
	getChildView("ear_location")->setEnabled(enable);
	getChildView("enable_lip_sync_check")->setEnabled(enable);
	getChildView("set_voice_hotkey_button")->setEnabled(enable);
	getChildView("set_voice_middlemouse_button")->setEnabled(enable);
	getChildView("device_settings_btn")->setEnabled(enable);
	getChildView("device_settings_panel")->setEnabled(enable);
}

