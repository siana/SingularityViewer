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
#include "ascentprefschat.h"
#include "llagent.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llfloaterautoreplacesettings.h"
#include "llradiogroup.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "NACLantispam.h"
#include "lgghunspell_wrapper.h"
#include "lltrans.h"

#include "llstartup.h"


LLPrefsAscentChat::LLPrefsAscentChat()
{
    LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_ascent_chat.xml");

	getChild<LLUICtrl>("SpellBase")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onSpellBaseComboBoxCommit, this, _2));
	getChild<LLUICtrl>("EmSpell_EditCustom")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onSpellEditCustom, this));
	getChild<LLUICtrl>("EmSpell_GetMore")->setCommitCallback(boost::bind(&lggHunSpell_Wrapper::getMoreButton, glggHunSpell, this));
	getChild<LLUICtrl>("EmSpell_Add")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onSpellAdd, this));
	getChild<LLUICtrl>("EmSpell_Remove")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onSpellRemove, this));

	getChild<LLUICtrl>("time_format_combobox")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitTimeDate, this, _1));
	getChild<LLUICtrl>("date_format_combobox")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitTimeDate, this, _1));

	bool started = (LLStartUp::getStartupState() == STATE_STARTED);
	if (!started) // Disable autoresponse when not logged in
	{
		LLView* autoresponse = getChildView("Autoresponse");
		autoresponse->setAllChildrenEnabled(false);
		autoresponse->setToolTip(LLTrans::getString("NotLoggedIn"));
	}

	childSetEnabled("reset_antispam", started);
	getChild<LLUICtrl>("reset_antispam")->setCommitCallback([](LLUICtrl* ctrl, const LLSD& param) {
		if (auto inst = NACLAntiSpamRegistry::getIfExists())
			inst->resetQueues();
	});

	getChild<LLUICtrl>("autoreplace")->setCommitCallback(boost::bind(LLFloaterAutoReplaceSettings::showInstance, LLSD()));
	getChild<LLUICtrl>("KeywordsOn")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));
	getChild<LLUICtrl>("KeywordsList")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));
	getChild<LLUICtrl>("KeywordsSound")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));
	getChild<LLUICtrl>("KeywordsInChat")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));
	getChild<LLUICtrl>("KeywordsInIM")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));
	getChild<LLUICtrl>("KeywordsChangeColor")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));
	getChild<LLUICtrl>("KeywordsColor")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));
	getChild<LLUICtrl>("KeywordsPlaySound")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitKeywords, this, _1));

    refreshValues();
    refresh();
}

LLPrefsAscentChat::~LLPrefsAscentChat()
{
}

void LLPrefsAscentChat::onSpellAdd()
{
	glggHunSpell->addButton(childGetValue("EmSpell_Avail").asString());
	refresh();
}

void LLPrefsAscentChat::onSpellRemove()
{
	glggHunSpell->removeButton(childGetValue("EmSpell_Installed").asString());
	refresh();
}

void LLPrefsAscentChat::onSpellEditCustom()
{
	glggHunSpell->editCustomButton();
}

void LLPrefsAscentChat::onSpellBaseComboBoxCommit(const LLSD& value)
{
	glggHunSpell->newDictSelection(value.asString());
}

void setTimeDateFormats(const S8& tempTimeFormat, const S8& tempDateFormat)
{
	std::string short_date, long_date, short_time, long_time, timestamp;

	if (tempDateFormat != -1)
	{
		if (tempDateFormat < 3)
		{
			short_date = !tempDateFormat ? "%F" :
				tempDateFormat == 1 ? "%Y/%m/%d" :
				"%d/%m/%Y";
			long_date  = "%A, %d %B %Y";
			timestamp  = "%a %d %b %Y";
		}
		else
		{
			short_date = "%m/%d/%Y";
			long_date  = "%A, %B %d %Y";
			timestamp  = "%a %b %d %Y";
		}
	}

	if (tempTimeFormat != -1)
	{
		if (tempTimeFormat == 0)
		{
			short_time = "%R";
			long_time  = "%T";
			if (!timestamp.empty()) timestamp += " %T";
		}
		else
		{
			short_time = "%I:%M %p";
			long_time  = "%I:%M:%S %p";
			if (!timestamp.empty()) timestamp += " %I:%M %p";
		}
	}
	else if (!timestamp.empty())
	{
		timestamp += ' ' + gSavedSettings.getString("ShortTimeFormat");
	}

	if (!short_date.empty())
	{
		gSavedSettings.setString("ShortDateFormat", short_date);
		gSavedSettings.setString("LongDateFormat",  long_date);
	}
	if (!short_time.empty())
	{
		gSavedSettings.setString("ShortTimeFormat", short_time);
		gSavedSettings.setString("LongTimeFormat",  long_time);
	}
	if (!timestamp.empty())
		gSavedSettings.setString("TimestampFormat", timestamp);
}

