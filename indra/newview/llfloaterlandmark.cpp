/** 
 * @file llfloaterlandmark.cpp
 * @author Richard Nelson, James Cook, Sam Kolb
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterlandmark.h"

#include "llagent.h"
#include "llagentui.h"
#include "llcheckboxctrl.h"
#include "llfiltereditor.h"
#include "llfolderview.h"
#include "llfoldervieweventlistener.h"
#include "llinventory.h"
#include "llinventoryfunctions.h"
#include "llinventorypanel.h"
#include "llparcel.h"
#include "llpermissions.h"
#include "llsaleinfo.h"
#include "llviewerinventory.h"
#include "llviewerparcelmgr.h"

#include "llnotificationsutil.h"

#include "llviewerwindow.h"		// alertXml
#include "llviewercontrol.h"
#include "lluictrlfactory.h"

#include "roles_constants.h"	// GP_LAND_ALLOW_LANDMARK


static const F32 CONTEXT_CONE_IN_ALPHA = 0.0f;
static const F32 CONTEXT_CONE_OUT_ALPHA = 1.f;
static const F32 CONTEXT_FADE_TIME = 0.08f;


LLFloaterLandmark::LLFloaterLandmark(const LLSD& data)
	:
	mTentativeLabel(NULL),
	mResolutionLabel(NULL),
	mIsDirty( FALSE ),
	mActive( TRUE ),
	mFilterEdit(NULL),
	mContextConeOpacity(0.f),
	mInventoryPanel(NULL),
	mSavedFolderState(NULL),
	mNoCopyLandmarkSelected( FALSE )
{
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_landmark_ctrl.xml");
}
BOOL LLFloaterLandmark::postBuild()
{
	mTentativeLabel = getChild<LLTextBox>("Multiple");

	mResolutionLabel = getChild<LLTextBox>("unknown");

	LLUICtrl* show_folders_check = getChild<LLUICtrl>("show_folders_check");
	show_folders_check->setCommitCallback(boost::bind(&LLFloaterLandmark::onShowFolders,this, _1));
	show_folders_check->setVisible(FALSE);
	
	mFilterEdit = getChild<LLFilterEditor>("inventory search editor");
	mFilterEdit->setCommitCallback(boost::bind(&LLFloaterLandmark::onFilterEdit, this, _2));
		
	mInventoryPanel = getChild<LLInventoryPanel>("inventory panel");

	if(mInventoryPanel)
	{
		U32 filter_types = 0x0;
		filter_types |= 0x1 << LLInventoryType::IT_LANDMARK;
		// filter_types |= 0x1 << LLInventoryType::IT_SNAPSHOT;

		mInventoryPanel->setFilterTypes(filter_types);
		//mInventoryPanel->setFilterPermMask(getFilterPermMask());  //Commented out due to no-copy texture loss.
		mInventoryPanel->setSelectCallback(boost::bind(&LLFloaterLandmark::onSelectionChange, this, _1, _2));
		mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
		mInventoryPanel->setAllowMultiSelect(FALSE);

		// store this filter as the default one
		mInventoryPanel->getRootFolder()->getFilter()->markDefault();

		// Commented out to stop opening all folders with textures
		mInventoryPanel->openDefaultFolderForType(LLAssetType::AT_LANDMARK);
		
		// don't put keyboard focus on selected item, because the selection callback
		// will assume that this was user input
		mInventoryPanel->setSelection(findItemID(mImageAssetID, FALSE), TAKE_FOCUS_NO);
	}

	mSavedFolderState = new LLSaveFolderState();
	mNoCopyLandmarkSelected = FALSE;
	
	getChild<LLButton>("Close")->setClickedCallback(boost::bind(&LLFloaterLandmark::onBtnClose,this));
	getChild<LLButton>("New")->setClickedCallback(boost::bind(&LLFloaterLandmark::onBtnNew,this));
	getChild<LLButton>("NewFolder")->setClickedCallback(boost::bind(&LLFloaterLandmark::onBtnNewFolder,this));
	getChild<LLButton>("Edit")->setClickedCallback(boost::bind(&LLFloaterLandmark::onBtnEdit,this));
	getChild<LLButton>("Rename")->setClickedCallback(boost::bind(&LLFloaterLandmark::onBtnRename,this));
	getChild<LLButton>("Delete")->setClickedCallback(boost::bind(&LLFloaterLandmark::onBtnDelete,this));

	setCanMinimize(FALSE);

	mSavedFolderState->setApply(FALSE);

	return true;
}

LLFloaterLandmark::~LLFloaterLandmark()
{
	delete mSavedFolderState;
	mSavedFolderState = NULL;
}

void LLFloaterLandmark::setActive( BOOL active )					
{
	mActive = active; 
}

// virtual
BOOL LLFloaterLandmark::handleDragAndDrop( 
		S32 x, S32 y, MASK mask,
		BOOL drop,
		EDragAndDropType cargo_type, void *cargo_data, 
		EAcceptance *accept,
		std::string& tooltip_msg)
{
	BOOL handled = FALSE;

	if (cargo_type == DAD_LANDMARK)
	{
		LLInventoryItem *item = (LLInventoryItem *)cargo_data;

		BOOL copy = item->getPermissions().allowCopyBy(gAgent.getID());
		BOOL mod = item->getPermissions().allowModifyBy(gAgent.getID());
		BOOL xfer = item->getPermissions().allowOperationBy(PERM_TRANSFER,
															gAgent.getID());

		PermissionMask item_perm_mask = 0;
		if (copy) item_perm_mask |= PERM_COPY;
		if (mod)  item_perm_mask |= PERM_MODIFY;
		if (xfer) item_perm_mask |= PERM_TRANSFER;
		
		//PermissionMask filter_perm_mask = getFilterPermMask();  Commented out due to no-copy texture loss.
		PermissionMask filter_perm_mask = mImmediateFilterPermMask;
		if ( (item_perm_mask & filter_perm_mask) == filter_perm_mask )
		{

			*accept = ACCEPT_YES_SINGLE;
		}
		else
		{
			*accept = ACCEPT_NO;
		}
	}
	else
	{
		*accept = ACCEPT_NO;
	}

	handled = TRUE;
	lldebugst(LLERR_USER_INPUT) << "dragAndDrop handled by LLFloaterLandmark " << getName() << llendl;

	return handled;
}

BOOL LLFloaterLandmark::handleKeyHere(KEY key, MASK mask)
{
	LLFolderView* root_folder = mInventoryPanel->getRootFolder();

	if (root_folder && mFilterEdit)
	{
		if (mFilterEdit->hasFocus() &&
		    (key == KEY_RETURN || key == KEY_DOWN) &&
		    mask == MASK_NONE)
		{
			if (!root_folder->getCurSelectedItem())
			{
				LLFolderViewItem* itemp = root_folder->getItemByID(gInventory.getRootFolderID());
				if (itemp)
				{
					root_folder->setSelection(itemp, FALSE, FALSE);
				}
			}
			root_folder->scrollToShowSelection();
			
			// move focus to inventory proper
			root_folder->setFocus(TRUE);
			
			return TRUE;
		}
		
		if (root_folder->hasFocus() && key == KEY_UP)
		{
			mFilterEdit->focusFirstItem(TRUE);
		}
	}

	return LLFloater::handleKeyHere(key, mask);
}

// virtual
void LLFloaterLandmark::onClose(bool app_quitting)
{
	destroy();
}



const LLUUID& LLFloaterLandmark::findItemID(const LLUUID& asset_id, BOOL copyable_only)
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null,
							cats,
							items,
							LLInventoryModel::INCLUDE_TRASH,
							asset_id_matches);

	if (items.count())
	{
		// search for copyable version first
		for (S32 i = 0; i < items.count(); i++)
		{
			LLInventoryItem* itemp = items[i];
			LLPermissions item_permissions = itemp->getPermissions();
			if (item_permissions.allowCopyBy(gAgent.getID(), gAgent.getGroupID()))
			{
				return itemp->getUUID();
			}
		}
		// otherwise just return first instance, unless copyable requested
		if (copyable_only)
		{
			return LLUUID::null;
		}
		else
		{
			return items[0]->getUUID();
		}
	}

	return LLUUID::null;
}

void LLFloaterLandmark::onBtnClose()
{
	mIsDirty = FALSE;
	close();
}

void LLFloaterLandmark::onBtnEdit()
{
	// There isn't one, so make a new preview
	LLViewerInventoryItem* itemp = gInventory.getItem(mImageAssetID);
	if(itemp)
	{
		open_landmark(itemp, itemp->getName(), TRUE);
	}
}

void LLFloaterLandmark::onBtnNew()
{
	LLViewerRegion* agent_region = gAgent.getRegion();
	if(!agent_region)
	{
		llwarns << "No agent region" << llendl;
		return;
	}
	LLParcel* agent_parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!agent_parcel)
	{
		llwarns << "No agent parcel" << llendl;
		return;
	}
	if (!agent_parcel->getAllowLandmark()
		&& !LLViewerParcelMgr::isParcelOwnedByAgent(agent_parcel, GP_LAND_ALLOW_LANDMARK))
	{
		LLNotificationsUtil::add("CannotCreateLandmarkNotOwner");
		return;
	}

	std::string landmark_name, landmark_desc;

	LLAgentUI::buildLocationString(landmark_name, LLAgentUI::LOCATION_FORMAT_LANDMARK);
	LLAgentUI::buildLocationString(landmark_desc, LLAgentUI::LOCATION_FORMAT_FULL);
	const LLUUID folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);

	create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
		folder_id, LLTransactionID::tnull,
		landmark_name, landmark_desc, // name, desc
		LLAssetType::AT_LANDMARK,
		LLInventoryType::IT_LANDMARK,
		NOT_WEARABLE, PERM_ALL, 
		NULL);
}

void LLFloaterLandmark::onBtnNewFolder()
{

}

void LLFloaterLandmark::onBtnDelete()
{
	LLViewerInventoryItem* item = gInventory.getItem(mImageAssetID);
	if(item)
	{
		// Move the item to the trash
		LLUUID trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
		if (item->getParentUUID() != trash_id)
		{
			LLInventoryModel::update_list_t update;
			LLInventoryModel::LLCategoryUpdate old_folder(item->getParentUUID(),-1);
			update.push_back(old_folder);
			LLInventoryModel::LLCategoryUpdate new_folder(trash_id, 1);
			update.push_back(new_folder);
			gInventory.accountForUpdate(update);

			LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
			new_item->setParent(trash_id);
			// no need to restamp it though it's a move into trash because
			// it's a brand new item already.
			new_item->updateParentOnServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
	}

	// Delete the item entirely
	/*
	item->removeFromServer();
	gInventory.deleteObject(item->getUUID());
	gInventory.notifyObservers();
	*/


}

