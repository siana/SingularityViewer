/** 
 * @file llpanel.cpp
 * @brief LLPanel base class
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

// Opaque view with a background and a border.  Can contain LLUICtrls.

#include "linden_common.h"

#include "llpanel.h"

#include "llfocusmgr.h"
#include "llfontgl.h"
#include "lllocalcliprect.h"
#include "llrect.h"
#include "llerror.h"
#include "lltimer.h"

#include "llmenugl.h"
//#include "llstatusbar.h"
#include "llui.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llcontrol.h"
#include "lltextbox.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "lluiimage.h"
#include "llviewborder.h"
#include "llbutton.h"
#include "llnotificationsutil.h"
#include "llfasttimer.h"

#include "llnotifications.h"

// LLLayoutStack
#include "llresizebar.h"
#include "llcriticaldamp.h"

static LLRegisterWidget<LLPanel> r1("panel");

void LLPanel::init()
{
	// mRectControl
	mBgColorAlpha        = LLUI::sColorsGroup->getColor( "DefaultBackgroundColor" );
	mBgColorOpaque       = LLUI::sColorsGroup->getColor( "FocusBackgroundColor" );
	mDefaultBtnHighlight = LLUI::sColorsGroup->getColor( "DefaultHighlightLight" );
	mBgVisible = FALSE;
	mBgOpaque = FALSE;
	mBorder = NULL;
	mDefaultBtn = NULL;
	setIsChrome(FALSE); //is this a decorator to a live window or a form?

	setTabStop(FALSE);
	mVisibleSignal = NULL;
}

LLPanel::LLPanel()
: mRectControl(),
	mCommitCallbackRegistrar(false),
	mEnableCallbackRegistrar(false)
{
	init();
	setName(std::string("panel"));
}

LLPanel::LLPanel(const std::string& name)
:	LLUICtrl(name),
	mRectControl(),
	mCommitCallbackRegistrar(false),
	mEnableCallbackRegistrar(false)
{
	init();
}


LLPanel::LLPanel(const std::string& name, const LLRect& rect, BOOL bordered)
:	LLUICtrl(name,rect),
	mRectControl(),
	mCommitCallbackRegistrar(false),
	mEnableCallbackRegistrar(false)
{
	init();
	if (bordered)
	{
		addBorder();
	}
}


LLPanel::LLPanel(const std::string& name, const std::string& rect_control, BOOL bordered)
:	LLUICtrl(name, LLUI::sConfigGroup->getRect(rect_control)),
	mRectControl( rect_control ),
	mCommitCallbackRegistrar(false),
	mEnableCallbackRegistrar(false)
{
	init();
	if (bordered)
	{
		addBorder();
	}
}

LLPanel::~LLPanel()
{
	storeRectControl();
	delete mVisibleSignal;
}

// virtual
BOOL LLPanel::isPanel() const
{
	return TRUE;
}

// virtual
BOOL LLPanel::postBuild()
{
	return TRUE;
}

void LLPanel::addBorder(LLViewBorder::EBevel border_bevel,
						LLViewBorder::EStyle border_style, S32 border_thickness)
{
	removeBorder();
	mBorder = new LLViewBorder( std::string("panel border"), 
								LLRect(0, getRect().getHeight(), getRect().getWidth(), 0), 
								border_bevel, border_style, border_thickness );
	mBorder->setSaveToXML(false);
	addChild( mBorder );
}

void LLPanel::removeBorder()
{
	if (mBorder)
	{
		removeChild(mBorder);
		delete mBorder;
		mBorder = NULL;
	}
}


// virtual
void LLPanel::clearCtrls()
{
	LLView::ctrl_list_t ctrls = getCtrlList();
	for (LLView::ctrl_list_t::iterator ctrl_it = ctrls.begin(); ctrl_it != ctrls.end(); ++ctrl_it)
	{
		LLUICtrl* ctrl = *ctrl_it;
		ctrl->setFocus( FALSE );
		ctrl->setEnabled( FALSE );
		ctrl->clear();
	}
}

void LLPanel::setCtrlsEnabled( BOOL b )
{
	LLView::ctrl_list_t ctrls = getCtrlList();
	for (LLView::ctrl_list_t::iterator ctrl_it = ctrls.begin(); ctrl_it != ctrls.end(); ++ctrl_it)
	{
		LLUICtrl* ctrl = *ctrl_it;
		ctrl->setEnabled( b );
	}
}

void LLPanel::draw()
{
	// draw background
	if( mBgVisible )
	{
		F32 alpha = getCurrentTransparency();

		LLRect&& local_rect = getLocalRect();
		if (mBgOpaque )
		{
			gl_rect_2d( local_rect, mBgColorOpaque % alpha);
		}
		else
		{
			gl_rect_2d( local_rect, mBgColorAlpha % alpha);
		}
	}

	updateDefaultBtn();

	LLView::draw();
}

/*virtual*/
void LLPanel::setAlpha(F32 alpha)
{
	mBgColorOpaque.setAlpha(alpha);
}