void LLPrefsAscentChat::onCommitTimeDate(LLUICtrl* ctrl)
{
	LLComboBox* combo = static_cast<LLComboBox*>(ctrl);
	if (ctrl->getName() == "time_format_combobox")
	{
		tempTimeFormat = combo->getCurrentIndex();
	}
	else if (ctrl->getName() == "date_format_combobox")
	{
		tempDateFormat = combo->getCurrentIndex();
	}

	setTimeDateFormats(tempTimeFormat, tempDateFormat);
}

void LLPrefsAscentChat::onCommitKeywords(LLUICtrl* ctrl)
{
    if (ctrl->getName() == "KeywordsOn")
    {
		bool enabled = childGetValue("KeywordsOn").asBoolean();
		childSetEnabled("KeywordsList",        enabled);
		childSetEnabled("KeywordsInChat",      enabled);
		childSetEnabled("KeywordsInIM",        enabled);
		childSetEnabled("KeywordsChangeColor", enabled);
		childSetEnabled("KeywordsColor",       enabled);
		childSetEnabled("KeywordsPlaySound",   enabled);
		childSetEnabled("KeywordsSound",       enabled);
    }

	gSavedPerAccountSettings.setBOOL("KeywordsOn",          childGetValue("KeywordsOn"));
	gSavedPerAccountSettings.setString("KeywordsList",      childGetValue("KeywordsList"));
	gSavedPerAccountSettings.setBOOL("KeywordsInChat",      childGetValue("KeywordsInChat"));
	gSavedPerAccountSettings.setBOOL("KeywordsInIM",        childGetValue("KeywordsInIM"));
	gSavedPerAccountSettings.setBOOL("KeywordsChangeColor", childGetValue("KeywordsChangeColor"));
	gSavedPerAccountSettings.setColor4("KeywordsColor",     childGetValue("KeywordsColor"));
	gSavedPerAccountSettings.setBOOL("KeywordsPlaySound",   childGetValue("KeywordsPlaySound"));
	gSavedPerAccountSettings.setString("KeywordsSound",     childGetValue("KeywordsSound"));
}

