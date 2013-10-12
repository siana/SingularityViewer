/** 
 * @file llfloaterproperties.cpp
 * @brief A floater which shows an inventory item's properties.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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
#include "llfloaterproperties.h"

#include <algorithm>
#include <functional>
#include "llcachename.h"
#include "lldbstrings.h"
#include "llinventory.h"
#include "llinventorydefines.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llgroupactions.h"
#include "llinventorymodel.h"
#include "llinventoryobserver.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "roles_constants.h"
#include "llselectmgr.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"

#include "lluictrlfactory.h"

#include "lfsimfeaturehandler.h"
#include "hippogridmanager.h"


// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

bool can_set_export(const U32& base, const U32& own, const U32& next);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPropertiesObserver
//
// helper class to watch the inventory. 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Ugh. This can't be a singleton because it needs to remove itself
//  from the inventory observer list when destroyed, which could
//  happen after gInventory has already been destroyed if a singleton.
// Instead, do our own ref counting and create / destroy it as needed
class LLPropertiesObserver : public LLInventoryObserver
{
public:
	LLPropertiesObserver(LLFloaterProperties* floater)
		: mFloater(floater)
	{
		gInventory.addObserver(this);
	}
	virtual ~LLPropertiesObserver()
	{
		gInventory.removeObserver(this);
	}
	virtual void changed(U32 mask);
private:
	LLFloaterProperties* mFloater;
};

void LLPropertiesObserver::changed(U32 mask)
{
	// if there's a change we're interested in.
	if((mask & (LLInventoryObserver::LABEL | LLInventoryObserver::INTERNAL | LLInventoryObserver::REMOVE)) != 0)
	{
		mFloater->dirty();
	}
}



///----------------------------------------------------------------------------
/// Class LLFloaterProperties
///----------------------------------------------------------------------------

//static
LLFloaterProperties* LLFloaterProperties::find(const LLUUID& item_id, const LLUUID &object_id)
{
	return getInstance(item_id ^ object_id);
}

// static
LLFloaterProperties* LLFloaterProperties::show(const LLUUID& item_id,
											   const LLUUID& object_id)
{
	LLFloaterProperties* instance = find(item_id, object_id);
	if(instance)
	{
		if (LLFloater::getFloaterHost() && LLFloater::getFloaterHost() != instance->getHost())
		{
			// this properties window is being opened in a new context
			// needs to be rehosted
			LLFloater::getFloaterHost()->addFloater(instance, TRUE);
		}

		instance->refresh();
		instance->open();		/* Flawfinder: ignore */
	}
	return instance;
}

// Default constructor
LLFloaterProperties::LLFloaterProperties(const std::string& name, const LLRect& rect, const std::string& title, const LLUUID& item_id, const LLUUID& object_id) :
	LLFloater(name, rect, title), LLInstanceTracker<LLFloaterProperties,LLUUID>(object_id ^ item_id),
	mItemID(item_id),
	mObjectID(object_id),
	mDirty(TRUE)
{
	mPropertiesObserver = new LLPropertiesObserver(this);
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_inventory_item_properties.xml");
}

// Destroys the object
LLFloaterProperties::~LLFloaterProperties()
{
	delete mPropertiesObserver;
	mPropertiesObserver = NULL;
}

