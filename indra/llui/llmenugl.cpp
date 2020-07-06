/** 
 * @file llmenugl.cpp
 * @brief LLMenuItemGL base class
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

// *****************************************************************************
//
// This file contains the opengl based menu implementation.
//
// NOTES: A menu label is split into 4 columns. The left column, the
// label colum, the accelerator column, and the right column. The left
// column is used for displaying boolean values for toggle and check
// controls. The right column is used for submenus.
//
// *****************************************************************************

//#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "llmenugl.h"

#include "llgl.h"
#include "llmath.h"
#include "llrender.h"
#include "llfocusmgr.h"
#include "llcoord.h"
#include "llwindow.h"
#include "llcriticaldamp.h"
#include "lluictrlfactory.h"

#include "llbutton.h"
#include "llfontgl.h"
#include "llresmgr.h"
#include "lltrans.h"
#include "llui.h"

#include "llstl.h"

#include "v2math.h"
#include <set>
#include <boost/tokenizer.hpp>

using namespace LLOldEvents;

// static
LLMenuHolderGL *LLMenuGL::sMenuContainer = nullptr;

S32 MENU_BAR_HEIGHT = 0;

///============================================================================
/// Local function declarations, constants, enums, and typedefs
///============================================================================

const S32 LABEL_BOTTOM_PAD_PIXELS = 2;

const U32 LEFT_PAD_PIXELS = 3;
const U32 LEFT_WIDTH_PIXELS = 15;
const U32 LEFT_PLAIN_PIXELS = LEFT_PAD_PIXELS + LEFT_WIDTH_PIXELS;

const U32 RIGHT_PAD_PIXELS = 2;
const U32 RIGHT_WIDTH_PIXELS = 15;
const U32 RIGHT_PLAIN_PIXELS = RIGHT_PAD_PIXELS + RIGHT_WIDTH_PIXELS;

const U32 ACCEL_PAD_PIXELS = 10;
const U32 PLAIN_PAD_PIXELS = LEFT_PAD_PIXELS + LEFT_WIDTH_PIXELS + RIGHT_PAD_PIXELS + RIGHT_WIDTH_PIXELS;

const U32 BRIEF_PAD_PIXELS = 2;

const U32 SEPARATOR_HEIGHT_PIXELS = 8;
const S32 TEAROFF_SEPARATOR_HEIGHT_PIXELS = 10;
const S32 MENU_ITEM_PADDING = 4;

const std::string SEPARATOR_NAME("separator");
const std::string SEPARATOR_LABEL( "-----------" );
const std::string VERTICAL_SEPARATOR_LABEL( "|" );
const std::string TEAROFF_SEPARATOR_LABEL( "~~~~~~~~~~~" );

const std::string LLMenuGL::BOOLEAN_TRUE_PREFIX( "\xE2\x9C\x94" ); // U+2714 HEAVY CHECK MARK
const std::string LLMenuGL::BRANCH_SUFFIX( "\xE2\x96\xB6" ); // U+25B6 BLACK RIGHT-POINTING TRIANGLE
const std::string LLMenuGL::ARROW_UP  ("^^^^^^^");
const std::string LLMenuGL::ARROW_DOWN("vvvvvvv");

const F32 MAX_MOUSE_SLOPE_SUB_MENU = 0.9f;

const S32 PIE_GESTURE_ACTIVATE_DISTANCE = 10;

LLColor4 LLMenuItemGL::sEnabledColor( 0.0f, 0.0f, 0.0f, 1.0f );
LLColor4 LLMenuItemGL::sDisabledColor( 0.5f, 0.5f, 0.5f, 1.0f );
LLColor4 LLMenuItemGL::sHighlightBackground( 0.0f, 0.0f, 0.7f, 1.0f );
LLColor4 LLMenuItemGL::sHighlightForeground( 1.0f, 1.0f, 1.0f, 1.0f );

LLColor4 LLMenuGL::sDefaultBackgroundColor( 0.25f, 0.25f, 0.25f, 0.75f );
BOOL LLMenuGL::sKeyboardMode = FALSE;

LLHandle<LLView> LLMenuHolderGL::sItemLastSelectedHandle;
LLFrameTimer LLMenuHolderGL::sItemActivationTimer;
//LLColor4 LLMenuGL::sBackgroundColor( 0.8f, 0.8f, 0.0f, 1.0f );

const S32 PIE_CENTER_SIZE = 20;		// pixels, radius of center hole
const F32 PIE_SCALE_FACTOR = 1.7f; // scale factor for pie menu when mouse is initially down
const F32 PIE_SHRINK_TIME = 0.2f; // time of transition between unbounded and bounded display of pie menu

const F32 ACTIVATE_HIGHLIGHT_TIME = 0.3f;

///============================================================================
/// Class LLMenuItemGL
///============================================================================

// Default constructor
LLMenuItemGL::LLMenuItemGL( const std::string& name, const std::string& label, KEY key, MASK mask ) :
	LLUICtrl( name ),
	mJumpKey(KEY_NONE),
	mAllowKeyRepeat(FALSE),
	mHighlight( FALSE ),
	mGotHover( FALSE ),
	mBriefItem( FALSE ),
	mStyle(LLFontGL::NORMAL),
	mDrawTextDisabled( FALSE ),
	mFont( LLFontGL::getFontSansSerif() ),
	mAcceleratorKey( key ),
	mAcceleratorMask( mask ),
	mLabel( label ),
	mEnabledColor( sEnabledColor ),
	mDisabledColor( sDisabledColor ),
	mHighlightBackground( sHighlightBackground ),
	mHighlightForeground( sHighlightForeground )
{
	setLabel( label );
}

LLMenuItemGL::~LLMenuItemGL()
{
	LL_DEBUGS() << "Menu destroyed:" << this->getName() << LL_ENDL;
};

// virtual
LLXMLNodePtr LLMenuItemGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLView::getXML();

	node->setName(LL_MENU_ITEM_TAG);

	node->createChild("type", TRUE)->setStringValue(getType());

	node->createChild("label", TRUE)->setStringValue(mLabel);

	if (mAcceleratorKey != KEY_NONE)
	{
		std::stringstream out;
		if (mAcceleratorMask & MASK_CONTROL)
		{
			out << "control|";
		}
		if (mAcceleratorMask & MASK_ALT)
		{
			out << "alt|";
		}
		if (mAcceleratorMask & MASK_SHIFT)
		{
			out << "shift|";
		}
		out << LLKeyboard::stringFromKey(mAcceleratorKey);

		node->createChild("shortcut", TRUE)->setStringValue(out.str());
		
#ifdef LL_DARWIN
		// Write in special tag if this key is really a ctrl combination on the Mac
		if (mAcceleratorMask & MASK_MAC_CONTROL)
		{
			node->createChild("useMacCtrl", TRUE)->setBoolValue( TRUE );
		}
#endif // LL_DARWIN
	}

	return node;
}


//virtual
void LLMenuItemGL::setValue(const LLSD& value)
{
	setLabel(value.asString());
}

//virtual
LLSD LLMenuItemGL::getValue() const
{
	return getLabel();
}

//virtual
BOOL LLMenuItemGL::handleAcceleratorKey(KEY key, MASK mask)
{
	if( getEnabled() && (!gKeyboard->getKeyRepeated(key) || mAllowKeyRepeat) && (key == mAcceleratorKey) && (mask == (mAcceleratorMask & MASK_NORMALKEYS)) )
	{
		onCommit();
		return TRUE;
	}
	return FALSE;
}

BOOL LLMenuItemGL::handleHover(S32 x, S32 y, MASK mask)
{
	setHover(TRUE);
	getWindow()->setCursor(UI_CURSOR_ARROW);
	return TRUE;
}

//virtual
BOOL LLMenuItemGL::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	return LLUICtrl::handleRightMouseDown(x,y,mask);
}

//virtual
BOOL LLMenuItemGL::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	// If this event came from a right-click context menu spawn,
	// process as a left-click to allow menu items to be hit
	if (LLMenuHolderGL::sContextMenuSpawnPos.mX != S32_MAX
		|| LLMenuHolderGL::sContextMenuSpawnPos.mY != S32_MAX)
	{
		BOOL handled = handleMouseUp(x, y, mask);
		return handled;
	}
	return LLUICtrl::handleRightMouseUp(x,y,mask);
}

// This function checks to see if the accelerator key is already in use;
// if not, it will be added to the list
BOOL LLMenuItemGL::addToAcceleratorList(std::list <LLKeyBinding*> *listp)
{
	LLKeyBinding *accelerator = nullptr;

	if (mAcceleratorKey != KEY_NONE)
	{
		std::list<LLKeyBinding*>::iterator list_it;
		for (list_it = listp->begin(); list_it != listp->end(); ++list_it)
		{
			accelerator = *list_it;
			if ((accelerator->mKey == mAcceleratorKey) && (accelerator->mMask == (mAcceleratorMask & MASK_NORMALKEYS)))
			{

			// *NOTE: get calling code to throw up warning or route
			// warning messages back to app-provided output
			//	std::string warning;
			//	warning.append("Duplicate key binding <");
			//	appendAcceleratorString( warning );
			//	warning.append("> for menu items:\n    ");
			//	warning.append(accelerator->mName);
			//	warning.append("\n    ");
			//	warning.append(mLabel);

			//	LL_WARNS() << warning << LL_ENDL;
			//	LLAlertDialog::modalAlert(warning);
				return FALSE;
			}
		}
		if (!accelerator)
		{				
			try
			{
				accelerator = new LLKeyBinding;
				accelerator->mKey = mAcceleratorKey;
				accelerator->mMask = (mAcceleratorMask & MASK_NORMALKEYS);
// 				accelerator->mName = mLabel;
				listp->push_back(accelerator);//addData(accelerator);
			}
			catch (const std::bad_alloc& e)
			{
				LL_WARNS() << "Failed to allocate memory for keybinding with exception: " << e.what() << LL_ENDL;
				return FALSE;
			}
		}
	}
	return TRUE;
}

// This function appends the character string representation of
// the current accelerator key and mask to the provided string.
void LLMenuItemGL::appendAcceleratorString( std::string& st ) const
{
	// break early if this is a silly thing to do.
	if( KEY_NONE == mAcceleratorKey )
	{
		return;
	}

	// Append any masks
#ifdef LL_DARWIN
	// Standard Mac names for modifier keys in menu equivalents
	// We could use the symbol characters, but they only exist in certain fonts.
	if( mAcceleratorMask & MASK_CONTROL )
	{
		if ( mAcceleratorMask & MASK_MAC_CONTROL )
		{
			st.append( LLTrans::getString("accel-mac-control") );
		}
		else
		{
			st.append( LLTrans::getString("accel-mac-command") );		// Symbol would be "\xE2\x8C\x98"
		}
	}
	if( mAcceleratorMask & MASK_ALT )
		st.append( LLTrans::getString("accel-mac-option") );		// Symbol would be "\xE2\x8C\xA5"
	if( mAcceleratorMask & MASK_SHIFT )
		st.append( LLTrans::getString("accel-mac-shift") );		// Symbol would be "\xE2\x8C\xA7"
#else
	if( mAcceleratorMask & MASK_CONTROL )
		st.append( LLTrans::getString("accel-win-control") );
	if( mAcceleratorMask & MASK_ALT )
		st.append( LLTrans::getString("accel-win-alt") );
	if( mAcceleratorMask & MASK_SHIFT )
		st.append( LLTrans::getString("accel-win-shift") );
#endif

	std::string keystr = LLKeyboard::stringFromKey( mAcceleratorKey );
	if ((mAcceleratorMask & MASK_NORMALKEYS) &&
		(keystr[0] == '-' || keystr[0] == '='))
	{
		st.append( " " );
	}
	st.append( keystr );
}

void LLMenuItemGL::setJumpKey(KEY key)
{
	mJumpKey = LLStringOps::toUpper((char)key);
}


// virtual 
U32 LLMenuItemGL::getNominalHeight( void ) const 
{ 
	return ll_round(mFont->getLineHeight()) + MENU_ITEM_PADDING;
}

//virtual
void LLMenuItemGL::setBriefItem(BOOL brief)
{
	mBriefItem = brief;
}

//virtual
BOOL LLMenuItemGL::isBriefItem() const
{
	return mBriefItem;
}

// Get the parent menu for this item
LLMenuGL* LLMenuItemGL::getMenu() const
{
	return (LLMenuGL*) getParent();
}


// getNominalWidth() - returns the normal width of this control in
// pixels - this is used for calculating the widest item, as well as
// for horizontal arrangement.
U32 LLMenuItemGL::getNominalWidth( void ) const
{
	U32 width;
	
	if (mBriefItem)
	{
		width = BRIEF_PAD_PIXELS;
	}
	else
	{
		width = PLAIN_PAD_PIXELS;
	}

	if( KEY_NONE != mAcceleratorKey )
	{
		width += getMenu()->getShortcutPad();
		std::string temp;
		appendAcceleratorString( temp );
		width += mFont->getWidth( temp );
	}
	width += mFont->getWidth( mLabel.getWString().c_str() );
	return width;
}

// called to rebuild the draw label
void LLMenuItemGL::buildDrawLabel( void )
{
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel.getString();
	appendAcceleratorString( st );
	mDrawAccelLabel = st;
}

void LLMenuItemGL::onCommit( void )
{
	// Check torn-off status to allow left-arrow keyboard navigation back
	// to parent menu.
	// Also, don't hide if item triggered by keyboard shortcut (and hence
	// parent not visible).
	if (!getMenu()->getTornOff() 
		&& getMenu()->getVisible())
	{
		LLMenuGL::sMenuContainer->hideMenus();
	}

	LLUICtrl::onCommit();
}

// set the hover status (called by it's menu)
 void LLMenuItemGL::setHighlight( BOOL highlight )
{
	if (highlight)
	{
		getMenu()->clearHoverItem();
	}

	if (mHighlight != highlight)
	{
		//dirtyRect();
	}

	mHighlight = highlight;
}


BOOL LLMenuItemGL::handleKeyHere( KEY key, MASK mask )
{
	if (getHighlight() && 
		getMenu()->isOpen())
	{
		if (key == KEY_UP)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			getMenu()->highlightPrevItem(this);
			return TRUE;
		}
		else if (key == KEY_DOWN)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			getMenu()->highlightNextItem(this);
			return TRUE;
		}
		else if (key == KEY_RETURN && mask == MASK_NONE)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			onCommit();
			return TRUE;
		}
	}

	return FALSE;
}

BOOL LLMenuItemGL::handleMouseUp( S32 x, S32 y, MASK mask)
{
	// switch to mouse navigation mode
	LLMenuGL::setKeyboardMode(FALSE);

	onCommit();
	make_ui_sound("UISndClickRelease");
	return LLView::handleMouseUp(x, y, mask);
}

BOOL LLMenuItemGL::handleMouseDown( S32 x, S32 y, MASK mask)
{
	// switch to mouse navigation mode
	LLMenuGL::setKeyboardMode(FALSE);

	setHighlight(TRUE);
	return LLView::handleMouseDown(x, y, mask);
}

BOOL LLMenuItemGL::handleScrollWheel( S32 x, S32 y, S32 clicks )
{
	// If the menu is scrollable let it handle the wheel event.
	return !getMenu()->isScrollable();
}

void LLMenuItemGL::draw( void )
{
	// *FIX: This can be optimized by using switches. Want to avoid
	// that until the functionality is finalized.

	// HACK: Brief items don't highlight.  Pie menu takes care of it.  JC
	// let disabled items be highlighted, just don't draw them as such
	if( getEnabled() && getHighlight() && !mBriefItem)
	{
		int debug_count = 0;
		if (dynamic_cast<LLMenuItemCallGL*>(this))
			debug_count++;
		gGL.color4fv( mHighlightBackground.mV );

		gl_rect_2d( 0, getRect().getHeight(), getRect().getWidth(), 0 );
	}

	LLColor4 color;

	LLFontGL::ShadowType font_shadow = LLFontGL::NO_SHADOW;
	if (getEnabled() && !mDrawTextDisabled )
	{
		font_shadow = LLFontGL::DROP_SHADOW_SOFT;
	}

	if ( getEnabled() && getHighlight() )
	{
		color = mHighlightForeground;
	}
	else if( getEnabled() && !mDrawTextDisabled )
	{
		color = mEnabledColor;
	}
	else
	{
		color = mDisabledColor;
	}

	// Draw the text on top.
	if (mBriefItem)
	{
		mFont->render( mLabel, 0, BRIEF_PAD_PIXELS / 2, 0, color,
					   LLFontGL::LEFT, LLFontGL::BOTTOM, mStyle );
	}
	else
	{
		if( !mDrawBoolLabel.empty() )
		{
			mFont->render( mDrawBoolLabel.getWString(), 0, (F32)LEFT_PAD_PIXELS, ((F32)MENU_ITEM_PADDING / 2.f) + 1.f, color,
						   LLFontGL::LEFT, LLFontGL::BOTTOM, mStyle, font_shadow, S32_MAX, S32_MAX, NULL, FALSE );
		}
		mFont->render( mLabel.getWString(), 0, (F32)LEFT_PLAIN_PIXELS, ((F32)MENU_ITEM_PADDING / 2.f) + 1.f, color,
					   LLFontGL::LEFT, LLFontGL::BOTTOM, mStyle, font_shadow, S32_MAX, S32_MAX, NULL, FALSE );
		if( !mDrawAccelLabel.empty() )
		{
			mFont->render( mDrawAccelLabel.getWString(), 0, (F32)getRect().mRight - (F32)RIGHT_PLAIN_PIXELS, ((F32)MENU_ITEM_PADDING / 2.f) + 1.f, color,
						   LLFontGL::RIGHT, LLFontGL::BOTTOM, mStyle, font_shadow, S32_MAX, S32_MAX, NULL, FALSE );
		}
		if( !mDrawBranchLabel.empty() )
		{
			mFont->render( mDrawBranchLabel.getWString(), 0, (F32)getRect().mRight - (F32)RIGHT_PAD_PIXELS, ((F32)MENU_ITEM_PADDING / 2.f) + 1.f, (!getEnabled() || getHighlight()) ? color : color % .7f,
						   LLFontGL::RIGHT, LLFontGL::BOTTOM, mStyle, font_shadow, S32_MAX, S32_MAX, NULL, FALSE );
		}
	}

	// underline "jump" key only when keyboard navigation has been initiated
	if (getMenu()->jumpKeysActive() && LLMenuGL::getKeyboardMode())
	{
		std::string upper_case_label = mLabel.getString();
		LLStringUtil::toUpper(upper_case_label);
		std::string::size_type offset = upper_case_label.find(mJumpKey);
		if (offset != std::string::npos)
		{
			const LLWString& utf32text = mLabel.getWString();
			S32 x_begin = LEFT_PLAIN_PIXELS + mFont->getWidth(utf32text, 0, offset);
			S32 x_end = LEFT_PLAIN_PIXELS + mFont->getWidth(utf32text, 0, offset + 1);
			gl_line_2d(x_begin, (MENU_ITEM_PADDING / 2) + 1, x_end, (MENU_ITEM_PADDING / 2) + 1);
		}
	}

	// clear got hover every frame
	setHover(FALSE);
}

BOOL LLMenuItemGL::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	mLabel.setArg(key, text);
	return TRUE;
}

void LLMenuItemGL::handleVisibilityChange(BOOL new_visibility)
{
	if (getMenu())
	{
		getMenu()->needsArrange();
	}
	LLView::handleVisibilityChange(new_visibility);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemSeparatorGL
//
// This class represents a separator.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


LLMenuItemSeparatorGL::LLMenuItemSeparatorGL( const std::string &name ) :
	LLMenuItemGL( name.empty() ? SEPARATOR_NAME : name, SEPARATOR_LABEL )
{
}
U32 LLMenuItemSeparatorGL::getNominalHeight( void ) const
{
	return SEPARATOR_HEIGHT_PIXELS;
}

LLXMLNodePtr LLMenuItemSeparatorGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemGL::getXML();

	node->setName(LL_MENU_ITEM_SEPARATOR_GL_TAG);

	return node;
}

void LLMenuItemSeparatorGL::draw( void )
{
	gGL.color4fv( mDisabledColor.mV );
	const S32 y = getRect().getHeight() / 2;
	const S32 PAD = 6;
	gl_line_2d( PAD, y, getRect().getWidth() - PAD, y );
}

BOOL LLMenuItemSeparatorGL::handleMouseDown(S32 x, S32 y, MASK mask)
{
	LLMenuGL* parent_menu = getMenu();
	if (y > getRect().getHeight() / 2)
	{
		// the menu items are in the child list in bottom up order
		LLView* prev_menu_item = parent_menu->findNextSibling(this);
		return (prev_menu_item && prev_menu_item->getVisible() && prev_menu_item->getEnabled()) ? prev_menu_item->handleMouseDown(x, prev_menu_item->getRect().getHeight(), mask) : FALSE;
	}
	else
	{
		LLView* next_menu_item = parent_menu->findPrevSibling(this);
		return (next_menu_item && next_menu_item->getVisible() && next_menu_item->getEnabled()) ? next_menu_item->handleMouseDown(x, 0, mask) : FALSE;
	}
}

BOOL LLMenuItemSeparatorGL::handleMouseUp(S32 x, S32 y, MASK mask) 
{
	LLMenuGL* parent_menu = getMenu();
	if (y > getRect().getHeight() / 2)
	{
		LLView* prev_menu_item = parent_menu->findNextSibling(this);
		return (prev_menu_item && prev_menu_item->getVisible() && prev_menu_item->getEnabled()) ? prev_menu_item->handleMouseUp(x, prev_menu_item->getRect().getHeight(), mask) : FALSE;
	}
	else
	{
		LLView* next_menu_item = parent_menu->findPrevSibling(this);
		return (next_menu_item && next_menu_item->getVisible() && next_menu_item->getEnabled()) ? next_menu_item->handleMouseUp(x, 0, mask) : FALSE;
	}
}

BOOL LLMenuItemSeparatorGL::handleHover(S32 x, S32 y, MASK mask) 
{
	LLMenuGL* parent_menu = getMenu();
	if (y > getRect().getHeight() / 2)
	{
		parent_menu->highlightPrevItem(this, FALSE);
		return FALSE;
	}
	else
	{
		parent_menu->highlightNextItem(this, FALSE);
		return FALSE;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemVerticalSeparatorGL
//
// This class represents a vertical separator.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemVerticalSeparatorGL final
:	public LLMenuItemSeparatorGL
{
public:
	LLMenuItemVerticalSeparatorGL(const std::string& name = LLStringUtil::null);

	BOOL handleMouseDown(S32 x, S32 y, MASK mask) override { return FALSE; }
};

LLMenuItemVerticalSeparatorGL::LLMenuItemVerticalSeparatorGL(const std::string& name)
: LLMenuItemSeparatorGL(name)
{
	setLabel( VERTICAL_SEPARATOR_LABEL );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemTearOffGL
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LLMenuItemTearOffGL::LLMenuItemTearOffGL() : 
	LLMenuItemGL(std::string("tear off"), TEAROFF_SEPARATOR_LABEL)
{
}


LLXMLNodePtr LLMenuItemTearOffGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemGL::getXML();

	node->setName(LL_MENU_ITEM_TEAR_OFF_GL_TAG);

	return node;
}

// Returns the first floater ancestor if there is one
LLFloater* LLMenuItemTearOffGL::getParentFloater()
{
	LLView* parent_view = getMenu();

	while (parent_view)
	{
		if (dynamic_cast<LLFloater*>(parent_view))
		{
			return dynamic_cast<LLFloater*>(parent_view);
		}

		bool parent_is_menu = dynamic_cast<LLMenuGL*>(parent_view) && !dynamic_cast<LLMenuBarGL*>(parent_view);

		if (parent_is_menu)
		{
			// use menu parent
			parent_view =  dynamic_cast<LLMenuGL*>(parent_view)->getParentMenuItem();
		}
		else
		{
			// just use regular view parent
			parent_view = parent_view->getParent();
		}
	}

	return nullptr;
}

void LLMenuItemTearOffGL::onCommit()
{
	if (getMenu()->getTornOff())
	{
		LLTearOffMenu* torn_off_menu = dynamic_cast<LLTearOffMenu*>(getMenu()->getParent());
		if (torn_off_menu)
		{
			torn_off_menu->close();
		}
	}
	else
	{
		// transfer keyboard focus and highlight to first real item in list
		if (getHighlight())
		{
			getMenu()->highlightNextItem(this);
		}

		getMenu()->needsArrange();

		LLFloater* parent_floater = getParentFloater();
		LLFloater* tear_off_menu = LLTearOffMenu::create(getMenu());

		if (tear_off_menu)
		{
			if (parent_floater)
			{
				parent_floater->addDependentFloater(tear_off_menu, FALSE);
			}

			// give focus to torn off menu because it will have
			// been taken away when parent menu closes
			tear_off_menu->setFocus(TRUE);
		}
	}
	LLMenuItemGL::onCommit();
}

void LLMenuItemTearOffGL::draw()
{
	// disabled items can be highlighted, but shouldn't render as such
	if( getEnabled() && getHighlight() && !isBriefItem())
	{
		gGL.color4fv( mHighlightBackground.mV );
		gl_rect_2d( 0, getRect().getHeight(), getRect().getWidth(), 0 );
	}

	if (getEnabled())
	{
		gGL.color4fv( mEnabledColor.mV );
	}
	else
	{
		gGL.color4fv( mDisabledColor.mV );
	}
	const S32 y = getRect().getHeight() / 3;
	const S32 PAD = 6;
	gl_line_2d( PAD, y, getRect().getWidth() - PAD, y );
	gl_line_2d( PAD, y * 2, getRect().getWidth() - PAD, y * 2 );
}

U32 LLMenuItemTearOffGL::getNominalHeight( void ) const 
{ 
	return TEAROFF_SEPARATOR_HEIGHT_PIXELS; 
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemBlankGL
//
// This class represents a blank, non-functioning item.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemBlankGL final : public LLMenuItemGL
{
public:
	LLMenuItemBlankGL(const std::string& name = LLStringUtil::null) : LLMenuItemGL(name, LLStringUtil::null)
	{
		setEnabled(FALSE);
	}
	void onCommit() override {}
	void draw() override {}
};


///============================================================================
/// Class LLMenuItemCallGL
///============================================================================

LLMenuItemCallGL::LLMenuItemCallGL( const std::string& name, 
									const std::string& label, 
									menu_callback clicked_cb, 
								    enabled_callback enabled_cb,
									void* user_data,
									KEY key, MASK mask,
									BOOL enabled,
									on_disabled_callback on_disabled_cb) :
	LLMenuItemGL( name, label, key, mask ),
	mCallback( clicked_cb ),
	mEnabledCallback( enabled_cb ),
	mLabelCallback(NULL),
	mUserData( user_data ),
	mOnDisabledCallback(on_disabled_cb)
{
	if(!enabled) setEnabled(FALSE);
}

LLMenuItemCallGL::LLMenuItemCallGL( const std::string& name, 
									menu_callback clicked_cb, 
								    enabled_callback enabled_cb,
									void* user_data,
									KEY key, MASK mask,
									BOOL enabled,
									on_disabled_callback on_disabled_cb) :
	LLMenuItemGL( name, name, key, mask ),
	mCallback( clicked_cb ),
	mEnabledCallback( enabled_cb ),
	mLabelCallback(NULL),
	mUserData( user_data ),
	mOnDisabledCallback(on_disabled_cb)
{
	if(!enabled) setEnabled(FALSE);
}

LLMenuItemCallGL::LLMenuItemCallGL(const std::string& name,
								   const std::string& label,
								   menu_callback clicked_cb,
								   enabled_callback enabled_cb,
								   label_callback label_cb,
								   void* user_data,
								   KEY key, MASK mask,
								   BOOL enabled,
								   on_disabled_callback on_disabled_cb) :
	LLMenuItemGL(name, label, key, mask),
	mCallback(clicked_cb),
	mEnabledCallback(enabled_cb),
	mLabelCallback(label_cb),
	mUserData(user_data),
	mOnDisabledCallback(on_disabled_cb)
{
	if(!enabled) setEnabled(FALSE);
}

LLMenuItemCallGL::LLMenuItemCallGL(const std::string& name,
								   menu_callback clicked_cb,
								   enabled_callback enabled_cb,
								   label_callback label_cb,
								   void* user_data,
								   KEY key, MASK mask,
								   BOOL enabled,
								   on_disabled_callback on_disabled_cb) :
	LLMenuItemGL(name, name, key, mask),
	mCallback(clicked_cb),
	mEnabledCallback(enabled_cb),
	mLabelCallback(label_cb),
	mUserData(user_data),
	mOnDisabledCallback(on_disabled_cb)
{
	if(!enabled) setEnabled(FALSE);
}

void LLMenuItemCallGL::setEnabledControl(std::string enabled_control, LLView *context)
{
	// Register new listener
	if (!enabled_control.empty())
	{
		LLControlVariable *control = context->findControl(enabled_control);
		if (!control)
		{
			context->addBoolControl(enabled_control, getEnabled());
			control = context->findControl(enabled_control);
			llassert_always(control);
		}
		control->getSignal()->connect(boost::bind(&LLView::controlListener, _2, getHandle(), std::string("enabled")));
		setEnabled(control->getValue());
	}
}

void LLMenuItemCallGL::setVisibleControl(std::string visible_control, LLView *context)
{
	// Register new listener
	if (!visible_control.empty())
	{
		LLControlVariable *control = context->findControl(visible_control);
		if (!control)
		{
			context->addBoolControl(visible_control, getVisible());
			control = context->findControl(visible_control);
			llassert_always(control);
		}
		control->getSignal()->connect(boost::bind(&LLView::controlListener, _2, getHandle(), std::string("visible")));
		setVisible(control->getValue());
	}
}

// virtual
LLXMLNodePtr LLMenuItemCallGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemGL::getXML();

	node->setName(LL_MENU_ITEM_CALL_GL_TAG);

	// Contents

	std::vector<LLListenerEntry> listeners = mDispatcher->getListeners();
	std::vector<LLListenerEntry>::iterator itor;
	for (itor = listeners.begin(); itor != listeners.end(); ++itor)
	{
		std::string listener_name = findEventListener((LLSimpleListener*)itor->listener);
		if (!listener_name.empty())
		{
			// *FIX:  It's not always on_click.	 It could be on_check, on_enable or on_visible,
			// but there's no way to get that from the data that is stored.

			LLXMLNodePtr child_node = node->createChild("on_click", FALSE);
			child_node->createChild("function", TRUE)->setStringValue(listener_name);
			child_node->createChild("filter", TRUE)->setStringValue(itor->filter.asString());
			child_node->createChild("userdata", TRUE)->setStringValue(itor->userdata.asString());
		}
	}

	return node;
}

void LLMenuItemCallGL::onCommit( void )
{
	// RN: menu item can be deleted in callback, so beware
	getMenu()->setItemLastSelected( this );

	if( mCallback )
	{
		mCallback( mUserData );
	}
	LLPointer<LLEvent> fired_event = new LLEvent(this);
	fireEvent(fired_event, "on_click");
	LLMenuItemGL::onCommit();
}

void LLMenuItemCallGL::updateEnabled( void )
{
	if( mEnabledCallback )
	{
		setEnabled( mEnabledCallback( mUserData ) );
	}
}

void LLMenuItemCallGL::buildDrawLabel( void )
{
	updateEnabled();
	
	LLPointer<LLEvent> fired_event = new LLEvent(this);
	fireEvent(fired_event, "on_build");
	
	if(mLabelCallback)
	{
		std::string label;
		mLabelCallback(label, mUserData);
		mLabel = label;
	}
	LLMenuItemGL::buildDrawLabel();
}

BOOL LLMenuItemCallGL::handleKeyHere( KEY key, MASK mask )
{
	return LLMenuItemGL::handleKeyHere(key, mask);
}

BOOL LLMenuItemCallGL::handleAcceleratorKey( KEY key, MASK mask )
{
 	if( (!gKeyboard->getKeyRepeated(key) || getAllowKeyRepeat()) && (key == mAcceleratorKey) && (mask == (mAcceleratorMask & MASK_NORMALKEYS)) )
	{
		LLPointer<LLEvent> fired_event = new LLEvent(this);
		fireEvent(fired_event, "on_build");
		updateEnabled();
		if( !getEnabled() )
		{
			if( mOnDisabledCallback )
			{
				mOnDisabledCallback( mUserData );
			}
		}
	}
	return LLMenuItemGL::handleAcceleratorKey(key, mask);
}

///============================================================================
/// Class LLMenuItemCheckGL
///============================================================================

LLMenuItemCheckGL::LLMenuItemCheckGL ( const std::string& name, 
									   const std::string& label,
									   menu_callback clicked_cb,
									   enabled_callback enabled_cb,
									   check_callback check_cb,
									   void* user_data,
									   KEY key, MASK mask ) :
	LLMenuItemCallGL( name, label, clicked_cb, enabled_cb, user_data, key, mask ),
	mCheckCallback( check_cb ), 
	mChecked(FALSE)
{
}

LLMenuItemCheckGL::LLMenuItemCheckGL ( const std::string& name, 
									   menu_callback clicked_cb,
									   enabled_callback enabled_cb,
									   check_callback check_cb,
									   void* user_data,
									   KEY key, MASK mask ) :
	LLMenuItemCallGL( name, name, clicked_cb, enabled_cb, user_data, key, mask ),
	mCheckCallback( check_cb ), 
	mChecked(FALSE)
{
}

LLMenuItemCheckGL::LLMenuItemCheckGL ( const std::string& name, 
									   const std::string& label,
									   menu_callback clicked_cb,
									   enabled_callback enabled_cb,
									   std::string control_name,
									   LLView *context,
									   void* user_data,
									   KEY key, MASK mask ) :
	LLMenuItemCallGL( name, label, clicked_cb, enabled_cb, user_data, key, mask ),
	mCheckCallback( NULL )
{
	setControlName(control_name, context);
}

//virtual
void LLMenuItemCheckGL::setValue(const LLSD& value)
{
	mChecked = value.asBoolean();
	if(mChecked)
	{
		mDrawBoolLabel = LLMenuGL::BOOLEAN_TRUE_PREFIX;
	}
	else
	{
		mDrawBoolLabel.clear();
	}
}

void LLMenuItemCheckGL::setCheckedControl(std::string checked_control, LLView *context)
{
	// Register new listener
	if (!checked_control.empty())
	{
		LLControlVariable *control = context->findControl(checked_control);
		if (!control)
		{
			context->addBoolControl(checked_control, mChecked);
			control = context->findControl(checked_control);
			llassert_always(control);
		}
		control->getSignal()->connect(boost::bind(&LLView::controlListener, _2, getHandle(), std::string("value")));
		mChecked = control->getValue();
	}
}

// virtual
LLXMLNodePtr LLMenuItemCheckGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemCallGL::getXML();

	node->setName(LL_MENU_ITEM_CHECK_GL_TAG);

	return node;
}

// called to rebuild the draw label
void LLMenuItemCheckGL::buildDrawLabel( void )
{
	if(mChecked || (mCheckCallback && mCheckCallback( getUserData() ) ) )
	{
		mDrawBoolLabel = LLMenuGL::BOOLEAN_TRUE_PREFIX;
	}
	else
	{
		mDrawBoolLabel.clear();
	}
	LLMenuItemCallGL::buildDrawLabel();
}


///============================================================================
/// Class LLMenuItemToggleGL
///============================================================================

LLMenuItemToggleGL::LLMenuItemToggleGL( const std::string& name, const std::string& label, BOOL* toggle,
										KEY key, MASK mask ) :
	LLMenuItemGL( name, label, key, mask ),
	mToggle( toggle )
{
}

LLMenuItemToggleGL::LLMenuItemToggleGL( const std::string& name, BOOL* toggle,
										KEY key, MASK mask ) :
	LLMenuItemGL( name, name, key, mask ),
	mToggle( toggle )
{
}


// called to rebuild the draw label
void LLMenuItemToggleGL::buildDrawLabel( void )
{
	if( *mToggle )
	{
		mDrawBoolLabel = LLMenuGL::BOOLEAN_TRUE_PREFIX;
	}
	else
	{
		mDrawBoolLabel.clear();
	}
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString( st );
	mDrawAccelLabel = st;
}

// onCommit() - do the primary funcationality of the menu item.
void LLMenuItemToggleGL::onCommit( void )
{
	getMenu()->setItemLastSelected( this );
	//LL_INFOS() << "LLMenuItemToggleGL::onCommit " << mLabel.c_str() << LL_ENDL;
	*mToggle = !(*mToggle);
	buildDrawLabel();
	LLMenuItemGL::onCommit();
}


LLMenuItemBranchGL::LLMenuItemBranchGL( const std::string& name, const std::string& label, LLHandle<LLView> branch_handle,
										KEY key, MASK mask ) :
	LLMenuItemGL( name, label, key, mask )
{
	LLMenuGL* branch = dynamic_cast<LLMenuGL*>(branch_handle.get());
	if(!branch)
	{
		LL_ERRS() << "Non-menu handle passed as branch reference." << LL_ENDL;
	}

	if (branch)
	{
		mBranchHandle = branch->getHandle();
		branch->setVisible(FALSE);
		branch->setParentMenuItem(this);
	}
}

LLMenuItemBranchGL::~LLMenuItemBranchGL()
{
	if (mBranchHandle.get())
	{
		mBranchHandle.get()->die();
	}
}



// virtual
LLView* LLMenuItemBranchGL::getChildView(const std::string& name, BOOL recurse, BOOL create_if_missing) const
{
	LLMenuGL* branch = getBranch();
	if (branch)
	{
		if (branch->getName() == name)
		{
			return branch;
		}

		// Always recurse on branches
		LLView* child = branch->getChildView(name, recurse, FALSE);
		if(child)
		{
			return child;
		}
	}
	return LLView::getChildView(name, recurse, create_if_missing);
}

// virtual
BOOL LLMenuItemBranchGL::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// switch to mouse navigation mode
	LLMenuGL::setKeyboardMode(FALSE);

	onCommit();
	make_ui_sound("UISndClickRelease");
	return TRUE;
}

BOOL LLMenuItemBranchGL::handleAcceleratorKey(KEY key, MASK mask)
{
	return getBranch() && getBranch()->handleAcceleratorKey(key, mask);
}

// virtual
LLXMLNodePtr LLMenuItemBranchGL::getXML(bool save_children) const
{
	if (getBranch())
	{
		return getBranch()->getXML();
	}

	return LLMenuItemGL::getXML();
}


// This function checks to see if the accelerator key is already in use;
// if not, it will be added to the list
BOOL LLMenuItemBranchGL::addToAcceleratorList(std::list<LLKeyBinding*> *listp)
{
	LLMenuGL* branch = getBranch();
	if (!branch)
		return FALSE;

	U32 item_count = branch->getItemCount();
	LLMenuItemGL *item;
	
	while (item_count--)
	{
		if ((item = branch->getItem(item_count)))
		{
			return item->addToAcceleratorList(listp);
		}
	}

	return FALSE;
}


// called to rebuild the draw label
void LLMenuItemBranchGL::buildDrawLabel( void )
{
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString( st );
	mDrawAccelLabel = st;
	mDrawBranchLabel = LLMenuGL::BRANCH_SUFFIX;
}

void LLMenuItemBranchGL::onCommit( void )
{
	openMenu();

	// keyboard navigation automatically propagates highlight to sub-menu
	// to facilitate fast menu control via jump keys
	if (LLMenuGL::getKeyboardMode() && getBranch()&& !getBranch()->getHighlightedItem())
	{
		getBranch()->highlightNextItem(NULL);
	}

	LLUICtrl::onCommit();
}

BOOL LLMenuItemBranchGL::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
	BOOL handled = FALSE;
	if (getBranch() && called_from_parent)
	{
		handled = getBranch()->handleKey(key, mask, called_from_parent);
	}

	if (!handled)
	{
		handled = LLMenuItemGL::handleKey(key, mask, called_from_parent);
	}

	return handled;
}

BOOL LLMenuItemBranchGL::handleUnicodeChar(llwchar uni_char, BOOL called_from_parent)
{
	BOOL handled = FALSE;
	if (getBranch() && called_from_parent)
	{
		handled = getBranch()->handleUnicodeChar(uni_char, TRUE);
	}

	if (!handled)
	{
		handled = LLMenuItemGL::handleUnicodeChar(uni_char, called_from_parent);
	}

	return handled;
}


void LLMenuItemBranchGL::setHighlight( BOOL highlight )
{
	if (highlight == getHighlight())
		return;

	LLMenuGL* branch = getBranch();
	if (!branch)
		return;

	BOOL auto_open = getEnabled() && (!branch->getVisible() || branch->getTornOff());
	// torn off menus don't open sub menus on hover unless they have focus
	LLFloater * menu_parent = dynamic_cast<LLFloater *>(getMenu()->getParent());
	if (getMenu()->getTornOff() && menu_parent && !menu_parent->hasFocus())
	{
		auto_open = FALSE;
	}
	// don't auto open torn off sub-menus (need to explicitly active menu item to give them focus)
	if (branch->getTornOff())
	{
		auto_open = FALSE;
	}
	LLMenuItemGL::setHighlight(highlight);
	if( highlight )
	{
		if(auto_open)
		{
			openMenu();
		}
	}
	else
	{
		if (branch->getTornOff())
		{
			LLFloater * branch_parent = dynamic_cast<LLFloater *>(branch->getParent());
			if (branch_parent)
			{
				branch_parent->setFocus(FALSE);
			}
			branch->clearHoverItem();
		}
		else
		{
			branch->setVisible( FALSE );
		}
	}
}

void LLMenuItemBranchGL::draw()
{
	LLMenuItemGL::draw();
	if (getBranch() && getBranch()->getVisible() && !getBranch()->getTornOff())
	{
		setHighlight(TRUE);
	}
}

void LLMenuItemBranchGL::updateBranchParent(LLView* parentp)
{
	if (getBranch() && getBranch()->getParent() == NULL)
	{
		// make the branch menu a sibling of my parent menu
		getBranch()->updateParent(parentp);
	}
}

void LLMenuItemBranchGL::handleVisibilityChange( BOOL new_visibility )
{
	if (new_visibility == FALSE && getBranch() && !getBranch()->getTornOff())
	{
		LL_DEBUGS() << "Forcing branch to visible. Menu: " << getName() << " Branch: " << getBranch()->getName() << LL_ENDL;
		getBranch()->setVisible(FALSE);
	}
	LLMenuItemGL::handleVisibilityChange(new_visibility);
}

BOOL LLMenuItemBranchGL::handleKeyHere( KEY key, MASK mask )
{
	LLMenuGL* branch = getBranch();
	if (!branch)
		return LLMenuItemGL::handleKeyHere(key, mask);

	// an item is highlighted, my menu is open, and I have an active sub menu or we are in
	// keyboard navigation mode
	if (getHighlight() 
		&& getMenu()->isOpen() 
		&& (isActive() || LLMenuGL::getKeyboardMode()))
	{
		if (branch->getVisible() && key == KEY_LEFT)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			BOOL handled = branch->clearHoverItem();
			if (branch->getTornOff())
			{
				LLFloater * branch_parent = dynamic_cast<LLFloater *>(branch->getParent());
				if (branch_parent)
				{
					branch_parent->setFocus(FALSE);
				}
			}
			if (handled && getMenu()->getTornOff())
			{
				LLFloater * menu_parent = dynamic_cast<LLFloater *>(getMenu()->getParent());
				if (menu_parent)
				{
					menu_parent->setFocus(TRUE);
				}
			}
			return handled;
		}

		if (key == KEY_RIGHT && !branch->getHighlightedItem())
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			LLMenuItemGL* itemp = branch->highlightNextItem(NULL);
			if (itemp)
			{
				return TRUE;
			}
		}
	}
	return LLMenuItemGL::handleKeyHere(key, mask);
}

//virtual
BOOL LLMenuItemBranchGL::isActive() const
{
	return isOpen() && getBranch() && getBranch()->getHighlightedItem();
}

//virtual
BOOL LLMenuItemBranchGL::isOpen() const
{
	return getBranch() && getBranch()->isOpen();
}

void LLMenuItemBranchGL::openMenu()
{
	LLMenuGL* branch = getBranch();
	if (!branch)
		return;

	if (branch->getTornOff())
	{
		LLFloater * branch_parent = dynamic_cast<LLFloater *>(branch->getParent());
		if (branch_parent)
		{
			gFloaterView->bringToFront(branch_parent);
			// this might not be necessary, as torn off branches don't get focus and hence no highligth
			branch->highlightNextItem(NULL);
		}
	}
	else if( !branch->getVisible() )
	{
		// get valid rectangle for menus
		const LLRect menu_region_rect = LLMenuGL::sMenuContainer->getMenuRect();

		branch->arrange();

		LLRect branch_rect = branch->getRect();
		// calculate root-view relative position for branch menu
		S32 left = getRect().mRight;
		S32 top = getRect().mTop - getRect().mBottom;

		localPointToOtherView(left, top, &left, &top, branch->getParent());

		branch_rect.setLeftTopAndSize( left, top,
								branch_rect.getWidth(), branch_rect.getHeight() );

		if (branch->getCanTearOff())
		{
			branch_rect.translate(0, TEAROFF_SEPARATOR_HEIGHT_PIXELS);
		}
		branch->setRect( branch_rect );
		
		// if branch extends outside of menu region change the direction it opens in
		S32 x, y;
		S32 delta_x = 0;
		S32 delta_y = 0;
		branch->localPointToOtherView( 0, 0, &x, &y, branch->getParent() ); 
		
		F32 center_y = top - (getRect().getHeight() / 2.f);

		if( y < menu_region_rect.mBottom  && center_y <= menu_region_rect.getCenterY())
		{
			// open upwards if menu extends past bottom
			// adjust by the height of the menu item branch since it is a submenu
			delta_y = branch_rect.getHeight() - getRect().getHeight();
			// Singu: Handle menu clipping off the top of the screen too.
			if ( branch_rect.mTop + delta_y > menu_region_rect.mTop )
			{
				delta_y -= branch_rect.mTop + delta_y - menu_region_rect.mTop;
				delta_y -= delta_y % getRect().getHeight();
			}
		}

		if( x + branch_rect.getWidth() > menu_region_rect.mRight )
		{
			// move sub-menu over to left side
			delta_x = llmax(-x, ( -(branch_rect.getWidth() + getRect().getWidth())));
		}
		branch->translate( delta_x, delta_y );

		branch->setVisible( TRUE );
		branch->getParent()->sendChildToFront(branch);
	}
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemBranchDownGL
//
// The LLMenuItemBranchDownGL represents a menu item that has a
// sub-menu. This is used to make menu bar menus.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemBranchDownGL : public LLMenuItemBranchGL
{
protected:

public:
	LLMenuItemBranchDownGL( const std::string& name, const std::string& label, LLHandle<LLView> branch,
							KEY key = KEY_NONE, MASK mask = MASK_NONE );

	virtual std::string getType() const	{ return "menu"; }

	// returns the normal width of this control in pixels - this is
	// used for calculating the widest item, as well as for horizontal
	// arrangement.
	virtual U32 getNominalWidth( void ) const;

	// called to rebuild the draw label
	virtual void buildDrawLabel( void );

	// handles opening, positioning, and arranging the menu branch associated with this item
	virtual void openMenu( void );

	// set the hover status (called by it's menu) and if the object is
	// active. This is used for behavior transfer.
	virtual void setHighlight( BOOL highlight );

	virtual BOOL isActive( void ) const;

	// LLView functionality
	virtual BOOL handleMouseDown( S32 x, S32 y, MASK mask );
	virtual BOOL handleMouseUp( S32 x, S32 y, MASK mask ); 
	virtual void draw( void );
	virtual BOOL handleKeyHere(KEY key, MASK mask);
	
	virtual BOOL handleAcceleratorKey(KEY key, MASK mask);
};

LLMenuItemBranchDownGL::LLMenuItemBranchDownGL( const std::string& name,
												const std::string& label,
												LLHandle<LLView> branch, 
												KEY key, MASK mask ) :
	LLMenuItemBranchGL( name, label, branch, key, mask )
{
}

// returns the normal width of this control in pixels - this is used
// for calculating the widest item, as well as for horizontal
// arrangement.
U32 LLMenuItemBranchDownGL::getNominalWidth( void ) const
{
	U32 width = LEFT_PAD_PIXELS + LEFT_WIDTH_PIXELS + RIGHT_PAD_PIXELS;
	width += getFont()->getWidth( mLabel.getWString().c_str() ); 
	return width;
}

// called to rebuild the draw label
void LLMenuItemBranchDownGL::buildDrawLabel( void )
{
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString( st );
	mDrawAccelLabel = st;
}

void LLMenuItemBranchDownGL::openMenu( void )
{
	LLMenuGL* branch = getBranch();
	if( branch->getVisible() && !branch->getTornOff() )
	{
		branch->setVisible( FALSE );
	}
	else
	{
		if (branch->getTornOff())
		{
			LLFloater * branch_parent = dynamic_cast<LLFloater *>(branch->getParent());
			if (branch_parent)
			{
				gFloaterView->bringToFront(branch_parent);
			}
		}
		else
		{
			// We're showing the drop-down menu, so patch up its labels/rects
			branch->arrange();

			LLRect rect = branch->getRect();
			S32 left = 0;
			S32 top = getRect().mBottom;
			localPointToOtherView(left, top, &left, &top, branch->getParent());

			rect.setLeftTopAndSize( left, top,
									rect.getWidth(), rect.getHeight() );
			branch->setRect( rect );
			S32 x = 0;
			S32 y = 0;
			branch->localPointToScreen( 0, 0, &x, &y ); 
			S32 delta_x = 0;

			LLCoordScreen window_size;
			LLWindow* windowp = getWindow();
			windowp->getSize(&window_size);

			S32 window_width = window_size.mX;
			if( x > window_width - rect.getWidth() )
			{
				delta_x = (window_width - rect.getWidth()) - x;
			}
			branch->translate( delta_x, 0 );

			setHighlight(TRUE);
			branch->setVisible( TRUE );
			branch->getParent()->sendChildToFront(branch);
		}
	}
}

// set the hover status (called by it's menu)
void LLMenuItemBranchDownGL::setHighlight( BOOL highlight )
{
 	if (highlight == getHighlight())
		return;

	//NOTE: Purposely calling all the way to the base to bypass auto-open.
	LLMenuItemGL::setHighlight(highlight);

	LLMenuGL* branch = getBranch();
	if (!branch)
		return;
	
	if( !highlight)
	{
		if (branch->getTornOff())
		{
			LLFloater * branch_parent = dynamic_cast<LLFloater *>(branch->getParent());
			if (branch_parent)
			{
				branch_parent->setFocus(FALSE);
			}
			branch->clearHoverItem();
		}
		else
		{
			branch->setVisible( FALSE );
		}
	}
}

BOOL LLMenuItemBranchDownGL::isActive() const
{
	// for top level menus, being open is sufficient to be considered 
	// active, because clicking on them with the mouse will open
	// them, without moving keyboard focus to them
	return isOpen();
}

BOOL LLMenuItemBranchDownGL::handleMouseDown( S32 x, S32 y, MASK mask )
{
	// switch to mouse control mode
	LLMenuGL::setKeyboardMode(FALSE);
	onCommit();
	make_ui_sound("UISndClick");
	setVisible(TRUE);
	return TRUE;
}

BOOL LLMenuItemBranchDownGL::handleMouseUp( S32 x, S32 y, MASK mask )
{
	return TRUE;
}


BOOL LLMenuItemBranchDownGL::handleAcceleratorKey(KEY key, MASK mask)
{
	BOOL branch_visible = getBranch()->getVisible();
	BOOL handled = getBranch()->handleAcceleratorKey(key, mask);
	if (handled && !branch_visible && getVisible())
	{
		// flash this menu entry because we triggered an invisible menu item
		LLMenuHolderGL::setActivatedItem(this);
	}

	return handled;
}

BOOL LLMenuItemBranchDownGL::handleKeyHere(KEY key, MASK mask)
{
	BOOL menu_open = getBranch()->getVisible();
	// don't do keyboard navigation of top-level menus unless in keyboard mode, or menu expanded
	if (getHighlight() && getMenu()->isOpen() && (isActive() || LLMenuGL::getKeyboardMode()))
	{
		if (key == KEY_LEFT)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			LLMenuItemGL* itemp = getMenu()->highlightPrevItem(this);
			// open new menu only if previous menu was open
			if (itemp && itemp->getEnabled() && menu_open)
			{
				itemp->onCommit();
			}

			return TRUE;
		}
		else if (key == KEY_RIGHT)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			LLMenuItemGL* itemp = getMenu()->highlightNextItem(this);
			// open new menu only if previous menu was open
			if (itemp && itemp->getEnabled() && menu_open)
			{
				itemp->onCommit();
			}

			return TRUE;
		}
		else if (key == KEY_DOWN)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			if (!isActive())
			{
				onCommit();
			}
			getBranch()->highlightNextItem(NULL);
			return TRUE;
		}
		else if (key == KEY_UP)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(TRUE);

			if (!isActive())
			{
				onCommit();
			}
			getBranch()->highlightPrevItem(NULL);
			return TRUE;
		}
	}

	return FALSE;
}

void LLMenuItemBranchDownGL::draw( void )
{
	//FIXME: try removing this
	if (getBranch()->getVisible() && !getBranch()->getTornOff())
	{
		setHighlight(TRUE);
	}

	if( getHighlight() )
	{
		gGL.color4fv( mHighlightBackground.mV );
		gl_rect_2d( 0, getRect().getHeight(), getRect().getWidth(), 0 );
	}

	LLFontGL::ShadowType font_shadow = LLFontGL::NO_SHADOW;
	if (getEnabled() && !getDrawTextDisabled() )
	{
		font_shadow = LLFontGL::DROP_SHADOW_SOFT;
	}

	LLColor4 color;
	if (getHighlight())
	{
		color = mHighlightForeground;
	}
	else if( getEnabled() )
	{
		color = mEnabledColor;
	}
	else
	{
		color = mDisabledColor;
	}
	getFont()->render( mLabel.getWString(), 0, (F32)getRect().getWidth() / 2.f, (F32)LABEL_BOTTOM_PAD_PIXELS, color,
				   LLFontGL::HCENTER, LLFontGL::BOTTOM, getFontStyle(), font_shadow );

	// underline navigation key only when keyboard navigation has been initiated
	if (getMenu()->jumpKeysActive() && LLMenuGL::getKeyboardMode())
	{
		std::string upper_case_label = mLabel.getString();
		LLStringUtil::toUpper(upper_case_label);
		std::string::size_type offset = upper_case_label.find(getJumpKey());
		if (offset != std::string::npos)
		{
			const LLWString& utf32text = mLabel.getWString();
			S32 x_offset = ll_round((F32)getRect().getWidth() / 2.f - getFont()->getWidthF32(utf32text, 0, S32_MAX) / 2.f);
			S32 x_begin = x_offset + getFont()->getWidth(utf32text, 0, offset);
			S32 x_end = x_offset + getFont()->getWidth(utf32text, 0, offset + 1);
			gl_line_2d(x_begin, LABEL_BOTTOM_PAD_PIXELS, x_end, LABEL_BOTTOM_PAD_PIXELS);
		}
	}

	// reset every frame so that we only show highlight 
	// when we get hover events on that frame
	setHover(FALSE);
}


class LLMenuScrollItem : public LLMenuItemCallGL
{
public:
	enum EArrowType
	{
		ARROW_DOWN,
		ARROW_UP
	};
	struct ArrowTypes : public LLInitParam::TypeValuesHelper<EArrowType, ArrowTypes>
	{
		static void declareValues()
		{
			declare("up", ARROW_UP);
			declare("down", ARROW_DOWN);
		}
	};

	struct Params : public LLInitParam::Block<Params, LLMenuItemCallGL::Params>
	{
		Optional<EArrowType, ArrowTypes> arrow_type;
		Optional<CommitCallbackParam> scroll_callback;
	};

protected:
	LLMenuScrollItem(const Params&);
	friend class LLUICtrlFactory;

public:
	/*virtual*/ void draw();
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent);
	/*virtual*/ void setEnabled(BOOL enabled);
	virtual void onCommit( void );

