/** 
 * @file llfloatermute.cpp
 * @brief Container for mute list
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

#include "llfloatermute.h"

#include "llavatarname.h"
#include "llnotificationsutil.h"
#include "llscrolllistitem.h"
#include "lluictrlfactory.h"

// project include
#include "llfloateravatarpicker.h"
#include "llgroupactions.h"
#include "llmutelist.h"
#include "llnamelistctrl.h"

#include <boost/lexical_cast.hpp>

//
// Constants
//
const std::string FLOATER_TITLE = "Muted Residents & Objects";
const F32 INSTANT_MSG_SIZE = 8.0f;
const LLColor4 INSTANT_MSG_COLOR(1, 1, 1, 1);
const LLColor4 MUTED_MSG_COLOR(0.5f, 0.5f, 0.5f, 1.f);

const S32 LINE = 16;
const S32 LEFT = 2;
const S32 VPAD = 4;
const S32 HPAD = 4;

//-----------------------------------------------------------------------------
// LLFloaterMuteObjectUI()
//-----------------------------------------------------------------------------
// Class for handling mute object by name floater.
class LLFloaterMuteObjectUI : public LLFloater
{
public:
	typedef void(*callback_t)(const std::string&, void*);

	static LLFloaterMuteObjectUI* show(callback_t callback,
					   void* userdata);
	virtual BOOL postBuild();

protected:
	LLFloaterMuteObjectUI();
	virtual ~LLFloaterMuteObjectUI();
	virtual BOOL handleKeyHere(KEY key, MASK mask);

private:
	// UI Callbacks
	static void onBtnOk(void *data);
	static void onBtnCancel(void *data);

	void (*mCallback)(const std::string& objectName, 
			  void* userdata);
	void* mCallbackUserData;

	static LLFloaterMuteObjectUI* sInstance;
};

LLFloaterMuteObjectUI* LLFloaterMuteObjectUI::sInstance = NULL;

LLFloaterMuteObjectUI::LLFloaterMuteObjectUI()
	: LLFloater(std::string("Mute object by name")),
	  mCallback(NULL),
	  mCallbackUserData(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_mute_object.xml", NULL);
}

// Destroys the object
LLFloaterMuteObjectUI::~LLFloaterMuteObjectUI()
{
	gFocusMgr.releaseFocusIfNeeded( this );
	sInstance = NULL;
}

LLFloaterMuteObjectUI* LLFloaterMuteObjectUI::show(callback_t callback,
						   void* userdata)
{
	const bool firstInstantiation = (sInstance == NULL);
	if (firstInstantiation)
	{
		sInstance = new LLFloaterMuteObjectUI;
	}
	sInstance->mCallback = callback;
	sInstance->mCallbackUserData = userdata;
  
	sInstance->open();
	if (firstInstantiation)
	{
		sInstance->center();
	}

	return sInstance;
}


BOOL LLFloaterMuteObjectUI::postBuild()
{
	childSetAction("OK", onBtnOk, this);
	childSetAction("Cancel", onBtnCancel, this);
	return TRUE;
}

void LLFloaterMuteObjectUI::onBtnOk(void* userdata)
{
	LLFloaterMuteObjectUI* self = (LLFloaterMuteObjectUI*)userdata;
	if (!self) return;

	if (self->mCallback)
	{
		const std::string& text = self->childGetValue("object_name").asString();
		self->mCallback(text,self->mCallbackUserData);
	}
	self->close();
}

void LLFloaterMuteObjectUI::onBtnCancel(void* userdata)
{
	LLFloaterMuteObjectUI* self = (LLFloaterMuteObjectUI*)userdata;
	if (!self) return;

	self->close();
}

BOOL LLFloaterMuteObjectUI::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		onBtnOk(this);
		return TRUE;
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		onBtnCancel(this);
		return TRUE;
	}

	return LLFloater::handleKeyHere(key, mask);
}

//
// Member Functions
//

//-----------------------------------------------------------------------------
// LLFloaterMute()
//-----------------------------------------------------------------------------
LLFloaterMute::LLFloaterMute(const LLSD& seed)
:	LLFloater(std::string("mute floater"), std::string("FloaterMuteRect3"), FLOATER_TITLE, 
			  RESIZE_YES, 220, 140, DRAG_ON_TOP, MINIMIZE_YES, CLOSE_YES)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_mute.xml", NULL, FALSE);
}

BOOL LLFloaterMute::postBuild()
{
	childSetCommitCallback("mutes", boost::bind(&LLFloaterMute::updateButtons, this));
	childSetAction("Mute resident...", onClickPick, this);
	childSetAction("Mute object by name...", onClickMuteByName, this);
	childSetAction("Unmute", onClickRemove, this);

	mAvatarIcon = LLUI::getUIImage("icon_avatar_offline.tga");
	mObjectIcon = LLUI::getUIImage("inv_item_object.tga");
	mGroupIcon	= LLUI::getUIImage("icon_group.tga");
	mNameIcon	= LLUI::getUIImage("icon_name.tga");

	mMuteList = getChild<LLNameListCtrl>("mutes");
	mMuteList->setCommitOnSelectionChange(TRUE);

	LLMuteList::getInstance()->addObserver(this);
	
	refreshMuteList();

	return TRUE;
}

//-----------------------------------------------------------------------------
// ~LLFloaterMute()
//-----------------------------------------------------------------------------
LLFloaterMute::~LLFloaterMute()
{
}

//-----------------------------------------------------------------------------
// refreshMuteList()
//-----------------------------------------------------------------------------
void LLFloaterMute::refreshMuteList()
{
	uuid_vec_t selected = mMuteList->getSelectedIDs();
	S32 scrollpos = mMuteList->getScrollPos();

	mMuteList->deleteAllItems();
	mMuteDict.clear();

	std::vector<LLMute> mutes = LLMuteList::getInstance()->getMutes();
	for (std::vector<LLMute>::iterator it = mutes.begin(); it != mutes.end(); ++it)
	{
		std::string display_name = it->mName;
		LLNameListCtrl::NameItem element;
		LLUUID entry_id = it->mID;
		mMuteDict.insert(std::make_pair(entry_id,*it));
		element.value = entry_id;
		element.name = display_name;

		LLScrollListCell::Params name_column;
		name_column.column = "name";
		name_column.type = "text";
		name_column.value = "";

		LLScrollListCell::Params icon_column;
		icon_column.column = "icon";
		icon_column.type = "icon";

		switch(it->mType)
		{
		case LLMute::GROUP:
			icon_column.value = mGroupIcon->getName();
			element.target = LLNameListItem::GROUP;
			break;
		case LLMute::AGENT:
			icon_column.value = mAvatarIcon->getName();
			element.target = LLNameListItem::INDIVIDUAL;
			break;
		case LLMute::OBJECT:
			icon_column.value = mObjectIcon->getName();
			element.target = LLNameListItem::SPECIAL;
			break;
		case LLMute::BY_NAME:
		default:
			icon_column.value = mNameIcon->getName();
			element.target = LLNameListItem::SPECIAL;

			break;
		}
		element.columns.add(icon_column);
		element.columns.add(name_column);
		mMuteList->addNameItemRow(element);
	}
	mMuteList->updateSort();
	mMuteList->selectMultiple(selected);
	mMuteList->setScrollPos(scrollpos);
	updateButtons();
}

void LLFloaterMute::selectMute(const LLUUID& mute_id)
{
	mMuteList->selectByID(mute_id);
	updateButtons();
}

//-----------------------------------------------------------------------------
// updateButtons()
//-----------------------------------------------------------------------------
void LLFloaterMute::updateButtons()
{
	getChildView("Unmute")->setEnabled(!!mMuteList->getFirstSelected());
}

//-----------------------------------------------------------------------------
// onClickRemove()
//-----------------------------------------------------------------------------
void LLFloaterMute::onClickRemove(void *data)
{
	LLFloaterMute* floater = (LLFloaterMute *)data;

	S32 last_selected = floater->mMuteList->getFirstSelectedIndex();
	bool removed = false;
	uuid_vec_t items = floater->mMuteList->getSelectedIDs();
	for(uuid_vec_t::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		std::map<LLUUID,LLMute>::iterator mute_it = floater->mMuteDict.find(*it);
		if (mute_it != floater->mMuteDict.end() && LLMuteList::getInstance()->remove(mute_it->second))
		{
			floater->mMuteDict.erase(mute_it);
			removed = true;
		}
	}

	if (removed)
	{
		// Above removals may rebuild this dialog.
		if (last_selected == floater->mMuteList->getItemCount())
		{
			// we were on the last item, so select the last item again
			floater->mMuteList->selectNthItem(last_selected - 1);
		}
		else
		{
			// else select the item after the last item previously selected
			floater->mMuteList->selectNthItem(last_selected);
		}
		floater->mMuteList->setNeedsSort();
		floater->mMuteList->updateSort();
		floater->updateButtons();
	}


}

//-----------------------------------------------------------------------------
// onClickPick()
//-----------------------------------------------------------------------------
void LLFloaterMute::onClickPick(void *data)
{
	LLFloaterMute* floaterp = (LLFloaterMute*)data;
	const BOOL allow_multiple = FALSE;
	const BOOL close_on_select = TRUE;
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&LLFloaterMute::onPickUser, floaterp, _1, _2), allow_multiple, close_on_select);
	floaterp->addDependentFloater(picker);
}

//-----------------------------------------------------------------------------
// onPickUser()
//-----------------------------------------------------------------------------
void LLFloaterMute::onPickUser(const uuid_vec_t& ids, const std::vector<LLAvatarName>& names)
{
	if (names.empty() || ids.empty()) return;

	LLMute mute(ids[0], names[0].getLegacyName(), LLMute::AGENT);
	LLMuteList::getInstance()->add(mute);
	updateButtons();
}


void LLFloaterMute::onClickMuteByName(void* data)
{
	LLFloaterMuteObjectUI* picker = LLFloaterMuteObjectUI::show(callbackMuteByName,data);
	assert(picker);

	LLFloaterMute* floaterp = (LLFloaterMute*)data;
	floaterp->addDependentFloater(picker);
}

void LLFloaterMute::callbackMuteByName(const std::string& text, void* data)
{
	if (text.empty()) return;

	LLMute mute(LLUUID::null, text, LLMute::BY_NAME);
	BOOL success = LLMuteList::getInstance()->add(mute);
	if (!success)
	{
		LLNotificationsUtil::add("MuteByNameFailed");
	}
}
