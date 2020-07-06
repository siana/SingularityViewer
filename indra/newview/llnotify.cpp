/** 
 * @file llnotify.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llnotify.h"

#include "llchat.h"

#include "lliconctrl.h"
#include "llmenugl.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltrans.h"
#include "lluiconstants.h"
#include "llviewerdisplay.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h" // for gViewerWindow
#include "llfloaterchat.h"	// for add_chat_history()
#include "lloverlaybar.h" // for gOverlayBar
#include "lluictrlfactory.h"
#include "llcheckboxctrl.h"

#include "hippogridmanager.h"

// [RLVa:KB] - Version: 1.23.4
#include "rlvhandler.h"
// [/RLVa:KB]

// Globals
LLNotifyBoxView* gNotifyBoxView = NULL;

const F32 ANIMATION_TIME = 0.333f;
const S32 BOTTOM_PAD = VPAD * 3;


// statics
S32 sNotifyBoxCount = 0;
static const LLFontGL* sFont = NULL;

void chat_notification(const LLNotificationPtr notification)
{
	// TODO: Make a separate archive for these.
	if (gSavedSettings.getBOOL("HideNotificationsInChat")) return;
	LLChat chat(notification->getMessage());
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0e) | Added: RLVa-0.2.0b
	// Notices should already have their contents filtered where necessary
	if (rlv_handler_t::isEnabled())
		chat.mRlvLocFiltered = chat.mRlvNamesFiltered = true;
// [/RLVa:KB]
	LLFloaterChat::getInstance()->addChatHistory(chat);
}

//---------------------------------------------------------------------------
// LLNotifyBox
//---------------------------------------------------------------------------

//static 
void LLNotifyBox::initClass()
{
	sFont = LLFontGL::getFontSansSerif();
	LLNotificationChannel::buildChannel("Notifications", "Visible", LLNotificationFilters::filterBy<std::string>(&LLNotification::getType, "notify"));
	LLNotificationChannel::buildChannel("NotificationTips", "Visible", LLNotificationFilters::filterBy<std::string>(&LLNotification::getType, "notifytip"));

	LLNotifications::instance().getChannel("Notifications")->connectChanged(&LLNotifyBox::onNotification);
	LLNotifications::instance().getChannel("NotificationTips")->connectChanged(&LLNotifyBox::onNotification);
}

//static 
bool LLNotifyBox::onNotification(const LLSD& notify)
{
	LLNotificationPtr notification = LLNotifications::instance().find(notify["id"].asUUID());
	
	if (!notification) return false;

	if (notify["sigtype"].asString() == "add" || notify["sigtype"].asString() == "change")
	{
		if (notification->getPayload().has("SUPPRESS_TOAST"))
		{
			chat_notification(notification);
			return false;
		}
		//bring existing notification to top
		//This getInstance is ugly, as LLNotifyBox is derived from both LLInstanceTracker and LLEventTimer, which also is derived from its own LLInstanceTracker
		//Have to explicitly determine which getInstance function to use.
		LLNotifyBox* boxp = LLInstanceTracker<LLNotifyBox, LLUUID>::getInstance(notification->getID());
		if (boxp && !boxp->isDead())
		{
			gNotifyBoxView->showOnly(boxp);
		}
		else
		{
			gNotifyBoxView->addChild(new LLNotifyBox(notification));
		}
	}
	else if (notify["sigtype"].asString() == "delete")
	{
		LLNotifyBox* boxp = LLInstanceTracker<LLNotifyBox, LLUUID>::getInstance(notification->getID());
		if (boxp && !boxp->isDead())
		{
			boxp->close();
		}
	}

	return false;
}

//---------------------------------------------------------------------------
// Singu Note: We could clean a lot of this up by creating derived classes for Notifications and NotificationTips.
LLNotifyBox::LLNotifyBox(LLNotificationPtr notification)
	:	LLPanel(notification->getName(), LLRect(), BORDER_NO),
		LLEventTimer(notification->getExpiration() == LLDate() 
			? LLDate(LLDate::now().secondsSinceEpoch() + (F64)gSavedSettings.getF32("NotifyTipDuration")) 
			: notification->getExpiration()),
		LLInstanceTracker<LLNotifyBox, LLUUID>(notification->getID()),
	  mNotification(notification),
	  mIsTip(notification->getType() == "notifytip"),
	  mAnimating(gNotifyBoxView->getChildCount() == 0), // Only animate first window
	  mNextBtn(NULL),
	  mNumOptions(0),
	  mNumButtons(0),
	  mAddedDefaultBtn(false),
	  mUserInputBox(NULL)
{
	std::string edit_text_name;
	std::string edit_text_contents;

	// setup paramaters
	const std::string& message(notification->getMessage());

	// initialize
	setFocusRoot(!mIsTip);

	// caution flag can be set explicitly by specifying it in the
	// notification payload, or it can be set implicitly if the
	// notify xml template specifies that it is a caution
	//
	// tip-style notification handle 'caution' differently -
	// they display the tip in a different color
	mIsCaution = notification->getPriority() >= NOTIFICATION_PRIORITY_HIGH;

	LLNotificationFormPtr form(notification->getForm());

	mNumOptions = form->getNumElements();
		  
	bool is_textbox = form->getElement("message").isDefined();

	bool layout_script_dialog(notification->getName() == "ScriptDialog" || notification->getName() == "ScriptDialogGroup");
	LLRect rect = mIsTip ? getNotifyTipRect(message)
		   		  		 : getNotifyRect(is_textbox ? 10 : mNumOptions, layout_script_dialog, mIsCaution);
	if ((form->getIgnoreType() == LLNotificationForm::IGNORE_WITH_DEFAULT_RESPONSE || form->getIgnoreType() == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE))
		rect.mBottom -= BTN_HEIGHT;
	setRect(rect);
	setFollows(mIsTip ? (FOLLOWS_BOTTOM|FOLLOWS_RIGHT) : (FOLLOWS_TOP|FOLLOWS_RIGHT));
	setBackgroundVisible(FALSE);
	setBackgroundOpaque(TRUE);

	const S32 TOP = getRect().getHeight() - (mIsTip ? (S32)sFont->getLineHeight() : 32);
	const S32 BOTTOM = (S32)sFont->getLineHeight();
	S32 x = HPAD + HPAD;
	S32 y = TOP;

	auto icon = new LLIconCtrl(std::string("icon"), LLRect(x, y, x+32, TOP-32), mIsTip ? "notify_tip_icon.tga" : mIsCaution ? "notify_caution_icon.tga" : "notify_box_icon.tga");

	icon->setMouseOpaque(FALSE);
	addChild(icon);

	x += HPAD + HPAD + 32;

	{
		const S32 BTN_TOP = BOTTOM_PAD + (((mNumOptions-1+2)/3)) * (BTN_HEIGHT+VPAD);

		// Tokenization on \n is handled by LLTextBox

		const S32 MAX_LENGTH = 512 + 20 + DB_FIRST_NAME_BUF_SIZE + DB_LAST_NAME_BUF_SIZE + DB_INV_ITEM_NAME_BUF_SIZE;  // For script dialogs: add space for title.
		const auto height = mIsTip ? BOTTOM : BTN_TOP+16;

		mText = new LLTextEditor(std::string("box"), LLRect(x, y, getRect().getWidth()-2, height), MAX_LENGTH, LLStringUtil::null, sFont, FALSE, true);

		mText->setWordWrap(TRUE);
		mText->setMouseOpaque(TRUE);
		mText->setBorderVisible(FALSE);
		mText->setTakesNonScrollClicks(TRUE);
		mText->setHideScrollbarForShortDocs(TRUE);
		mText->setReadOnlyBgColor ( LLColor4::transparent ); // the background color of the box is manually 
															// rendered under the text box, therefore we want 
															// the actual text box to be transparent

		auto text_color = gColors.getColor(mIsCaution ? "NotifyCautionWarnColor" : "NotifyTextColor");
		LLStyleSP style = new LLStyle(true, text_color, LLStringUtil::null);
		style->mBold = mIsCaution && !mIsTip;

		mText->setReadOnlyFgColor(text_color); //sets caution text color for tip notifications
		if (!mIsCaution || !mIsTip) // We could do some extra color math here to determine if bg's too close to link color, but let's just cross with the link color instead
			mText->setLinkColor(new LLColor4(lerp(text_color, gSavedSettings.getColor4("HTMLLinkColor"), 0.4f)));
		mText->setTabStop(FALSE); // can't tab to it (may be a problem for scrolling via keyboard)
		mText->appendText(message,false,false,style); // Now we can set the text, since colors have been set.
		if (is_textbox || layout_script_dialog)
			mText->appendText(notification->getSubstitutions()["SCRIPT_MESSAGE"], false, true, style, false);
		addChild(mText);
	}

	if (mIsTip)
	{
		chat_notification(mNotification);
	}
	else
	{
		mNextBtn = new LLButton(std::string("next"),
						   LLRect(getRect().getWidth()-26, BOTTOM_PAD + 20, getRect().getWidth()-2, BOTTOM_PAD),
						   std::string("notify_next.png"),
						   std::string("notify_next.png"),
						   LLStringUtil::null,
						   boost::bind(&LLNotifyBox::moveToBack, this, true),
						   sFont);
		mNextBtn->setScaleImage(TRUE);
		mNextBtn->setToolTip(LLTrans::getString("next"));
		addChild(mNextBtn);

		for (S32 i = 0; i < mNumOptions; i++)
		{
			LLSD form_element = form->getElement(i);
			std::string element_type = form_element["type"].asString();
			if (element_type == "button") 
			{
				addButton(form_element["name"].asString(), form_element["text"].asString(), TRUE, form_element["default"].asBoolean(), layout_script_dialog);
			}
			else if (element_type == "input") 
			{
				edit_text_contents = form_element["value"].asString();
				edit_text_name = form_element["name"].asString();
			}
		}

		if (is_textbox)
		{
			S32 button_rows = layout_script_dialog ? 2 : 1;

			LLRect input_rect;
			input_rect.setOriginAndSize(x, BOTTOM_PAD + button_rows * (BTN_HEIGHT + VPAD),
										3 * 80 + 4 * HPAD, button_rows * (BTN_HEIGHT + VPAD) + BTN_HEIGHT);

			mUserInputBox = new LLTextEditor(edit_text_name, input_rect, 254,
											 edit_text_contents, sFont, FALSE);
			mUserInputBox->setBorderVisible(TRUE);
			mUserInputBox->setTakesNonScrollClicks(TRUE);
			mUserInputBox->setHideScrollbarForShortDocs(TRUE);
			mUserInputBox->setWordWrap(TRUE);
			mUserInputBox->setTabsToNextField(FALSE);
			mUserInputBox->setCommitOnFocusLost(FALSE);
			mUserInputBox->setAcceptCallingCardNames(FALSE);
			mUserInputBox->setHandleEditKeysDirectly(TRUE);

			addChild(mUserInputBox, -1);
		}
		else
		{
			setIsChrome(TRUE);
		}

		if (mNumButtons == 0)
		{
			addButton("OK", "OK", false, true, layout_script_dialog);
			mAddedDefaultBtn = true;
		}

		std::string check_title;
		if (form->getIgnoreType() == LLNotificationForm::IGNORE_WITH_DEFAULT_RESPONSE)
		{
			check_title = LLNotificationTemplates::instance().getGlobalString("skipnexttime");
		}
		else if (form->getIgnoreType() == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
		{
			check_title = LLNotificationTemplates::instance().getGlobalString("alwayschoose");
		}
		if (!check_title.empty())
		{
			const LLFontGL* font = LLResMgr::getInstance()->getRes(LLFONT_SANSSERIF);
			S32 line_height = llfloor(font->getLineHeight() + 0.99f);

			// Extend dialog for "check next time"
			S32 max_msg_width = getRect().getWidth() - HPAD * 9;
			S32 check_width = S32(font->getWidth(check_title) + 0.99f) + 16;
			max_msg_width = llmax(max_msg_width, check_width);

			S32 msg_x = (getRect().getWidth() - max_msg_width) / 2;

			LLRect check_rect;
			check_rect.setOriginAndSize(msg_x, BOTTOM_PAD + BTN_HEIGHT + VPAD*2 + (BTN_HEIGHT + VPAD) * (mNumButtons / 3),
				max_msg_width, line_height);

			LLCheckboxCtrl* check = new LLCheckboxCtrl(std::string("check"), check_rect, check_title, font,
				// Lambda abuse.
				[this](LLUICtrl* ctrl, const LLSD& param)
				{
						this->mNotification->setIgnored(ctrl->getValue());
				});
			check->setEnabledColor(LLUI::sColorsGroup->getColor(mIsCaution ? "AlertCautionTextColor" : "AlertTextColor"));
			if (mIsCaution)
			{
				check->setButtonColor(LLUI::sColorsGroup->getColor("ButtonCautionImageColor"));
			}
			addChild(check);
		}
		
		if (++sNotifyBoxCount <= 0)
			LL_WARNS() << "A notification was mishandled. sNotifyBoxCount = " << sNotifyBoxCount << LL_ENDL;
		// If this is the only notify box, don't show the next button
		else if (sNotifyBoxCount == 1 && mNextBtn)
			mNextBtn->setVisible(false);
	}
}

// virtual
LLNotifyBox::~LLNotifyBox()
{
}

// virtual
LLButton* LLNotifyBox::addButton(const std::string& name, const std::string& label, bool is_option, bool is_default, bool layout_script_dialog)
{
	// make caution notification buttons slightly narrower
	// so that 3 of them can fit without overlapping the "next" button
	S32 btn_width = (mIsCaution || mNumOptions >= 3) ? 84 : 90;

	LLRect btn_rect;
	S32 btn_height= BTN_HEIGHT;
	const LLFontGL* font = sFont;
	S32 ignore_pad = 0;
	S32 button_index = mNumButtons;
	S32 index = button_index;
	S32 x = (HPAD * 4) + 32;

	if (layout_script_dialog)
	{
		// Add one "blank" option space, before the "Block" and "Ignore" buttons
		index = button_index + 1;
		if (button_index == 0 || button_index == 1)
		{
			// Ignore button is smaller, less wide
			btn_height = BTN_HEIGHT_SMALL;
			static const LLFontGL* sFontSmall = LLFontGL::getFontSansSerifSmall();
			font = sFontSmall;
			ignore_pad = 10;
		}
	}

	btn_rect.setOriginAndSize(x + (index % 3) * (btn_width+HPAD+HPAD) + ignore_pad,
		BOTTOM_PAD + (index / 3) * (BTN_HEIGHT+VPAD),
		btn_width - 2*ignore_pad,
		btn_height);

	LLButton* btn = new LLButton(name, btn_rect, "", boost::bind(&LLNotifyBox::onClickButton, this, is_option ? name : ""));
	btn->setLabel(label);
	btn->setToolTip(label);
	btn->setFont(font);

	if (mIsCaution)
	{
		btn->setImageColor(LLUI::sColorsGroup->getColor("ButtonCautionImageColor"));
		btn->setDisabledImageColor(LLUI::sColorsGroup->getColor("ButtonCautionImageColor"));
	}

	addChild(btn, -1);

	if (is_default)
		setDefaultBtn(btn);

	mNumButtons++;
	return btn;
}

BOOL LLNotifyBox::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool done = LLPanel::handleMouseUp(x, y, mask);
	if (!done && mIsTip)
	{
		mNotification->respond(mNotification->getResponseTemplate(LLNotification::WITH_DEFAULT_BUTTON));

		close();
		return TRUE;
	}

	setFocus(TRUE);

	return done;
}

// virtual
BOOL LLNotifyBox::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (!LLPanel::handleRightMouseDown(x, y, mask)) // Allow Children to handle first
		moveToBack(true);
	return true;
}

// virtual
BOOL LLNotifyBox::handleHover(S32 x, S32 y, MASK mask)
{
	if (mIsTip) mEventTimer.stop(); // Stop timer on hover so the user can interact
	return LLPanel::handleHover(x, y, mask);
}

bool LLNotifyBox::userIsInteracting() const
{
	// If the mouse is over us, the user may wish to interact
	S32 local_x;
	S32 local_y;
	screenPointToLocal(gViewerWindow->getCurrentMouseX(), gViewerWindow->getCurrentMouseY(), &local_x, &local_y);
	return pointInView(local_x, local_y) || // We're actively hovered
		// our text is the target of an active menu that could be open (getVisibleMenu sucks because it contains a loop of two dynamic casts, so keep this at the end)
		(mText && mText->getActive<LLTextEditor>() == mText && LLMenuGL::sMenuContainer->getVisibleMenu());
}

// virtual
void LLNotifyBox::draw()
{
	// If we are teleporting, stop the timer and restart it when the teleporting completes
	if (gTeleportDisplay)
		mEventTimer.stop();
	else if (!mEventTimer.getStarted() && (!mIsTip || !userIsInteracting())) // If it's not a tip, we can resume instantly, otherwise the user may be interacting
		mEventTimer.start();

	F32 display_time = mAnimateTimer.getElapsedTimeF32();

	if (mAnimating && display_time < ANIMATION_TIME)
	{
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		LLUI::pushMatrix();

		S32 height = getRect().getHeight();
		F32 fraction = display_time / ANIMATION_TIME;
		F32 voffset = (1.f - fraction) * height;
		if (mIsTip) voffset *= -1.f;
		LLUI::translate(0.f, voffset, 0.f);
		
		drawBackground();
		LLPanel::draw();

		LLUI::popMatrix();
	}
	else
	{
		if (mAnimating)
		{
			mAnimating = false;
			if (!mIsTip) 
				// hide everyone behind me once I'm done animating
				gNotifyBoxView->showOnly(this);
		}
		drawBackground();
		LLPanel::draw();
	}
}

void LLNotifyBox::drawBackground() const
{
	if (LLUIImagePtr imagep = LLUI::getUIImage("Rounded_Square"))
	{
		gGL.getTexUnit(0)->bind(imagep->getImage());
		// set proper background color depending on whether notify box is a caution or not
		bool has_focus(gFocusMgr.childHasKeyboardFocus(this));
		if (has_focus)
		{
			const S32 focus_width = 2;
			static const LLCachedControl<LLColor4> sBorder(gColors, "FloaterFocusBorderColor");
			LLColor4 color = sBorder;
			gGL.color4fv(color.mV);
			gl_segmented_rect_2d_tex(-focus_width, getRect().getHeight() + focus_width, 
									getRect().getWidth() + focus_width, -focus_width,
									imagep->getTextureWidth(), imagep->getTextureHeight(),
									16, mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM);
			static const LLCachedControl<LLColor4> sDropShadow(gColors, "ColorDropShadow");
			color = sDropShadow;
			gGL.color4fv(color.mV);
			gl_segmented_rect_2d_tex(0, getRect().getHeight(), getRect().getWidth(), 0, imagep->getTextureWidth(), imagep->getTextureHeight(), 16, mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM);
		}

		static const LLCachedControl<LLColor4> sCautionColor(gColors, "NotifyCautionBoxColor");
		static const LLCachedControl<LLColor4> sColor(gColors, "NotifyBoxColor");
		LLColor4 color = mIsCaution ? sCautionColor : sColor;
		gGL.color4fv(color.mV);
		gl_segmented_rect_2d_tex(has_focus, getRect().getHeight()-has_focus, getRect().getWidth()-has_focus, has_focus, imagep->getTextureWidth(), imagep->getTextureHeight(), 16, mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM);
	}
}


void LLNotifyBox::close()
{
	bool not_tip = !mIsTip;
	die();
	if (not_tip)
	{
		--sNotifyBoxCount;
		if (LLNotifyBox* front = gNotifyBoxView->getFirstNontipBox())
		{
			gNotifyBoxView->showOnly(front);
			// we're assuming that close is only called by user action (for non-tips), so we then give focus to the next close button
			if (LLView* view = front->getDefaultButton())
				view->setFocus(true);
			gFocusMgr.triggerFocusFlash(); // TODO it's ugly to call this here
		}
	}
}

void LLNotifyBox::format(std::string& msg, const LLStringUtil::format_map_t& args)
{
	// add default substitutions
	LLStringUtil::format_map_t targs = args;
	const LLStringUtil::format_map_t& default_args = LLTrans::getDefaultArgs();
	for (LLStringUtil::format_map_t::const_iterator iter = default_args.begin();
		 iter != default_args.end(); ++iter)
	{
		targs[iter->first] = iter->second;
	}

	LLStringUtil::format(msg, targs);
}


/*virtual*/
BOOL LLNotifyBox::tick()
{
	if (mIsTip)
	{
		LLNotifications::instance().cancel(mNotification);
		close();
	}
	return FALSE;
}

