/** 
 * @file llnamelistctrl.cpp
 * @brief A list of names, automatically refreshed from name cache.
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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

#include "llnamelistctrl.h"

#include <boost/tokenizer.hpp>

#include "llavatarnamecache.h"
#include "llcachename.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llfloaterexperienceprofile.h"
#include "llgroupactions.h"
#include "llinventory.h"
#include "llscrolllistitem.h"
#include "llscrolllistcell.h"
#include "llscrolllistcolumn.h"
#include "llsdparam.h"
#include "lltrans.h"

static LLRegisterWidget<LLNameListCtrl> r("name_list");


void LLNameListItem::NameTypeNames::declareValues()
{
	declare("INDIVIDUAL", INDIVIDUAL);
	declare("GROUP", GROUP);
	declare("SPECIAL", SPECIAL);
	declare("EXPERIENCE", EXPERIENCE);
}

LLNameListCtrl::LLNameListCtrl(const std::string& name, const LLRect& rect, BOOL allow_multiple_selection, BOOL draw_border, bool draw_heading, S32 name_column_index, const std::string& name_system, const std::string& tooltip)
:	LLScrollListCtrl(name, rect, NULL, allow_multiple_selection, draw_border,draw_heading),
	mNameColumnIndex(name_column_index),
	mAllowCallingCardDrop(false),
	mNameSystem(name_system),
	mPendingLookupsRemaining(0)
{
	setToolTip(tooltip);
}

// public
LLScrollListItem* LLNameListCtrl::addNameItem(const LLUUID& agent_id, EAddPosition pos,
								 BOOL enabled, const std::string& suffix, const std::string& prefix)
{
	//LL_INFOS() << "LLNameListCtrl::addNameItem " << agent_id << LL_ENDL;

	NameItem item;
	item.value = agent_id;
	item.enabled = enabled;
	item.target = LLNameListItem::INDIVIDUAL;
	
	return addNameItemRow(item, pos, suffix, prefix);
}

// virtual, public
BOOL LLNameListCtrl::handleDragAndDrop( 
		S32 x, S32 y, MASK mask,
		BOOL drop,
		EDragAndDropType cargo_type, void *cargo_data, 
		EAcceptance *accept,
		std::string& tooltip_msg)
{
	if (!mAllowCallingCardDrop)
	{
		return FALSE;
	}

	BOOL handled = FALSE;

	if (cargo_type == DAD_CALLINGCARD)
	{
		if (drop)
		{
			LLInventoryItem* item = (LLInventoryItem *)cargo_data;
			addNameItem(item->getCreatorUUID());
		}

		*accept = ACCEPT_YES_MULTI;
	}
	else
	{
		*accept = ACCEPT_NO;
		if (tooltip_msg.empty())
		{
			if (!getToolTip().empty())
			{
				tooltip_msg = getToolTip();
			}
			else
			{
				// backwards compatable English tooltip (should be overridden in xml)
				tooltip_msg.assign("Drag a calling card here\nto add a resident.");
			}
		}
	}

	handled = TRUE;
	LL_DEBUGS("UserInput") << "dragAndDrop handled by LLNameListCtrl " << getName() << LL_ENDL;

	return handled;
}

BOOL LLNameListCtrl::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	bool handled = LLScrollListCtrl::handleDoubleClick(x, y, mask);
	if (!handled)
	{
		if (auto item = static_cast<LLNameListItem*>(hitItem(x, y)))
		{
			switch (item->getNameType())
			{
				case LLNameListItem::INDIVIDUAL: LLAvatarActions::showProfile(item->getValue()); break;
				case LLNameListItem::GROUP: LLGroupActions::show(item->getValue()); break;
				case LLNameListItem::EXPERIENCE: LLFloaterExperienceProfile::showInstance(item->getValue()); break;
				default: return false;
			}
			handled = true;
		}
	}
	return handled;
}

#define CONVERT_TO_RETTYPE(nametype, rettype)	\
nametype:										\
{												\
	if (ret == NONE)							\
		ret = rettype;							\
	else if (ret != rettype)					\
		return MULTIPLE;						\
	break;										\
}

LFIDBearer::Type LLNameListCtrl::getSelectedType() const
{
	auto ret = NONE;
	for (const auto& item : getAllSelected())
	{
		switch (static_cast<LLNameListItem*>(item)->getNameType())
		{
			CONVERT_TO_RETTYPE(case LLNameListItem::INDIVIDUAL, AVATAR)
			CONVERT_TO_RETTYPE(case LLNameListItem::GROUP, GROUP)
			CONVERT_TO_RETTYPE(case LLNameListItem::EXPERIENCE, EXPERIENCE)
			CONVERT_TO_RETTYPE(default, COUNT) // Invalid, but just use count instead
		}
	}
	return ret;
}
#undef CONVERT_TO_RETTYPE

// public
void LLNameListCtrl::addGroupNameItem(const LLUUID& group_id, EAddPosition pos,
									  BOOL enabled)
{
	NameItem item;
	item.value = group_id;
	item.enabled = enabled;
	item.target = LLNameListItem::GROUP;

	addNameItemRow(item, pos);
}

// public
void LLNameListCtrl::addGroupNameItem(LLNameListCtrl::NameItem& item, EAddPosition pos)
{
	item.target = LLNameListItem::GROUP;
	addNameItemRow(item, pos);
}

LLScrollListItem* LLNameListCtrl::addNameItem(LLNameListCtrl::NameItem& item, EAddPosition pos)
{
	item.target = LLNameListItem::INDIVIDUAL;
	return addNameItemRow(item, pos);
}

LLScrollListItem* LLNameListCtrl::addElement(const LLSD& element, EAddPosition pos, void* userdata)
{
	LLNameListCtrl::NameItem item_params;
	LLParamSDParser parser;
	parser.readSD(element, item_params);
	item_params.userdata = userdata;
	return addNameItemRow(item_params, pos);
}


LLScrollListItem* LLNameListCtrl::addNameItemRow(
	const LLNameListCtrl::NameItem& name_item,
	EAddPosition pos,
	const std::string& suffix,
	const std::string& prefix)
{
	LLUUID id = name_item.value().asUUID();
	LLNameListItem* item = new LLNameListItem(name_item);

	if (!item) return nullptr;

	LLScrollListCtrl::addRow(item, name_item, pos);

	// use supplied name by default
	std::string fullname = name_item.name;
	switch(name_item.target)
	{
	case LLNameListItem::GROUP:
		if (!gCacheName->getGroupName(id, fullname))
		{
			avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(id);
			if (it != mAvatarNameCacheConnections.end())
			{
				if (it->second.connected())
				{
					it->second.disconnect();
				}
				mAvatarNameCacheConnections.erase(it);
			}
			mAvatarNameCacheConnections[id] = gCacheName->getGroup(id, boost::bind(&LLNameListCtrl::onGroupNameCache, this, _1, _2, item->getHandle()));
		}
		break;
	case LLNameListItem::SPECIAL:
		// just use supplied name
		break;
	case LLNameListItem::INDIVIDUAL:
	{
		LLAvatarName av_name;
		if (id.isNull())
		{
			static const auto nobody = LLTrans::getString("AvatarNameNobody");
			fullname = nobody;
		}
		else if (LLAvatarNameCache::get(id, &av_name))
		{
			fullname = av_name.getNSName(mNameSystem);
		}
		else
		{
			// ...schedule a callback
			avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(id);
			if (it != mAvatarNameCacheConnections.end())
			{
				if (it->second.connected())
				{
					it->second.disconnect();
				}
				mAvatarNameCacheConnections.erase(it);
			}
			mAvatarNameCacheConnections[id] = LLAvatarNameCache::get(id,boost::bind(&LLNameListCtrl::onAvatarNameCache,this, _1, _2, suffix, prefix, item->getHandle()));

			if (mPendingLookupsRemaining <= 0)
			{
				// BAKER TODO:
				// We might get into a state where mPendingLookupsRemaining might
				//	go negative.  So just reset it right now and figure out if it's
				//	possible later :)
				mPendingLookupsRemaining = 0;
				mNameListCompleteSignal(false);
			}
			mPendingLookupsRemaining++;
		}
		break;
	}
	case LLNameListItem::EXPERIENCE:
		// just use supplied name
	default:
		break;
	}
	
	// Append optional suffix.
	if(!suffix.empty())
	{
		fullname.append(suffix);
	}

	LLScrollListCell* cell = item->getColumn(mNameColumnIndex);
	if (cell)
	{
		cell->setValue(prefix + fullname);
	}

	dirtyColumns();

	// this column is resizable
	LLScrollListColumn* columnp = getColumn(mNameColumnIndex);
	if (columnp && columnp->mHeader)
	{
		columnp->mHeader->setHasResizableElement(TRUE);
	}

	return item;
}

// public
void LLNameListCtrl::removeNameItem(const LLUUID& agent_id)
{
	// Find the item specified with agent_id.
	S32 idx = -1;
	for (item_list::iterator it = getItemList().begin(); it != getItemList().end(); it++)
	{
		LLScrollListItem* item = *it;
		if (item->getUUID() == agent_id)
		{
			idx = getItemIndex(item);
			break;
		}
	}

	// Remove it.
	if (idx >= 0)
	{
		selectNthItem(idx); // not sure whether this is needed, taken from previous implementation
		deleteSingleItem(idx);

		mPendingLookupsRemaining--;
	}
}

// public
LLScrollListItem* LLNameListCtrl::getNameItemByAgentId(const LLUUID& agent_id)
{
	for (item_list::iterator it = getItemList().begin(); it != getItemList().end(); it++)
	{
		LLScrollListItem* item = *it;
		if (item && item->getUUID() == agent_id)
		{
			return item;
		}
	}
	return NULL;
}

void LLNameListCtrl::onAvatarNameCache(const LLUUID& agent_id,
									   const LLAvatarName& av_name,
									   std::string suffix,
									   std::string prefix,
									   LLHandle<LLNameListItem> item)
{
	avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(agent_id);
	if (it != mAvatarNameCacheConnections.end())
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
		mAvatarNameCacheConnections.erase(it);
	}

	std::string name;
	name = av_name.getNSName(mNameSystem);

	// Append optional suffix.
	if (!suffix.empty())
	{
		name.append(suffix);
	}

	if (!prefix.empty())
	{
	    name.insert(0, prefix);
	}

	LLNameListItem* list_item = item.get();
	if (list_item && list_item->getUUID() == agent_id)
	{
		LLScrollListCell* cell = list_item->getColumn(mNameColumnIndex);
		if (cell)
		{
			cell->setValue(name);
			setNeedsSort();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// BAKER - FIX NameListCtrl
	//if (mPendingLookupsRemaining <= 0)
	{
		// We might get into a state where mPendingLookupsRemaining might
		//	go negative.  So just reset it right now and figure out if it's
		//	possible later :)
		//mPendingLookupsRemaining = 0;

		mNameListCompleteSignal(true);
	}
	//else
	{
	//	mPendingLookupsRemaining--;
	}
	//////////////////////////////////////////////////////////////////////////

	dirtyColumns();
}

void LLNameListCtrl::onGroupNameCache(const LLUUID& group_id, const std::string name, LLHandle<LLNameListItem> item)
{
	avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(group_id);
	if (it != mAvatarNameCacheConnections.end())
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
		mAvatarNameCacheConnections.erase(it);
	}

	LLNameListItem* list_item = item.get();
	if (list_item && list_item->getUUID() == group_id)
	{
		LLScrollListCell* cell = list_item->getColumn(mNameColumnIndex);
		if (cell)
		{
			cell->setValue(name);
			setNeedsSort();
		}
	}

	dirtyColumns();
}


void LLNameListCtrl::sortByName(BOOL ascending)
{
	sortByColumnIndex(mNameColumnIndex,ascending);
}

// virtual
LLXMLNodePtr LLNameListCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLScrollListCtrl::getXML();

	node->setName(LL_NAME_LIST_CTRL_TAG);

	node->createChild("allow_calling_card_drop", TRUE)->setBoolValue(mAllowCallingCardDrop);

	if (mNameColumnIndex != 0)
	{
		node->createChild("name_column_index", TRUE)->setIntValue(mNameColumnIndex);
	}

	// Don't save contents, probably filled by code

	return node;
}

LLView* LLNameListCtrl::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLRect rect;
	createRect(node, rect, parent, LLRect());

	BOOL multi_select = false;
	node->getAttributeBOOL("multi_select", multi_select);

	BOOL draw_border = true;
	node->getAttributeBOOL("draw_border", draw_border);

	BOOL draw_heading = false;
	node->getAttributeBOOL("draw_heading", draw_heading);

	S32 name_column_index = 0;
	node->getAttributeS32("name_column_index", name_column_index);

	std::string name_system("PhoenixNameSystem");
	node->getAttributeString("name_system", name_system);

	LLNameListCtrl* name_list = new LLNameListCtrl("name_list", rect, multi_select, draw_border, draw_heading, name_column_index, name_system);
	if (node->hasAttribute("heading_height"))
	{
		S32 heading_height;
		node->getAttributeS32("heading_height", heading_height);
		name_list->setHeadingHeight(heading_height);
	}

	BOOL allow_calling_card_drop = FALSE;
	if (node->getAttributeBOOL("allow_calling_card_drop", allow_calling_card_drop))
	{
		name_list->setAllowCallingCardDrop(allow_calling_card_drop);
	}

	name_list->setScrollListParameters(node);

	name_list->initFromXML(node, parent);

	return name_list;
}
