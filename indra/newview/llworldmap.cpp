/** 
 * @file llworldmap.cpp
 * @brief Underlying data representation for map of the world
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llworldmap.h"

#include "llregionhandle.h"
#include "message.h"


#include "llappviewer.h"	// for gPacificDaylightTime
#include "llagent.h"
#include "llmapresponders.h"
#include "llviewercontrol.h"
#include "llfloaterworldmap.h"
#include "lltexturecache.h"
#include "lltracker.h"
#include "llviewertexturelist.h"
#include "llviewerregion.h"
#include "llregionflags.h"
#include "llworldmapmessage.h"
#include "hippogridmanager.h"
#include "lfsimfeaturehandler.h"

bool LLWorldMap::sGotMapURL =  false;
// Timers to temporise database requests
const F32 AGENTS_UPDATE_TIMER = 30.0;			// Seconds between 2 agent requests for a region
const F32 REQUEST_ITEMS_TIMER = 10.f * 60.f;	// Seconds before we consider re-requesting item data for the grid

//---------------------------------------------------------------------------
// LLItemInfo
//---------------------------------------------------------------------------

LLItemInfo::LLItemInfo(F32 global_x, F32 global_y,
					   const std::string& name, 
					   LLUUID id)
:	mName(name),
	mToolTip(""),
	mPosGlobal(global_x, global_y, 40.0),
	mID(id),
	mCount(1)
//	mSelected(false)
//	mColor()
{
}

//---------------------------------------------------------------------------
// LLSimInfo
//---------------------------------------------------------------------------

LLSimInfo::LLSimInfo(U64 handle)
:	mHandle(handle),
	mName(),
	mAgentsUpdateTime(0),
	mAccess(0x0),
	mRegionFlags(0x0),
	mFirstAgentRequest(true),
// <FS:CR> Aurora Sim
    mSizeX(REGION_WIDTH_UNITS),
	mSizeY(REGION_WIDTH_UNITS),
// </FS:CR> Aurora Sim
	mAlpha(0.f)
{
}

void LLSimInfo::setLandForSaleImage (LLUUID image_id) 
{
	const bool is_whitecore = gHippoGridManager->getConnectedGrid()->isWhiteCore();
	if (is_whitecore && mMapImageID[SIM_LAYER_OVERLAY].isNull() && image_id.notNull() && gTextureList.findImage(image_id, TEX_LIST_STANDARD))
		LLAppViewer::getTextureCache()->removeFromCache(image_id);

	mMapImageID[SIM_LAYER_OVERLAY] = image_id;

	// Fetch the image
	if (mMapImageID[SIM_LAYER_OVERLAY].notNull())
	{
		mLayerImage[SIM_LAYER_OVERLAY] = LLViewerTextureManager::getFetchedTexture(mMapImageID[SIM_LAYER_OVERLAY], FTT_DEFAULT, MIPMAP_TRUE, LLGLTexture::BOOST_MAP, LLViewerTexture::LOD_TEXTURE);
		if (is_whitecore) mLayerImage[SIM_LAYER_OVERLAY]->forceImmediateUpdate();
		mLayerImage[SIM_LAYER_OVERLAY]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}
	else
	{
		mLayerImage[SIM_LAYER_OVERLAY] = NULL;
	}
}

LLPointer<LLViewerFetchedTexture> LLSimInfo::getLandForSaleImage ()
{
	if (mLayerImage[SIM_LAYER_OVERLAY].isNull() && mMapImageID[SIM_LAYER_OVERLAY].notNull())
	{
		// Fetch the image if it hasn't been done yet (unlikely but...)
		mLayerImage[SIM_LAYER_OVERLAY] = LLViewerTextureManager::getFetchedTexture(mMapImageID[SIM_LAYER_OVERLAY], FTT_DEFAULT, MIPMAP_TRUE, LLGLTexture::BOOST_MAP, LLViewerTexture::LOD_TEXTURE);
		mLayerImage[SIM_LAYER_OVERLAY]->setAddressMode(LLTexUnit::TAM_CLAMP);
	}
	if (!mLayerImage[SIM_LAYER_OVERLAY].isNull())
	{
		// Boost the fetch level when we try to access that image
		mLayerImage[SIM_LAYER_OVERLAY]->setBoostLevel(LLGLTexture::BOOST_MAP);
	}
	return mLayerImage[SIM_LAYER_OVERLAY];
}

LLVector3d LLSimInfo::getGlobalPos(const LLVector3& local_pos) const
{
	LLVector3d pos = from_region_handle(mHandle);
	pos.mdV[VX] += local_pos.mV[VX];
	pos.mdV[VY] += local_pos.mV[VY];
	pos.mdV[VZ] += local_pos.mV[VZ];
	return pos;
}

LLVector3d LLSimInfo::getGlobalOrigin() const
{
	return from_region_handle(mHandle);
}
LLVector3 LLSimInfo::getLocalPos(LLVector3d global_pos) const
{
	LLVector3d sim_origin = from_region_handle(mHandle);
	return LLVector3(global_pos - sim_origin);
}

void LLSimInfo::clearImage()
{
	for(U32 layer = SIM_LAYER_BEGIN; layer < SIM_LAYER_COUNT;++layer)
	{
		if(!mLayerImage[layer].isNull())
		{
			mLayerImage[layer]->setBoostLevel(0);
			mLayerImage[layer]=NULL;
		}
	}
}

void LLSimInfo::dropImagePriority(sim_layer_type layer/* = SIM_LAYER_COUNT*/)
{
	if(layer == SIM_LAYER_COUNT)
	{
		for(U32 layer = SIM_LAYER_BEGIN; layer < SIM_LAYER_COUNT;++layer)
		{
			if(!mLayerImage[layer].isNull())
			{
				mLayerImage[layer]->setBoostLevel(0);
			}
		}
	}
	else if(layer < SIM_LAYER_COUNT && layer >= SIM_LAYER_BEGIN && !mLayerImage[layer].isNull())
	{
		mLayerImage[layer]->setBoostLevel(0);
	}
}