private:
	LLButton*				mArrowBtn;
};

LLMenuScrollItem::LLMenuScrollItem(const Params& p)
:	LLMenuItemCallGL(p.name, NULL/*p*/)
{
	std::string icon;
	if (p.arrow_type.isProvided() && p.arrow_type == ARROW_UP)
	{
		icon = "arrow_up.tga";
	}
	else
	{
		icon = "arrow_down.tga";
	}

	/* Singu TODO: LLButton::Params
	LLButton::Params bparams;

	// Disabled the Return key handling by LLMenuScrollItem instead of
	// passing the key press to the currently selected menu item. See STORM-385.
	bparams.commit_on_return(false);
	bparams.mouse_opaque(true);
	bparams.scale_image(false);
	bparams.click_callback(p.scroll_callback);
	bparams.mouse_held_callback(p.scroll_callback);
	bparams.follows.flags(FOLLOWS_ALL);
	std::string background = "transparent.j2c";
	bparams.image_unselected.name(background);
	bparams.image_disabled.name(background);
	bparams.image_selected.name(background);
	bparams.image_hover_selected.name(background);
	bparams.image_disabled_selected.name(background);
	bparams.image_hover_unselected.name(background);
	bparams.image_overlay.name(icon);

	mArrowBtn = LLUICtrlFactory::create<LLButton>(bparams);
	*/
	const LLRect rect = getRect();
	const std::string background = "transparent.j2c";
	mArrowBtn = new LLButton("", LLRect(0, 0, rect.getWidth(), rect.getHeight()), background, background, "", NULL);
	mArrowBtn->setCommitOnReturn(false);
	mArrowBtn->setMouseOpaque(true);
	mArrowBtn->setScaleImage(false);
	mArrowBtn->setClickedCallback(p.scroll_callback);
	mArrowBtn->setHeldDownCallback(p.scroll_callback);
	mArrowBtn->setFollows(FOLLOWS_ALL);
	mArrowBtn->setImageOverlay(icon);
	addChild(mArrowBtn);
}