// Store current settings for cancel
void LLPrefsAscentChat::refreshValues()
{
    //Chat/IM -----------------------------------------------------------------------------
    mIMAnnounceIncoming             = gSavedSettings.getBOOL("AscentInstantMessageAnnounceIncoming");
    mHideTypingNotification         = gSavedSettings.getBOOL("AscentHideTypingNotification");
    mInstantMessagesFriendsOnly     = gSavedSettings.getBOOL("InstantMessagesFriendsOnly");
    mShowGroupNameInChatIM          = gSavedSettings.getBOOL("OptionShowGroupNameInChatIM");
    mShowDisplayNameChanges         = gSavedSettings.getBOOL("ShowDisplayNameChanges");
    mUseTypingBubbles               = gSavedSettings.getBOOL("UseTypingBubbles");
    mPlayTypingSound                = gSavedSettings.getBOOL("PlayTypingSound");
    mHideNotificationsInChat        = gSavedSettings.getBOOL("HideNotificationsInChat");
    mEnableMUPose                   = gSavedSettings.getBOOL("AscentAllowMUpose");
    mEnableOOCAutoClose             = gSavedSettings.getBOOL("AscentAutoCloseOOC");
    mLinksForChattingObjects        = gSavedSettings.getU32("LinksForChattingObjects");
    mSecondsInChatAndIMs            = gSavedSettings.getBOOL("SecondsInChatAndIMs");
    mSecondsInLog                   = gSavedSettings.getBOOL("SecondsInLog");

    std::string format = gSavedSettings.getString("ShortTimeFormat");
    mTimeFormat = format == "%R" ? 0
	    : format == "%I:%M %p" ? 1
	    : -1;

    format = gSavedSettings.getString("ShortDateFormat");
    mDateFormat = format == "%F" ? 0 :
	    format == "%Y/%m/%d" ? 1 :
	    format == "%d/%m/%Y" ? 2 :
	    format == "%m/%d/%Y" ? 3 :
	    -1;

    tempTimeFormat = mTimeFormat;
    tempDateFormat = mDateFormat;

	//Chat UI -----------------------------------------------------------------------------
	mWoLfVerticalIMTabs             = gSavedSettings.getBOOL("WoLfVerticalIMTabs");
	mOtherChatsTornOff              = gSavedSettings.getBOOL("OtherChatsTornOff");
	mIMAnnounceStealFocus           = gSavedSettings.getBOOL("PhoenixIMAnnounceStealFocus");
	mShowLocalChatFloaterBar        = gSavedSettings.getBOOL("ShowLocalChatFloaterBar");
	mHorizButt                      = gSavedSettings.getBOOL("ContactsUseHorizontalButtons");
	mOneLineIMButt                  = gSavedSettings.getBOOL("UseConciseIMButtons");
	mOneLineGroupButt               = gSavedSettings.getBOOL("UseConciseGroupChatButtons");
	mOneLineConfButt                = gSavedSettings.getBOOL("UseConciseConferenceButtons");
	mOnlyComm                       = gSavedSettings.getBOOL("CommunicateSpecificShortcut");
	mLegacyEndScroll                = gSavedSettings.getBOOL("LiruLegacyScrollToEnd");
	mItalicizeActions               = gSavedSettings.getBOOL("LiruItalicizeActions");
	mLegacyLogLaunch                = gSavedSettings.getBOOL("LiruLegacyLogLaunch");
	mChatTabNames                   = gSavedSettings.getS32("IMNameSystem");
	mFriendNames                    = gSavedSettings.getS32("FriendNameSystem");
	mGroupMembersNames              = gSavedSettings.getS32("GroupMembersNameSystem");
	mLandManagementNames            = gSavedSettings.getS32("LandManagementNameSystem");
	mProfileNames			= gSavedSettings.getS32("ProfileNameSystem");
	mRadarNames                     = gSavedSettings.getS32("RadarNameSystem");
	mSpeakerNames                   = gSavedSettings.getS32("SpeakerNameSystem");

	//Autoresponse ------------------------------------------------------------------------
	mIMResponseRepeat            = gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseRepeat");
	mIMResponseAway              = gSavedPerAccountSettings.getBOOL("AutoresponseOnlyIfAway");
	mIMResponseAnyone            = gSavedPerAccountSettings.getBOOL("AutoresponseAnyone");
	mIMResponseAnyoneFriends     = gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneFriendsOnly");
	mIMResponseAnyoneItem        = gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneItem");
	mIMResponseAnyoneItemID      = gSavedPerAccountSettings.getString("AutoresponseAnyoneItemID");
	mIMResponseAnyoneMessage     = gSavedPerAccountSettings.getString("AutoresponseAnyoneMessage");
	mIMResponseAnyoneShow        = gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneShow");
	mIMResponseNonFriends        = gSavedPerAccountSettings.getBOOL("AutoresponseNonFriends");
	mIMResponseNonFriendsItem    = gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsItem");
	mIMResponseNonFriendsItemID  = gSavedPerAccountSettings.getString("AutoresponseNonFriendsItemID");
	mIMResponseNonFriendsMessage = gSavedPerAccountSettings.getString("AutoresponseNonFriendsMessage");
	mIMResponseNonFriendsShow    = gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsShow");
	mIMResponseMuted             = gSavedPerAccountSettings.getBOOL("AutoresponseMuted");
	mIMResponseMutedItem         = gSavedPerAccountSettings.getBOOL("AutoresponseMutedItem");
	mIMResponseMutedItemID       = gSavedPerAccountSettings.getString("AutoresponseMutedItemID");
	mIMResponseMutedMessage      = gSavedPerAccountSettings.getString("AutoresponseMutedMessage");
	mIMResponseMutedShow         = gSavedPerAccountSettings.getBOOL("AutoresponseMutedShow");
	mIMResponseBusyItem          = gSavedPerAccountSettings.getBOOL("BusyModeResponseItem");
	mIMResponseBusyItemID        = gSavedPerAccountSettings.getString("BusyModeResponseItemID");
	mIMResponseBusyMessage       = gSavedPerAccountSettings.getString("BusyModeResponse");
	mIMResponseBusyShow          = gSavedPerAccountSettings.getBOOL("BusyModeResponseShow");

    //Spam --------------------------------------------------------------------------------
	mEnableAS                       = gSavedSettings.getBOOL("AntiSpamEnabled");
    mGlobalQueue                    = gSavedSettings.getBOOL("_NACL_AntiSpamGlobalQueue");
    mChatSpamCount                  = gSavedSettings.getU32("_NACL_AntiSpamAmount");
    mChatSpamTime                   = gSavedSettings.getU32("_NACL_AntiSpamTime");
    mBlockDialogSpam                = gSavedSettings.getBOOL("_NACL_Antispam");
    mBlockAlertSpam                 = gSavedSettings.getBOOL("AntiSpamAlerts");
    mBlockFriendSpam                = gSavedSettings.getBOOL("AntiSpamFriendshipOffers");
    mBlockGroupInviteSpam           = gSavedSettings.getBOOL("AntiSpamGroupInvites");
	mBlockGroupFeeInviteSpam        = gSavedSettings.getBOOL("AntiSpamGroupFeeInvites");
    mBlockGroupNoticeSpam           = gSavedSettings.getBOOL("AntiSpamGroupNotices");
    mBlockItemOfferSpam             = gSavedSettings.getBOOL("AntiSpamItemOffers");
	mBlockNotFriendSpam        = gSavedSettings.getBOOL("AntiSpamNotFriend");
	mBlockNotMineSpam          = gSavedSettings.getBOOL("AntiSpamNotMine");
    mBlockScriptSpam                = gSavedSettings.getBOOL("AntiSpamScripts");
    mBlockTeleportSpam              = gSavedSettings.getBOOL("AntiSpamTeleports");
	mBlockTeleportRequestSpam       = gSavedSettings.getBOOL("AntiSpamTeleportRequests");
    mNotifyOnSpam                   = gSavedSettings.getBOOL("AntiSpamNotify");
    mSoundMulti                     = gSavedSettings.getU32("_NACL_AntiSpamSoundMulti");
    mNewLines                       = gSavedSettings.getU32("_NACL_AntiSpamNewlines");
    mPreloadMulti                   = gSavedSettings.getU32("_NACL_AntiSpamSoundPreloadMulti");
	mEnableGestureSounds            = gSavedSettings.getBOOL("EnableGestureSounds");

    //Text Options ------------------------------------------------------------------------
    mSpellDisplay                   = gSavedSettings.getBOOL("SpellDisplay");
    mKeywordsOn                     = gSavedPerAccountSettings.getBOOL("KeywordsOn");
    mKeywordsList                   = gSavedPerAccountSettings.getString("KeywordsList");
    mKeywordsInChat                 = gSavedPerAccountSettings.getBOOL("KeywordsInChat");
    mKeywordsInIM                   = gSavedPerAccountSettings.getBOOL("KeywordsInIM");
    mKeywordsChangeColor            = gSavedPerAccountSettings.getBOOL("KeywordsChangeColor");
    mKeywordsColor                  = gSavedPerAccountSettings.getColor4("KeywordsColor");
    mKeywordsPlaySound              = gSavedPerAccountSettings.getBOOL("KeywordsPlaySound");
    mKeywordsSound                  = static_cast<LLUUID>(gSavedPerAccountSettings.getString("KeywordsSound"));
}