void LLPanel::updateDefaultBtn()
{
	// This method does not call LLView::draw() so callers will need
	// to take care of that themselves at the appropriate place in
	// their rendering sequence

	if( mDefaultBtn)
	{
		if (gFocusMgr.childHasKeyboardFocus( this ) && mDefaultBtn->getEnabled())
		{
			LLButton* buttonp = dynamic_cast<LLButton*>(gFocusMgr.getKeyboardFocus());
			BOOL focus_is_child_button = buttonp && buttonp->getCommitOnReturn();
			// only enable default button when current focus is not a return-capturing button
			mDefaultBtn->setBorderEnabled(!focus_is_child_button);
		}
		else
		{
			mDefaultBtn->setBorderEnabled(FALSE);
		}
	}
}

void LLPanel::refresh()
{
	// do nothing by default
	// but is automatically called in setFocus(TRUE)
}

void LLPanel::setDefaultBtn(LLButton* btn)
{
	if (mDefaultBtn && mDefaultBtn->getEnabled())
	{
		mDefaultBtn->setBorderEnabled(FALSE);
	}
	mDefaultBtn = btn; 
	if (mDefaultBtn)
	{
		mDefaultBtn->setBorderEnabled(TRUE);
	}
}

void LLPanel::setDefaultBtn(const std::string& id)
{
	LLButton *button = getChild<LLButton>(id);
	if (button)
	{
		setDefaultBtn(button);
	}
	else
	{
		setDefaultBtn(NULL);
	}
}

BOOL LLPanel::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;

	LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());

	// handle user hitting ESC to defocus
	if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		setFocus(FALSE);
		return TRUE;
	}
	else if( (mask == MASK_SHIFT) && (KEY_TAB == key))
	{
		//SHIFT-TAB
		if (cur_focus)
		{
			LLUICtrl* focus_root = cur_focus->findRootMostFocusRoot();
			if (focus_root)
			{
				handled = focus_root->focusPrevItem(FALSE);
			}
		}
	}
	else if( (mask == MASK_NONE ) && (KEY_TAB == key))	
	{
		//TAB
		if (cur_focus)
		{
			LLUICtrl* focus_root = cur_focus->findRootMostFocusRoot();
			if (focus_root)
			{
				handled = focus_root->focusNextItem(FALSE);
			}
		}
	}
	
	// If RETURN was pressed and something has focus, call onCommit()
	if (!handled && cur_focus && key == KEY_RETURN && mask == MASK_NONE)
	{
		if (cur_focus->getCommitOnReturn())
		{
			// current focus is a return-capturing element,
			// let *that* element handle the return key
			handled = FALSE; 
		}
		else if (mDefaultBtn && mDefaultBtn->getVisible() && mDefaultBtn->getEnabled())
		{
			// If we have a default button, click it when return is pressed
			mDefaultBtn->onCommit();
			handled = TRUE;
		}
		else if (cur_focus->acceptsTextInput())
		{
			// call onCommit for text input handling control
			cur_focus->onCommit();
			handled = TRUE;
		}
	}

	return handled;
}

BOOL LLPanel::checkRequirements()
{
	if (!mRequirementsError.empty())
	{
		LLSD args;
		args["COMPONENTS"] = mRequirementsError;
		args["FLOATER"] = getName();

		LL_WARNS() << getName() << " failed requirements check on: \n"  
				<< mRequirementsError << LL_ENDL;
		
		LLNotifications::instance().add(LLNotification::Params("FailedRequirementsCheck").payload(args));
		mRequirementsError.clear();
		return FALSE;
	}

	return TRUE;
}