/*virtual*/
void LLMenuScrollItem::draw()
{
	LLUICtrl::draw();
}

/*virtual*/
void LLMenuScrollItem::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	mArrowBtn->reshape(width, height, called_from_parent);
	LLView::reshape(width, height, called_from_parent);
}

/*virtual*/
void LLMenuScrollItem::setEnabled(BOOL enabled)
{
	mArrowBtn->setEnabled(enabled);
	LLView::setEnabled(enabled);
}

void LLMenuScrollItem::onCommit( void )
{
	LLUICtrl::onCommit();
}

///============================================================================
/// Class LLMenuGL
///============================================================================

static LLRegisterWidget<LLMenuGL> r1("menu");

// Default constructor
LLMenuGL::LLMenuGL( const std::string& name, const std::string& label )
:	LLUICtrl( name, LLRect(), FALSE),
	mBackgroundColor( sDefaultBackgroundColor ),
	mBgVisible( TRUE ),
	mHasSelection(false),
	mHorizontalLayout(false),
	mScrollable(mHorizontalLayout ? FALSE : /*p.scrollable*/false), // Scrolling is supported only for vertical layout
	mMaxScrollableItems(/*p.max_scrollable_items*/ U32_MAX),
	mLabel( label ),
	mDropShadowed( TRUE ),
	mKeepFixedSize( FALSE ),
	mLastMouseX(0),
	mLastMouseY(0),
	mMouseVelX(0),
	mMouseVelY(0),
	mTornOff(FALSE),
	mTearOffItem(NULL),
	mSpilloverBranch(NULL),
	mFirstVisibleItem(NULL),
	mArrowUpItem(NULL),
	mArrowDownItem(NULL),
	mSpilloverMenu(NULL),
	mJumpKey(KEY_NONE),
	mCreateJumpKeys(true/*p.create_jump_keys*/),
	mNeedsArrange(FALSE),
	mResetScrollPositionOnShow(true),
	mShortcutPad(ACCEL_PAD_PIXELS)
{
	mFadeTimer.stop();
	setTabStop(FALSE);
}

LLMenuGL::LLMenuGL( const std::string& label)
:	LLUICtrl( label, LLRect(), FALSE),
	mBackgroundColor( sDefaultBackgroundColor ),
	mBgVisible( TRUE ),
	mHasSelection(false),
	mHorizontalLayout(false),
	mScrollable(mHorizontalLayout ? FALSE : /*p.scrollable*/false), // Scrolling is supported only for vertical layout
	mMaxScrollableItems(/*p.max_scrollable_items*/ U32_MAX),
	mLabel( label ),
	mDropShadowed( TRUE ),
	mKeepFixedSize( FALSE ),
	mLastMouseX(0),
	mLastMouseY(0),
	mMouseVelX(0),
	mMouseVelY(0),
	mTornOff(FALSE),
	mTearOffItem(NULL),
	mSpilloverBranch(NULL),
	mSpilloverMenu(NULL),
	mFirstVisibleItem(NULL),
	mArrowUpItem(NULL),
	mArrowDownItem(NULL),
	mJumpKey(KEY_NONE),
	mCreateJumpKeys(true/*p.create_jump_keys*/),
	mNeedsArrange(FALSE),
	mResetScrollPositionOnShow(true),
	mShortcutPad(ACCEL_PAD_PIXELS)
{
	mFadeTimer.stop();
	setTabStop(FALSE);
}

