/* Copyright (C) 2019 Liru Færs
 *
 * LFIDBearer is a class that holds an ID or IDs that menus can use
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

#include "linden_common.h"
#include "lfidbearer.h"
#include "llmenugl.h"
#include "lluictrlfactory.h"

const std::array<const std::string, LFIDBearer::COUNT> LFIDBearer::sMenuStrings
{
	"menu_avs_list.xml" // 0
,	"menu_groups_list.xml" // 1
,	"menu_objects_list.xml" // 2
,	"menu_experiences.xml" // 3
};
std::array<LLMenuGL*, LFIDBearer::COUNT> LFIDBearer::sMenus {};

const LFIDBearer* LFIDBearer::sActive = nullptr;
LFIDBearer::Type LFIDBearer::sActiveType = LFIDBearer::AVATAR;
uuid_vec_t LFIDBearer::sActiveIDs {};

void LFIDBearer::buildMenus()
{
	auto& factory = LLUICtrlFactory::instance();
	for (auto i = 0; i < COUNT; ++i)
		sMenus[i] = factory.buildMenu(sMenuStrings[i], LLMenuGL::sMenuContainer);
}

LLMenuGL* LFIDBearer::showMenu(LLView* self, const std::string& menu_name, S32 x, S32 y, std::function<void(LLMenuGL*)> on_menu_built)
{
	auto menu = LLUICtrlFactory::instance().buildMenu(menu_name, LLMenuGL::sMenuContainer);
	if (on_menu_built) on_menu_built(menu);
	showMenu(self, menu, x, y);
	return menu;
}

void LFIDBearer::showMenu(LLView* self, LLMenuGL* menu, S32 x, S32 y)
{
	setActive(); // Menu listeners rely on this
	menu->buildDrawLabels();
	menu->updateParent(LLMenuGL::sMenuContainer);
	LLMenuGL::showPopup(self, menu, x, y);
}
