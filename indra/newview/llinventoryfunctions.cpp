/** 
 * @file llinventoryfunctions.cpp
 * @brief Implementation of the inventory view and associated stuff.
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

#include <utility> // for std::pair<>

#include "llinventoryfunctions.h"

// library includes
#include "llagent.h"
#include "llagentwearables.h"
#include "llcallingcard.h"
#include "llinventorydefines.h"
#include "llsdserialize.h"
#include "llspinctrl.h"
#include "lltrans.h"
#include "llui.h"
#include "message.h"

// newview includes
#include "llappearancemgr.h"
#include "llappviewer.h"
//#include "llfirstuse.h"
#include "llfloaterinventory.h"
#include "llfloatermarketplacelistings.h"
#include "llfocusmgr.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "lliconctrl.h"
#include "llimview.h"
#include "llinventorybridge.h"
#include "llinventoryclipboard.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "lllineeditor.h"
#include "llmarketplacenotifications.h"
#include "llmarketplacefunctions.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llpanelmaininventory.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llresmgr.h"
#include "llscrollbar.h"
#include "llscrollcontainer.h"
#include "llselectmgr.h"
#include "lltabcontainer.h"
#include "lltooldraganddrop.h"
#include "lluictrlfactory.h"
#include "llviewermessage.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]

#include <boost/foreach.hpp>

BOOL LLInventoryState::sWearNewClothing = FALSE;
LLUUID LLInventoryState::sWearNewClothingTransactionID;

// Helper function : callback to update a folder after inventory action happened in the background
void update_folder_cb(const LLUUID& dest_folder)
{
	LLViewerInventoryCategory* dest_cat = gInventory.getCategory(dest_folder);
	gInventory.updateCategory(dest_cat);
	gInventory.notifyObservers();
}

// Helper function : Count only the copyable items, i.e. skip the stock items (which are no copy)
S32 count_copyable_items(LLInventoryModel::item_array_t& items)
{
	S32 count = 0;
	for (LLInventoryModel::item_array_t::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		LLViewerInventoryItem* item = *it;
		if (item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			count++;
		}
	}
	return count;
}

// Helper function : Count only the non-copyable items, i.e. the stock items, skip the others
S32 count_stock_items(LLInventoryModel::item_array_t& items)
{
	S32 count = 0;
	for (LLInventoryModel::item_array_t::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		LLViewerInventoryItem* item = *it;
		if (!item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			count++;
		}
	}
	return count;
}

// Helper function : Count the number of stock folders
S32 count_stock_folders(LLInventoryModel::cat_array_t& categories)
{
	S32 count = 0;
	for (LLInventoryModel::cat_array_t::const_iterator it = categories.begin(); it != categories.end(); ++it)
	{
		LLInventoryCategory* cat = *it;
		if (cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			count++;
		}
	}
	return count;
}

// Helper funtion : Count the number of items (not folders) in the descending hierarchy
S32 count_descendants_items(const LLUUID& cat_id)
{
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_id,cat_array,item_array);

	S32 count = item_array->size();

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLViewerInventoryCategory* category = *iter;
		count += count_descendants_items(category->getUUID());
	}

	return count;
}

// Helper function : Returns true if the hierarchy contains nocopy items
bool contains_nocopy_items(const LLUUID& id)
{
	LLInventoryCategory* cat = gInventory.getCategory(id);

	if (cat)
	{
		// Get the content
		LLInventoryModel::cat_array_t* cat_array;
		LLInventoryModel::item_array_t* item_array;
		gInventory.getDirectDescendentsOf(id,cat_array,item_array);

		// Check all the items: returns true upon encountering a nocopy item
		for (LLInventoryModel::item_array_t::iterator iter = item_array->begin(); iter != item_array->end(); iter++)
		{
			LLInventoryItem* item = *iter;
			LLViewerInventoryItem * inv_item = (LLViewerInventoryItem *) item;
			if (!inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
			{
				return true;
			}
		}

		// Check all the sub folders recursively
		for (LLInventoryModel::cat_array_t::iterator iter = cat_array->begin(); iter != cat_array->end(); iter++)
		{
			LLViewerInventoryCategory* cat = *iter;
			if (contains_nocopy_items(cat->getUUID()))
			{
				return true;
			}
		}
	}
	else
	{
		LLInventoryItem* item = gInventory.getItem(id);
		LLViewerInventoryItem * inv_item = (LLViewerInventoryItem *) item;
		if (!inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			return true;
		}
	}

	// Exit without meeting a nocopy item
	return false;
}

// Generates a string containing the path to the item specified by
// item_id.
void append_path(const LLUUID& id, std::string& path)
{
	std::string temp;
	const LLInventoryObject* obj = gInventory.getObject(id);
	LLUUID parent_id;
	if(obj) parent_id = obj->getParentUUID();
	std::string forward_slash("/");
	while(obj)
	{
		obj = gInventory.getCategory(parent_id);
		if(obj)
		{
			temp.assign(forward_slash + obj->getName() + temp);
			parent_id = obj->getParentUUID();
		}
	}
	path.append(temp);
}

void update_marketplace_folder_hierarchy(const LLUUID cat_id)
{
	// When changing the marketplace status of a folder, the only thing that needs to happen is
	// for all observers of the folder to, possibly, change the display label of the folder
	// so that's the only thing we change on the update mask.
	gInventory.addChangedMask(LLInventoryObserver::LABEL, cat_id);

	// Update all descendent folders down
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_id,cat_array,item_array);

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLInventoryCategory* category = *iter;
		update_marketplace_folder_hierarchy(category->getUUID());
	}
    return;
}

void update_marketplace_category(const LLUUID& cur_uuid, bool perform_consistency_enforcement)
{
	// When changing the marketplace status of an item, we usually have to change the status of all
	// folders in the same listing. This is because the display of each folder is affected by the
	// overall status of the whole listing.
	// Consequently, the only way to correctly update an item anywhere in the marketplace is to
	// update the whole listing from its listing root.
	// This is not as bad as it seems as we only update folders, not items, and the folder nesting depth
	// is limited to 4.
	// We also take care of degenerated cases so we don't update all folders in the inventory by mistake.

    if (cur_uuid.isNull()
        || gInventory.getCategory(cur_uuid) == NULL
        || gInventory.getCategory(cur_uuid)->getVersion() == LLViewerInventoryCategory::VERSION_UNKNOWN)
	{
		return;
	}

	// Grab marketplace listing data for this item
	S32 depth = depth_nesting_in_marketplace(cur_uuid);
	if (depth > 0)
	{
		// Retrieve the listing uuid this object is in
		LLUUID listing_uuid = nested_parent_id(cur_uuid, depth);
        LLViewerInventoryCategory* listing_cat = gInventory.getCategory(listing_uuid);
        bool listing_cat_loaded = listing_cat != NULL && listing_cat->getVersion() != LLViewerInventoryCategory::VERSION_UNKNOWN;

		// Verify marketplace data consistency for this listing
        if (perform_consistency_enforcement
            && listing_cat_loaded
            && LLMarketplaceData::instance().isListed(listing_uuid))
		{
			LLUUID version_folder_uuid = LLMarketplaceData::instance().getVersionFolder(listing_uuid);
			S32 version_depth = depth_nesting_in_marketplace(version_folder_uuid);
			if (version_folder_uuid.notNull() && (!gInventory.isObjectDescendentOf(version_folder_uuid, listing_uuid) || (version_depth != 2)))
			{
				LL_INFOS("SLM") << "Unlist and clear version folder as the version folder is not at the right place anymore!!" << LL_ENDL;
				LLMarketplaceData::instance().setVersionFolder(listing_uuid, LLUUID::null,1);
			}
            else if (version_folder_uuid.notNull()
                     && gInventory.isCategoryComplete(version_folder_uuid)
                     && LLMarketplaceData::instance().getActivationState(version_folder_uuid)
                     && (count_descendants_items(version_folder_uuid) == 0)
                     && !LLMarketplaceData::instance().isUpdating(version_folder_uuid,version_depth))
			{
				LL_INFOS("SLM") << "Unlist as the version folder is empty of any item!!" << LL_ENDL;
				LLNotificationsUtil::add("AlertMerchantVersionFolderEmpty");
				LLMarketplaceData::instance().activateListing(listing_uuid, false,1);
			}
		}

		// Check if the count on hand needs to be updated on SLM
        if (perform_consistency_enforcement
            && listing_cat_loaded
            && (compute_stock_count(listing_uuid) != LLMarketplaceData::instance().getCountOnHand(listing_uuid)))
		{
			LLMarketplaceData::instance().updateCountOnHand(listing_uuid,1);
		}
		// Update all descendents starting from the listing root
		update_marketplace_folder_hierarchy(listing_uuid);
	}
	else if (depth == 0)
	{
		// If this is the marketplace listings root itself, update all descendents
		if (gInventory.getCategory(cur_uuid))
		{
			update_marketplace_folder_hierarchy(cur_uuid);
		}
	}
	else
	{
		// If the folder is outside the marketplace listings root, clear its SLM data if needs be
		if (perform_consistency_enforcement && LLMarketplaceData::instance().isListed(cur_uuid))
		{
			LL_INFOS("SLM") << "Disassociate as the listing folder is not under the marketplace folder anymore!!" << LL_ENDL;
			LLMarketplaceData::instance().clearListing(cur_uuid);
		}
		// Update all descendents if this is a category
		if (gInventory.getCategory(cur_uuid))
		{
			update_marketplace_folder_hierarchy(cur_uuid);
		}
	}

    return;
}

// Iterate through the marketplace and flag for label change all categories that countain a stock folder (i.e. stock folders and embedding folders up the hierarchy)
void update_all_marketplace_count(const LLUUID& cat_id)
{
	// Get all descendent folders down
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_id, cat_array, item_array);

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLInventoryCategory* category = *iter;
		if (category->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			// Listing containing stock folders needs to be updated but not others
			// Note: we take advantage of the fact that stock folder *do not* contain sub folders to avoid a recursive call here
			update_marketplace_category(category->getUUID());
		}
		else
		{
			// Explore the contained folders recursively
			update_all_marketplace_count(category->getUUID());
		}
	}
}

void update_all_marketplace_count()
{
	// Get the marketplace root and launch the recursive exploration
	const LLUUID marketplace_listings_uuid = gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS, false);
	if (!marketplace_listings_uuid.isNull())
	{
		update_all_marketplace_count(marketplace_listings_uuid);
	}
	return;
}

void rename_category(LLInventoryModel* model, const LLUUID& cat_id, const std::string& new_name)
{
	LLViewerInventoryCategory* cat;

	if (!model ||
		!get_is_category_renameable(model, cat_id) ||
		(cat = model->getCategory(cat_id)) == NULL ||
		cat->getName() == new_name)
	{
		return;
	}

	LLSD updates;
	updates["name"] = new_name;
	update_inventory_category(cat_id, updates, NULL);
}

void copy_inventory_category(LLInventoryModel* model,
							 LLViewerInventoryCategory* cat,
							 const LLUUID& parent_id,
							 const LLUUID& root_copy_id,
							 bool move_no_copy_items)
{
	// Create the initial folder
	LLUUID new_cat_uuid = gInventory.createNewCategory(parent_id, LLFolderType::FT_NONE, cat->getName());
	model->notifyObservers();

	// We need to exclude the initial root of the copy to avoid recursively copying the copy, etc...
	LLUUID root_id = (root_copy_id.isNull() ? new_cat_uuid : root_copy_id);

	// Get the content of the folder
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(),cat_array,item_array);

	// If root_copy_id is null, tell the marketplace model we'll be waiting for new items to be copied over for this folder
	if (root_copy_id.isNull())
	{
		LLMarketplaceData::instance().setValidationWaiting(root_id,count_descendants_items(cat->getUUID()));		
	}

	// Copy all the items
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin(); iter != item_array_copy.end(); iter++)
	{
		LLInventoryItem* item = *iter;
		LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(update_folder_cb, new_cat_uuid));

		if (!item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			// If the item is nocopy, we do nothing or, optionally, move it
			if (move_no_copy_items)
			{
				// Reparent the item
				LLViewerInventoryItem * viewer_inv_item = (LLViewerInventoryItem *) item;
				gInventory.changeItemParent(viewer_inv_item, new_cat_uuid, true);
			}
			// Decrement the count in root_id since that one item won't be copied over
			LLMarketplaceData::instance().decrementValidationWaiting(root_id);
		}
		else
		{
			copy_inventory_item(
						gAgent.getID(),
						item->getPermissions().getOwner(),
						item->getUUID(),
						new_cat_uuid,
						std::string(),
						cb);
		}
	}

	// Copy all the folders
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLViewerInventoryCategory* category = *iter;
		if (category->getUUID() != root_id)
		{
			copy_inventory_category(model, category, new_cat_uuid, root_id, move_no_copy_items);
		}
	}
}

class LLInventoryCollectAllItems : public LLInventoryCollectFunctor
{
public:
	virtual bool operator()(LLInventoryCategory* cat, LLInventoryItem* item)
	{
		return true;
	}
};

BOOL get_is_parent_to_worn_item(const LLUUID& id)
{
	const LLViewerInventoryCategory* cat = gInventory.getCategory(id);
	if (!cat)
	{
		return FALSE;
	}

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLInventoryCollectAllItems collect_all;
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), cats, items, LLInventoryModel::EXCLUDE_TRASH, collect_all);

	for (LLInventoryModel::item_array_t::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		const LLViewerInventoryItem * const item = *it;

		llassert(item->getIsLinkType());

		LLUUID linked_id = item->getLinkedUUID();
		const LLViewerInventoryItem * const linked_item = gInventory.getItem(linked_id);

		if (linked_item)
		{
			LLUUID parent_id = linked_item->getParentUUID();

			while (!parent_id.isNull())
			{
				LLInventoryCategory * parent_cat = gInventory.getCategory(parent_id);

				if (cat == parent_cat)
				{
					return TRUE;
				}

				parent_id = parent_cat->getParentUUID();
			}
		}
	}

	return FALSE;
}

BOOL get_is_item_worn(const LLUUID& id)
{
	const LLViewerInventoryItem* item = gInventory.getItem(id);
	if (!item)
		return FALSE;

	// Consider the item as worn if it has links in COF.
// [SL:KB] - The code below causes problems across the board so it really just needs to go
//	if (LLAppearanceMgr::instance().isLinkedInCOF(id))
//	{
//		return TRUE;
//	}

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(item->getLinkedUUID()))
				return TRUE;
			break;
		}
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if(gAgentWearables.isWearingItem(item->getLinkedUUID()))
				return TRUE;
			break;
		case LLAssetType::AT_GESTURE:
			if (LLGestureMgr::instance().isGestureActive(item->getLinkedUUID()))
				return TRUE;
			break;
		default:
			break;
	}
	return FALSE;
}

BOOL get_can_item_be_worn(const LLUUID& id)
{
	const LLViewerInventoryItem* item = gInventory.getItem(id);
	if (!item)
		return FALSE;

	if (LLAppearanceMgr::instance().isLinkedInCOF(item->getLinkedUUID()))
	{
		// an item having links in COF (i.e. a worn item)
		return FALSE;
	}

	if (gInventory.isObjectDescendentOf(id, LLAppearanceMgr::instance().getCOF()))
	{
		// a non-link object in COF (should not normally happen)
		return FALSE;
	}
	
	const LLUUID trash_id = gInventory.findCategoryUUIDForType(
			LLFolderType::FT_TRASH);

	// item can't be worn if base obj in trash, see EXT-7015
	if (gInventory.isObjectDescendentOf(item->getLinkedUUID(),
			trash_id))
	{
		return false;
	}

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(item->getLinkedUUID()))
			{
				// Already being worn
				return FALSE;
			}
			else
			{
				// Not being worn yet.
				return TRUE;
			}
			break;
		}
	case LLAssetType::AT_BODYPART:
	case LLAssetType::AT_CLOTHING:
			if(gAgentWearables.isWearingItem(item->getLinkedUUID()))
			{
				// Already being worn
				return FALSE;
			}
			else
			{
				// Not being worn yet.
				return TRUE;
			}
			break;
		default:
			break;
	}
	return FALSE;
}

BOOL get_is_item_removable(const LLInventoryModel* model, const LLUUID& id)
{
	if (!model)
	{
		return FALSE;
	}

	if(id == gInventory.getRootFolderID())
	{
		return FALSE;
	}

	// Can't delete an item that's in the library.
	if(!model->isObjectDescendentOf(id, gInventory.getRootFolderID()))
	{
		return FALSE;
	}

	// Disable delete from COF folder; have users explicitly choose "detach/take off",
	// unless the item is not worn but in the COF (i.e. is bugged).
	if (LLAppearanceMgr::instance().getIsProtectedCOFItem(id))
	{
		if (get_is_item_worn(id))
		{
			return FALSE;
		}
	}

// [RLVa:KB] - Checked: 2011-03-29 (RLVa-1.3.0g) | Modified: RLVa-1.3.0g
	if ( (rlv_handler_t::isEnabled()) && 
		 (RlvFolderLocks::instance().hasLockedFolder(RLV_LOCK_ANY)) && (!RlvFolderLocks::instance().canRemoveItem(id)) )
	{
		return FALSE;
	}
// [/RLVa:KB]

	const LLInventoryObject *obj = model->getItem(id);
	if (obj && obj->getIsLinkType())
	{
		return TRUE;
	}
	if (get_is_item_worn(id))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL get_is_category_removable(const LLInventoryModel* model, const LLUUID& id)
{
	// NOTE: This function doesn't check the folder's children.
	// See LLFolderBridge::isItemRemovable for a function that does
	// consider the children.

	if (!model)
	{
		return FALSE;
	}

	if (!model->isObjectDescendentOf(id, gInventory.getRootFolderID()))
	{
		return FALSE;
	}

// [RLVa:KB] - Checked: 2011-03-29 (RLVa-1.3.0g) | Modified: RLVa-1.3.0g
	if ( ((rlv_handler_t::isEnabled()) && 
		 (RlvFolderLocks::instance().hasLockedFolder(RLV_LOCK_ANY)) && (!RlvFolderLocks::instance().canRemoveFolder(id))) )
	{
		return FALSE;
	}
// [/RLVa:KB]

	if (!isAgentAvatarValid()) return FALSE;

	const LLInventoryCategory* category = model->getCategory(id);
	if (!category)
	{
		return FALSE;
	}

	const LLFolderType::EType folder_type = category->getPreferredType();
	
#ifndef DELETE_SYSTEM_FOLDERS
	if (LLFolderType::lookupIsProtectedType(folder_type))
	{
		return FALSE;
	}
#endif

	// Can't delete the outfit that is currently being worn.
	if (folder_type == LLFolderType::FT_OUTFIT)
	{
		const LLViewerInventoryItem *base_outfit_link = LLAppearanceMgr::instance().getBaseOutfitLink();
		if (base_outfit_link && (category == base_outfit_link->getLinkedCategory()))
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL get_is_category_renameable(const LLInventoryModel* model, const LLUUID& id)
{
	if (!model)
	{
		return FALSE;
	}

// [RLVa:KB] - Checked: 2011-03-29 (RLVa-1.3.0g) | Modified: RLVa-1.3.0g
	if ( (rlv_handler_t::isEnabled()) && (model == &gInventory) && (!RlvFolderLocks::instance().canRenameFolder(id)) )
	{
		return FALSE;
	}
// [/RLVa:KB]

	LLViewerInventoryCategory* cat = model->getCategory(id);

	if (cat && !LLFolderType::lookupIsProtectedType(cat->getPreferredType()) &&
		cat->getOwnerID() == gAgent.getID())
	{
		return TRUE;
	}
	return FALSE;
}


void show_item_profile(const LLUUID& item_uuid)
{
	LLUUID linked_uuid = gInventory.getLinkedItemID(item_uuid);
	if(!LLFloaterProperties::show(linked_uuid, LLUUID::null))
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PropertiesRect");
		rect.translate( left - rect.mLeft, top - rect.mTop );
		LLFloaterProperties* floater;
		floater = new LLFloaterProperties("item properties",
										rect,
										"Inventory Item Properties",
										linked_uuid,
										LLUUID::null);
		// keep onscreen
		gFloaterView->adjustToFitScreen(floater, FALSE);
	}
}

void open_marketplace_listings()
{
	LLFloaterMarketplaceListings::showInstance();
}

///----------------------------------------------------------------------------
// Marketplace functions
//
// Handles Copy and Move to or within the Marketplace listings folder.
// Handles creation of stock folders, nesting of listings and version folders,
// permission checking and listings validation.
///----------------------------------------------------------------------------

S32 depth_nesting_in_marketplace(LLUUID cur_uuid)
{
	// Get the marketplace listings root, exit with -1 (i.e. not under the marketplace listings root) if none
	const LLUUID marketplace_listings_uuid = gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS, false);
	if (marketplace_listings_uuid.isNull())
	{
		return -1;
	}
	// If not a descendent of the marketplace listings root, then the nesting depth is -1 by definition
	if (!gInventory.isObjectDescendentOf(cur_uuid, marketplace_listings_uuid))
	{
		return -1;
	}

	// Iterate through the parents till we hit the marketplace listings root
	// Note that the marketplace listings root itself will return 0
	S32 depth = 0;
	LLInventoryObject* cur_object = gInventory.getObject(cur_uuid);
	while (cur_uuid != marketplace_listings_uuid)
	{
		depth++;
		cur_uuid = cur_object->getParentUUID();
		cur_object = gInventory.getCategory(cur_uuid);
	}
	return depth;
}

// Returns the UUID of the marketplace listing this object is in
LLUUID nested_parent_id(LLUUID cur_uuid, S32 depth)
{
	if (depth < 1)
	{
		// For objects outside the marketplace listings root (or root itself), we return a NULL UUID
		return LLUUID::null;
	}
	else if (depth == 1)
	{
		// Just under the root, we return the passed UUID itself if it's a folder, NULL otherwise (not a listing)
		LLViewerInventoryCategory* cat = gInventory.getCategory(cur_uuid);
		return (cat ? cur_uuid : LLUUID::null);
	}

	// depth > 1
	LLInventoryObject* cur_object = gInventory.getObject(cur_uuid);
	while (depth > 1)
	{
		depth--;
		cur_uuid = cur_object->getParentUUID();
		cur_object = gInventory.getCategory(cur_uuid);
	}
	return cur_uuid;
}

S32 compute_stock_count(LLUUID cat_uuid, bool force_count /* false */)
{
	// Handle the case of the folder being a stock folder immediately
	LLViewerInventoryCategory* cat = gInventory.getCategory(cat_uuid);
	if (!cat)
	{
		// Not a category so no stock count to speak of
		return COMPUTE_STOCK_INFINITE;
	}
	if (cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
	{
		if (cat->getVersion() == LLViewerInventoryCategory::VERSION_UNKNOWN)
		{
			// If the folder is not completely fetched, we do not want to return any confusing value that could lead to unlisting
			// "COMPUTE_STOCK_NOT_EVALUATED" denotes that a stock folder has a count that cannot be evaluated at this time (folder not up to date)
			return COMPUTE_STOCK_NOT_EVALUATED;
		}
		// Note: stock folders are *not* supposed to have nested subfolders so we stop recursion here but we count only items (subfolders will be ignored)
		// Note: we *always* give a stock count for stock folders, it's useful even if the listing is unassociated
		LLInventoryModel::cat_array_t* cat_array;
		LLInventoryModel::item_array_t* item_array;
		gInventory.getDirectDescendentsOf(cat_uuid,cat_array,item_array);
		return item_array->size();
	}

	// When force_count is true, we do not do any verification of the marketplace status and simply compute
	// the stock amount based on the descendent hierarchy. This is used specifically when creating a listing.
	if (!force_count)
	{
		// Grab marketplace data for this folder
		S32 depth = depth_nesting_in_marketplace(cat_uuid);
		LLUUID listing_uuid = nested_parent_id(cat_uuid, depth);
		if (!LLMarketplaceData::instance().isListed(listing_uuid))
		{
			// If not listed, the notion of stock is meaningless so it won't be computed for any level
			return COMPUTE_STOCK_INFINITE;
		}

		LLUUID version_folder_uuid = LLMarketplaceData::instance().getVersionFolder(listing_uuid);
		// Handle the case of the first 2 levels : listing and version folders
		if (depth == 1)
		{
			if (version_folder_uuid.notNull())
			{
				// If there is a version folder, the stock value for the listing is the version folder stock
				return compute_stock_count(version_folder_uuid, true);
			}
			else
			{
				// If there's no version folder associated, the notion of stock count has no meaning
				return COMPUTE_STOCK_INFINITE;
			}
		}
		else if (depth == 2)
		{
			if (version_folder_uuid.notNull() && (version_folder_uuid != cat_uuid))
			{
				// If there is a version folder but we're not it, our stock count is meaningless
				return COMPUTE_STOCK_INFINITE;
			}
		}
	}

	// In all other cases, the stock count is the min of stock folders count found in the descendents
	// "COMPUTE_STOCK_NOT_EVALUATED" denotes that a stock folder in the hierarchy has a count that cannot be evaluated at this time (folder not up to date)
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat_uuid,cat_array,item_array);

	// "COMPUTE_STOCK_INFINITE" denotes a folder that doesn't contain any stock folders in its descendents
	S32 curr_count = COMPUTE_STOCK_INFINITE;

	// Note: marketplace listings have a maximum depth nesting of 4
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLInventoryCategory* category = *iter;
		S32 count = compute_stock_count(category->getUUID(), true);
		if ((curr_count == COMPUTE_STOCK_INFINITE) || ((count != COMPUTE_STOCK_INFINITE) && (count < curr_count)))
		{
			curr_count = count;
		}
	}

	return curr_count;
}