// Update the agent count for that region
void LLSimInfo::updateAgentCount(F64 time)
{
	if ((time - mAgentsUpdateTime > AGENTS_UPDATE_TIMER) || mFirstAgentRequest)
	{
		LLWorldMapMessage::getInstance()->sendItemRequest(MAP_ITEM_AGENT_LOCATIONS, mHandle);
		mAgentsUpdateTime = time;
		mFirstAgentRequest = false;
	}
}

// Get the total agents count
const S32 LLSimInfo::getAgentCount() const
{
	S32 total_agent_count = 0;
	for (LLSimInfo::item_info_list_t::const_iterator it = mAgentLocations.begin(); it != mAgentLocations.end(); ++it)
	{
		total_agent_count += it->getCount();
	}
	return total_agent_count;
}

bool LLSimInfo::isName(const std::string& name) const
{
	return (LLStringUtil::compareInsensitive(name, mName) == 0);
}

void LLSimInfo::dump() const
{
	U32 x_pos, y_pos;
	from_region_handle(mHandle, &x_pos, &y_pos);

	LL_INFOS("World Map") << x_pos << "," << y_pos
		<< " " << mName 
		<< " " << (S32)mAccess
		<< " " << std::hex << mRegionFlags << std::dec
		<< " " << mSizeX << "x" << mSizeY
//		<< " " << mWaterHeight
		<< LL_ENDL;
}

void LLSimInfo::clearItems()
{
	mTelehubs.clear();
	mInfohubs.clear();
	mPGEvents.clear();
	mMatureEvents.clear();
	mAdultEvents.clear();
	mLandForSale.clear();
	mLandForSaleAdult.clear();
//  We persist the agent count though as it is updated on a frequent basis
// 	mAgentLocations.clear();
}

