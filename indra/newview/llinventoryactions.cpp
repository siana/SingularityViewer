/** 
 * @file llinventoryactions.cpp
 * @brief Implementation of the actions associated with menu items.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llfoldervieweventlistener.h"
#include "llimview.h"
#include "llinventorybridge.h"
#include "llinventoryclipboard.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventorypanel.h"
#include "llmakeoutfitdialog.h"
#include "llnotificationsutil.h"
#include "llpanelmaininventory.h"
#include "llpanelobjectinventory.h"
#include "llpreview.h" // For LLMultiPreview
#include "lltrans.h"
#include "llvoavatarself.h"
#include "llnotifications.h"

extern LLUUID gAgentID;

using namespace LLOldEvents;

typedef LLMemberListener<LLPanelObjectInventory> object_inventory_listener_t;
typedef LLMemberListener<LLInventoryView> inventory_listener_t;
typedef LLMemberListener<LLInventoryPanel> inventory_panel_listener_t;

bool doToSelected(LLFolderView* folder, std::string action)
{
	LLInventoryModel* model = &gInventory;
	if ("rename" == action)
	{
		folder->startRenamingSelectedItem();
		return true;
	}
	else if ("delete" == action)
	{
		folder->removeSelectedItems();
		return true;
	}
	else if ("copy" == action || "cut" == action)
	{	
		LLInventoryClipboard::instance().reset();
	}

	std::set<LLUUID> selected_items = folder->getSelectionList();

	LLMultiFloater* multi_floaterp = NULL;

	if (("task_open" == action  || "open" == action) && selected_items.size() > 1)
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);

		multi_floaterp = new LLMultiPreview(LLRect(left, top, left + 300, top - 100));
		gFloaterView->addChild(multi_floaterp);

		LLFloater::setFloaterHost(multi_floaterp);
	}
	else if (("task_properties" == action || "properties" == action) && selected_items.size() > 1)
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);

		multi_floaterp = new LLMultiProperties(LLRect(left, top, left + 100, top - 100));
		gFloaterView->addChild(multi_floaterp);

		LLFloater::setFloaterHost(multi_floaterp);
	}

	std::set<LLUUID>::iterator set_iter;

	for (set_iter = selected_items.begin(); set_iter != selected_items.end(); ++set_iter)
	{
		LLFolderViewItem* folder_item = folder->getItemByID(*set_iter);
		if(!folder_item) continue;
		LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getListener();
		if(!bridge) continue;

		bridge->performAction(model, action);
	}

	LLFloater::setFloaterHost(NULL);
	if (multi_floaterp)
	{
		multi_floaterp->open();
	}

	return true;
}

class LLDoToSelectedPanel : public object_inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLPanelObjectInventory *panel = mPtr;
		LLFolderView* folder = panel->getRootFolder();
		if(!folder) return true;

		return doToSelected(folder, userdata.asString());
	}
};

class LLDoToSelectedFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel *panel = mPtr->getPanel();
		LLFolderView* folder = panel->getRootFolder();
		if(!folder) return true;

		return doToSelected(folder, userdata.asString());
	}
};

class LLDoToSelected : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel *panel = mPtr;
		LLFolderView* folder = panel->getRootFolder();
		if(!folder) return true;

		return doToSelected(folder, userdata.asString());
	}
};

class LLNewWindow : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLRect rect(gSavedSettings.getRect("FloaterInventoryRect"));
		S32 left = 0 , top = 0;
		gFloaterView->getNewFloaterPosition(&left, &top);
		rect.setLeftTopAndSize(left, top, rect.getWidth(), rect.getHeight());
		LLInventoryView* iv = new LLInventoryView(std::string("Inventory"),
												rect,
												mPtr->getActivePanel()->getModel());
		iv->getActivePanel()->setFilterTypes(mPtr->getActivePanel()->getFilterObjectTypes());
		iv->getActivePanel()->setFilterSubString(mPtr->getActivePanel()->getFilterSubString());
		iv->open();		/*Flawfinder: ignore*/

		// force onscreen
		gFloaterView->adjustToFitScreen(iv, FALSE);
		return true;
	}
};

class LLShowFilters : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->toggleFindOptions();
		return true;
	}
};

class LLResetFilter : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->resetFilters();
		return true;
	}
};

class LLCloseAllFolders : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->closeAllFolders();
		return true;
	}
};

class LLCloseAllFoldersFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		mPtr->getPanel()->closeAllFolders();
		return true;
	}
};