// local helper
bool can_move_to_marketplace(LLInventoryItem* inv_item, std::string& tooltip_msg, bool resolve_links)
{
	// Collapse links directly to items/folders
	LLViewerInventoryItem* viewer_inv_item = (LLViewerInventoryItem *) inv_item;
	LLViewerInventoryItem* linked_item = viewer_inv_item->getLinkedItem();
	LLViewerInventoryCategory* linked_category = viewer_inv_item->getLinkedCategory();

	// Linked items and folders cannot be put for sale
	if (linked_category || linked_item)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxLinked");
		return false;
	}

	// A category is always considered as passing...
	if (linked_category != NULL)
	{
		return true;
	}

	// Take the linked item if necessary
	if (linked_item != NULL)
	{
		inv_item = linked_item;
	}

	// Check that the agent has transfer permission on the item: this is required as a resident cannot
	// put on sale items she cannot transfer. Proceed with move if we have permission.
	bool allow_transfer = inv_item->getPermissions().allowOperationBy(PERM_TRANSFER, gAgent.getID());
	if (!allow_transfer)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxNoTransfer");
		return false;
	}

	// Check worn/not worn status: worn items cannot be put on the marketplace
	bool worn = get_is_item_worn(inv_item->getUUID());
	if (worn)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxWorn");
		return false;
	}

	// Check library status: library items cannot be put on the marketplace
	if (!gInventory.isObjectDescendentOf(inv_item->getUUID(), gInventory.getRootFolderID()))
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxNotInInventory");
		return false;
	}

	// Check type: for the moment, calling cards cannot be put on the marketplace
	bool calling_card = (LLAssetType::AT_CALLINGCARD == inv_item->getType());
	if (calling_card)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxCallingCard");
		return false;
	}

	return true;
}

