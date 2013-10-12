/** 
 * @file llfiltereditor.cpp
 * @brief LLFilterEditor implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// Text editor widget to let users enter a single line.

#include "linden_common.h"
 
#include "llfiltereditor.h"

static LLRegisterWidget<LLFilterEditor> r2("filter_editor");

LLFilterEditor::LLFilterEditor(const std::string& name, 
		const LLRect& rect,
		S32 max_length_bytes)
:	LLSearchEditor(name,rect,max_length_bytes)
{
	setCommitOnFocusLost(FALSE); // we'll commit on every keystroke, don't re-commit when we take focus away (i.e. we go to interact with the actual results!)
}


void LLFilterEditor::handleKeystroke()
{
	this->LLSearchEditor::handleKeystroke();

	// Commit on every keystroke.
	onCommit();
}

// static
LLView* LLFilterEditor::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLRect rect;
	createRect(node, rect, parent, LLRect());

	S32 max_text_length = 128;
	node->getAttributeS32("max_length", max_text_length);

	//std::string text = node->getValue().substr(0, max_text_length - 1);

	LLFilterEditor* search_editor = new LLFilterEditor("filter_editor",
								rect, 
								max_text_length);

	std::string label;
	if(node->getAttributeString("label", label))
	{
		search_editor->setLabel(label);
	}
	
	//search_editor->setText(text);

	search_editor->initFromXML(node, parent);
	
	return search_editor;
}
