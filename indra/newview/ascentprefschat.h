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
	void onCommitKeywords(LLUICtrl* ctrl);

private:
	//Chat/IM -----------------------------------------------------------------------------
	bool mIMAnnounceIncoming;
	bool mHideTypingNotification;
	bool mInstantMessagesFriendsOnly;
	bool mShowGroupNameInChatIM;
	bool mShowDisplayNameChanges;
	bool mUseTypingBubbles;
	bool mPlayTypingSound;
	bool mHideNotificationsInChat;
	bool mEnableMUPose;
	bool mEnableOOCAutoClose;
	U32 mLinksForChattingObjects;
	S8 mTimeFormat;
	S8 mDateFormat;
	S8 tempTimeFormat;
	S8 tempDateFormat;
	bool mSecondsInChatAndIMs;
	bool mSecondsInLog;

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
	bool mLegacyEndScroll;
	bool mItalicizeActions;
	bool mLegacyLogLaunch;
	S32 mChatTabNames;
	S32 mFriendNames;
	S32 mGroupMembersNames;
	S32 mLandManagementNames;
	S32 mProfileNames;
	S32 mRadarNames;
	S32 mSpeakerNames;

	//Autoresponse ------------------------------------------------------------------------
	bool mIMResponseRepeat;
	bool mIMResponseAway;
	bool mIMResponseAnyone;
	bool mIMResponseAnyoneFriends;
	bool mIMResponseAnyoneItem;
	std::string mIMResponseAnyoneItemID;
	std::string mIMResponseAnyoneMessage;
	bool mIMResponseAnyoneShow;
	bool mIMResponseNonFriends;
	bool mIMResponseNonFriendsItem;
	std::string mIMResponseNonFriendsItemID;
	std::string mIMResponseNonFriendsMessage;
	bool mIMResponseNonFriendsShow;
	bool mIMResponseMuted;
	bool mIMResponseMutedItem;
	std::string mIMResponseMutedItemID;
	std::string mIMResponseMutedMessage;
	bool mIMResponseMutedShow;
	bool mIMResponseBusyItem;
	std::string mIMResponseBusyItemID;
	std::string mIMResponseBusyMessage;
	bool mIMResponseBusyShow;

	//Spam --------------------------------------------------------------------------------
	bool mEnableAS;
	bool mGlobalQueue;
	U32  mChatSpamCount;
	U32  mChatSpamTime;
	bool mBlockDialogSpam;
	bool mBlockAlertSpam;
	bool mBlockFriendSpam;
	bool mBlockGroupNoticeSpam;
	bool mBlockGroupInviteSpam;
	bool mBlockGroupFeeInviteSpam;
	bool mBlockItemOfferSpam;
	bool mBlockNotMineSpam;
	bool mBlockNotFriendSpam;
	bool mBlockScriptSpam;
	bool mBlockTeleportSpam;
	bool mBlockTeleportRequestSpam;
	bool mNotifyOnSpam;
	bool mSoundMulti;
	U32  mNewLines;
	U32  mPreloadMulti;
	bool mEnableGestureSounds;

	//Text Options ------------------------------------------------------------------------
	bool mSpellDisplay;
	bool mKeywordsOn;
	std::string mKeywordsList;
	bool mKeywordsInChat;
	bool mKeywordsInIM;
	bool mKeywordsChangeColor;
	LLColor4 mKeywordsColor;
	bool mKeywordsPlaySound;
	LLUUID mKeywordsSound;
};

#endif