// local helper
// Returns the max tree length (in folder nodes) down from the argument folder
int get_folder_levels(LLInventoryCategory* inv_cat)
{
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(inv_cat->getUUID(), cats, items);

	int max_child_levels = 0;

	for (U32 i = 0; i < cats->size(); ++i)
	{
		LLInventoryCategory* category = cats->at(i);
		max_child_levels = llmax(max_child_levels, get_folder_levels(category));
	}

	return 1 + max_child_levels;
}

// local helper
// Returns the distance (in folder nodes) between the ancestor and its descendant. Returns -1 if not related.
int get_folder_path_length(const LLUUID& ancestor_id, const LLUUID& descendant_id)
{
	int depth = 0;

	if (ancestor_id == descendant_id) return depth;

	const LLInventoryCategory* category = gInventory.getCategory(descendant_id);

	while (category)
	{
		LLUUID parent_id = category->getParentUUID();

		if (parent_id.isNull()) break;

		depth++;

		if (parent_id == ancestor_id) return depth;

		category = gInventory.getCategory(parent_id);
	}

	LL_WARNS("SLM") << "get_folder_path_length() couldn't trace a path from the descendant to the ancestor" << LL_ENDL;
	return -1;
}

// local helper
// Returns true if all items within the argument folder are fit for sale, false otherwise
bool has_correct_permissions_for_sale(LLInventoryCategory* cat, std::string& error_msg)
{
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(),cat_array,item_array);

	LLInventoryModel::item_array_t item_array_copy = *item_array;

	for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin(); iter != item_array_copy.end(); iter++)
	{
		LLInventoryItem* item = *iter;
		if (!can_move_to_marketplace(item, error_msg, false))
		{
			return false;
		}
	}

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;

	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLInventoryCategory* category = *iter;
		if (!has_correct_permissions_for_sale(category, error_msg))
		{
			return false;
		}
	}
	return true;
}

