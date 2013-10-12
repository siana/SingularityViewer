/** 
 * @file llfloaterwindlight.h
 * @brief LLFloaterWindLight class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

/*
 * Menu for adjusting the atmospheric settings of the world
 */

#ifndef LL_LLFLOATERWINDLIGHT_H
#define LL_LLFLOATERWINDLIGHT_H

#include "llfloater.h"

struct WLColorControl;
struct WLFloatControl;

class LLComboBox;

/// Menuing system for all of windlight's functionality
class LLFloaterWindLight : public LLFloater
{
public:

	LLFloaterWindLight();
	virtual ~LLFloaterWindLight();
	
	// initialize all
	void initCallbacks(void);

	// one and one instance only
	static LLFloaterWindLight* instance();

	// help button stuff
	static void onClickHelp(void* data);
	void initHelpBtn(const std::string& name, const std::string& xml_alert);

	bool newPromptCallback(const LLSD& notification, const LLSD& response);

	void setColorSwatch(const std::string& name, const WLColorControl& from_ctrl, F32 k);
	
	// general purpose callbacks for dealing with color controllers
	void onColorControlRMoved(LLUICtrl* ctrl, void* userdata);
	void onColorControlGMoved(LLUICtrl* ctrl, void* userdata);
	void onColorControlBMoved(LLUICtrl* ctrl, void* userdata);
	void onColorControlIMoved(LLUICtrl* ctrl, void* userdata);
	void onFloatControlMoved(LLUICtrl* ctrl, void* userdata);

	// lighting callbacks for glow
	void onGlowRMoved(LLUICtrl* ctrl, void* userdata);
	void onGlowBMoved(LLUICtrl* ctrl, void* userdata);

	// lighting callbacks for sun
	void onSunMoved(LLUICtrl* ctrl, void* userdata);

	// for handling when the star slider is moved to adjust the alpha
	void onStarAlphaMoved(LLUICtrl* ctrl);

	// when user hits the load preset button
	void onNewPreset();

	// when user hits the save to file button
	void onSavePreset(LLUICtrl* ctrl);
	
	// prompts a user when overwriting a preset notecard
	bool saveNotecardCallback(const LLSD& notification, const LLSD& response);

	// prompts a user when overwriting a preset
	bool saveAlertCallback(const LLSD& notification, const LLSD& response);

	// when user hits the save preset button
	void onDeletePreset();

	// prompts a user when overwriting a preset
	bool deleteAlertCallback(const LLSD& notification, const LLSD& response);

	// what to do when you change the preset name
	void onChangePresetName(LLUICtrl* ctrl);

	// handle cloud scrolling
	void onCloudScrollXMoved(const LLSD& value);
	void onCloudScrollYMoved(const LLSD& value);
	void onCloudScrollXToggled(const LLSD& value);
	void onCloudScrollYToggled(const LLSD& value);

	//// menu management

	// show off our menu
	static void show();

	// return if the menu exists or not
	static bool isOpen();

	// stuff to do on exit
	virtual void onClose(bool app_quitting);

	// sync up sliders with parameters
	void syncMenu();


	static void selectTab(std::string tab_name);

private:
	// one instance on the inside
	static LLFloaterWindLight* sWindLight;

	static std::set<std::string> sDefaultPresets;

	void onClickNext();
	void onClickPrev();

	void populateSkyPresetsList();

	LLComboBox*		mSkyPresetCombo;
};


#endif