void LLSimInfo::insertAgentLocation(const LLItemInfo& item) 
{
	std::string name = item.getName();

	// Find the last item in the list with a different name and erase them
	item_info_list_t::iterator lastiter;
	for (lastiter = mAgentLocations.begin(); lastiter != mAgentLocations.end(); ++lastiter)
	{
		LLItemInfo& info = *lastiter;
		if (info.isName(name))
		{
			break;
		}
	}
	if (lastiter != mAgentLocations.begin())
	{
		mAgentLocations.erase(mAgentLocations.begin(), lastiter);
	}

	// Now append the new location
	mAgentLocations.push_back(item); 
}

//---------------------------------------------------------------------------
// World Map
//---------------------------------------------------------------------------

LLWorldMap::LLWorldMap() :
	mIsTrackingLocation( false ),
	mIsTrackingFound( false ),
	mIsInvalidLocation( false ),
	mIsTrackingDoubleClick( false ),
	mIsTrackingCommit( false ),
	mTrackingLocation( 0, 0, 0 ),
	mFirstRequest(true),
	mMapLoaded(false)
{
	//LL_INFOS("World Map") << "Creating the World Map -> LLWorldMap::LLWorldMap()" << LL_ENDL;
	/*for (U32 map=SIM_LAYER_BEGIN; map<SIM_LAYER_OVERLAY; ++map)
	{
		mMapBlockLoaded[map] = new bool[MAP_BLOCK_RES*MAP_BLOCK_RES];
	}*/
	clearSimFlags();
}


LLWorldMap::~LLWorldMap()
{
	//LL_INFOS("World Map") << "Destroying the World Map -> LLWorldMap::~LLWorldMap()" << LL_ENDL;
	reset();
	/*for (U32 map=SIM_LAYER_BEGIN; map<SIM_LAYER_OVERLAY; ++map)
	{
		delete[] mMapBlockLoaded[map];
	}*/
}


void LLWorldMap::reset()
{
	clearItems(true);		// Clear the items lists
	clearImageRefs();		// Clear the world mipmap and the land for sale tiles
	clearSimFlags();		// Clear the block info flags array 

	// Finally, clear the region map itself
	for_each(mSimInfoMap.begin(), mSimInfoMap.end(), DeletePairedPointer());
	mSimInfoMap.clear();

	mMapLoaded = false;
	mMapLayers.clear();

	for (U32 map=SIM_LAYER_BEGIN; map<SIM_LAYER_OVERLAY; ++map)
	{
		mMapBlockMap[map].clear();
	}
}

// Returns true if the items have been cleared
bool LLWorldMap::clearItems(bool force)
{
	bool clear = false;
	if ((mRequestTimer.getElapsedTimeF32() > REQUEST_ITEMS_TIMER) || mFirstRequest || force)
	{
		mRequestTimer.reset();

		LLSimInfo* sim_info = NULL;
		for (sim_info_map_t::iterator it = mSimInfoMap.begin(); it != mSimInfoMap.end(); ++it)
		{
			sim_info = it->second;
			if (sim_info)
			{
				sim_info->clearItems();
			}
		}
		clear = true;
		mFirstRequest = false;
	}
	return clear;
}

void LLWorldMap::clearImageRefs()
{
	// We clear the reference to the images we're holding.
	// Images hold by the world mipmap first
	mWorldMipmap.reset();	
	
	// Images hold by the region map
	LLSimInfo* sim_info = NULL;
	for (sim_info_map_t::iterator it = mSimInfoMap.begin(); it != mSimInfoMap.end(); ++it)
	{
		sim_info = it->second;
		if(sim_info)
		{
			sim_info->clearImage();
		}
	}
}

// Doesn't clear the already-loaded sim infos, just re-requests them
void LLWorldMap::clearSimFlags()
{
	for (U32 map=SIM_LAYER_BEGIN; map<SIM_LAYER_OVERLAY; ++map)
	{
		mMapBlockMap[map].clear();
		/*for (S32 idx=0; idx<MAP_BLOCK_RES*MAP_BLOCK_RES; ++idx)
		{
			mMapBlockLoaded[map][idx] = FALSE;
		}*/
	}
}

