/** 
 * @file llbutton.cpp
 * @brief LLButton base class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include "llbutton.h"

// Linden library includes
#include "v4color.h"
#include "llstring.h"

// Project includes
#include "llhtmlhelp.h"
#include "llkeyboard.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llresmgr.h"
#include "llcriticaldamp.h"
#include "llfocusmgr.h"
#include "llwindow.h"
#include "llrender.h"

static LLRegisterWidget<LLButton> r("button");

// globals loaded from settings.xml
S32	LLBUTTON_ORIG_H_PAD	= 6; // Pre-zoomable UI
S32	LLBUTTON_H_PAD	= 0;
S32	LLBUTTON_V_PAD	= 0;
S32 BTN_HEIGHT_SMALL= 0;
S32 BTN_HEIGHT		= 0;

S32 BTN_GRID		= 12;
S32 BORDER_SIZE = 1;

LLButton::LLButton(	const std::string& name, const LLRect& rect, const std::string& control_name, commit_callback_t commit_callback)
:	LLUICtrl(name, rect, TRUE, commit_callback),
	
	mMouseDownFrame( 0 ),
	mMouseHeldDownCount(0),
	mBorderEnabled( FALSE ),
	mFlashing( FALSE ),
	mCurGlowStrength(0.f),
	mNeedsHighlight(FALSE),
	mUnselectedLabel(name),
	mSelectedLabel(name),
	mGLFont( LLFontGL::getFontSansSerif() ),
	mHeldDownDelay( 0.5f ),			// seconds until held-down callback is called
	mHeldDownFrameDelay( 0 ),
	mImageUnselected(LLUI::getUIImage("button_enabled_32x128.tga")),
	mImageSelected(LLUI::getUIImage("button_enabled_selected_32x128.tga")),
	mImageDisabled(LLUI::getUIImage("button_disabled_32x128.tga")),
	mImageDisabledSelected(LLUI::getUIImage("button_disabled_32x128.tga")),
	mImagePressed(LLUI::getUIImage("button_enabled_selected_32x128.tga")),
	mImagePressedSelected(LLUI::getUIImage("button_enabled_32x128.tga")),
	mImageFlash(NULL),
	mImageHoverSelected( NULL ),
	mImageHoverUnselected( NULL ),
	mUnselectedLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelColor" )),
	mSelectedLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelSelectedColor")),
	mDisabledLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelDisabledColor")),
	mDisabledSelectedLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelSelectedDisabledColor")),
	mImageColor(LLUI::sColorsGroup->getColor("ButtonImageColor")),
	mFlashBgColor(LLUI::sColorsGroup->getColor("ButtonFlashBgColor")),
	mDisabledImageColor(LLUI::sColorsGroup->getColor("ButtonImageColor")),
	mImageOverlay(NULL),
	mImageOverlayColor(LLColor4::white),
	mImageOverlayDisabledColor(LLColor4(1.f,1.f,1.f,.5f)),
	mImageOverlaySelectedColor(LLColor4::white),
	mImageOverlayAlignment(LLFontGL::HCENTER),
	mImageOverlayTopPad(0),
	mImageOverlayBottomPad(0),
	mImgOverlayLabelSpace(1),
	mIsToggle( FALSE ),
	mScaleImage( TRUE ),
	mDropShadowedText( TRUE ),	
	mAutoResize( FALSE ),
	mHAlign( LLFontGL::HCENTER ),
	mLeftHPad( LLBUTTON_H_PAD ),
	mRightHPad( LLBUTTON_H_PAD ),
	mBottomVPad( LLBUTTON_V_PAD ),
	mHoverGlowStrength(0.25f),
	mCommitOnReturn(TRUE),
	mFadeWhenDisabled(FALSE),
	mForcePressedState(false),
	mDisplayPressedState(TRUE),
	mMouseDownSignal(NULL),
	mMouseUpSignal(NULL),
	mHeldDownSignal(NULL),
	mHandleRightMouse(false),
	mButtonFlashCount(LLUI::sConfigGroup->getS32("ButtonFlashCount")),
	mButtonFlashRate(LLUI::sConfigGroup->getF32("ButtonFlashRate")),
	mAlpha(1.f)
{
	init(control_name);
}


LLButton::LLButton(const std::string& name, const LLRect& rect, 
				   const std::string &unselected_image_name, 
				   const std::string &selected_image_name, 
				   const std::string& control_name,
				   commit_callback_t commit_callback,
				   const LLFontGL *font,
				   const std::string& unselected_label, 
				   const std::string& selected_label )
:	LLUICtrl(name, rect, TRUE, commit_callback),
	mMouseDownFrame( 0 ),
	mMouseHeldDownCount(0),
	mBorderEnabled( FALSE ),
	mFlashing( FALSE ),
	mCurGlowStrength(0.f),
	mNeedsHighlight(FALSE),
	mUnselectedLabel(unselected_label),
	mSelectedLabel(selected_label),
	mGLFont( font ? font : LLFontGL::getFontSansSerif() ),
	mHeldDownDelay( 0.5f ),			// seconds until held-down callback is called
	mHeldDownFrameDelay( 0 ),
	mImageUnselected(LLUI::getUIImage("button_enabled_32x128.tga")),
	mImageSelected(LLUI::getUIImage("button_enabled_selected_32x128.tga")),
	mImageDisabled(LLUI::getUIImage("button_disabled_32x128.tga")),
	mImageDisabledSelected(LLUI::getUIImage("button_disabled_32x128.tga")),
	mImagePressed(LLUI::getUIImage("button_enabled_selected_32x128.tga")),
	mImagePressedSelected(LLUI::getUIImage("button_enabled_32x128.tga")),
	mImageFlash(NULL),
	mImageHoverSelected( NULL ),
	mImageHoverUnselected( NULL ),
	mUnselectedLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelColor" )),
	mSelectedLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelSelectedColor")),
	mDisabledLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelDisabledColor")),
	mDisabledSelectedLabelColor(LLUI::sColorsGroup->getColor("ButtonLabelSelectedDisabledColor")),
	mImageColor(LLUI::sColorsGroup->getColor("ButtonImageColor")),
	mFlashBgColor(LLUI::sColorsGroup->getColor("ButtonFlashBgColor")),
	mDisabledImageColor(LLUI::sColorsGroup->getColor("ButtonImageColor")),
	mImageOverlay(NULL),
	mImageOverlayColor(LLColor4::white),
	mImageOverlayDisabledColor(LLColor4(1.f,1.f,1.f,.5f)),
	mImageOverlaySelectedColor(LLColor4::white),
	mImageOverlayAlignment(LLFontGL::HCENTER),
	mImageOverlayTopPad(0),
	mImageOverlayBottomPad(0),
	mImgOverlayLabelSpace(1),
	mIsToggle( FALSE ),
	mScaleImage( TRUE ),
	mDropShadowedText( TRUE ),
	mAutoResize( FALSE ),
	mHAlign( LLFontGL::HCENTER ),
	mLeftHPad( LLBUTTON_H_PAD ), 
	mRightHPad( LLBUTTON_H_PAD ),
	mBottomVPad( LLBUTTON_V_PAD ),
	mHoverGlowStrength(0.25f),
	mCommitOnReturn(TRUE),
	mFadeWhenDisabled(FALSE),
	mForcePressedState(false),
	mDisplayPressedState(TRUE),
	mMouseDownSignal(NULL),
	mMouseUpSignal(NULL),
	mHeldDownSignal(NULL),
	mHandleRightMouse(false),
	mButtonFlashCount(LLUI::sConfigGroup->getS32("ButtonFlashCount")),
	mButtonFlashRate(LLUI::sConfigGroup->getF32("ButtonFlashRate")),
	mAlpha(1.f)
{

	if( unselected_image_name != "" )
	{
		// user-specified image - don't use fixed borders unless requested
		mImageUnselected = LLUI::getUIImage(unselected_image_name);
		mImageDisabled = mImageUnselected;
		
		mFadeWhenDisabled = TRUE;
		//mScaleImage = FALSE;
		
		mImagePressedSelected = mImageUnselected;
	}

	if( selected_image_name != "" )
	{
		// user-specified image - don't use fixed borders unless requested
		mImageSelected = LLUI::getUIImage(selected_image_name);
		mImageDisabledSelected = mImageSelected;

		mFadeWhenDisabled = TRUE;
		//mScaleImage = FALSE;
		
		mImagePressed = mImageSelected;
	}

	init(control_name);
}

void LLButton::init(const std::string& control_name)
{
	// Hack to make sure there is space for at least one character
	if (getRect().getWidth() - (mRightHPad + mLeftHPad) < mGLFont->getWidth(std::string(" ")))
	{
		// Use old defaults
		mLeftHPad = LLBUTTON_ORIG_H_PAD;
		mRightHPad = LLBUTTON_ORIG_H_PAD;
	}
	
	mMouseDownTimer.stop();
	
	setControlName(control_name, NULL);
}


// virtual
LLButton::~LLButton()
{
	delete mMouseDownSignal;
	delete mMouseUpSignal;
	delete mHeldDownSignal;
}

// HACK: Committing a button is the same as instantly clicking it.
// virtual
void LLButton::onCommit()
{
	// WARNING: Sometimes clicking a button destroys the floater or
	// panel containing it.  Therefore we need to call 	LLUICtrl::onCommit()
	// LAST, otherwise this becomes deleted memory.

	if (mMouseDownSignal) (*mMouseDownSignal)(this, LLSD());
	
	if (mMouseUpSignal) (*mMouseUpSignal)(this, LLSD());

	if (getSoundFlags() & MOUSE_DOWN)
	{
		make_ui_sound("UISndClick");
	}

	if (getSoundFlags() & MOUSE_UP)
	{
		make_ui_sound("UISndClickRelease");
	}

	if (mIsToggle)
	{
		toggleState();
	}

	// do this last, as it can result in destroying this button
	LLUICtrl::onCommit();
}

/*boost::signals2::connection LLButton::setClickedCallback(const CommitCallbackParam& cb)
{
	return setClickedCallback(initCommitCallback(cb));
}
boost::signals2::connection LLButton::setMouseDownCallback(const CommitCallbackParam& cb)
{
	return setMouseDownCallback(initCommitCallback(cb));
}
boost::signals2::connection LLButton::setMouseUpCallback(const CommitCallbackParam& cb)
{
	return setMouseUpCallback(initCommitCallback(cb));
}
boost::signals2::connection LLButton::setHeldDownCallback(const CommitCallbackParam& cb)
{
	return setHeldDownCallback(initCommitCallback(cb));
}*/