// Destroys the object
LLMenuGL::~LLMenuGL( void )
{
	// delete the branch, as it might not be in view hierarchy
	// leave the menu, because it is always in view hierarchy
	delete mSpilloverBranch;
	mJumpKeys.clear();
}

void LLMenuGL::setCanTearOff(BOOL tear_off)
{
	if (tear_off && mTearOffItem == NULL)
	{
		mTearOffItem = new LLMenuItemTearOffGL();
		addChild(mTearOffItem);
	}
	else if (!tear_off && mTearOffItem != NULL)
	{
		mItems.remove(mTearOffItem);
		removeChild(mTearOffItem);
		delete mTearOffItem;
		mTearOffItem = NULL;
		needsArrange();
	}
}

// virtual
LLXMLNodePtr LLMenuGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLView::getXML();

	node->setName(LL_MENU_GL_TAG);

	// Attributes

	node->createChild("opaque", TRUE)->setBoolValue(mBgVisible);

	node->createChild("drop_shadow", TRUE)->setBoolValue(mDropShadowed);

	node->createChild("tear_off", TRUE)->setBoolValue((mTearOffItem != NULL));

	if (mBgVisible)
	{
		// TomY TODO: this should save out the color control name
		node->createChild("color", TRUE)->setFloatValue(4, mBackgroundColor.mV);
	}

	// Contents
	item_list_t::const_iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		LLView* child = (*item_iter);
		LLMenuItemGL* item = (LLMenuItemGL*)child;

		LLXMLNodePtr child_node = item->getXML();

		node->addChild(child_node);
	}

	return node;
}

void LLMenuGL::parseChildXML(LLXMLNodePtr child, LLView *parent)
{
	std::string name(child->getName()->mString);
	if (child->hasName(LL_MENU_GL_TAG))
	{
		// SUBMENU
		LLMenuGL *submenu = (LLMenuGL*)LLMenuGL::fromXML(child, parent, LLUICtrlFactory::getInstance());
		appendMenu(submenu);
		if (LLMenuGL::sMenuContainer != NULL)
		{
			submenu->updateParent(LLMenuGL::sMenuContainer);
		}
		else
		{
			submenu->updateParent(parent);
		}
	}
	else if (child->hasName(LL_MENU_ITEM_SEPARATOR_GL_TAG))
	{
		std::string item_name;
		child->getAttributeString("name", item_name);
		LLMenuItemGL* separator = new LLMenuItemSeparatorGL(item_name);
		addChild(separator);
	}
	else if(child->hasName(LL_MENU_ITEM_CALL_GL_TAG) || 
			child->hasName(LL_MENU_ITEM_CHECK_GL_TAG))
	{
		// ITEM
		std::string type;
		std::string item_name;
		std::string source_label;
		std::string item_label;
		KEY		 jump_key = KEY_NONE;

		child->getAttributeString("type", type);
		child->getAttributeString("name", item_name);
		child->getAttributeString("label", source_label);

		LLStringUtil::format(source_label, LLTrans::getDefaultArgs());
		
		// parse jump key out of label
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep("_");
		tokenizer tokens(source_label, sep);
		tokenizer::iterator token_iter;
		S32 token_count = 0;
		for( token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
		{
			item_label += (*token_iter);
			if (token_count > 0)
			{
				jump_key = (*token_iter).c_str()[0];
			}
			++token_count;
		}

		MASK mask = 0;
					
#ifdef LL_DARWIN
		// See if this Mac accelerator should really use the ctrl key and not get mapped to cmd
		BOOL useMacCtrl = FALSE;
		child->getAttributeBOOL("useMacCtrl", useMacCtrl);
#endif // LL_DARWIN
			
		std::string shortcut;
		child->getAttributeString("shortcut", shortcut);
		if (shortcut.find("control") != shortcut.npos)
		{
#ifdef LL_DARWIN
			if ( useMacCtrl )
			{
				mask |= MASK_MAC_CONTROL;
			}
#endif // LL_DARWIN
			mask |= MASK_CONTROL;
		}
		if (shortcut.find("alt") != shortcut.npos)
		{
			mask |= MASK_ALT;
		}
		if (shortcut.find("shift") != shortcut.npos)
		{
			mask |= MASK_SHIFT;
		}
		S32 pipe_pos = shortcut.rfind('|');
		std::string key_str = shortcut.substr(pipe_pos+1);

		KEY key = KEY_NONE;
		LLKeyboard::keyFromString(key_str, &key);

		LLMenuItemCallGL *new_item;
		LLXMLNodePtr call_child;

		if (child->hasName(LL_MENU_ITEM_CHECK_GL_TAG))
		{
			std::string control_name;
			child->getAttributeString("control_name", control_name);

			new_item = new LLMenuItemCheckGL(item_name, item_label, 0, 0, control_name, parent, 0, key, mask);

			for (call_child = child->getFirstChild(); call_child.notNull(); call_child = call_child->getNextSibling())
			{
				if (call_child->hasName("on_check") || call_child->hasName(name+".on_check"))
				{
					std::string callback_name;
					std::string control_name;
					if (call_child->hasAttribute("function"))
					{
						call_child->getAttributeString("function", callback_name);

						control_name = callback_name;

						std::string callback_data = item_name;
						if (call_child->hasAttribute("userdata"))
						{
							call_child->getAttributeString("userdata", callback_data);
							if (!callback_data.empty())
							{
								control_name = llformat("%s(%s)", callback_name.c_str(), callback_data.c_str());
							}
						}

						LLSD userdata;
						userdata["control"] = control_name;
						userdata["data"] = callback_data;

						LLSimpleListener* callback = parent->getListenerByName(callback_name);

						if (!callback)
						{
							LL_DEBUGS() << "Ignoring \"on_check\" \"" << item_name << "\" because \"" << callback_name << "\" is not registered" << LL_ENDL;
							continue;
						}

						new_item->addListener(callback, "on_build", userdata);
					}
					else if (call_child->hasAttribute("control"))
					{
						call_child->getAttributeString("control", control_name);
					}
					else
					{
						continue;
					}
					LLControlVariable *control = parent->findControl(control_name);
					if (!control)
					{
						parent->addBoolControl(control_name, FALSE);
					}
					((LLMenuItemCheckGL*)new_item)->setCheckedControl(control_name, parent);
				}
			}
		}
		else
		{
			new_item = new LLMenuItemCallGL(item_name, item_label, 0, 0, 0, 0, key, mask);
		}

		for (call_child = child->getFirstChild(); call_child.notNull(); call_child = call_child->getNextSibling())
		{
			if (call_child->hasName("on_click") || call_child->hasName(name+".on_click"))
			{
				std::string callback_name;
				call_child->getAttributeString("function", callback_name);

				std::string callback_data = item_name;
				if (call_child->hasAttribute("userdata"))
				{
					call_child->getAttributeString("userdata", callback_data);
				}

				LLSimpleListener* callback = parent->getListenerByName(callback_name);

				if (!callback)
				{
					LL_DEBUGS() << "Ignoring \"on_click\" \"" << item_name << "\" because \"" << callback_name << "\" is not registered" << LL_ENDL;
					continue;
				}

				new_item->addListener(callback, "on_click", callback_data);
			}
			if (call_child->hasName("on_enable") || call_child->hasName(name+".on_enable"))
			{
				std::string callback_name;
				std::string control_name;
				if (call_child->hasAttribute("function"))
				{
					call_child->getAttributeString("function", callback_name);

					control_name = callback_name;

					std::string callback_data;
					if (call_child->hasAttribute("userdata"))
					{
						call_child->getAttributeString("userdata", callback_data);
						if (!callback_data.empty())
						{
							control_name = llformat("%s(%s)", callback_name.c_str(), callback_data.c_str());
						}
					}

					LLSD userdata;
					userdata["control"] = control_name;
					userdata["data"] = callback_data;

					LLSimpleListener* callback = parent->getListenerByName(callback_name);

					if (!callback)
					{
						LL_DEBUGS() << "Ignoring \"on_enable\" \"" << item_name << "\" because \"" << callback_name << "\" is not registered" << LL_ENDL;
						continue;
					}

					new_item->addListener(callback, "on_build", userdata);
				}
				else if (call_child->hasAttribute("control"))
				{
					call_child->getAttributeString("control", control_name);
				}
				else
				{
					continue;
				}
				new_item->setEnabledControl(control_name, parent);
			}
			if (call_child->hasName("on_visible") || call_child->hasName(name+".on_visible"))
			{
				std::string callback_name;
				std::string control_name;
				if (call_child->hasAttribute("function"))
				{
					call_child->getAttributeString("function", callback_name);

					control_name = callback_name;

					std::string callback_data;
					if (call_child->hasAttribute("userdata"))
					{
						call_child->getAttributeString("userdata", callback_data);
						if (!callback_data.empty())
						{
							control_name = llformat("%s(%s)", callback_name.c_str(), callback_data.c_str());
						}
					}

					LLSD userdata;
					userdata["control"] = control_name;
					userdata["data"] = callback_data;

					LLSimpleListener* callback = parent->getListenerByName(callback_name);

					if (!callback)
					{
						LL_DEBUGS() << "Ignoring \"on_visible\" \"" << item_name << "\" because \"" << callback_name << "\" is not registered" << LL_ENDL;
						continue;
					}

					new_item->addListener(callback, "on_build", userdata);
				}
				else if (call_child->hasAttribute("control"))
				{
					call_child->getAttributeString("control", control_name);
				}
				else
				{
					continue;
				}
				new_item->setVisibleControl(control_name, parent);
			}
		}
		new_item->setLabel(item_label);
		if (jump_key != KEY_NONE)
			new_item->setJumpKey(jump_key);
		
		append(new_item);
	}
}

bool LLMenuGL::addChild(LLView* view, S32 tab_group)
{
	if (LLMenuGL* menup = dynamic_cast<LLMenuGL*>(view))
	{
		LL_DEBUGS() << "Adding menu " << menup->getName() << " to " << getName() << LL_ENDL;
		appendMenu(menup);
		return true;
	}
	else if (LLMenuItemGL* itemp = dynamic_cast<LLMenuItemGL*>(view))
	{
		LL_DEBUGS() << "Adding " << itemp->getName() << " to " << getName() << LL_ENDL;
		append(itemp);
		return true;
	}
	LL_DEBUGS() << "Error adding unknown child '"<<(view ? view->getName() : std::string("NULL")) << "' to " << getName() << LL_ENDL;
	return false;
}

void LLMenuGL::removeChild( LLView* ctrl)
{
	// previously a dynamic_cast with if statement to check validity
	// unfortunately removeChild is called by ~LLView, and at that point the
	// object being deleted is no longer a LLMenuItemGL so a dynamic_cast will fail
	LLMenuItemGL* itemp = static_cast<LLMenuItemGL*>(ctrl);

	item_list_t::iterator found_it = std::find(mItems.begin(), mItems.end(), (itemp));
	if (found_it != mItems.end())
	{
		mItems.erase(found_it);
	}

	return LLUICtrl::removeChild(ctrl);
}

BOOL LLMenuGL::postBuild()
{
	return LLUICtrl::postBuild();
}

// are we the childmost active menu and hence our jump keys should be enabled?
// or are we a free-standing torn-off menu (which uses jump keys too)
BOOL LLMenuGL::jumpKeysActive()
{
	LLMenuItemGL* highlighted_item = getHighlightedItem();
	BOOL active = getVisible() && getEnabled();
	if (getTornOff())
	{
		// activation of jump keys on torn off menus controlled by keyboard focus
		active = active && ((LLFloater*)getParent())->hasFocus();

	}
	else
	{
		// Are we the terminal active menu?
		// Yes, if parent menu item deems us to be active (just being visible is sufficient for top-level menus)
		// and we don't have a highlighted menu item pointing to an active sub-menu
		active = active && (!getParentMenuItem() || getParentMenuItem()->isActive()) // I have a parent that is active...
		                && (!highlighted_item || !highlighted_item->isActive()); //... but no child that is active
	}
	return active;
}

BOOL LLMenuGL::isOpen()
{
	if (getTornOff())
	{
		LLMenuItemGL* itemp = getHighlightedItem();
		// if we have an open sub-menu, then we are considered part of 
		// the open menu chain even if we don't have focus
		if (itemp && itemp->isOpen())
		{
			return TRUE;
		}
		// otherwise we are only active if we have keyboard focus
		return ((LLFloater*)getParent())->hasFocus();
	}
	else
	{
		// normally, menus are hidden as soon as the user focuses
		// on another menu, so just use the visibility criterion
		return getVisible();
	}
}
// static
LLView* LLMenuGL::fromXML(LLXMLNodePtr node, LLView* parent, LLUICtrlFactory* factory)
{
	std::string name("menu");
	node->getAttributeString("name", name);

	LLMenuGL* menu = new LLMenuGL(name);

	// Menus can be extended using filename
	if (node->hasAttribute("filename"))
	{
		std::string filename;
		node->getAttributeString("filename", filename);
		LLXMLNodePtr root;
		LLUICtrlFactory::getLayeredXMLNode(filename, root);
		menu->initMenuXML(root, parent);
	}
	menu->initMenuXML(node, parent);

	return menu;
}

void LLMenuGL::initMenuXML(LLXMLNodePtr node, LLView* parent)
{
	std::string label;
	if (node->getAttributeString("label", label))
	{
		LLStringUtil::format(label, LLTrans::getDefaultArgs());

		// parse jump key out of label
		std::string new_menu_label;

		typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
		boost::char_separator<char> sep("_");

		KEY jump_key = KEY_NONE;
		S32 token_count = 0;
		for (auto token : tokenizer(label, sep))
		{
			new_menu_label += token;
			if (token_count > 0)
			{
				jump_key = token.front();
			}
			++token_count;
		}

		setLabel(new_menu_label);
		setJumpKey(jump_key);
	}

	BOOL opaque = TRUE;
	node->getAttributeBOOL("opaque", opaque);

	bool b(false);
	node->getAttribute_bool("scrollable", b);
	setScrollable(b);


	BOOL tear_off = FALSE;
	node->getAttributeBOOL("tear_off", tear_off);
	setCanTearOff(tear_off);

	if (node->hasAttribute("drop_shadow"))
	{
		BOOL drop_shadow = FALSE;
		node->getAttributeBOOL("drop_shadow", drop_shadow);
		setDropShadowed(drop_shadow);
	}

	setBackgroundVisible(opaque);
	LLColor4 color(0,0,0,1);
	if (opaque && LLUICtrlFactory::getAttributeColor(node,"color", color))
	{
		setBackgroundColor(color);
	}

	BOOL create_jump_keys = FALSE;
	node->getAttributeBOOL("create_jump_keys", create_jump_keys);

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		parseChildXML(child, parent);
	}

	if (create_jump_keys)
	{
		createJumpKeys();
	}
}



bool LLMenuGL::scrollItems(EScrollingDirection direction)
{
	// Slowing down items scrolling when arrow button is held
	if (mScrollItemsTimer.hasExpired() && NULL != mFirstVisibleItem)
	{
		mScrollItemsTimer.setTimerExpirySec(.033f);
	}
	else
	{
		return false;
	}

	switch (direction)
	{
	case SD_UP:
	{
		item_list_t::iterator cur_item_iter;
		item_list_t::iterator prev_item_iter;
		for (cur_item_iter = mItems.begin(), prev_item_iter = mItems.begin(); cur_item_iter != mItems.end(); cur_item_iter++)
		{
			if( (*cur_item_iter) == mFirstVisibleItem)
			{
				break;
			}
			if ((*cur_item_iter)->getVisible())
			{
				prev_item_iter = cur_item_iter;
			}
		}

		if ((*prev_item_iter)->getVisible())
		{
			mFirstVisibleItem = *prev_item_iter;
		}
		break;
	}
	case SD_DOWN:
	{
		if (NULL == mFirstVisibleItem)
		{
			mFirstVisibleItem = *mItems.begin();
		}

		item_list_t::iterator cur_item_iter;

		for (cur_item_iter = mItems.begin(); cur_item_iter != mItems.end(); cur_item_iter++)
		{
			if( (*cur_item_iter) == mFirstVisibleItem)
			{
				break;
			}
		}

		item_list_t::iterator next_item_iter;

		if (cur_item_iter != mItems.end())
		{
			for (next_item_iter = ++cur_item_iter; next_item_iter != mItems.end(); next_item_iter++)
			{
				if( (*next_item_iter)->getVisible())
				{
					break;
				}
			}

			if (next_item_iter != mItems.end() &&
				(*next_item_iter)->getVisible())
			{
				mFirstVisibleItem = *next_item_iter;
			}
		}
		break;
	}
	case SD_BEGIN:
	{
		mFirstVisibleItem = *mItems.begin();
		break;
	}
	case SD_END:
	{
		item_list_t::reverse_iterator first_visible_item_iter = mItems.rend();

		// Need to scroll through number of actual existing items in menu.
		// Otherwise viewer will hang for a time needed to scroll U32_MAX
		// times in std::advance(). STORM-659.
		size_t nitems = mItems.size();
		U32 scrollable_items = nitems < mMaxScrollableItems ? nitems : mMaxScrollableItems;

		// Advance by mMaxScrollableItems back from the end of the list
		// to make the last item visible.
		std::advance(first_visible_item_iter, scrollable_items);
		mFirstVisibleItem = *first_visible_item_iter;
		break;
	}
	default:
		//LL_WARNS() << "Unknown scrolling direction: " << direction << LL_ENDL;
		LL_WARNS() << "Unknown scrolling direction: " << direction << LL_ENDL;
	}

	mNeedsArrange = TRUE;
	arrangeAndClear();

	return true;
}

void LLMenuGL::setScrollable(bool b)
{
	mScrollable = b;
}