LLSimInfo* LLWorldMap::createSimInfoFromHandle(const U64 handle)
{
	LLSimInfo* sim_info = new LLSimInfo(handle);
	mSimInfoMap[handle] = sim_info;
	return sim_info;
}

void LLWorldMap::equalizeBoostLevels()
{
	mWorldMipmap.equalizeBoostLevels();
	return;
}

LLSimInfo* LLWorldMap::simInfoFromPosGlobal(const LLVector3d& pos_global)
{
	U64 handle = to_region_handle(pos_global);
	return simInfoFromHandle(handle);
}

LLSimInfo* LLWorldMap::simInfoFromHandle(const U64 handle)
{
	sim_info_map_t::const_iterator it = mSimInfoMap.find(handle);
	if (it != mSimInfoMap.end())
	{
		return it->second;
	}

// <FS:CR> Aurora Sim
	U32 x = 0, y = 0;
	from_region_handle(handle, &x, &y);
	for (it = mSimInfoMap.begin(); it != mSimInfoMap.end(); ++it)
	{
		U32 checkRegionX, checkRegionY;
		from_region_handle((*it).first, &checkRegionX, &checkRegionY);

		LLSimInfo* info = (*it).second;
		if (x >= checkRegionX && x < (checkRegionX + info->getSizeX()) &&
			y >= checkRegionY && y < (checkRegionY + info->getSizeY()))
		{
			return info;
		}
	}
// </FS:CR> Aurora Sim
	return NULL;
}


LLSimInfo* LLWorldMap::simInfoFromName(const std::string& sim_name)
{
	LLSimInfo* sim_info = NULL;
	if (!sim_name.empty())
	{
		// Iterate through the entire sim info map and compare the name
		sim_info_map_t::iterator it;
		for (it = mSimInfoMap.begin(); it != mSimInfoMap.end(); ++it)
		{
			sim_info = it->second;
			if (sim_info && sim_info->isName(sim_name) )
			{
				// break out of loop if success
				break;
			}
		}
		// If we got to the end, we haven't found the sim. Reset the ouput value to NULL.
		if (it == mSimInfoMap.end())
			sim_info = NULL;
	}
	return sim_info;
}

bool LLWorldMap::simNameFromPosGlobal(const LLVector3d& pos_global, std::string & outSimName )
{
	LLSimInfo* sim_info = simInfoFromPosGlobal(pos_global);

	if (sim_info)
	{
		outSimName = sim_info->getName();
	}
	else
	{
		outSimName = "(unknown region)";
	}

	return (sim_info != NULL);
}

