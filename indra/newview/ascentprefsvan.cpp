/** 
 * @file ascentprefsvan.cpp
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
#include "ascentprefsvan.h"

//project includes
#include "llaudioengine.h" //For gAudiop
#include "llstreamingaudio.h" //For LLStreamingAudioInterface
#include "llcolorswatch.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llagent.h"
#include "llfloaterchat.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "v4color.h"
#include "lluictrlfactory.h"
#include "llcombobox.h"
#include "llwind.h"
#include "llviewernetwork.h"
#include "pipeline.h"


LLPrefsAscentVan::LLPrefsAscentVan()
{
    LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_ascent_vanity.xml");

	childSetVisible("announce_stream_metadata", gAudiop && gAudiop->getStreamingAudioImpl() && gAudiop->getStreamingAudioImpl()->supportsMetaData());

	getChild<LLUICtrl>("tag_spoofing_combobox")->setCommitCallback(boost::bind(&LLPrefsAscentVan::onCommitClientTag, this, _1));

	getChild<LLUICtrl>("custom_tag_label_box")->setCommitCallback(boost::bind(&LLControlGroup::setString, boost::ref(gSavedSettings), "AscentCustomTagLabel", _2));
	commit_callback_t lineEditorControl(boost::bind(&LLControlGroup::setString, boost::ref(gSavedSettings), boost::bind(&LLUICtrl::getName, _1), _2));
	getChild<LLUICtrl>("UISndRestart")->setCommitCallback(lineEditorControl);

	getChild<LLUICtrl>("update_clientdefs")->setCommitCallback(boost::bind(LLPrefsAscentVan::onManualClientUpdate));

    refreshValues();
    refresh();
}

LLPrefsAscentVan::~LLPrefsAscentVan()
{
}

void LLPrefsAscentVan::onCommitClientTag(LLUICtrl* ctrl)
{
    std::string client_uuid;
    U32 client_index;

	LLComboBox* combo = static_cast<LLComboBox*>(ctrl);

	client_index = combo->getCurrentIndex();
	//Don't rebake if it's not neccesary.
	if (client_index != mSelectedClient)
	{
		client_uuid = combo->getSelectedValue().asString();
		gSavedSettings.setString("AscentReportClientUUID",  client_uuid);
		gSavedSettings.setU32("AscentReportClientIndex",  client_index);

		if (gAgentAvatarp)
		{
			// Slam pending upload count to "unstick" things
			bool slam_for_debug = true;
			gAgentAvatarp->forceBakeAllTextures(slam_for_debug);
		}
	}
}

//static
void LLPrefsAscentVan::onManualClientUpdate()
{
	LLChat chat("Definitions already up-to-date.");
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	if(SHClientTagMgr::instance().fetchDefinitions())
	{
		if(SHClientTagMgr::instance().parseDefinitions())
			chat.mText="Client definitons updated.";
		else
			chat.mText="Failed to parse updated definitions.";
	}
	LLFloaterChat::addChat(chat);
}

// Store current settings for cancel
void LLPrefsAscentVan::refreshValues()
{
    //Main -----------------------------------------------------------------------------------
    mUseAccountSettings		= gSavedSettings.getBOOL("AscentStoreSettingsPerAccount");
    mShowTPScreen			= !gSavedSettings.getBOOL("AscentDisableTeleportScreens");
    mPlayTPSound			= gSavedSettings.getBOOL("OptionPlayTpSound");
    mShowLogScreens			= !gSavedSettings.getBOOL("AscentDisableLogoutScreens");
	mDisableChatAnimation   = gSavedSettings.getBOOL("SGDisableChatAnimation");
	mAddNotReplace = gSavedSettings.getBOOL("LiruAddNotReplace");
	mTurnAround = gSavedSettings.getBOOL("TurnAroundWhenWalkingBackwards");
	mCustomizeAnim = gSavedSettings.getBOOL("LiruCustomizeAnim");
	mAnnounceSnapshots = gSavedSettings.getBOOL("AnnounceSnapshots");
	mAnnounceStreamMetadata = gSavedSettings.getBOOL("AnnounceStreamMetadata");
	mInactiveFloaterTransparency = gSavedSettings.getF32("InactiveFloaterTransparency");
	mActiveFloaterTransparency = gSavedSettings.getF32("ActiveFloaterTransparency");
	mScriptErrorsStealFocus = gSavedSettings.getBOOL("LiruScriptErrorsStealFocus");
	mConnectToNeighbors = gSavedSettings.getBOOL("AlchemyConnectToNeighbors");
	mRestartMinimized		= gSavedSettings.getBOOL("LiruRegionRestartMinimized");
	mRestartSound			= gSavedSettings.getString("UISndRestart");

    //Tags\Colors ----------------------------------------------------------------------------
    mAscentBroadcastTag     = gSavedSettings.getBOOL("AscentBroadcastTag");
    mReportClientUUID       = gSavedSettings.getString("AscentReportClientUUID");
    mSelectedClient			= gSavedSettings.getU32("AscentReportClientIndex");
    mShowSelfClientTag		= gSavedSettings.getBOOL("AscentShowSelfTag");
    mShowSelfClientTagColor = gSavedSettings.getBOOL("AscentShowSelfTagColor");
    mShowFriendsTag         = gSavedSettings.getBOOL("AscentShowFriendsTag");
    mDisplayClientTagOnNewLine		= gSavedSettings.getBOOL("SLBDisplayClientTagOnNewLine");
    mCustomTagOn			= gSavedSettings.getBOOL("AscentUseCustomTag");
    mCustomTagLabel			= gSavedSettings.getString("AscentCustomTagLabel");
    mCustomTagColor			= gSavedSettings.getColor4("AscentCustomTagColor");
    mShowOthersTag          = gSavedSettings.getBOOL("AscentShowOthersTag");
    mShowOthersTagColor     = gSavedSettings.getBOOL("AscentShowOthersTagColor");
    mShowIdleTime           = gSavedSettings.getBOOL("AscentShowIdleTime");
    mUseStatusColors        = gSavedSettings.getBOOL("AscentUseStatusColors");
    mUpdateTagsOnLoad       = gSavedSettings.getBOOL("AscentUpdateTagsOnLoad");
    mEffectColor			= gSavedSettings.getColor4("EffectColor");
    mFriendColor			= gSavedSettings.getColor4("AscentFriendColor");
    mEstateOwnerColor		= gSavedSettings.getColor4("AscentEstateOwnerColor");
    mLindenColor			= gSavedSettings.getColor4("AscentLindenColor");
    mMutedColor				= gSavedSettings.getColor4("AscentMutedColor");
	mMapAvatarColor			= gSavedSettings.getColor4("MapAvatar");
    mCustomColor			= gSavedSettings.getColor4("MoyMiniMapCustomColor");
	mColorFriendChat        = gSavedSettings.getBOOL("ColorFriendChat");
	mColorEOChat            = gSavedSettings.getBOOL("ColorEstateOwnerChat");
	mColorLindenChat        = gSavedSettings.getBOOL("ColorLindenChat");
	mColorMutedChat         = gSavedSettings.getBOOL("ColorMutedChat");
//	mColorCustomChat        = gSavedSettings.getBOOL("ColorCustomChat");

    mAvatarXModifier        = gSavedSettings.getF32("AscentAvatarXModifier");
    mAvatarYModifier        = gSavedSettings.getF32("AscentAvatarYModifier");
    mAvatarZModifier        = gSavedSettings.getF32("AscentAvatarZModifier");
}

// Update controls based on current settings
void LLPrefsAscentVan::refresh()
{
    //Main -----------------------------------------------------------------------------------
	getChildView("UISndRestart")->setValue(mRestartSound);

    //Tags\Colors ----------------------------------------------------------------------------
    LLComboBox* combo = getChild<LLComboBox>("tag_spoofing_combobox");
    combo->setCurrentByIndex(mSelectedClient);

    childSetEnabled("friends_color_textbox",     mUseStatusColors);

    childSetEnabled("custom_tag_label_text",   mCustomTagOn);
    childSetEnabled("custom_tag_label_box",    mCustomTagOn);
    childSetValue("custom_tag_label_box", gSavedSettings.getString("AscentCustomTagLabel"));
    childSetEnabled("custom_tag_color_text",   mCustomTagOn);
    childSetEnabled("custom_tag_color_swatch", mCustomTagOn);
}

// Reset settings to local copy
void LLPrefsAscentVan::cancel()
{
    //Main -----------------------------------------------------------------------------------
    gSavedSettings.setBOOL("AscentStoreSettingsPerAccount", mUseAccountSettings);
    gSavedSettings.setBOOL("AscentDisableTeleportScreens", !mShowTPScreen);
    gSavedSettings.setBOOL("OptionPlayTpSound",             mPlayTPSound);
    gSavedSettings.setBOOL("AscentDisableLogoutScreens",   !mShowLogScreens);
	gSavedSettings.setBOOL("SGDisableChatAnimation",		mDisableChatAnimation);
	gSavedSettings.setBOOL("LiruAddNotReplace", mAddNotReplace);
	gSavedSettings.setBOOL("TurnAroundWhenWalkingBackwards", mTurnAround);
	gSavedSettings.setBOOL("LiruCustomizeAnim", mCustomizeAnim);
	gSavedSettings.setBOOL("AnnounceSnapshots", mAnnounceSnapshots);
	gSavedSettings.setBOOL("AnnounceStreamMetadata", mAnnounceStreamMetadata);
	gSavedSettings.setF32("InactiveFloaterTransparency", mInactiveFloaterTransparency);
	gSavedSettings.setF32("ActiveFloaterTransparency", mActiveFloaterTransparency);
	gSavedSettings.setBOOL("LiruScriptErrorsStealFocus",    mScriptErrorsStealFocus);
	gSavedSettings.setBOOL("AlchemyConnectToNeighbors",     mConnectToNeighbors);
	gSavedSettings.setBOOL("LiruRegionRestartMinimized", mRestartMinimized);
	gSavedSettings.setString("UISndRestart", mRestartSound);

    //Tags\Colors ----------------------------------------------------------------------------
    gSavedSettings.setBOOL("AscentBroadcastTag",         mAscentBroadcastTag);
    gSavedSettings.setString("AscentReportClientUUID",   mReportClientUUID);
    gSavedSettings.setU32("AscentReportClientIndex",     mSelectedClient);
    gSavedSettings.setBOOL("AscentShowSelfTag",          mShowSelfClientTag);
    gSavedSettings.setBOOL("AscentShowSelfTagColor",     mShowSelfClientTagColor);
    gSavedSettings.setBOOL("AscentShowFriendsTag",       mShowFriendsTag);
    gSavedSettings.setBOOL("SLBDisplayClientTagOnNewLine",       mDisplayClientTagOnNewLine);
    gSavedSettings.setBOOL("AscentUseCustomTag",         mCustomTagOn);
    gSavedSettings.setString("AscentCustomTagLabel",     mCustomTagLabel);
    gSavedSettings.setColor4("AscentCustomTagColor",     mCustomTagColor);
    gSavedSettings.setBOOL("AscentShowOthersTag",        mShowOthersTag);
    gSavedSettings.setBOOL("AscentShowOthersTagColor",   mShowOthersTagColor);
    gSavedSettings.setBOOL("AscentShowIdleTime",         mShowIdleTime);
    gSavedSettings.setBOOL("AscentUseStatusColors",      mUseStatusColors);
    gSavedSettings.setBOOL("AscentUpdateTagsOnLoad",     mUpdateTagsOnLoad);
    gSavedSettings.setColor4("EffectColor",              mEffectColor);
    gSavedSettings.setColor4("AscentFriendColor",        mFriendColor);
    gSavedSettings.setColor4("AscentEstateOwnerColor",   mEstateOwnerColor);
    gSavedSettings.setColor4("AscentLindenColor",        mLindenColor);
    gSavedSettings.setColor4("AscentMutedColor",         mMutedColor);
	gSavedSettings.setColor4("MapAvatar",                mMapAvatarColor);
    gSavedSettings.setColor4("MoyMiniMapCustomColor",    mCustomColor);
    gSavedSettings.setBOOL("ColorFriendChat",            mColorFriendChat);
    gSavedSettings.setBOOL("ColorEstateOwnerChat",       mColorEOChat);
    gSavedSettings.setBOOL("ColorLindenChat",            mColorLindenChat);
    gSavedSettings.setBOOL("ColorMutedChat",             mColorMutedChat);
//	gSavedSettings.setBOOL("ColorCustomChat",            mColorCustomChat);

    gSavedSettings.setF32("AscentAvatarXModifier",       mAvatarXModifier);
    gSavedSettings.setF32("AscentAvatarYModifier",       mAvatarYModifier);
    gSavedSettings.setF32("AscentAvatarZModifier",       mAvatarZModifier);
}

// Update local copy so cancel has no effect
void LLPrefsAscentVan::apply()
{
    refreshValues();
    refresh();
}