// Returns true if inv_item can be dropped in dest_folder, a folder nested in marketplace listings (or merchant inventory) under the root_folder root
// If returns is false, tooltip_msg contains an error message to display to the user (localized and all).
// bundle_size is the amount of sibling items that are getting moved to the marketplace at the same time.
bool can_move_item_to_marketplace(const LLInventoryCategory* root_folder, LLInventoryCategory* dest_folder, LLInventoryItem* inv_item, std::string& tooltip_msg, S32 bundle_size, bool from_paste)
{
	// Check stock folder type matches item type in marketplace listings (even if of no use there for the moment)
	LLViewerInventoryCategory* view_folder = dynamic_cast<LLViewerInventoryCategory*>(dest_folder);
	bool move_in_stock = (view_folder && (view_folder->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK));
	bool accept = (view_folder && view_folder->acceptItem(inv_item));
	if (!accept)
	{
		tooltip_msg = LLTrans::getString("TooltipOutboxMixedStock");
	}

	// Check that the item has the right type and permissions to be sold on the marketplace
	if (accept)
	{
		accept = can_move_to_marketplace(inv_item, tooltip_msg, true);
	}

	// Check that the total amount of items won't violate the max limit on the marketplace
	if (accept)
	{
		// If the dest folder is a stock folder, we do not count the incoming items toward the total (stock items are seen as one)
		S32 existing_item_count = (move_in_stock ? 0 : bundle_size);

		// If the dest folder is a stock folder, we do assume that the incoming items are also stock items (they should anyway)
		S32 existing_stock_count = (move_in_stock ? bundle_size : 0);

		S32 existing_folder_count = 0;

		// Get the version folder: that's where the counts start from
		const LLViewerInventoryCategory* version_folder = ((root_folder && (root_folder != dest_folder)) ? gInventory.getFirstDescendantOf(root_folder->getUUID(), dest_folder->getUUID()) : NULL);

		if (version_folder)
		{
			if (!from_paste && gInventory.isObjectDescendentOf(inv_item->getUUID(), version_folder->getUUID()))
			{
				// Clear those counts or they will be counted twice because we're already inside the version category
				existing_item_count = 0;
			}

			LLInventoryModel::cat_array_t existing_categories;
			LLInventoryModel::item_array_t existing_items;

			gInventory.collectDescendents(version_folder->getUUID(), existing_categories, existing_items, FALSE);

			existing_item_count += count_copyable_items(existing_items) + count_stock_folders(existing_categories);
			existing_stock_count += count_stock_items(existing_items);
			existing_folder_count += existing_categories.size();

			// If the incoming item is a nocopy (stock) item, we need to consider that it will create a stock folder
			if (!inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()) && !move_in_stock)
			{
				// Note : we do not assume that all incoming items are nocopy of different kinds...
				existing_folder_count += 1;
			}
		}

		if (existing_item_count > (S32)gSavedSettings.getU32("InventoryOutboxMaxItemCount"))
		{
			LLStringUtil::format_map_t args;
			U32 amount = gSavedSettings.getU32("InventoryOutboxMaxItemCount");
			args["[AMOUNT]"] = llformat("%d",amount);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyObjects", args);
			accept = false;
		}
		else if (existing_stock_count > (S32)gSavedSettings.getU32("InventoryOutboxMaxStockItemCount"))
		{
			LLStringUtil::format_map_t args;
			U32 amount = gSavedSettings.getU32("InventoryOutboxMaxStockItemCount");
			args["[AMOUNT]"] = llformat("%d",amount);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyStockItems", args);
			accept = false;
		}
		else if (existing_folder_count > (S32)gSavedSettings.getU32("InventoryOutboxMaxFolderCount"))
		{
			LLStringUtil::format_map_t args;
			U32 amount = gSavedSettings.getU32("InventoryOutboxMaxFolderCount");
			args["[AMOUNT]"] = llformat("%d",amount);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyFolders", args);
			accept = false;
		}
	}

	return accept;
}