void LLNotifyBox::setVisible(BOOL visible)
{
	// properly set the status of the next button
	if (visible && !mIsTip)
		mNextBtn->setVisible(sNotifyBoxCount > 1);
	LLPanel::setVisible(visible);
}

void LLNotifyBox::moveToBack(bool getfocus)
{
	// Move this dialog to the back.
	gNotifyBoxView->sendChildToBack(this);
	if (!mIsTip && mNextBtn)
	{
		mNextBtn->setVisible(false);

		// And enable the next button on the frontmost one, if there is one
		if (gNotifyBoxView->getChildCount())
			if (LLNotifyBox* front = gNotifyBoxView->getFirstNontipBox())
			{
				gNotifyBoxView->showOnly(front);
				if (getfocus)
				{
					// if are called from a user interaction
					// we give focus to the next next button
					if (front->mNextBtn)
						front->mNextBtn->setFocus(true);
					gFocusMgr.triggerFocusFlash(); // TODO: it's ugly to call this here
				}
			}
	}
}


// static
LLRect LLNotifyBox::getNotifyRect(S32 num_options, bool layout_script_dialog, bool is_caution)
{
	S32 notify_height = gSavedSettings.getS32("NotifyBoxHeight");
	if (is_caution)
	{
		// make caution-style dialog taller to accomodate extra text,
		// as well as causing the accept/decline buttons to be drawn
		// in a different position, to help prevent "quick-click-through"
		// of many permissions prompts
		notify_height = gSavedSettings.getS32("PermissionsCautionNotifyBoxHeight");
	}
	const S32 NOTIFY_WIDTH = gSavedSettings.getS32("NotifyBoxWidth");

	const S32 TOP = gNotifyBoxView->getRect().getHeight();
	const S32 RIGHT = gNotifyBoxView->getRect().getWidth();
	const S32 LEFT = RIGHT - NOTIFY_WIDTH;

	if (num_options < 1)
		num_options = 1;

	// Add one "blank" option space.
	if (layout_script_dialog)
		num_options += 1;

	S32 additional_lines = (num_options-1) / 3;

	notify_height += additional_lines * (BTN_HEIGHT + VPAD);

	return LLRect(LEFT, TOP, RIGHT, TOP-notify_height);
}