// Update controls based on current settings
void LLPrefsAscentChat::refresh()
{
	//Chat --------------------------------------------------------------------------------
	// time format combobox
	if (mTimeFormat != -1)
	{
		if (auto combo = getChild<LLComboBox>("time_format_combobox"))
		{
			combo->setCurrentByIndex(mTimeFormat);
		}
	}

	// date format combobox
	if (mDateFormat != -1)
	{
		if (auto combo = getChild<LLComboBox>("date_format_combobox"))
		{
			combo->setCurrentByIndex(mDateFormat);
		}
	}

	//Chat UI -----------------------------------------------------------------------------
	if (auto combo = getChild<LLComboBox>("chat_tabs_namesystem_combobox"))
		combo->setCurrentByIndex(mChatTabNames);
	if (auto combo = getChild<LLComboBox>("friends_namesystem_combobox"))
		combo->setCurrentByIndex(mFriendNames);
	if (auto combo = getChild<LLComboBox>("group_members_namesystem_combobox"))
		combo->setCurrentByIndex(mGroupMembersNames);
	if (auto combo = getChild<LLComboBox>("land_management_namesystem_combobox"))
		combo->setCurrentByIndex(mLandManagementNames);
	if (auto combo = getChild<LLComboBox>("radar_namesystem_combobox"))
		combo->setCurrentByIndex(mRadarNames);
	if (auto combo = getChild<LLComboBox>("speaker_namesystem_combobox"))
		combo->setCurrentByIndex(mSpeakerNames);

    //Text Options ------------------------------------------------------------------------
    if (auto combo = getChild<LLComboBox>("SpellBase"))
    {
        combo->removeall();

        for (const auto& name : glggHunSpell->getDicts())
        {
            combo->add(name);
        }

        combo->setSimple(gSavedSettings.getString("SpellBase"));
    }

    if (auto combo = getChild<LLComboBox>("EmSpell_Avail"))
    {
        combo->removeall();

        combo->add(LLStringUtil::null);

        for (const auto& name : glggHunSpell->getAvailDicts())
        {
            combo->add(name);
        }

        combo->setSimple(LLStringUtil::null);
    }

    if (auto combo = getChild<LLComboBox>("EmSpell_Installed"))
    {
        combo->removeall();

        combo->add(LLStringUtil::null);

        for (const auto& name : glggHunSpell->getInstalledDicts())
        {
            combo->add(name);
        }

        combo->setSimple(LLStringUtil::null);
    }

    childSetEnabled("KeywordsList",        mKeywordsOn);
    childSetEnabled("KeywordsInChat",      mKeywordsOn);
    childSetEnabled("KeywordsInIM",        mKeywordsOn);
    childSetEnabled("KeywordsChangeColor", mKeywordsOn);
    childSetEnabled("KeywordsColor",       mKeywordsOn);
    childSetEnabled("KeywordsPlaySound",   mKeywordsOn);
    childSetEnabled("KeywordsSound",       mKeywordsOn);

    childSetValue("KeywordsOn",          mKeywordsOn);
    childSetValue("KeywordsList",        mKeywordsList);
    childSetValue("KeywordsInChat",      mKeywordsInChat);
    childSetValue("KeywordsInIM",        mKeywordsInIM);
    childSetValue("KeywordsChangeColor", mKeywordsChangeColor);

    LLColorSwatchCtrl* colorctrl = getChild<LLColorSwatchCtrl>("KeywordsColor");
    colorctrl->set(LLColor4(mKeywordsColor),TRUE);

    childSetValue("KeywordsPlaySound",   mKeywordsPlaySound);
    childSetValue("KeywordsSound",       mKeywordsSound);
}