class LLEmptyTrash : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getModel();
		if(!model) return false;
		LLNotificationsUtil::add("ConfirmEmptyTrash", LLSD(), LLSD(), boost::bind(&LLEmptyTrash::callback_empty_trash, this, _1, _2));
		return true;
	}

	bool callback_empty_trash(const LLSD& notification, const LLSD& response)
	{
		S32 option = LLNotification::getSelectedOption(notification, response);
		if (option == 0) // YES
		{
			LLInventoryModel* model = mPtr->getModel();
			LLUUID trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
			model->purgeDescendentsOf(trash_id);
			model->notifyObservers();
		}
		return false;
	}
};

class LLEmptyLostAndFound : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getModel();
		if(!model) return false;
		LLNotificationsUtil::add("ConfirmEmptyLostAndFound", LLSD(), LLSD(), boost::bind(&LLEmptyLostAndFound::callback_empty_lost_and_found, this, _1, _2));
		return true;
	}

	bool callback_empty_lost_and_found(const LLSD& notification, const LLSD& response)
	{
		S32 option = LLNotification::getSelectedOption(notification, response);
		if (option == 0) // YES
		{
			LLInventoryModel* model = mPtr->getModel();
			LLUUID lost_and_found_id = model->findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
			model->purgeDescendentsOf(lost_and_found_id);
			model->notifyObservers();
		}
		return false;
	}
};

class LLEmptyTrashFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getPanel()->getModel();
		if(!model) return false;
		LLUUID trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
		model->purgeDescendentsOf(trash_id);
		model->notifyObservers();
		return true;
	}
};

void do_create(LLInventoryModel *model, LLInventoryPanel *ptr, std::string type, LLFolderBridge *self = NULL)
{
	if ("category" == type)
	{
		LLUUID category;
		if (self)
		{
			category = model->createNewCategory(self->getUUID(), LLFolderType::FT_NONE, LLStringUtil::null);
		}
		else
		{
			category = model->createNewCategory(gInventory.getRootFolderID(),
												LLFolderType::FT_NONE, LLStringUtil::null);
		}
		model->notifyObservers();
		ptr->setSelection(category, TRUE);
	}
	else if ("lsl" == type)
	{
		LLUUID parent_id = self ? self->getUUID() : model->findCategoryUUIDForType(LLFolderType::FT_LSL_TEXT);
		ptr->createNewItem(LLTrans::getString("New Script"),
							parent_id,
							LLAssetType::AT_LSL_TEXT,
							LLInventoryType::IT_LSL,
							PERM_MOVE | PERM_TRANSFER);
	}
	else if ("notecard" == type)
	{
		LLUUID parent_id = self ? self->getUUID() : model->findCategoryUUIDForType(LLFolderType::FT_NOTECARD);
		ptr->createNewItem(LLTrans::getString("New Note"),
							parent_id,
							LLAssetType::AT_NOTECARD,
							LLInventoryType::IT_NOTECARD,
							PERM_ALL);
	}
	else if ("gesture" == type)
	{
		LLUUID parent_id = self ? self->getUUID() : model->findCategoryUUIDForType(LLFolderType::FT_GESTURE);
		ptr->createNewItem(LLTrans::getString("New Gesture"),
							parent_id,
							LLAssetType::AT_GESTURE,
							LLInventoryType::IT_GESTURE,
							PERM_ALL);
	}
	else if ("outfit" == type || ("update outfit" == type && !LLAppearanceMgr::getInstance()->updateBaseOutfit())) // If updateBaseOutfit fails, prompt to make a new outfit
	{
		new LLMakeOutfitDialog(false);
		return;
	}
	else
	{
		LLAgentWearables::createWearable(LLWearableType::typeNameToType(type), false, self ? self->getUUID() : LLUUID::null);
	}
	ptr->getRootFolder()->setNeedsAutoRename(TRUE);	
}

class LLDoCreate : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getModel();
		if(!model) return false;
		std::string type = userdata.asString();
		do_create(model, mPtr, type, LLFolderBridge::sSelf.get());
		return true;
	}
};

class LLDoCreateFloater : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getPanel()->getModel();
		if(!model) return false;
		std::string type = userdata.asString();
		do_create(model, mPtr->getPanel(), type);
		return true;
	}
};