// public
void LLWorldMap::sendMapLayerRequest()
{
	if (!gAgent.getRegion()) return;

	std::string url = gAgent.getRegion()->getCapability(
		gAgent.isGodlike() ? "MapLayerGod" : "MapLayer");

	U32 flags = layerToFlags((sim_layer_type)SIM_LAYER_COMPOSITE);
	if (!url.empty())
	{
		LLSD body;
		body["Flags"] = (LLSD::Integer)flags;
		//LL_INFOS() << "LLWorldMap::sendMapLayerRequest via capability" << LL_ENDL;
		LLHTTPClient::post(url, body, new LLMapLayerResponder);
	}
	else
	{
		//LL_INFOS() << "LLWorldMap::sendMapLayerRequest via message system" << LL_ENDL;
		LLMessageSystem* msg = gMessageSystem;

		// Request for layer
		msg->newMessageFast(_PREHASH_MapLayerRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->addU32Fast(_PREHASH_Flags, flags);
		msg->addU32Fast(_PREHASH_EstateID, 0); // Filled in on sim
		msg->addBOOLFast(_PREHASH_Godlike, FALSE); // Filled in on sim
		gAgent.sendReliableMessage();
	}
}



// public static
void LLWorldMap::processMapLayerReply(LLMessageSystem* msg, void**)
{
	//LL_INFOS() << "LLWorldMap::processMapLayerReply from message system" << LL_ENDL;

	U32 agent_flags;
	msg->getU32Fast(_PREHASH_AgentData, _PREHASH_Flags, agent_flags);

	U32 layer = flagsToLayer(agent_flags);
	if (layer != SIM_LAYER_COMPOSITE)
	{
		LL_WARNS() << "Invalid or out of date map image type returned!" << LL_ENDL;
		return;
	}

	LLUUID image_id;

	S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_LayerData);

	LLWorldMap::getInstance()->mMapLayers.clear();

	for (S32 block=0; block<num_blocks; ++block)
	{
		LLWorldMapLayer new_layer;
		new_layer.LayerDefined = TRUE;
		msg->getUUIDFast(_PREHASH_LayerData, _PREHASH_ImageID, new_layer.LayerImageID, block);

		U32 left, right, top, bottom;
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Left, left, block);
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Right, right, block);
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Top, top, block);
		msg->getU32Fast(_PREHASH_LayerData, _PREHASH_Bottom, bottom, block);

		new_layer.LayerImage = LLViewerTextureManager::getFetchedTexture(new_layer.LayerImageID, FTT_MAP_TILE, MIPMAP_TRUE, LLGLTexture::BOOST_MAP, LLViewerTexture::LOD_TEXTURE);

		gGL.getTexUnit(0)->bind(new_layer.LayerImage.get());
		new_layer.LayerImage->setAddressMode(LLTexUnit::TAM_CLAMP);

		new_layer.LayerExtents.mLeft = left;
		new_layer.LayerExtents.mRight = right;
		new_layer.LayerExtents.mBottom = bottom;
		new_layer.LayerExtents.mTop = top;

		LLWorldMap::getInstance()->mMapLayers.push_back(new_layer);
	}

	LLWorldMap::getInstance()->mMapLoaded = true;
}

// public static
bool LLWorldMap::useWebMapTiles()
{
	static const LLCachedControl<bool> use_web_map_tiles("UseWebMapTiles",false);
	return use_web_map_tiles &&
		   (( gHippoGridManager->getConnectedGrid()->isSecondLife() || sGotMapURL || !LFSimFeatureHandler::instance().mapServerURL().empty()));
}

void LLWorldMap::reloadItems(bool force)
{
	//LL_INFOS("World Map") << "LLWorldMap::reloadItems()" << LL_ENDL;
	if (clearItems(force))
	{
		LLWorldMapMessage::getInstance()->sendItemRequest(MAP_ITEM_TELEHUB);
		LLWorldMapMessage::getInstance()->sendItemRequest(MAP_ITEM_PG_EVENT);
		LLWorldMapMessage::getInstance()->sendItemRequest(MAP_ITEM_MATURE_EVENT);
		LLWorldMapMessage::getInstance()->sendItemRequest(MAP_ITEM_ADULT_EVENT);
		LLWorldMapMessage::getInstance()->sendItemRequest(MAP_ITEM_LAND_FOR_SALE);
	}
	if(!useWebMapTiles())
	{
		if(!mMapLoaded || force)
			sendMapLayerRequest();
	}
}


