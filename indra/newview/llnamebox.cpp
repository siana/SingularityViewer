/** 
 * @file llnamebox.cpp
 * @brief A text display widget
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

#include "llviewerprecompiledheaders.h"

#include "llnamebox.h"
#include "llwindow.h"

static LLRegisterWidget<LLNameBox> r("name_box");

LLNameBox::LLNameBox(const std::string& name,
	const LLUUID& name_id,
	const Type& type,
	const std::string& loading,
	bool rlv_sensitive,
	const std::string& name_system,
	bool click_for_profile)
: LLNameUI(loading, rlv_sensitive, name_id, type, name_system, click_for_profile)
, LLTextBox(name, LLRect(), LLStringUtil::null, nullptr, TRUE)
{
	setClickedCallback(boost::bind(&LLNameUI::showProfile, this));
	if (!name_id.isNull())
	{
		setNameID(name_id, type);
	}
	else setText(mInitialValue);
}

void LLNameBox::displayAsLink(bool link)
{
	static const LLUICachedControl<LLColor4> color("HTMLAgentColor");
	setColor(link ? color : LLUI::sColorsGroup->getColor("LabelTextColor"));
	setDisabledColor(link ? color : LLUI::sColorsGroup->getColor("LabelDisabledColor"));
}

// virtual
BOOL LLNameBox::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	auto handled = LLTextBox::handleRightMouseDown(x, y, mask);
	if (mAllowInteract && !handled)
	{
		if (mNameID.notNull())
		{
			showMenu(this, sMenus[getSelectedType()], x, y);
			handled = true;
		}
	}
	return handled;
}

// virtual
BOOL LLNameBox::handleHover(S32 x, S32 y, MASK mask)
{
	auto handled = LLTextBox::handleHover(x, y, mask);
	if (mClickForProfile && mAllowInteract)
	{
		getWindow()->setCursor(UI_CURSOR_HAND);
		handled = true;
	}
	return handled;
}

// virtual
LLXMLNodePtr LLNameBox::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLTextBox::getXML();

	node->setName("name_box");
	node->createChild("initial_value", TRUE)->setStringValue(mInitialValue);
	node->createChild("rlv_sensitive", TRUE)->setBoolValue(mRLVSensitive);
	node->createChild("click_for_profile", TRUE)->setBoolValue(mClickForProfile);
	node->createChild("name_system", TRUE)->setStringValue(mNameSystem);

	return node;
}

// static
LLView* LLNameBox::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	S8 type = AVATAR;
	node->getAttributeS8("id_type", type);
	LLUUID id;
	node->getAttributeUUID("id", id);
	std::string loading;
	node->getAttributeString("initial_value", loading);
	bool rlv_sensitive = false;
	node->getAttribute_bool("rlv_sensitive", rlv_sensitive);
	std::string name_system;
	node->getAttributeString("name_system", name_system);
	bool click_for_profile = true;
	node->getAttribute_bool("click_for_profile", click_for_profile);
	LLNameBox* name_box = new LLNameBox("name_box", id, (Type)type, loading, rlv_sensitive, name_system, click_for_profile);
	name_box->initFromXML(node,parent);

	return name_box;
}
