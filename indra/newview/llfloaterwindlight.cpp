/** 
 * @file llfloaterwindlight.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterwindlight.h"

// libs
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llmultisliderctrl.h"
#include "llnotificationsutil.h"
#include "llsliderctrl.h"
#include "llfloaterdaycycle.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"

// newview
#include "llagent.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"

LLFloaterWindLight* LLFloaterWindLight::sWindLight = NULL;

std::set<std::string> LLFloaterWindLight::sDefaultPresets;

static const F32 WL_SUN_AMBIENT_SLIDER_SCALE = 3.0f;
static const F32 WL_BLUE_HORIZON_DENSITY_SCALE = 2.0f;
static const F32 WL_CLOUD_SLIDER_SCALE = 1.0f;

LLFloaterWindLight::LLFloaterWindLight() : LLFloater(std::string("windlight floater"))
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_windlight_options.xml");
	
	// add the combo boxes
	mSkyPresetCombo = getChild<LLComboBox>("WLPresetsCombo");
	if (mSkyPresetCombo)
	{
		populateSkyPresetsList();
		mSkyPresetCombo->setCommitCallback(boost::bind(&LLFloaterWindLight::onChangePresetName, this, _1));
	}

	// add the list of presets
	// no editing or deleting of the blank string
	sDefaultPresets.insert("");
	boost_tokenizer tokens(getString("WLDefaultSkyNames"), boost::char_separator<char>(":"));
	for (boost_tokenizer::iterator token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
	{
		std::string tok(*token_iter);
		sDefaultPresets.insert(tok);
	}

	// load it up
	initCallbacks();
}

LLFloaterWindLight::~LLFloaterWindLight()
{
}

void LLFloaterWindLight::initCallbacks(void)
{

	// help buttons
	initHelpBtn("WLBlueHorizonHelp", "HelpBlueHorizon");
	initHelpBtn("WLHazeHorizonHelp", "HelpHazeHorizon");
	initHelpBtn("WLBlueDensityHelp", "HelpBlueDensity");
	initHelpBtn("WLHazeDensityHelp", "HelpHazeDensity");

	initHelpBtn("WLDensityMultHelp", "HelpDensityMult");
	initHelpBtn("WLDistanceMultHelp", "HelpDistanceMult");
	initHelpBtn("WLMaxAltitudeHelp", "HelpMaxAltitude");

	initHelpBtn("WLSunlightColorHelp", "HelpSunlightColor");
	initHelpBtn("WLAmbientHelp", "HelpSunAmbient");
	initHelpBtn("WLSunGlowHelp", "HelpSunGlow");
	initHelpBtn("WLTimeOfDayHelp", "HelpTimeOfDay");
	initHelpBtn("WLEastAngleHelp", "HelpEastAngle");

	initHelpBtn("WLSceneGammaHelp", "HelpSceneGamma");
	initHelpBtn("WLStarBrightnessHelp", "HelpStarBrightness");

	initHelpBtn("WLCloudColorHelp", "HelpCloudColor");
	initHelpBtn("WLCloudDetailHelp", "HelpCloudDetail");
	initHelpBtn("WLCloudDensityHelp", "HelpCloudDensity");
	initHelpBtn("WLCloudCoverageHelp", "HelpCloudCoverage");

	initHelpBtn("WLCloudScaleHelp", "HelpCloudScale");
	initHelpBtn("WLCloudScrollXHelp", "HelpCloudScrollX");
	initHelpBtn("WLCloudScrollYHelp", "HelpCloudScrollY");

	initHelpBtn("WLClassicCloudsHelp", "HelpClassicClouds");

	LLWLParamManager& param_mgr = LLWLParamManager::instance();

	// blue horizon
	getChild<LLUICtrl>("WLBlueHorizonR")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlRMoved, this, _1, &param_mgr.mBlueHorizon));
	getChild<LLUICtrl>("WLBlueHorizonG")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlGMoved, this, _1, &param_mgr.mBlueHorizon));
	getChild<LLUICtrl>("WLBlueHorizonB")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlBMoved, this, _1, &param_mgr.mBlueHorizon));
	getChild<LLUICtrl>("WLBlueHorizonI")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlIMoved, this, _1, &param_mgr.mBlueHorizon));

	// haze density, horizon, mult, and altitude
	getChild<LLUICtrl>("WLHazeDensity")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mHazeDensity));
	getChild<LLUICtrl>("WLHazeHorizon")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mHazeHorizon));
	getChild<LLUICtrl>("WLDensityMult")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mDensityMult));
	getChild<LLUICtrl>("WLMaxAltitude")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mMaxAlt));

	// blue density
	getChild<LLUICtrl>("WLBlueDensityR")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlRMoved, this, _1, &param_mgr.mBlueDensity));
	getChild<LLUICtrl>("WLBlueDensityG")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlGMoved, this, _1, &param_mgr.mBlueDensity));
	getChild<LLUICtrl>("WLBlueDensityB")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlBMoved, this, _1, &param_mgr.mBlueDensity));
	getChild<LLUICtrl>("WLBlueDensityI")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlIMoved, this, _1, &param_mgr.mBlueDensity));

	// Lighting
	
	// sunlight
	getChild<LLUICtrl>("WLSunlightR")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlRMoved, this, _1, &param_mgr.mSunlight));
	getChild<LLUICtrl>("WLSunlightG")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlGMoved, this, _1, &param_mgr.mSunlight));
	getChild<LLUICtrl>("WLSunlightB")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlBMoved, this, _1, &param_mgr.mSunlight));
	getChild<LLUICtrl>("WLSunlightI")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlIMoved, this, _1, &param_mgr.mSunlight));

	// glow
	getChild<LLUICtrl>("WLGlowR")->setCommitCallback(boost::bind(&LLFloaterWindLight::onGlowRMoved, this, _1, &param_mgr.mGlow));
	getChild<LLUICtrl>("WLGlowB")->setCommitCallback(boost::bind(&LLFloaterWindLight::onGlowBMoved, this, _1, &param_mgr.mGlow));

	// ambient
	getChild<LLUICtrl>("WLAmbientR")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlRMoved, this, _1, &param_mgr.mAmbient));
	getChild<LLUICtrl>("WLAmbientG")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlGMoved, this, _1, &param_mgr.mAmbient));
	getChild<LLUICtrl>("WLAmbientB")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlBMoved, this, _1, &param_mgr.mAmbient));
	getChild<LLUICtrl>("WLAmbientI")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlIMoved, this, _1, &param_mgr.mAmbient));

	// time of day
	getChild<LLUICtrl>("WLSunAngle")->setCommitCallback(boost::bind(&LLFloaterWindLight::onSunMoved, this, _1, &param_mgr.mLightnorm));
	getChild<LLUICtrl>("WLEastAngle")->setCommitCallback(boost::bind(&LLFloaterWindLight::onSunMoved, this, _1, &param_mgr.mLightnorm));

	// Clouds

	// Cloud Color
	getChild<LLUICtrl>("WLCloudColorR")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlRMoved, this, _1, &param_mgr.mCloudColor));
	getChild<LLUICtrl>("WLCloudColorG")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlGMoved, this, _1, &param_mgr.mCloudColor));
	getChild<LLUICtrl>("WLCloudColorB")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlBMoved, this, _1, &param_mgr.mCloudColor));
	getChild<LLUICtrl>("WLCloudColorI")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlIMoved, this, _1, &param_mgr.mCloudColor));

	// Cloud
	getChild<LLUICtrl>("WLCloudX")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlRMoved, this, _1, &param_mgr.mCloudMain));
	getChild<LLUICtrl>("WLCloudY")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlGMoved, this, _1, &param_mgr.mCloudMain));
	getChild<LLUICtrl>("WLCloudDensity")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlBMoved, this, _1, &param_mgr.mCloudMain));

	// Cloud Detail
	getChild<LLUICtrl>("WLCloudDetailX")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlRMoved, this, _1, &param_mgr.mCloudDetail));
	getChild<LLUICtrl>("WLCloudDetailY")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlGMoved, this, _1, &param_mgr.mCloudDetail));
	getChild<LLUICtrl>("WLCloudDetailDensity")->setCommitCallback(boost::bind(&LLFloaterWindLight::onColorControlBMoved, this, _1, &param_mgr.mCloudDetail));

	// Cloud extras
	getChild<LLUICtrl>("WLCloudCoverage")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mCloudCoverage));
	getChild<LLUICtrl>("WLCloudScale")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mCloudScale));
	getChild<LLUICtrl>("WLCloudLockX")->setCommitCallback(boost::bind(&LLFloaterWindLight::onCloudScrollXToggled, this, _2));
	getChild<LLUICtrl>("WLCloudLockY")->setCommitCallback(boost::bind(&LLFloaterWindLight::onCloudScrollYToggled, this, _2));
	getChild<LLUICtrl>("WLCloudScrollX")->setCommitCallback(boost::bind(&LLFloaterWindLight::onCloudScrollXMoved, this, _2));
	getChild<LLUICtrl>("WLCloudScrollY")->setCommitCallback(boost::bind(&LLFloaterWindLight::onCloudScrollYMoved, this, _2));
	getChild<LLUICtrl>("WLDistanceMult")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mDistanceMult));

	// WL Top
	getChild<LLUICtrl>("WLDayCycleMenuButton")->setCommitCallback(boost::bind(LLFloaterDayCycle::show));

	// Load/save
	//getChild<LLUICtrl>("WLLoadPreset")->setCommitCallback(boost::bind(&LLFloaterWindLight::onLoadPreset, this, mSkyPresetCombo));
	getChild<LLUICtrl>("WLNewPreset")->setCommitCallback(boost::bind(&LLFloaterWindLight::onNewPreset, this));
	getChild<LLUICtrl>("WLDeletePreset")->setCommitCallback(boost::bind(&LLFloaterWindLight::onDeletePreset, this));
	getChild<LLUICtrl>("WLSavePreset")->setCommitCallback(boost::bind(&LLFloaterWindLight::onSavePreset, this, _1));


	// Dome
	getChild<LLUICtrl>("WLGamma")->setCommitCallback(boost::bind(&LLFloaterWindLight::onFloatControlMoved, this, _1, &param_mgr.mWLGamma));
	getChild<LLUICtrl>("WLStarAlpha")->setCommitCallback(boost::bind(&LLFloaterWindLight::onStarAlphaMoved, this, _1));

	// next/prev buttons
	//getChild<LLUICtrl>("next")->setCommitCallback(boost::bind(&LLFloaterWindLight::onClickNext, this));
	//getChild<LLUICtrl>("prev")->setCommitCallback(boost::bind(&LLFloaterWindLight::onClickPrev, this));
}

void LLFloaterWindLight::onClickHelp(void* data)
{
	LLFloaterWindLight* self = LLFloaterWindLight::instance();

	const std::string xml_alert = *(std::string*)data;
	self->addContextualNotification(xml_alert);
}

void LLFloaterWindLight::initHelpBtn(const std::string& name, const std::string& xml_alert)
{
	childSetAction(name, onClickHelp, new std::string(xml_alert));
}

bool LLFloaterWindLight::newPromptCallback(const LLSD& notification, const LLSD& response)
{
	std::string text = response["message"].asString();
	if(text.empty())
	{
		return false;
	}

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(option == 0)
	{
		LLFloaterDayCycle* sDayCycle = NULL;
		LLComboBox* keyCombo = NULL;
		if(LLFloaterDayCycle::isOpen())
		{
			sDayCycle = LLFloaterDayCycle::instance();
			keyCombo = sDayCycle->getChild<LLComboBox>("WLKeyPresets");
		}

		// add the current parameters to the list
		// see if it's there first

		const LLWLParamKey key(text, LLEnvKey::SCOPE_LOCAL);

		// if not there, add a new one
		if(!LLWLParamManager::getInstance()->hasParamSet(key))
		{
			LLWLParamManager::getInstance()->addParamSet(key, LLWLParamManager::getInstance()->mCurParams);
			mSkyPresetCombo->add(text);
			mSkyPresetCombo->sortByName();

			// add a blank to the bottom
			mSkyPresetCombo->selectFirstItem();
			if(mSkyPresetCombo->getSimple().empty())
			{
				mSkyPresetCombo->remove(0);
			}
			mSkyPresetCombo->add(LLStringUtil::null);

			mSkyPresetCombo->selectByValue(text);

			if(LLFloaterDayCycle::isOpen()) 
			{
				keyCombo->add(text);
				keyCombo->sortByName();
			}
			const LLWLParamKey key(text, LLEnvKey::SCOPE_LOCAL);
			LLWLParamManager::getInstance()->savePreset(key);

			LLEnvManagerNew::instance().setUseSkyPreset(text);

		// otherwise, send a message to the user
		}
		else
		{
			LLNotificationsUtil::add("ExistsSkyPresetAlert");
		}
	}
	return false;
}

void LLFloaterWindLight::syncMenu()
{
	bool err;

	LLWLParamManager * param_mgr = LLWLParamManager::getInstance();

	LLWLParamSet& cur_params = param_mgr->mCurParams;
	//std::map<std::string, LLVector4> & currentParams = param_mgr->mCurParams.mParamValues;

// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	// Fixes LL "bug" (preset name isn't kept synchronized)
	if (mSkyPresetCombo->getSelectedItemLabel() != LLEnvManagerNew::instance().getSkyPresetName())
	{
		mSkyPresetCombo->selectByValue(LLEnvManagerNew::instance().getSkyPresetName());
	}
// [/RLVa:KB]

	// blue horizon
	param_mgr->mBlueHorizon = cur_params.getVector(param_mgr->mBlueHorizon.mName, err);
	//setColorSwatch("WLBlueHorizon", param_mgr->mBlueHorizon, WL_BLUE_HORIZON_DENSITY_SCALE);

	// haze density, horizon, mult, and altitude
	param_mgr->mHazeDensity = cur_params.getFloat(param_mgr->mHazeDensity.mName, err);
	childSetValue("WLHazeDensity", (F32) param_mgr->mHazeDensity);
	param_mgr->mHazeHorizon = cur_params.getFloat(param_mgr->mHazeHorizon.mName, err);
	childSetValue("WLHazeHorizon", (F32) param_mgr->mHazeHorizon);
	param_mgr->mDensityMult = cur_params.getFloat(param_mgr->mDensityMult.mName, err);
	childSetValue("WLDensityMult", ((F32) param_mgr->mDensityMult) * param_mgr->mDensityMult.mult);
	param_mgr->mMaxAlt = cur_params.getFloat(param_mgr->mMaxAlt.mName, err);
	childSetValue("WLMaxAltitude", (F32) param_mgr->mMaxAlt);

	// blue density
	param_mgr->mBlueDensity = cur_params.getVector(param_mgr->mBlueDensity.mName, err);
	//setColorSwatch("WLBlueDensity", param_mgr->mBlueDensity, WL_BLUE_HORIZON_DENSITY_SCALE);

	// Lighting

	// sunlight
	param_mgr->mSunlight = cur_params.getVector(param_mgr->mSunlight.mName, err);
	//setColorSwatch("WLSunlight", param_mgr->mSunlight, WL_SUN_AMBIENT_SLIDER_SCALE);

	// glow
	param_mgr->mGlow = cur_params.getVector(param_mgr->mGlow.mName, err);
	childSetValue("WLGlowR", 2 - param_mgr->mGlow.r / 20.0f);
	childSetValue("WLGlowB", -param_mgr->mGlow.b / 5.0f);
		
	// ambient
	param_mgr->mAmbient = cur_params.getVector(param_mgr->mAmbient.mName, err);
	//setColorSwatch("WLAmbient", param_mgr->mAmbient, WL_SUN_AMBIENT_SLIDER_SCALE);

	childSetValue("WLSunAngle", param_mgr->mCurParams.getFloat("sun_angle",err) / F_TWO_PI);
	childSetValue("WLEastAngle", param_mgr->mCurParams.getFloat("east_angle",err) / F_TWO_PI);

	// Clouds

	// Cloud Color
	param_mgr->mCloudColor = cur_params.getVector(param_mgr->mCloudColor.mName, err);
	//setColorSwatch("WLCloudColor", param_mgr->mCloudColor, WL_CLOUD_SLIDER_SCALE);

	// Cloud
	param_mgr->mCloudMain = cur_params.getVector(param_mgr->mCloudMain.mName, err);
	childSetValue("WLCloudX", param_mgr->mCloudMain.r);
	childSetValue("WLCloudY", param_mgr->mCloudMain.g);
	childSetValue("WLCloudDensity", param_mgr->mCloudMain.b);

	// Cloud Detail
	param_mgr->mCloudDetail = cur_params.getVector(param_mgr->mCloudDetail.mName, err);
	childSetValue("WLCloudDetailX", param_mgr->mCloudDetail.r);
	childSetValue("WLCloudDetailY", param_mgr->mCloudDetail.g);
	childSetValue("WLCloudDetailDensity", param_mgr->mCloudDetail.b);

	// Cloud extras
	param_mgr->mCloudCoverage = cur_params.getFloat(param_mgr->mCloudCoverage.mName, err);
	param_mgr->mCloudScale = cur_params.getFloat(param_mgr->mCloudScale.mName, err);
	childSetValue("WLCloudCoverage", (F32) param_mgr->mCloudCoverage);
	childSetValue("WLCloudScale", (F32) param_mgr->mCloudScale);

	// cloud scrolling
	bool lockX = !param_mgr->mCurParams.getEnableCloudScrollX();
	bool lockY = !param_mgr->mCurParams.getEnableCloudScrollY();
	childSetValue("WLCloudLockX", lockX);
	childSetValue("WLCloudLockY", lockY);
	
	// disable if locked, enable if not
	if (lockX)
	{
		childDisable("WLCloudScrollX");
	}
	else
	{
		childEnable("WLCloudScrollX");
	}
	if (lockY)
	{
		childDisable("WLCloudScrollY");
	}
	else
	{
		childEnable("WLCloudScrollY");
	}

	// *HACK cloud scrolling is off my an additive of 10
	childSetValue("WLCloudScrollX", param_mgr->mCurParams.getCloudScrollX() - 10.0f);
	childSetValue("WLCloudScrollY", param_mgr->mCurParams.getCloudScrollY() - 10.0f);

	param_mgr->mDistanceMult = cur_params.getFloat(param_mgr->mDistanceMult.mName, err);
	childSetValue("WLDistanceMult", (F32) param_mgr->mDistanceMult);

	// Tweak extras

	param_mgr->mWLGamma = cur_params.getFloat(param_mgr->mWLGamma.mName, err);
	childSetValue("WLGamma", (F32) param_mgr->mWLGamma);

	childSetValue("WLStarAlpha", param_mgr->mCurParams.getStarBrightness());
}

void LLFloaterWindLight::setColorSwatch(const std::string& name, const WLColorControl& from_ctrl, F32 k)
{
	std::string child_name(name);
	const size_t end = child_name.length();
	LLVector4 color_vec = from_ctrl;
	color_vec/=k;

	child_name.push_back('R');
	childSetValue(name.data(), color_vec[0]);
	child_name.replace(end,1,1,'G');
	childSetValue(child_name, color_vec[1]);
	child_name.replace(end,1,1,'B');
	childSetValue(child_name, color_vec[2]);
	child_name.replace(end,1,1,'I');
	childSetValue(child_name, llmax(color_vec.mV[0], color_vec.mV[1], color_vec.mV[2]));
}

// static
LLFloaterWindLight* LLFloaterWindLight::instance()
{
	if (!sWindLight)
	{
		sWindLight = new LLFloaterWindLight();
		sWindLight->open();
		sWindLight->setFocus(TRUE);
	}
	return sWindLight;
}
void LLFloaterWindLight::show()
{
	if (!sWindLight)
	{
		LLFloaterWindLight* windLight = instance();
		windLight->syncMenu();

		// comment in if you want the menu to rebuild each time
		//LLUICtrlFactory::getInstance()->buildFloater(windLight, "floater_windlight_options.xml");
		//windLight->initCallbacks();
	}
	else
	{
		if (sWindLight->getVisible())
		{
			sWindLight->close();
		}
		else
		{
			sWindLight->open();
		}
	}
}

bool LLFloaterWindLight::isOpen()
{
	if (sWindLight != NULL)
	{
		return sWindLight->getVisible();
	}
	return false;
}

// virtual
void LLFloaterWindLight::onClose(bool app_quitting)
{
	if (sWindLight)
	{
		sWindLight->setVisible(FALSE);
	}
}

// color control callbacks
void LLFloaterWindLight::onColorControlRMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);
	WLColorControl* color_ctrl = static_cast<WLColorControl *>(userdata);

	color_ctrl->r = sldr_ctrl->getValueF32();
	if (color_ctrl->isSunOrAmbientColor)
	{
		color_ctrl->r *= WL_SUN_AMBIENT_SLIDER_SCALE;
	}
	if (color_ctrl->isBlueHorizonOrDensity)
	{
		color_ctrl->r *= WL_BLUE_HORIZON_DENSITY_SCALE;
	}

	// move i if it's the max
	if (color_ctrl->r >= color_ctrl->g && color_ctrl->r >= color_ctrl->b && color_ctrl->hasSliderName)
	{
		color_ctrl->i = color_ctrl->r;
		std::string name = color_ctrl->mSliderName;
		name.append("I");

		if (color_ctrl->isSunOrAmbientColor)
		{
			childSetValue(name, color_ctrl->r / WL_SUN_AMBIENT_SLIDER_SCALE);
		}
		else if	(color_ctrl->isBlueHorizonOrDensity)
		{
			childSetValue(name, color_ctrl->r / WL_BLUE_HORIZON_DENSITY_SCALE);
		}
		else
		{
			childSetValue(name, color_ctrl->r);
		}
	}

	color_ctrl->update(LLWLParamManager::getInstance()->mCurParams);

	LLWLParamManager::getInstance()->propagateParameters();
}

void LLFloaterWindLight::onColorControlGMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);
	WLColorControl* color_ctrl = static_cast<WLColorControl *>(userdata);

	color_ctrl->g = sldr_ctrl->getValueF32();
	if (color_ctrl->isSunOrAmbientColor)
	{
		color_ctrl->g *= WL_SUN_AMBIENT_SLIDER_SCALE;
	}
	if (color_ctrl->isBlueHorizonOrDensity)
	{
		color_ctrl->g *= WL_BLUE_HORIZON_DENSITY_SCALE;
	}

	// move i if it's the max
	if (color_ctrl->g >= color_ctrl->r && color_ctrl->g >= color_ctrl->b && color_ctrl->hasSliderName)
	{
		color_ctrl->i = color_ctrl->g;
		std::string name = color_ctrl->mSliderName;
		name.append("I");

		if (color_ctrl->isSunOrAmbientColor)
		{
			childSetValue(name, color_ctrl->g / WL_SUN_AMBIENT_SLIDER_SCALE);
		}
		else if (color_ctrl->isBlueHorizonOrDensity)
		{
			childSetValue(name, color_ctrl->g / WL_BLUE_HORIZON_DENSITY_SCALE);
		}
		else
		{
			childSetValue(name, color_ctrl->g);
		}
	}

	color_ctrl->update(LLWLParamManager::getInstance()->mCurParams);

	LLWLParamManager::getInstance()->propagateParameters();
}

void LLFloaterWindLight::onColorControlBMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);
	WLColorControl* color_ctrl = static_cast<WLColorControl *>(userdata);

	color_ctrl->b = sldr_ctrl->getValueF32();
	if (color_ctrl->isSunOrAmbientColor)
	{
		color_ctrl->b *= WL_SUN_AMBIENT_SLIDER_SCALE;
	}
	if (color_ctrl->isBlueHorizonOrDensity)
	{
		color_ctrl->b *= WL_BLUE_HORIZON_DENSITY_SCALE;
	}

	// move i if it's the max
	if (color_ctrl->b >= color_ctrl->r && color_ctrl->b >= color_ctrl->g && color_ctrl->hasSliderName)
	{
		color_ctrl->i = color_ctrl->b;
		std::string name = color_ctrl->mSliderName;
		name.append("I");

		if (color_ctrl->isSunOrAmbientColor)
		{
			childSetValue(name, color_ctrl->b / WL_SUN_AMBIENT_SLIDER_SCALE);
		}
		else if (color_ctrl->isBlueHorizonOrDensity)
		{
			childSetValue(name, color_ctrl->b / WL_BLUE_HORIZON_DENSITY_SCALE);
		}
		else
		{
			childSetValue(name, color_ctrl->b);
		}
	}

	color_ctrl->update(LLWLParamManager::getInstance()->mCurParams);

	LLWLParamManager::getInstance()->propagateParameters();
}

void LLFloaterWindLight::onColorControlIMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);
	WLColorControl* color_ctrl = static_cast<WLColorControl *>(userdata);

	color_ctrl->i = sldr_ctrl->getValueF32();
	
	// only for sliders where we pass a name
	if(color_ctrl->hasSliderName)
	{
		// set it to the top
		F32 maxVal = llmax(llmax(color_ctrl->r, color_ctrl->g), color_ctrl->b);

		F32 scale = 1.f;
		if(color_ctrl->isSunOrAmbientColor)
			scale = WL_SUN_AMBIENT_SLIDER_SCALE;
		else if(color_ctrl->isBlueHorizonOrDensity)
			scale = WL_BLUE_HORIZON_DENSITY_SCALE;

		F32 iVal = color_ctrl->i * scale;

		// handle if at 0
		if(iVal == 0)
		{
			color_ctrl->r = 0;
			color_ctrl->g = 0;
			color_ctrl->b = 0;
		
		// if all at the start
		// set them all to the intensity
		}
		else if (maxVal == 0)
		{
			color_ctrl->r = iVal;
			color_ctrl->g = iVal;
			color_ctrl->b = iVal;
		}
		else
		{
			// add delta amounts to each
			F32 delta = (iVal - maxVal) / maxVal;
			color_ctrl->r *= (1.0f + delta);
			color_ctrl->g *= (1.0f + delta);
			color_ctrl->b *= (1.0f + delta);
		}

		// set the sliders to the new vals
		std::string child_name(color_ctrl->mSliderName);
		const size_t end = child_name.length();
		child_name.push_back('R');
		childSetValue(child_name, color_ctrl->r/scale);
		child_name.replace(end,1,1,'G');
		childSetValue(child_name, color_ctrl->g/scale);
		child_name.replace(end,1,1,'B');
		childSetValue(child_name, color_ctrl->b/scale);
	}

	// now update the current parameters and send them to shaders
	color_ctrl->update(LLWLParamManager::getInstance()->mCurParams);
	LLWLParamManager::getInstance()->propagateParameters();
}

/// GLOW SPECIFIC CODE
void LLFloaterWindLight::onGlowRMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);
	WLColorControl* color_ctrl = static_cast<WLColorControl *>(userdata);

	// scaled by 20
	color_ctrl->r = (2 - sldr_ctrl->getValueF32()) * 20;

	color_ctrl->update(LLWLParamManager::getInstance()->mCurParams);
	LLWLParamManager::getInstance()->propagateParameters();
}

/// \NOTE that we want NEGATIVE (-) B
void LLFloaterWindLight::onGlowBMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);
	WLColorControl* color_ctrl = static_cast<WLColorControl *>(userdata);

	/// \NOTE that we want NEGATIVE (-) B and NOT by 20 as 20 is too big
	color_ctrl->b = -sldr_ctrl->getValueF32() * 5;

	color_ctrl->update(LLWLParamManager::getInstance()->mCurParams);
	LLWLParamManager::getInstance()->propagateParameters();
}

void LLFloaterWindLight::onFloatControlMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);
	WLFloatControl * floatControl = static_cast<WLFloatControl *>(userdata);

	floatControl->x = sldr_ctrl->getValueF32() / floatControl->mult;

	floatControl->update(LLWLParamManager::getInstance()->mCurParams);
	LLWLParamManager::getInstance()->propagateParameters();
}


// Lighting callbacks

// time of day
void LLFloaterWindLight::onSunMoved(LLUICtrl* ctrl, void* userdata)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sun_sldr = getChild<LLSliderCtrl>("WLSunAngle");
	LLSliderCtrl* east_sldr = getChild<LLSliderCtrl>("WLEastAngle");
	WLColorControl* color_ctrl = static_cast<WLColorControl *>(userdata);
	
	// get the two angles
	LLWLParamManager * param_mgr = LLWLParamManager::getInstance();

	param_mgr->mCurParams.setSunAngle(F_TWO_PI * sun_sldr->getValueF32());
	param_mgr->mCurParams.setEastAngle(F_TWO_PI * east_sldr->getValueF32());

	// set the sun vector
	color_ctrl->r = -sin(param_mgr->mCurParams.getEastAngle()) * 
		cos(param_mgr->mCurParams.getSunAngle());
	color_ctrl->g = sin(param_mgr->mCurParams.getSunAngle());
	color_ctrl->b = cos(param_mgr->mCurParams.getEastAngle()) * 
		cos(param_mgr->mCurParams.getSunAngle());
	color_ctrl->i = 1.f;

	color_ctrl->update(param_mgr->mCurParams);
	param_mgr->propagateParameters();
}

void LLFloaterWindLight::onStarAlphaMoved(LLUICtrl* ctrl)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	LLSliderCtrl* sldr_ctrl = static_cast<LLSliderCtrl*>(ctrl);

	LLWLParamManager::getInstance()->mCurParams.setStarBrightness(sldr_ctrl->getValueF32());
}

// Clouds
void LLFloaterWindLight::onCloudScrollXMoved(const LLSD& value)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	// *HACK  all cloud scrolling is off by an additive of 10.
	LLWLParamManager::getInstance()->mCurParams.setCloudScrollX(value.asFloat() + 10.0f);
}

void LLFloaterWindLight::onCloudScrollYMoved(const LLSD& value)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	// *HACK  all cloud scrolling is off by an additive of 10.
	LLWLParamManager::getInstance()->mCurParams.setCloudScrollY(value.asFloat() + 10.0f);
}

void LLFloaterWindLight::onCloudScrollXToggled(const LLSD& value)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	bool lock = value.asBoolean();
	LLWLParamManager::getInstance()->mCurParams.setEnableCloudScrollX(!lock);
	getChild<LLUICtrl>("WLCloudScrollX")->setEnabled(!lock);
}

void LLFloaterWindLight::onCloudScrollYToggled(const LLSD& value)
{
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	bool lock = value.asBoolean();
	LLWLParamManager::getInstance()->mCurParams.setEnableCloudScrollY(!lock);
	getChild<LLUICtrl>("WLCloudScrollY")->setEnabled(!lock);
}

void LLFloaterWindLight::onNewPreset()
{
	LLNotificationsUtil::add("NewSkyPreset", LLSD(), LLSD(), boost::bind(&LLFloaterWindLight::newPromptCallback, this, _1, _2));
}

void LLFloaterWindLight::onSavePreset(LLUICtrl* ctrl)
{
	// don't save the empty name
	if(mSkyPresetCombo->getSelectedItemLabel().empty())
	{
		return;
	}

	if (ctrl->getValue().asString() == "save_inventory_item")
	{

	}
	else
	{
		// check to see if it's a default and shouldn't be overwritten
		std::set<std::string>::iterator sIt = sDefaultPresets.find(mSkyPresetCombo->getSelectedItemLabel());
		if(sIt != sDefaultPresets.end() && !gSavedSettings.getBOOL("SkyEditPresets")) 
		{
			LLNotificationsUtil::add("WLNoEditDefault");
			return;
		}

		LLWLParamManager::getInstance()->mCurParams.mName = mSkyPresetCombo->getSelectedItemLabel();

		LLNotificationsUtil::add("WLSavePresetAlert", LLSD(), LLSD(), boost::bind(&LLFloaterWindLight::saveAlertCallback, this, _1, _2));
	}
}

bool LLFloaterWindLight::saveNotecardCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	// if they choose save, do it.  Otherwise, don't do anything
	if(option == 0) 
	{
		LLWLParamManager * param_mgr = LLWLParamManager::getInstance();
		param_mgr->setParamSet(param_mgr->mCurParams.mName, param_mgr->mCurParams);
		param_mgr->savePresetToNotecard(param_mgr->mCurParams.mName);
	}
	return false;
}

bool LLFloaterWindLight::saveAlertCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	// if they choose save, do it.  Otherwise, don't do anything
	if(option == 0)
	{
		LLWLParamManager * param_mgr = LLWLParamManager::getInstance();

		param_mgr->setParamSet(param_mgr->mCurParams.mName, param_mgr->mCurParams);
		
		// comment this back in to save to file
		const LLWLParamKey key(param_mgr->mCurParams.mName, LLEnvKey::SCOPE_LOCAL);
		param_mgr->savePreset(key);
	}
	return false;
}

void LLFloaterWindLight::onDeletePreset()
{
	if(mSkyPresetCombo->getSelectedValue().asString().empty())
	{
		return;
	}

	LLSD args;
	args["SKY"] = mSkyPresetCombo->getSelectedValue().asString();
	LLNotificationsUtil::add("WLDeletePresetAlert", args, LLSD(), boost::bind(&LLFloaterWindLight::deleteAlertCallback, this, _1, _2));
}

bool LLFloaterWindLight::deleteAlertCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	// if they choose delete, do it.  Otherwise, don't do anything
	if(option == 0)
	{
		LLFloaterDayCycle* day_cycle = NULL;
		LLComboBox* key_combo = NULL;

		if(LLFloaterDayCycle::isOpen())
		{
			day_cycle = LLFloaterDayCycle::instance();
			key_combo = day_cycle->getChild<LLComboBox>("WLKeyPresets");
		}

		std::string name(mSkyPresetCombo->getSelectedValue().asString());

		// check to see if it's a default and shouldn't be deleted
		std::set<std::string>::iterator sIt = sDefaultPresets.find(name);
		if(sIt != sDefaultPresets.end()) 
		{
			LLNotificationsUtil::add("WLNoEditDefault");
			return false;
		}

		LLWLParamManager::getInstance()->removeParamSet(name, true);
		
		// remove and choose another
		S32 new_index = mSkyPresetCombo->getCurrentIndex();

		mSkyPresetCombo->remove(name);
		if(key_combo != NULL) 
		{
			key_combo->remove(name);

			// remove from slider, as well
			day_cycle->deletePreset(name);
		}

		// pick the previously selected index after delete
		if(new_index > 0)
		{
			--new_index;
		}
		
		if(mSkyPresetCombo->getItemCount() > 0)
		{
			mSkyPresetCombo->setCurrentByIndex(new_index);

			// If we don't update the name here, we crash on next/prev -- MC
			LLWLParamManager::getInstance()->mCurParams.mName = mSkyPresetCombo->getSelectedValue().asString();
			if (LLWLParamManager::getInstance()->mCurParams.mName.empty())
			{
				LLWLParamManager::getInstance()->mCurParams.mName = "Default";
			}
			LLEnvManagerNew::instance().setUseSkyPreset(LLWLParamManager::getInstance()->mCurParams.mName);
		}
	}
	return false;
}


void LLFloaterWindLight::onChangePresetName(LLUICtrl* ctrl)
{
	LLComboBox* combo_box = static_cast<LLComboBox*>(ctrl);
	
	if(combo_box->getSimple().empty())
	{
		return;
	}

	const LLWLParamKey key(combo_box->getSelectedValue().asString(), LLEnvKey::SCOPE_LOCAL);
	if (LLWLParamManager::getInstance()->hasParamSet(key))
	{
		LLEnvManagerNew::instance().setUseSkyPreset(key.name);
	}
	else
	{
		//if that failed, use region's
		// LLEnvManagerNew::instance().useRegionSky();
		LLEnvManagerNew::instance().setUseSkyPreset("Default");
	}
	
	//LL_INFOS("WindLight") << "Current inventory ID: " << LLWLParamManager::getInstance()->mCurParams.mInventoryID << LL_ENDL;
	syncMenu();
}


void LLFloaterWindLight::onClickNext()
{
	S32 index = mSkyPresetCombo->getCurrentIndex();
	++index;
	if (index == mSkyPresetCombo->getItemCount())
		index = 0;
	mSkyPresetCombo->setCurrentByIndex(index);

	onChangePresetName(mSkyPresetCombo);
}

void LLFloaterWindLight::onClickPrev()
{
	S32 index = mSkyPresetCombo->getCurrentIndex();
	if (index == 0)
		index = mSkyPresetCombo->getItemCount();
	--index;
	mSkyPresetCombo->setCurrentByIndex(index);

	onChangePresetName(mSkyPresetCombo);
}

//static
void LLFloaterWindLight::selectTab(std::string tab_name)
{
	if (!tab_name.empty())
	{
		LLTabContainer* tabs = LLFloaterWindLight::instance()->getChild<LLTabContainer>("WindLight Tabs");
		tabs->selectTabByName(tab_name);
	}
}

void LLFloaterWindLight::populateSkyPresetsList()
{
	mSkyPresetCombo->removeall();

	LLWLParamManager::preset_name_list_t local_presets;
	LLWLParamManager::getInstance()->getLocalPresetNames(local_presets);

	for (LLWLParamManager::preset_name_list_t::const_iterator it = local_presets.begin(); it != local_presets.end(); ++it)
	{
		mSkyPresetCombo->add(*it);
	}

	LLWLParamManager::preset_name_list_t user_presets;
	LLWLParamManager::getInstance()->getUserPresetNames(user_presets);

	for (LLWLParamManager::preset_name_list_t::const_iterator it = user_presets.begin(); it != user_presets.end(); ++it)
	{
		mSkyPresetCombo->add(*it);
	}

	mSkyPresetCombo->selectByValue(LLEnvManagerNew::instance().getSkyPresetName());
}