// virtual
BOOL LLFloaterProperties::postBuild()
{
	
	childSetTextArg("TextPrice", "[CURRENCY]", gHippoGridManager->getConnectedGrid()->getCurrencySymbol());
		// build the UI
	// item name & description
	getChild<LLLineEditor>("LabelItemName")->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	getChild<LLUICtrl>("LabelItemName")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitName,this));
	getChild<LLLineEditor>("LabelItemDesc")->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	getChild<LLUICtrl>("LabelItemDesc")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitDescription,this));
	// Creator information
	getChild<LLUICtrl>("BtnCreator")->setCommitCallback(boost::bind(&LLFloaterProperties::onClickCreator,this));
	// owner information
	getChild<LLUICtrl>("BtnOwner")->setCommitCallback(boost::bind(&LLFloaterProperties::onClickOwner,this));
	// last owner information
	getChild<LLUICtrl>("BtnLastOwner")->setCommitCallback(boost::bind(&LLFloaterProperties::onClickLastOwner,this));
	// acquired date
	// owner permissions
	// Permissions debug text
	// group permissions
	getChild<LLUICtrl>("CheckGroupCopy")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckGroupMod")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckGroupMove")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	// everyone permissions
	getChild<LLUICtrl>("CheckEveryoneCopy")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckEveryoneMove")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckExport")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	if (!gHippoGridManager->getCurrentGrid()->isSecondLife())
		LFSimFeatureHandler::instance().setSupportsExportCallback(boost::bind(&LLFloaterProperties::refresh, this));
	// next owner permissions
	getChild<LLUICtrl>("CheckNextOwnerModify")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	// Mark for sale or not, and sale info
	getChild<LLUICtrl>("CheckPurchase")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitSaleInfo, this));
	getChild<LLUICtrl>("RadioSaleType")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitSaleType, this));
	// "Price" label for edit
	getChild<LLUICtrl>("Edit Cost")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitSaleInfo, this));
	// The UI has been built, now fill in all the values
	refresh();

	return TRUE;
}

// virtual
void LLFloaterProperties::onOpen()
{
	refresh();
}

void LLFloaterProperties::refresh()
{
	LLInventoryItem* item = findItem();
	if(item)
	{
		refreshFromItem(item);
	}
	else
	{
		//RN: it is possible that the container object is in the middle of an inventory refresh
		// causing findItem() to fail, so just temporarily disable everything
		
		mDirty = TRUE;

		const char* enableNames[]={
			"LabelItemName",
			"LabelItemDesc",
			"LabelCreatorName",
			"BtnCreator",
			"LabelOwnerName",
			"BtnOwner",
			"LabelLastOwnerName",
			"BtnLastOwner",
			"CheckOwnerModify",
			"CheckOwnerCopy",
			"CheckOwnerTransfer",
			"CheckOwnerExport",
			"CheckGroupCopy",
			"CheckGroupMod",
			"CheckGroupMove",
			"CheckEveryoneCopy",
			"CheckEveryoneMove",
			"CheckExport",
			"CheckNextOwnerModify",
			"CheckNextOwnerCopy",
			"CheckNextOwnerTransfer",
			"CheckPurchase",
			"RadioSaleType",
			"Edit Cost"
		};
		for(size_t t=0; t<LL_ARRAY_SIZE(enableNames); ++t)
		{
			getChildView(enableNames[t])->setEnabled(false);
		}
		const char* hideNames[]={
			"BaseMaskDebug",
			"OwnerMaskDebug",
			"GroupMaskDebug",
			"EveryoneMaskDebug",
			"NextMaskDebug"
		};
		for(size_t t=0; t<LL_ARRAY_SIZE(hideNames); ++t)
		{
			getChildView(hideNames[t])->setVisible(false);
		}
	}
}

void LLFloaterProperties::draw()
{
	if (mDirty)
	{
		// RN: clear dirty first because refresh can set dirty to TRUE
		mDirty = FALSE;
		refresh();
	}

	LLFloater::draw();
}