// rearrange the child rects so they fit the shape of the menu.
void LLMenuGL::arrange( void )
{
	// calculate the height & width, and set our rect based on that
	// information.
	const LLRect& initial_rect = getRect();

	U32 width = 0, height = MENU_ITEM_PADDING;

	cleanupSpilloverBranch();

	if( mItems.size() ) 
	{
		const LLRect menu_region_rect = LLMenuGL::sMenuContainer ? LLMenuGL::sMenuContainer->getMenuRect() : LLRect(0, S32_MAX, S32_MAX, 0);

		// torn off menus are not constrained to the size of the screen
		U32 max_width = getTornOff() ? U32_MAX : menu_region_rect.getWidth();
		U32 max_height = U32_MAX;
		if (!getTornOff())
		{
			max_height = getRect().mTop - menu_region_rect.mBottom;
			if (menu_region_rect.mTop - getRect().mTop > (S32)max_height)
			{
				max_height = menu_region_rect.mTop - getRect().mTop;
			}
		}

		// *FIX: create the item first and then ask for its dimensions?
		S32 spillover_item_width = PLAIN_PAD_PIXELS + LLFontGL::getFontSansSerif()->getWidth( std::string("More") ); // *TODO: Translate
		S32 spillover_item_height = ll_round(LLFontGL::getFontSansSerif()->getLineHeight()) + MENU_ITEM_PADDING;

		// Scrolling support
		item_list_t::iterator first_visible_item_iter;
		item_list_t::iterator first_hidden_item_iter = mItems.end();
		S32 height_before_first_visible_item = -1;
		S32 visible_items_height = 0;
		U32 scrollable_items_cnt = 0;
		
		if (mHorizontalLayout)
		{
			item_list_t::iterator item_iter;
			for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
			{
				// do first so LLMenuGLItemCall can call on_visible to determine if visible
				(*item_iter)->buildDrawLabel();

				if ((*item_iter)->getVisible())
				{
					if (!getTornOff() 
						&& *item_iter != mSpilloverBranch
						&& width + (*item_iter)->getNominalWidth() > max_width - spillover_item_width)
					{
						// no room for any more items
						createSpilloverBranch();

						std::vector<LLMenuItemGL*> items_to_remove;
						std::copy(item_iter, mItems.end(), std::back_inserter(items_to_remove));
						std::vector<LLMenuItemGL*>::iterator spillover_iter;
						for (spillover_iter= items_to_remove.begin(); spillover_iter != items_to_remove.end(); ++spillover_iter)
						{
							LLMenuItemGL* itemp = (*spillover_iter);
							removeChild(itemp);
							mSpilloverMenu->addChild(itemp);
						}

						addChild(mSpilloverBranch);
						
						height = llmax(height, mSpilloverBranch->getNominalHeight());
						width += mSpilloverBranch->getNominalWidth();

						break;
					}
					else
					{
						// track our rect
						height = llmax(height, (*item_iter)->getNominalHeight());
						width += (*item_iter)->getNominalWidth();
					}
				}
			}
		}
		else
		{
			item_list_t::iterator item_iter;

			for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
			{
				// do first so LLMenuGLItemCall can call on_visible to determine if visible
				(*item_iter)->buildDrawLabel();

				if ((*item_iter)->getVisible())
				{
					if (!getTornOff() 
						&& !mScrollable
						&& *item_iter != mSpilloverBranch
						&& height + (*item_iter)->getNominalHeight() > max_height - spillover_item_height)
					{
						// no room for any more items
						createSpilloverBranch();

						std::vector<LLMenuItemGL*> items_to_remove;
						std::copy(item_iter, mItems.end(), std::back_inserter(items_to_remove));
						std::vector<LLMenuItemGL*>::iterator spillover_iter;
						for (spillover_iter= items_to_remove.begin(); spillover_iter != items_to_remove.end(); ++spillover_iter)
						{
							LLMenuItemGL* itemp = (*spillover_iter);
							removeChild(itemp);
							mSpilloverMenu->addChild(itemp);
						}


						addChild(mSpilloverBranch);

						height += mSpilloverBranch->getNominalHeight();
						width = llmax( width, mSpilloverBranch->getNominalWidth() );

						break;
					}
					else
					{
						// track our rect
						height += (*item_iter)->getNominalHeight();
						width = llmax( width, (*item_iter)->getNominalWidth() );
					}

					if (mScrollable)
					{
						// Determining visible items boundaries
						if (NULL == mFirstVisibleItem)
						{
							mFirstVisibleItem = *item_iter;
						}

						if (*item_iter == mFirstVisibleItem)
						{
							height_before_first_visible_item = height - (*item_iter)->getNominalHeight();
							first_visible_item_iter = item_iter;
							scrollable_items_cnt = 0;
						}

						if (-1 != height_before_first_visible_item && 0 == visible_items_height &&
						    (++scrollable_items_cnt > mMaxScrollableItems ||
						     height - height_before_first_visible_item > max_height - spillover_item_height * 2 ))
						{
							first_hidden_item_iter = item_iter;
							visible_items_height = height - height_before_first_visible_item - (*item_iter)->getNominalHeight();
							scrollable_items_cnt--;
						}
					}
				}
			}

			if (mScrollable)
			{
				S32 max_items_height = max_height - spillover_item_height * 2;

				if (visible_items_height == 0)
					visible_items_height = height - height_before_first_visible_item;

				// Fix mFirstVisibleItem value, if it doesn't allow to display all items, that can fit
				if (visible_items_height < max_items_height && scrollable_items_cnt < mMaxScrollableItems)
				{
					item_list_t::iterator tmp_iter(first_visible_item_iter);
					while (visible_items_height < max_items_height &&
					       scrollable_items_cnt < mMaxScrollableItems &&
					       first_visible_item_iter != mItems.begin())
					{
						if ((*first_visible_item_iter)->getVisible())
						{
							// It keeps visible item, after first_visible_item_iter
							tmp_iter = first_visible_item_iter;
						}

						first_visible_item_iter--;

						if ((*first_visible_item_iter)->getVisible())
						{
							visible_items_height += (*first_visible_item_iter)->getNominalHeight();
							height_before_first_visible_item -= (*first_visible_item_iter)->getNominalHeight();
							scrollable_items_cnt++;
						}
					}

					// Roll back one item, that doesn't fit
					if (visible_items_height > max_items_height)
					{
						visible_items_height -= (*first_visible_item_iter)->getNominalHeight();
						height_before_first_visible_item += (*first_visible_item_iter)->getNominalHeight();
						scrollable_items_cnt--;
						first_visible_item_iter = tmp_iter;
					}
					if (!(*first_visible_item_iter)->getVisible())
					{
						first_visible_item_iter = tmp_iter;
					}

					mFirstVisibleItem = *first_visible_item_iter;
				}
			}
		}

		S32 cur_height = (S32)llmin(max_height, height);

		if (mScrollable &&
		    (height_before_first_visible_item > MENU_ITEM_PADDING ||
			    height_before_first_visible_item + visible_items_height < (S32)height))
		{
			// Reserving 2 extra slots for arrow items
			cur_height = visible_items_height + spillover_item_height * 2;
		}

		setRect(LLRect(getRect().mLeft, getRect().mTop, getRect().mLeft + width, getRect().mTop - cur_height));

		S32 cur_width = 0;
		S32 offset = 0;
		if (mScrollable)
		{
			// No space for all items, creating arrow items
			if (height_before_first_visible_item > MENU_ITEM_PADDING ||
			    height_before_first_visible_item + visible_items_height < (S32)height)
			{
				if (NULL == mArrowUpItem)
				{
					LLMenuScrollItem::Params item_params;
					item_params.name(ARROW_UP);
					item_params.arrow_type(LLMenuScrollItem::ARROW_UP);
					item_params.scroll_callback.function(boost::bind(&LLMenuGL::scrollItems, this, SD_UP));

					mArrowUpItem = LLUICtrlFactory::create<LLMenuScrollItem>(item_params);
					LLUICtrl::addChild(mArrowUpItem);

				}
				if (NULL == mArrowDownItem)
				{
					LLMenuScrollItem::Params item_params;
					item_params.name(ARROW_DOWN);
					item_params.arrow_type(LLMenuScrollItem::ARROW_DOWN);
					item_params.scroll_callback.function(boost::bind(&LLMenuGL::scrollItems, this, SD_DOWN));

					mArrowDownItem = LLUICtrlFactory::create<LLMenuScrollItem>(item_params);
					LLUICtrl::addChild(mArrowDownItem);
				}

				LLRect rect;
				mArrowUpItem->setRect(rect.setLeftTopAndSize( 0, cur_height, width, mArrowUpItem->getNominalHeight()));
				mArrowUpItem->setVisible(TRUE);
				mArrowUpItem->setEnabled(height_before_first_visible_item > MENU_ITEM_PADDING);
				mArrowUpItem->reshape(width, mArrowUpItem->getNominalHeight());
				mArrowDownItem->setRect(rect.setLeftTopAndSize( 0, mArrowDownItem->getNominalHeight(), width, mArrowDownItem->getNominalHeight()));
				mArrowDownItem->setVisible(TRUE);
				mArrowDownItem->setEnabled(height_before_first_visible_item + visible_items_height < (S32)height);
				mArrowDownItem->reshape(width, mArrowDownItem->getNominalHeight());

				cur_height -= mArrowUpItem->getNominalHeight();

				offset = menu_region_rect.mRight; // This moves items behind visible area
			}
			else
			{
				if (NULL != mArrowUpItem)
				{
					mArrowUpItem->setVisible(FALSE);
				}
				if (NULL != mArrowDownItem)
				{
					mArrowDownItem->setVisible(FALSE);
				}
			}

		}

		item_list_t::iterator item_iter;
		for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
		{
			if ((*item_iter)->getVisible())
			{
				if (mScrollable)
				{
					if (item_iter == first_visible_item_iter)
					{
						offset = 0;
					}
					else if (item_iter == first_hidden_item_iter)
					{
						offset = menu_region_rect.mRight;  // This moves items behind visible area
					}
				}

				// setup item rect to hold label
				LLRect rect;
				if (mHorizontalLayout)
				{
					rect.setLeftTopAndSize( cur_width, height, (*item_iter)->getNominalWidth(), height);
					cur_width += (*item_iter)->getNominalWidth();
				}
				else
				{
					rect.setLeftTopAndSize( 0 + offset, cur_height, width, (*item_iter)->getNominalHeight());
					if (offset == 0)
					{
						cur_height -= (*item_iter)->getNominalHeight();
					}
				}
				(*item_iter)->setRect( rect );
			}
		}
	}
	if (mKeepFixedSize)
	{
		reshape(initial_rect.getWidth(), initial_rect.getHeight());
	}
}

void LLMenuGL::arrangeAndClear( void )
{
	if (mNeedsArrange)
	{
		arrange();
		mNeedsArrange = FALSE;
	}
}

void LLMenuGL::createSpilloverBranch()
{
	if (!mSpilloverBranch)
	{
		// should be NULL but delete anyway
		delete mSpilloverMenu;
		// technically, you can't tear off spillover menus, but we're passing the handle
		// along just to be safe
		mSpilloverMenu = new LLMenuGL(std::string("More"), std::string("More"));
		mSpilloverMenu->updateParent(LLMenuGL::sMenuContainer);
		// Inherit colors
		mSpilloverMenu->setBackgroundColor( mBackgroundColor );
		mSpilloverMenu->setCanTearOff(FALSE);

		mSpilloverBranch = new LLMenuItemBranchGL(std::string("More"), std::string("More"), mSpilloverMenu->getHandle());
		mSpilloverBranch->setFontStyle(LLFontGL::ITALIC);
	}
}

void LLMenuGL::cleanupSpilloverBranch()
{
	if (mSpilloverBranch && mSpilloverBranch->getParent() == this)
	{
		// head-recursion to propagate items back up to root menu
		mSpilloverMenu->cleanupSpilloverBranch();

		// pop off spillover items
		while (mSpilloverMenu->getItemCount())
		{
			LLMenuItemGL* itemp = mSpilloverMenu->getItem(0);
			mSpilloverMenu->removeChild(itemp);
			// put them at the end of our own list
			addChild(itemp);
		}

		// Delete the branch, and since the branch will delete the menu,
		// set the menu* to null.
		delete mSpilloverBranch;
		mSpilloverBranch = NULL;
		mSpilloverMenu = NULL;
	}
}

void LLMenuGL::createJumpKeys()
{
	if (!mCreateJumpKeys) return;
	mCreateJumpKeys = FALSE;

	mJumpKeys.clear();

	std::set<std::string> unique_words;
	std::set<std::string> shared_words;

	item_list_t::iterator item_it;
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");

	for(item_it = mItems.begin(); item_it != mItems.end(); ++item_it)
	{
		std::string uppercase_label = (*item_it)->getLabel();
		LLStringUtil::toUpper(uppercase_label);

		tokenizer tokens(uppercase_label, sep);
		tokenizer::iterator token_iter;
		for( token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
		{
			if (unique_words.find(*token_iter) != unique_words.end())
			{
				// this word exists in more than one menu instance
				shared_words.insert(*token_iter);
			}
			else
			{
				// we have a new word, keep track of it
				unique_words.insert(*token_iter);
			}
		}
	}

	// pre-assign specified jump keys
	for(item_it = mItems.begin(); item_it != mItems.end(); ++item_it)
	{
		KEY jump_key = (*item_it)->getJumpKey();
		if(jump_key != KEY_NONE)
		{
			if (mJumpKeys.find(jump_key) == mJumpKeys.end())
			{
				mJumpKeys.insert(std::pair<KEY, LLMenuItemGL*>(jump_key, (*item_it)));
			}
			else
			{
				// this key is already spoken for, 
				// so we need to reassign it below
				(*item_it)->setJumpKey(KEY_NONE);
			}
		}
	}

	for(item_it = mItems.begin(); item_it != mItems.end(); ++item_it)
	{
		// skip over items that already have assigned jump keys
		if ((*item_it)->getJumpKey() != KEY_NONE)
		{
			continue;
		}
		std::string uppercase_label = (*item_it)->getLabel();
		LLStringUtil::toUpper(uppercase_label);

		tokenizer tokens(uppercase_label, sep);
		tokenizer::iterator token_iter;

		BOOL found_key = FALSE;
		for( token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
		{
			std::string uppercase_word = *token_iter;

			// this word is not shared with other menu entries...
			if (shared_words.find(*token_iter) == shared_words.end())
			{
				S32 i;
				for(i = 0; i < (S32)uppercase_word.size(); i++)
				{
					char jump_key = uppercase_word[i];
					
					if (LLStringOps::isDigit(jump_key) || (LLStringOps::isUpper(jump_key) &&
						mJumpKeys.find(jump_key) == mJumpKeys.end()))
					{
						mJumpKeys.insert(std::pair<KEY, LLMenuItemGL*>(jump_key, (*item_it)));
						(*item_it)->setJumpKey(jump_key);
						found_key = TRUE;
						break;
					}
				}
			}
			if (found_key)
			{
				break;
			}
		}
	}
}

// remove all items on the menu
void LLMenuGL::empty( void )
{
	cleanupSpilloverBranch();

	mItems.clear();
	mFirstVisibleItem = NULL;
	mArrowUpItem = NULL;
	mArrowDownItem = NULL;

	deleteAllChildren();
}

// erase group of items from menu
void LLMenuGL::erase( S32 begin, S32 end, bool arrange/* = true*/)
{
	S32 items = mItems.size();

	if ( items == 0 || begin >= end || begin < 0 || end > items )
	{
		return;
	}

	item_list_t::iterator start_position = mItems.begin();
	std::advance(start_position, begin);

	item_list_t::iterator end_position = mItems.begin();
	std::advance(end_position, end);

	erase(start_position, end_position);

	if (arrange)
	{
		needsArrange();
	}
}

// add new item at position
void LLMenuGL::insert(S32 position, LLView* ctrl, bool arrange /*= true*/)
{
	LLMenuItemGL* item = dynamic_cast<LLMenuItemGL *>(ctrl);

	if (NULL == item || position < 0 || (U32)position >= mItems.size())
	{
		return;
	}

	item_list_t::iterator position_iter = mItems.begin();
	std::advance(position_iter, position);
	insert(position_iter, item, arrange);
}
void LLMenuGL::insert(item_list_t::const_iterator position_iter, LLMenuItemGL* item, bool arrange /*= true*/)
{
	mItems.insert(position_iter, item);
	LLUICtrl::addChild(item);

	if (arrange)
	{
		needsArrange();
	}
}

// Adjust rectangle of the menu
void LLMenuGL::setLeftAndBottom(S32 left, S32 bottom)
{
	setRect(LLRect(left, getRect().mTop, getRect().mRight, bottom));
	needsArrange();
}

BOOL LLMenuGL::handleJumpKey(KEY key)
{
	// must perform case-insensitive comparison, so just switch to uppercase input key
	key = toupper(key);
	navigation_key_map_t::iterator found_it = mJumpKeys.find(key);
	if(found_it != mJumpKeys.end() && found_it->second->getEnabled())
	{
		// switch to keyboard navigation mode
		LLMenuGL::setKeyboardMode(TRUE);

		// force highlight to close old menus and open and sub-menus
		found_it->second->setHighlight(TRUE);
		found_it->second->onCommit();

	}
	// if we are navigating the menus, we need to eat the keystroke
	// so rest of UI doesn't handle it
	return TRUE;
}


// Add the menu item to this menu.
BOOL LLMenuGL::append( LLMenuItemGL* item )
{
	if (!item) return FALSE;
	mItems.push_back( item );
	LLUICtrl::addChild(item);
	needsArrange();
	return TRUE;
}

// add a separator to this menu
BOOL LLMenuGL::addSeparator(const std::string& name)
{
	LLMenuItemGL* separator = new LLMenuItemSeparatorGL(name);
	return addChild(separator);
}

// add a menu - this will create a cascading menu
BOOL LLMenuGL::appendMenu( LLMenuGL* menu )
{
	if( menu == this )
	{
		LL_ERRS() << "** Attempt to attach menu to itself. This is certainly "
			   << "a logic error." << LL_ENDL;
	}
	BOOL success = TRUE;

	LLMenuItemBranchGL* branch = NULL;
	branch = new LLMenuItemBranchGL( menu->getName(), menu->getLabel(), menu->getHandle() );
	branch->setJumpKey(menu->getJumpKey());
	success &= append( branch );

	// Inherit colors
	menu->setBackgroundColor( mBackgroundColor );
	menu->updateParent(LLMenuGL::sMenuContainer);
	return success;
}

void LLMenuGL::setEnabledSubMenus(BOOL enable)
{
	setEnabled(enable);
	item_list_t::iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		(*item_iter)->setEnabledSubMenus( enable );
	}
}

// setItemEnabled() - pass the label and the enable flag for a menu
// item. TRUE will make sure it's enabled, FALSE will disable it.
void LLMenuGL::setItemEnabled( const std::string& name, BOOL enable )
{
	item_list_t::iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		if( (*item_iter)->getName() == name )
		{
			(*item_iter)->setEnabled( enable );
			(*item_iter)->setEnabledSubMenus( enable );
			break;
		}
	}
}

void LLMenuGL::setItemVisible( const std::string& name, BOOL visible )
{
	item_list_t::iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		if( (*item_iter)->getName() == name )
		{
			(*item_iter)->setVisible( visible );
			needsArrange();
			break;
		}
	}
}

void LLMenuGL::setItemLastSelected(LLMenuItemGL* item)
{
	if (getVisible())
	{
		LLMenuHolderGL::setActivatedItem(item);
	}

	// update enabled and checkmark status
	item->buildDrawLabel();
}

//  Set whether drop shadowed 
void LLMenuGL::setDropShadowed( const BOOL shadowed )
{
	mDropShadowed = shadowed;
}

void LLMenuGL::setTornOff(BOOL torn_off)
{ 
	mTornOff = torn_off;
}

LLMenuItemGL* LLMenuGL::getItem(S32 number)
{
	if (number >= 0 && number < (S32)mItems.size())
	{
		item_list_t::iterator item_iter;
		for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
		{
			if (number == 0)
			{
				return (*item_iter);
			}
			number--;
		}
	}
	return NULL;
}

LLMenuItemGL* LLMenuGL::getHighlightedItem()
{
	item_list_t::iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		if ((*item_iter)->getHighlight())
		{
			return (*item_iter);
		}
	}
	return NULL;
}

LLMenuItemGL* LLMenuGL::highlightNextItem(LLMenuItemGL* cur_item, BOOL skip_disabled)
{
	if (mItems.empty()) return NULL;
	// highlighting first item on a torn off menu is the
	// same as giving focus to it
	if (!cur_item && getTornOff())
	{
		LLFloater * parent = dynamic_cast<LLFloater *>(getParent());
		if (parent)
		{
			parent->setFocus(TRUE);
		}
	}

	// Current item position in the items list
	item_list_t::iterator cur_item_iter = std::find(mItems.begin(), mItems.end(), cur_item);

	item_list_t::iterator next_item_iter;
	if (cur_item_iter == mItems.end())
	{
		next_item_iter = mItems.begin();
	}
	else
	{
		next_item_iter = cur_item_iter;
		next_item_iter++;

		// First visible item position in the items list
		item_list_t::iterator first_visible_item_iter = std::find(mItems.begin(), mItems.end(), mFirstVisibleItem);

		if (next_item_iter == mItems.end())
		{
			next_item_iter = mItems.begin();

			// If current item is the last in the list, the menu is scrolled to the beginning
			// and the first item is highlighted.
			if (mScrollable && !scrollItems(SD_BEGIN))
			{
				return NULL;
			}
		}
		// If current item is the last visible, the menu is scrolled one item down
		// and the next item is highlighted.
		else if (mScrollable &&
				 (U32)std::abs(std::distance(first_visible_item_iter, next_item_iter)) >= mMaxScrollableItems)
		{
			// Call highlightNextItem() recursively only if the menu was successfully scrolled down.
			// If scroll timer hasn't expired yet the menu won't be scrolled and calling
			// highlightNextItem() will result in an endless recursion.
			if (scrollItems(SD_DOWN))
			{
				return highlightNextItem(cur_item, skip_disabled);
			}
			else
			{
				return NULL;
			}
		}
	}

	// when first highlighting a menu, skip over tear off menu item
	if (mTearOffItem && !cur_item)
	{
		// we know the first item is the tear off menu item
		cur_item_iter = mItems.begin();
		next_item_iter++;
		if (next_item_iter == mItems.end())
		{
			next_item_iter = mItems.begin();
		}
	}

	while(1)
	{
		// skip separators and disabled/invisible items
		if ((*next_item_iter)->getEnabled() && (*next_item_iter)->getVisible() && !dynamic_cast<LLMenuItemSeparatorGL*>(*next_item_iter))
		{
			if (cur_item)
			{
				cur_item->setHighlight(FALSE);
			}
			(*next_item_iter)->setHighlight(TRUE);
			return (*next_item_iter);
		}


		if (!skip_disabled || next_item_iter == cur_item_iter)
		{
			break;
		}

		next_item_iter++;
		if (next_item_iter == mItems.end())
		{
			if (cur_item_iter == mItems.end())
			{
				break;
			}
			next_item_iter = mItems.begin();
		}
	}

	return NULL;
}