boost::signals2::connection LLButton::setClickedCallback( const commit_signal_t::slot_type& cb )
{
	return LLUICtrl::setCommitCallback(cb);
}
boost::signals2::connection LLButton::setMouseDownCallback( const commit_signal_t::slot_type& cb )
{
	if (!mMouseDownSignal) mMouseDownSignal = new commit_signal_t();
	return mMouseDownSignal->connect(cb);
}
boost::signals2::connection LLButton::setMouseUpCallback( const commit_signal_t::slot_type& cb )
{
	if (!mMouseUpSignal) mMouseUpSignal = new commit_signal_t();
	return mMouseUpSignal->connect(cb);
}
boost::signals2::connection LLButton::setHeldDownCallback( const commit_signal_t::slot_type& cb )
{
	if (!mHeldDownSignal) mHeldDownSignal = new commit_signal_t();
	return mHeldDownSignal->connect(cb);
}


// *TODO: Deprecate (for backwards compatibility only)
boost::signals2::connection LLButton::setClickedCallback( button_callback_t cb, void* data )
{
	return setClickedCallback(boost::bind(cb, data));
}
boost::signals2::connection LLButton::setMouseDownCallback( button_callback_t cb, void* data )
{
	return setMouseDownCallback(boost::bind(cb, data));
}
boost::signals2::connection LLButton::setMouseUpCallback( button_callback_t cb, void* data )
{
	return setMouseUpCallback(boost::bind(cb, data));
}
boost::signals2::connection LLButton::setHeldDownCallback( button_callback_t cb, void* data )
{
	return setHeldDownCallback(boost::bind(cb, data));
}

