// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
* @file lllandmarkactions.cpp
* @brief LLLandmarkActions class implementation
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
#include "lllandmarkactions.h"

#include "roles_constants.h"

#include "llinventory.h"
#include "llinventoryfunctions.h"
#include "lllandmark.h"
#include "llparcel.h"
#include "llregionhandle.h"

#include "llnotificationsutil.h"

#include "llagent.h"
#include "llagentui.h"
#include "llinventorymodel.h"
#include "lllandmarklist.h"
#include "llslurl.h"
#include "llstring.h"
#include "llviewerinventory.h"
#include "llviewerparcelmgr.h"
#include "llworldmapmessage.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llworldmap.h"

void copy_slurl_to_clipboard_callback(const std::string& slurl);

class LLFetchlLandmarkByPos : public LLInventoryCollectFunctor
{
private:
	LLVector3d mPos;
public:
	LLFetchlLandmarkByPos(const LLVector3d& pos) :
		mPos(pos)
	{}

	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override
    {
		if (!item || item->getType() != LLAssetType::AT_LANDMARK)
			return false;

		LLLandmark* landmark = gLandmarkList.getAsset(item->getAssetUUID());
		if (!landmark) // the landmark not been loaded yet
			return false;

		LLVector3d landmark_global_pos;
		if (!landmark->getGlobalPos(landmark_global_pos))
			return false;
		//we have to round off each coordinates to compare positions properly
		return ll_round(mPos.mdV[VX]) ==  ll_round(landmark_global_pos.mdV[VX])
				&& ll_round(mPos.mdV[VY]) ==  ll_round(landmark_global_pos.mdV[VY])
				&& ll_round(mPos.mdV[VZ]) ==  ll_round(landmark_global_pos.mdV[VZ]);
	}
};

class LLFetchLandmarksByName : public LLInventoryCollectFunctor
{
private:
	std::string name;
	BOOL use_substring;
	//this member will be contain copy of founded items to keep the result unique
	std::set<std::string> check_duplicate;

public:
LLFetchLandmarksByName(std::string &landmark_name, BOOL if_use_substring)
:name(landmark_name),
use_substring(if_use_substring)
	{
	LLStringUtil::toLower(name);
	}

public:
	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override
    {
		if (!item || item->getType() != LLAssetType::AT_LANDMARK)
			return false;

		LLLandmark* landmark = gLandmarkList.getAsset(item->getAssetUUID());
		if (!landmark) // the landmark not been loaded yet
			return false;

		bool acceptable = false;
		std::string landmark_name = item->getName();
		LLStringUtil::toLower(landmark_name);
		if(use_substring)
		{
			acceptable =  landmark_name.find( name ) != std::string::npos;
		}
		else
		{
			acceptable = landmark_name == name;
		}
		if(acceptable){
			if(check_duplicate.find(landmark_name) != check_duplicate.end()){
				// we have duplicated items in landmarks
				acceptable = false;
			}else{
				check_duplicate.insert(landmark_name);
			}
		}

		return acceptable;
	}
};

// Returns true if the given inventory item is a landmark pointing to the current parcel.
// Used to find out if there is at least one landmark from current parcel.
class LLFirstAgentParcelLandmark : public LLInventoryCollectFunctor
{
private:	
	bool mFounded;// to avoid unnecessary  check
	
public:
	LLFirstAgentParcelLandmark(): mFounded(false){}
	
	bool operator()(LLInventoryCategory* cat, LLInventoryItem* item) override
    {
		if (mFounded || !item || item->getType() != LLAssetType::AT_LANDMARK)
			return false;

		LLLandmark* landmark = gLandmarkList.getAsset(item->getAssetUUID());
		if (!landmark) // the landmark not been loaded yet
			return false;

		LLVector3d landmark_global_pos;
		if (!landmark->getGlobalPos(landmark_global_pos))
			return false;
		mFounded = LLViewerParcelMgr::getInstance()->inAgentParcel(landmark_global_pos);
		return mFounded;
	}
};

static void fetch_landmarks(LLInventoryModel::cat_array_t& cats,
							LLInventoryModel::item_array_t& items,
							LLInventoryCollectFunctor& add)
{
	// Look in "My Favorites"
	const LLUUID favorites_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
	gInventory.collectDescendentsIf(favorites_folder_id,
		cats,
		items,
		LLInventoryModel::EXCLUDE_TRASH,
		add);

	// Look in "Landmarks"
	const LLUUID landmarks_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
	gInventory.collectDescendentsIf(landmarks_folder_id,
		cats,
		items,
		LLInventoryModel::EXCLUDE_TRASH,
		add);
}