LLMenuItemGL* LLMenuGL::highlightPrevItem(LLMenuItemGL* cur_item, BOOL skip_disabled)
{
	if (mItems.empty()) return NULL;

	// highlighting first item on a torn off menu is the
	// same as giving focus to it
	if (!cur_item && getTornOff())
	{
		LLFloater * parent = dynamic_cast<LLFloater *>(getParent());
		if (parent)
		{
			parent->setFocus(TRUE);
		}
	}

	// Current item reverse position from the end of the list
	item_list_t::reverse_iterator cur_item_iter = std::find(mItems.rbegin(), mItems.rend(), cur_item);

	item_list_t::reverse_iterator prev_item_iter;
	if (cur_item_iter == mItems.rend())
	{
		prev_item_iter = mItems.rbegin();
	}
	else
	{
		prev_item_iter = cur_item_iter;
		prev_item_iter++;

		// First visible item reverse position in the items list
		item_list_t::reverse_iterator first_visible_item_iter = std::find(mItems.rbegin(), mItems.rend(), mFirstVisibleItem);

		if (prev_item_iter == mItems.rend())
		{
			prev_item_iter = mItems.rbegin();

			// If current item is the first in the list, the menu is scrolled to the end
			// and the last item is highlighted.
			if (mScrollable && !scrollItems(SD_END))
			{
				return NULL;
			}
		}
		// If current item is the first visible, the menu is scrolled one item up
		// and the previous item is highlighted.
		else if (mScrollable &&
				 std::distance(first_visible_item_iter, cur_item_iter) <= 0)
		{
			// Call highlightNextItem() only if the menu was successfully scrolled up.
			// If scroll timer hasn't expired yet the menu won't be scrolled and calling
			// highlightNextItem() will result in an endless recursion.
			if (scrollItems(SD_UP))
			{
				return highlightPrevItem(cur_item, skip_disabled);
			}
			else
			{
				return NULL;
			}
		}
	}

	while(1)
	{
		// skip separators and disabled/invisible items
		if ((*prev_item_iter)->getEnabled() && (*prev_item_iter)->getVisible() && (*prev_item_iter)->getName() != SEPARATOR_NAME)
		{
			(*prev_item_iter)->setHighlight(TRUE);
			return (*prev_item_iter);
		}

		if (!skip_disabled || prev_item_iter == cur_item_iter)
		{
			break;
		}

		prev_item_iter++;
		if (prev_item_iter == mItems.rend())
		{
			if (cur_item_iter == mItems.rend())
			{
				break;
			}

			prev_item_iter = mItems.rbegin();
		}
	}

	return NULL;
}

void LLMenuGL::buildDrawLabels()
{
	item_list_t::iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		(*item_iter)->buildDrawLabel();
	}
}

void LLMenuGL::updateParent(LLView* parentp)
{
	if (getParent())
	{
		getParent()->removeChild(this);
	}
	if (parentp)
	{
		parentp->addChild(this);
	}
	item_list_t::iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		(*item_iter)->updateBranchParent(parentp);
	}
}

BOOL LLMenuGL::handleAcceleratorKey(KEY key, MASK mask)
{
	// don't handle if not enabled
	if(!getEnabled())
	{
		return FALSE;
	}

	// Pass down even if not visible
	item_list_t::iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		LLMenuItemGL* itemp = *item_iter;
		if (itemp->handleAcceleratorKey(key, mask))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL LLMenuGL::handleUnicodeCharHere( llwchar uni_char )
{
	if (jumpKeysActive())
	{
		return handleJumpKey((KEY)uni_char);
	}
	return FALSE;
}

BOOL LLMenuGL::handleHover( S32 x, S32 y, MASK mask )
{
	// leave submenu in place if slope of mouse < MAX_MOUSE_SLOPE_SUB_MENU
	BOOL no_mouse_data = mLastMouseX == 0 && mLastMouseY == 0;
	S32 mouse_delta_x = no_mouse_data ? 0 : x - mLastMouseX;
	S32 mouse_delta_y = no_mouse_data ? 0 : y - mLastMouseY;
	LLVector2 mouse_dir((F32)mouse_delta_x, (F32)mouse_delta_y);
	mouse_dir.normVec();
	LLVector2 mouse_avg_dir((F32)mMouseVelX, (F32)mMouseVelY);
	mouse_avg_dir.normVec();
	F32 interp = 0.5f * (llclamp(mouse_dir * mouse_avg_dir, 0.f, 1.f));
	mMouseVelX = ll_round(lerp((F32)mouse_delta_x, (F32)mMouseVelX, interp));
	mMouseVelY = ll_round(lerp((F32)mouse_delta_y, (F32)mMouseVelY, interp));
	mLastMouseX = x;
	mLastMouseY = y;

	// don't change menu focus unless mouse is moving or alt key is not held down
	if ((llabs(mMouseVelX) > 0 || 
			llabs(mMouseVelY) > 0) /*&&
		(!mHasSelection ||
		// (mouse_delta_x == 0 && mouse_delta_y == 0) ||
		(mMouseVelX < 0) ||
		llabs((F32)mMouseVelY) / llabs((F32)mMouseVelX) > MAX_MOUSE_SLOPE_SUB_MENU)*/)
	{
		for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (!viewp->pointInView(local_x, local_y) && ((LLMenuItemGL*)viewp)->getHighlight())
			{
				// moving mouse always highlights new item
				if (mouse_delta_x != 0 || mouse_delta_y != 0)
				{
					((LLMenuItemGL*)viewp)->setHighlight(FALSE);
				}
			}
		}

		for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			//RN: always call handleHover to track mGotHover status
			// but only set highlight when mouse is moving
			if( viewp->getVisible() && 
				//RN: allow disabled items to be highlighted to preserve "active" menus when
				// moving mouse through them
				//viewp->getEnabled() && 
				viewp->pointInView(local_x, local_y) && 
				viewp->handleHover(local_x, local_y, mask))
			{
				// moving mouse always highlights new item
				if (mouse_delta_x != 0 || mouse_delta_y != 0)
				{
					((LLMenuItemGL*)viewp)->setHighlight(TRUE);
					LLMenuGL::setKeyboardMode(FALSE);
				}
				mHasSelection = true;
			}
		}
	}
	getWindow()->setCursor(UI_CURSOR_ARROW);

	// *HACK Release the mouse capture
	// This is done to release the mouse after the Navigation Bar "Back" or "Forward" button
	// drop-down menu is shown. Otherwise any other view won't be able to handle mouse events
	// until the user chooses one of the drop-down menu items.

	return TRUE;
}

BOOL LLMenuGL::handleScrollWheel( S32 x, S32 y, S32 clicks )
{
	if (!mScrollable)
		return blockMouseEvent(x, y);

	if( clicks > 0 )
	{
		while( clicks-- )
			scrollItems(SD_DOWN);
	}
	else
	{
		while( clicks++ )
			scrollItems(SD_UP);
	}

	return TRUE;
}


void LLMenuGL::draw( void )
{
	if (mNeedsArrange)
	{
		arrange();
		mNeedsArrange = FALSE;
	}
	if (mDropShadowed && !mTornOff)
	{
		static LLUICachedControl<S32> drop_shadow_floater("DropShadowFloater", 0);
		static const LLColor4 color_drop_shadow(LLUI::sColorsGroup->getColor4("ColorDropShadow"));
		gl_drop_shadow(0, getRect().getHeight(), getRect().getWidth(), 0, 
			color_drop_shadow, drop_shadow_floater );
	}

	if( mBgVisible )
	{
		gl_rect_2d( 0, getRect().getHeight(), getRect().getWidth(), 0, mBackgroundColor );
	}
	LLView::draw();
}

void LLMenuGL::drawBackground(LLMenuItemGL* itemp, LLColor4& color)
{
	gGL.color4fv( color.mV );
	LLRect item_rect = itemp->getRect();
	gl_rect_2d( 0, item_rect.getHeight(), item_rect.getWidth(), 0);
}

void LLMenuGL::setVisible(BOOL visible)
{
	if (visible != getVisible())
	{
		if (!visible)
		{
			LL_DEBUGS() << "Hiding " << getName() << LL_ENDL;
			mFadeTimer.start();
			clearHoverItem();
			// reset last known mouse coordinates so
			// we don't spoof a mouse move next time we're opened
			mLastMouseX = 0;
			mLastMouseY = 0;
		}
		else
		{ 
			LL_DEBUGS() << "Showing " << getName() << LL_ENDL;
			mHasSelection = true;
			mFadeTimer.stop();
		}

		LLView::setVisible(visible);
	}
}

LLMenuGL* LLMenuGL::getChildMenuByName(const std::string& name, BOOL recurse) const
{
	LLView* view = getChildView(name, recurse, FALSE);
	if (view)
	{
		LLMenuItemBranchGL* branch = dynamic_cast<LLMenuItemBranchGL*>(view);
		if (branch)
		{
			return branch->getBranch();
		}

		LLMenuGL* menup = dynamic_cast<LLMenuGL*>(view);
		if (menup)
		{
			return menup;
		}
	}
	LL_WARNS() << "Child Menu " << name << " not found in menu " << getName() << LL_ENDL;
	return NULL;
}

BOOL LLMenuGL::clearHoverItem()
{
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLMenuItemGL* itemp = (LLMenuItemGL*)*child_it;
		if (itemp->getHighlight())
		{
			itemp->setHighlight(FALSE);
			return TRUE;
		}
	}		
	return FALSE;
}

void hide_top_view( LLView* view )
{
	if( view ) view->setVisible( FALSE );
}


// x and y are the desired location for the popup, in the spawning_view's
// coordinate frame, NOT necessarily the mouse location
// static
void LLMenuGL::showPopup(LLView* spawning_view, LLMenuGL* menu, S32 x, S32 y)
{
	const S32 CURSOR_HEIGHT = 22;		// Approximate "normal" cursor size
	const S32 CURSOR_WIDTH = 12;

	if(menu->getChildList()->empty())
	{
		return;
	}

	menu->setVisible( TRUE );

	//Do not show menu if all menu items are disabled
	BOOL item_enabled = false;
	for (LLView::child_list_t::const_iterator itor = menu->getChildList()->begin();
			 itor != menu->getChildList()->end();
			 ++itor)
	{
		LLView *menu_item = (*itor);
		item_enabled = item_enabled || menu_item->getEnabled();
	}

	if(!item_enabled)
	{
		menu->setVisible( FALSE );
		return;
	}

	// Save click point for detecting cursor moves before mouse-up.
	// Must be in local coords to compare with mouseUp events.
	// If the mouse doesn't move, the menu will stay open ala the Mac.
	// See also LLContextMenu::show()
	S32 mouse_x, mouse_y;

	// Resetting scrolling position
	if (menu->isScrollable() && menu->isScrollPositionOnShowReset())
	{
		menu->mFirstVisibleItem = NULL;
	}

	// Fix menu rect if needed.
	menu->needsArrange();
	menu->arrangeAndClear();

	LLUI::getMousePositionLocal(menu->getParent(), &mouse_x, &mouse_y);
	LLMenuHolderGL::sContextMenuSpawnPos.set(mouse_x,mouse_y);

	const LLRect menu_region_rect = LLMenuGL::sMenuContainer->getRect();

	const S32 HPAD = 2;
	LLRect rect = menu->getRect();
	S32 left = x + HPAD;
	S32 top = y;
	spawning_view->localPointToOtherView(left, top, &left, &top, menu->getParent());
	rect.setLeftTopAndSize( left, top,
							rect.getWidth(), rect.getHeight() );
	menu->setRect( rect );


	// Adjust context menu to fit onscreen
	LLRect mouse_rect;
	const S32 MOUSE_CURSOR_PADDING = 5;
	mouse_rect.setLeftTopAndSize(mouse_x - MOUSE_CURSOR_PADDING, 
		mouse_y + MOUSE_CURSOR_PADDING, 
		CURSOR_WIDTH + MOUSE_CURSOR_PADDING * 2, 
		CURSOR_HEIGHT + MOUSE_CURSOR_PADDING * 2);
	menu->translateIntoRectWithExclusion( menu_region_rect, mouse_rect, FALSE );
	menu->getParent()->sendChildToFront(menu);
}

///============================================================================
/// Class LLMenuBarGL
///============================================================================

static LLRegisterWidget<LLMenuBarGL> r2("menu_bar");

// Default constructor
LLMenuBarGL::LLMenuBarGL( const std::string& name )
:	LLMenuGL ( name, name )
{
	mHorizontalLayout = TRUE;
	mKeepFixedSize = TRUE;
	mAltKeyTrigger = FALSE;
}

// Default destructor
LLMenuBarGL::~LLMenuBarGL()
{
	std::for_each(mAccelerators.begin(), mAccelerators.end(), DeletePointer());
	mAccelerators.clear();
}

// virtual
LLXMLNodePtr LLMenuBarGL::getXML(bool save_children) const
{
	// Sorty of hacky: reparent items to this and then back at the end of the export
	LLView *orig_parent = NULL;
	item_list_t::const_iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		LLMenuItemGL* child = *item_iter;
		LLMenuItemBranchGL* branch = (LLMenuItemBranchGL*)child;
		LLMenuGL *menu = branch->getBranch();
		orig_parent = menu->getParent();
		menu->updateParent((LLView *)this);
	}

	LLXMLNodePtr node = LLMenuGL::getXML();

	node->setName(LL_MENU_BAR_GL_TAG);

	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		LLMenuItemGL* child = *item_iter;
		LLMenuItemBranchGL* branch = (LLMenuItemBranchGL*)child;
		LLMenuGL *menu = branch->getBranch();
		menu->updateParent(orig_parent);
	}

	return node;
}

LLView* LLMenuBarGL::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	BOOL opaque = FALSE;
	node->getAttributeBOOL("opaque", opaque);

	LLMenuBarGL *menubar = new LLMenuBarGL("menu");

	LLHandle<LLFloater> parent_handle;
	LLFloater* parent_floater = dynamic_cast<LLFloater*>(parent);
	if (parent_floater)
	{
		parent_handle = parent_floater->getHandle();
	}

	// We need to have the rect early so that it's around when building
	// the menu items
	LLRect view_rect;
	createRect(node, view_rect, parent, menubar->getRequiredRect());
	menubar->setRect(view_rect);

	if (node->hasAttribute("drop_shadow"))
	{
		BOOL drop_shadow = FALSE;
		node->getAttributeBOOL("drop_shadow", drop_shadow);
		menubar->setDropShadowed(drop_shadow);
	}

	menubar->setBackgroundVisible(opaque);
	LLColor4 color(0,0,0,0);
	if (opaque && LLUICtrlFactory::getAttributeColor(node,"color", color))
	{
		menubar->setBackgroundColor(color);
	}

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName("menu"))
		{
			LLMenuGL *menu = (LLMenuGL*)LLMenuGL::fromXML(child, parent, factory);
			menubar->appendMenu(menu);
			if (LLMenuGL::sMenuContainer != NULL)
			{
				menu->updateParent(LLMenuGL::sMenuContainer);
			}
			else
			{
				menu->updateParent(parent);
			}
		}
	}

	menubar->initFromXML(node, parent);

	BOOL create_jump_keys = FALSE;
	node->getAttributeBOOL("create_jump_keys", create_jump_keys);
	if (create_jump_keys)
	{
		menubar->createJumpKeys();
	}

	return menubar;
}

BOOL LLMenuBarGL::handleAcceleratorKey(KEY key, MASK mask)
{
	if (getHighlightedItem() && mask == MASK_NONE)
	{
		// unmodified key accelerators are ignored when navigating menu
		// (but are used as jump keys so will still work when appropriate menu is up)
		return FALSE;
	}
	BOOL result = LLMenuGL::handleAcceleratorKey(key, mask);
	if (result && mask & MASK_ALT)
	{
		// ALT key used to trigger hotkey, don't use as shortcut to open menu
		mAltKeyTrigger = FALSE;
	}

	if(!result
		&& (key == KEY_F10 && mask == MASK_CONTROL)
		&& !gKeyboard->getKeyRepeated(key)
		&& isInVisibleChain())
	{
		if (getHighlightedItem())
		{
			clearHoverItem();
		}
		else
		{
			// close menus originating from other menu bars when first opening menu via keyboard
			LLMenuGL::sMenuContainer->hideMenus();
			highlightNextItem(NULL);
			LLMenuGL::setKeyboardMode(TRUE);
		}
		return TRUE;
	}

	return result;
}

BOOL LLMenuBarGL::handleKeyHere(KEY key, MASK mask)
{
	static LLUICachedControl<bool> use_altkey_for_menus ("UseAltKeyForMenus", 0);
	if(key == KEY_ALT && !gKeyboard->getKeyRepeated(key) && use_altkey_for_menus)
	{
		mAltKeyTrigger = TRUE;
	}
	else // if any key other than ALT hit, clear out waiting for Alt key mode
	{
		mAltKeyTrigger = FALSE;
	}

	if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		LLMenuGL::setKeyboardMode(FALSE);
		// if any menus are visible, this will return TRUE, stopping further processing of ESCAPE key
		return LLMenuGL::sMenuContainer->hideMenus();
	}

	// before processing any other key, check to see if ALT key has triggered menu access
	checkMenuTrigger();

	return LLMenuGL::handleKeyHere(key, mask);
}

BOOL LLMenuBarGL::handleJumpKey(KEY key)
{
	// perform case-insensitive comparison
	key = toupper(key);
	navigation_key_map_t::iterator found_it = mJumpKeys.find(key);
	if(found_it != mJumpKeys.end() && found_it->second->getEnabled())
	{
		// switch to keyboard navigation mode
		LLMenuGL::setKeyboardMode(TRUE);

		found_it->second->setHighlight(TRUE);
		found_it->second->onCommit();
	}
	return TRUE;
}

BOOL LLMenuBarGL::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// clicks on menu bar closes existing menus from other contexts but leave
	// own menu open so that we get toggle behavior
	if (!getHighlightedItem() || !getHighlightedItem()->isActive())
	{
		LLMenuGL::sMenuContainer->hideMenus();
	}

	return LLMenuGL::handleMouseDown(x, y, mask);
}

void LLMenuBarGL::setVisible(BOOL visible)
{
	if(visible != getVisible())
	{
		if(!visible)
		{
			LL_DEBUGS() << "Hiding " << getName() << LL_ENDL;
		}
		else
		{
			LL_DEBUGS() << "Showing " << getName() << LL_ENDL;
		}
	}
	LLUICtrl::setVisible(visible);
}

void LLMenuBarGL::draw()
{
	LLMenuItemGL* itemp = getHighlightedItem();
	// If we are in mouse-control mode and the mouse cursor is not hovering over
	// the current highlighted menu item and it isn't open, then remove the
	// highlight. This is done via a polling mechanism here, as we don't receive
    // notifications when the mouse cursor moves off of us
	if (itemp && !itemp->isOpen() && !itemp->getHover() && !LLMenuGL::getKeyboardMode())
	{
		clearHoverItem();
	}

	checkMenuTrigger();

	LLMenuGL::draw();
}


void LLMenuBarGL::checkMenuTrigger()
{
	// has the ALT key been pressed and subsequently released?
	if (mAltKeyTrigger && !gKeyboard->getKeyDown(KEY_ALT))
	{
		// if alt key was released quickly, treat it as a menu access key
		// otherwise it was probably an Alt-zoom or similar action
		static LLUICachedControl<F32> menu_access_key_time ("MenuAccessKeyTime", 0);
		if (gKeyboard->getKeyElapsedTime(KEY_ALT) <= menu_access_key_time ||
			gKeyboard->getKeyElapsedFrameCount(KEY_ALT) < 2)
		{
			if (getHighlightedItem())
			{
				clearHoverItem();
			}
			else
			{
				// close menus originating from other menu bars
				LLMenuGL::sMenuContainer->hideMenus();

				highlightNextItem(NULL);
				LLMenuGL::setKeyboardMode(TRUE);
			}
		}
		mAltKeyTrigger = FALSE;
	}
}

BOOL LLMenuBarGL::jumpKeysActive()
{
	// require user to be in keyboard navigation mode to activate key triggers
	// as menu bars are always visible and it is easy to leave the mouse cursor over them
	return LLMenuGL::getKeyboardMode() && getHighlightedItem() && LLMenuGL::jumpKeysActive();
}