// Returns true if inv_cat can be dropped in dest_folder, a folder nested in marketplace listings (or merchant inventory) under the root_folder root
// If returns is false, tooltip_msg contains an error message to display to the user (localized and all).
// bundle_size is the amount of sibling items that are getting moved to the marketplace at the same time.
bool can_move_folder_to_marketplace(const LLInventoryCategory* root_folder, LLInventoryCategory* dest_folder, LLInventoryCategory* inv_cat, std::string& tooltip_msg, S32 bundle_size, bool check_items, bool from_paste)
{
	bool accept = true;

	// Compute the nested folders level we'll add into with that incoming folder
	int incoming_folder_depth = get_folder_levels(inv_cat);
	// Compute the nested folders level we're inserting ourselves in
	// Note: add 1 when inserting under a listing folder as we need to take the root listing folder in the count
    int insertion_point_folder_depth = (root_folder ? get_folder_path_length(root_folder->getUUID(), dest_folder->getUUID()) + 1 : 1);

	// Get the version folder: that's where the folders and items counts start from
	const LLViewerInventoryCategory* version_folder = (insertion_point_folder_depth >= 2 ? gInventory.getFirstDescendantOf(root_folder->getUUID(), dest_folder->getUUID()) : NULL);

	// Compare the whole with the nested folders depth limit
	// Note: substract 2 as we leave root and version folder out of the count threshold
	if ((incoming_folder_depth + insertion_point_folder_depth - 2) > (S32)(gSavedSettings.getU32("InventoryOutboxMaxFolderDepth")))
	{
		LLStringUtil::format_map_t args;
		U32 amount = gSavedSettings.getU32("InventoryOutboxMaxFolderDepth");
		args["[AMOUNT]"] = llformat("%d",amount);
		tooltip_msg = LLTrans::getString("TooltipOutboxFolderLevels", args);
		accept = false;
	}

	if (accept)
	{
		LLInventoryModel::cat_array_t descendent_categories;
		LLInventoryModel::item_array_t descendent_items;
		gInventory.collectDescendents(inv_cat->getUUID(), descendent_categories, descendent_items, FALSE);

		S32 dragged_folder_count = descendent_categories.size() + bundle_size;  // Note: We assume that we're moving a bunch of folders in. That might be wrong...
		S32 dragged_item_count = count_copyable_items(descendent_items) + count_stock_folders(descendent_categories);
		S32 dragged_stock_count = count_stock_items(descendent_items);
		S32 existing_item_count = 0;
		S32 existing_stock_count = 0;
		S32 existing_folder_count = 0;

		if (version_folder)
		{
			if (!from_paste && gInventory.isObjectDescendentOf(inv_cat->getUUID(), version_folder->getUUID()))
			{
				// Clear those counts or they will be counted twice because we're already inside the version category
				dragged_folder_count = 0;
				dragged_item_count = 0;
				dragged_stock_count = 0;
			}

			// Tally the total number of categories and items inside the root folder
			LLInventoryModel::cat_array_t existing_categories;
			LLInventoryModel::item_array_t existing_items;
			gInventory.collectDescendents(version_folder->getUUID(), existing_categories, existing_items, FALSE);

			existing_folder_count += existing_categories.size();
			existing_item_count += count_copyable_items(existing_items) + count_stock_folders(existing_categories);
			existing_stock_count += count_stock_items(existing_items);
		}

		const S32 total_folder_count = existing_folder_count + dragged_folder_count;
		const S32 total_item_count = existing_item_count + dragged_item_count;
		const S32 total_stock_count = existing_stock_count + dragged_stock_count;

		if (total_folder_count > (S32)gSavedSettings.getU32("InventoryOutboxMaxFolderCount"))
		{
			LLStringUtil::format_map_t args;
			U32 amount = gSavedSettings.getU32("InventoryOutboxMaxFolderCount");
			args["[AMOUNT]"] = llformat("%d",amount);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyFolders", args);
			accept = false;
		}
		else if (total_item_count > (S32)gSavedSettings.getU32("InventoryOutboxMaxItemCount"))
		{
			LLStringUtil::format_map_t args;
			U32 amount = gSavedSettings.getU32("InventoryOutboxMaxItemCount");
			args["[AMOUNT]"] = llformat("%d",amount);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyObjects", args);
			accept = false;
		}
		else if (total_stock_count > (S32)gSavedSettings.getU32("InventoryOutboxMaxStockItemCount"))
		{
			LLStringUtil::format_map_t args;
			U32 amount = gSavedSettings.getU32("InventoryOutboxMaxStockItemCount");
			args["[AMOUNT]"] = llformat("%d",amount);
			tooltip_msg = LLTrans::getString("TooltipOutboxTooManyStockItems", args);
			accept = false;
		}

		// Now check that each item in the folder can be moved in the marketplace
		if (accept && check_items)
		{
			for (U32 i=0; i < descendent_items.size(); ++i)
			{
				LLInventoryItem* item = descendent_items[i];
				if (!can_move_to_marketplace(item, tooltip_msg, false))
				{
					accept = false;
					break;
				}
			}
		}
	}

	return accept;
}

bool move_item_to_marketplacelistings(LLInventoryItem* inv_item, LLUUID dest_folder, bool copy)
{
	// Get the marketplace listings depth of the destination folder, exit with error if not under marketplace
	S32 depth = depth_nesting_in_marketplace(dest_folder);
	if (depth < 0)
	{
		LLSD subs;
		subs["[ERROR_CODE]"] = LLTrans::getString("Marketplace Error Prefix") + LLTrans::getString("Marketplace Error Not Merchant");
		LLNotificationsUtil::add("MerchantPasteFailed", subs);
		return false;
	}
	
	// We will collapse links into items/folders
	LLViewerInventoryItem * viewer_inv_item = (LLViewerInventoryItem *) inv_item;
	LLViewerInventoryCategory * linked_category = viewer_inv_item->getLinkedCategory();

	if (linked_category != NULL)
	{
		// Move the linked folder directly
		return move_folder_to_marketplacelistings(linked_category, dest_folder, copy);
	}
	else
	{
		// Grab the linked item if any
		LLViewerInventoryItem * linked_item = viewer_inv_item->getLinkedItem();
		viewer_inv_item = (linked_item != NULL ? linked_item : viewer_inv_item);

		// If we want to copy but the item is no copy, fail silently (this is a common case that doesn't warrant notification)
		if (copy && !viewer_inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			return false;
		}

		// Check that the agent has transfer permission on the item: this is required as a resident cannot
		// put on sale items she cannot transfer. Proceed with move if we have permission.
		std::string error_msg;
		if (can_move_to_marketplace(inv_item, error_msg, true))
		{
			// When moving an isolated item, we might need to create the folder structure to support it
			if (depth == 0)
			{
				// We need a listing folder
				dest_folder = gInventory.createNewCategory(dest_folder, LLFolderType::FT_NONE, viewer_inv_item->getName());
				depth++;
			}
			if (depth == 1)
			{
				// We need a version folder
				dest_folder = gInventory.createNewCategory(dest_folder, LLFolderType::FT_NONE, viewer_inv_item->getName());
				depth++;
			}
			LLViewerInventoryCategory* dest_cat = gInventory.getCategory(dest_folder);
			if (!viewer_inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()) &&
				(dest_cat->getPreferredType() != LLFolderType::FT_MARKETPLACE_STOCK))
			{
				// We need to create a stock folder to move a no copy item
				dest_folder = gInventory.createNewCategory(dest_folder, LLFolderType::FT_MARKETPLACE_STOCK, viewer_inv_item->getName());
				dest_cat = gInventory.getCategory(dest_folder);
				depth++;
			}

			// Verify we can have this item in that destination category
			if (!dest_cat->acceptItem(viewer_inv_item))
			{
				LLSD subs;
				subs["[ERROR_CODE]"] = LLTrans::getString("Marketplace Error Prefix") + LLTrans::getString("Marketplace Error Not Accepted");
				LLNotificationsUtil::add("MerchantPasteFailed", subs);
				return false;
			}

			// Get the parent folder of the moved item : we may have to update it
			LLUUID src_folder = viewer_inv_item->getParentUUID();

			if (copy)
			{
				// Copy the item
				LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(update_folder_cb, dest_folder));
				copy_inventory_item(
									 gAgent.getID(),
									 viewer_inv_item->getPermissions().getOwner(),
									 viewer_inv_item->getUUID(),
									 dest_folder,
									 std::string(),
									 cb);
			}
			else
			{
				// Reparent the item
				gInventory.changeItemParent(viewer_inv_item, dest_folder, true);
			}
		}
		else
		{
			LLSD subs;
			subs["[ERROR_CODE]"] = LLTrans::getString("Marketplace Error Prefix") + error_msg;
			LLNotificationsUtil::add("MerchantPasteFailed", subs);
			return false;
		}
	}

	open_marketplace_listings();
	return true;
}