LLInventoryModel::item_array_t LLLandmarkActions::fetchLandmarksByName(std::string& name, BOOL use_substring)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLFetchLandmarksByName by_name(name, use_substring);
	fetch_landmarks(cats, items, by_name);

	return items;
}

bool LLLandmarkActions::landmarkAlreadyExists()
{
	// Determine whether there are landmarks pointing to the current global  agent position.
	return findLandmarkForAgentPos() != nullptr;
}

//static
bool LLLandmarkActions::hasParcelLandmark()
{
	LLFirstAgentParcelLandmark get_first_agent_landmark;
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	fetch_landmarks(cats, items, get_first_agent_landmark);
	return !items.empty();
	
}

// *TODO: This could be made more efficient by only fetching the FIRST
// landmark that meets the criteria
LLViewerInventoryItem* LLLandmarkActions::findLandmarkForGlobalPos(const LLVector3d &pos)
{
	// Determine whether there are landmarks pointing to the current parcel.
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLFetchlLandmarkByPos is_current_pos_landmark(pos);
	fetch_landmarks(cats, items, is_current_pos_landmark);

    return (items.empty()) ? nullptr : items[0];
}

LLViewerInventoryItem* LLLandmarkActions::findLandmarkForAgentPos()
{
	return findLandmarkForGlobalPos(gAgent.getPositionGlobal());
}

bool LLLandmarkActions::canCreateLandmarkHere()
{
	LLParcel* agent_parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if(!agent_parcel)
	{
		LL_WARNS() << "No agent region" << LL_ENDL;
		return false;
	}
	if (agent_parcel->getAllowLandmark()
		|| LLViewerParcelMgr::isParcelOwnedByAgent(agent_parcel, GP_LAND_ALLOW_LANDMARK))
	{
		return true;
	}

	return false;
}

void LLLandmarkActions::createLandmarkHere(
	const std::string& name, 
	const std::string& desc, 
	const LLUUID& folder_id)
{
	if(!gAgent.getRegion())
	{
		LL_WARNS() << "No agent region" << LL_ENDL;
		return;
	}
	LLParcel* agent_parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!agent_parcel)
	{
		LL_WARNS() << "No agent parcel" << LL_ENDL;
		return;
	}
	if (!canCreateLandmarkHere())
	{
		LLNotificationsUtil::add("CannotCreateLandmarkNotOwner");
		return;
	}

	create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
		folder_id, LLTransactionID::tnull,
		name, desc,
		LLAssetType::AT_LANDMARK,
		LLInventoryType::IT_LANDMARK,
		NOT_WEARABLE, PERM_ALL, 
		NULL);
}

void LLLandmarkActions::createLandmarkHere()
{
	std::string landmark_name, landmark_desc;

	LLAgentUI::buildLocationString(landmark_name, LLAgentUI::LOCATION_FORMAT_LANDMARK);
	LLAgentUI::buildLocationString(landmark_desc, LLAgentUI::LOCATION_FORMAT_FULL);
	const LLUUID folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);

	createLandmarkHere(landmark_name, landmark_desc, folder_id);
}

LLVector3d getRegionPosFromGlobalPos(const LLVector3d& global_pos, const LLSimInfo* siminfo)
{
    LLVector3d local_pos;
    local_pos[0] = fmod(global_pos[0], siminfo ? siminfo->getSizeX() : 256);
    local_pos[1] = fmod(global_pos[1], siminfo ? siminfo->getSizeY() : 256);
    local_pos[2] = global_pos[2];
    return local_pos;
}

void LLLandmarkActions::getSLURLfromPosGlobal(const LLVector3d& global_pos, slurl_callback_t cb, bool escaped /* = true */)
{
    const LLSimInfo* siminfo = LLWorldMap::getInstance()->simInfoFromPosGlobal(global_pos);
	if (siminfo)
	{
		std::string slurl = LLSLURL(siminfo->getName(), getRegionPosFromGlobalPos(global_pos, siminfo)).getSLURLString();
		cb(slurl);
	}
	else
	{
		U64 new_region_handle = to_region_handle(global_pos);

		LLWorldMapMessage::url_callback_t url_cb = boost::bind(&LLLandmarkActions::onRegionResponseSLURL,
														cb,
														global_pos,
														escaped,
														_2);

		LLWorldMapMessage::getInstance()->sendHandleRegionRequest(new_region_handle, url_cb, LLStringExplicit("unused"), false);
	}
}