// static
LLRect LLNotifyBox::getNotifyTipRect(const std::string &utf8message)
{
	LLWString message = utf8str_to_wstring(utf8message);
	S32 message_len = message.length();

	const S32 NOTIFY_WIDTH = gSavedSettings.getS32("NotifyBoxWidth");
	// Make room for the icon area.
	const S32 text_area_width = NOTIFY_WIDTH - HPAD * 4 - 32;

	const llwchar* wchars = message.c_str();
	const llwchar* start = wchars;
	const llwchar* end;
	S32 total_drawn = 0;
	bool done = false;
	S32 line_count;

	for (line_count = 2; !done; ++line_count)
	{
		for (end = start; *end != 0 && *end != '\n'; end++);

		if (*end == 0)
		{
			end = wchars + message_len;
			done = true;
		}
		
		for (S32 remaining = end - start; remaining;)
		{
			S32 drawn = sFont->maxDrawableChars(start, (F32)text_area_width, remaining, LLFontGL::WORD_BOUNDARY_IF_POSSIBLE);

			if (0 == drawn)
			{
				drawn = 1;  // Draw at least one character, even if it doesn't all fit. (avoids an infinite loop)
			}

			total_drawn += drawn; 
			start += drawn;
			remaining -= drawn;

			if (total_drawn < message_len)
			{
				if (wchars[ total_drawn ] != '\n')
				{
					// wrap because line was too long
					line_count++;
				}
			}
			else
			{
				done = true;
			}
		}

		total_drawn++;	// for '\n'
		start = ++end;
	}

	const S32 MIN_NOTIFY_HEIGHT = 72;
	const S32 MAX_NOTIFY_HEIGHT = 600;
	S32 notify_height = llceil((F32) (line_count+1) * sFont->getLineHeight());
	if (gOverlayBar)
	{
		notify_height += gOverlayBar->getBoundingRect().mTop;
	}
	else
	{
		// *FIX: this is derived from the padding caused by the
		// rounded rects, shouldn't be a const here.
		notify_height += 10;  
	}
	notify_height += VPAD;
	notify_height = llclamp(notify_height, MIN_NOTIFY_HEIGHT, MAX_NOTIFY_HEIGHT);

	const S32 RIGHT = gNotifyBoxView->getRect().getWidth();
	const S32 LEFT = RIGHT - NOTIFY_WIDTH;

	// Make sure it goes slightly offscreen
	return LLRect(LEFT, notify_height-1, RIGHT, -1);
}


