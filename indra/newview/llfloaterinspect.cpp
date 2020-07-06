/** 
 * @file llfloaterinspect.cpp
 * @brief Floater for object inspection tool
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llfloaterinspect.h"

#include "llfloatertools.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llviewerobject.h"
#include "lluictrlfactory.h"
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "llagent.h"
// [/RLVa:KB]

//LLFloaterInspect* LLFloaterInspect::sInstance = NULL;

LLFloaterInspect::LLFloaterInspect(const LLSD&)
  : LLFloater(std::string("Inspect Object")),
	mObjectList(nullptr),
	mDirty(FALSE)
{
	mCommitCallbackRegistrar.add("Inspect.OwnerProfile",	boost::bind(&LLFloaterInspect::onClickOwnerProfile, this));
	mCommitCallbackRegistrar.add("Inspect.LastOwnerProfile",	boost::bind(&LLFloaterInspect::onClickLastOwnerProfile, this));
	mCommitCallbackRegistrar.add("Inspect.CreatorProfile",	boost::bind(&LLFloaterInspect::onClickCreatorProfile, this));
	mCommitCallbackRegistrar.add("Inspect.SelectObject",	boost::bind(&LLFloaterInspect::onSelectObject, this));
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inspect.xml");
}

BOOL LLFloaterInspect::postBuild()
{
	mObjectList = getChild<LLScrollListCtrl>("object_list");
//	childSetAction("button owner",onClickOwnerProfile, this);
//	childSetAction("button creator",onClickCreatorProfile, this);
//	childSetCommitCallback("object_list", onSelectObject, this);

	refresh();

	return TRUE;
}

LLFloaterInspect::~LLFloaterInspect(void)
{
	if(!gFloaterTools->getVisible())
	{
		if(LLToolMgr::getInstance()->getBaseTool() == LLToolCompInspect::getInstance())
		{
			LLToolMgr::getInstance()->clearTransientTool();
		}
		// Switch back to basic toolset
		LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);	
	}
	else
	{
		gFloaterTools->setFocus(TRUE);
	}
}

void LLFloaterInspect::onOpen()
{
	BOOL forcesel = LLSelectMgr::getInstance()->setForceSelection(TRUE);
	LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());
	LLSelectMgr::getInstance()->setForceSelection(forcesel);	// restore previouis value
	mObjectSelection = LLSelectMgr::getInstance()->getSelection();
	refresh();
}
void LLFloaterInspect::onClickCreatorProfile()
{
	if(mObjectList->getAllSelected().size() == 0)
	{
		return;
	}
	LLScrollListItem* first_selected =mObjectList->getFirstSelected();

	if (first_selected)
	{
		struct f : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			virtual bool apply(LLSelectNode* node)
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(first_selected->getUUID());
		LLSelectNode* node = mObjectSelection->getFirstNode(&func);
		if(node)
		{
//			LLAvatarActions::showProfile(node->mPermissions->getCreator());
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
			const LLUUID& idCreator = node->mPermissions->getCreator();
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) &&
				 ((node->mPermissions->getOwner() == idCreator) || (RlvUtil::isNearbyAgent(idCreator))) )
			{
				return;
			}
			LLAvatarActions::showProfile(idCreator);
// [/RLVa:KB]
		}
	}
}

void LLFloaterInspect::onClickOwnerProfile()
{
	if(mObjectList->getAllSelected().size() == 0) return;
	LLScrollListItem* first_selected =mObjectList->getFirstSelected();

	if (first_selected)
	{
		LLUUID selected_id = first_selected->getUUID();
		struct f : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			virtual bool apply(LLSelectNode* node)
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(selected_id);
		LLSelectNode* node = mObjectSelection->getFirstNode(&func);
		if(node)
		{
			const LLUUID& owner_id = node->mPermissions->getOwner();
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS))
				return;
// [/RLVa:KB]
			LLAvatarActions::showProfile(owner_id);
		}
	}
}

void LLFloaterInspect::onClickLastOwnerProfile()
{
	if(mObjectList->getAllSelected().size() == 0) return;
	LLScrollListItem* first_selected =mObjectList->getFirstSelected();

	if (first_selected)
	{
		LLUUID selected_id = first_selected->getUUID();
		struct f : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			virtual bool apply(LLSelectNode* node)
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(selected_id);
		LLSelectNode* node = mObjectSelection->getFirstNode(&func);
		if(node)
		{
			const LLUUID& last_owner_id = node->mPermissions->getLastOwner();
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS))
				if (last_owner_id == node->mPermissions->getOwner()) return;
// [/RLVa:KB]
			LLAvatarActions::showProfile(last_owner_id);
		}
	}
}

void LLFloaterInspect::onSelectObject()
{
	if(LLFloaterInspect::getSelectedUUID() != LLUUID::null)
	{
//		getChildView("button owner")->setEnabled(true);
//		getChildView("button creator")->setEnabled(true);
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
		getChildView("button owner")->setEnabled(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS));
		// TODO-RLVa: [RLVa-1.2.2] Is it worth checking the selected node just to selectively disable this button?
		getChildView("button creator")->setEnabled(true);
// [/RLVa:KB]
	}
}

LLUUID LLFloaterInspect::getSelectedUUID()
{
	if(mObjectList->getAllSelected().size() > 0)
	{
		LLScrollListItem* first_selected =mObjectList->getFirstSelected();
		if (first_selected)
		{
			return first_selected->getUUID();
		}
	}
	return LLUUID::null;
}

void LLFloaterInspect::refresh()
{
	LLUUID creator_id;
	std::string creator_name;
	S32 pos = mObjectList->getScrollPos();
	getChildView("button owner")->setEnabled(false);
	getChildView("button creator")->setEnabled(false);
	LLUUID selected_uuid;
	S32 selected_index = mObjectList->getFirstSelectedIndex();
	if(selected_index > -1)
	{
		LLScrollListItem* first_selected =
			mObjectList->getFirstSelected();
		if (first_selected)
		{
			selected_uuid = first_selected->getUUID();
		}
	}
	mObjectList->operateOnAll(LLScrollListCtrl::OP_DELETE);
	//List all transient objects, then all linked objects

	for (LLObjectSelection::valid_iterator iter = mObjectSelection->valid_begin();
		 iter != mObjectSelection->valid_end(); iter++)
	{
		LLSelectNode* obj = *iter;
		LLSD row;
		std::string owner_name, creator_name, last_owner_name;

		if (obj->mCreationDate == 0)
		{	// Don't have valid information from the server, so skip this one
			continue;
		}

		// Singu Note: Diverge from LL and handle datetime column in a sortable manner later on

		const LLUUID& idOwner = obj->mPermissions->getOwner();
		const LLUUID& idCreator = obj->mPermissions->getCreator();
		// <edit>
		const LLUUID& idLastOwner = obj->mPermissions->getLastOwner();
		// </edit>
		LLAvatarName av_name;

		// Only work with the names if we actually get a result
		// from the name cache. If not, defer setting the
		// actual name and set a placeholder.
		if (LLAvatarNameCache::get(idOwner, &av_name))
		{
//			owner_name = av_name.getNSName();
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
			bool fRlvFilterOwner = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && (idOwner != gAgent.getID()) &&
				(!obj->mPermissions->isGroupOwned());
			owner_name = (!fRlvFilterOwner) ? av_name.getNSName() : RlvStrings::getAnonym(av_name);
// [/RLVa:KB]
		}
		else
		{
			owner_name = LLTrans::getString("RetrievingData");
			if (mOwnerNameCacheConnection.find(idOwner) == mOwnerNameCacheConnection.end())
				mOwnerNameCacheConnection.emplace(idOwner, LLAvatarNameCache::get(idOwner, boost::bind(&LLFloaterInspect::onGetOwnerNameCallback, this, _1)));
		}

		// <edit>
		if (LLAvatarNameCache::get(idLastOwner, &av_name))
		{
//			last_owner_name = av_name.getNSName();
// [RLVa:LF] - Copied from the above creator check Checked: 2010-11-01 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
			LLAvatarNameCache::get(idLastOwner, &av_name);
			bool fRlvFilterLastOwner = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && idLastOwner != gAgent.getID() &&
				(obj->mPermissions->getOwner() == idLastOwner || RlvUtil::isNearbyAgent(idLastOwner));
			last_owner_name = (!fRlvFilterLastOwner) ? av_name.getNSName() : RlvStrings::getAnonym(av_name);
// [/RLVa:LF]
		}
		else
		{
			last_owner_name = LLTrans::getString("RetrievingData");
			if (mLastOwnerNameCacheConnection.find(idLastOwner) == mLastOwnerNameCacheConnection.end())
				mLastOwnerNameCacheConnection.emplace(idLastOwner,  LLAvatarNameCache::get(idLastOwner, boost::bind(&LLFloaterInspect::onGetLastOwnerNameCallback, this, _1)));
		}
		// </edit>

		if (LLAvatarNameCache::get(idCreator, &av_name))
		{
//			creator_name = av_name.getNSName();
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
			const LLUUID& idCreator = obj->mPermissions->getCreator();
			LLAvatarNameCache::get(idCreator, &av_name);
			bool fRlvFilterCreator = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && (idCreator != gAgent.getID()) &&
				( (obj->mPermissions->getOwner() == idCreator) || (RlvUtil::isNearbyAgent(idCreator)) );
			creator_name = (!fRlvFilterCreator) ? av_name.getNSName() : RlvStrings::getAnonym(av_name);
// [/RLVa:KB]
		}
		else
		{
			creator_name = LLTrans::getString("RetrievingData");
			if (mCreatorNameCacheConnection.find(idCreator) == mCreatorNameCacheConnection.end())
				mCreatorNameCacheConnection.emplace(idCreator, LLAvatarNameCache::get(idCreator, boost::bind(&LLFloaterInspect::onGetCreatorNameCallback, this, _1)));
		}

		row["id"] = obj->getObject()->getID();
		row["columns"][0]["column"] = "object_name";
		row["columns"][0]["type"] = "text";
		// make sure we're either at the top of the link chain
		// or top of the editable chain, for attachments
		if(!(obj->getObject()->isRoot() || obj->getObject()->isRootEdit()))
		{
			row["columns"][0]["value"] = std::string("   ") + obj->mName;
		}
		else
		{
			row["columns"][0]["value"] = obj->mName;
		}
		row["columns"][1]["column"] = "owner_name";
		row["columns"][1]["type"] = "text";
		row["columns"][1]["value"] = owner_name;
		// <edit>
		row["columns"][2]["column"] = "last_owner_name";
		row["columns"][2]["type"] = "text";
		row["columns"][2]["value"] = last_owner_name;
		// </edit>
		row["columns"][3]["column"] = "creator_name";
		row["columns"][3]["type"] = "text";
		row["columns"][3]["value"] = creator_name;
		// <edit>
		row["columns"][4]["column"] = "face_num";
		row["columns"][4]["type"] = "text";
		row["columns"][4]["value"] = llformat("%d",obj->getObject()->getNumFaces());

		row["columns"][5]["column"] = "vertex_num";
		row["columns"][5]["type"] = "text";
		row["columns"][5]["value"] = llformat("%d",obj->getObject()->getNumVertices());
		// inventory silliness
		S32 scripts,total_inv;
		std::map<LLUUID,std::pair<U32,U32> >::iterator itr = mInventoryNums.find(obj->getObject()->getID());
		if (itr != mInventoryNums.end())
		{
			scripts = itr->second.first;
			total_inv = itr->second.second;
		}
		else
		{
			scripts = 0;
			total_inv = 0;
			if (std::find(mQueue.begin(),mQueue.end(),obj->getObject()->getID()) == mQueue.end())
			{
				mQueue.push_back(obj->getObject()->getID());
				registerVOInventoryListener(obj->getObject(), NULL);
				requestVOInventory();
			}
		}
		row["columns"][6]["column"] = "script_num";
		row["columns"][6]["type"] = "text";
		row["columns"][6]["value"] = llformat("%d",scripts);
		row["columns"][7]["column"] = "inv_num";
		row["columns"][7]["type"] = "text";
		row["columns"][7]["value"] = llformat("%d",total_inv);
		// </edit>
		row["columns"][8]["column"] = "creation_date";
		row["columns"][8]["type"] = "date";
		row["columns"][8]["value"] = LLDate(obj->mCreationDate/1000000);
		static const LLCachedControl<std::string> format("TimestampFormat");
		row["columns"][8]["format"] = format;
		mObjectList->addElement(row, ADD_TOP);
	}
	if(selected_index > -1 && mObjectList->getItemIndex(selected_uuid) == selected_index)
	{
		mObjectList->selectNthItem(selected_index);
	}
	else
	{
		mObjectList->selectNthItem(0);
	}
	onSelectObject();
	mObjectList->setScrollPos(pos);
}

// <edit>
void LLFloaterInspect::inventoryChanged(LLViewerObject* viewer_object, LLInventoryObject::object_list_t* inv, S32, void*)
{
	auto iter = std::find(mQueue.begin(),mQueue.end(),viewer_object->getID());
	if (viewer_object && inv && iter != mQueue.end())
	{
		U32 scripts = 0;
		LLInventoryObject::object_list_t::const_iterator end = inv->end();
		for (LLInventoryObject::object_list_t::const_iterator it = inv->begin(); it != end; ++it)
			if ((*it)->getType() == LLAssetType::AT_LSL_TEXT)
				++scripts;
		mInventoryNums[viewer_object->getID()] = std::make_pair(scripts,inv->size());
		mQueue.erase(iter);
		setDirty();
	}
}
// </edit>

void LLFloaterInspect::onFocusReceived()
{
	LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());
	LLFloater::onFocusReceived();
}

void LLFloaterInspect::dirty()
{
	// <edit>
	mInventoryNums.clear();
	mQueue.clear();
	// </edit>
	setDirty();
}

void LLFloaterInspect::onGetOwnerNameCallback(const LLUUID& id)
{
	mOwnerNameCacheConnection.erase(id);
	setDirty();
}

void LLFloaterInspect::onGetLastOwnerNameCallback(const LLUUID& id)
{
	mLastOwnerNameCacheConnection.erase(id);
	setDirty();
}

void LLFloaterInspect::onGetCreatorNameCallback(const LLUUID& id)
{
	mCreatorNameCacheConnection.erase(id);
	setDirty();
}

void LLFloaterInspect::draw()
{
	if (mDirty)
	{
		refresh();
		mDirty = FALSE;
	}

	LLFloater::draw();
}
