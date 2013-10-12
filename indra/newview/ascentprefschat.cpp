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

	// Saved per account settings aren't detected by control_name, therefore autoresponse controls get their values here and have them saved during apply.
	childSetValue("AscentInstantMessageResponseRepeat",  gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseRepeat"));
	childSetValue("AutoresponseAnyone",                  gSavedPerAccountSettings.getBOOL("AutoresponseAnyone"));
	childSetValue("AutoresponseAnyoneFriendsOnly",       gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneFriendsOnly"));
	childSetValue("AutoresponseAnyoneItem",              gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneItem"));
	childSetValue("AutoresponseAnyoneMessage",           gSavedPerAccountSettings.getString("AutoresponseAnyoneMessage"));
	childSetValue("AutoresponseAnyoneShow",              gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneShow"));
	childSetValue("AutoresponseNonFriends",              gSavedPerAccountSettings.getBOOL("AutoresponseNonFriends"));
	childSetValue("AutoresponseNonFriendsItem",          gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsItem"));
	childSetValue("AutoresponseNonFriendsMessage",       gSavedPerAccountSettings.getString("AutoresponseNonFriendsMessage"));
	childSetValue("AutoresponseNonFriendsShow",          gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsShow"));
	childSetValue("AutoresponseMuted",                   gSavedPerAccountSettings.getBOOL("AutoresponseMuted"));
	childSetValue("AutoresponseMutedItem",               gSavedPerAccountSettings.getBOOL("AutoresponseMutedItem"));
	childSetValue("AutoresponseMutedMessage",            gSavedPerAccountSettings.getString("AutoresponseMutedMessage"));
	childSetValue("BusyModeResponse",                    gSavedPerAccountSettings.getString("BusyModeResponse"));
	childSetValue("BusyModeResponseItem",                gSavedPerAccountSettings.getBOOL("BusyModeResponseItem"));
	childSetValue("BusyModeResponseShow",                gSavedPerAccountSettings.getBOOL("BusyModeResponseShow"));

	childSetEnabled("reset_antispam", started);
	getChild<LLUICtrl>("reset_antispam")->setCommitCallback(boost::bind(NACLAntiSpamRegistry::purgeAllQueues));
	getChild<LLUICtrl>("enable_as")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitEnableAS, this, _2));
	getChild<LLUICtrl>("antispam_checkbox")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitDialogBlock, this, _1, _2));
	getChild<LLUICtrl>("Group Invites")->setCommitCallback(boost::bind(&LLPrefsAscentChat::onCommitDialogBlock, this, _1, _2));

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

    std::string short_date, long_date, short_time, long_time, timestamp;

	if (tempTimeFormat == 0)
    {
        short_time = "%H:%M";
        long_time  = "%H:%M:%S";
        timestamp  = " %H:%M:%S";
    }
    else
    {
        short_time = "%I:%M %p";
        long_time  = "%I:%M:%S %p";
        timestamp  = " %I:%M %p";
    }

	if (tempDateFormat == 0)
    {
        short_date = "%Y-%m-%d";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
	else if (tempDateFormat == 1)
    {
        short_date = "%d/%m/%Y";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
    else
    {
        short_date = "%m/%d/%Y";
        long_date  = "%A, %B %d %Y";
        timestamp  = "%a %b %d %Y" + timestamp;
    }

    gSavedSettings.setString("ShortDateFormat", short_date);
    gSavedSettings.setString("LongDateFormat",  long_date);
    gSavedSettings.setString("ShortTimeFormat", short_time);
    gSavedSettings.setString("LongTimeFormat",  long_time);
    gSavedSettings.setString("TimestampFormat", timestamp);
}

void LLPrefsAscentChat::onCommitEnableAS(const LLSD& value)
{
	bool enabled = value.asBoolean();
	childSetEnabled("spammsg_checkbox",          enabled);
	childSetEnabled("antispamtime",              enabled);
	childSetEnabled("antispamamount",            enabled);
	childSetEnabled("antispamsoundmulti",        enabled);
	childSetEnabled("antispamsoundpreloadmulti", enabled);
	childSetEnabled("antispamnewlines",          enabled);
	childSetEnabled("Notify On Spam",            enabled);
}

void LLPrefsAscentChat::onCommitDialogBlock(LLUICtrl* ctrl, const LLSD& value)
{
	childSetEnabled("Group Fee Invites", !childGetValue("antispam_checkbox").asBoolean() && !childGetValue("Group Invites").asBoolean());
	bool enabled = value.asBoolean();
	if (ctrl->getName() == "antispam_checkbox")
	{
		childSetEnabled("Block All Dialogs From", !enabled);
		childSetEnabled("Alerts",                 !enabled);
		childSetEnabled("Friendship Offers",      !enabled);
		childSetEnabled("Group Invites",          !enabled);
		childSetEnabled("Group Notices",          !enabled);
		childSetEnabled("Item Offers",            !enabled);
		childSetEnabled("Scripts",                !enabled);
		childSetEnabled("Teleport Offers",        !enabled);
		childSetEnabled("Teleport Requests",      !enabled);
		childSetEnabled("Except those from:",     !enabled);
		childSetEnabled("My objects",             !enabled);
		childSetEnabled("My friends",             !enabled);
	}
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
    if (format.find("%p") == -1)
    {
        mTimeFormat = 0;
    }
    else
    {
        mTimeFormat = 1;
    }

    format = gSavedSettings.getString("ShortDateFormat");
    if (format.find("%m/%d/%") != -1)
    {
        mDateFormat = 2;
    }
    else if (format.find("%d/%m/%") != -1)
    {
        mDateFormat = 1;
    }
    else
    {
        mDateFormat = 0;
    }

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
	mItalicizeActions               = gSavedSettings.getBOOL("LiruItalicizeActions");
	mLegacySpeakerNames             = gSavedSettings.getBOOL("LiruLegacySpeakerNames");

	//Autoresponse ------------------------------------------------------------------------
	mIMResponseAnyoneItemID     = gSavedPerAccountSettings.getString("AutoresponseAnyoneItemID");
	mIMResponseNonFriendsItemID = gSavedPerAccountSettings.getString("AutoresponseNonFriendsItemID");
	mIMResponseMutedItemID      = gSavedPerAccountSettings.getString("AutoresponseMutedItemID");
	mIMResponseBusyItemID       = gSavedPerAccountSettings.getString("BusyModeResponseItemID");

	gSavedPerAccountSettings.setBOOL("AscentInstantMessageResponseRepeat", childGetValue("AscentInstantMessageResponseRepeat"));
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyone",                 childGetValue("AutoresponseAnyone"));
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyoneFriendsOnly",      childGetValue("AutoresponseAnyoneFriendsOnly"));
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyoneItem",             childGetValue("AutoresponseAnyoneItem"));
	gSavedPerAccountSettings.setString("AutoresponseAnyoneMessage",        childGetValue("AutoresponseAnyoneMessage"));
	gSavedPerAccountSettings.setBOOL("AutoresponseAnyoneShow",             childGetValue("AutoresponseAnyoneShow"));
	gSavedPerAccountSettings.setBOOL("AutoresponseNonFriends",             childGetValue("AutoresponseNonFriends"));
	gSavedPerAccountSettings.setBOOL("AutoresponseNonFriendsItem",         childGetValue("AutoresponseNonFriendsItem"));
	gSavedPerAccountSettings.setString("AutoresponseNonFriendsMessage",    childGetValue("AutoresponseNonFriendsMessage"));
	gSavedPerAccountSettings.setBOOL("AutoresponseNonFriendsShow",         childGetValue("AutoresponseNonFriendsShow"));
	gSavedPerAccountSettings.setBOOL("AutoresponseMuted",                  childGetValue("AutoresponseMuted"));
	gSavedPerAccountSettings.setBOOL("AutoresponseMutedItem",              childGetValue("AutoresponseMutedItem"));
	gSavedPerAccountSettings.setString("AutoresponseMutedMessage",         childGetValue("AutoresponseMutedMessage"));
	gSavedPerAccountSettings.setString("BusyModeResponse",                 childGetValue("BusyModeResponse"));
	gSavedPerAccountSettings.setBOOL("BusyModeResponseItem",               childGetValue("BusyModeResponseItem"));
	gSavedPerAccountSettings.setBOOL("BusyModeResponseShow",               childGetValue("BusyModeResponseShow"));

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
    LLComboBox* combo = getChild<LLComboBox>("time_format_combobox");
    if (combo)
    {
        combo->setCurrentByIndex(mTimeFormat);
    }

    // date format combobox
    combo = getChild<LLComboBox>("date_format_combobox");
    if (combo)
    {
        combo->setCurrentByIndex(mDateFormat);
    }

    //Antispam ------------------------------------------------------------------------
	// sensitivity tuners
	childSetEnabled("spammsg_checkbox",          mEnableAS);
	childSetEnabled("antispamtime",              mEnableAS);
	childSetEnabled("antispamamount",            mEnableAS);
	childSetEnabled("antispamsoundmulti",        mEnableAS);
	childSetEnabled("antispamsoundpreloadmulti", mEnableAS);
	childSetEnabled("antispamnewlines",          mEnableAS);
	childSetEnabled("Notify On Spam",            mEnableAS);
	// dialog blocking tuners
	childSetEnabled("Block All Dialogs From", !mBlockDialogSpam);
	childSetEnabled("Alerts",                 !mBlockDialogSpam);
	childSetEnabled("Friendship Offers",      !mBlockDialogSpam);
	childSetEnabled("Group Invites",          !mBlockDialogSpam);
	childSetEnabled("Group Fee Invites",      !mBlockDialogSpam && !mBlockGroupInviteSpam);
	childSetEnabled("Group Notices",          !mBlockDialogSpam);
	childSetEnabled("Item Offers",            !mBlockDialogSpam);
	childSetEnabled("Scripts",                !mBlockDialogSpam);
	childSetEnabled("Teleport Offers",        !mBlockDialogSpam);

    //Text Options ------------------------------------------------------------------------
    combo = getChild<LLComboBox>("SpellBase");

    if (combo != NULL) 
    {
        combo->removeall();
        std::vector<std::string> names = glggHunSpell->getDicts();

        for (int i = 0; i < (int)names.size(); i++) 
        {
            combo->add(names[i]);
        }

        combo->setSimple(gSavedSettings.getString("SpellBase"));
    }

    combo = getChild<LLComboBox>("EmSpell_Avail");

    if (combo != NULL) 
    {
        combo->removeall();

        combo->add("");
        std::vector<std::string> names = glggHunSpell->getAvailDicts();

        for (int i = 0; i < (int)names.size(); i++) 
        {
            combo->add(names[i]);
        }

        combo->setSimple(std::string(""));
    }

    combo = getChild<LLComboBox>("EmSpell_Installed");

    if (combo != NULL) 
    {
        combo->removeall();

        combo->add("");
        std::vector<std::string> names = glggHunSpell->getInstalledDicts();

        for (int i = 0; i < (int)names.size(); i++) 
        {
            combo->add(names[i]);
        }

        combo->setSimple(std::string(""));
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

    std::string short_date, long_date, short_time, long_time, timestamp;

    if (mTimeFormat == 0)
    {
        short_time = "%H:%M";
        long_time  = "%H:%M:%S";
        timestamp  = " %H:%M:%S";
    }
    else
    {
        short_time = "%I:%M %p";
        long_time  = "%I:%M:%S %p";
        timestamp  = " %I:%M %p";
    }

    if (mDateFormat == 0)
    {
        short_date = "%Y-%m-%d";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
    else if (mDateFormat == 1)
    {
        short_date = "%d/%m/%Y";
        long_date  = "%A %d %B %Y";
        timestamp  = "%a %d %b %Y" + timestamp;
    }
    else
    {
        short_date = "%m/%d/%Y";
        long_date  = "%A, %B %d %Y";
        timestamp  = "%a %b %d %Y" + timestamp;
    }

    gSavedSettings.setString("ShortDateFormat", short_date);
    gSavedSettings.setString("LongDateFormat",  long_date);
    gSavedSettings.setString("ShortTimeFormat", short_time);
    gSavedSettings.setString("LongTimeFormat",  long_time);
    gSavedSettings.setString("TimestampFormat", timestamp);

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
	gSavedSettings.setBOOL("LiruItalicizeActions",                 mItalicizeActions);
	gSavedSettings.setBOOL("LiruLegacySpeakerNames",               mLegacySpeakerNames);

	//Autoresponse ------------------------------------------------------------------------
	gSavedPerAccountSettings.setString("AutoresponseAnyoneItemID",      mIMResponseAnyoneItemID);
	gSavedPerAccountSettings.setString("AutoresponseNonFriendsItemID",  mIMResponseNonFriendsItemID);
	gSavedPerAccountSettings.setString("AutoresponseMutedItemID",       mIMResponseMutedItemID);
	gSavedPerAccountSettings.setString("BusyModeResponseItemID",        mIMResponseBusyItemID);

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

