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

#pragma once

#include "llfloater.h"
#include "llinstancetracker.h"
#include "llsdutil.h"


class LFFloaterInvPanel final : public LLFloater, public LLInstanceTracker<LFFloaterInvPanel, LLSD>
{
	LFFloaterInvPanel(const LLSD& cat, const std::string& name = LLStringUtil::null, class LLInventoryModel* model = nullptr);

public:
	static void show(const LLSD& cat, const std::string& name = LLStringUtil::null, LLInventoryModel* model = nullptr); // Show the floater for cat (create with other params if necessary)
	static void toggle(const LLSD& cat)
	{
		if (auto instance = getInstance(cat))
			instance->close();
		else
			show(cat);
	}
	static void closeAll(); // Called when not allowed to have inventory open

	BOOL handleKeyHere(KEY key, MASK mask) override;
};