// static public
// Insert a region in the region map
// returns true if region inserted, false otherwise
// <FS:CR> Aurora Sim
//bool LLWorldMap::insertRegion(U32 x_world, U32 y_world, std::string& name, LLUUID& image_id, U32 accesscode, U64 region_flags)
bool LLWorldMap::insertRegion(U32 x_world, U32 y_world, U16 x_size, U16 y_size, std::string& name, LLUUID& image_id, U32 accesscode, U64 region_flags)
// </FS:CR> Aurora Sim
{
	// This region doesn't exist
	if (accesscode == 255)
	{
		// Checks if the track point is in it and invalidates it if it is
		if (LLWorldMap::getInstance()->isTrackingInRectangle( x_world, y_world, x_world + REGION_WIDTH_UNITS, y_world + REGION_WIDTH_UNITS))
		{
			LLWorldMap::getInstance()->setTrackingInvalid();
		}
		// return failure to insert
		return false;
	}
	else
	{
		U64 handle = to_region_handle(x_world, y_world);
	 	//LL_INFOS("World Map") << "Map sim : " << name << ", ID : " << image_id.getString() << LL_ENDL;
		// Insert the region in the region map of the world map
		// Loading the LLSimInfo object with what we got and insert it in the map
		LLSimInfo* siminfo = LLWorldMap::getInstance()->simInfoFromHandle(handle);
		if (siminfo == NULL)
		{
			siminfo = LLWorldMap::getInstance()->createSimInfoFromHandle(handle);
		}
		siminfo->setName(name);
		siminfo->setAccess(accesscode);
		siminfo->setRegionFlags(region_flags);
	//	siminfo->setWaterHeight((F32) water_height);
		siminfo->setLandForSaleImage(image_id);
// <FS:CR> Aurora Sim
		siminfo->setSize(x_size, y_size);
// </FS:CR> Aurora Sim

		// Handle the location tracking (for teleport, UI feedback and info display)
		if (LLWorldMap::getInstance()->isTrackingInRectangle( x_world, y_world, x_world + REGION_WIDTH_UNITS, y_world + REGION_WIDTH_UNITS))
		{
			if (siminfo->isDown())
			{
				// We were tracking this location, but it's no available
				LLWorldMap::getInstance()->setTrackingInvalid();
			}
			else
			{
				// We were tracking this location, and it does exist and is available
				LLWorldMap::getInstance()->setTrackingValid();
			}
		}
		// return insert region success
		return true;
	}
}