// Reset settings to local copy
void LLPrefsAscentChat::cancel()
{
    //Chat/IM -----------------------------------------------------------------------------
    gSavedSettings.setBOOL("AscentInstantMessageAnnounceIncoming", mIMAnnounceIncoming);
    gSavedSettings.setBOOL("AscentHideTypingNotification",         mHideTypingNotification);
    gSavedSettings.setBOOL("InstantMessagesFriendsOnly",           mInstantMessagesFriendsOnly);
    gSavedSettings.setBOOL("OptionShowGroupNameInChatIM",          mShowGroupNameInChatIM);
    gSavedSettings.setBOOL("ShowDisplayNameChanges",               mShowDisplayNameChanges);
    gSavedSettings.setBOOL("UseTypingBubbles",                     mUseTypingBubbles);
    gSavedSettings.setBOOL("PlayTypingSound",                      mPlayTypingSound);
    gSavedSettings.setBOOL("HideNotificationsInChat",              mHideNotificationsInChat);
    gSavedSettings.setBOOL("AscentAllowMUpose",                    mEnableMUPose);
    gSavedSettings.setBOOL("AscentAutoCloseOOC",                   mEnableOOCAutoClose);
    gSavedSettings.setU32("LinksForChattingObjects",               mLinksForChattingObjects);
    gSavedSettings.setBOOL("SecondsInChatAndIMs",                  mSecondsInChatAndIMs);
    gSavedSettings.setBOOL("SecondsInLog",                         mSecondsInLog);

	setTimeDateFormats(mTimeFormat, mDateFormat);

	//Chat UI -----------------------------------------------------------------------------
	gSavedSettings.setBOOL("WoLfVerticalIMTabs",                   mWoLfVerticalIMTabs);
	gSavedSettings.setBOOL("OtherChatsTornOff",                    mOtherChatsTornOff);
	gSavedSettings.setBOOL("PhoenixIMAnnounceStealFocus",          mIMAnnounceStealFocus);
	gSavedSettings.setBOOL("ShowLocalChatFloaterBar",              mShowLocalChatFloaterBar);
	gSavedSettings.setBOOL("ContactsUseHorizontalButtons",         mHorizButt);
	gSavedSettings.setBOOL("UseConciseIMButtons",                  mOneLineIMButt);
	gSavedSettings.setBOOL("UseConciseGroupChatButtons",           mOneLineGroupButt);
	gSavedSettings.setBOOL("UseConciseConferenceButtons",          mOneLineConfButt);
	gSavedSettings.setBOOL("CommunicateSpecificShortcut",          mOnlyComm);
	gSavedSettings.setBOOL("LiruLegacyScrollToEnd",                mLegacyEndScroll);
	gSavedSettings.setBOOL("LiruItalicizeActions",                 mItalicizeActions);
	gSavedSettings.setBOOL("LiruLegacyLogLaunch",                  mLegacyLogLaunch);
	gSavedSettings.setS32("IMNameSystem",                          mChatTabNames);
	gSavedSettings.setS32("FriendNameSystem",                      mFriendNames);
	gSavedSettings.setS32("GroupMembersNameSystem",                mGroupMembersNames);
	gSavedSettings.setS32("LandManagementNameSystem",              mLandManagementNames);
	gSavedSettings.setS32("ProfileNameSystem",                     mProfileNames);
	gSavedSettings.setS32("RadarNameSystem",                       mRadarNames);
	gSavedSettings.setS32("SpeakerNameSystem",                     mSpeakerNames);

	//Autoresponse ------------------------------------------------------------------------
	gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseRepeat", mIMResponseRepeat);
	gSavedPerAccountSettings.setBOOL("AutoresponseOnlyIfAway",             mIMResponseAway);
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyone",                 mIMResponseAnyone);
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyoneFriendsOnly",      mIMResponseAnyoneFriends);
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyoneItem",             mIMResponseAnyoneItem);
	gSavedPerAccountSettings.setString("AutoresponseAnyoneItemID",         mIMResponseAnyoneItemID);
	gSavedPerAccountSettings.setString("AutoresponseAnyoneMessage",        mIMResponseAnyoneMessage);
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyoneShow",             mIMResponseAnyoneShow);
	gSavedPerAccountSettings.setBOOL("AutoresponseNonFriends",             mIMResponseNonFriends);
	gSavedPerAccountSettings.setBOOL("AutoresponseNonFriendsItem",         mIMResponseNonFriendsItem);
	gSavedPerAccountSettings.setString("AutoresponseNonFriendsItemID",     mIMResponseNonFriendsItemID);
	gSavedPerAccountSettings.setString("AutoresponseNonFriendsMessage",    mIMResponseNonFriendsMessage);
	gSavedPerAccountSettings.setBOOL("AutoresponseNonFriendsShow",         mIMResponseNonFriendsShow);
	gSavedPerAccountSettings.setBOOL("AutoresponseMuted",                  mIMResponseMuted);
	gSavedPerAccountSettings.setBOOL("AutoresponseMutedItem",              mIMResponseMutedItem);
	gSavedPerAccountSettings.setString("AutoresponseMutedItemID",          mIMResponseMutedItemID);
	gSavedPerAccountSettings.setString("AutoresponseMutedMessage",         mIMResponseMutedMessage);
	gSavedPerAccountSettings.setBOOL("AutoresponseMutedShow",              mIMResponseMutedShow);
	gSavedPerAccountSettings.setBOOL("BusyModeResponseItem",               mIMResponseBusyItem);
	gSavedPerAccountSettings.setString("BusyModeResponseItemID",           mIMResponseBusyItemID);
	gSavedPerAccountSettings.setString("BusyModeResponse",                 mIMResponseBusyMessage);
	gSavedPerAccountSettings.setBOOL("BusyModeResponseShow",               mIMResponseBusyShow);

	//Spam --------------------------------------------------------------------------------
	gSavedSettings.setBOOL("AntiSpamEnabled",                mEnableAS);
    gSavedSettings.setBOOL("_NACL_AntiSpamGlobalQueue",      mGlobalQueue);
    gSavedSettings.setU32("_NACL_AntiSpamAmount",            mChatSpamCount);
    gSavedSettings.setU32("_NACL_AntiSpamTime",              mChatSpamTime);
    gSavedSettings.setBOOL("_NACL_Antispam",                 mBlockDialogSpam);
	gSavedSettings.setBOOL("AntiSpamAlerts",                 mBlockAlertSpam);
	gSavedSettings.setBOOL("AntiSpamFriendshipOffers",       mBlockFriendSpam);
	gSavedSettings.setBOOL("AntiSpamGroupNotices",           mBlockGroupNoticeSpam);
	gSavedSettings.setBOOL("AntiSpamGroupInvites",           mBlockGroupInviteSpam);
	gSavedSettings.setBOOL("AntiSpamGroupFeeInvites",		 mBlockGroupFeeInviteSpam);
	gSavedSettings.setBOOL("AntiSpamItemOffers",             mBlockItemOfferSpam);
	gSavedSettings.setBOOL("AntiSpamNotFriend",              mBlockNotFriendSpam);
	gSavedSettings.setBOOL("AntiSpamNotMine",                mBlockNotMineSpam);
	gSavedSettings.setBOOL("AntiSpamScripts",                mBlockScriptSpam);
	gSavedSettings.setBOOL("AntiSpamTeleports",              mBlockTeleportSpam);
	gSavedSettings.setBOOL("AntiSpamTeleportRequests",       mBlockTeleportRequestSpam);
	gSavedSettings.setBOOL("AntiSpamNotify",                 mNotifyOnSpam);
    gSavedSettings.setU32("_NACL_AntiSpamSoundMulti",        mSoundMulti);
    gSavedSettings.setU32("_NACL_AntiSpamNewlines",          mNewLines);
    gSavedSettings.setU32("_NACL_AntiSpamSoundPreloadMulti", mPreloadMulti);
	gSavedSettings.setBOOL("EnableGestureSounds",            mEnableGestureSounds);

    //Text Options ------------------------------------------------------------------------
    gSavedSettings.setBOOL("SpellDisplay",                  mSpellDisplay);
    gSavedPerAccountSettings.setBOOL("KeywordsOn",          mKeywordsOn);
    gSavedPerAccountSettings.setString("KeywordsList",      mKeywordsList);
    gSavedPerAccountSettings.setBOOL("KeywordsInChat",      mKeywordsInChat);
    gSavedPerAccountSettings.setBOOL("KeywordsInIM",        mKeywordsInIM);
    gSavedPerAccountSettings.setBOOL("KeywordsChangeColor", mKeywordsChangeColor);
    gSavedPerAccountSettings.setColor4("KeywordsColor",     mKeywordsColor);
    gSavedPerAccountSettings.setBOOL("KeywordsPlaySound",   mKeywordsPlaySound);
    gSavedPerAccountSettings.setString("KeywordsSound",     mKeywordsSound.asString());
}

// Update local copy so cancel has no effect
void LLPrefsAscentChat::apply()
{
    refreshValues();
    refresh();
}

