/** 
 * @file hbprefsinert.h
 * @brief  Ascent Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Henri Beauchamp.
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

#ifndef ASCENTPREFSCHAT_H
#define ASCENTPREFSCHAT_H


#include "llpanel.h"

class LLPrefsAscentChat : public LLPanel
{
public:
    LLPrefsAscentChat();
    ~LLPrefsAscentChat();

    void apply();
    void cancel();
    void refresh();
    void refreshValues();

protected:
	void onSpellAdd();
	void onSpellRemove();
	void onSpellEditCustom();
	void onSpellBaseComboBoxCommit(const LLSD& value);
	void onCommitTimeDate(LLUICtrl* ctrl);
	void onCommitEnableAS(const LLSD& value);
	void onCommitDialogBlock(LLUICtrl* ctrl, const LLSD& value);
	void onCommitKeywords(LLUICtrl* ctrl);

    //Chat/IM -----------------------------------------------------------------------------
    BOOL mIMAnnounceIncoming;
    BOOL mHideTypingNotification;
    bool mInstantMessagesFriendsOnly;
    BOOL mShowGroupNameInChatIM;
    bool mShowDisplayNameChanges;
    bool mUseTypingBubbles;
    BOOL mPlayTypingSound;
    BOOL mHideNotificationsInChat;
    BOOL mEnableMUPose;
    BOOL mEnableOOCAutoClose;
    U32 mLinksForChattingObjects;
    U32 mTimeFormat;
    U32 mDateFormat;
        U32 tempTimeFormat;
        U32 tempDateFormat;
    BOOL mSecondsInChatAndIMs;
    BOOL mSecondsInLog;

    //Chat UI -----------------------------------------------------------------------------
	bool mWoLfVerticalIMTabs;
	bool mOtherChatsTornOff;
	bool mIMAnnounceStealFocus;
	bool mShowLocalChatFloaterBar;
	bool mHorizButt;
	bool mOneLineIMButt;
	bool mOneLineGroupButt;
	bool mOneLineConfButt;
	bool mOnlyComm;
	bool mItalicizeActions;
	bool mLegacySpeakerNames;

	//Autoresponse ------------------------------------------------------------------------
	std::string mIMResponseAnyoneItemID;
	std::string mIMResponseNonFriendsItemID;
	std::string mIMResponseMutedItemID;
	std::string mIMResponseBusyItemID;

    //Spam --------------------------------------------------------------------------------
    BOOL mEnableAS;
    BOOL mGlobalQueue;
    U32  mChatSpamCount;
    U32  mChatSpamTime;
    BOOL mBlockDialogSpam;
    BOOL mBlockAlertSpam;
    BOOL mBlockFriendSpam;
    BOOL mBlockGroupNoticeSpam;
    BOOL mBlockGroupInviteSpam;
	BOOL mBlockGroupFeeInviteSpam;
    BOOL mBlockItemOfferSpam;
	bool mBlockNotMineSpam;
	bool mBlockNotFriendSpam;
    BOOL mBlockScriptSpam;
    BOOL mBlockTeleportSpam;
	bool mBlockTeleportRequestSpam;
    BOOL mNotifyOnSpam;
    BOOL mSoundMulti;
    U32  mNewLines;
    U32  mPreloadMulti;
	bool mEnableGestureSounds;

    //Text Options ------------------------------------------------------------------------
    BOOL mSpellDisplay;
    BOOL mKeywordsOn;
    std::string mKeywordsList;
    BOOL mKeywordsInChat;
    BOOL mKeywordsInIM;
    BOOL mKeywordsChangeColor;
    LLColor4 mKeywordsColor;
    BOOL mKeywordsPlaySound;
    LLUUID mKeywordsSound;
};

#endif