void LLFloaterProperties::refreshFromItem(LLInventoryItem* item)
{
	////////////////////////
	// PERMISSIONS LOOKUP //
	////////////////////////

	// do not enable the UI for incomplete items.
	LLViewerInventoryItem* i = (LLViewerInventoryItem*)item;
	BOOL is_complete = i->isFinished();
	const BOOL cannot_restrict_permissions = LLInventoryType::cannotRestrictPermissions(i->getInventoryType());
	const BOOL is_calling_card = (i->getInventoryType() == LLInventoryType::IT_CALLINGCARD);
	const LLPermissions& perm = item->getPermissions();
	const BOOL can_agent_manipulate = gAgent.allowOperation(PERM_OWNER, perm, 
															GP_OBJECT_MANIPULATE);
	const BOOL can_agent_sell = gAgent.allowOperation(PERM_OWNER, perm, 
													  GP_OBJECT_SET_SALE) &&
		!cannot_restrict_permissions;
	const BOOL is_link = i->getIsLinkType();

	// You need permission to modify the object to modify an inventory
	// item in it.
	LLViewerObject* object = NULL;
	if(!mObjectID.isNull()) object = gObjectList.findObject(mObjectID);
	BOOL is_obj_modify = TRUE;
	if(object)
	{
		is_obj_modify = object->permOwnerModify();
	}

	//////////////////////
	// ITEM NAME & DESC //
	//////////////////////
	BOOL is_modifiable = gAgent.allowOperation(PERM_MODIFY, perm,
											   GP_OBJECT_MANIPULATE)
		&& is_obj_modify && is_complete;

	getChildView("LabelItemNameTitle")->setEnabled(TRUE);
	getChildView("LabelItemName")->setEnabled(is_modifiable && !is_calling_card); // for now, don't allow rename of calling cards
	getChild<LLUICtrl>("LabelItemName")->setValue(item->getName());
	getChildView("LabelItemDescTitle")->setEnabled(TRUE);
	getChildView("LabelItemDesc")->setEnabled(is_modifiable);
	getChildView("IconLocked")->setVisible(!is_modifiable);
	getChild<LLUICtrl>("LabelItemDesc")->setValue(item->getDescription());

	//////////////////
	// CREATOR NAME //
	//////////////////
	if(!gCacheName) return;
	if(!gAgent.getRegion()) return;

	if (item->getCreatorUUID().notNull())
	{
		std::string name;
		gCacheName->getFullName(item->getCreatorUUID(), name);
		getChildView("BtnCreator")->setEnabled(TRUE);
		getChildView("LabelCreatorTitle")->setEnabled(TRUE);
		getChildView("LabelCreatorName")->setEnabled(TRUE);
		getChild<LLUICtrl>("LabelCreatorName")->setValue(name);
	}
	else
	{
		getChildView("BtnCreator")->setEnabled(FALSE);
		getChildView("LabelCreatorTitle")->setEnabled(FALSE);
		getChildView("LabelCreatorName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelCreatorName")->setValue(getString("unknown"));
	}

	if (perm.getLastOwner().notNull())
	{
		std::string name;
		gCacheName->getFullName(perm.getLastOwner(), name);
		getChildView("LabelLastOwnerTitle")->setEnabled(true);
		getChildView("LabelLastOwnerName")->setEnabled(true);
		getChild<LLUICtrl>("LabelLastOwnerName")->setValue(name);
	}
	else
	{
		getChildView("LabelLastOwnerTitle")->setEnabled(false);
		getChildView("LabelLastOwnerName")->setEnabled(false);
		getChild<LLUICtrl>("LabelLastOwnerName")->setValue(getString("unknown"));
	}

	////////////////
	// OWNER NAME //
	////////////////
	if(perm.isOwned())
	{
		std::string name;
		if (perm.isGroupOwned())
		{
			gCacheName->getGroupName(perm.getGroup(), name);
		}
		else
		{
			gCacheName->getFullName(perm.getOwner(), name);
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
			{
				name = RlvStrings::getAnonym(name);
			}
// [/RLVa:KB]
		}
		getChildView("BtnOwner")->setEnabled(TRUE);
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-1.0.0e
		getChildView("BtnOwner")->setEnabled(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES));
// [/RLVa:KB]
		getChildView("LabelOwnerTitle")->setEnabled(TRUE);
		getChildView("LabelOwnerName")->setEnabled(TRUE);
		getChild<LLUICtrl>("LabelOwnerName")->setValue(name);
	}
	else
	{
		getChildView("BtnOwner")->setEnabled(FALSE);
		getChildView("LabelOwnerTitle")->setEnabled(FALSE);
		getChildView("LabelOwnerName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelOwnerName")->setValue(getString("public"));
	}
	
	//////////////////
	// ACQUIRE DATE //
	//////////////////

	// *TODO: Localize / translate this
	time_t time_utc = item->getCreationDate();
	if (0 == time_utc)
	{
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(getString("unknown"));
	}
	else
	{
		std::string timeStr;
		timeToFormattedString(time_utc, gSavedSettings.getString("TimestampFormat"), timeStr);
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(timeStr);
	}

	///////////////////////
	// OWNER PERMISSIONS //
	///////////////////////
	if(can_agent_manipulate)
	{
		getChild<LLUICtrl>("OwnerLabel")->setValue(getString("you_can"));
	}
	else
	{
		getChild<LLUICtrl>("OwnerLabel")->setValue(getString("owner_can"));
	}

	U32 base_mask		= perm.getMaskBase();
	U32 owner_mask		= perm.getMaskOwner();
	U32 group_mask		= perm.getMaskGroup();
	U32 everyone_mask	= perm.getMaskEveryone();
	U32 next_owner_mask	= perm.getMaskNextOwner();

	getChildView("OwnerLabel")->setEnabled(TRUE);
	getChildView("CheckOwnerModify")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerModify")->setValue(LLSD((BOOL)(owner_mask & PERM_MODIFY)));
	getChildView("CheckOwnerCopy")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerCopy")->setValue(LLSD((BOOL)(owner_mask & PERM_COPY)));
	getChildView("CheckOwnerTransfer")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerTransfer")->setValue(LLSD((BOOL)(owner_mask & PERM_TRANSFER)));

	bool supports_export = LFSimFeatureHandler::instance().simSupportsExport();
	getChildView("CheckOwnerExport")->setEnabled(false);
	getChild<LLUICtrl>("CheckOwnerExport")->setValue(LLSD((BOOL)(supports_export && owner_mask & PERM_EXPORT)));
	if (!gHippoGridManager->getCurrentGrid()->isSecondLife())
		getChildView("CheckOwnerExport")->setVisible(false);

	///////////////////////
	// DEBUG PERMISSIONS //
	///////////////////////

	if( gSavedSettings.getBOOL("DebugPermissions") )
	{
		BOOL slam_perm 			= FALSE;
		BOOL overwrite_group	= FALSE;
		BOOL overwrite_everyone	= FALSE;

		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = item->getFlags();
			slam_perm 			= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
			overwrite_everyone	= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
			overwrite_group		= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		
		std::string perm_string;

		perm_string = "B: ";
		perm_string += mask_to_string(base_mask);
		if (!supports_export && base_mask & PERM_EXPORT) // Hide Export when not available
			perm_string.erase(perm_string.find_last_of("E"));
		getChild<LLUICtrl>("BaseMaskDebug")->setValue(perm_string);
		getChildView("BaseMaskDebug")->setVisible(TRUE);
		
		perm_string = "O: ";
		perm_string += mask_to_string(owner_mask);
		if (!supports_export && owner_mask & PERM_EXPORT) // Hide Export when not available
			perm_string.erase(perm_string.find_last_of("E"));
		getChild<LLUICtrl>("OwnerMaskDebug")->setValue(perm_string);
		getChildView("OwnerMaskDebug")->setVisible(TRUE);
		
		perm_string = "G";
		perm_string += overwrite_group ? "*: " : ": ";
		perm_string += mask_to_string(group_mask);
		getChild<LLUICtrl>("GroupMaskDebug")->setValue(perm_string);
		getChildView("GroupMaskDebug")->setVisible(TRUE);
		
		perm_string = "E";
		perm_string += overwrite_everyone ? "*: " : ": ";
		perm_string += mask_to_string(everyone_mask);
		if (!supports_export && everyone_mask & PERM_EXPORT) // Hide Export when not available
			perm_string.erase(perm_string.find_last_of("E"));
		getChild<LLUICtrl>("EveryoneMaskDebug")->setValue(perm_string);
		getChildView("EveryoneMaskDebug")->setVisible(TRUE);
			
		perm_string = "N";
		perm_string += slam_perm ? "*: " : ": ";
		perm_string += mask_to_string(next_owner_mask);
		getChild<LLUICtrl>("NextMaskDebug")->setValue(perm_string);
		getChildView("NextMaskDebug")->setVisible(TRUE);
	}
	else
	{
		getChildView("BaseMaskDebug")->setVisible(FALSE);
		getChildView("OwnerMaskDebug")->setVisible(FALSE);
		getChildView("GroupMaskDebug")->setVisible(FALSE);
		getChildView("EveryoneMaskDebug")->setVisible(FALSE);
		getChildView("NextMaskDebug")->setVisible(FALSE);
	}

	/////////////
	// SHARING //
	/////////////

	// Check for ability to change values.
	if (!is_link && is_obj_modify && can_agent_manipulate)
	{
		getChild<LLUICtrl>("GroupLabel")->setEnabled(TRUE);
		getChild<LLUICtrl>("CheckGroupCopy")->setEnabled((owner_mask & (PERM_TRANSFER|PERM_COPY)) == (PERM_TRANSFER|PERM_COPY));
		getChild<LLUICtrl>("CheckGroupMod")->setEnabled(owner_mask & PERM_MODIFY);
		getChild<LLUICtrl>("CheckGroupMove")->setEnabled(TRUE);
		getChild<LLUICtrl>("EveryoneLabel")->setEnabled(TRUE);
		getChild<LLUICtrl>("CheckEveryoneCopy")->setEnabled((owner_mask & (PERM_TRANSFER|PERM_COPY)) == (PERM_TRANSFER|PERM_COPY));
		getChild<LLUICtrl>("CheckEveryoneMove")->setEnabled(TRUE);
	}
	else
	{
		getChild<LLUICtrl>("GroupLabel")->setEnabled(FALSE);
		getChild<LLUICtrl>("CheckGroupCopy")->setEnabled(FALSE);
		getChild<LLUICtrl>("CheckGroupMod")->setEnabled(FALSE);
		getChild<LLUICtrl>("CheckGroupMove")->setEnabled(FALSE);
		getChild<LLUICtrl>("EveryoneLabel")->setEnabled(FALSE);
		getChild<LLUICtrl>("CheckEveryoneCopy")->setEnabled(FALSE);
		getChild<LLUICtrl>("CheckEveryoneMove")->setEnabled(FALSE);
	}
	getChild<LLUICtrl>("CheckExport")->setEnabled(supports_export && item->getType() != LLAssetType::AT_OBJECT && gAgentID == item->getCreatorUUID()
									&& can_set_export(base_mask, owner_mask, next_owner_mask));

	// Set values.
	BOOL is_group_copy = (group_mask & PERM_COPY) ? TRUE : FALSE;
	BOOL is_group_modify = (group_mask & PERM_MODIFY) ? TRUE : FALSE;
	BOOL is_group_move = (group_mask & PERM_MOVE) ? TRUE : FALSE;

	getChild<LLUICtrl>("CheckGroupCopy")->setValue(LLSD((BOOL)is_group_copy));
	getChild<LLUICtrl>("CheckGroupMod")->setValue(LLSD((BOOL)is_group_modify));
	getChild<LLUICtrl>("CheckGroupMove")->setValue(LLSD((BOOL)is_group_move));
	
	getChild<LLUICtrl>("CheckEveryoneCopy")->setValue(LLSD((BOOL)(everyone_mask & PERM_COPY)));
	getChild<LLUICtrl>("CheckEveryoneMove")->setValue(LLSD((BOOL)(everyone_mask & PERM_MOVE)));
	getChild<LLUICtrl>("CheckExport")->setValue(LLSD((BOOL)(supports_export && everyone_mask & PERM_EXPORT)));

	///////////////
	// SALE INFO //
	///////////////

	const LLSaleInfo& sale_info = item->getSaleInfo();
	BOOL is_for_sale = sale_info.isForSale();
	// Check for ability to change values.
	if (is_obj_modify && can_agent_sell 
		&& gAgent.allowOperation(PERM_TRANSFER, perm, GP_OBJECT_MANIPULATE))
	{
		getChildView("SaleLabel")->setEnabled(is_complete);
		getChildView("CheckPurchase")->setEnabled(is_complete);

		bool no_export = !(everyone_mask & PERM_EXPORT); // Next owner perms can't be changed if set
		getChildView("NextOwnerLabel")->setEnabled(no_export);
		getChildView("CheckNextOwnerModify")->setEnabled(no_export && (base_mask & PERM_MODIFY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerCopy")->setEnabled(no_export && (base_mask & PERM_COPY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerTransfer")->setEnabled(no_export && (next_owner_mask & PERM_COPY) && !cannot_restrict_permissions);

		getChildView("RadioSaleType")->setEnabled(is_complete && is_for_sale);
		getChildView("TextPrice")->setEnabled(is_complete && is_for_sale);
		getChildView("Edit Cost")->setEnabled(is_complete && is_for_sale);
	}
	else
	{
		getChildView("SaleLabel")->setEnabled(FALSE);
		getChildView("CheckPurchase")->setEnabled(FALSE);

		getChildView("NextOwnerLabel")->setEnabled(FALSE);
		getChildView("CheckNextOwnerModify")->setEnabled(FALSE);
		getChildView("CheckNextOwnerCopy")->setEnabled(FALSE);
		getChildView("CheckNextOwnerTransfer")->setEnabled(FALSE);

		getChildView("RadioSaleType")->setEnabled(FALSE);
		getChildView("TextPrice")->setEnabled(FALSE);
		getChildView("Edit Cost")->setEnabled(FALSE);
	}

	// Set values.
	getChild<LLUICtrl>("CheckPurchase")->setValue(is_for_sale);
	getChildView("Edit Cost")->setEnabled(is_for_sale);
	getChild<LLUICtrl>("CheckNextOwnerModify")->setValue(LLSD(BOOL(next_owner_mask & PERM_MODIFY)));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setValue(LLSD(BOOL(next_owner_mask & PERM_COPY)));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setValue(LLSD(BOOL(next_owner_mask & PERM_TRANSFER)));

	LLRadioGroup* radioSaleType = getChild<LLRadioGroup>("RadioSaleType");
	if (is_for_sale)
	{
		radioSaleType->setSelectedIndex((S32)sale_info.getSaleType() - 1);
		S32 numerical_price;
		numerical_price = sale_info.getSalePrice();
		getChild<LLUICtrl>("Edit Cost")->setValue(llformat("%d",numerical_price));
	}
	else
	{
		radioSaleType->setSelectedIndex(-1);
		getChild<LLUICtrl>("Edit Cost")->setValue(llformat("%d",0));
	}
}