void LLFloaterLandmark::onBtnRename()
{

}

void LLFloaterLandmark::onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action)
{
	if (items.size())
	{
		LLFolderViewItem* first_item = items.front();
		LLInventoryItem* itemp = gInventory.getItem(first_item->getListener()->getUUID());
		mNoCopyLandmarkSelected = FALSE;
		if (itemp)
		{
			if (!itemp->getPermissions().allowCopyBy(gAgent.getID()))
			{
				mNoCopyLandmarkSelected = TRUE;
			}
			mImageAssetID = itemp->getUUID();
			mIsDirty = TRUE;
		}
	}
}

void LLFloaterLandmark::onShowFolders(LLUICtrl* ctrl)
{
	LLCheckBoxCtrl* check_box = (LLCheckBoxCtrl*)ctrl;

	if (check_box->get())
	{
		mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
	}
	else
	{
		mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NO_FOLDERS);
	}
}

void LLFloaterLandmark::onFilterEdit(const LLSD& value )
{
	std::string upper_case_search_string = value.asString();
	LLStringUtil::toUpper(upper_case_search_string);

	if (upper_case_search_string.empty())
	{
		if (mInventoryPanel->getFilterSubString().empty())
		{
			// current filter and new filter empty, do nothing
			return;
		}

		mSavedFolderState->setApply(TRUE);
		mInventoryPanel->getRootFolder()->applyFunctorRecursively(*mSavedFolderState);
		// add folder with current item to list of previously opened folders
		LLOpenFoldersWithSelection opener;
		mInventoryPanel->getRootFolder()->applyFunctorRecursively(opener);
		mInventoryPanel->getRootFolder()->scrollToShowSelection();

	}
	else if (mInventoryPanel->getFilterSubString().empty())
	{
		// first letter in search term, save existing folder open state
		if (!mInventoryPanel->getRootFolder()->isFilterModified())
		{
			mSavedFolderState->setApply(FALSE);
			mInventoryPanel->getRootFolder()->applyFunctorRecursively(*mSavedFolderState);
		}
	}

	mInventoryPanel->setFilterSubString(upper_case_search_string);
}

