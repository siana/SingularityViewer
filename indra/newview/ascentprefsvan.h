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

#ifndef ASCENTPREFSVAN_H
#define ASCENTPREFSVAN_H

#include "llpanel.h"


class LLPrefsAscentVan : public LLPanel
{
public:
    LLPrefsAscentVan();
    ~LLPrefsAscentVan();

    void apply();
    void cancel();
    void refresh();
    void refreshValues();

protected:
	void onCommitClientTag(LLUICtrl* ctrl);
	void onCommitCheckBox(LLUICtrl* ctrl, const LLSD& value);
	void onCommitTextModified(LLUICtrl* ctrl, const LLSD& value);
	static void onManualClientUpdate();
    //Main
    BOOL mUseAccountSettings;
    BOOL mShowTPScreen;
    BOOL mPlayTPSound;
    BOOL mShowLogScreens;
	bool mDisableChatAnimation;
	bool mAddNotReplace;
	bool mTurnAround;
	bool mAnnounceSnapshots;
	bool mAnnounceStreamMetadata;
	bool mUnfocusedFloatersOpaque;
	bool mCompleteNameProfiles;
	bool mScriptErrorsStealFocus;
    //Tags\Colors
    BOOL mAscentBroadcastTag;
    std::string mReportClientUUID;
    U32 mSelectedClient;
    BOOL mShowSelfClientTag;
    BOOL mShowSelfClientTagColor;
    BOOL mShowFriendsTag;
    BOOL mDisplayClientTagOnNewLine;
    BOOL mCustomTagOn;
    std::string mCustomTagLabel;
    LLColor4 mCustomTagColor;
    BOOL mShowOthersTag;
    BOOL mShowOthersTagColor;
    BOOL mShowIdleTime;
    BOOL mUseStatusColors;
    BOOL mUpdateTagsOnLoad;
    LLColor4 mEffectColor;
    LLColor4 mFriendColor;
    LLColor4 mEstateOwnerColor;
    LLColor4 mLindenColor;
    LLColor4 mMutedColor;
	LLColor4 mMapAvatarColor;
	LLColor4 mCustomColor;
	bool mColorFriendChat;
	bool mColorEOChat;
	bool mColorLindenChat;
	bool mColorMutedChat;
//	bool mColorCustomChat;

    F32 mAvatarXModifier;
    F32 mAvatarYModifier;
    F32 mAvatarZModifier;

private:

};

#endif
