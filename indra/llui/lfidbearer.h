/* Copyright (C) 2019 Liru Færs
 *
 * LFIDBearer is a class that holds an ID or IDs that menus can use
 * This class also bears the type of ID/IDs that it is holding
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

#include "lluuid.h"

class LLMenuGL;
class LLView;

struct LFIDBearer
{
	enum Type : S8
	{
		MULTIPLE = -2,
		NONE = -1,
		AVATAR = 0,
		GROUP,
		OBJECT,
		EXPERIENCE,
		COUNT
	};

	virtual ~LFIDBearer() { if (sActive == this) sActive = nullptr; }
	virtual LLUUID getStringUUIDSelectedItem() const = 0;
	virtual uuid_vec_t getSelectedIDs() const { return { getStringUUIDSelectedItem() }; }
	virtual Type getSelectedType() const { return AVATAR; }

	template<typename T> static const T* getActive() { return static_cast<const T*>(sActive); }
	static const LLUUID& getActiveSelectedID() { return sActiveIDs.empty() ? LLUUID::null : sActiveIDs[0]; }
	static const uuid_vec_t& getActiveSelectedIDs() { return sActiveIDs; }
	static size_t getActiveNumSelected() { return sActiveIDs.size(); }
	static const Type& getActiveType() { return sActiveType; }

	void setActive() const
	{
		sActive = this;
		sActiveType = getSelectedType();
		sActiveIDs = getSelectedIDs();
		//sActiveIDs or even some kinda hybrid map, if Type is MULTIPLE fill the vals? and remove a buncha virtual functions?
	}

	static void buildMenus();
	LLMenuGL* showMenu(LLView* self, const std::string& menu_name, S32 x, S32 y, std::function<void(LLMenuGL*)> on_menu_built = nullptr);
	void showMenu(LLView* self, LLMenuGL* menu, S32 x, S32 y);

protected:
	// Menus that recur, such as general avatars or groups menus
	static const std::array<const std::string, COUNT> sMenuStrings;
	static std::array<LLMenuGL*, COUNT> sMenus;

private:
	static const LFIDBearer* sActive;
	static Type sActiveType;
	static uuid_vec_t sActiveIDs;
};