void LLLandmarkActions::getRegionNameAndCoordsFromPosGlobal(const LLVector3d& global_pos, region_name_and_coords_callback_t cb)
{
	LLSimInfo* sim_infop = LLWorldMap::getInstance()->simInfoFromPosGlobal(global_pos);
	if (sim_infop)
	{
		LLVector3 pos = sim_infop->getLocalPos(global_pos);
		std::string name = sim_infop->getName() ;
		cb(name, ll_round(pos.mV[VX]), ll_round(pos.mV[VY]),ll_round(pos.mV[VZ]));
	}
	else
	{
		U64 new_region_handle = to_region_handle(global_pos);

		LLWorldMapMessage::url_callback_t url_cb = boost::bind(&LLLandmarkActions::onRegionResponseNameAndCoords,
														cb,
														global_pos,
														_1);

		LLWorldMapMessage::getInstance()->sendHandleRegionRequest(new_region_handle, url_cb, std::string("unused"), false);
	}
}

void LLLandmarkActions::onRegionResponseSLURL(slurl_callback_t cb,
										 const LLVector3d& global_pos,
										 bool escaped,
										 const std::string& url)
{
	std::string sim_name;
	std::string slurl;
    const LLSimInfo* siminfo = LLWorldMap::getInstance()->simInfoFromPosGlobal(global_pos);
	if (siminfo)
	{
	  slurl = LLSLURL(siminfo->getName(), getRegionPosFromGlobalPos(global_pos, siminfo)).getSLURLString();
	}
	else
	{
		slurl.clear();
	}

	cb(slurl);
}

void LLLandmarkActions::onRegionResponseNameAndCoords(region_name_and_coords_callback_t cb,
										 const LLVector3d& global_pos,
										 U64 region_handle)
{
	LLSimInfo* sim_infop = LLWorldMap::getInstance()->simInfoFromHandle(region_handle);
	if (sim_infop)
	{
		LLVector3 local_pos = sim_infop->getLocalPos(global_pos);
		std::string name = sim_infop->getName() ;
		cb(name, ll_round(local_pos.mV[VX]), ll_round(local_pos.mV[VY]), ll_round(local_pos.mV[VZ]));
	}
}

bool LLLandmarkActions::getLandmarkGlobalPos(const LLUUID& landmarkInventoryItemID, LLVector3d& posGlobal)
{
	LLViewerInventoryItem* item = gInventory.getItem(landmarkInventoryItemID);
	if (NULL == item)
		return false;

	const LLUUID& asset_id = item->getAssetUUID();

	LLLandmark* landmark = gLandmarkList.getAsset(asset_id);
	if (NULL == landmark)
		return false;

	return landmark->getGlobalPos(posGlobal);
}

LLLandmark* LLLandmarkActions::getLandmark(const LLUUID& landmarkInventoryItemID, LLLandmarkList::loaded_callback_t cb)
{
	LLViewerInventoryItem* item = gInventory.getItem(landmarkInventoryItemID);
	if (NULL == item)
		return NULL;

	const LLUUID& asset_id = item->getAssetUUID();

	LLLandmark* landmark = gLandmarkList.getAsset(asset_id, cb);
	if (landmark)
	{
		return landmark;
	}

	return NULL;
}

void LLLandmarkActions::copySLURLtoClipboard(const LLUUID& landmarkInventoryItemID)
{
	LLLandmark* landmark = LLLandmarkActions::getLandmark(landmarkInventoryItemID);
	if(landmark)
	{
		LLVector3d global_pos;
		landmark->getGlobalPos(global_pos);
		LLLandmarkActions::getSLURLfromPosGlobal(global_pos,&copy_slurl_to_clipboard_callback,true);
	}
}

void copy_slurl_to_clipboard_callback(const std::string& slurl)
{
	gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(slurl));
	LLSD args;
	args["SLURL"] = slurl;
	LLNotificationsUtil::add("CopySLURL", args);
}
