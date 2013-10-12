/**
 * @file llmakeoutfitdialog.cpp
 * @brief The Make Outfit Dialog, triggered by "Make Outfit" and similar UICtrls.
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

#include "llmakeoutfitdialog.h"

#include "llappearancemgr.h"
#include "lluictrlfactory.h"
#include "llvoavatarself.h"
#include "llagent.h"
#include "llviewerregion.h"

#include "hippogridmanager.h"

LLMakeOutfitDialog::LLMakeOutfitDialog(bool modal) : LLModalDialog(LLStringUtil::null, 515, 510, modal)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_new_outfit_dialog.xml");

	setCanClose(!modal);
	setCanMinimize(!modal);

	// Build list of check boxes
	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		std::string name = std::string("checkbox_") + LLWearableType::getTypeLabel((LLWearableType::EType)i);
		mCheckBoxList.push_back(std::make_pair(name, i));
		// Hide undergarments from teens
		if (gAgent.isTeen() && ((LLWearableType::WT_UNDERSHIRT == (LLWearableType::EType)i) || (LLWearableType::WT_UNDERPANTS == (LLWearableType::EType)i)))
		{
			childSetVisible(name, false); // Lest they know what's beyond their reach.
		}
		else
		{
			bool enabled = gAgentWearables.getWearableCount((LLWearableType::EType)i);	// TODO: MULTI-WEARABLE
			//bool selected = enabled && (LLWearableType::WT_SHIRT <= i); // only select clothing by default
			childSetEnabled(name, enabled);
			childSetValue(name, enabled);
		}
	}

	// NOTE: .xml needs to be updated if attachments are added or their names are changed!
	LLVOAvatar* avatar = gAgentAvatarp;
	if (avatar)
	{
		for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); iter != avatar->mAttachmentPoints.end();)
		{
			LLVOAvatar::attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			S32	attachment_pt = curiter->first;	
			bool object_attached = (attachment->getNumObjects() > 0);
			std::string name = std::string("checkbox_") + attachment->getName();
			mCheckBoxList.push_back(std::make_pair(name, attachment_pt));
			childSetEnabled(name, object_attached);
			childSetValue(name, object_attached);
		}
	}

	if (!gHippoGridManager->getConnectedGrid()->supportsInvLinks())
	{
		childSetValue("checkbox_use_links", false);
		childSetEnabled("checkbox_use_outfits", false);
		childSetValue("checkbox_use_outfits", false);
	}

	getChild<LLUICtrl>("Save")->setCommitCallback(boost::bind(&LLMakeOutfitDialog::onSave, this)); 
	getChild<LLUICtrl>("Cancel")->setCommitCallback(boost::bind(&LLMakeOutfitDialog::close, this, _1)); 
	getChild<LLUICtrl>("Check All")->setCommitCallback(boost::bind(&LLMakeOutfitDialog::onCheckAll, this, true));
	getChild<LLUICtrl>("Uncheck All")->setCommitCallback(boost::bind(&LLMakeOutfitDialog::onCheckAll, this, false));
	getChild<LLUICtrl>("checkbox_use_outfits")->setCommitCallback(boost::bind(&LLMakeOutfitDialog::refresh, this));
	startModal();
}

//virtual
void LLMakeOutfitDialog::draw()
{
	bool one_or_more_items_selected = false;
	for (S32 i = 0; i < (S32)mCheckBoxList.size(); i++)
	{
		if (childGetValue(mCheckBoxList[i].first).asBoolean())
		{
			one_or_more_items_selected = true;
			break;
		}
	}
	childSetEnabled("Save", one_or_more_items_selected);

	LLModalDialog::draw();
}

BOOL LLMakeOutfitDialog::postBuild()
{
	refresh();
	return true;
}

void LLMakeOutfitDialog::refresh()
{
	bool fUseOutfits = getUseOutfits();

	for (S32 idxType = 0; idxType < LLWearableType::WT_COUNT; idxType++)
	{
		LLWearableType::EType wtType = (LLWearableType::EType)idxType;
		if (LLAssetType::AT_BODYPART != LLWearableType::getAssetType(wtType))
			continue;
		LLUICtrl* pCheckCtrl = getChild<LLUICtrl>(std::string("checkbox_") + LLWearableType::getTypeLabel(wtType));
		if (!pCheckCtrl)
			continue;

		pCheckCtrl->setEnabled(!fUseOutfits);
		if (fUseOutfits)
			pCheckCtrl->setValue(true);
	}
	getChild<LLUICtrl>("checkbox_use_links")->setEnabled(!fUseOutfits && gHippoGridManager->getConnectedGrid()->supportsInvLinks());
	getChild<LLUICtrl>("checkbox_legacy_copy_changes")->setEnabled(!fUseOutfits);
}


void LLMakeOutfitDialog::setWearableToInclude(S32 wearable, bool enabled, bool selected)
{
	LLWearableType::EType wtType = (LLWearableType::EType)wearable;
	if (((0 <= wtType) && (wtType < LLWearableType::WT_COUNT)) && 
		((LLAssetType::AT_BODYPART != LLWearableType::getAssetType(wtType)) || !getUseOutfits()))
	{
		std::string name = std::string("checkbox_") + LLWearableType::getTypeLabel(wtType);
		childSetEnabled(name, enabled);
		childSetValue(name, selected);
	}
}

void LLMakeOutfitDialog::getIncludedItems(LLInventoryModel::item_array_t& item_list)
{
	LLInventoryModel::cat_array_t *cats;
	LLInventoryModel::item_array_t *items;
	gInventory.getDirectDescendentsOf(LLAppearanceMgr::instance().getCOF(), cats, items);
	for (LLInventoryModel::item_array_t::const_iterator iter = items->begin(); iter != items->end(); ++iter)
	{
		LLViewerInventoryItem* item = (*iter);
		if (!item)
			continue;
		if (item->isWearableType())
		{
			LLWearableType::EType type = item->getWearableType();
			if (type < LLWearableType::WT_COUNT && childGetValue(mCheckBoxList[type].first).asBoolean())
			{
				item_list.push_back(item);
			}
		}
		else
		{
			LLViewerJointAttachment* attachment = gAgentAvatarp->getWornAttachmentPoint(item->getLinkedUUID());
			if (attachment && childGetValue(std::string("checkbox_")+attachment->getName()).asBoolean())
			{
				item_list.push_back(item);
			}
		}
	}
}

void LLMakeOutfitDialog::onSave()
{
	std::string folder_name = childGetValue("name ed").asString();
	LLStringUtil::trim(folder_name);
	if (!folder_name.empty())
	{
		makeOutfit(folder_name);
		close(); // destroys this object
	}
}

void LLMakeOutfitDialog::onCheckAll(bool check)
{
	for (S32 i = 0; i < (S32)(mCheckBoxList.size()); i++)
	{
		std::string name = mCheckBoxList[i].first;
		if (childIsEnabled(name)) childSetValue(name, check);
	}
}

void LLMakeOutfitDialog::makeOutfit(const std::string folder_name)
{
	LLInventoryModel::item_array_t item_list;
	getIncludedItems(item_list);

	// MULTI-WEARABLES TODO
	if (getUseOutfits())
		LLAppearanceMgr::instance().makeNewOutfitLinks(folder_name, item_list);
	else
		LLAppearanceMgr::instance().makeNewOutfitLegacy(folder_name, item_list, getUseLinks());

	if ( gAgent.getRegion() && gAgent.getRegion()->getCentralBakeVersion())
	{
		LLAppearanceMgr::instance().requestServerAppearanceUpdate();
	}
}