void LLPanel::handleVisibilityChange ( BOOL new_visibility )
{
	LLUICtrl::handleVisibilityChange ( new_visibility );
	if (mVisibleSignal)
		(*mVisibleSignal)(this, LLSD(new_visibility) ); // Pass BOOL as LLSD
}

void LLPanel::setFocus(BOOL b)
{
	if( b && !hasFocus())
	{
		// give ourselves focus preemptively, to avoid infinite loop
		LLUICtrl::setFocus(TRUE);
		// then try to pass to first valid child
		focusFirstItem();
	}
	else
	{
		LLUICtrl::setFocus(b);
	}
}

void LLPanel::setBorderVisible(BOOL b)
{
	if (mBorder)
	{
		mBorder->setVisible( b );
	}
}

LLTrace::BlockTimerStatHandle FTM_PANEL_CONSTRUCTION("Panel Construction");
// virtual
LLXMLNodePtr LLPanel::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_PANEL_TAG);
	if (mBorder && mBorder->getVisible())
	{
		node->createChild("border", TRUE)->setBoolValue(TRUE);
	}

	if (!mRectControl.empty())
	{
		node->createChild("rect_control", TRUE)->setStringValue(mRectControl);
	}

	if (!mLabel.empty())
	{
		node->createChild("label", TRUE)->setStringValue(mLabel);
	}
	
	ui_string_map_t::const_iterator i = mUIStrings.begin();
	ui_string_map_t::const_iterator end = mUIStrings.end();
	for (; i != end; ++i)
	{
		LLXMLNodePtr child_node = node->createChild("string", FALSE);
		child_node->setStringValue(i->second);
		child_node->createChild("name", TRUE)->setStringValue(i->first);
	}

	if (save_children)
	{
		LLView::child_list_const_reverse_iter_t rit;
		for (rit = getChildList()->rbegin(); rit != getChildList()->rend(); ++rit)
		{
			LLView* childp = *rit;

			if (childp->getSaveToXML())
			{
				LLXMLNodePtr xml_node = childp->getXML();

				node->addChild(xml_node);
			}
		}
	}

	return node;
}

LLView* LLPanel::fromXML(LLXMLNodePtr node, LLView* parent, LLUICtrlFactory *factory)
{
	std::string name("panel");
	node->getAttributeString("name", name);

	std::string class_attr;
	node->getAttributeString("class", class_attr);

	LLPanel* panelp = NULL;

	{
		if(!class_attr.empty())
		{
			panelp = LLRegisterPanelClass::instance().createPanelClass(class_attr);
		}

		if (!panelp)
		{
			panelp = factory->createFactoryPanel(name);
			/* Singu TODO: Future?
			llassert(panelp);

			if (!panelp)
			{
				return NULL; // :(
			}*/
		}
	}
	LL_RECORD_BLOCK_TIME(FTM_PANEL_CONSTRUCTION);

	// Fall back on a default panel, if there was no special factory.
	if (!panelp)
	{
		LLRect rect;
		createRect(node, rect, parent, LLRect());
		// create a new panel without a border, by default
		panelp = new LLPanel(name, rect, FALSE);
		// for local registry callbacks; define in constructor, referenced in XUI or postBuild
		panelp->mCommitCallbackRegistrar.pushScope(); 
		panelp->mEnableCallbackRegistrar.pushScope();
		panelp->initPanelXML(node, parent, factory);
		panelp->mCommitCallbackRegistrar.popScope();
		panelp->mEnableCallbackRegistrar.popScope();
		// preserve panel's width and height, but override the location
		const LLRect& panelrect = panelp->getRect();
		S32 w = panelrect.getWidth();
		S32 h = panelrect.getHeight();
		rect.setLeftTopAndSize(rect.mLeft, rect.mTop, w, h);
		panelp->setRect(rect);
	}
	else
	{
		if(!factory->builtPanel(panelp))
		{
			// for local registry callbacks; define in constructor, referenced in XUI or postBuild
			panelp->mCommitCallbackRegistrar.pushScope(); 
			panelp->mEnableCallbackRegistrar.pushScope();
			panelp->initPanelXML(node, parent, factory);
			panelp->mCommitCallbackRegistrar.popScope();
			panelp->mEnableCallbackRegistrar.popScope();
		}
		else
		{
			LLRect new_rect = panelp->getRect();
			// override rectangle with embedding parameters as provided
			panelp->createRect(node, new_rect, parent);
			panelp->setOrigin(new_rect.mLeft, new_rect.mBottom);
			panelp->setShape(new_rect);
			// optionally override follows flags from including nodes
			panelp->parseFollowsFlags(node);
		}
	}

	return panelp;
}