class LLSetSortBy : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		std::string sort_field = userdata.asString();
		if (sort_field == "name")
		{
			U32 order = mPtr->getActivePanel()->getSortOrder();
			mPtr->getActivePanel()->setSortOrder( order & ~LLInventoryFilter::SO_DATE );
			
			mPtr->getControl("Inventory.SortByName")->setValue( TRUE );
			mPtr->getControl("Inventory.SortByDate")->setValue( FALSE );
		}
		else if (sort_field == "date")
		{
			U32 order = mPtr->getActivePanel()->getSortOrder();
			mPtr->getActivePanel()->setSortOrder( order | LLInventoryFilter::SO_DATE );

			mPtr->getControl("Inventory.SortByName")->setValue( FALSE );
			mPtr->getControl("Inventory.SortByDate")->setValue( TRUE );
		}
		else if (sort_field == "foldersalwaysbyname")
		{
			U32 order = mPtr->getActivePanel()->getSortOrder();
			if ( order & LLInventoryFilter::SO_FOLDERS_BY_NAME )
			{
				order &= ~LLInventoryFilter::SO_FOLDERS_BY_NAME;

				mPtr->getControl("Inventory.FoldersAlwaysByName")->setValue( FALSE );
			}
			else
			{
				order |= LLInventoryFilter::SO_FOLDERS_BY_NAME;

				mPtr->getControl("Inventory.FoldersAlwaysByName")->setValue( TRUE );
			}
			mPtr->getActivePanel()->setSortOrder( order );
		}
		else if (sort_field == "systemfolderstotop")
		{
			U32 order = mPtr->getActivePanel()->getSortOrder();
			if ( order & LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP )
			{
				order &= ~LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;

				mPtr->getControl("Inventory.SystemFoldersToTop")->setValue( FALSE );
			}
			else
			{
				order |= LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;

				mPtr->getControl("Inventory.SystemFoldersToTop")->setValue( TRUE );
			}
			mPtr->getActivePanel()->setSortOrder( order );
		}

		return true;
	}
};

class LLRefreshInvModel : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryModel* model = mPtr->getPanel()->getModel();
		if(!model) return false;
		model->empty();
		LLInventoryModelBackgroundFetch::instance().start();
		return true;
	}
};

class LLSetSearchType : public inventory_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		U32 flags = mPtr->getActivePanel()->getRootFolder()->toggleSearchType(userdata.asString());
		mPtr->getControl("Inventory.SearchName")->setValue((BOOL)(flags & 1));
		mPtr->getControl("Inventory.SearchDesc")->setValue((BOOL)(flags & 2));
		mPtr->getControl("Inventory.SearchCreator")->setValue((BOOL)(flags & 4));
		return true;
	}
};

class LLBeginIMSession : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel *panel = mPtr;
		LLInventoryModel* model = panel->getModel();
		if(!model) return true;
		std::set<LLUUID> selected_items = panel->getRootFolder()->getSelectionList();

		std::string name;
		static int session_num = 1;

		LLDynamicArray<LLUUID> members;
		EInstantMessage type = IM_SESSION_CONFERENCE_START;

		for (std::set<LLUUID>::const_iterator iter = selected_items.begin(); iter != selected_items.end(); iter++)
		{

			LLUUID item = *iter;
			LLFolderViewItem* folder_item = panel->getRootFolder()->getItemByID(item);
			
			if(folder_item) 
			{
				LLFolderViewEventListener* fve_listener = folder_item->getListener();
				if (fve_listener && (fve_listener->getInventoryType() == LLInventoryType::IT_CATEGORY))
				{

					LLFolderBridge* bridge = (LLFolderBridge*)folder_item->getListener();
					if(!bridge) return true;
					LLViewerInventoryCategory* cat = bridge->getCategory();
					if(!cat) return true;
					name = cat->getName();
					LLUniqueBuddyCollector is_buddy;
					LLInventoryModel::cat_array_t cat_array;
					LLInventoryModel::item_array_t item_array;
					model->collectDescendentsIf(bridge->getUUID(),
												cat_array,
												item_array,
												LLInventoryModel::EXCLUDE_TRASH,
												is_buddy);
					S32 count = item_array.count();
					if(count > 0)
					{
						// create the session
						gIMMgr->setFloaterOpen(TRUE);

						LLAvatarTracker& at = LLAvatarTracker::instance();
						LLUUID id;
						for(S32 i = 0; i < count; ++i)
						{
							id = item_array.get(i)->getCreatorUUID();
							if(at.isBuddyOnline(id))
							{
								members.put(id);
							}
						}
					}
				}
				else
				{
					LLFolderViewItem* folder_item = panel->getRootFolder()->getItemByID(item);
					if(!folder_item) return true;
					LLInvFVBridge* listenerp = (LLInvFVBridge*)folder_item->getListener();

					if (listenerp->getInventoryType() == LLInventoryType::IT_CALLINGCARD)
					{
						LLInventoryItem* inv_item = gInventory.getItem(listenerp->getUUID());

						if (inv_item)
						{
							LLAvatarTracker& at = LLAvatarTracker::instance();
							LLUUID id = inv_item->getCreatorUUID();

							if(at.isBuddyOnline(id))
							{
								members.put(id);
							}
						}
					} //if IT_CALLINGCARD
				} //if !IT_CATEGORY
			}
		} //for selected_items	

		// the session_id is randomly generated UUID which will be replaced later
		// with a server side generated number

		if (name.empty())
		{
			name = llformat("Session %d", session_num++);
		}


		gIMMgr->addSession(
			name,
			type,
			members[0],
			members);
		
		return true;
	}
};

