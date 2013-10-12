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
#include "llinventory.h"
#include "llscrolllistitem.h"
#include "llscrolllistcolumn.h"
#include "llsdparam.h"
#include "lltrans.h"

static LLRegisterWidget<LLNameListCtrl> r("name_list");


void LLNameListCtrl::NameTypeNames::declareValues()
{
	declare("INDIVIDUAL", LLNameListCtrl::INDIVIDUAL);
	declare("GROUP", LLNameListCtrl::GROUP);
	declare("SPECIAL", LLNameListCtrl::SPECIAL);
}

LLNameListCtrl::LLNameListCtrl(const std::string& name, const LLRect& rect, BOOL allow_multiple_selection, BOOL draw_border, bool draw_heading, S32 name_column_index, const std::string& tooltip)
:	LLScrollListCtrl(name, rect, NULL, allow_multiple_selection, draw_border,draw_heading),
	mNameColumnIndex(name_column_index),
	mAllowCallingCardDrop(false),
	mShortNames(false),
	mAvatarNameCacheConnection()
{
	setToolTip(tooltip);
}

// public
LLScrollListItem* LLNameListCtrl::addNameItem(const LLUUID& agent_id, EAddPosition pos,
								 BOOL enabled, const std::string& suffix)
{
	//llinfos << "LLNameListCtrl::addNameItem " << agent_id << llendl;

	NameItem item;
	item.value = agent_id;
	item.enabled = enabled;
	item.target = INDIVIDUAL;
	
	return addNameItemRow(item, pos, suffix);
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
	lldebugst(LLERR_USER_INPUT) << "dragAndDrop handled by LLNameListCtrl " << getName() << llendl;

	return handled;
}


// public
void LLNameListCtrl::addGroupNameItem(const LLUUID& group_id, EAddPosition pos,
									  BOOL enabled)
{
	NameItem item;
	item.value = group_id;
	item.enabled = enabled;
	item.target = GROUP;

	addNameItemRow(item, pos);
}

// public
void LLNameListCtrl::addGroupNameItem(LLNameListCtrl::NameItem& item, EAddPosition pos)
{
	item.target = GROUP;
	addNameItemRow(item, pos);
}

LLScrollListItem* LLNameListCtrl::addNameItem(LLNameListCtrl::NameItem& item, EAddPosition pos)
{
	item.target = INDIVIDUAL;
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
	const std::string& suffix)
{
	LLUUID id = name_item.value().asUUID();
	LLNameListItem* item = new LLNameListItem(name_item,name_item.target() == GROUP);

	if (!item) return NULL;

	LLScrollListCtrl::addRow(item, name_item, pos);

	// use supplied name by default
	std::string fullname = name_item.name;
	switch(name_item.target)
	{
	case GROUP:
		gCacheName->getGroupName(id, fullname);
		// fullname will be "nobody" if group not found
		break;
	case SPECIAL:
		// just use supplied name
		break;
	case INDIVIDUAL:
	{
		LLAvatarName av_name;
		if (id.isNull())
		{
			fullname = LLTrans::getString("AvatarNameNobody");
		}
		else if (LLAvatarNameCache::get(id, &av_name))
		{
			if (mShortNames)
				fullname = av_name.mDisplayName;
			else
				fullname = av_name.getCompleteName();
		}
		else
		{
			// ...schedule a callback
				// This is not correct and will likely lead to partially populated lists in cases where avatar names are not cached.
				// *TODO : Change this to have 2 callbacks : one callback per list item and one for the whole list.
				/* Singu Note: Indeed it does, for now let's not use it
				if (mAvatarNameCacheConnection.connected())
				{
					mAvatarNameCacheConnection.disconnect();
				}
				mAvatarNameCacheConnection =*/ LLAvatarNameCache::get(id,boost::bind(&LLNameListCtrl::onAvatarNameCache,this, _1, _2, item->getHandle()));
		}
			break;
		}
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
		cell->setValue(fullname);
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
	}
}

void LLNameListCtrl::onAvatarNameCache(const LLUUID& agent_id,
									   const LLAvatarName& av_name,
									   LLHandle<LLNameListItem> item)
{
	//mAvatarNameCacheConnection.disconnect();

	std::string name;
	if (mShortNames)
		name = av_name.mDisplayName;
	else
		name = av_name.getCompleteName();

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

	BOOL multi_select = FALSE;
	node->getAttributeBOOL("multi_select", multi_select);

	BOOL draw_border = TRUE;
	node->getAttributeBOOL("draw_border", draw_border);

	BOOL draw_heading = FALSE;
	node->getAttributeBOOL("draw_heading", draw_heading);

	S32 name_column_index = 0;
	node->getAttributeS32("name_column_index", name_column_index);

	LLNameListCtrl* name_list = new LLNameListCtrl("name_list", rect, multi_select, draw_border, draw_heading, name_column_index);
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

	LLSD columns;
	S32 index = 0;
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName("column"))
		{
			std::string labelname("");
			child->getAttributeString("label", labelname);

			std::string columnname(labelname);
			child->getAttributeString("name", columnname);

			std::string sortname(columnname);
			child->getAttributeString("sort", sortname);
		
			if (child->hasAttribute("relative_width"))
			{
				F32 columnrelwidth = 0.f;
				child->getAttributeF32("relative_width", columnrelwidth);
				columns[index]["relative_width"] = columnrelwidth;
			}
			else if (child->hasAttribute("relwidth"))
			{
				F32 columnrelwidth = 0.f;
				child->getAttributeF32("relwidth", columnrelwidth);
				columns[index]["relative_width"] = columnrelwidth;
			}
			else if (child->hasAttribute("dynamic_width"))
			{
				BOOL columndynamicwidth = FALSE;
				child->getAttributeBOOL("dynamic_width", columndynamicwidth);
				columns[index]["dynamic_width"] = columndynamicwidth;
			}
			else if (child->hasAttribute("dynamicwidth"))
			{
				BOOL columndynamicwidth = FALSE;
				child->getAttributeBOOL("dynamicwidth", columndynamicwidth);
				columns[index]["dynamic_width"] = columndynamicwidth;
			}
			else
			{
				S32 columnwidth = -1;
				child->getAttributeS32("width", columnwidth);
				columns[index]["width"] = columnwidth;
			}

			LLFontGL::HAlign h_align = LLFontGL::LEFT;
			h_align = LLView::selectFontHAlign(child);

			columns[index]["name"] = columnname;
			columns[index]["label"] = labelname;
			columns[index]["halign"] = (S32)h_align;
			columns[index]["sort"] = sortname;

			index++;
		}
	}
	name_list->setColumnHeadings(columns);


	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName("row"))
		{
			LLUUID id;
			child->getAttributeUUID("id", id);

			LLSD row;

			row["id"] = id;

			S32 column_idx = 0;
			LLXMLNodePtr row_child;
			for (row_child = node->getFirstChild(); row_child.notNull(); row_child = row_child->getNextSibling())
			{
				if (row_child->hasName("column"))
				{
					std::string value = row_child->getTextContents();

					std::string columnname("");
					row_child->getAttributeString("name", columnname);

					std::string font("");
					row_child->getAttributeString("font", font);

					std::string font_style("");
					row_child->getAttributeString("font-style", font_style);

					row["columns"][column_idx]["column"] = columnname;
					row["columns"][column_idx]["value"] = value;
					row["columns"][column_idx]["font"] = font;
					row["columns"][column_idx]["font-style"] = font_style;
					column_idx++;
				}
			}
			name_list->addElement(row);
		}
	}

	std::string contents = node->getTextContents();
	name_list->setCommentText(contents);

	return name_list;
}