// static public
// Insert an item in the relevant region map
// returns true if item inserted, false otherwise
bool LLWorldMap::insertItem(U32 x_world, U32 y_world, std::string& name, LLUUID& uuid, U32 type, S32 extra, S32 extra2)
{
	// Create an item record for the received object
	LLItemInfo new_item((F32)x_world, (F32)y_world, name, uuid);

	// Compute a region handle based on the objects coordinates
	LLVector3d	pos((F32)x_world, (F32)y_world, 40.0);
	U64 handle = to_region_handle(pos);

	// Get the region record for that handle or NULL if we haven't browsed it yet
	LLSimInfo* siminfo = LLWorldMap::getInstance()->simInfoFromHandle(handle);
	if (siminfo == NULL)
	{
		siminfo = LLWorldMap::getInstance()->createSimInfoFromHandle(handle);
	}

	//LL_INFOS("World Map") << "Process item : type = " << type << LL_ENDL;
	switch (type)
	{
		case MAP_ITEM_TELEHUB: // telehubs
		{
			/* Merov: we are not using the hub color anymore for display so commenting that out
			// Telehub color
			U32 X = x_world / REGION_WIDTH_UNITS;
			U32 Y = y_world / REGION_WIDTH_UNITS;
			F32 red = fmod((F32)X * 0.11f, 1.f) * 0.8f;
			F32 green = fmod((F32)Y * 0.11f, 1.f) * 0.8f;
			F32 blue = fmod(1.5f * (F32)(X + Y) * 0.11f, 1.f) * 0.8f;
			F32 add_amt = (X % 2) ? 0.15f : -0.15f;
			add_amt += (Y % 2) ? -0.15f : 0.15f;
			LLColor4 color(red + add_amt, green + add_amt, blue + add_amt);
			new_item.setColor(color);
			*/
			
			// extra2 specifies whether this is an infohub or a telehub.
			if (extra2)
			{
				siminfo->insertInfoHub(new_item);
			}
			else
			{
				siminfo->insertTeleHub(new_item);
			}
			break;
		}
		case MAP_ITEM_PG_EVENT: // events
		case MAP_ITEM_MATURE_EVENT:
		case MAP_ITEM_ADULT_EVENT:
			{
				struct tm* timep;
				// Convert to Pacific, based on server's opinion of whether
				// it's daylight savings time there.
				timep = utc_to_pacific_time(extra, gPacificDaylightTime);

				S32 display_hour = timep->tm_hour % 12;
				if (display_hour == 0) display_hour = 12;

				new_item.setTooltip( llformat( "%d:%02d %s",
											  display_hour,
											  timep->tm_min,
											  (timep->tm_hour < 12 ? "AM" : "PM") ) );

			// HACK: store Z in extra2
			new_item.setElevation((F64)extra2);
			if (type == MAP_ITEM_PG_EVENT)
			{
				siminfo->insertPGEvent(new_item);
			}
			else if (type == MAP_ITEM_MATURE_EVENT)
			{
				siminfo->insertMatureEvent(new_item);
			}
			else if (type == MAP_ITEM_ADULT_EVENT)
			{
				siminfo->insertAdultEvent(new_item);
			}
			break;
		}
		case MAP_ITEM_LAND_FOR_SALE:		// land for sale
		case MAP_ITEM_LAND_FOR_SALE_ADULT:	// adult land for sale 
		{
			new_item.setTooltip(llformat("%d sq. m. %s%d (%.1f %s/sq. m.)", extra,
				gHippoGridManager->getConnectedGrid()->getCurrencySymbol().c_str(),
				extra2,
				(extra > 0) ? ((F32)extra2 / extra) : 0.f,
				gHippoGridManager->getConnectedGrid()->getCurrencySymbol().c_str()));

			if (type == MAP_ITEM_LAND_FOR_SALE)
			{
				siminfo->insertLandForSale(new_item);
			}
			else if (type == MAP_ITEM_LAND_FOR_SALE_ADULT)
			{
				siminfo->insertLandForSaleAdult(new_item);
			}
			break;
		}
		case MAP_ITEM_CLASSIFIED: // classifieds
		{
			//DEPRECATED: no longer used
			break;
		}
		case MAP_ITEM_AGENT_LOCATIONS: // agent locations
		{
// 				LL_INFOS("World Map") << "New Location " << new_item.mName << LL_ENDL;
			if (extra > 0)
			{
				new_item.setCount(extra);
				siminfo->insertAgentLocation(new_item);
			}
			break;
		}
		default:
			break;
	}
	return true;
}

bool LLWorldMap::isTrackingInRectangle(F64 x0, F64 y0, F64 x1, F64 y1)
{
	if (!mIsTrackingLocation)
		return false;
	return ((mTrackingLocation[0] >= x0) && (mTrackingLocation[0] < x1) && (mTrackingLocation[1] >= y0) && (mTrackingLocation[1] < y1));
}

// Drop priority of all images being fetched by the map
void LLWorldMap::dropImagePriorities()
{
	// Drop the download of tiles priority to nil
	mWorldMipmap.dropBoostLevels();
	// Same for the "land for sale" tiles per region
	for (sim_info_map_t::iterator it = mSimInfoMap.begin(); it != mSimInfoMap.end(); ++it)
	{
		LLSimInfo* info = it->second;
		info->dropImagePriority();
	}
}