class LLAttachObject : public inventory_panel_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLInventoryPanel *panel = mPtr;
		LLFolderView* folder = panel->getRootFolder();
		if(!folder) return true;

		std::set<LLUUID> selected_items = folder->getSelectionList();
		LLUUID id = *selected_items.begin();

		std::string joint_name = userdata.asString();
		LLVOAvatar *avatarp = gAgentAvatarp;
		LLViewerJointAttachment* attachmentp = NULL;
		for (LLVOAvatar::attachment_map_t::iterator iter = avatarp->mAttachmentPoints.begin(); 
			 iter != avatarp->mAttachmentPoints.end(); )
		{
			LLVOAvatar::attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			if (attachment->getName() == joint_name)
			{
				attachmentp = attachment;
				break;
			}
		}
		if (!attachmentp)
		{
			return true;
		}
		if (LLViewerInventoryItem* item = (LLViewerInventoryItem*)gInventory.getItem(id))
		{
			if(gInventory.isObjectDescendentOf(id, gInventory.getRootFolderID()))
			{
				rez_attachment(item, attachmentp);	// don't replace if called from an "Attach To..." menu
			}
			else if(item->isFinished())
			{
				// must be in library. copy it to our inventory and put it on.
				LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(rez_attachment_cb, _1, attachmentp, false));
				copy_inventory_item(
					gAgentID,
					item->getPermissions().getOwner(),
					item->getUUID(),
					LLUUID::null,
					std::string(),
					cb);
			}
		}
		gFocusMgr.setKeyboardFocus(NULL);

		return true;
	}
};

void init_object_inventory_panel_actions(LLPanelObjectInventory *panel)
{
	(new LLDoToSelectedPanel())->registerListener(panel, "Inventory.DoToSelected");
}

void init_inventory_actions(LLInventoryView *floater)
{
	(new LLDoToSelectedFloater())->registerListener(floater, "Inventory.DoToSelected");
	(new LLCloseAllFoldersFloater())->registerListener(floater, "Inventory.CloseAllFolders");
	(new LLEmptyTrashFloater())->registerListener(floater, "Inventory.EmptyTrash");
	(new LLDoCreateFloater())->registerListener(floater, "Inventory.DoCreate");

	(new LLNewWindow())->registerListener(floater, "Inventory.NewWindow");
	(new LLShowFilters())->registerListener(floater, "Inventory.ShowFilters");
	(new LLResetFilter())->registerListener(floater, "Inventory.ResetFilter");
	(new LLSetSortBy())->registerListener(floater, "Inventory.SetSortBy");

	(new LLSetSearchType())->registerListener(floater, "Inventory.SetSearchType");
}

void init_inventory_panel_actions(LLInventoryPanel *panel)
{
	(new LLDoToSelected())->registerListener(panel, "Inventory.DoToSelected");
	(new LLAttachObject())->registerListener(panel, "Inventory.AttachObject");
	(new LLCloseAllFolders())->registerListener(panel, "Inventory.CloseAllFolders");
	(new LLEmptyTrash())->registerListener(panel, "Inventory.EmptyTrash");
	(new LLEmptyLostAndFound())->registerListener(panel, "Inventory.EmptyLostAndFound");
	(new LLDoCreate())->registerListener(panel, "Inventory.DoCreate");
	(new LLBeginIMSession())->registerListener(panel, "Inventory.BeginIMSession");
}