BOOL LLPanel::initPanelXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name = getName();
	node->getAttributeString("name", name);
	setName(name);

	setPanelParameters(node, parent);

	initChildrenXML(node, factory);

	std::string xml_filename;
	node->getAttributeString("filename", xml_filename);

	BOOL didPost;

	if (!xml_filename.empty())
	{
		didPost = factory->buildPanel(this, xml_filename, NULL);

		LLRect new_rect = getRect();
		// override rectangle with embedding parameters as provided
		createRect(node, new_rect, parent);
		setOrigin(new_rect.mLeft, new_rect.mBottom);
		setShape(new_rect);
		// optionally override follows flags from including nodes
		parseFollowsFlags(node);
	}
	else
	{
		didPost = FALSE;
	}
	
	if (!didPost)
	{
		postBuild();
		didPost = TRUE;
	}

	return didPost;
}

void LLPanel::initChildrenXML(LLXMLNodePtr node, LLUICtrlFactory* factory)
{
	std::string kidstring(node->getName()->mString);
	kidstring += ".string";
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		// look for string declarations for programmatic text
		if (child->hasName("string") || child->hasName(kidstring))
		{
			std::string string_name;
			child->getAttributeString("name", string_name);
			if (!string_name.empty())
			{
				std::string contents = child->getTextContents();
				child->getAttributeString("value", contents);
				mUIStrings[string_name] = contents;
			}
		}
		else
		{
			factory->createWidget(this, child);
		}
	}
}

void LLPanel::setPanelParameters(LLXMLNodePtr node, LLView* parent)
{
	/////// Rect, follows, tool_tip, enabled, visible attributes ///////
	initFromXML(node, parent);

	/////// Border attributes ///////
	BOOL border = mBorder != NULL;
	node->getAttributeBOOL("border", border);
	if (border)
	{
		LLViewBorder::EBevel bevel_style = LLViewBorder::BEVEL_OUT;
		LLViewBorder::getBevelFromAttribute(node, bevel_style);

		LLViewBorder::EStyle border_style = LLViewBorder::STYLE_LINE;
		std::string border_string;
		node->getAttributeString("border_style", border_string);
		LLStringUtil::toLower(border_string);

		if (border_string == "texture")
		{
			border_style = LLViewBorder::STYLE_TEXTURE;
		}

		S32 border_thickness = LLPANEL_BORDER_WIDTH;
		node->getAttributeS32("border_thickness", border_thickness);

		addBorder(bevel_style, border_style, border_thickness);
	}
	else
	{
		removeBorder();
	}

	/////// Background attributes ///////
	BOOL background_visible = mBgVisible;
	node->getAttributeBOOL("background_visible", background_visible);
	setBackgroundVisible(background_visible);
	
	BOOL background_opaque = mBgOpaque;
	node->getAttributeBOOL("background_opaque", background_opaque);
	setBackgroundOpaque(background_opaque);

	LLColor4 color;
	color = mBgColorOpaque;
	LLUICtrlFactory::getAttributeColor(node,"bg_opaque_color", color);
	setBackgroundColor(color);

	color = mBgColorAlpha;
	LLUICtrlFactory::getAttributeColor(node,"bg_alpha_color", color);
	setTransparentColor(color);

	std::string label = getLabel();
	node->getAttributeString("label", label);
	setLabel(label);
}

bool LLPanel::hasString(const std::string& name)
{
	return mUIStrings.find(name) != mUIStrings.end();
}