void LLNotifyBox::onClickButton(const std::string name)
{
	LLSD response = mNotification->getResponseTemplate();
	if (!mAddedDefaultBtn && !name.empty())
	{
		response[name] = true;
	}
	if (mUserInputBox)
	{
		response[mUserInputBox->getName()] = mUserInputBox->getValue();
	}
	mNotification->respond(response);
}


LLNotifyBoxView::LLNotifyBoxView(const std::string& name, const LLRect& rect, BOOL mouse_opaque, U32 follows)
	: LLUICtrl(name,rect,mouse_opaque,NULL,follows) 
{
}

LLNotifyBox* LLNotifyBoxView::getFirstNontipBox() const
{
	// *TODO: Don't make assumptions like this!
	// assumes every child is a notify box
	for(child_list_const_iter_t iter = getChildList()->begin();
			iter != getChildList()->end();
			iter++)
	{
		// hack! *TODO: Integrate llnotify and llgroupnotify
		if (isGroupNotifyBox(*iter))
			continue;

		LLNotifyBox* box = static_cast<LLNotifyBox*>(*iter);
		if (!box->isTip() && !box->isDead())
			return box;
	}
	return NULL;
}

void LLNotifyBoxView::showOnly(LLView* view)
{
	// assumes that the argument is actually a child
	if (!dynamic_cast<LLNotifyBox*>(view)) return;

	// make every other notification invisible
	for(child_list_const_iter_t iter = getChildList()->begin();
			iter != getChildList()->end();
			iter++)
	{
		if (view == (*iter)) continue;
		LLView* view(*iter);
		if (isGroupNotifyBox(view) || !view->getVisible())
			continue;
		if (!static_cast<LLNotifyBox*>(view)->isTip())
			view->setVisible(false);
	}
	view->setVisible(true);
	sendChildToFront(view);
}

void LLNotifyBoxView::deleteAllChildren()
{
	LLUICtrl::deleteAllChildren();
	sNotifyBoxCount = 0;
}

void LLNotifyBoxView::purgeMessagesMatching(const Matcher& matcher)
{
	// Make a *copy* of the child list to iterate over 
	// since we'll be removing items from the real list as we go.
	LLView::child_list_t notification_queue(*getChildList());
	for(LLView::child_list_iter_t iter = notification_queue.begin();
		iter != notification_queue.end();
		iter++)
	{
		if (isGroupNotifyBox(*iter))
			continue;

		LLNotifyBox* notification = static_cast<LLNotifyBox*>(*iter);
		if (matcher.matches(notification->getNotification()))
		{
			removeChild(notification);
		}
	}
}

bool LLNotifyBoxView::isGroupNotifyBox(const LLView* view) const
{
	return view->getName() == "groupnotify";
}

