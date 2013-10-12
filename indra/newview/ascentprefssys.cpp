/** 
 * @file ascentprefssys.cpp
 * @Ascent Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Henri Beauchamp.
 * Rewritten in its entirety 2010 Hg Beeks. 
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

//File include
#include "ascentprefssys.h"
#include "llagent.h"
#include "llaudioengine.h" //For POWER USER affirmation.
#include "llchat.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llfloaterchat.h" //For POWER USER affirmation.
#include "llradiogroup.h"
#include "lltexturectrl.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "lltrans.h"

LLPrefsAscentSys::LLPrefsAscentSys()
{
    LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_ascent_system.xml");

    //General -----------------------------------------------------------------------------
	getChild<LLUICtrl>("speed_rez_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("double_click_teleport_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("show_look_at_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("enable_clouds")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("power_user_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("power_user_confirm_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));

    //Command Line ------------------------------------------------------------------------
	getChild<LLUICtrl>("chat_cmd_toggle")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLinePos")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineGround")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineHeight")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineTeleportHome")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineRezPlatform")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineCalc")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineClearChat")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineDrawDistance")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdTeleportToCam")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineKeyToName")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineOfferTp")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineMapTo")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("AscentCmdLineTP2")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("SinguCmdLineAway")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));
	getChild<LLUICtrl>("SinguCmdLineURL")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCmdLine, this, _1, _2));

	//Security ----------------------------------------------------------------------------
	getChild<LLUICtrl>("disable_click_sit_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));

	//Build -------------------------------------------------------------------------------
	getChild<LLUICtrl>("next_owner_copy")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	childSetEnabled("next_owner_transfer", gSavedSettings.getBOOL("NextOwnerCopy"));
	getChild<LLUICtrl>("material")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitComboBox, this, _1, _2));
	getChild<LLUICtrl>("combobox shininess")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitComboBox, this, _1, _2));
	getChild<LLTextureCtrl>("texture control")->setDefaultImageAssetID(LLUUID(gSavedSettings.getString("EmeraldBuildPrefs_Texture")));
	getChild<LLUICtrl>("texture control")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitTexturePicker, this, _1));

	refreshValues();
    refresh();
}

LLPrefsAscentSys::~LLPrefsAscentSys()
{
}

void LLPrefsAscentSys::onCommitCheckBox(LLUICtrl* ctrl, const LLSD& value)
{
//    llinfos << "Change to " << ctrl->getControlName()  << " aka " << ctrl->getName() << llendl;

	const std::string name = ctrl->getName();
	bool enabled = value.asBoolean();
	if (name == "speed_rez_check")
	{
		childSetEnabled("speed_rez_interval", enabled);
		childSetEnabled("speed_rez_seconds", enabled);
	}
	else if (name == "double_click_teleport_check")
	{
		childSetEnabled("center_after_teleport_check", enabled);
		childSetEnabled("offset_teleport_check", enabled);
	}
	else if (name == "enable_clouds")
	{
		childSetEnabled("enable_classic_clouds", enabled);
	}
	else if (name == "power_user_check")
	{
		childSetEnabled("power_user_confirm_check", enabled);
		childSetValue("power_user_confirm_check", false);
	}
	else if (name == "power_user_confirm_check")
	{
		gSavedSettings.setBOOL("AscentPowerfulWizard", enabled);

		if (enabled)
		{
			LLVector3d lpos_global = gAgent.getPositionGlobal();
			gAudiop->triggerSound(LLUUID("58a38e89-44c6-c52b-deb8-9f1ddc527319"), gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
			LLChat chat;
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			chat.mText = LLTrans::getString("PowerUser1") + "\n" + LLTrans::getString("PowerUser2") + "\n" + LLTrans::getString("Unlocked:") + "\n" + LLTrans::getString("PowerUser3") + "\n- " + LLTrans::getString("RightClick") + " > " + LLTrans::getString("PowerUser4") + "\n- " + LLTrans::getString("RightClick") + " > " + LLTrans::getString("PowerUser5");
			LLFloaterChat::addChat(chat);
		}
	}
	else if (name == "disable_click_sit_check")
	{
		childSetEnabled("disable_click_sit_own_check", !enabled);
	}
	else if (name == "next_owner_copy")
	{
		if (!enabled) gSavedSettings.setBOOL("NextOwnerTransfer", true);
		childSetEnabled("next_owner_transfer", enabled);
	}
}

void LLPrefsAscentSys::onCommitCmdLine(LLUICtrl* ctrl, const LLSD& value)
{
	const std::string& name = ctrl->getName();
	if (name == "chat_cmd_toggle")
    {
		bool enabled = value.asBoolean();
		childSetEnabled("cmd_line_text_2",           enabled);
		childSetEnabled("cmd_line_text_3",           enabled);
		childSetEnabled("cmd_line_text_4",           enabled);
		childSetEnabled("cmd_line_text_5",           enabled);
		childSetEnabled("cmd_line_text_6",           enabled);
		childSetEnabled("cmd_line_text_7",           enabled);
		childSetEnabled("cmd_line_text_8",           enabled);
		childSetEnabled("cmd_line_text_9",           enabled);
		childSetEnabled("cmd_line_text_10",          enabled);
		childSetEnabled("cmd_line_text_11",          enabled);
		childSetEnabled("cmd_line_text_12",          enabled);
		childSetEnabled("cmd_line_text_13",          enabled);
		childSetEnabled("cmd_line_text_15",          enabled);
		childSetEnabled("AscentCmdLinePos",          enabled);
		childSetEnabled("AscentCmdLineGround",       enabled);
		childSetEnabled("AscentCmdLineHeight",       enabled);
		childSetEnabled("AscentCmdLineTeleportHome", enabled);
		childSetEnabled("AscentCmdLineRezPlatform",  enabled);
		childSetEnabled("AscentPlatformSize",        enabled);
		childSetEnabled("AscentCmdLineCalc",         enabled);
		childSetEnabled("AscentCmdLineClearChat",    enabled);
		childSetEnabled("AscentCmdLineDrawDistance", enabled);
		childSetEnabled("AscentCmdTeleportToCam",    enabled);
		childSetEnabled("AscentCmdLineKeyToName",    enabled);
		childSetEnabled("AscentCmdLineOfferTp",      enabled);
		childSetEnabled("AscentCmdLineMapTo",        enabled);
		childSetEnabled("map_to_keep_pos",           enabled);
		childSetEnabled("AscentCmdLineTP2",          enabled);
		childSetEnabled("SinguCmdLineAway",          enabled);
		childSetEnabled("SinguCmdLineURL",           enabled);
    }
	else
	{
		gSavedSettings.setString(name, value); // Singu Note: Keep commandline settings using the same name as their settings
	}
}

void LLPrefsAscentSys::onCommitComboBox(LLUICtrl* ctrl, const LLSD& value)
{
	gSavedSettings.setString(ctrl->getControlName(), value.asString());
}

void LLPrefsAscentSys::onCommitTexturePicker(LLUICtrl* ctrl)
{
	LLTextureCtrl*	image_ctrl = static_cast<LLTextureCtrl*>(ctrl);
	if(image_ctrl)	gSavedSettings.setString("EmeraldBuildPrefs_Texture", image_ctrl->getImageAssetID().asString());
}

void LLPrefsAscentSys::refreshValues()
{
    //General -----------------------------------------------------------------------------
    mDoubleClickTeleport		= gSavedSettings.getBOOL("DoubleClickTeleport");
        mResetCameraAfterTP		= gSavedSettings.getBOOL("OptionRotateCamAfterLocalTP");
        mOffsetTPByUserHeight	= gSavedSettings.getBOOL("OptionOffsetTPByAgentHeight");
	mClearBeaconAfterTeleport	= gSavedSettings.getBOOL("ClearBeaconAfterTeleport");
    mLiruFlyAfterTeleport		= gSavedSettings.getBOOL("LiruFlyAfterTeleport");
    mLiruContinueFlying			= gSavedSettings.getBOOL("LiruContinueFlyingOnUnsit");
    mPreviewAnimInWorld			= gSavedSettings.getBOOL("PreviewAnimInWorld");
    mSaveScriptsAsMono			= gSavedSettings.getBOOL("SaveInventoryScriptsAsMono");
    mAlwaysRezInGroup			= gSavedSettings.getBOOL("AscentAlwaysRezInGroup");
    mBuildAlwaysEnabled			= gSavedSettings.getBOOL("AscentBuildAlwaysEnabled");
    mAlwaysShowFly				= gSavedSettings.getBOOL("AscentFlyAlwaysEnabled");
    mDisableMinZoom				= gSavedSettings.getBOOL("AscentDisableMinZoomDist");
    mPowerUser					= gSavedSettings.getBOOL("AscentPowerfulWizard");
    mFetchInventoryOnLogin		= gSavedSettings.getBOOL("FetchInventoryOnLogin");
    mEnableLLWind				= gSavedSettings.getBOOL("WindEnabled");
    mEnableClouds				= gSavedSettings.getBOOL("CloudsEnabled");
        mEnableClassicClouds		= gSavedSettings.getBOOL("SkyUseClassicClouds");
    mSpeedRez					= gSavedSettings.getBOOL("SpeedRez");
        mSpeedRezInterval			= gSavedSettings.getU32("SpeedRezInterval");
	mUseWebProfiles				= gSavedSettings.getBOOL("UseWebProfiles");
	mUseWebSearch				= gSavedSettings.getBOOL("UseWebSearch");

    //Command Line ------------------------------------------------------------------------
    mCmdLine                    = gSavedSettings.getBOOL("AscentCmdLine");
    mCmdLinePos                 = gSavedSettings.getString("AscentCmdLinePos");
    mCmdLineGround              = gSavedSettings.getString("AscentCmdLineGround");
    mCmdLineHeight              = gSavedSettings.getString("AscentCmdLineHeight");
    mCmdLineTeleportHome        = gSavedSettings.getString("AscentCmdLineTeleportHome");
    mCmdLineRezPlatform         = gSavedSettings.getString("AscentCmdLineRezPlatform");
    mCmdPlatformSize            = gSavedSettings.getF32("AscentPlatformSize");
    mCmdLineCalc                = gSavedSettings.getString("AscentCmdLineCalc");
    mCmdLineClearChat           = gSavedSettings.getString("AscentCmdLineClearChat");
    mCmdLineDrawDistance        = gSavedSettings.getString("AscentCmdLineDrawDistance");
    mCmdTeleportToCam           = gSavedSettings.getString("AscentCmdTeleportToCam");
    mCmdLineKeyToName           = gSavedSettings.getString("AscentCmdLineKeyToName");
    mCmdLineOfferTp             = gSavedSettings.getString("AscentCmdLineOfferTp");
    mCmdLineMapTo               = gSavedSettings.getString("AscentCmdLineMapTo");
    mCmdMapToKeepPos            = gSavedSettings.getBOOL("AscentMapToKeepPos");
    mCmdLineTP2                 = gSavedSettings.getString("AscentCmdLineTP2");
    mCmdLineAway                = gSavedSettings.getString("SinguCmdLineAway");
	mCmdLineURL                 = gSavedSettings.getString("SinguCmdLineURL");

    //Security ----------------------------------------------------------------------------
    mBroadcastViewerEffects		= gSavedSettings.getBOOL("BroadcastViewerEffects");
    mDisablePointAtAndBeam		= gSavedSettings.getBOOL("DisablePointAtAndBeam");
    mPrivateLookAt				= gSavedSettings.getBOOL("PrivateLookAt");
    mShowLookAt					= gSavedSettings.getBOOL("AscentShowLookAt");
	mQuietSnapshotsToDisk		= gSavedSettings.getBOOL("QuietSnapshotsToDisk");
	mDetachBridge				= gSavedSettings.getBOOL("SGDetachBridge");
    mRevokePermsOnStandUp		= gSavedSettings.getBOOL("RevokePermsOnStandUp");
    mDisableClickSit			= gSavedSettings.getBOOL("DisableClickSit");
	mDisableClickSitOtherOwner	= gSavedSettings.getBOOL("DisableClickSitOtherOwner");
    mDisplayScriptJumps			= gSavedSettings.getBOOL("AscentDisplayTotalScriptJumps");
    mNumScriptDiff              = gSavedSettings.getF32("Ascentnumscriptdiff");

	//Build -------------------------------------------------------------------------------
	mAlpha						= gSavedSettings.getF32("EmeraldBuildPrefs_Alpha");
	mColor						= gSavedSettings.getColor4("EmeraldBuildPrefs_Color");
	mFullBright					= gSavedSettings.getBOOL("EmeraldBuildPrefs_FullBright");
	mGlow						= gSavedSettings.getF32("EmeraldBuildPrefs_Glow");
	mItem						= gSavedPerAccountSettings.getString("EmeraldBuildPrefs_Item");
	mMaterial					= gSavedSettings.getString("BuildPrefs_Material");
	mNextCopy					= gSavedSettings.getBOOL("NextOwnerCopy");
	mNextMod					= gSavedSettings.getBOOL("NextOwnerModify");
	mNextTrans					= gSavedSettings.getBOOL("NextOwnerTransfer");
	mShiny						= gSavedSettings.getString("EmeraldBuildPrefs_Shiny");
	mTemporary					= gSavedSettings.getBOOL("EmeraldBuildPrefs_Temporary");
	mTexture					= gSavedSettings.getString("EmeraldBuildPrefs_Texture");
	mPhantom					= gSavedSettings.getBOOL("EmeraldBuildPrefs_Phantom");
	mPhysical					= gSavedSettings.getBOOL("EmeraldBuildPrefs_Physical");
	mXsize						= gSavedSettings.getF32("BuildPrefs_Xsize");
	mYsize						= gSavedSettings.getF32("BuildPrefs_Ysize");
	mZsize						= gSavedSettings.getF32("BuildPrefs_Zsize");
}

void LLPrefsAscentSys::refresh()
{
    //General -----------------------------------------------------------------------------
    childSetEnabled("center_after_teleport_check",	mDoubleClickTeleport);
    childSetEnabled("offset_teleport_check",		mDoubleClickTeleport);
    childSetValue("power_user_check",				mPowerUser);
    childSetValue("power_user_confirm_check",		mPowerUser);
    childSetEnabled("speed_rez_interval",           mSpeedRez);
    childSetEnabled("speed_rez_seconds",            mSpeedRez);

    //Command Line ------------------------------------------------------------------------
    childSetEnabled("cmd_line_text_2",            mCmdLine);
    childSetEnabled("cmd_line_text_3",            mCmdLine);
    childSetEnabled("cmd_line_text_4",            mCmdLine);
    childSetEnabled("cmd_line_text_5",            mCmdLine);
    childSetEnabled("cmd_line_text_6",            mCmdLine);
    childSetEnabled("cmd_line_text_7",            mCmdLine);
    childSetEnabled("cmd_line_text_8",            mCmdLine);
    childSetEnabled("cmd_line_text_9",            mCmdLine);
    childSetEnabled("cmd_line_text_10",           mCmdLine);
    childSetEnabled("cmd_line_text_11",           mCmdLine);
    childSetEnabled("cmd_line_text_12",           mCmdLine);
    childSetEnabled("cmd_line_text_13",           mCmdLine);
    childSetEnabled("cmd_line_text_15",           mCmdLine);
    childSetEnabled("AscentCmdLinePos",           mCmdLine);
    childSetEnabled("AscentCmdLineGround",        mCmdLine);
    childSetEnabled("AscentCmdLineHeight",        mCmdLine);
    childSetEnabled("AscentCmdLineTeleportHome",  mCmdLine);
    childSetEnabled("AscentCmdLineRezPlatform",   mCmdLine);
    childSetEnabled("AscentPlatformSize",         mCmdLine);
    childSetEnabled("AscentCmdLineCalc",          mCmdLine);
    childSetEnabled("AscentCmdLineClearChat",     mCmdLine);
    childSetEnabled("AscentCmdLineDrawDistance",  mCmdLine);
    childSetEnabled("AscentCmdTeleportToCam",     mCmdLine);
    childSetEnabled("AscentCmdLineKeyToName",     mCmdLine);
    childSetEnabled("AscentCmdLineOfferTp",       mCmdLine);
    childSetEnabled("AscentCmdLineMapTo",         mCmdLine);
    childSetEnabled("map_to_keep_pos",            mCmdLine);
    childSetEnabled("AscentCmdLineTP2",           mCmdLine);
    childSetEnabled("SinguCmdLineAway",           mCmdLine);
	childSetEnabled("SinguCmdLineURL",            mCmdLine);

    //Security ----------------------------------------------------------------------------
    childSetValue("AscentCmdLinePos",           mCmdLinePos);
    childSetValue("AscentCmdLineGround",        mCmdLineGround);
    childSetValue("AscentCmdLineHeight",        mCmdLineHeight);
    childSetValue("AscentCmdLineTeleportHome",  mCmdLineTeleportHome);
    childSetValue("AscentCmdLineRezPlatform",   mCmdLineRezPlatform);
    childSetValue("AscentCmdLineCalc",          mCmdLineCalc);
    childSetValue("AscentCmdLineClearChat",     mCmdLineClearChat);
    childSetValue("AscentCmdLineDrawDistance",  mCmdLineDrawDistance);
    childSetValue("AscentCmdTeleportToCam",     mCmdTeleportToCam);
    childSetValue("AscentCmdLineKeyToName",     mCmdLineKeyToName);
    childSetValue("AscentCmdLineOfferTp",       mCmdLineOfferTp);
    childSetValue("AscentCmdLineMapTo",         mCmdLineMapTo);
    childSetValue("AscentCmdLineTP2",           mCmdLineTP2);
    childSetValue("SinguCmdLineAway",           mCmdLineAway);
	childSetValue("SinguCmdLineURL",            mCmdLineURL);

	//Build -------------------------------------------------------------------------------
	childSetValue("alpha",               mAlpha);
	getChild<LLColorSwatchCtrl>("colorswatch")->setOriginal(mColor);
	childSetValue("EmFBToggle",          mFullBright);
	childSetValue("glow",                mGlow);
	childSetValue("material",            mMaterial);
	childSetValue("next_owner_copy",     mNextCopy);
	childSetValue("next_owner_modify",   mNextMod);
	childSetValue("next_owner_transfer", mNextTrans);
	childSetValue("EmPhantomToggle",     mPhantom);
	childSetValue("EmPhysicalToggle",    mPhysical);
	childSetValue("combobox shininess",  mShiny);
	childSetValue("EmTemporaryToggle",   mTemporary);
	childSetValue("texture control",     mTexture);
	childSetValue("X size",              mXsize);
	childSetValue("Y size",              mYsize);
	childSetValue("Z size",              mZsize);
}

void LLPrefsAscentSys::cancel()
{
    //General -----------------------------------------------------------------------------
    gSavedSettings.setBOOL("DoubleClickTeleport", mDoubleClickTeleport);
        gSavedSettings.setBOOL("OptionRotateCamAfterLocalTP", mResetCameraAfterTP);
        gSavedSettings.setBOOL("OptionOffsetTPByAgentHeight", mOffsetTPByUserHeight);
	gSavedSettings.setBOOL("ClearBeaconAfterTeleport", mClearBeaconAfterTeleport);
    gSavedSettings.setBOOL("LiruFlyAfterTeleport", mLiruFlyAfterTeleport);
    gSavedSettings.setBOOL("LiruContinueFlyingOnUnsit", mLiruContinueFlying);
    gSavedSettings.setBOOL("PreviewAnimInWorld", mPreviewAnimInWorld);
    gSavedSettings.setBOOL("SaveInventoryScriptsAsMono", mSaveScriptsAsMono);
    gSavedSettings.setBOOL("AscentAlwaysRezInGroup", mAlwaysRezInGroup);
    gSavedSettings.setBOOL("AscentBuildAlwaysEnabled", mBuildAlwaysEnabled);
    gSavedSettings.setBOOL("AscentFlyAlwaysEnabled", mAlwaysShowFly);
    gSavedSettings.setBOOL("AscentDisableMinZoomDist", mDisableMinZoom);
    gSavedSettings.setBOOL("FetchInventoryOnLogin", mFetchInventoryOnLogin);
    gSavedSettings.setBOOL("WindEnabled", mEnableLLWind);
    gSavedSettings.setBOOL("CloudsEnabled", mEnableClouds);
        gSavedSettings.setBOOL("SkyUseClassicClouds", mEnableClassicClouds);
    gSavedSettings.setBOOL("SpeedRez", mSpeedRez);
        gSavedSettings.setU32("SpeedRezInterval", mSpeedRezInterval);
	gSavedSettings.setBOOL("UseWebProfiles", mUseWebProfiles);
	gSavedSettings.setBOOL("UseWebSearch", mUseWebSearch);

    //Command Line ------------------------------------------------------------------------
    gSavedSettings.setBOOL("AscentCmdLine",                 mCmdLine);
    gSavedSettings.setString("AscentCmdLinePos",		    mCmdLinePos);
    gSavedSettings.setString("AscentCmdLineGround",		    mCmdLineGround);
    gSavedSettings.setString("AscentCmdLineHeight",		    mCmdLineHeight);
    gSavedSettings.setString("AscentCmdLineTeleportHome",	mCmdLineTeleportHome);
    gSavedSettings.setString("AscentCmdLineRezPlatform",	mCmdLineRezPlatform);
    gSavedSettings.setF32("AscentPlatformSize",             mCmdPlatformSize);
    gSavedSettings.setString("AscentCmdLineCalc",		    mCmdLineCalc);
    gSavedSettings.setString("AscentCmdLineClearChat",	    mCmdLineClearChat);
    gSavedSettings.setString("AscentCmdLineDrawDistance",	mCmdLineDrawDistance);
    gSavedSettings.setString("AscentCmdTeleportToCam",		mCmdTeleportToCam);
    gSavedSettings.setString("AscentCmdLineKeyToName",		mCmdLineKeyToName);
    gSavedSettings.setString("AscentCmdLineOfferTp",		mCmdLineOfferTp);
    gSavedSettings.setString("AscentCmdLineMapTo",			mCmdLineMapTo);
    gSavedSettings.setBOOL("AscentMapToKeepPos",            mCmdMapToKeepPos);
    gSavedSettings.setString("AscentCmdLineTP2",			mCmdLineTP2);
    gSavedSettings.setString("SinguCmdLineAway",			mCmdLineAway);
	gSavedSettings.setString("SinguCmdLineURL",				mCmdLineURL);

    //Security ----------------------------------------------------------------------------
    gSavedSettings.setBOOL("BroadcastViewerEffects",        mBroadcastViewerEffects);
    gSavedSettings.setBOOL("DisablePointAtAndBeam",         mDisablePointAtAndBeam);
    gSavedSettings.setBOOL("PrivateLookAt",                 mPrivateLookAt);
    gSavedSettings.setBOOL("AscentShowLookAt",              mShowLookAt);
    gSavedSettings.setBOOL("QuietSnapshotsToDisk",			mQuietSnapshotsToDisk);
    gSavedSettings.setBOOL("SGDetachBridge",    			mDetachBridge);
    gSavedSettings.setBOOL("RevokePermsOnStandUp",          mRevokePermsOnStandUp);
    gSavedSettings.setBOOL("DisableClickSit",               mDisableClickSit);
	gSavedSettings.setBOOL("DisableClickSitOtherOwner",     mDisableClickSitOtherOwner);
    gSavedSettings.setBOOL("AscentDisplayTotalScriptJumps", mDisplayScriptJumps);
    gSavedSettings.setF32("Ascentnumscriptdiff",            mNumScriptDiff);

	//Build -------------------------------------------------------------------------------
	gSavedSettings.setF32("EmeraldBuildPrefs_Alpha",        mAlpha);
	gSavedSettings.setColor4("EmeraldBuildPrefs_Color",     mColor);
	gSavedSettings.setBOOL("EmeraldBuildPrefs_FullBright",  mFullBright);
	gSavedSettings.setF32("EmeraldBuildPrefs_Glow",         mGlow);
	gSavedPerAccountSettings.setString("EmeraldBuildPrefs_Item",      mItem);
	gSavedSettings.setString("BuildPrefs_Material",         mMaterial);
	gSavedSettings.setBOOL("NextOwnerCopy",                 mNextCopy);
	gSavedSettings.setBOOL("NextOwnerModify",               mNextMod);
	gSavedSettings.setBOOL("NextOwnerTransfer",             mNextTrans);
	gSavedSettings.setBOOL("EmeraldBuildPrefs_Phantom",     mPhantom);
	gSavedSettings.setBOOL("EmeraldBuildPrefs_Physical",    mPhysical);
	gSavedSettings.setString("EmeraldBuildPrefs_Shiny",     mShiny);
	gSavedSettings.setBOOL("EmeraldBuildPrefs_Temporary",   mTemporary);
	gSavedSettings.setString("EmeraldBuildPrefs_Texture",   mTexture);
	gSavedSettings.setF32("BuildPrefs_Xsize",               mXsize);
	gSavedSettings.setF32("BuildPrefs_Ysize",               mYsize);
	gSavedSettings.setF32("BuildPrefs_Zsize",               mZsize);
}

void LLPrefsAscentSys::apply()
{
    refreshValues();
    refresh();
}
