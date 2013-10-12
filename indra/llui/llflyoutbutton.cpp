/**
 * @file llflyoutbutton.cpp
 * @brief LLFlyoutButton base class
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

#include "linden_common.h"

// file includes
#include "llflyoutbutton.h"

static LLRegisterWidget<LLFlyoutButton> r2("flyout_button");

const S32 FLYOUT_BUTTON_ARROW_WIDTH = 24;

LLFlyoutButton::LLFlyoutButton(const std::string& name, const LLRect& rect, const std::string& label)
:	LLComboBox(name, rect, LLStringUtil::null),
	mToggleState(FALSE),
	mActionButton(NULL)
{
	// Always use text box
	// Text label button
	mActionButton = new LLButton(label, LLRect(), LLStringUtil::null, boost::bind(&LLFlyoutButton::onActionButtonClick, this, _2));
	mActionButton->setScaleImage(true);
	mActionButton->setFollowsAll();
	mActionButton->setHAlign(LLFontGL::HCENTER);
	mActionButton->setLabel(label);
	addChild(mActionButton);

	mActionButton->setImageSelected(LLUI::getUIImage("flyout_btn_left_selected.tga"));
	mActionButton->setImageUnselected(LLUI::getUIImage("flyout_btn_left.tga"));
	mActionButton->setImageDisabled(LLUI::getUIImage("flyout_btn_left_disabled.tga"));
	mActionButton->setImageDisabledSelected(LLPointer<LLUIImage>(NULL));

	mButton->setImageSelected(LLUI::getUIImage("flyout_btn_right_selected.tga"));
	mButton->setImageUnselected(LLUI::getUIImage("flyout_btn_right.tga"));
	mButton->setImageDisabled(LLUI::getUIImage("flyout_btn_right_disabled.tga"));
	mButton->setImageDisabledSelected(LLPointer<LLUIImage>(NULL));
	mButton->setRightHPad(6);

	updateLayout();
}

// virtual
LLXMLNodePtr LLFlyoutButton::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLComboBox::getXML();
	node->setName(LL_FLYOUT_BUTTON_TAG);

	for (LLXMLNodePtr child = node->getFirstChild(); child.notNull();)
	{
		if (child->hasName("combo_item"))
		{
			child->setName(LL_FLYOUT_BUTTON_ITEM_TAG);

			//setName does a delete and add, so we have to start over
			child = node->getFirstChild();
		}
		else
		{
			child = child->getNextSibling();
		}
	}

	return node;
}

//static
LLView* LLFlyoutButton::fromXML(LLXMLNodePtr node, LLView* parent, LLUICtrlFactory* factory)
{
	std::string label("");
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLFlyoutButton* flyout_button = new LLFlyoutButton("flyout_button", rect, label);

	std::string list_position;
	node->getAttributeString("list_position", list_position);
	if (list_position == "below")
	{
		flyout_button->mListPosition = BELOW;
	}
	else if (list_position == "above")
	{
		flyout_button->mListPosition = ABOVE;
	}

	flyout_button->initFromXML(node, parent);

	for (LLXMLNodePtr child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName(LL_FLYOUT_BUTTON_ITEM_TAG))
		{
			std::string label(child->getTextContents());
			child->getAttributeString("label", label);
			std::string value(label);
			child->getAttributeString("value", value);

			flyout_button->add(label, LLSD(value));
		}
	}

	flyout_button->updateLayout();

	return flyout_button;
}

void LLFlyoutButton::updateLayout()
{
	LLComboBox::updateLayout();

	mButton->setOrigin(getRect().getWidth() - FLYOUT_BUTTON_ARROW_WIDTH, 0);
	mButton->reshape(FLYOUT_BUTTON_ARROW_WIDTH, getRect().getHeight());
	mButton->setFollows(FOLLOWS_RIGHT | FOLLOWS_TOP | FOLLOWS_BOTTOM);
	mButton->setTabStop(false);
	mButton->setImageOverlay(mListPosition == BELOW ? "down_arrow.tga" : "up_arrow.tga", LLFontGL::RIGHT);

	mActionButton->setOrigin(0, 0);
	mActionButton->reshape(getRect().getWidth() - FLYOUT_BUTTON_ARROW_WIDTH, getRect().getHeight());
}

void LLFlyoutButton::onActionButtonClick(const LLSD& data)
{
	// remember last list selection?
	mList->deselect();
	onCommit();
}

void LLFlyoutButton::draw()
{
	mActionButton->setToggleState(mToggleState);
	mButton->setToggleState(mToggleState);

	//FIXME: this should be an attribute of comboboxes, whether they have a distinct label or
	// the label reflects the last selected item, for now we have to manually remove the label
	mButton->setLabel(LLStringUtil::null);
	LLComboBox::draw();
}

void LLFlyoutButton::setEnabled(BOOL enabled)
{
	mActionButton->setEnabled(enabled);
	LLComboBox::setEnabled(enabled);
}

void LLFlyoutButton::setToggleState(BOOL state)
{
	mToggleState = state;
}


