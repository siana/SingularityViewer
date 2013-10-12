/** 
 * @file llinventoryicon.cpp
 * @brief Implementation of the inventory icon.
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

#include "llviewerprecompiledheaders.h"
#include "llinventoryicon.h"

#include "lldictionary.h"
#include "llinventorydefines.h"
#include "llui.h"
#include "llwearabletype.h"

struct IconEntry : public LLDictionaryEntry
{
	IconEntry(const std::string &item_name)
		:
		LLDictionaryEntry(item_name)
	{}
};

class LLIconDictionary : public LLSingleton<LLIconDictionary>,
						 public LLDictionary<LLInventoryType::EIconName, IconEntry>
{
public:
	LLIconDictionary();
};

LLIconDictionary::LLIconDictionary()
{
	addEntry(LLInventoryType::ICONNAME_TEXTURE, 				new IconEntry("inv_item_texture.tga"));
	addEntry(LLInventoryType::ICONNAME_SOUND, 					new IconEntry("inv_item_sound.tga"));
	addEntry(LLInventoryType::ICONNAME_CALLINGCARD_ONLINE, 		new IconEntry("inv_item_callingcard_online.tga"));
	addEntry(LLInventoryType::ICONNAME_CALLINGCARD_OFFLINE, 	new IconEntry("inv_item_callingcard_offline.tga"));
	addEntry(LLInventoryType::ICONNAME_LANDMARK, 				new IconEntry("inv_item_landmark.tga"));
	addEntry(LLInventoryType::ICONNAME_LANDMARK_VISITED, 		new IconEntry("inv_item_landmark_visited.tga"));
	addEntry(LLInventoryType::ICONNAME_SCRIPT, 					new IconEntry("inv_item_script.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING, 				new IconEntry("inv_item_clothing.tga"));
	addEntry(LLInventoryType::ICONNAME_OBJECT, 					new IconEntry("inv_item_object.tga"));
	addEntry(LLInventoryType::ICONNAME_OBJECT_MULTI, 			new IconEntry("inv_item_object_multi.tga"));
	addEntry(LLInventoryType::ICONNAME_NOTECARD, 				new IconEntry("inv_item_notecard.tga"));
	addEntry(LLInventoryType::ICONNAME_BODYPART, 				new IconEntry("inv_item_skin.tga"));
	addEntry(LLInventoryType::ICONNAME_SNAPSHOT, 				new IconEntry("inv_item_snapshot.tga"));

	addEntry(LLInventoryType::ICONNAME_BODYPART_SHAPE, 			new IconEntry("inv_item_shape.tga"));
	addEntry(LLInventoryType::ICONNAME_BODYPART_SKIN, 			new IconEntry("inv_item_skin.tga"));
	addEntry(LLInventoryType::ICONNAME_BODYPART_HAIR, 			new IconEntry("inv_item_hair.tga"));
	addEntry(LLInventoryType::ICONNAME_BODYPART_EYES, 			new IconEntry("inv_item_eyes.tga"));

	addEntry(LLInventoryType::ICONNAME_CLOTHING_SHIRT, 			new IconEntry("inv_item_shirt.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_PANTS, 			new IconEntry("inv_item_pants.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_SHOES, 			new IconEntry("inv_item_shoes.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_SOCKS, 			new IconEntry("inv_item_socks.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_JACKET, 		new IconEntry("inv_item_jacket.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_GLOVES, 		new IconEntry("inv_item_gloves.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_UNDERSHIRT, 	new IconEntry("inv_item_undershirt.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_UNDERPANTS, 	new IconEntry("inv_item_underpants.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_SKIRT, 			new IconEntry("inv_item_skirt.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_ALPHA, 			new IconEntry("inv_item_alpha.tga"));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_TATTOO, 		new IconEntry("inv_item_tattoo.tga"));
	addEntry(LLInventoryType::ICONNAME_ANIMATION, 				new IconEntry("inv_item_animation.tga"));
	addEntry(LLInventoryType::ICONNAME_GESTURE, 				new IconEntry("inv_item_gesture.tga"));

        addEntry(LLInventoryType::ICONNAME_CLOTHING_PHYSICS, 		new IconEntry("inv_item_physics.tga"));

	addEntry(LLInventoryType::ICONNAME_LINKITEM, 				new IconEntry("inv_link_item.tga"));
	addEntry(LLInventoryType::ICONNAME_LINKFOLDER, 				new IconEntry("inv_link_folder.tga"));
	addEntry(LLInventoryType::ICONNAME_MESH,	 				new IconEntry("inv_item_mesh.tga"));

	addEntry(LLInventoryType::ICONNAME_CLOTHING_UNKNOWN, 		new IconEntry("inv_item_unknown.tga"));
	addEntry(LLInventoryType::ICONNAME_INVALID, 				new IconEntry("inv_invalid.png"));

	addEntry(LLInventoryType::ICONNAME_NONE, 					new IconEntry("NONE"));
}

LLUIImagePtr LLInventoryIcon::getIcon(LLAssetType::EType asset_type,
									  LLInventoryType::EType inventory_type,
									  U32 misc_flag,
									  BOOL item_is_multi)
{
	const std::string& icon_name = getIconName(asset_type, inventory_type, misc_flag, item_is_multi);
	return LLUI::getUIImage(icon_name);
}

LLUIImagePtr LLInventoryIcon::getIcon(LLInventoryType::EIconName idx)
{
	return LLUI::getUIImage(getIconName(idx));
}

const std::string& LLInventoryIcon::getIconName(LLAssetType::EType asset_type,
												LLInventoryType::EType inventory_type,
												U32 misc_flag,
												BOOL item_is_multi)
{
	LLInventoryType::EIconName idx = LLInventoryType::ICONNAME_OBJECT;
	if (item_is_multi)
	{
		idx = LLInventoryType::ICONNAME_OBJECT_MULTI;
		return getIconName(idx);
	}
	
	switch(asset_type)
	{
		case LLAssetType::AT_TEXTURE:
			idx = (inventory_type == LLInventoryType::IT_SNAPSHOT) ? LLInventoryType::ICONNAME_SNAPSHOT : LLInventoryType::ICONNAME_TEXTURE;
			break;
		case LLAssetType::AT_SOUND:
			idx = LLInventoryType::ICONNAME_SOUND;
			break;
		case LLAssetType::AT_CALLINGCARD:
			idx = (misc_flag != 0) ? LLInventoryType::ICONNAME_CALLINGCARD_ONLINE : LLInventoryType::ICONNAME_CALLINGCARD_OFFLINE;
			break;
		case LLAssetType::AT_LANDMARK:
			idx = (misc_flag != 0) ? LLInventoryType::ICONNAME_LANDMARK_VISITED : LLInventoryType::ICONNAME_LANDMARK;
			break;
		case LLAssetType::AT_SCRIPT:
		case LLAssetType::AT_LSL_TEXT:
		case LLAssetType::AT_LSL_BYTECODE:
			idx = LLInventoryType::ICONNAME_SCRIPT;
			break;
		case LLAssetType::AT_CLOTHING:
		case LLAssetType::AT_BODYPART:
			idx = assignWearableIcon(misc_flag);
			break;
		case LLAssetType::AT_NOTECARD:
			idx = LLInventoryType::ICONNAME_NOTECARD;
			break;
		case LLAssetType::AT_ANIMATION:
			idx = LLInventoryType::ICONNAME_ANIMATION;
			break;
		case LLAssetType::AT_GESTURE:
			idx = LLInventoryType::ICONNAME_GESTURE;
			break;
		case LLAssetType::AT_LINK:
			idx = LLInventoryType::ICONNAME_LINKITEM;
			break;
		case LLAssetType::AT_LINK_FOLDER:
			idx = LLInventoryType::ICONNAME_LINKFOLDER;
			break;
		case LLAssetType::AT_OBJECT:
			idx = LLInventoryType::ICONNAME_OBJECT;
			break;
		case LLAssetType::AT_MESH:
			idx = LLInventoryType::ICONNAME_MESH;
		default:
			break;
	}
	
	return getIconName(idx);
}


const std::string& LLInventoryIcon::getIconName(LLInventoryType::EIconName idx)
{
	const IconEntry *entry = LLIconDictionary::instance().lookup(idx);
	return entry->mName;
}

LLInventoryType::EIconName LLInventoryIcon::assignWearableIcon(U32 misc_flag)
{
	const LLWearableType::EType wearable_type = LLWearableType::EType(LLInventoryItemFlags::II_FLAGS_WEARABLES_MASK & misc_flag);
	return LLWearableType::getIconName(wearable_type);
}
