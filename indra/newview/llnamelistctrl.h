/** 
 * @file llnamelistctrl.h
 * @brief A list of names, automatically refreshing from the name cache.
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

#ifndef LL_LLNAMELISTCTRL_H
#define LL_LLNAMELISTCTRL_H

#include <set>

#include "llscrolllistctrl.h"

class LLAvatarName;

/**
 * LLNameListCtrl item
 *
 * We don't use LLScrollListItem to be able to override getUUID(), which is needed
 * because the name list item value is not simply an UUID but a map (uuid, is_group).
 */
class LLNameListItem final : public LLScrollListItem, public LLHandleProvider<LLNameListItem>
{
public:
	enum ENameType
	{
		INDIVIDUAL,
		GROUP,
		SPECIAL,
		EXPERIENCE
	};

	// provide names for enums
	struct NameTypeNames : public LLInitParam::TypeValuesHelper<ENameType, NameTypeNames>
	{
		static void declareValues();
	};

	struct Params : public LLInitParam::Block<Params, LLScrollListItem::Params>
	{
		Optional<std::string>				name;
		Optional<ENameType, NameTypeNames>	target;

		Params()
			: name("name"),
			target("target", INDIVIDUAL)
		{}
	};
	ENameType getNameType() const { return mNameType; }
	void setNameType(ENameType name_type) { mNameType = name_type; }

protected:
	friend class LLNameListCtrl;

	LLNameListItem( const Params& p )
	:	LLScrollListItem(p), mNameType(p.target)
	{
	}

	LLNameListItem( const LLScrollListItem::Params& p, ENameType name_type)
	:	LLScrollListItem(p), mNameType(name_type)
	{
	}

private:
	ENameType mNameType;
};


class LLNameListCtrl final
:	public LLScrollListCtrl, public LLInstanceTracker<LLNameListCtrl>
{
public:
	typedef boost::signals2::signal<void(bool)> namelist_complete_signal_t;
	typedef LLNameListItem::Params NameItem;

	struct NameColumn : public LLInitParam::ChoiceBlock<NameColumn>
	{
		Alternative<S32>				column_index;
		Alternative<std::string>		column_name;
		NameColumn()
		:	column_index("name_column_index", 0),
			column_name("name_column")
		{}
	};

protected:
	LLNameListCtrl(const std::string& name, const LLRect& rect, BOOL allow_multiple_selection, BOOL draw_border = TRUE, bool draw_heading = false, S32 name_column_index = 0, const std::string& name_system = "PhoenixNameSystem", const std::string& tooltip = LLStringUtil::null);
	virtual ~LLNameListCtrl()
	{
		for (avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.begin(); it != mAvatarNameCacheConnections.end(); ++it)
		{
			if (it->second.connected())
			{
				it->second.disconnect();
			}
		}
		mAvatarNameCacheConnections.clear();
	}
	friend class LLUICtrlFactory;
public:
	virtual LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	// Add a user to the list by name.  It will be added, the name 
	// requested from the cache, and updated as necessary.
	LLScrollListItem* addNameItem(const LLUUID& agent_id, EAddPosition pos = ADD_BOTTOM,
					 BOOL enabled = TRUE, const std::string& suffix = LLStringUtil::null, const std::string& prefix = LLStringUtil::null);
	LLScrollListItem* addNameItem(NameItem& item, EAddPosition pos = ADD_BOTTOM);

	/*virtual*/ LLScrollListItem* addElement(const LLSD& element, EAddPosition pos = ADD_BOTTOM, void* userdata = NULL) override;
	LLScrollListItem* addNameItemRow(const NameItem& value, EAddPosition pos = ADD_BOTTOM, const std::string& suffix = LLStringUtil::null,
																							const std::string& prefix = LLStringUtil::null);

	// Add a user to the list by name.  It will be added, the name 
	// requested from the cache, and updated as necessary.
	void addGroupNameItem(const LLUUID& group_id, EAddPosition pos = ADD_BOTTOM,
						  BOOL enabled = TRUE);
	void addGroupNameItem(NameItem& item, EAddPosition pos = ADD_BOTTOM);


	void removeNameItem(const LLUUID& agent_id);

	LLScrollListItem* getNameItemByAgentId(const LLUUID& agent_id);

	// LLView interface
	/*virtual*/ BOOL	handleDragAndDrop(S32 x, S32 y, MASK mask,
									  BOOL drop, EDragAndDropType cargo_type, void *cargo_data,
									  EAcceptance *accept,
									  std::string& tooltip_msg) override;
	BOOL handleDoubleClick(S32 x, S32 y, MASK mask) override;

	Type getSelectedType() const override final;

	void setAllowCallingCardDrop(BOOL b) { mAllowCallingCardDrop = b; }

	void sortByName(BOOL ascending);
private:
	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name, std::string suffix, std::string prefix, LLHandle<LLNameListItem> item);
	void onGroupNameCache(const LLUUID& group_id, const std::string name, LLHandle<LLNameListItem> item);

private:
	S32    	 mNameColumnIndex;
	//std::string		mNameColumn;
	BOOL	 mAllowCallingCardDrop;
	const LLCachedControl<S32> mNameSystem; // Singu Note: Instead of mShortNames
	typedef std::map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
	avatar_name_cache_connection_map_t mAvatarNameCacheConnections;

	S32 mPendingLookupsRemaining;
	namelist_complete_signal_t mNameListCompleteSignal;

public:
	boost::signals2::connection setOnNameListCompleteCallback(std::function<void(bool)> onNameListCompleteCallback)
	{
		return mNameListCompleteSignal.connect(onNameListCompleteCallback);
	}

};


#endif