BOOL LLButton::handleUnicodeCharHere(llwchar uni_char)
{
	BOOL handled = FALSE;
	if(' ' == uni_char 
		&& !gKeyboard->getKeyRepeated(' '))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		LLUICtrl::onCommit();
		
		handled = TRUE;		
	}
	return handled;	
}

BOOL LLButton::handleKeyHere(KEY key, MASK mask )
{
	BOOL handled = FALSE;
	if( mCommitOnReturn && KEY_RETURN == key && mask == MASK_NONE && !gKeyboard->getKeyRepeated(key))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		handled = TRUE;

		LLUICtrl::onCommit();
	}
	return handled;
}


BOOL LLButton::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (!childrenHandleMouseDown(x, y, mask))
	{
		// Route future Mouse messages here preemptively.  (Release on mouse up.)
		gFocusMgr.setMouseCapture( this );

		if (hasTabStop() && !getIsChrome())
		{
			setFocus(TRUE);
		}

		/*
		 * ATTENTION! This call fires another mouse down callback.
		 * If you wish to remove this call emit that signal directly
		 * by calling LLUICtrl::mMouseDownSignal(x, y, mask);
		 */
		LLUICtrl::handleMouseDown(x, y, mask);

		if(mMouseDownSignal) (*mMouseDownSignal)(this, LLSD());

	mMouseDownTimer.start();
		mMouseDownFrame = (S32) LLFrameTimer::getFrameCount();
		mMouseHeldDownCount = 0;

		
		if (getSoundFlags() & MOUSE_DOWN)
		{
			make_ui_sound("UISndClick");
		}
	}
	return TRUE;
}


BOOL LLButton::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// We only handle the click if the click both started and ended within us
	if( hasMouseCapture() )
	{
		// Always release the mouse
		gFocusMgr.setMouseCapture( NULL );

		/*
		 * ATTENTION! This call fires another mouse up callback.
		 * If you wish to remove this call emit that signal directly
		 * by calling LLUICtrl::mMouseUpSignal(x, y, mask);
		 */
		LLUICtrl::handleMouseUp(x, y, mask);

		// Regardless of where mouseup occurs, handle callback
		if(mMouseUpSignal) (*mMouseUpSignal)(this, LLSD());

		resetMouseDownTimer();

		// DO THIS AT THE VERY END to allow the button to be destroyed as a result of being clicked.
		// If mouseup in the widget, it's been clicked
		if (pointInView(x, y))
		{
			if (getSoundFlags() & MOUSE_UP)
			{
				make_ui_sound("UISndClickRelease");
			}

			if (mIsToggle)
			{
				toggleState();
			}

			LLUICtrl::onCommit();
		}
	}
	else
	{
		childrenHandleMouseUp(x, y, mask);
	}

	return TRUE;
}