void LLFloaterProperties::onClickCreator()
{
	LLInventoryItem* item = findItem();
	if(!item) return;
	LLAvatarActions::showProfile(item->getCreatorUUID());
}

// static
void LLFloaterProperties::onClickOwner()
{
	LLInventoryItem* item = findItem();
	if(!item) return;
	if(item->getPermissions().isGroupOwned())
	{
		LLGroupActions::show(item->getPermissions().getGroup());
	}
	else
	{
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
		if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
// [/RLVa:KB]
		{
			LLAvatarActions::showProfile(item->getPermissions().getOwner());
		}
	}
}

void LLFloaterProperties::onClickLastOwner()
{
	if (const LLInventoryItem* item = findItem()) LLAvatarActions::showProfile(item->getPermissions().getLastOwner());
}

// static
void LLFloaterProperties::onCommitName()
{
	//llinfos << "LLFloaterProperties::onCommitName()" << llendl;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item)
	{
		return;
	}
	LLLineEditor* labelItemName = getChild<LLLineEditor>("LabelItemName");

	if(labelItemName&&
	   (item->getName() != labelItemName->getText()) && 
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)) )
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->rename(labelItemName->getText());
		if(mObjectID.isNull())
		{
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
}

void LLFloaterProperties::onCommitDescription()
{
	//llinfos << "LLFloaterProperties::onCommitDescription()" << llendl;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item) return;

	LLLineEditor* labelItemDesc = getChild<LLLineEditor>("LabelItemDesc");
	if(!labelItemDesc)
	{
		return;
	}
	if((item->getDescription() != labelItemDesc->getText()) && 
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		new_item->setDescription(labelItemDesc->getText());
		if(mObjectID.isNull())
		{
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
}

void LLFloaterProperties::onCommitPermissions()
{
	//llinfos << "LLFloaterProperties::onCommitPermissions()" << llendl;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item) return;
	LLPermissions perm(item->getPermissions());


	LLCheckBoxCtrl* CheckGroupCopy = getChild<LLCheckBoxCtrl>("CheckGroupCopy");
	if(CheckGroupCopy)
	{
		perm.setGroupBits(gAgent.getID(), gAgent.getGroupID(),
						CheckGroupCopy->get(), PERM_COPY);
	}
	LLCheckBoxCtrl* CheckGroupMod = getChild<LLCheckBoxCtrl>("CheckGroupMod");
	if(CheckGroupMod)
	{
		perm.setGroupBits(gAgent.getID(), gAgent.getGroupID(),
						CheckGroupMod->get(), PERM_MODIFY);
	}
	LLCheckBoxCtrl* CheckGroupMove = getChild<LLCheckBoxCtrl>("CheckGroupMove");
	if(CheckGroupMove)
	{
		perm.setGroupBits(gAgent.getID(), gAgent.getGroupID(),
						CheckGroupMove->get(), PERM_MOVE);
	}

	LLCheckBoxCtrl* CheckEveryoneMove = getChild<LLCheckBoxCtrl>("CheckEveryoneMove");
	if(CheckEveryoneMove)
	{
		perm.setEveryoneBits(gAgent.getID(), gAgent.getGroupID(),
						 CheckEveryoneMove->get(), PERM_MOVE);
	}
	LLCheckBoxCtrl* CheckEveryoneCopy = getChild<LLCheckBoxCtrl>("CheckEveryoneCopy");
	if(CheckEveryoneCopy)
	{
		perm.setEveryoneBits(gAgent.getID(), gAgent.getGroupID(),
						 CheckEveryoneCopy->get(), PERM_COPY);
	}
	LLCheckBoxCtrl* CheckExport = getChild<LLCheckBoxCtrl>("CheckExport");
	if(CheckExport)
	{
		perm.setEveryoneBits(gAgent.getID(), gAgent.getGroupID(), CheckExport->get(), PERM_EXPORT);
	}

	LLCheckBoxCtrl* CheckNextOwnerModify = getChild<LLCheckBoxCtrl>("CheckNextOwnerModify");
	if(CheckNextOwnerModify)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							CheckNextOwnerModify->get(), PERM_MODIFY);
	}
	LLCheckBoxCtrl* CheckNextOwnerCopy = getChild<LLCheckBoxCtrl>("CheckNextOwnerCopy");
	if(CheckNextOwnerCopy)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							CheckNextOwnerCopy->get(), PERM_COPY);
	}
	LLCheckBoxCtrl* CheckNextOwnerTransfer = getChild<LLCheckBoxCtrl>("CheckNextOwnerTransfer");
	if(CheckNextOwnerTransfer)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							CheckNextOwnerTransfer->get(), PERM_TRANSFER);
	}
	if(perm != item->getPermissions()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setPermissions(perm);
		U32 flags = new_item->getFlags();
		// If next owner permissions have changed (and this is an object)
		// then set the slam permissions flag so that they are applied on rez.
		if((perm.getMaskNextOwner()!=item->getPermissions().getMaskNextOwner())
		   && (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
		}
		// If everyone permissions have changed (and this is an object)
		// then set the overwrite everyone permissions flag so they
		// are applied on rez.
		if ((perm.getMaskEveryone()!=item->getPermissions().getMaskEveryone())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
		}
		// If group permissions have changed (and this is an object)
		// then set the overwrite group permissions flag so they
		// are applied on rez.
		if ((perm.getMaskGroup()!=item->getPermissions().getMaskGroup())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		new_item->setFlags(flags);
		if(mObjectID.isNull())
		{
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

// static
void LLFloaterProperties::onCommitSaleInfo()
{
	//llinfos << "LLFloaterProperties::onCommitSaleInfo()" << llendl;
	updateSaleInfo();
}

// static
void LLFloaterProperties::onCommitSaleType()
{
	//llinfos << "LLFloaterProperties::onCommitSaleType()" << llendl;
	updateSaleInfo();
}

void LLFloaterProperties::updateSaleInfo()
{
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item) return;
	LLSaleInfo sale_info(item->getSaleInfo());
	if(!gAgent.allowOperation(PERM_TRANSFER, item->getPermissions(), GP_OBJECT_SET_SALE))
	{
		getChild<LLUICtrl>("CheckPurchase")->setValue(LLSD((BOOL)FALSE));
	}

	if((BOOL)getChild<LLUICtrl>("CheckPurchase")->getValue())
	{
		// turn on sale info
		LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_COPY;
	
		LLRadioGroup* RadioSaleType = getChild<LLRadioGroup>("RadioSaleType");
		if(RadioSaleType)
		{
			switch (RadioSaleType->getSelectedIndex())
			{
			case 0:
				sale_type = LLSaleInfo::FS_ORIGINAL;
				break;
			case 1:
				sale_type = LLSaleInfo::FS_COPY;
				break;
			case 2:
				sale_type = LLSaleInfo::FS_CONTENTS;
				break;
			default:
				sale_type = LLSaleInfo::FS_COPY;
				break;
			}
		}

		if (sale_type == LLSaleInfo::FS_COPY 
			&& !gAgent.allowOperation(PERM_COPY, item->getPermissions(), 
									  GP_OBJECT_SET_SALE))
		{
			sale_type = LLSaleInfo::FS_ORIGINAL;
		}

	     
		
		S32 price = -1;
		price =  getChild<LLUICtrl>("Edit Cost")->getValue().asInteger();;

		// Invalid data - turn off the sale
		if (price < 0)
		{
			sale_type = LLSaleInfo::FS_NOT;
			price = 0;
		}

		sale_info.setSaleType(sale_type);
		sale_info.setSalePrice(price);
	}
	else
	{
		sale_info.setSaleType(LLSaleInfo::FS_NOT);
	}
	if(sale_info != item->getSaleInfo()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		// Force an update on the sale price at rez
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = new_item->getFlags();
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_SALE;
			new_item->setFlags(flags);
		}

		new_item->setSaleInfo(sale_info);
		if(mObjectID.isNull())
		{
			// This is in the agent's inventory.
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			// This is in an object's contents.
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

LLInventoryItem* LLFloaterProperties::findItem() const
{
	LLInventoryItem* item = NULL;
	if(mObjectID.isNull())
	{
		// it is in agent inventory
		item = gInventory.getItem(mItemID);
	}
	else
	{
		LLViewerObject* object = gObjectList.findObject(mObjectID);
		if(object)
		{
			item = (LLInventoryItem*)object->getInventoryObject(mItemID);
		}
	}
	return item;
}

//static
void LLFloaterProperties::closeByID(const LLUUID& item_id, const LLUUID &object_id)
{
	LLFloaterProperties* floaterp = getInstance(item_id ^ object_id);
	if (floaterp)
	{
		floaterp->close();
	}
}

//static
void LLFloaterProperties::dirtyAll()
{
	for(instance_iter it = beginInstances();it!=endInstances();++it)
	{
		it->dirty();
	}
}

///----------------------------------------------------------------------------
/// LLMultiProperties
///----------------------------------------------------------------------------

LLMultiProperties::LLMultiProperties(const LLRect &rect) : LLMultiFloater(LLTrans::getString("MultiPropertiesTitle"), rect)
{
}

///----------------------------------------------------------------------------
/// Local function definitions
///----------------------------------------------------------------------------