bool move_folder_to_marketplacelistings(LLInventoryCategory* inv_cat, const LLUUID& dest_folder, bool copy, bool move_no_copy_items)
{
	// Check that we have adequate permission on all items being moved. Proceed if we do.
	std::string error_msg;
	if (has_correct_permissions_for_sale(inv_cat, error_msg))
	{
		// Get the destination folder
		LLViewerInventoryCategory* dest_cat = gInventory.getCategory(dest_folder);

		// Check it's not a stock folder
		if (dest_cat->getPreferredType() == LLFolderType::FT_MARKETPLACE_STOCK)
		{
			LLSD subs;
			subs["[ERROR_CODE]"] = LLTrans::getString("Marketplace Error Prefix") + LLTrans::getString("Marketplace Error Not Accepted");
			LLNotificationsUtil::add("MerchantPasteFailed", subs);
			return false;
		}

		// Get the parent folder of the moved item : we may have to update it
		LLUUID src_folder = inv_cat->getParentUUID();

		LLViewerInventoryCategory* viewer_inv_cat = (LLViewerInventoryCategory*) inv_cat;
		if (copy)
		{
			// Copy the folder
			copy_inventory_category(&gInventory, viewer_inv_cat, dest_folder, LLUUID::null, move_no_copy_items);
		}
		else
		{
			// Reparent the folder
			gInventory.changeCategoryParent(viewer_inv_cat, dest_folder, false);
			// Check the destination folder recursively for no copy items and promote the including folders if any
			validate_marketplacelistings(dest_cat);
		}

		// Update the modified folders
		update_marketplace_category(src_folder);
		update_marketplace_category(dest_folder);
		gInventory.notifyObservers();
	}
	else
	{
		LLSD subs;
		subs["[ERROR_CODE]"] = LLTrans::getString("Marketplace Error Prefix") + error_msg;
		LLNotificationsUtil::add("MerchantPasteFailed", subs);
		return false;
	}

	open_marketplace_listings();
	return true;
}

bool sort_alpha(const LLViewerInventoryCategory* cat1, const LLViewerInventoryCategory* cat2)
{
	return cat1->getName().compare(cat2->getName()) < 0;
}

void dump_trace(std::string& message, S32 depth, LLError::ELevel log_level)
{
	LL_INFOS() << "validate_marketplacelistings : error = "<< log_level << ", depth = " << depth << ", message = " << message <<  LL_ENDL;
}