std::string LLPanel::getString(const std::string& name, const LLStringUtil::format_map_t& args) const
{
	ui_string_map_t::const_iterator found_it = mUIStrings.find(name);
	if (found_it != mUIStrings.end())
	{
		// make a copy as format works in place
		LLUIString formatted_string = LLUIString(found_it->second);
		formatted_string.setArgList(args);
		return formatted_string.getString();
	}
	std::string err_str("Failed to find string " + name + " in panel " + getName()); // *TODO: Translate
	// *TODO: once the QAR-369 ui-cleanup work on settings is in we need to change the following line to be
	//if(LLUI::sConfigGroup->getBOOL("QAMode"))
	if(LLUI::sQAMode)
	{
		LL_ERRS() << err_str << LL_ENDL;
	}
	else
	{
		LL_WARNS() << err_str << LL_ENDL;
	}
	return LLStringUtil::null;
}

std::string LLPanel::getString(const std::string& name) const
{
	ui_string_map_t::const_iterator found_it = mUIStrings.find(name);
	if (found_it != mUIStrings.end())
	{
		return found_it->second;
	}
	std::string err_str("Failed to find string " + name + " in panel " + getName()); // *TODO: Translate
	if(LLUI::sQAMode)
	{
		LL_ERRS() << err_str << LL_ENDL;
	}
	else
	{
		LL_WARNS() << err_str << LL_ENDL;
	}
	return LLStringUtil::null;
}


void LLPanel::childSetVisible(const std::string& id, bool visible)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setVisible(visible);
	}
}

bool LLPanel::childIsVisible(const std::string& id) const
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		return (bool)child->getVisible();
	}
	return false;
}

void LLPanel::childSetEnabled(const std::string& id, bool enabled)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setEnabled(enabled);
	}
}

void LLPanel::childSetTentative(const std::string& id, bool tentative)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setTentative(tentative);
	}
}

bool LLPanel::childIsEnabled(const std::string& id) const
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		return (bool)child->getEnabled();
	}
	return false;
}


void LLPanel::childSetToolTip(const std::string& id, const std::string& msg)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setToolTip(msg);
	}
}

void LLPanel::childSetRect(const std::string& id, const LLRect& rect)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setRect(rect);
	}
}

bool LLPanel::childGetRect(const std::string& id, LLRect& rect) const
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		rect = child->getRect();
		return true;
	}
	return false;
}

void LLPanel::childSetFocus(const std::string& id, BOOL focus)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setFocus(focus);
	}
}

BOOL LLPanel::childHasFocus(const std::string& id)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		return child->hasFocus();
	}
	else
	{
		childNotFound(id);
		return FALSE;
	}
}

// *TODO: Deprecate; for backwards compatability only:
// Prefer getChild<LLUICtrl>("foo")->setCommitCallback(boost:bind(...)),
// which takes a generic slot.  Or use mCommitCallbackRegistrar.add() with
// a named callback and reference it in XML.
void LLPanel::childSetCommitCallback(const std::string& id, boost::function<void (LLUICtrl*,void*)> cb, void* data)
{
	LLUICtrl* child = findChild<LLUICtrl>(id);
	if (child)
	{
		child->setCommitCallback(boost::bind(cb, child, data));
	}
}

void LLPanel::childSetValidate(const std::string& id, boost::function<bool (const LLSD& data)> cb)
{
	LLUICtrl* child = findChild<LLUICtrl>(id);
	if (child)
	{
		child->setValidateBeforeCommit(cb);
	}
}

void LLPanel::childSetColor(const std::string& id, const LLColor4& color)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setColor(color);
	}
}
void LLPanel::childSetAlpha(const std::string& id, F32 alpha)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setAlpha(alpha);
	}
}

LLCtrlSelectionInterface* LLPanel::childGetSelectionInterface(const std::string& id) const
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		return child->getSelectionInterface();
	}
	return NULL;
}

LLCtrlListInterface* LLPanel::childGetListInterface(const std::string& id) const
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		return child->getListInterface();
	}
	return NULL;
}

LLCtrlScrollInterface* LLPanel::childGetScrollInterface(const std::string& id) const
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		return child->getScrollInterface();
	}
	return NULL;
}

void LLPanel::childSetValue(const std::string& id, LLSD value)
{
	LLView* child = getChild<LLView>(id, true);
	if (child)
	{
		child->setValue(value);
	}
}

LLSD LLPanel::childGetValue(const std::string& id) const
{
	LLView* child = getChild<LLView>(id, true);
	if (child)
	{
		return child->getValue();
	}
	// Not found => return undefined
	return LLSD();
}