BOOL	LLButton::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mHandleRightMouse && !childrenHandleRightMouseDown(x, y, mask))
	{
		// Route future Mouse messages here preemptively.  (Release on mouse up.)
		gFocusMgr.setMouseCapture( this );

		if (hasTabStop() && !getIsChrome())
		{
			setFocus(TRUE);
		}

//		if (pointInView(x, y))
//		{
//		}
		// send the mouse down signal
		LLUICtrl::handleRightMouseDown(x,y,mask);
		// *TODO: Return result of LLUICtrl call above?  Should defer to base class
		// but this might change the mouse handling of existing buttons in a bad way
		// if they are not mouse opaque.
	}

	return TRUE;
}

BOOL	LLButton::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	if (mHandleRightMouse)
	{
		// We only handle the click if the click both started and ended within us
		if( hasMouseCapture() )
		{
			// Always release the mouse
			gFocusMgr.setMouseCapture( NULL );

	//		if (pointInView(x, y))
	//		{
	//			mRightMouseUpSignal(this, x,y,mask);
	//		}
		}
		else 
		{
			childrenHandleRightMouseUp(x, y, mask);
		}
	
		// send the mouse up signal
		LLUICtrl::handleRightMouseUp(x,y,mask);
		// *TODO: Return result of LLUICtrl call above?  Should defer to base class
		// but this might change the mouse handling of existing buttons in a bad way.
		// if they are not mouse opaque.
	}
	return TRUE;
}

void LLButton::setHighlight(bool b)
{
	mNeedsHighlight = b;
}

BOOL LLButton::handleHover(S32 x, S32 y, MASK mask)
{
	if (isInEnabledChain() 
		&& (!gFocusMgr.getMouseCapture() || gFocusMgr.getMouseCapture() == this))
		mNeedsHighlight = TRUE;

	if (!childrenHandleHover(x, y, mask))
	{
		if (mMouseDownTimer.getStarted())
		{
			F32 elapsed = getHeldDownTime();
			if( mHeldDownDelay <= elapsed && mHeldDownFrameDelay <= (S32)LLFrameTimer::getFrameCount() - mMouseDownFrame)
			{
				LLSD param;
				param["count"] = mMouseHeldDownCount++;
				if (mHeldDownSignal) (*mHeldDownSignal)(this, param);
			}
		}

		// We only handle the click if the click both started and ended within us
		getWindow()->setCursor(UI_CURSOR_ARROW);
		lldebugst(LLERR_USER_INPUT) << "hover handled by " << getName() << llendl;
	}
	return TRUE;
}

void LLButton::getOverlayImageSize(S32& overlay_width, S32& overlay_height)
{
	overlay_width = mImageOverlay->getWidth();
	overlay_height = mImageOverlay->getHeight();

	F32 scale_factor = llmin((F32)getRect().getWidth() / (F32)overlay_width, (F32)getRect().getHeight() / (F32)overlay_height, 1.f);
	overlay_width = llround((F32)overlay_width * scale_factor);
	overlay_height = llround((F32)overlay_height * scale_factor);
}


