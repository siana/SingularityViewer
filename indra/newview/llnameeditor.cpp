/** 
 * @file llnameeditor.cpp
 * @brief Name Editor to refresh a name.
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
 
#include "llnameeditor.h"

#include "llmenugl.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

static LLRegisterWidget<LLNameEditor> r("name_editor");

LLNameEditor::LLNameEditor(const std::string& name, const LLRect& rect,
		const LLUUID& name_id,
		const Type& type,
		const std::string& loading,
		bool rlv_sensitive,
		const std::string& name_system,
		bool click_for_profile,
		const LLFontGL* glfont,
		S32 max_text_length)
: LLNameUI(loading, rlv_sensitive, name_id, type, name_system, click_for_profile)
, LLLineEditor(name, rect, LLStringUtil::null, glfont, max_text_length)
{
	if (!name_id.isNull())
	{
		setNameID(name_id, type);
	}
	else setText(mInitialValue);
}

// virtual
BOOL LLNameEditor::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mClickForProfile && mAllowInteract)
	{
		showProfile();
		return true;
	}
	else return LLLineEditor::handleMouseDown(x, y, mask);
}

// virtual
BOOL LLNameEditor::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	bool simple_menu = mContextMenuHandle.get()->getName() == "rclickmenu";
	std::string new_menu;
	bool needs_simple = !mAllowInteract || mNameID.isNull(); // Need simple if no ID or blocking interaction
	if (!simple_menu && needs_simple) // Switch to simple menu
	{
		new_menu = "menu_texteditor.xml";
	}
	else // TODO: This is lazy, but I cannot recall a name editor that switches between group and avatar, so logic is not needed yet.
	{
		new_menu = mType == GROUP ? "menu_nameeditor_group.xml" : "menu_nameeditor_avatar.xml";
	}
	if (!new_menu.empty()) setContextMenu(LLUICtrlFactory::instance().buildMenu(new_menu, LLMenuGL::sMenuContainer));
	setActive();

	return LLLineEditor::handleRightMouseDown(x, y, mask);
}

// virtual
BOOL LLNameEditor::handleHover(S32 x, S32 y, MASK mask)
{
	auto handled = LLLineEditor::handleHover(x, y, mask);
	if (mAllowInteract && mClickForProfile && !mIsSelecting)
	{
		getWindow()->setCursor(UI_CURSOR_HAND);
		handled = true;
	}
	return handled;
}

void LLNameEditor::displayAsLink(bool link)
{
	static const LLUICachedControl<LLColor4> color("HTMLAgentColor");
	setReadOnlyFgColor(link ? color : LLUI::sColorsGroup->getColor("TextFgReadOnlyColor"));
}

void LLNameEditor::setText(const std::string& text)
{
	setToolTip(text);
	LLLineEditor::setText(text);
}

// virtual
LLXMLNodePtr LLNameEditor::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLLineEditor::getXML();

	node->setName(LL_NAME_EDITOR_TAG);
	node->createChild("label", TRUE)->setStringValue(mInitialValue);
	node->createChild("rlv_sensitive", TRUE)->setBoolValue(mRLVSensitive);
	node->createChild("click_for_profile", TRUE)->setBoolValue(mClickForProfile);
	node->createChild("name_system", TRUE)->setStringValue(mNameSystem);

	return node;
}

LLView* LLNameEditor::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLRect rect;
	createRect(node, rect, parent, LLRect());

	S32 max_text_length = 1024;
	node->getAttributeS32("max_length", max_text_length);
	S8 type = AVATAR;
	node->getAttributeS8("id_type", type);
	LLUUID id;
	node->getAttributeUUID("id", id);
	std::string loading;
	node->getAttributeString("label", loading);
	bool rlv_sensitive = false;
	node->getAttribute_bool("rlv_sensitive", rlv_sensitive);
	bool click_for_profile = true;
	node->getAttribute_bool("click_for_profile", click_for_profile);
	std::string name_system;
	node->getAttributeString("name_system", name_system);

	LLNameEditor* line_editor = new LLNameEditor("name_editor",
								rect,
								id, (Type)type, loading, rlv_sensitive, name_system,
								click_for_profile,
								LLView::selectFont(node),
								max_text_length);

	line_editor->setColorParameters(node);
	line_editor->initFromXML(node, parent);
	
	return line_editor;
}