// Make all relevant business logic checks on the marketplace listings starting with the folder as argument.
// This function does no deletion of listings but a mere audit and raises issues to the user (through the
// optional callback cb). It also returns a boolean, true if things validate, false if issues are raised.
// The only inventory changes that are done is to move and sort folders containing no-copy items to stock folders.
bool validate_marketplacelistings(LLInventoryCategory* cat, validation_callback_t cb, bool fix_hierarchy, S32 depth)
{
#if 0
	// Used only for debug
	if (!cb)
	{
		cb =  boost::bind(&dump_trace, _1, _2, _3);
	}
#endif
    // Folder is valid unless issue is raised
	bool result = true;

	// Get the type and the depth of the folder
	LLViewerInventoryCategory* viewer_cat = (LLViewerInventoryCategory*) (cat);
	const LLFolderType::EType folder_type = cat->getPreferredType();
	if (depth < 0)
	{
		// If the depth argument was not provided, evaluate the depth directly
		depth = depth_nesting_in_marketplace(cat->getUUID());
	}
	if (depth < 0)
	{
		// If the folder is not under the marketplace listings root, we run validation as if it was a listing folder and prevent any hierarchy fix
		// This allows the function to be used to pre-validate a folder anywhere in the inventory
		depth = 1;
		fix_hierarchy = false;
	}

	// Set the indentation for print output (typically, audit button in marketplace folder floater)
	std::string indent;
	for (int i = 1; i < depth; i++)
	{
		indent += "    ";
	}

	// Check out that version folders are marketplace ready
	if (depth == 2)
	{
		std::string message;
		// Note: if we fix the hierarchy, we want to check the items individually, hence the last argument here
		if (!can_move_folder_to_marketplace(cat, cat, cat, message, 0, fix_hierarchy))
		{
			result = false;
			if (cb)
			{
				message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Error") + " " + message;
				cb(message, depth, LLError::LEVEL_ERROR);
			}
		}
	}

	// Check out that stock folders are at the right level
	if ((folder_type == LLFolderType::FT_MARKETPLACE_STOCK) && (depth <= 2))
	{
		if (fix_hierarchy)
		{
			if (cb)
			{
				std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Warning") + " " + LLTrans::getString("Marketplace Validation Warning Stock");
				cb(message, depth, LLError::LEVEL_WARN);
			}
			// Nest the stock folder one level deeper in a normal folder and restart from there
			LLUUID parent_uuid = cat->getParentUUID();
			LLUUID folder_uuid = gInventory.createNewCategory(parent_uuid, LLFolderType::FT_NONE, cat->getName());
			LLInventoryCategory* new_cat = gInventory.getCategory(folder_uuid);
			gInventory.changeCategoryParent(viewer_cat, folder_uuid, false);
			result &= validate_marketplacelistings(new_cat, cb, fix_hierarchy, depth + 1);
			return result;
		}
		else
		{
			result = false;
			if (cb)
			{
				std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Error") + " " + LLTrans::getString("Marketplace Validation Warning Stock");
				cb(message, depth, LLError::LEVEL_ERROR);
			}
		}
	}

	// Item sorting and validation : sorting and moving the various stock items is complicated as the set of constraints is high
	// We need to:
	// * separate non stock items, stock items per types in different folders
	// * have stock items nested at depth 2 at least
	// * never ever move the non-stock items

	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(),cat_array,item_array);

	// We use a composite (type,permission) key on that map to store UUIDs of items of same (type,permissions)
	std::map<U32, uuid_vec_t > items_vector;

	// Parse the items and create vectors of item UUIDs sorting copyable items and stock items of various types
	bool has_bad_items = false;
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin(); iter != item_array_copy.end(); iter++)
	{
		LLInventoryItem* item = *iter;
		LLViewerInventoryItem * viewer_inv_item = (LLViewerInventoryItem *) item;

		// Test but skip items that shouldn't be there to start with, raise an error message for those
		std::string error_msg;
		if (!can_move_to_marketplace(item, error_msg, false))
		{
			has_bad_items = true;
			if (cb && fix_hierarchy)
			{
				std::string message = indent + viewer_inv_item->getName() + LLTrans::getString("Marketplace Validation Error") + " " + error_msg;
				cb(message, depth, LLError::LEVEL_ERROR);
			}
			continue;
		}
		// Update the appropriate vector item for that type
		LLInventoryType::EType type = LLInventoryType::IT_COUNT;    // Default value for non stock items
		U32 perms = 0;
		if (!viewer_inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			// Get the item type for stock items
			type = viewer_inv_item->getInventoryType();
			perms = viewer_inv_item->getPermissions().getMaskNextOwner();
		}
		U32 key = (((U32)(type) & 0xFF) << 24) | (perms & 0xFFFFFF);
		items_vector[key].push_back(viewer_inv_item->getUUID());
	}

	// How many types of items? Which type is it if only one?
	S32 count = items_vector.size();
	U32 default_key = (U32)(LLInventoryType::IT_COUNT) << 24; // This is the key for any normal copyable item
	U32 unique_key = (count == 1 ? items_vector.begin()->first : default_key); // The key in the case of one item type only

	// If we have no items in there (only folders or empty), analyze a bit further
	if ((count == 0) && !has_bad_items)
	{
		if (cat_array->size() == 0)
		{
			// So we have no item and no folder. That's at least a warning.
			if (depth == 2)
			{
				// If this is an empty version folder, warn only (listing won't be delivered by AIS, but only AIS should unlist)
				if (cb)
				{
					std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Error Empty Version");
					cb(message, depth, LLError::LEVEL_WARN);
				}
			}
			else if ((folder_type == LLFolderType::FT_MARKETPLACE_STOCK) && (depth > 2))
			{
				// If this is a legit but empty stock folder, warn only (listing must stay searchable when out of stock)
				if (cb)
				{
					std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Error Empty Stock");
					cb(message, depth, LLError::LEVEL_WARN);
				}
			}
			else if (cb)
			{
				// We warn if there's nothing in a regular folder (maybe it's an under construction listing)
				std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Warning Empty");
				cb(message, depth, LLError::LEVEL_WARN);
			}
		}
		else
		{
			// Done with that folder : Print out the folder name unless we already found an error here
			if (cb && result && (depth >= 1))
			{
				std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Log");
				cb(message, depth, LLError::LEVEL_INFO);
			}
		}
	}
	// If we have a single type of items of the right type in the right place, we're done
	else if ((count == 1) && !has_bad_items && (((unique_key == default_key) && (depth > 1)) || ((folder_type == LLFolderType::FT_MARKETPLACE_STOCK) && (depth > 2) && (cat_array->size() == 0))))
	{
		// Done with that folder : Print out the folder name unless we already found an error here
		if (cb && result && (depth >= 1))
		{
			std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Log");
			cb(message, depth, LLError::LEVEL_INFO);
		}
	}
	else
	{
		if (fix_hierarchy && !has_bad_items)
		{
			// Alert the user when an existing stock folder has to be split
			if ((folder_type == LLFolderType::FT_MARKETPLACE_STOCK) && ((count >= 2) || (cat_array->size() > 0)))
			{
				LLNotificationsUtil::add("AlertMerchantStockFolderSplit");
			}
			// If we have more than 1 type of items or we are at the listing level or we have stock/no stock type mismatch, wrap the items in subfolders
			if ((count > 1) || (depth == 1) ||
				((folder_type == LLFolderType::FT_MARKETPLACE_STOCK) && (unique_key == default_key)) ||
				((folder_type != LLFolderType::FT_MARKETPLACE_STOCK) && (unique_key != default_key)))
			{
				// Create one folder per vector at the right depth and of the right type
				auto items_vector_it = items_vector.begin();
				while (items_vector_it != items_vector.end())
				{
					// Create a new folder
					LLUUID parent_uuid = (depth > 2 ? viewer_cat->getParentUUID() : viewer_cat->getUUID());
					LLViewerInventoryItem* viewer_inv_item = gInventory.getItem(items_vector_it->second.back());
					std::string folder_name = (depth >= 1 ? viewer_cat->getName() : viewer_inv_item->getName());
					LLFolderType::EType new_folder_type = (items_vector_it->first == default_key ? LLFolderType::FT_NONE : LLFolderType::FT_MARKETPLACE_STOCK);
					if (cb)
					{
						std::string message = "";
						if (new_folder_type == LLFolderType::FT_MARKETPLACE_STOCK)
						{
							message = indent + folder_name + LLTrans::getString("Marketplace Validation Warning Create Stock");
						}
						else
						{
							message = indent + folder_name + LLTrans::getString("Marketplace Validation Warning Create Version");
						}
						cb(message, depth, LLError::LEVEL_WARN);
					}
					LLUUID folder_uuid = gInventory.createNewCategory(parent_uuid, new_folder_type, folder_name);

					// Move each item to the new folder
					while (!items_vector_it->second.empty())
					{
						LLViewerInventoryItem* viewer_inv_item = gInventory.getItem(items_vector_it->second.back());
						if (cb)
						{
							std::string message = indent + viewer_inv_item->getName() + LLTrans::getString("Marketplace Validation Warning Move");
							cb(message, depth, LLError::LEVEL_WARN);
						}
						gInventory.changeItemParent(viewer_inv_item, folder_uuid, true);
						items_vector_it->second.pop_back();
					}

					// Next type
					update_marketplace_category(parent_uuid);
					update_marketplace_category(folder_uuid);
					gInventory.notifyObservers();
					items_vector_it++;
				}
			}
			// Stock folder should have no sub folder so reparent those up
			if (folder_type == LLFolderType::FT_MARKETPLACE_STOCK)
			{
				LLUUID parent_uuid = cat->getParentUUID();
				gInventory.getDirectDescendentsOf(cat->getUUID(),cat_array,item_array);
				LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
				for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
				{
					LLViewerInventoryCategory* viewer_cat = (LLViewerInventoryCategory*) (*iter);
					gInventory.changeCategoryParent(viewer_cat, parent_uuid, false);
					result &= validate_marketplacelistings(viewer_cat, cb, fix_hierarchy, depth);
				}
			}
		}
		else if (cb)
		{
			// We are not fixing the hierarchy but reporting problems, report everything we can find
			// Print the folder name
			if (result && (depth >= 1))
			{
				if ((folder_type == LLFolderType::FT_MARKETPLACE_STOCK) && (count >= 2))
				{
					// Report if a stock folder contains a mix of items
					result = false;
					std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Error Mixed Stock");
					cb(message, depth, LLError::LEVEL_ERROR);
				}
				else if ((folder_type == LLFolderType::FT_MARKETPLACE_STOCK) && (cat_array->size() != 0))
				{
					// Report if a stock folder contains subfolders
					result = false;
					std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Error Subfolder In Stock");
					cb(message, depth, LLError::LEVEL_ERROR);
				}
				else
				{
					// Simply print the folder name
					std::string message = indent + cat->getName() + LLTrans::getString("Marketplace Validation Log");
					cb(message, depth, LLError::LEVEL_INFO);

				}
			}
			// Scan each item and report if there's a problem
			LLInventoryModel::item_array_t item_array_copy = *item_array;
			for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin(); iter != item_array_copy.end(); iter++)
			{
				LLInventoryItem* item = *iter;
				LLViewerInventoryItem* viewer_inv_item = (LLViewerInventoryItem*) item;
				std::string error_msg;
				if (!can_move_to_marketplace(item, error_msg, false))
				{
					// Report items that shouldn't be there to start with
					result = false;
					std::string message = indent + "    " + viewer_inv_item->getName() + LLTrans::getString("Marketplace Validation Error") + " " + error_msg;
					cb(message, depth, LLError::LEVEL_ERROR);
				}
				else if ((!viewer_inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID())) && (folder_type != LLFolderType::FT_MARKETPLACE_STOCK))
				{
					// Report stock items that are misplaced
					result = false;
					std::string message = indent + "    " + viewer_inv_item->getName() + LLTrans::getString("Marketplace Validation Error Stock Item");
					cb(message, depth, LLError::LEVEL_ERROR);
				}
				else if (depth == 1)
				{
					// Report items not wrapped in version folder
					result = false;
					std::string message = indent + "    " + viewer_inv_item->getName() + LLTrans::getString("Marketplace Validation Warning Unwrapped Item");
					cb(message, depth, LLError::LEVEL_ERROR);
				}
			}
		}

		// Clean up
		if (viewer_cat->getDescendentCount() == 0)
		{
			// Remove the current folder if it ends up empty
			if (cb)
			{
				std::string message = indent + viewer_cat->getName() + LLTrans::getString("Marketplace Validation Warning Delete");
				cb(message, depth, LLError::LEVEL_WARN);
			}
			gInventory.removeCategory(cat->getUUID());
			gInventory.notifyObservers();
			return result && !has_bad_items;
		}
	}
	
	// Recursion : Perform the same validation on each nested folder
	gInventory.getDirectDescendentsOf(cat->getUUID(),cat_array,item_array);
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	// Sort the folders in alphabetical order first
	std::sort(cat_array_copy.begin(), cat_array_copy.end(), sort_alpha);

	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLInventoryCategory* category = *iter;
		result &= validate_marketplacelistings(category, cb, fix_hierarchy, depth + 1);
	}

	update_marketplace_category(cat->getUUID());
	gInventory.notifyObservers();
	return result && !has_bad_items;
}

///----------------------------------------------------------------------------
/// LLInventoryCollectFunctor implementations
///----------------------------------------------------------------------------