// rearrange the child rects so they fit the shape of the menu bar.
void LLMenuBarGL::arrange( void )
{
	U32 pos = 0;
	LLRect rect( 0, getRect().getHeight(), 0, 0 );
	item_list_t::const_iterator item_iter;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		LLMenuItemGL* item = *item_iter;
		if (item->getVisible())
		{
			rect.mLeft = pos;
			pos += item->getNominalWidth();
			rect.mRight = pos;
			item->setRect( rect );
			item->buildDrawLabel();
		}
	}
	reshape(rect.mRight, rect.getHeight());
}


S32 LLMenuBarGL::getRightmostMenuEdge()
{
	// Find the last visible menu
	item_list_t::reverse_iterator item_iter;
	for (item_iter = mItems.rbegin(); item_iter != mItems.rend(); ++item_iter)
	{
		if ((*item_iter)->getVisible())
		{
			break;
		}
	}

	if (item_iter == mItems.rend())
	{
		return 0;
	}
	return (*item_iter)->getRect().mRight;
}

// add a vertical separator to this menu
BOOL LLMenuBarGL::addSeparator(const std::string& name)
{
	LLMenuItemGL* separator = new LLMenuItemVerticalSeparatorGL(name);
	return append(separator);
}

// add a menu - this will create a drop down menu.
BOOL LLMenuBarGL::appendMenu( LLMenuGL* menu )
{
	if( menu == this )
	{
		LL_ERRS() << "** Attempt to attach menu to itself. This is certainly "
			   << "a logic error." << LL_ENDL;
	}

	BOOL success = TRUE;

	LLMenuItemBranchGL* branch = NULL;
	branch = new LLMenuItemBranchDownGL( menu->getName(), menu->getLabel(), menu->getHandle());
	success &= branch->addToAcceleratorList(&mAccelerators);
	success &= append( branch );
	branch->setJumpKey(branch->getJumpKey());
	menu->updateParent(LLMenuGL::sMenuContainer);

	return success;
}

BOOL LLMenuBarGL::handleHover( S32 x, S32 y, MASK mask )
{
	BOOL handled = FALSE;
	LLView* active_menu = NULL;

	BOOL no_mouse_data = mLastMouseX == 0 && mLastMouseY == 0;
	S32 mouse_delta_x = no_mouse_data ? 0 : x - mLastMouseX;
	S32 mouse_delta_y = no_mouse_data ? 0 : y - mLastMouseY;
	mMouseVelX = (mMouseVelX / 2) + (mouse_delta_x / 2);
	mMouseVelY = (mMouseVelY / 2) + (mouse_delta_y / 2);
	mLastMouseX = x;
	mLastMouseY = y;

	// if nothing currently selected or mouse has moved since last call, pick menu item via mouse
	// otherwise let keyboard control it
	if (!getHighlightedItem() || !LLMenuGL::getKeyboardMode() || llabs(mMouseVelX) > 0 || llabs(mMouseVelY) > 0)
	{
		// find current active menu
		for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			if (((LLMenuItemGL*)viewp)->isOpen())
			{
				active_menu = viewp;
			}
		}

		// check for new active menu
		for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if( viewp->getVisible() &&
				viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleHover(local_x, local_y, mask))
			{
				((LLMenuItemGL*)viewp)->setHighlight(TRUE);
				handled = TRUE;
				if (active_menu && active_menu != viewp)
				{
					((LLMenuItemGL*)viewp)->onCommit();
					LLMenuGL::setKeyboardMode(FALSE);
				}
				LLMenuGL::setKeyboardMode(FALSE);
			}
		}

		if (handled)
		{
			// set hover false on inactive menus
			for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
			{
				LLView* viewp = *child_it;
				S32 local_x = x - viewp->getRect().mLeft;
				S32 local_y = y - viewp->getRect().mBottom;
				if (!viewp->pointInView(local_x, local_y) && ((LLMenuItemGL*)viewp)->getHighlight())
				{
					((LLMenuItemGL*)viewp)->setHighlight(FALSE);
				}
			}
		}
	}

	getWindow()->setCursor(UI_CURSOR_ARROW);

	return TRUE;
}

///============================================================================
/// Class LLMenuHolderGL
///============================================================================
LLCoordGL LLMenuHolderGL::sContextMenuSpawnPos(S32_MAX, S32_MAX);
LLMenuHolderGL::LLMenuHolderGL()
	:	LLPanel(std::string("Menu Holder"))
{
	setMouseOpaque(FALSE);
	sItemActivationTimer.stop();
	mCanHide = TRUE;
}

LLMenuHolderGL::LLMenuHolderGL(const std::string& name, const LLRect& rect, BOOL mouse_opaque, U32 follows)
:	LLPanel(name, rect, FALSE)
{
	setMouseOpaque(mouse_opaque);
	sItemActivationTimer.stop();
	mCanHide = TRUE;
}

void LLMenuHolderGL::draw()
{
	LLView::draw();
	// now draw last selected item as overlay
	LLMenuItemGL* selecteditem = (LLMenuItemGL*)sItemLastSelectedHandle.get();
	if (selecteditem && selecteditem->getVisible() && sItemActivationTimer.getStarted() && sItemActivationTimer.getElapsedTimeF32() < ACTIVATE_HIGHLIGHT_TIME)
	{
		// make sure toggle items, for example, show the proper state when fading out
		selecteditem->buildDrawLabel();

		LLRect item_rect;
		selecteditem->localRectToOtherView(selecteditem->getLocalRect(), &item_rect, this);

		F32 interpolant = sItemActivationTimer.getElapsedTimeF32() / ACTIVATE_HIGHLIGHT_TIME;

		LLUI::pushMatrix();
		{
			LLUI::translate((F32)item_rect.mLeft, (F32)item_rect.mBottom, 0.f);
			LLColor4 bg_color(LLMenuItemGL::sHighlightBackground.mV[VRED],
				LLMenuItemGL::sHighlightBackground.mV[VGREEN],
				LLMenuItemGL::sHighlightBackground.mV[VBLUE],
				lerp(LLMenuItemGL::sHighlightBackground.mV[VALPHA], 0.f, interpolant));
			selecteditem->getMenu()->drawBackground(selecteditem, bg_color);
			selecteditem->draw();
		}
		LLUI::popMatrix();
	}
}

BOOL LLMenuHolderGL::handleMouseDown( S32 x, S32 y, MASK mask )
{
	BOOL handled = LLView::childrenHandleMouseDown(x, y, mask) != NULL;
	if (!handled)
	{
		// clicked off of menu, hide them all
		hideMenus();
	}
	return handled;
}

BOOL LLMenuHolderGL::handleRightMouseDown( S32 x, S32 y, MASK mask )
{
	BOOL handled = LLView::childrenHandleRightMouseDown(x, y, mask) != NULL;
	if (!handled)
	{
		// clicked off of menu, hide them all
		hideMenus();
	}
	return handled;
}

// This occurs when you mouse-down to spawn a context menu, hold the button
// down, move off the menu, then mouse-up.  We want this to close the menu.
BOOL LLMenuHolderGL::handleRightMouseUp( S32 x, S32 y, MASK mask )
{
	const S32 SLOP = 2;
	S32 spawn_dx = (x - sContextMenuSpawnPos.mX);
	S32 spawn_dy = (y - sContextMenuSpawnPos.mY);
	if (-SLOP <= spawn_dx && spawn_dx <= SLOP
		&& -SLOP <= spawn_dy && spawn_dy <= SLOP)
	{
		// we're still inside the slop region from spawning this menu
		// so interpret the mouse-up as a single-click to show and leave on
		// screen
		sContextMenuSpawnPos.set(S32_MAX, S32_MAX);
		return TRUE;
	}

	BOOL handled = LLView::childrenHandleRightMouseUp(x, y, mask) != NULL;
	if (!handled)
	{
		// clicked off of menu, hide them all
		hideMenus();
	}
	return handled;
}

BOOL LLMenuHolderGL::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
	BOOL handled =  false;
	LLMenuGL* const  pMenu  = dynamic_cast<LLMenuGL*>(getVisibleMenu());

	if (pMenu)
	{
		//eat TAB key - EXT-7000
		if (key == KEY_TAB && mask == MASK_NONE)
		{
			return TRUE;
		}

		//handle ESCAPE and RETURN key
		handled = LLPanel::handleKey(key, mask, called_from_parent);
		if (!handled)
		{
			if (pMenu->getHighlightedItem())
			{
				handled = pMenu->handleKey(key, mask, TRUE);
			}
			else
			{
				if (key == KEY_UP || key == KEY_DOWN) // Singu Note: Only highlight if the user actually meant to navigate through the menu
				{
					//highlight first enabled one
					if (pMenu->highlightNextItem(NULL))
					{
						handled = true;
					}
				}
			}
		}
	}

	return handled;

}

void LLMenuHolderGL::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	if (width != getRect().getWidth() || height != getRect().getHeight())
	{
		hideMenus();
	}
	LLView::reshape(width, height, called_from_parent);
}

LLView* const LLMenuHolderGL::getVisibleMenu() const
{
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLView* viewp = *child_it;
		if (viewp->getVisible() && dynamic_cast<LLMenuGL*>(viewp) != NULL && !dynamic_cast<LLMenuBarGL*>(viewp))
		{
			return viewp;
		}
	}
	return NULL;
}


BOOL LLMenuHolderGL::hideMenus()
{
	if (!mCanHide)
	{
		return FALSE;
	}

	sItemActivationTimer.stop();

	BOOL menu_visible = hasVisibleMenu();
	if (menu_visible)
	{
		LLMenuGL::setKeyboardMode(FALSE);
		// clicked off of menu, hide them all
		for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			if (dynamic_cast<LLMenuGL*>(viewp) != NULL && viewp->getVisible() && !dynamic_cast<LLMenuBarGL*>(viewp))
			{
				viewp->setVisible(FALSE);
			}
		}
	}
	//if (gFocusMgr.childHasKeyboardFocus(this))
	//{
	//	gFocusMgr.setKeyboardFocus(NULL);
	//}

	return menu_visible;
}

void LLMenuHolderGL::setActivatedItem(LLMenuItemGL* item)
{
	sItemLastSelectedHandle = item->getHandle();
	sItemActivationTimer.start();
}

///============================================================================
/// Class LLTearOffMenu
///============================================================================
LLTearOffMenu::LLTearOffMenu(LLMenuGL* menup) :
	LLFloater(menup->getName(), LLRect(0, 100, 100, 0), menup->getLabel(), FALSE, DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT, FALSE, FALSE)
{
	S32 floater_header_size = LLFLOATER_HEADER_SIZE;

	setName(menup->getName());
	setTitle(menup->getLabel());
	setCanMinimize(FALSE);
	// flag menu as being torn off
	menup->setTornOff(TRUE);
	// update menu layout as torn off menu (no spillover menus)
	menup->needsArrange();

	LLRect rect;
	menup->localRectToOtherView(LLRect(-1, menup->getRect().getHeight(), menup->getRect().getWidth() + 3, 0), &rect, gFloaterView);
	// make sure this floater is big enough for menu
	mTargetHeight = (F32)(rect.getHeight() + floater_header_size);
	reshape(rect.getWidth(), rect.getHeight());
	setRect(rect);

	// attach menu to floater
	menup->setFollows(FOLLOWS_LEFT | FOLLOWS_BOTTOM);
	mOldParent = menup->getParent();
	addChild(menup);
	menup->setVisible(TRUE);
	LLRect menu_rect = menup->getRect();
	menu_rect.setOriginAndSize( 1, 1,
		menu_rect.getWidth(), menu_rect.getHeight());
	menup->setRect(menu_rect);
	menup->setDropShadowed(FALSE);

	mMenu = menup;

	// highlight first item (tear off item will be disabled)
	mMenu->highlightNextItem(NULL);
}

LLTearOffMenu::~LLTearOffMenu()
{
}

void LLTearOffMenu::draw()
{
	mMenu->setBackgroundVisible(isBackgroundOpaque());
	mMenu->needsArrange();

	if (getRect().getHeight() != mTargetHeight)
	{
		// animate towards target height
		reshape(getRect().getWidth(), llceil(lerp((F32)getRect().getHeight(), mTargetHeight, LLSmoothInterpolation::getInterpolant(0.05f))));
	}
	LLFloater::draw();
}

void LLTearOffMenu::onFocusReceived()
{
	// if nothing is highlighted, just highlight first item
	if (!mMenu->getHighlightedItem())
	{
		mMenu->highlightNextItem(NULL);
	}

	// parent menu items get highlights so navigation logic keeps working
	LLMenuItemGL* parent_menu_item = mMenu->getParentMenuItem();
	while(parent_menu_item)
	{
		if (parent_menu_item->getMenu()->getVisible())
		{
			parent_menu_item->setHighlight(TRUE);
			parent_menu_item = parent_menu_item->getMenu()->getParentMenuItem();
		}
		else
		{
			break;
		}
	}
	LLFloater::onFocusReceived();
}

void LLTearOffMenu::onFocusLost()
{
	// remove highlight from parent item and our own menu
	mMenu->clearHoverItem();
	LLFloater::onFocusLost();
}

BOOL LLTearOffMenu::handleUnicodeChar(llwchar uni_char, BOOL called_from_parent)
{
	// pass keystrokes down to menu
	return mMenu->handleUnicodeChar(uni_char, TRUE);
}

BOOL LLTearOffMenu::handleKeyHere(KEY key, MASK mask)
{
	if (!mMenu->getHighlightedItem())
	{
		if (key == KEY_UP)
		{
			mMenu->highlightPrevItem(NULL);
			return TRUE;
		}
		else if (key == KEY_DOWN)
		{
			mMenu->highlightNextItem(NULL);
			return TRUE;
		}
	}
	// pass keystrokes down to menu
	return mMenu->handleKey(key, mask, TRUE);
}

void LLTearOffMenu::translate(S32 x, S32 y)
{
	if (x != 0 && y != 0)
	{
		// hide open sub-menus by clearing current hover item
		mMenu->clearHoverItem();
	}
	LLFloater::translate(x, y);
}

//static
LLTearOffMenu* LLTearOffMenu::create(LLMenuGL* menup)
{
	LLTearOffMenu* tearoffp = new LLTearOffMenu(menup);
	// keep onscreen
	gFloaterView->adjustToFitScreen(tearoffp, FALSE);
	tearoffp->open();	/* Flawfinder: ignore */

	return tearoffp;
}

void LLTearOffMenu::onClose(bool app_quitting)
{
	removeChild(mMenu);
	mOldParent->addChild(mMenu);
	mMenu->clearHoverItem();
	mMenu->setFollowsNone();
	mMenu->setBackgroundVisible(TRUE);
	mMenu->setVisible(FALSE);
	mMenu->setTornOff(FALSE);
	mMenu->setDropShadowed(TRUE);
	destroy();
}


LLContextMenuBranch::LLContextMenuBranch(const std::string& name, const std::string& label, LLContextMenu* branch)
:	LLMenuItemGL( name, label, KEY_NONE, MASK_NONE ),
	mBranch( branch )
{
	mBranch->hide();
	mBranch->setParentMenuItem(this);
}

// virtual
LLXMLNodePtr LLContextMenuBranch::getXML(bool save_children) const
{
	if (mBranch)
	{
		return mBranch->getXML();
	}

	return LLMenuItemGL::getXML();
}

// called to rebuild the draw label
void LLContextMenuBranch::buildDrawLabel( void )
{
	{
		// default enablement is this -- if any of the subitems are
		// enabled, this item is enabled. JC
		U32 sub_count = mBranch->getItemCount();
		U32 i;
		BOOL any_enabled = FALSE;
		for (i = 0; i < sub_count; i++)
		{
			LLMenuItemGL* item = mBranch->getItem(i);
			item->buildDrawLabel();
			if (item->getEnabled() && !item->getDrawTextDisabled() )
			{
				any_enabled = TRUE;
				break;
			}
		}
		setDrawTextDisabled(!any_enabled);
		setEnabled(TRUE);
	}

	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString( st );
	mDrawAccelLabel = st;

	// Singu Note: This is meaningless to pies
	mDrawBranchLabel = LLMenuGL::BRANCH_SUFFIX;
}

void LLContextMenuBranch::showSubMenu()
{
	if (getDrawTextDisabled()) return; // Singu Note: Don't open disabled submenus!
	S32 center_x;
	S32 center_y;
	static LLUICachedControl<bool> context("LiruUseContextMenus", false);
	if (context) // Use the edge of this item
	{
		localPointToScreen(getRect().getWidth(), getRect().getHeight(), &center_x, &center_y);
	}
	else // Use the center of the parent pie menu, and hide it
	{
		LLContextMenu* parent = static_cast<LLContextMenu*>(getParent());
		const LLRect& rect = parent->getRect();
		parent->localPointToScreen(rect.getWidth() / 2, rect.getHeight() / 2, &center_x, &center_y);
		parent->hide();
	}
	mBranch->show(center_x, center_y, context);
}

// onCommit() - do the primary funcationality of the menu item.
void LLContextMenuBranch::onCommit( void )
{
	showSubMenu();
}

void LLContextMenuBranch::setHighlight( BOOL highlight )
{
	if (highlight == getHighlight()) return;
	LLMenuItemGL::setHighlight(highlight);

	// Singu Note: Pie menus show subs only on click
	static LLUICachedControl<bool> context("LiruUseContextMenus", false);
	if (!context) return;

	if (highlight)
	{
		showSubMenu();
	}
	else
	{
		mBranch->hide();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
// class LLContextMenu
// A context menu
//-----------------------------------------------------------------------------
LLContextMenu::LLContextMenu(const std::string& name, const std::string& label)
:	LLMenuGL(name, label.empty() ? name : label),
	mHoveredAnyItem(FALSE),
	mHoverItem(NULL)
{
	//setBackgroundVisible(TRUE);
	LLMenuGL::setVisible(FALSE);
}

// virtual
LLXMLNodePtr LLContextMenu::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuGL::getXML();

	node->setName(LL_PIE_MENU_TAG);

	return node;
}

void LLContextMenu::initXML(LLXMLNodePtr node, LLView *context, LLUICtrlFactory *factory, bool is_context)
{
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName(LL_PIE_MENU_TAG))
		{
			// In context menus, more submenu is just an extension of the parent
			bool more(false);
			if (is_context && child->getAttribute_bool("more", more) && more)
			{
				//addSeparator(); // Singu Note: perhaps a separator (above) is in order, too?
				initXML(child, context, factory, true);
				//addSeparator(); // Singu Note: perhaps a separator (below) is in order, too?
			}
			else
			{
				// SUBMENU
				std::string name("menu");
				child->getAttributeString("name", name);
				std::string label(name);
				child->getAttributeString("label", label);

				// Singu Note: Pie Submenus are denoted with >, while context submenus have an obvious arrow at the end
				LLContextMenu* submenu = is_context ? new LLContextMenu(name, label) : new LLPieMenu(name, label + " >");
				appendContextSubMenu(submenu);
				submenu->initXML(child, context, factory, is_context);
			}
		}
		else
		{
			parseChildXML(child, context);
		}
	}
}

// virtual
void LLContextMenu::setVisible(BOOL visible)
{
	if (!visible)
		hide();
}

// Takes cursor position in screen space?
void LLContextMenu::show(S32 x, S32 y, bool context)
{
	if (getChildList()->empty())
	{
		// nothing to show, so abort
		return;
	}
	// Save click point for detecting cursor moves before mouse-up.
	// Must be in local coords to compare with mouseUp events.
	// If the mouse doesn't move, the menu will stay open ala the Mac.
	// See also LLMenuGL::showPopup()
	LLMenuHolderGL::sContextMenuSpawnPos.set(x,y);

	arrangeAndClear();

	S32 width = getRect().getWidth();
	S32 height = getRect().getHeight();
	const LLRect menu_region_rect = LLMenuGL::sMenuContainer->getMenuRect();
	LLView* parent_view = getParent();

	// Singu TODO: These could probably be combined a bit more.
	if (context) // Singu Note: Determine menu repositioning behavior based on menu type
	{
		// Open upwards if menu extends past bottom
		if (y - height < menu_region_rect.mBottom)
		{
			if (getParentMenuItem()) // Adjust if this is a submenu
			{
				y += height - getParentMenuItem()->getNominalHeight();
			}
			else
			{
				y += height;
			}
		}

		// Open out to the left if menu extends past right edge
		if (x + width > menu_region_rect.mRight)
		{
			if (getParentMenuItem())
			{
				x -= getParentMenuItem()->getRect().getWidth() + width;
			}
			else
			{
				x -= width;
			}
		}

		S32 local_x, local_y;
		parent_view->screenPointToLocal(x, y, &local_x, &local_y);

		LLRect rect;
		rect.setLeftTopAndSize(local_x, local_y, width, height);
		setRect(rect);
	}
	else
	{
		S32 local_x, local_y;
		parent_view->screenPointToLocal(x, y, &local_x, &local_y);

		LLRect rect;
		rect.setCenterAndSize(local_x, local_y, width, height);
		setRect(rect);
		if (!menu_region_rect.contains(rect)) // Adjust the pie rectangle to keep it on screen
		{
			S32 trans[2]={0,0};
			if (rect.mLeft < menu_region_rect.mLeft)
			{
				trans[0] = menu_region_rect.mLeft - rect.mLeft;
			}
			else if (rect.mRight > menu_region_rect.mRight)
			{
				trans[0] = menu_region_rect.mRight - rect.mRight;
			}
			if (rect.mBottom < menu_region_rect.mBottom)
			{
				trans[1] = menu_region_rect.mBottom - rect.mBottom;
			}
			else if (rect.mTop > menu_region_rect.mTop)
			{
				trans[1] = menu_region_rect.mTop - rect.mTop;
			}
			setRect(rect.translate(trans[0],trans[1]));
			LLUI::setMousePositionLocal(getParent(),rect.getCenterX(), rect.getCenterY());
		}
	}

	arrange();
	LLView::setVisible(TRUE);
}