// Load all regions in a given rectangle (in region grid coordinates, i.e. world / 256 meters)
void LLWorldMap::updateRegions(S32 x0, S32 y0, S32 x1, S32 y1)
{
	// Convert those boundaries to the corresponding (MAP_BLOCK_SIZE x MAP_BLOCK_SIZE) block coordinates
	U32 global_x0 = x0 / MAP_BLOCK_SIZE;
	U32 global_x1 = x1 / MAP_BLOCK_SIZE;
	U32 global_y0 = y0 / MAP_BLOCK_SIZE;
	U32 global_y1 = y1 / MAP_BLOCK_SIZE;

	//Singu note: There's a bunch of extra logic here, as opensim grids support sim coordinates that extend beyond the range
	// used on the official grid. We basically just extend what LL had here by nesting the mMapBlockLoaded array in a 'dynamic' grid,
	// essentially making that array a 'block' itself. An std::map was used to conserve memory, as we selectively only allocate desired
	// blocks, and although lookups aren't blazingly fast with that container, it isn't likeley to accumulate very many entries.

	//MapBlockRequest uses U16 for coordinate components.
	// In order not to exceed U16_MAX values, MAP_BLOCK_RES*MAP_BLOCK_SIZE*(i or j) can't exceed U16_MAX(65535)
	U32 max_range = (U16_MAX+1)/MAP_BLOCK_RES/MAP_BLOCK_SIZE - 1;

	//Desired coordinate ranges in our 'dynamic' grid of 512x512 grids of 4x4 sim blocks.
	U32 map_block_x0 = global_x0 / MAP_BLOCK_RES;
	U32 map_block_x1 = llmin(global_x1 / MAP_BLOCK_RES, max_range);
	U32 map_block_y0 = global_y0 / MAP_BLOCK_RES;
	U32 map_block_y1 = llmin(global_y1 / MAP_BLOCK_RES, max_range);

	const bool layer_start = useWebMapTiles() ? SIM_LAYER_OVERLAY : SIM_LAYER_BEGIN;
	for (U32 i = map_block_x0; i <= map_block_x1; ++i)
	{
	for (U32 j = map_block_y0; j <= map_block_y1; ++j)
	{

	//Desired coordinate ranges in our 512x512 grids of 4x4 sim blocks.
	x0 = global_x0 - i * MAP_BLOCK_RES;	
	x1 = llmin(global_x1 - i * (U32)MAP_BLOCK_RES, (U32)MAP_BLOCK_RES-1);
	y0 = global_y0 - j * MAP_BLOCK_RES;
	y1 = llmin(global_y1 - j * (U32)MAP_BLOCK_RES, (U32)MAP_BLOCK_RES-1);

	for(U32 layer = layer_start;layer<SIM_LAYER_COUNT;++layer)
	{
	std::vector<bool> &block = mMapBlockMap[layer][(i << 16) | j];
	if(block.empty())
	{
		//New block. Allocate the array and set all entries to false. (seen as mMapBlockLoaded in v3)
		block.resize(MAP_BLOCK_RES*MAP_BLOCK_RES,false);
	}

	// Load the region info those blocks
	for (S32 block_x = llmax(x0, 0); block_x <= llmin(x1, MAP_BLOCK_RES-1); ++block_x)
	{
		for (S32 block_y = llmax(y0, 0); block_y <= llmin(y1, MAP_BLOCK_RES-1); ++block_y)
		{
			S32 offset = block_x | (block_y * MAP_BLOCK_RES);
			// if(!mMapBlockLoaded[LLWorldMap::getInstance()->mCurrentMap][offset])
			if (!block[offset])
			{
				U16 min_x = (block_x + i * MAP_BLOCK_RES) * MAP_BLOCK_SIZE;
				U16 max_x = min_x + MAP_BLOCK_SIZE - 1;
				U16 min_y = (block_y + j * MAP_BLOCK_RES) * MAP_BLOCK_SIZE;
				U32 max_y = min_y + MAP_BLOCK_SIZE - 1;

 				//LL_INFOS("World Map") << "Loading Block (" << block_x << "," << block_y << ")" << LL_ENDL;
				LLWorldMapMessage::getInstance()->sendMapBlockRequest(min_x, min_y, max_x, max_y, false, layerToFlags((sim_layer_type)layer));
				block[offset] = true;
			}
		}
	}
	}
	}
	}
}

void LLWorldMap::dump()
{
	LL_INFOS("World Map") << "LLWorldMap::dump()" << LL_ENDL;
	for (sim_info_map_t::iterator it = mSimInfoMap.begin(); it != mSimInfoMap.end(); ++it)
	{
		LLSimInfo* info = it->second;
		if (info)
		{
			info->dump();
		}
	}
}
