/* @file lffloaterinvpanel.cpp
 * @brief Simple floater displaying an inventory panel with any category as its root
 *
 * Copyright (C) 2013 Liru Færs
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#include "llviewerprecompiledheaders.h"

#include "lffloaterinvpanel.h"

#include <boost/algorithm/string/erase.hpp>

#include "llinventorypanel.h"
#include "lluictrlfactory.h"


LFFloaterInvPanel::LFFloaterInvPanel(const LLSD& cat, const std::string& name, LLInventoryModel* model)
: LLInstanceTracker<LFFloaterInvPanel, LLSD>(cat)
{
	// Setup the floater first
	auto mPanel = new LLInventoryPanel("inv_panel", LLInventoryPanel::DEFAULT_SORT_ORDER, cat, LLRect(), model ? model : &gInventory, true);

	// Load from XUI
	mCommitCallbackRegistrar.add("InvPanel.Search", boost::bind(&LLInventoryPanel::setFilterSubString, mPanel, _2));
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inv_panel.xml");

	// Now set the title
	const auto& title = name.empty() ? gInventory.getCategory(mPanel->getRootFolderID())->getName() : name;
	setTitle(title);

	// Figure out a unique name for our rect control
	const auto rect_control = llformat("FloaterInv%sRect", boost::algorithm::erase_all_copy(title, " ").data());
	if (!gSavedSettings.controlExists(rect_control)) // If we haven't existed before, create it
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = getRect();
		rect.translate(left - rect.mLeft, top - rect.mTop);
		gSavedSettings.declareRect(rect_control, rect, "Rectangle for " + title + " window");
	}
	setRectControl(rect_control);
	applyRectControl(); // Set our initial rect to the stored (or just created) control

	// Now take care of the children
	LLPanel* panel = getChild<LLPanel>("placeholder_panel");
	mPanel->setRect(panel->getRect());
	mPanel->setOrigin(0, 0);
	mPanel->postBuild();
	mPanel->setFollows(FOLLOWS_ALL);
	mPanel->setEnabled(true);
	mPanel->removeBorder();
	panel->addChild(mPanel);
}

// static
void LFFloaterInvPanel::show(const LLSD& cat, const std::string& name, LLInventoryModel* model)
{
	LFFloaterInvPanel* floater = LFFloaterInvPanel::getInstance(cat);
	if (!floater) floater = new LFFloaterInvPanel(cat, name, model);
	floater->open();
}

// static
void LFFloaterInvPanel::closeAll()
{
	// We must make a copy first, because LLInstanceTracker doesn't allow destruction while having iterators to it.
	std::vector<LFFloaterInvPanel*> cache;
	for (instance_iter i = beginInstances(), end(endInstances()); i != end; ++i)
	{
		cache.push_back(&*i);
	}
	// Now close all, without using instance_iter iterators.
	for (auto& floater : cache)
	{
		floater->close();
	}
}

BOOL LFFloaterInvPanel::handleKeyHere(KEY key, MASK mask)
{
	if (mask == MASK_NONE && (key == KEY_RETURN || key == KEY_DOWN))
	{
		auto& mPanel = *getChild<LLInventoryPanel>("inv_panel");
		if (!mPanel.hasFocus())
		{
			mPanel.setFocus(true);
			if (LLFolderView* root = mPanel.getRootFolder())
				root->scrollToShowSelection();
			return true;
		}
	}

	return LLFloater::handleKeyHere(key, mask);
}