// virtual
void LLButton::draw()
{
	F32 alpha = mAlpha * getDrawContext().mAlpha;
	bool flash = FALSE;
	if( mFlashing )
	{
		F32 elapsed = mFlashingTimer.getElapsedTimeF32();
		S32 flash_count = S32(elapsed * mButtonFlashRate * 2.f);
		// flash on or off?
		flash = (flash_count % 2 == 0) || flash_count > S32((F32)mButtonFlashCount * 2.f);
	}

	bool pressed_by_keyboard = FALSE;
	if (hasFocus())
	{
		pressed_by_keyboard = gKeyboard->getKeyDown(' ') || (mCommitOnReturn && gKeyboard->getKeyDown(KEY_RETURN));
	}

	bool mouse_pressed_and_over = false;
	if (hasMouseCapture())
	{
		S32 local_mouse_x ;
		S32 local_mouse_y;
		LLUI::getMousePositionLocal(this, &local_mouse_x, &local_mouse_y);
		mouse_pressed_and_over = pointInView(local_mouse_x, local_mouse_y);
	}

	bool enabled = isInEnabledChain();
	
	bool pressed = pressed_by_keyboard 
					|| mouse_pressed_and_over
					|| mForcePressedState;
	bool selected = getToggleState();
	
	bool use_glow_effect = FALSE;
	LLColor4 glow_color = LLColor4::white;
	LLRender::eBlendType glow_type = LLRender::BT_ADD_WITH_ALPHA;
	LLUIImage* imagep = NULL;
	if (pressed && mDisplayPressedState)
	{
		imagep = selected ? mImagePressedSelected : mImagePressed;
	}
	else if ( mNeedsHighlight )
	{
		if (selected)
		{
			if (mImageHoverSelected)
			{
				imagep = mImageHoverSelected;
			}
			else
			{
				imagep = mImageSelected;
				use_glow_effect = TRUE;
			}
		}
		else
		{
			if (mImageHoverUnselected)
			{
				imagep = mImageHoverUnselected;
			}
			else
			{
				imagep = mImageUnselected;
				use_glow_effect = TRUE;
			}
		}
	}
	else 
	{
		imagep = selected ? mImageSelected : mImageUnselected;
	}

	// Override if more data is available
	// HACK: Use gray checked state to mean either:
	//   enabled and tentative
	// or
	//   disabled but checked
	if (!mImageDisabledSelected.isNull() 
		&& 
			( (enabled && getTentative()) 
			|| (!enabled && selected ) ) )
	{
		imagep = mImageDisabledSelected;
	}
	else if (!mImageDisabled.isNull() 
		&& !enabled 
		&& !selected)
	{
		imagep = mImageDisabled;
	}

	if (mFlashing)
	{
		// if button should flash and we have icon for flashing, use it as image for button
		if(flash && mImageFlash)
		{
			// setting flash to false to avoid its further influence on glow
			flash = false;
			imagep = mImageFlash;
		}
		// else use usual flashing via flash_color
		else
		{
			LLColor4 flash_color = mFlashBgColor.get();
			use_glow_effect = TRUE;
			glow_type = LLRender::BT_ALPHA; // blend the glow
			if (mNeedsHighlight) // highlighted AND flashing
				glow_color = (glow_color*0.5f + flash_color*0.5f) % 2.0f; // average between flash and highlight colour, with sum of the opacity
			else
				glow_color = flash_color;
		}
	}

	if (mNeedsHighlight && !imagep)
	{
		use_glow_effect = TRUE;
	}

	// Figure out appropriate color for the text
	LLColor4 label_color;

	// label changes when button state changes, not when pressed
	if ( enabled )
	{
		if ( getToggleState() )
		{
			label_color = mSelectedLabelColor.get();
		}
		else
		{
			label_color = mUnselectedLabelColor.get();
		}
	}
	else
	{
		if ( getToggleState() )
		{
			label_color = mDisabledSelectedLabelColor.get();
		}
		else
		{
			label_color = mDisabledLabelColor.get();
		}
	}

	// Unselected label assignments
	LLWString label = getCurrentLabel();

	// overlay with keyboard focus border
	if (hasFocus())
	{
		F32 lerp_amt = gFocusMgr.getFocusFlashAmt();
		drawBorder(imagep, gFocusMgr.getFocusColor() % alpha, llround(lerp(1.f, 3.f, lerp_amt)));
	}
	
	if (use_glow_effect)
	{
		mCurGlowStrength = lerp(mCurGlowStrength,
					mFlashing ? (flash? 1.0 : 0.0)
					: mHoverGlowStrength,
					LLCriticalDamp::getInterpolant(0.05f));
	}
	else
	{
		mCurGlowStrength = lerp(mCurGlowStrength, 0.f, LLCriticalDamp::getInterpolant(0.05f));
	}

	// Draw button image, if available.
	// Otherwise draw basic rectangular button.
	if (imagep != NULL)
	{
		// apply automatic 50% alpha fade to disabled image
		LLColor4 disabled_color = mFadeWhenDisabled ? mDisabledImageColor.get() % 0.5f : mDisabledImageColor.get();
		if ( mScaleImage)
		{
			imagep->draw(getLocalRect(), (enabled ? mImageColor.get() : disabled_color) % alpha  );
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				imagep->drawSolid(0, 0, getRect().getWidth(), getRect().getHeight(), glow_color % (mCurGlowStrength * alpha));
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
		else
		{
			imagep->draw(0, 0, (enabled ? mImageColor.get() : disabled_color) % alpha );
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				imagep->drawSolid(0, 0, glow_color % (mCurGlowStrength * alpha));
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
	}
	else
	{
		// no image
		lldebugs << "No image for button " << getName() << llendl;
		// draw it in pink so we can find it
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, LLColor4::pink1 % alpha, FALSE);
	}

	// let overlay image and text play well together
	S32 text_left = mLeftHPad;
	S32 text_right = getRect().getWidth() - mRightHPad;
	S32 text_width = getRect().getWidth() - mLeftHPad - mRightHPad;

	// draw overlay image
	if (mImageOverlay.notNull() && mImageOverlay->getWidth() > 1)
	{
		// get max width and height (discard level 0)
		S32 overlay_width;
		S32 overlay_height;

		getOverlayImageSize(overlay_width, overlay_height);

		S32 center_x = getLocalRect().getCenterX();
		S32 center_y = getLocalRect().getCenterY();

		//FUGLY HACK FOR "DEPRESSED" BUTTONS
		if (pressed && mDisplayPressedState)
		{
			center_y--;
			center_x++;
		}

		center_y += (mImageOverlayBottomPad - mImageOverlayTopPad);
		// fade out overlay images on disabled buttons
		LLColor4 overlay_color = mImageOverlayColor.get();
		if (!enabled)
		{
			overlay_color = mImageOverlayDisabledColor.get();
		}
		else if (getToggleState())
		{
			overlay_color = mImageOverlaySelectedColor.get();
		}
		overlay_color.mV[VALPHA] *= alpha;

		switch(mImageOverlayAlignment)
		{
		case LLFontGL::LEFT:
			text_left += overlay_width + mImgOverlayLabelSpace;
			text_width -= overlay_width + mImgOverlayLabelSpace;
			mImageOverlay->draw(
				mLeftHPad,
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		case LLFontGL::HCENTER:
			mImageOverlay->draw(
				center_x - (overlay_width / 2), 
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		case LLFontGL::RIGHT:
			text_right -= overlay_width + mImgOverlayLabelSpace;
			text_width -= overlay_width + mImgOverlayLabelSpace;
			mImageOverlay->draw(
				getRect().getWidth() - mRightHPad - overlay_width,
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		default:
			// draw nothing
			break;
		}
	}

	// Draw label
	if( !label.empty() )
	{
		LLWStringUtil::trim(label);

		S32 x;
		switch( mHAlign )
		{
		case LLFontGL::RIGHT:
			x = text_right;
			break;
		case LLFontGL::HCENTER:
			x = text_left + (text_width / 2);
			break;
		case LLFontGL::LEFT:
		default:
			x = text_left;
			break;
		}

		S32 y_offset = 2 + (getRect().getHeight() - 20)/2;
	
		if (pressed && mDisplayPressedState)
		{
			y_offset--;
			x++;
		}

		mGLFont->render(label, 0, 
			(F32)x, 
			(F32)(mBottomVPad + y_offset), 
			label_color % alpha,
			mHAlign, LLFontGL::BOTTOM,
			LLFontGL::NORMAL,
			mDropShadowedText ? LLFontGL::DROP_SHADOW_SOFT : LLFontGL::NO_SHADOW,
			U32_MAX, text_width,
			NULL, FALSE, FALSE);
	}

	if (sDebugRects	
		|| (LLView::sEditingUI && this == LLView::sEditingUIView))
	{
		drawDebugRect();
	}
	
	// reset hover status for next frame
	mNeedsHighlight = FALSE;
	
	LLUICtrl::draw();
}

void LLButton::drawBorder(LLUIImage* imagep, const LLColor4& color, S32 size)
{
	if (imagep == NULL) return;
	if (mScaleImage)
	{
		imagep->drawBorder(getLocalRect(), color, size);
	}
	else
	{
		imagep->drawBorder(0, 0, color, size);
	}
}

BOOL LLButton::getToggleState() const
{
    return getValue().asBoolean();
}

void LLButton::setToggleState(BOOL b)
{
	if( b != getToggleState() )
	{
		setControlValue(b); // will fire LLControlVariable callbacks (if any)
		setValue(b);        // may or may not be redundant
		// Unselected label assignments
		autoResize();
	}
}

void LLButton::setFlashing( BOOL b )	
{ 
	if ((bool)b != mFlashing)
	{
		mFlashing = b; 
		mFlashingTimer.reset();
	}
}


BOOL LLButton::toggleState()			
{ 
    bool flipped = ! getToggleState();
	setToggleState(flipped); 

	return flipped; 
}

void LLButton::setLabel( const LLStringExplicit& label )
{
	setLabelUnselected(label);
	setLabelSelected(label);
}

//virtual
BOOL LLButton::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	mUnselectedLabel.setArg(key, text);
	mSelectedLabel.setArg(key, text);
	return TRUE;
}

void LLButton::setLabelUnselected( const LLStringExplicit& label )
{
	mUnselectedLabel = label;
}

void LLButton::setLabelSelected( const LLStringExplicit& label )
{
	mSelectedLabel = label;
}

const LLUIString& LLButton::getCurrentLabel() const
{
	if( getToggleState() )
	{
		return mSelectedLabel;
	}
	else
	{
		return mUnselectedLabel;
	}
}

void LLButton::setImageUnselected(LLPointer<LLUIImage> image)
{
	mImageUnselected = image;
	if (mImageUnselected.isNull())
	{
		llwarns << "Setting default button image for: " << getName() << " to NULL" << llendl;
	}
}

void LLButton::autoResize()
{
	resize(getCurrentLabel());
}

void LLButton::resize(LLUIString label)
{
	// get label length 
	S32 label_width = mGLFont->getWidth(label.getString());
	// get current btn length 
	S32 btn_width =getRect().getWidth();
    // check if it need resize 
	if (mAutoResize)
	{ 
		S32 min_width = label_width + mLeftHPad + mRightHPad;
		if (mImageOverlay)
		{
			S32 overlay_width = mImageOverlay->getWidth();
			F32 scale_factor = (getRect().getHeight() - (mImageOverlayBottomPad + mImageOverlayTopPad)) / (F32)mImageOverlay->getHeight();
			overlay_width = llround((F32)overlay_width * scale_factor);

			switch(mImageOverlayAlignment)
			{
			case LLFontGL::LEFT:
			case LLFontGL::RIGHT:
				min_width += overlay_width + mImgOverlayLabelSpace;
				break;
			case LLFontGL::HCENTER:
				min_width = llmax(min_width, overlay_width + mLeftHPad + mRightHPad);
				break;
			default:
				// draw nothing
				break;
			}
		}
		if (btn_width < min_width)
		{
			reshape(min_width, getRect().getHeight());
		}
	} 
}
void LLButton::setImages( const std::string &image_name, const std::string &selected_name )
{
	setImageUnselected(LLUI::getUIImage(image_name));
	setImageSelected(LLUI::getUIImage(selected_name));
}

void LLButton::setImageSelected(LLPointer<LLUIImage> image)
{
	mImageSelected = image;
}

void LLButton::setImageColor(const LLColor4& c)		
{ 
	mImageColor = c; 
}

void LLButton::setColor(const LLColor4& color)
{
	setImageColor(color);
}

void LLButton::setImageDisabled(LLPointer<LLUIImage> image)
{
	mImageDisabled = image;
	mDisabledImageColor = mImageColor;
	mFadeWhenDisabled = TRUE;
}

void LLButton::setImageDisabledSelected(LLPointer<LLUIImage> image)
{
	mImageDisabledSelected = image;
	mDisabledImageColor = mImageColor;
	mFadeWhenDisabled = TRUE;
}

void LLButton::setImageHoverSelected(LLPointer<LLUIImage> image)
{
	mImageHoverSelected = image;
}

void LLButton::setImageHoverUnselected(LLPointer<LLUIImage> image)
{
	mImageHoverUnselected = image;
}

void LLButton::setImageFlash(LLPointer<LLUIImage> image)
{
	mImageFlash = image;
}

void LLButton::setImageOverlay(const std::string& image_name, LLFontGL::HAlign alignment, const LLColor4& color)
{
	if (image_name.empty())
	{
		mImageOverlay = NULL;
		mImageOverlaySelectedColor = LLColor4::white;
	}
	else
	{
		mImageOverlay = LLUI::getUIImage(image_name);
		mImageOverlayAlignment = alignment;
		mImageOverlaySelectedColor = color;
		mImageOverlayColor = color;
	}
}

void LLButton::setImageOverlay(const LLUUID& image_id, LLFontGL::HAlign alignment, const LLColor4& color)
{
	if (image_id.isNull())
	{
		mImageOverlay = NULL;
		mImageOverlaySelectedColor = LLColor4::white;
	}
	else
	{
		mImageOverlay = LLUI::getUIImageByID(image_id);
		mImageOverlayAlignment = alignment;
		mImageOverlaySelectedColor = color;
		mImageOverlayColor = color;
	}
}

void LLButton::onMouseCaptureLost()
{
	resetMouseDownTimer();
}

//-------------------------------------------------------------------------
// Utilities
//-------------------------------------------------------------------------
S32 round_up(S32 grid, S32 value)
{
	S32 mod = value % grid;

	if (mod > 0)
	{
		// not even multiple
		return value + (grid - mod);
	}
	else
	{
		return value;
	}
}

void LLButton::addImageAttributeToXML(LLXMLNodePtr node, 
									  const LLPointer<LLUIImage> image,
									  const std::string& xml_tag_name) const
{
	if(!image)
		return;

	const std::string image_name = image->getName();

	if(image_name.empty())
		return;

	LLUUID id;
	if(!id.set(image_name, false))
		node->createChild(xml_tag_name.c_str(), TRUE)->setStringValue(image_name);
	else
		node->createChild((xml_tag_name + "_id").c_str(), TRUE)->setUUIDValue(id);
}

// virtual
LLXMLNodePtr LLButton::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_BUTTON_TAG);

	node->createChild("label", TRUE)->setStringValue(getLabelUnselected());
	node->createChild("label_selected", TRUE)->setStringValue(getLabelSelected());
	node->createChild("font", TRUE)->setStringValue(LLFontGL::nameFromFont(mGLFont));
	node->createChild("halign", TRUE)->setStringValue(LLFontGL::nameFromHAlign(mHAlign));

	addImageAttributeToXML(node,mImageUnselected,std::string("image_unselected"));
	addImageAttributeToXML(node,mImageSelected,std::string("image_selected"));
	addImageAttributeToXML(node,mImageHoverSelected,std::string("image_hover_selected"));
	addImageAttributeToXML(node,mImageHoverUnselected,std::string("image_hover_unselected"));
	addImageAttributeToXML(node,mImageDisabled,std::string("image_disabled"));
	addImageAttributeToXML(node,mImageDisabledSelected,std::string("image_disabled_selected"));

	node->createChild("scale_image", TRUE)->setBoolValue(mScaleImage);

	return node;
}

void clicked_help(void* data)
{
	LLButton* self = (LLButton*)data;
	if (!self) return;
	
	if (!LLUI::sHtmlHelp)
	{
		return;
	}
	
	LLUI::sHtmlHelp->show(self->getHelpURL());
}

// static
LLView* LLButton::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name("button");
	node->getAttributeString("name", name);

	std::string label = name;
	node->getAttributeString("label", label);

	std::string label_selected = label;
	node->getAttributeString("label_selected", label_selected);

	LLFontGL* font = selectFont(node);

	std::string	image_unselected;
	if (node->hasAttribute("image_unselected")) node->getAttributeString("image_unselected",image_unselected);
	
	std::string	image_selected;
	if (node->hasAttribute("image_selected")) node->getAttributeString("image_selected",image_selected);
	
	std::string	image_hover_selected;
	if (node->hasAttribute("image_hover_selected")) node->getAttributeString("image_hover_selected",image_hover_selected);
	
	std::string	image_hover_unselected;
	if (node->hasAttribute("image_hover_unselected")) node->getAttributeString("image_hover_unselected",image_hover_unselected);
	
	std::string	image_disabled_selected;
	if (node->hasAttribute("image_disabled_selected")) node->getAttributeString("image_disabled_selected",image_disabled_selected);
	
	std::string	image_disabled;
	if (node->hasAttribute("image_disabled")) node->getAttributeString("image_disabled",image_disabled);

	std::string	image_overlay;
	node->getAttributeString("image_overlay", image_overlay);

	LLFontGL::HAlign image_overlay_alignment = LLFontGL::HCENTER;
	std::string image_overlay_alignment_string;
	if (node->hasAttribute("image_overlay_alignment"))
	{
		node->getAttributeString("image_overlay_alignment", image_overlay_alignment_string);
		image_overlay_alignment = LLFontGL::hAlignFromName(image_overlay_alignment_string);
	}


	LLButton *button = new LLButton(name, 
			LLRect(),
			image_unselected,
			image_selected,
			LLStringUtil::null, 
			NULL, 
			font,
			label,
			label_selected);

	node->getAttributeS32("pad_right", button->mRightHPad);
	node->getAttributeS32("pad_left", button->mLeftHPad);

	BOOL is_toggle = button->getIsToggle();
	node->getAttributeBOOL("toggle", is_toggle);
	button->setIsToggle(is_toggle);

	if(image_hover_selected != LLStringUtil::null) button->setImageHoverSelected(LLUI::getUIImage(image_hover_selected));
	
	if(image_hover_unselected != LLStringUtil::null) button->setImageHoverUnselected(LLUI::getUIImage(image_hover_unselected));
	
	if(image_disabled_selected != LLStringUtil::null) button->setImageDisabledSelected(LLUI::getUIImage(image_disabled_selected));
	
	if(image_disabled != LLStringUtil::null) button->setImageDisabled(LLUI::getUIImage(image_disabled));
	
	if(image_overlay != LLStringUtil::null) button->setImageOverlay(image_overlay, image_overlay_alignment);

	if (node->hasAttribute("halign"))
	{
		LLFontGL::HAlign halign = selectFontHAlign(node);
		button->setHAlign(halign);
	}

	if (node->hasAttribute("scale_image"))
	{
		BOOL	needsScale = FALSE;
		node->getAttributeBOOL("scale_image",needsScale);
		button->setScaleImage( needsScale );
	}

	if(label.empty())
	{
		button->setLabelUnselected(node->getTextContents());
	}
	if (label_selected.empty())
	{
		button->setLabelSelected(node->getTextContents());
	}
		
	if (node->hasAttribute("help_url")) 
	{
		std::string	help_url;
		node->getAttributeString("help_url",help_url);
		button->setHelpURLCallback(help_url);
	}

	button->initFromXML(node, parent);
	
	return button;
}

void LLButton::resetMouseDownTimer()
{
	mMouseDownTimer.stop();
	mMouseDownTimer.reset();
}

BOOL LLButton::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// just treat a double click as a second click
	return handleMouseDown(x, y, mask);
}

void LLButton::setHelpURLCallback(const std::string &help_url)
{
	mHelpURL = help_url;
	setClickedCallback(clicked_help,this);
}
