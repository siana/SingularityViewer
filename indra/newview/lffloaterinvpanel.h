/* @file lffloaterinvpanel.h
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

#ifndef LFFLOATERINVPANEL_H
#define LFFLOATERINVPANEL_H

#include "llfloater.h"
#include "llinstancetracker.h"


class LFFloaterInvPanel : public LLFloater, public LLInstanceTracker<LFFloaterInvPanel, LLUUID>
{
	LFFloaterInvPanel(const LLUUID& cat_id, class LLInventoryModel* model, const std::string& name);
	~LFFloaterInvPanel();

public:
	static void show(const LLUUID& cat_id, LLInventoryModel* model, const std::string& name); // Show the floater for cat_id (create with other params if necessary)
	static void closeAll(); // Called when not allowed to have inventory open

	/*virtual*/ BOOL handleKeyHere(KEY key, MASK mask);
	void onSearch(const LLSD& val);

private:
	class LLInventoryPanel* mPanel;
};

#endif //LFFLOATERINVPANEL_H
