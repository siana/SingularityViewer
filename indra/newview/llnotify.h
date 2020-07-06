/** 
 * @file llnotify.h
 * @brief Non-blocking notification that doesn't take keyboard focus.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLNOTIFY_H
#define LL_LLNOTIFY_H

#include "llfontgl.h"
#include "llpanel.h"
#include "lleventtimer.h"
#include "llnotifications.h"

class LLButton;
class LLNotifyBoxTemplate;
class LLTextEditor;

// NotifyBox - for notifications that require a response from the user.  
class LLNotifyBox final :
	public LLPanel, 
	public LLEventTimer,
	public LLInitClass<LLNotifyBox>,
	public LLInstanceTracker<LLNotifyBox, LLUUID>
{
public:
	static void initClass();

	bool isTip() const { return mIsTip; }
	bool isCaution() const { return mIsCaution; }
	/*virtual*/ void setVisible(BOOL visible) override;
	void stopAnimation() { mAnimating = false; }

	void close();

	LLNotificationPtr getNotification() const { return mNotification; }

	static void format(std::string& msg, const LLStringUtil::format_map_t& args);

protected:
	LLNotifyBox(LLNotificationPtr notification);

	/*virtual*/ ~LLNotifyBox();

	LLButton* addButton(const std::string& name, const std::string& label, bool is_option, bool is_default, bool layout_script_dialog);
	
	/*virtual*/ BOOL handleMouseUp(S32 x, S32 y, MASK mask) override;
	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL handleHover(S32 x, S32 y, MASK mask) override;
	bool userIsInteracting() const;

	// Animate as sliding onto the screen.
	/*virtual*/ void draw() override;
	/*virtual*/ BOOL tick() override;

	void moveToBack(bool getfocus = false);

	// Returns the rect, relative to gNotifyView, where this
	// notify box should be placed.
	static LLRect getNotifyRect(S32 num_options, bool layout_script_dialog, bool is_caution);
	static LLRect getNotifyTipRect(const std::string &message);

	// internal handler for button being clicked
	void onClickButton(const std::string name);

private:
	static bool onNotification(const LLSD& notify);
	void drawBackground() const;

protected:
	LLTextEditor *mUserInputBox = nullptr, *mText = nullptr;

	LLNotificationPtr mNotification;
	bool mIsTip;
	bool mIsCaution; // is this a caution notification?
	bool mAnimating; // Are we sliding onscreen?

	// Time since this notification was displayed.
	// This is an LLTimer not a frame timer because I am concerned
	// that I could be out-of-sync by one frame in the animation.
	LLTimer mAnimateTimer;

	LLButton* mNextBtn;

	S32 mNumOptions;
	S32 mNumButtons;
	bool mAddedDefaultBtn;
};

class LLNotifyBoxView final : public LLUICtrl
{
public:
	LLNotifyBoxView(const std::string& name, const LLRect& rect, BOOL mouse_opaque, U32 follows=FOLLOWS_NONE);
	void showOnly(LLView* ctrl);
	LLNotifyBox* getFirstNontipBox() const;
	/*virtual*/ void deleteAllChildren() override;

	struct Matcher
	{
		Matcher(){}
		virtual ~Matcher() {}
		virtual bool matches(const LLNotificationPtr) const = 0;
	};
	// Walks the list and removes any stacked messages for which the given matcher returns TRUE.
	// Useful when muting people and things in order to clear out any similar previously queued messages.
	void purgeMessagesMatching(const Matcher& matcher);

private:
	bool isGroupNotifyBox(const LLView* view) const;
};

// This view contains the stack of notification windows.
extern LLNotifyBoxView* gNotifyBoxView;

#endif