// static
bool LLInventoryCollectFunctor::itemTransferCommonlyAllowed(const LLInventoryItem* item)
{
	if (!item)
		return false;

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			//if (!get_is_item_worn(item->getUUID())) // </edit>
				return true;
			break;
		default:
			return true;
			break;
	}
//	return false;
}

bool LLIsType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return TRUE;
	}
	if(item)
	{
		if(item->getType() == mType) return TRUE;
	}
	return FALSE;
}

bool LLIsNotType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return FALSE;
	}
	if(item)
	{
		if(item->getType() == mType) return FALSE;
		else return TRUE;
	}
	return TRUE;
}

bool LLIsOfAssetType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return TRUE;
	}
	if(item)
	{
		if(item->getActualType() == mType) return TRUE;
	}
	return FALSE;
}

bool LLIsValidItemLink::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem) return false;
	return (vitem->getActualType() == LLAssetType::AT_LINK  && !vitem->getIsBrokenLink());
}

bool LLIsTypeWithPermissions::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) 
		{
			return TRUE;
		}
	}
	if(item)
	{
		if(item->getType() == mType)
		{
			LLPermissions perm = item->getPermissions();
			if ((perm.getMaskBase() & mPerm) == mPerm)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

bool LLBuddyCollector::operator()(LLInventoryCategory* cat,
								  LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
		   && (!item->getCreatorUUID().isNull())
		   && (item->getCreatorUUID() != gAgent.getID()))
		{
			return true;
		}
	}
	return false;
}


bool LLUniqueBuddyCollector::operator()(LLInventoryCategory* cat,
										LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
 		   && (item->getCreatorUUID().notNull())
 		   && (item->getCreatorUUID() != gAgent.getID()))
		{
			mSeen.insert(item->getCreatorUUID());
			return true;
		}
	}
	return false;
}


bool LLParticularBuddyCollector::operator()(LLInventoryCategory* cat,
											LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
		   && (item->getCreatorUUID() == mBuddyID))
		{
			return TRUE;
		}
	}
	return FALSE;
}


bool LLNameCategoryCollector::operator()(
	LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(cat)
	{
		if (!LLStringUtil::compareInsensitive(mName, cat->getName()))
		{
			return true;
		}
	}
	return false;
}

bool LLFindCOFValidItems::operator()(LLInventoryCategory* cat,
									 LLInventoryItem* item)
{
	// Valid COF items are:
	// - links to wearables (body parts or clothing)
	// - links to attachments
	// - links to gestures
	// - links to ensemble folders
	LLViewerInventoryItem *linked_item = ((LLViewerInventoryItem*)item)->getLinkedItem();
	if (linked_item)
	{
		LLAssetType::EType type = linked_item->getType();
		return (type == LLAssetType::AT_CLOTHING ||
				type == LLAssetType::AT_BODYPART ||
				type == LLAssetType::AT_GESTURE ||
				type == LLAssetType::AT_OBJECT);
	}
	else
	{
		LLViewerInventoryCategory *linked_category = ((LLViewerInventoryItem*)item)->getLinkedCategory();
		// BAP remove AT_NONE support after ensembles are fully working?
		return (linked_category &&
				((linked_category->getPreferredType() == LLFolderType::FT_NONE) ||
				 (LLFolderType::lookupIsEnsembleType(linked_category->getPreferredType()))));
	}
}

bool LLFindWearables::operator()(LLInventoryCategory* cat,
								 LLInventoryItem* item)
{
	if(item)
	{
		if((item->getType() == LLAssetType::AT_CLOTHING)
		   || (item->getType() == LLAssetType::AT_BODYPART))
		{
			return TRUE;
		}
	}
	return FALSE;
}

LLFindWearablesEx::LLFindWearablesEx(bool is_worn, bool include_body_parts)
:	mIsWorn(is_worn)
,	mIncludeBodyParts(include_body_parts)
{}

bool LLFindWearablesEx::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem) return false;

	// Skip non-wearables.
	if (!vitem->isWearableType() && vitem->getType() != LLAssetType::AT_OBJECT && vitem->getType() != LLAssetType::AT_GESTURE)
	{
		return false;
	}

	// Skip body parts if requested.
	if (!mIncludeBodyParts && vitem->getType() == LLAssetType::AT_BODYPART)
	{
		return false;
	}

	// Skip broken links.
	if (vitem->getIsBrokenLink())
	{
		return false;
	}

	return (bool) get_is_item_worn(item->getUUID()) == mIsWorn;
}

bool LLFindWearablesOfType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (!item) return false;
	if (item->getType() != LLAssetType::AT_CLOTHING &&
		item->getType() != LLAssetType::AT_BODYPART)
	{
		return false;
	}

	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem || vitem->getWearableType() != mWearableType) return false;

	return true;
}

void LLFindWearablesOfType::setType(LLWearableType::EType type)
{
	mWearableType = type;
}

bool LLFindNonRemovableObjects::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (item)
	{
		return !get_is_item_removable(&gInventory, item->getUUID());
	}
	if (cat)
	{
		return !get_is_category_removable(&gInventory, cat->getUUID());
	}

	LL_WARNS() << "Not a category and not an item?" << LL_ENDL;
	return false;
}

///----------------------------------------------------------------------------
/// LLAssetIDMatches 
///----------------------------------------------------------------------------
bool LLAssetIDMatches ::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	return (item && item->getAssetUUID() == mAssetID);
}

///----------------------------------------------------------------------------
/// LLLinkedItemIDMatches 
///----------------------------------------------------------------------------
bool LLLinkedItemIDMatches::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	return (item && 
			(item->getIsLinkType()) &&
			(item->getLinkedUUID() == mBaseItemID)); // A linked item's assetID will be the compared-to item's itemID.
}

void LLSaveFolderState::setApply(BOOL apply)
{
	mApply = apply; 
	// before generating new list of open folders, clear the old one
	if(!apply) 
	{
		clearOpenFolders(); 
	}
}

void LLSaveFolderState::doFolder(LLFolderViewFolder* folder)
{
	LLInvFVBridge* bridge = (LLInvFVBridge*)folder->getListener();
	if(!bridge) return;
	
	if(mApply)
	{
		// we're applying the open state
		LLUUID id(bridge->getUUID());
		if(mOpenFolders.find(id) != mOpenFolders.end())
		{
			if (!folder->isOpen())
			{
				folder->setOpen(TRUE);
			}
		}
		else
		{
			// keep selected filter in its current state, this is less jarring to user
			if (!folder->isSelected() && folder->isOpen())
			{
				folder->setOpen(FALSE);
			}
		}
	}
	else
	{
		// we're recording state at this point
		if(folder->isOpen())
		{
			mOpenFolders.insert(bridge->getUUID());
		}
	}
}

void LLOpenFilteredFolders::doItem(LLFolderViewItem *item)
{
	if (item->getFiltered())
	{
		item->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
}

void LLOpenFilteredFolders::doFolder(LLFolderViewFolder* folder)
{
	if (folder->getFiltered() && folder->getParentFolder())
	{
		folder->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
	// if this folder didn't pass the filter, and none of its descendants did
	else if (!folder->getFiltered() && !folder->hasFilteredDescendants())
	{
		folder->setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_NO);
	}
}

void LLSelectFirstFilteredItem::doItem(LLFolderViewItem *item)
{
	if (item->getFiltered() && !mItemSelected)
	{
		item->getRoot()->setSelection(item, FALSE, FALSE);
		if (item->getParentFolder())
		{
			item->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
		}
		item->getRoot()->scrollToShowSelection();
		mItemSelected = TRUE;
	}
}

void LLSelectFirstFilteredItem::doFolder(LLFolderViewFolder* folder)
{
	if (folder->getFiltered() && !mItemSelected)
	{
		folder->getRoot()->setSelection(folder, FALSE, FALSE);
		if (folder->getParentFolder())
		{
			folder->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
		}
		folder->getRoot()->scrollToShowSelection();
		mItemSelected = TRUE;
	}
}

void LLOpenFoldersWithSelection::doItem(LLFolderViewItem *item)
{
	if (item->getParentFolder() && item->isSelected())
	{
		item->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
}

void LLOpenFoldersWithSelection::doFolder(LLFolderViewFolder* folder)
{
	if (folder->getParentFolder() && folder->isSelected())
	{
		folder->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
}
