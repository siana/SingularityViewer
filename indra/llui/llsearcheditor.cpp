/** 
 * @file llsearcheditor.cpp
 * @brief LLSearchEditor implementation
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
 
#include "llsearcheditor.h"

static LLRegisterWidget<LLSearchEditor> r2("search_editor");


LLSearchEditor::LLSearchEditor(const std::string& name, 
		const LLRect& rect,
		S32 max_length_bytes) 
	: 
		LLUICtrl(name, rect),
		mSearchEditor(NULL),
		mClearButton(NULL)
{
	mSearchEditor = new LLLineEditor(std::string("filter edit box"),
								   getLocalRect(),
								   LLStringUtil::null,
								   NULL,
								   max_length_bytes,
								   boost::bind(&LLUICtrl::onCommit, this),
								   boost::bind(&LLSearchEditor::handleKeystroke, this));

	mSearchEditor->setFollowsAll();
	mSearchEditor->setSelectAllonFocusReceived(TRUE);
	mSearchEditor->setRevertOnEsc( FALSE );
	mSearchEditor->setPassDelete(TRUE);

	addChild(mSearchEditor);

	S32 btn_width = rect.getHeight(); // button is square, and as tall as search editor
	LLRect clear_btn_rect(rect.getWidth() - btn_width, rect.getHeight(), rect.getWidth(), 0);
	mClearButton = new LLButton(std::string("clear button"), 
								clear_btn_rect, 
								std::string("icn_clear_lineeditor.tga"),
								std::string("UIImgBtnCloseInactiveUUID"),
								LLStringUtil::null,
								boost::bind(&LLSearchEditor::onClearButtonClick, this, _2));
	mClearButton->setFollowsRight();
	mClearButton->setFollowsTop();
	mClearButton->setImageColor(LLUI::sColorsGroup->getColor("TextFgTentativeColor"));
	mClearButton->setTabStop(FALSE);
	mSearchEditor->addChild(mClearButton);

	mSearchEditor->setTextPadding(0, btn_width);
}

//virtual
void LLSearchEditor::draw()
{
	if (mClearButton)
		mClearButton->setVisible(!mSearchEditor->getWText().empty());

	LLUICtrl::draw();
}

//virtual
void LLSearchEditor::setValue(const LLSD& value )
{
	mSearchEditor->setValue(value);
}

//virtual
LLSD LLSearchEditor::getValue() const
{
	return mSearchEditor->getValue();
}

//virtual
BOOL LLSearchEditor::setTextArg( const std::string& key, const LLStringExplicit& text )
{
	return mSearchEditor->setTextArg(key, text);
}

//virtual
BOOL LLSearchEditor::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	return mSearchEditor->setLabelArg(key, text);
}

//virtual
void LLSearchEditor::setLabel( const LLStringExplicit &new_label )
{
	mSearchEditor->setLabel(new_label);
}

//virtual
void LLSearchEditor::clear()
{
	if (mSearchEditor)
	{
		mSearchEditor->clear();
	}
}

//virtual
void LLSearchEditor::setFocus( BOOL b )
{
	if (mSearchEditor)
	{
		mSearchEditor->setFocus(b);
	}
}

void LLSearchEditor::onClearButtonClick(const LLSD& data)
{
	setText(LLStringUtil::null);
	mSearchEditor->onCommit(); // force keystroke callback
}

void LLSearchEditor::handleKeystroke()
{
	if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, getValue());
	}
}

// static
LLView* LLSearchEditor::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLRect rect;
	createRect(node, rect, parent, LLRect());

	S32 max_text_length = 128;
	node->getAttributeS32("max_length", max_text_length);

	//std::string text = node->getValue().substr(0, max_text_length - 1);

	LLSearchEditor* search_editor = new LLSearchEditor("search_editor",
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
