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
	getChild<LLUICtrl>("show_look_at_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("power_user_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("power_user_confirm_check")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));

    //Command Line ------------------------------------------------------------------------
	commit_callback_t lineEditorControl(boost::bind(&LLControlGroup::setString, boost::ref(gSavedSettings), boost::bind(&LLUICtrl::getName, _1), _2));
	getChild<LLUICtrl>("AscentCmdLinePos")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineGround")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineHeight")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineTeleportHome")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineRezPlatform")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineCalc")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineClearChat")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineDrawDistance")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdTeleportToCam")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineKeyToName")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineOfferTp")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineMapTo")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AscentCmdLineTP2")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("SinguCmdLineAway")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("SinguCmdLineRegionSay")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("SinguCmdLineURL")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AlchemyChatCommandResyncAnim")->setCommitCallback(lineEditorControl);
	getChild<LLUICtrl>("AlchemyChatCommandHoverHeight")->setCommitCallback(lineEditorControl);

	//Security ----------------------------------------------------------------------------

	//Build -------------------------------------------------------------------------------
	getChild<LLUICtrl>("next_owner_copy")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
	getChild<LLUICtrl>("script_next_owner_copy")->setCommitCallback(boost::bind(&LLPrefsAscentSys::onCommitCheckBox, this, _1, _2));
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
//    LL_INFOS() << "Change to " << ctrl->getControlName()  << " aka " << ctrl->getName() << LL_ENDL;

	const std::string name = ctrl->getName();
	bool enabled = value.asBoolean();
	if (name == "power_user_check")
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
			if(gAudiop)
				gAudiop->triggerSound(LLUUID("58a38e89-44c6-c52b-deb8-9f1ddc527319"), gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
			void cmdline_printchat(const std::string& message);
			cmdline_printchat(LLTrans::getString("PowerUser1") + '\n' + LLTrans::getString("PowerUser2") + '\n' + LLTrans::getString("Unlocked:") + '\n' + LLTrans::getString("PowerUser3") + "\n- " + LLTrans::getString("RightClick") + " > " + LLTrans::getString("PowerUser4") + "\n- " + LLTrans::getString("RightClick") + " > " + LLTrans::getString("PowerUser5"));
		}
	}
	else if (name == "next_owner_copy")
	{
		if (!enabled) gSavedSettings.setBOOL("ObjectsNextOwnerTransfer", true);
	}
	else if (name == "script_next_owner_copy")
	{
		if (!enabled) gSavedSettings.setBOOL("ScriptsNextOwnerTransfer", true);
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
    mDoubleClickAutoPilot		= gSavedSettings.getBOOL("DoubleClickAutoPilot");
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
	mUseContextMenus			= gSavedSettings.getBOOL("LiruUseContextMenus");
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
	mCmdLineRegionSay           = gSavedSettings.getString("SinguCmdLineRegionSay");
	mCmdLineURL                 = gSavedSettings.getString("SinguCmdLineURL");
	mCmdLineResync              = gSavedSettings.getString("AlchemyChatCommandResyncAnim");
	mCmdLineHover               = gSavedSettings.getString("AlchemyChatCommandHoverHeight");

    //Security ----------------------------------------------------------------------------
    mBroadcastViewerEffects		= gSavedSettings.getBOOL("BroadcastViewerEffects");
    mDisablePointAtAndBeam		= gSavedSettings.getBOOL("DisablePointAtAndBeam");
    mPrivateLookAt				= gSavedSettings.getBOOL("PrivateLookAt");
    mHideOwnLookAt				= gSavedSettings.getBOOL("AlchemyLookAtHideSelf");
    mShowLookAt					= gSavedSettings.getBOOL("AscentShowLookAt");
	mLookAtNames				= gSavedSettings.getS32("LookAtNameSystem");
	mLookAtLines				= gSavedSettings.getBOOL("AlchemyLookAtLines");
	mQuietSnapshotsToDisk		= gSavedSettings.getBOOL("QuietSnapshotsToDisk");
	mAnnounceBumps				= gSavedSettings.getBOOL("AnnounceBumps");
	mSitOnAway				= gSavedSettings.getBOOL("AlchemySitOnAway");
	mDetachBridge				= gSavedSettings.getBOOL("SGDetachBridge");
    mRevokePermsOnStandUp		= gSavedSettings.getBOOL("RevokePermsOnStandUp");
    mDisableClickSit			= gSavedSettings.getBOOL("DisableClickSit");
	mDisableClickSitOtherOwner	= gSavedSettings.getBOOL("DisableClickSitOtherOwner");
    mDisplayScriptJumps			= gSavedSettings.getBOOL("AscentDisplayTotalScriptJumps");
    mNumScriptDiff              = gSavedSettings.getF32("Ascentnumscriptdiff");
	mReplaceLinks				= gSavedSettings.getBOOL("SinguReplaceLinks");
	mEmergencySeconds           = gSavedPerAccountSettings.getU32("EmergencyTeleportSeconds");
	mLandmark			= gSavedPerAccountSettings.getString("EmergencyTeleportLandmark");
	mLandmarkBackup			= gSavedPerAccountSettings.getString("EmergencyTeleportLandmarkBackup");

	//Build -------------------------------------------------------------------------------
	mAlpha						= gSavedSettings.getF32("EmeraldBuildPrefs_Alpha");
	mColor						= gSavedSettings.getColor4("EmeraldBuildPrefs_Color");
	mFullBright					= gSavedSettings.getBOOL("EmeraldBuildPrefs_FullBright");
	mGlow						= gSavedSettings.getF32("EmeraldBuildPrefs_Glow");
	mItem						= gSavedPerAccountSettings.getString("EmeraldBuildPrefs_Item");
	mMaterial					= gSavedSettings.getString("BuildPrefs_Material");
	mNextCopy					= gSavedSettings.getBOOL("ObjectsNextOwnerCopy");
	mNextMod					= gSavedSettings.getBOOL("ObjectsNextOwnerModify");
	mNextTrans					= gSavedSettings.getBOOL("ObjectsNextOwnerTransfer");
	mScriptNextCopy				= gSavedSettings.getBOOL("ScriptsNextOwnerCopy");
	mScriptNextMod				= gSavedSettings.getBOOL("ScriptsNextOwnerModify");
	mScriptNextTrans			= gSavedSettings.getBOOL("ScriptsNextOwnerTransfer");
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
    childSetValue("power_user_check",				mPowerUser);
	if (LLUICtrl* ctrl = getChild<LLUICtrl>("power_user_confirm_check"))
	{
		ctrl->setEnabled(mPowerUser);
		ctrl->setValue(mPowerUser);
	}

	//Command Line ----------------------------------------------------------------------------
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
	childSetValue("SinguCmdLineRegionSay",      mCmdLineRegionSay);
	childSetValue("SinguCmdLineURL",            mCmdLineURL);
	childSetValue("AlchemyChatCommandResyncAnim",            mCmdLineResync);
	childSetValue("AlchemyChatCommandHoverHeight",            mCmdLineHover);

	//Security ----------------------------------------------------------------------------
	if (LLComboBox* combo = getChild<LLComboBox>("lookat_namesystem_combobox"))
		combo->setValue(mLookAtNames);

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
    gSavedSettings.setBOOL("DoubleClickAutoPilot", mDoubleClickAutoPilot);
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
	gSavedSettings.setBOOL("LiruUseContextMenus", mUseContextMenus);
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
	gSavedSettings.setString("SinguCmdLineRegionSay",		mCmdLineRegionSay);
	gSavedSettings.setString("SinguCmdLineURL",				mCmdLineURL);
	gSavedSettings.setString("AlchemyChatCommandResyncAnim",				mCmdLineResync);
	gSavedSettings.setString("AlchemyChatCommandHoverHeight",				mCmdLineHover);

    //Security ----------------------------------------------------------------------------
    gSavedSettings.setBOOL("BroadcastViewerEffects",        mBroadcastViewerEffects);
    gSavedSettings.setBOOL("DisablePointAtAndBeam",         mDisablePointAtAndBeam);
    gSavedSettings.setBOOL("PrivateLookAt",                 mPrivateLookAt);
    gSavedSettings.setBOOL("AlchemyLookAtHideSelf",         mHideOwnLookAt);
    gSavedSettings.setBOOL("AscentShowLookAt",              mShowLookAt);
	gSavedSettings.setS32("LookAtNameSystem",               mLookAtNames);
	gSavedSettings.setBOOL("AlchemyLookAtLines",            mLookAtLines);
    gSavedSettings.setBOOL("QuietSnapshotsToDisk",			mQuietSnapshotsToDisk);
    gSavedSettings.setBOOL("AnnounceBumps",    			mAnnounceBumps);
    gSavedSettings.setBOOL("AlchemySitOnAway",    			mSitOnAway);
    gSavedSettings.setBOOL("SGDetachBridge",    			mDetachBridge);
    gSavedSettings.setBOOL("RevokePermsOnStandUp",          mRevokePermsOnStandUp);
    gSavedSettings.setBOOL("DisableClickSit",               mDisableClickSit);
	gSavedSettings.setBOOL("DisableClickSitOtherOwner",     mDisableClickSitOtherOwner);
    gSavedSettings.setBOOL("AscentDisplayTotalScriptJumps", mDisplayScriptJumps);
    gSavedSettings.setF32("Ascentnumscriptdiff",            mNumScriptDiff);
	gSavedSettings.setBOOL("SinguReplaceLinks",             mReplaceLinks);
	gSavedPerAccountSettings.setU32("EmergencyTeleportSeconds", mEmergencySeconds);
	gSavedPerAccountSettings.setString("EmergencyTeleportLandmark",      mLandmark);
	gSavedPerAccountSettings.setString("EmergencyTeleportLandmarkBackup",      mLandmarkBackup);

	//Build -------------------------------------------------------------------------------
	gSavedSettings.setF32("EmeraldBuildPrefs_Alpha",        mAlpha);
	gSavedSettings.setColor4("EmeraldBuildPrefs_Color",     mColor);
	gSavedSettings.setBOOL("EmeraldBuildPrefs_FullBright",  mFullBright);
	gSavedSettings.setF32("EmeraldBuildPrefs_Glow",         mGlow);
	gSavedPerAccountSettings.setString("EmeraldBuildPrefs_Item",      mItem);
	gSavedSettings.setString("BuildPrefs_Material",         mMaterial);
	gSavedSettings.setBOOL("ObjectsNextOwnerCopy",          mNextCopy);
	gSavedSettings.setBOOL("ObjectsNextOwnerModify",        mNextMod);
	gSavedSettings.setBOOL("ObjectsNextOwnerTransfer",      mNextTrans);
	gSavedSettings.setBOOL("ScriptsNextOwnerCopy",          mScriptNextCopy);
	gSavedSettings.setBOOL("ScriptsNextOwnerModify",        mScriptNextMod);
	gSavedSettings.setBOOL("ScriptsNextOwnerTransfer",      mScriptNextTrans);
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