BOOL LLPanel::childSetTextArg(const std::string& id, const std::string& key, const LLStringExplicit& text)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		return child->setTextArg(key, text);
	}
	return FALSE;
}

BOOL LLPanel::childSetLabelArg(const std::string& id, const std::string& key, const LLStringExplicit& text)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		return child->setLabelArg(key, text);
	}
	return FALSE;
}

BOOL LLPanel::childSetToolTipArg(const std::string& id, const std::string& key, const LLStringExplicit& text)
{
	LLView* child = getChildView(id, true, FALSE);
	if (child)
	{
		return child->setToolTipArg(key, text);
	}
	return FALSE;
}

void LLPanel::childSetMinValue(const std::string& id, LLSD min_value)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setMinValue(min_value);
	}
}

void LLPanel::childSetMaxValue(const std::string& id, LLSD max_value)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setMaxValue(max_value);
	}
}

void LLPanel::childShowTab(const std::string& id, const std::string& tabname, bool visible)
{
	LLTabContainer* child = getChild<LLTabContainer>(id);
	if (child)
	{
		child->selectTabByName(tabname);
	}
}

LLPanel *LLPanel::childGetVisibleTab(const std::string& id) const
{
	LLTabContainer* child = getChild<LLTabContainer>(id);
	if (child)
	{
		return child->getCurrentPanel();
	}
	return NULL;
}
void LLPanel::childSetWrappedText(const std::string& id, const std::string& text, bool visible)
{
	LLTextBox* child = getChild<LLTextBox>(id);
	if (child)
	{
		child->setVisible(visible);
		child->setWrappedText(text);
	}
}



void LLPanel::childSetAction(const std::string& id, const commit_signal_t::slot_type& function)
{
	LLButton* button = getChild<LLButton>(id);
	if (button)
	{
		button->setClickedCallback(function);
	}
}

void LLPanel::childSetAction(const std::string& id, boost::function<void(void*)> function, void* value)
{
	LLButton* button = getChild<LLButton>(id);
	if (button)
	{
		button->setClickedCallback(boost::bind(function, value));
	}
}

void LLPanel::childSetActionTextbox(const std::string& id, boost::function<void(void*)> function, void* value)
{
	LLTextBox* textbox = getChild<LLTextBox>(id);
	if (textbox)
	{
		textbox->setClickedCallback(boost::bind(function, value));
	}
}

void LLPanel::childSetControlName(const std::string& id, const std::string& control_name)
{
	LLView* view = getChild<LLView>(id);
	if (view)
	{
		view->setControlName(control_name, NULL);
	}
}

boost::signals2::connection LLPanel::setVisibleCallback( const commit_signal_t::slot_type& cb )
{
	if (!mVisibleSignal)
	{
		mVisibleSignal = new commit_signal_t();
	}

	return mVisibleSignal->connect(cb);
}

//virtual
LLView* LLPanel::getChildView(const std::string& name, BOOL recurse, BOOL create_if_missing) const
{
	// just get child, don't try to create a dummy one
	LLView* view = LLUICtrl::getChildView(name, recurse, FALSE);
	if (!view && !recurse)
	{
		childNotFound(name);
	}
	if (!view && create_if_missing)
	{
		view = createDummyWidget<LLView>(name);
	}
	return view;
}

void LLPanel::childNotFound(const std::string& id) const
{
	if (mExpectedMembers.find(id) == mExpectedMembers.end())
	{
		mNewExpectedMembers.insert(id);
	}
}

void LLPanel::childDisplayNotFound()
{
	if (mNewExpectedMembers.empty())
	{
		return;
	}
	std::string msg;
	expected_members_list_t::iterator itor;
	for (itor=mNewExpectedMembers.begin(); itor!=mNewExpectedMembers.end(); ++itor)
	{
		msg.append(*itor);
		msg.append("\n");
		mExpectedMembers.insert(*itor);
	}
	mNewExpectedMembers.clear();
	LLSD args;
	args["CONTROLS"] = msg;
	LLNotificationsUtil::add("FloaterNotFound", args);
}

void LLPanel::storeRectControl()
{
	if( !mRectControl.empty() )
	{
		LLUI::sConfigGroup->setRect( mRectControl, getRect() );
	}
}