void LLContextMenu::hide()
{
	if (!getVisible()) return;

	LLView::setVisible(FALSE);

	if (mHoverItem)
	{
		mHoverItem->setHighlight( FALSE );
	}
	mHoverItem = nullptr;
}


BOOL LLContextMenu::handleHover( S32 x, S32 y, MASK mask )
{
	LLMenuGL::handleHover(x, y, mask);

	LLMenuItemGL* item = getHighlightedItem();

	return handleHoverOver(item, x, y);
}

BOOL LLContextMenu::handleHoverOver(LLMenuItemGL* item, S32 x, S32 y)
{
	BOOL handled = FALSE;

	if (item && item->getEnabled())
	{
		getWindow()->setCursor(UI_CURSOR_ARROW);
		handled = TRUE;

		if (item != mHoverItem)
		{
			if (mHoverItem)
			{
				mHoverItem->setHighlight( FALSE );
			}
			mHoverItem = item;
			mHoverItem->setHighlight( TRUE );
		}
		mHoveredAnyItem = TRUE;
	}
	else
	{
		// clear out our selection
		if (mHoverItem)
		{
			mHoverItem->setHighlight(FALSE);
			mHoverItem = NULL;
		}
	}

	if( !handled && pointInView( x, y ) )
	{
		getWindow()->setCursor(UI_CURSOR_ARROW);
		handled = TRUE;
	}

	return handled;
}

// handleMouseDown and handleMouseUp are handled by LLMenuGL


BOOL LLContextMenu::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = FALSE;

	// The click was somewhere within our rectangle
	LLMenuItemGL* item = getHighlightedItem();

	S32 local_x = x - getRect().mLeft;
	S32 local_y = y - getRect().mBottom;

	BOOL clicked_in_menu = pointInView(local_x, local_y);

	// grab mouse if right clicking anywhere within pie (even deadzone in middle), to detect drag outside of pie
	if (clicked_in_menu)
	{
		// capture mouse cursor as if on initial menu show
		handled = TRUE;
	}

	if (item)
	{
		// lie to the item about where the click happened
		// to make sure it's within the item's rectangle
		if (item->handleMouseDown( 0, 0, mask ))
		{
			handled = TRUE;
		}
	}

	return handled;
}

BOOL LLContextMenu::handleRightMouseUp( S32 x, S32 y, MASK mask )
{
	S32 local_x = x - getRect().mLeft;
	S32 local_y = y - getRect().mBottom;

	if (!mHoveredAnyItem && !pointInView(local_x, local_y))
	{
		sMenuContainer->hideMenus();
		return TRUE;
	}


	BOOL result = handleMouseUp( x, y, mask );
	mHoveredAnyItem = FALSE;

	return result;
}

bool LLContextMenu::addChild(LLView* view, S32 tab_group)
{
	if (LLContextMenu* context = dynamic_cast<LLContextMenu*>(view))
		return appendContextSubMenu(context);
	if (LLMenuItemGL* item = dynamic_cast<LLMenuItemGL*>(view))
	{
		if (!mItems.empty() && mItems.back()->getType() == "separator" && item->getType() == "separator") // Singu TODO: Profile this? Does it matter?
			item->setVisible(false);
		return append(item);
	}
	if (LLMenuGL* menu = dynamic_cast<LLMenuGL*>(view))
		return appendMenu(menu);
	return false;
}


BOOL LLContextMenu::appendContextSubMenu(LLContextMenu* menu)
{
	if (menu == this)
	{
		LL_ERRS() << "Can't attach a context menu to itself" << LL_ENDL;
	}
	LLContextMenuBranch* item = new LLContextMenuBranch(menu->getName(), menu->getLabel(), menu);
	getParent()->addChild(item->getBranch());
	return append(item);
}

const S32 PIE_MENU_HEIGHT = 190;
const S32 PIE_MENU_WIDTH = 190;

//-----------------------------------------------------------------------------
// class LLPieMenu
// A circular menu of items, icons, etc.
//-----------------------------------------------------------------------------
LLPieMenu::LLPieMenu(const std::string& name, const std::string& label)
:	LLContextMenu(name, label),
	mFirstMouseDown(FALSE),
	mUseInfiniteRadius(FALSE),
	mHoverIndex(-1),
	mHoverThisFrame(FALSE),
	mOuterRingAlpha(1.f),
	mCurRadius(0.f),
	mRightMouseDown(FALSE)
{
	setRect(LLRect(0,PIE_MENU_HEIGHT,PIE_MENU_WIDTH,0));
}

// Separators on pie menus are invisible
bool LLPieMenu::addChild(LLView* view, S32 tab_group)
{
	if (LLContextMenu::addChild(view, tab_group) && view->getVisible())
	{
		LLMenuItemSeparatorGL* sep = dynamic_cast<LLMenuItemSeparatorGL*>(view);
		if(sep)
			sep->setVisible(false);
		return true;
	}
	return false;
}

BOOL LLPieMenu::handleHover( S32 x, S32 y, MASK mask )
{
	// release mouse capture after short period of visibility if we're using a finite boundary
	// so that right click outside of boundary will trigger new pie menu
	if (hasMouseCapture() &&
		!mRightMouseDown &&
		mShrinkBorderTimer.getStarted() &&
		mShrinkBorderTimer.getElapsedTimeF32() >= PIE_SHRINK_TIME)
	{
		gFocusMgr.setMouseCapture(NULL);
		mUseInfiniteRadius = FALSE;
	}

	mHoverThisFrame = TRUE;

	S32 index = mHoverIndex;
	mHoverIndex = pieItemIndexFromXY(x, y);
	BOOL handled = handleHoverOver(pieItemFromIndex(mHoverIndex), x, y);

	if (mHoverItem && mHoverIndex != index)
	{
		switch(mHoverIndex)
		{
		case 0:
			make_ui_sound("UISndPieMenuSliceHighlight0");
			break;
		case 1:
			make_ui_sound("UISndPieMenuSliceHighlight1");
			break;
		case 2:
			make_ui_sound("UISndPieMenuSliceHighlight2");
			break;
		case 3:
			make_ui_sound("UISndPieMenuSliceHighlight3");
			break;
		case 4:
			make_ui_sound("UISndPieMenuSliceHighlight4");
			break;
		case 5:
			make_ui_sound("UISndPieMenuSliceHighlight5");
			break;
		case 6:
			make_ui_sound("UISndPieMenuSliceHighlight6");
			break;
		case 7:
			make_ui_sound("UISndPieMenuSliceHighlight7");
			break;
		default:
			make_ui_sound("UISndPieMenuSliceHighlight0");
			break;
		}
	}
	return handled;
}

BOOL LLPieMenu::handleMouseDown( S32 x, S32 y, MASK mask )
{
	BOOL handled = FALSE;
	// The click was somewhere within our rectangle
	LLMenuItemGL *item = pieItemFromXY( x, y );

	if (item)
	{
		// lie to the item about where the click happened
		// to make sure it's within the item's rectangle
		handled = item->handleMouseDown( 0, 0, mask );
	}

	// always handle mouse down as mouse up will close open menus
	return TRUE;
}

BOOL LLPieMenu::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = FALSE;

	mRightMouseDown = TRUE;

	// The click was somewhere within our rectangle
	LLMenuItemGL *item = pieItemFromXY( x, y );
	S32 delta_x = x /*+ mShiftHoriz*/ - getLocalRect().getCenterX();
	S32 delta_y = y /*+ mShiftVert*/ - getLocalRect().getCenterY();
	BOOL clicked_in_pie = ((delta_x * delta_x) + (delta_y * delta_y) < mCurRadius*mCurRadius) || mUseInfiniteRadius;

	// grab mouse if right clicking anywhere within pie (even deadzone in middle), to detect drag outside of pie
	if (clicked_in_pie)
	{
		// capture mouse cursor as if on initial menu show
		gFocusMgr.setMouseCapture(this);
		mShrinkBorderTimer.stop();
		mUseInfiniteRadius = TRUE;
		handled = TRUE;
	}

	if (item)
	{
		// lie to the item about where the click happened
		// to make sure it's within the item's rectangle
		if (item->handleMouseDown( 0, 0, mask ))
		{
			handled = TRUE;
		}
	}

	return handled;
}

BOOL LLPieMenu::handleRightMouseUp( S32 x, S32 y, MASK mask )
{
	// release mouse capture when right mouse button released, and we're past the shrink time
	if (mShrinkBorderTimer.getStarted() &&
		mShrinkBorderTimer.getElapsedTimeF32() > PIE_SHRINK_TIME)
	{
		mUseInfiniteRadius = FALSE;
		gFocusMgr.setMouseCapture(NULL);
	}

	S32 delta_x = x /*+ mShiftHoriz*/ - getLocalRect().getCenterX();
	S32 delta_y = y /*+ mShiftVert*/ - getLocalRect().getCenterY();
	if (!mHoveredAnyItem && !mFirstMouseDown && (delta_x * delta_x) + (delta_y * delta_y) < PIE_CENTER_SIZE * PIE_CENTER_SIZE)
	{
		// user released right mouse button in middle of pie, interpret this as closing the menu
		sMenuContainer->hideMenus();
		return TRUE;
	}


	BOOL result = handleMouseUp( x, y, mask );
	mRightMouseDown = FALSE;
	mHoveredAnyItem = FALSE;

	return result;
}

BOOL LLPieMenu::handleMouseUp( S32 x, S32 y, MASK mask )
{
	BOOL handled = FALSE;

	// The click was somewhere within our rectangle
	LLMenuItemGL *item = pieItemFromXY( x, y );

	if (item)
	{
		// lie to the item about where the click happened
		// to make sure it's within the item's rectangle
		if (item->getEnabled())
		{
			handled = item->handleMouseUp( 0, 0, mask );
			hide();
		}
	}
	else if (!mRightMouseDown)
	{
		// if shift is held, click is in the view, and a parent menu exists, go back up
		if (mask & MASK_SHIFT && pointInView(x, y))
		{
			if (LLMenuItemGL* branch = getParentMenuItem())
			{
				if (LLContextMenu* parent = dynamic_cast<LLContextMenu*>(branch->getParent()))
				{
					hide();
					parent->show(LLMenuHolderGL::sContextMenuSpawnPos.mX, LLMenuHolderGL::sContextMenuSpawnPos.mY, false);
					return true;
				}
			}
		}
		// call hidemenus to make sure transient selections get cleared
		sMenuContainer->hideMenus();
	}

	if (handled)
	{
		make_ui_sound("UISndClickRelease");
	}

	if (!handled && !mUseInfiniteRadius)
	{
		// call hidemenus to make sure transient selections get cleared
		sMenuContainer->hideMenus();
	}

	if (mFirstMouseDown)
	{
		make_ui_sound("UISndPieMenuAppear");
		mFirstMouseDown = FALSE;
	}
	
	// *FIX: is this necessary?
	if (!mShrinkBorderTimer.getStarted())
	{
		mShrinkBorderTimer.start();
	}

	return handled;
}


// virtual
void LLPieMenu::draw()
{
	// clear hover if mouse moved away
	if (!mHoverThisFrame && mHoverItem)
	{
		mHoverItem->setHighlight(FALSE);
		mHoverItem = NULL;
		mHoverIndex = -1;
	}

	const auto& rect = getRect();
	// correct for non-square pixels
	F32 center_x = ((F32) rect.getWidth())/2;
	F32 center_y = ((F32) rect.getHeight())/2;
	constexpr S32 steps = 100;
	mCurRadius = PIE_SCALE_FACTOR * llmax(center_x, center_y);

	mOuterRingAlpha = mUseInfiniteRadius ? 0.f : 1.f;
	if (mShrinkBorderTimer.getStarted())
	{
		const auto& elapsed = mShrinkBorderTimer.getElapsedTimeF32();
		mOuterRingAlpha = clamp_rescale(elapsed, 0.f, PIE_SHRINK_TIME, 0.f, 1.f);
		mCurRadius *= clamp_rescale(elapsed, 0.f, PIE_SHRINK_TIME, 1.f, 1.f / PIE_SCALE_FACTOR);
	}


	gGL.pushUIMatrix();
	{
		gGL.translateUI(center_x, center_y, 0.f);

		static const LLUICachedControl<F32> line_width("PieMenuLineWidth");
		static const LLCachedControl<LLColor4> line_color(*LLUI::sColorsGroup, "PieMenuLineColor");
		static const LLCachedControl<LLColor4> bg_color(*LLUI::sColorsGroup, "PieMenuBgColor");
		static const LLCachedControl<LLColor4> selected_color(*LLUI::sColorsGroup, "PieMenuSelectedColor");

		// main body
		LLColor4 outer_color = bg_color;
		outer_color.mV[VALPHA] *= mOuterRingAlpha;
		gl_washer_2d( mCurRadius, (F32) PIE_CENTER_SIZE, steps, bg_color, outer_color );

		// selected wedge
		if (mHoverItem)
		{
			constexpr F32 arc_size = F_PI * 0.25f;

			F32 start_radians = (mHoverIndex * arc_size) - (arc_size * 0.5f);
			F32 end_radians = start_radians + arc_size;

			LLColor4 outer_color = selected_color;
			outer_color.mV[VALPHA] *= mOuterRingAlpha;
			gl_washer_segment_2d( mCurRadius, (F32)PIE_CENTER_SIZE, start_radians, end_radians, steps / 8, selected_color, outer_color );
		}

		LLUI::setLineWidth( line_width );

		// inner lines
		outer_color = line_color;
		outer_color.mV[VALPHA] *= mOuterRingAlpha;
		gl_washer_spokes_2d( mCurRadius, (F32)PIE_CENTER_SIZE, 8, line_color, outer_color );

		// inner circle
		gGL.color4fv( line_color().mV );
		gl_circle_2d( 0, 0, (F32)PIE_CENTER_SIZE, steps, FALSE );

		// outer circle
		gGL.color4fv( outer_color.mV );
		gl_circle_2d( 0, 0, mCurRadius, steps, FALSE );

		LLUI::setLineWidth(1.0f);
	}
	gGL.popUIMatrix();

	mHoverThisFrame = FALSE;

	LLView::draw();
}

// virtual
void LLPieMenu::drawBackground(LLMenuItemGL*, LLColor4&)
{
	// Selection is drawn in our draw call, do nothing here and override base drawing rectangles.
}

// virtual
BOOL LLPieMenu::append(LLMenuItemGL *item)
{
	item->setBriefItem(TRUE);
	item->setFont( LLFontGL::getFontSansSerifSmall() );
	return LLMenuGL::append(item);
}

// virtual
BOOL LLPieMenu::addSeparator(const std::string& name)
{
	return append(new LLMenuItemBlankGL(name));
}

// virtual
void LLPieMenu::arrange()
{
	// all divide by 6
	const S32 CARD_X = 60;
	const S32 DIAG_X = 48;
	const S32 CARD_Y = 76;
	const S32 DIAG_Y = 42;

	const S32 ITEM_CENTER_X[] = { CARD_X, DIAG_X,      0, -DIAG_X, -CARD_X, -DIAG_X,       0,  DIAG_X };
	const S32 ITEM_CENTER_Y[] = {      0, DIAG_Y, CARD_Y,  DIAG_Y,       0, -DIAG_Y, -CARD_Y, -DIAG_Y };

	S32 font_height = 0;
	if( mItems.size() )
	{
		font_height = (*mItems.begin())->getNominalHeight();
	}
	S32 item_width = 0;

//	F32 sin_delta = OO_SQRT2;	// sin(45 deg)
//	F32 cos_delta = OO_SQRT2;	// cos(45 deg)

	// TODO: Compute actual bounding rect for menu

	LLRect rect = getRect();

	// place items around a circle, with item 0 at positive X,
	// rotating counter-clockwise
	item_list_t::iterator item_iter;
	S32 i = 0;
	for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
	{
		LLMenuItemGL *item = *item_iter;

		item_width = item->getNominalWidth();

		// Put in the right place around a circle centered at 0,0
		rect.setCenterAndSize(ITEM_CENTER_X[i],
							  ITEM_CENTER_Y[i],
							  item_width, font_height );

		// Correct for the actual rectangle size
		rect.translate( getRect().getWidth()/2, getRect().getHeight()/2 );

		item->setRect( rect );

		// Make sure enablement is correct
		item->buildDrawLabel();
		i++;
	}
}

LLMenuItemGL *LLPieMenu::pieItemFromXY(S32 x, S32 y)
{
	return pieItemFromIndex(pieItemIndexFromXY(x, y));
}

S32 LLPieMenu::pieItemIndexFromXY(S32 x, S32 y)
{
	// An arc of the pie menu is 45 degrees
	const F32 ARC_DEG = 45.f;

	// correct for non-square pixels
	S32 delta_x = x - getRect().getWidth() / 2;
	S32 delta_y = y - getRect().getHeight() / 2;

	// circle safe zone in the center
	S32 dist_squared = delta_x*delta_x + delta_y*delta_y;
	if (dist_squared < PIE_CENTER_SIZE*PIE_CENTER_SIZE)
	{
		return -1;
	}

	// infinite radius is only used with right clicks
	S32 radius = llmax( getRect().getWidth()/2, getRect().getHeight()/2 );
	if (!(mUseInfiniteRadius && mRightMouseDown) && dist_squared > radius * radius)
	{
		return -1;
	}

	F32 angle = RAD_TO_DEG * (F32) atan2((F32)delta_y, (F32)delta_x);

	// rotate marks CCW so that east = [0, ARC_DEG) instead of
	// [-ARC_DEG/2, ARC_DEG/2)
	angle += ARC_DEG / 2.f;

	// make sure we're only using positive angles
	if (angle < 0.f) angle += 360.f;

	return S32( angle / ARC_DEG );
}

LLMenuItemGL* LLPieMenu::pieItemFromIndex(S32 which)
{
	if (0 <= which && which < (S32)mItems.size() )
	{
		item_list_t::iterator item_iter;
		for (item_iter = mItems.begin(); item_iter != mItems.end(); ++item_iter)
		{
			if (which == 0)
			{
				return (*item_iter)->getVisible() ? (*item_iter) : NULL;
			}
			--which;
		}
	}

	return NULL;
}


// virtual
void LLPieMenu::show(S32 x, S32 y, bool mouse_down)
{
	LLContextMenu::show(x, y, false);

	// *FIX: what happens when mouse buttons reversed?
	mRightMouseDown = mouse_down;
	mFirstMouseDown = mouse_down;
	mUseInfiniteRadius = TRUE;
	mHoveredAnyItem = FALSE;

	if (!mFirstMouseDown) make_ui_sound("UISndPieMenuAppear");

	// we want all mouse events in case user does quick right click again off of pie menu
	// rectangle, to support gestural menu traversal
	gFocusMgr.setMouseCapture(this);

	mouse_down ? mShrinkBorderTimer.stop() : mShrinkBorderTimer.start();
}

// virtual
void LLPieMenu::hide()
{
	LLContextMenu::hide();
	make_ui_sound("UISndPieMenuHide");

	mFirstMouseDown = FALSE;
	mRightMouseDown = FALSE;
	mUseInfiniteRadius = FALSE;
	mHoveredAnyItem = FALSE;

	gFocusMgr.setMouseCapture(NULL);
}

