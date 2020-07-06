/** 
 * @file llviewerobjectlist.cpp
 * @brief Implementation of LLViewerObjectList class.
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

#include "llviewerobjectlist.h"

#include "message.h"
#include "timing.h"
#include "llfasttimer.h"
#include "llrender.h"
#include "llwindow.h"		// decBusyCount()

#include "llviewercontrol.h"
#include "llface.h"
#include "llvoavatarself.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llnetmap.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llhoverview.h"
#include "llworld.h"
#include "llstring.h"
#include "llhudnametag.h"
#include "lldrawable.h"
#include "llflexibleobject.h"
#include "llviewertextureanim.h"
#include "xform.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llselectmgr.h"
#include "llresmgr.h"
#include "llsdutil.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerstatsrecorder.h"
#include "llvovolume.h"
#include "llvoavatarself.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llkeyboard.h"
#include "u64.h"
#include "llviewertexturelist.h"
#include "lldatapacker.h"
#ifdef LL_STANDALONE
#include <zlib.h>
#else
#include "zlib/zlib.h"
#endif
#include "object_flags.h"

#include "llappviewer.h"
#include "llfloaterblacklist.h"

#include "llviewerobjectbackup.h"

extern F32 gMinObjectDistance;
extern BOOL gAnimateTextures;

#include "importtracker.h"
extern ImportTracker gImportTracker;

#define MAX_CONCURRENT_PHYSICS_REQUESTS 256

void dialog_refresh_all();

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy objectCostResponder_timeout;
extern AIHTTPTimeoutPolicy physicsFlagsResponder_timeout;

// <singu>
#include "llimpanel.h"
#include "llimview.h"
LLFloaterIMPanel* find_im_floater(const LLUUID& id)
{
	return gIMMgr->findFloaterBySession(id ^ gAgentID);
}
// </singu>

#define CULL_VIS
//#define ORPHAN_SPAM
//#define IGNORE_DEAD

// Global lists of objects - should go away soon.
LLViewerObjectList gObjectList;

extern LLPipeline	gPipeline;

// Statics for object lookup tables.
U32						LLViewerObjectList::sSimulatorMachineIndex = 1; // Not zero deliberately, to speed up index check.
std::map<U64, U32>			LLViewerObjectList::sIPAndPortToIndex;
std::map<U64, LLUUID>	LLViewerObjectList::sIndexAndLocalIDToUUID;
LLStat					LLViewerObjectList::sCacheHitRate("object_cache_hits", 128);

LLViewerObjectList::LLViewerObjectList()
{
	mNumVisCulled = 0;
	mNumSizeCulled = 0;
	mCurLazyUpdateIndex = 0;
	mCurBin = 0;
	mNumDeadObjects = 0;
	mMinNumDeadObjects = 20;
	mNumOrphans = 0;
	mNumNewObjects = 0;
	mWasPaused = FALSE;
	mNumDeadObjectUpdates = 0;
	mNumUnknownKills = 0;
	mNumUnknownUpdates = 0;
}

LLViewerObjectList::~LLViewerObjectList()
{
	destroy();
}

void LLViewerObjectList::destroy()
{
	killAllObjects();

	resetObjectBeacons();
	mActiveObjects.clear();
	mDeadObjects.clear();
	mMapObjects.clear();
	mUUIDObjectMap.clear();
	mUUIDAvatarMap.clear();
}


void LLViewerObjectList::getUUIDFromLocal(LLUUID &id,
										  const U32 local_id,
										  const U32 ip,
										  const U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index = sIPAndPortToIndex[ipport];

	if (!index)
	{
		index = sSimulatorMachineIndex++;
		sIPAndPortToIndex[ipport] = index;
	}

	U64	indexid = (((U64)index) << 32) | (U64)local_id;

	id = get_if_there(sIndexAndLocalIDToUUID, indexid, LLUUID::null);
}

U64 LLViewerObjectList::getIndex(const U32 local_id,
								 const U32 ip,
								 const U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index = sIPAndPortToIndex[ipport];

	if (!index)
	{
		return 0;
	}

	return (((U64)index) << 32) | (U64)local_id;
}

BOOL LLViewerObjectList::removeFromLocalIDTable(const LLViewerObject* objectp)
{
	if(objectp && objectp->getRegion())
	{
		U32 local_id = objectp->mLocalID;		
		U32 ip = objectp->getRegion()->getHost().getAddress();
		U32 port = objectp->getRegion()->getHost().getPort();
		U64 ipport = (((U64)ip) << 32) | (U64)port;
		U32 index = sIPAndPortToIndex[ipport];
		
		// LL_INFOS() << "Removing object from table, local ID " << local_id << ", ip " << ip << ":" << port << LL_ENDL;
		
		U64	indexid = (((U64)index) << 32) | (U64)local_id;
		
		std::map<U64, LLUUID>::iterator iter = sIndexAndLocalIDToUUID.find(indexid);
		if (iter == sIndexAndLocalIDToUUID.end())
		{
			return FALSE;
		}
		
		// Found existing entry
		if (iter->second == objectp->getID())
		{   // Full UUIDs match, so remove the entry
			sIndexAndLocalIDToUUID.erase(iter);
			return TRUE;
		}
		// UUIDs did not match - this would zap a valid entry, so don't erase it
		//LL_INFOS() << "Tried to erase entry where id in table (" 
		//		<< iter->second	<< ") did not match object " << object.getID() << LL_ENDL;
	}
	
	return FALSE ;
}

void LLViewerObjectList::setUUIDAndLocal(const LLUUID &id,
										  const U32 local_id,
										  const U32 ip,
										  const U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index = sIPAndPortToIndex[ipport];

	if (!index)
	{
		index = sSimulatorMachineIndex++;
		sIPAndPortToIndex[ipport] = index;
	}

	U64	indexid = (((U64)index) << 32) | (U64)local_id;

	sIndexAndLocalIDToUUID[indexid] = id;
	
	//LL_INFOS() << "Adding object to table, full ID " << id
	//	<< ", local ID " << local_id << ", ip " << ip << ":" << port << LL_ENDL;
}

S32 gFullObjectUpdates = 0;
S32 gTerseObjectUpdates = 0;

void LLViewerObjectList::processUpdateCore(LLViewerObject* objectp, 
										   void** user_data, 
										   U32 i, 
										   const EObjectUpdateType update_type, 
										   LLDataPacker* dpp, 
										   BOOL just_created)
{
	LLMessageSystem* msg = gMessageSystem;

	// ignore returned flags
	objectp->processUpdateMessage(msg, user_data, i, update_type, dpp);
		
	if (objectp->isDead())
	{
		// The update failed
		return;
	}

	updateActive(objectp);

	if (just_created) 
	{
		gPipeline.addObject(objectp);
	}
	else
	{
		LLObjectBackup::primUpdate(objectp);
	}
	

	// Also sets the approx. pixel area
	objectp->setPixelAreaAndAngle(gAgent);

	// RN: this must be called after we have a drawable 
	// (from gPipeline.addObject)
	// so that the drawable parent is set properly
	if(msg != NULL)
	{
		findOrphans(objectp, msg->getSenderIP(), msg->getSenderPort());
	}
	else
	{
		LLViewerRegion* regionp = objectp->getRegion();
		if(regionp != NULL)
		{
			findOrphans(objectp, regionp->getHost().getAddress(), regionp->getHost().getPort());
		}
	}

	
	if(just_created && objectp &&
	(gImportTracker.getState() == ImportTracker::WAND /*||
	gImportTracker.getState() == ImportTracker::BUILDING*/) &&
	objectp->mCreateSelected && objectp->permYouOwner() &&
	objectp->permModify() && objectp->permCopy() && objectp->permTransfer())
	{
		gImportTracker.get_update(objectp->mLocalID, just_created, objectp->mCreateSelected);
	}

	// If we're just wandering around, don't create new objects selected.
	if (just_created 
		&& update_type != OUT_TERSE_IMPROVED 
		&& objectp->mCreateSelected)
	{
		if ( LLToolMgr::getInstance()->getCurrentTool() != LLToolPie::getInstance() )
		{
			// LL_INFOS() << "DEBUG selecting " << objectp->mID << " " 
			// << objectp->mLocalID << LL_ENDL;
			LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
			dialog_refresh_all();
		}

		objectp->mCreateSelected = false;
		gViewerWindow->getWindow()->decBusyCount();
		gViewerWindow->getWindow()->setCursor( UI_CURSOR_ARROW );
		
		LLObjectBackup::newPrim(objectp);
	}
}

static LLTrace::BlockTimerStatHandle FTM_PROCESS_OBJECTS("Process Objects");

void LLViewerObjectList::processObjectUpdate(LLMessageSystem *mesgsys,
											 void **user_data,
											 const EObjectUpdateType update_type,
											 bool cached, bool compressed)
{
	LL_RECORD_BLOCK_TIME(FTM_PROCESS_OBJECTS);	
	
	LLViewerObject *objectp;
	S32			num_objects;
	U32			local_id;
	LLPCode		pcode = 0;
	LLUUID		fullid;
	S32			i;

	// figure out which simulator these are from and get it's index
	// Coordinates in simulators are region-local
	// Until we get region-locality working on viewer we
	// have to transform to absolute coordinates.
	num_objects = mesgsys->getNumberOfBlocksFast(_PREHASH_ObjectData);

	// I don't think this case is ever hit.  TODO* Test this.
	if (!cached && !compressed && update_type != OUT_FULL)
	{
		//LL_INFOS() << "TEST: !cached && !compressed && update_type != OUT_FULL" << LL_ENDL;
		gTerseObjectUpdates += num_objects;
		/*
		S32 size;
		if (mesgsys->getReceiveCompressedSize())
		{
			size = mesgsys->getReceiveCompressedSize();
		}
		else
		{
			size = mesgsys->getReceiveSize();
		}
		LL_INFOS() << "Received terse " << num_objects << " in " << size << " byte (" << size/num_objects << ")" << LL_ENDL;
		*/
	}
	else
	{
		/*
		S32 size;
		if (mesgsys->getReceiveCompressedSize())
		{
			size = mesgsys->getReceiveCompressedSize();
		}
		else
		{
			size = mesgsys->getReceiveSize();
		}

		LL_INFOS() << "Received " << num_objects << " in " << size << " byte (" << size/num_objects << ")" << LL_ENDL;
		*/
		gFullObjectUpdates += num_objects;
	}

	U64 region_handle;
	mesgsys->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromHandle(region_handle);

	if (!regionp)
	{
		LL_WARNS() << "Object update from unknown region! " << region_handle << LL_ENDL;
		return;
	}

	U8 compressed_dpbuffer[2048];
	LLDataPackerBinaryBuffer compressed_dp(compressed_dpbuffer, 2048);
	LLDataPacker *cached_dpp = NULL;
	LLViewerStatsRecorder& recorder = LLViewerStatsRecorder::instance();
	
	for (i = 0; i < num_objects; i++)
	{
		// timer is unused?
		LLTimer update_timer;
		BOOL justCreated = FALSE;
		S32	msg_size = 0;

		if (cached)
		{
			U32 id;
			U32 crc;
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, id, i);
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_CRC, crc, i);
			msg_size += sizeof(U32) * 2;
		
			// Lookup data packer and add this id to cache miss lists if necessary.
			U8 cache_miss_type = LLViewerRegion::CACHE_MISS_TYPE_NONE;
			cached_dpp = regionp->getDP(id, crc, cache_miss_type);
			if (cached_dpp)
			{
				// Cache Hit.
				cached_dpp->reset();
				cached_dpp->unpackUUID(fullid, "ID");
				cached_dpp->unpackU32(local_id, "LocalID");
				cached_dpp->unpackU8(pcode, "PCode");
			}
			else
			{
				// Cache Miss.
				recorder.cacheMissEvent(id, update_type, cache_miss_type, msg_size);

				continue; // no data packer, skip this object
			}
		}
		else if (compressed)
		{
			S32							uncompressed_length = 2048;
			compressed_dp.reset();
			
			uncompressed_length = mesgsys->getSizeFast(_PREHASH_ObjectData, i, _PREHASH_Data);
			mesgsys->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_Data, compressed_dpbuffer, 0, i, 2048);
			compressed_dp.assignBuffer(compressed_dpbuffer, uncompressed_length);

			if (update_type != OUT_TERSE_IMPROVED) // OUT_FULL_COMPRESSED only?
			{
				U32 flags = 0;
				mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_UpdateFlags, flags, i);
				compressed_dp.unpackUUID(fullid, "ID");
				compressed_dp.unpackU32(local_id, "LocalID");
				compressed_dp.unpackU8(pcode, "PCode");
			}
			else //OUT_TERSE_IMPROVED
			{
				compressed_dp.unpackU32(local_id, "LocalID");
				getUUIDFromLocal(fullid,
								 local_id,
								 gMessageSystem->getSenderIP(),
								 gMessageSystem->getSenderPort());
				if (fullid.isNull())
				{
					LL_DEBUGS() << "update for unknown localid " << local_id << " host " << gMessageSystem->getSender() << ":" << gMessageSystem->getSenderPort() << LL_ENDL;
					mNumUnknownUpdates++;
				}
			}
		}
		else if (update_type != OUT_FULL) // !compressed, !OUT_FULL ==> OUT_FULL_CACHED only?
		{
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);
			msg_size += sizeof(U32);

			getUUIDFromLocal(fullid,
							local_id,
							gMessageSystem->getSenderIP(),
							gMessageSystem->getSenderPort());
			if (fullid.isNull())
			{
				// LL_WARNS() << "update for unknown localid " << local_id << " host " << gMessageSystem->getSender() << LL_ENDL;
				mNumUnknownUpdates++;
			}
		}
		else // OUT_FULL only?
		{
			mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_FullID, fullid, i);
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);
			msg_size += sizeof(LLUUID);
			msg_size += sizeof(U32);
			// LL_INFOS() << "Full Update, obj " << local_id << ", global ID" << fullid << "from " << mesgsys->getSender() << LL_ENDL;
		}
		objectp = findObject(fullid);

		// This looks like it will break if the local_id of the object doesn't change
		// upon boundary crossing, but we check for region id matching later...
		// Reset object local id and region pointer if things have changed
		if (objectp && 
			((objectp->mLocalID != local_id) ||
			 (objectp->getRegion() != regionp)))
		{
			//if (objectp->getRegion())
			//{
			//	LL_INFOS() << "Local ID change: Removing object from table, local ID " << objectp->mLocalID 
			//			<< ", id from message " << local_id << ", from " 
			//			<< LLHost(objectp->getRegion()->getHost().getAddress(), objectp->getRegion()->getHost().getPort())
			//			<< ", full id " << fullid 
			//			<< ", objects id " << objectp->getID()
			//			<< ", regionp " << (U32) regionp << ", object region " << (U32) objectp->getRegion()
			//			<< LL_ENDL;
			//}
			removeFromLocalIDTable(objectp);
			setUUIDAndLocal(fullid,
							local_id,
							gMessageSystem->getSenderIP(),
							gMessageSystem->getSenderPort());
			
			if (objectp->mLocalID != local_id)
			{    // Update local ID in object with the one sent from the region
				objectp->mLocalID = local_id;
			}
			
			if (objectp->getRegion() != regionp)
			{    // Object changed region, so update it
				objectp->updateRegion(regionp); // for LLVOAvatar
			}
		}

		if (!objectp)
		{
			if (compressed)
			{
				if (update_type == OUT_TERSE_IMPROVED)
				{
					// LL_INFOS() << "terse update for an unknown object (compressed):" << fullid << LL_ENDL;
					recorder.objectUpdateFailure(local_id, update_type, msg_size);
					continue;
				}
			}
			else if (cached) // Cache hit only?
			{
			}
			else
			{
				if (update_type != OUT_FULL)
				{
					//LL_INFOS() << "terse update for an unknown object:" << fullid << LL_ENDL;
					recorder.objectUpdateFailure(local_id, update_type, msg_size);
					continue;
				}

				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_PCode, pcode, i);
				msg_size += sizeof(U8);

			}
#ifdef IGNORE_DEAD
			if (mDeadObjects.find(fullid) != mDeadObjects.end())
			{
				mNumDeadObjectUpdates++;
				//LL_INFOS() << "update for a dead object:" << fullid << LL_ENDL;
				recorder.objectUpdateFailure(local_id, update_type, msg_size);
				continue;
			}
#endif


			if(std::find(LLFloaterBlacklist::blacklist_objects.begin(),
				LLFloaterBlacklist::blacklist_objects.end(),fullid) != LLFloaterBlacklist::blacklist_objects.end())
			{
				//LL_INFOS() << "Blacklisted object asset " << fullid.asString() << " blocked." << LL_ENDL; 
				continue;
			}


			objectp = createObject(pcode, regionp, fullid, local_id, gMessageSystem->getSender());
			if (!objectp)
			{
				LL_INFOS() << "createObject failure for object: " << fullid << LL_ENDL;
				recorder.objectUpdateFailure(local_id, update_type, msg_size);
				continue;
			}
			justCreated = TRUE;
			mNumNewObjects++;
			sCacheHitRate.addValue(cached ? 100.f : 0.f);

		}


		if (objectp->isDead())
		{
			LL_WARNS() << "Dead object " << objectp->mID << " in UUID map 1!" << LL_ENDL;
		}

		bool bCached = false;
		if (compressed)
		{
			if (update_type != OUT_TERSE_IMPROVED) // OUT_FULL_COMPRESSED only?
			{
				objectp->mLocalID = local_id;
			}
			processUpdateCore(objectp, user_data, i, update_type, &compressed_dp, justCreated);
			if (update_type != OUT_TERSE_IMPROVED) // OUT_FULL_COMPRESSED only?
			{
				bCached = true;
				LLViewerRegion::eCacheUpdateResult result = objectp->mRegionp->cacheFullUpdate(objectp, compressed_dp);
				recorder.cacheFullUpdate(local_id, update_type, result, objectp, msg_size);
			}
		}
		else if (cached)
		{
			objectp->mLocalID = local_id;
			processUpdateCore(objectp, user_data, i, update_type, cached_dpp, justCreated);
		}
		else
		{
			if (update_type == OUT_FULL)
			{
				objectp->mLocalID = local_id;
			}
			processUpdateCore(objectp, user_data, i, update_type, NULL, justCreated);
		}
		recorder.objectUpdateEvent(local_id, update_type, objectp, msg_size);
		objectp->setLastUpdateType(update_type);
		objectp->setLastUpdateCached(bCached);
	}

	recorder.log(0.2f);

	LLVOAvatar::cullAvatarsByPixelArea();
}

void LLViewerObjectList::processCompressedObjectUpdate(LLMessageSystem *mesgsys,
											 void **user_data,
											 const EObjectUpdateType update_type)
{
	processObjectUpdate(mesgsys, user_data, update_type, false, true);
}

void LLViewerObjectList::processCachedObjectUpdate(LLMessageSystem *mesgsys,
											 void **user_data,
											 const EObjectUpdateType update_type)
{
	processObjectUpdate(mesgsys, user_data, update_type, true, false);
}	

void LLViewerObjectList::dirtyAllObjectInventory()
{
	for (vobj_list_t::iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
	{
		(*iter)->dirtyInventory();
	}
}

void LLViewerObjectList::updateApparentAngles(LLAgent &agent)
{
	S32 i;
	S32 num_objects = 0;
	LLViewerObject *objectp;

	S32 num_updates, max_value;
	if (NUM_BINS - 1 == mCurBin)
	{
		num_updates = (S32) mObjects.size() - mCurLazyUpdateIndex;
		max_value = (S32) mObjects.size();
		gTextureList.setUpdateStats(TRUE);
	}
	else
	{
		num_updates = ((S32) mObjects.size() / NUM_BINS) + 1;
		max_value = llmin((S32) mObjects.size(), mCurLazyUpdateIndex + num_updates);
	}


	// Slam priorities for textures that we care about (hovered, selected, and focused)
	// Hovered
	// Assumes only one level deep of parenting
	LLSelectNode* nodep = LLSelectMgr::instance().getHoverNode();
	if (nodep)
	{
		objectp = nodep->getObject();
		if (objectp)
		{
			objectp->boostTexturePriority();
		}
	}

	// Focused
	objectp = gAgentCamera.getFocusObject();
	if (objectp)
	{
		objectp->boostTexturePriority();
	}

	// Selected
	struct f : public LLSelectedObjectFunctor
	{
		virtual bool apply(LLViewerObject* objectp)
		{
			objectp->boostTexturePriority();
			return true;
		}
	} func;
	LLSelectMgr::getInstance()->getSelection()->applyToRootObjects(&func);

	// Iterate through some of the objects and lazy update their texture priorities
	for (i = mCurLazyUpdateIndex; i < max_value; i++)
	{
		objectp = mObjects[i];
		if (!objectp->isDead())
		{
			num_objects++;

			//  Update distance & gpw 
			objectp->setPixelAreaAndAngle(agent); // Also sets the approx. pixel area
			objectp->updateTextures();	// Update the image levels of textures for this object.
		}
	}

	mCurLazyUpdateIndex = max_value;
	if (mCurLazyUpdateIndex == mObjects.size())
	{
		mCurLazyUpdateIndex = 0;
	}

	mCurBin = (mCurBin + 1) % NUM_BINS;

	LLVOAvatar::cullAvatarsByPixelArea();
}

class LLObjectCostResponder : public LLHTTPClient::ResponderWithResult
{
public:
	LLObjectCostResponder(const LLSD& object_ids)
		: mObjectIDs(object_ids)
	{
	}

	// Clear's the global object list's pending
	// request list for all objects requested
	void clear_object_list_pending_requests()
	{
		// TODO*: No more hard coding
		for (auto const& id : mObjectIDs.array())
		{
			gObjectList.onObjectCostFetchFailure(id.asUUID());
		}
	}

	/*virtual*/ void httpFailure(void)
	{
		LL_WARNS()
			<< "Transport error requesting object cost "
			<< "HTTP status: " << mStatus << ", reason: "
			<< mReason << "." << LL_ENDL;

		// TODO*: Error message to user
		// For now just clear the request from the pending list
		clear_object_list_pending_requests();
	}

	/*virtual*/ void httpSuccess(void)
	{
		if ( !mContent.isMap() || mContent.has("error") )
		{
			// Improper response or the request had an error,
			// show an error to the user?
			LL_WARNS()
				<< "Application level error when fetching object "
				<< "cost. Message: " << mContent["error"]["message"].asString()
				<< ", identifier: " << mContent["error"]["identifier"].asString()
				<< LL_ENDL;

			// TODO*: Adaptively adjust request size if the
			// service says we've requested too many and retry

			// TODO*: Error message if not retrying
			clear_object_list_pending_requests();
			return;
		}

		// Success, grab the resource cost and linked set costs
		// for an object if one was returned
		for (auto const& entry : mObjectIDs.array())
		{
			LLUUID object_id = entry.asUUID();

			// Check to see if the request contains data for the object
			if ( mContent.has(entry.asString()) )
			{
				F32 link_cost =
					mContent[entry.asString()]["linked_set_resource_cost"].asReal();
				F32 object_cost =
					mContent[entry.asString()]["resource_cost"].asReal();

				F32 physics_cost = mContent[entry.asString()]["physics_cost"].asReal();
				F32 link_physics_cost = mContent[entry.asString()]["linked_set_physics_cost"].asReal();

				gObjectList.updateObjectCost(object_id, object_cost, link_cost, physics_cost, link_physics_cost);
			}
			else
			{
				// TODO*: Give user feedback about the missing data?
				gObjectList.onObjectCostFetchFailure(object_id);
			}
		}
	}

	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return objectCostResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "LLObjectCostResponder"; }

private:
	LLSD mObjectIDs;
};

class LLPhysicsFlagsResponder : public LLHTTPClient::ResponderWithResult
{
public:
	LLPhysicsFlagsResponder(const LLSD& object_ids)
		: mObjectIDs(object_ids)
	{
	}

	// Clear's the global object list's pending
	// request list for all objects requested
	void clear_object_list_pending_requests()
	{
		// TODO*: No more hard coding
		for (auto const& id : mObjectIDs.array())
		{
			gObjectList.onPhysicsFlagsFetchFailure(id.asUUID());
		}
	}

	/*virtual*/ void httpFailure(void)
	{
		LL_WARNS()
			<< "Transport error requesting object physics flags "
			<< "HTTP status: " << mStatus << ", reason: "
			<< mReason << "." << LL_ENDL;

		// TODO*: Error message to user
		// For now just clear the request from the pending list
		clear_object_list_pending_requests();
	}

	/*virtual*/ void httpSuccess(void)
	{
		if ( !mContent.isMap() || mContent.has("error") )
		{
			// Improper response or the request had an error,
			// show an error to the user?
			LL_WARNS()
				<< "Application level error when fetching object "
				<< "physics flags.  Message: " << mContent["error"]["message"].asString()
				<< ", identifier: " << mContent["error"]["identifier"].asString()
				<< LL_ENDL;

			// TODO*: Adaptively adjust request size if the
			// service says we've requested too many and retry

			// TODO*: Error message if not retrying
			clear_object_list_pending_requests();
			return;
		}

		// Success, grab the resource cost and linked set costs
		// for an object if one was returned
		for (auto const& entry : mObjectIDs.array())
		{
			LLUUID object_id = entry.asUUID();

			// Check to see if the request contains data for the object
			if (mContent.has(entry.asString()))
			{
				const LLSD& data = mContent[entry.asString()];

				S32 shape_type = data["PhysicsShapeType"].asInteger();

				gObjectList.updatePhysicsShapeType(object_id, shape_type);

				if (data.has("Density"))
				{
					F32 density = data["Density"].asReal();
					F32 friction = data["Friction"].asReal();
					F32 restitution = data["Restitution"].asReal();
					F32 gravity_multiplier = data["GravityMultiplier"].asReal();
					
					gObjectList.updatePhysicsProperties(object_id, 
						density, friction, restitution, gravity_multiplier);
				}
			}
			else
			{
				// TODO*: Give user feedback about the missing data?
				gObjectList.onPhysicsFlagsFetchFailure(object_id);
			}
		}
	}

	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return physicsFlagsResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "LLPhysicsFlagsResponder"; }

private:
	LLSD mObjectIDs;
};

void LLViewerObjectList::update(LLAgent &agent, LLWorld &world)
{
	// Update globals
	static const LLCachedControl<bool> VelocityInterpolate("VelocityInterpolate");
	static const LLCachedControl<bool> PingInterpolate("PingInterpolate");
	LLViewerObject::setVelocityInterpolate( VelocityInterpolate );
	LLViewerObject::setPingInterpolate( PingInterpolate );
	
	static LLCachedControl<F32> interp_time("InterpolationTime");
	static LLCachedControl<F32> phase_out_time("InterpolationPhaseOut");

	if (interp_time < 0.0 || 
		phase_out_time < 0.0 ||
		phase_out_time > interp_time)
	{
		LL_WARNS() << "Invalid values for InterpolationTime or InterpolationPhaseOut, resetting to defaults" << LL_ENDL;
		interp_time = 3.0f;
		phase_out_time = 1.0f;
	}
	LLViewerObject::setPhaseOutUpdateInterpolationTime( interp_time );
	LLViewerObject::setMaxUpdateInterpolationTime( phase_out_time );

	static const LLCachedControl<bool> AnimateTextures("AnimateTextures");
	gAnimateTextures = AnimateTextures;

	// update global timer
	F32 last_time = gFrameTimeSeconds;
	U64Microseconds time = totalTime();				 // this will become the new gFrameTime when the update is done
	// Time _can_ go backwards, for example if the user changes the system clock.
	// It doesn't cause any fatal problems (just some oddness with stats), so we shouldn't assert here.
//	llassert(time > gFrameTime);
	F64Seconds time_diff = time - gFrameTime;
	gFrameTime	= time;
	F64Seconds time_since_start = gFrameTime - gStartTime;
	gFrameTimeSeconds = time_since_start;

	gFrameIntervalSeconds = gFrameTimeSeconds - last_time;
	if (gFrameIntervalSeconds < 0.f)
	{
		gFrameIntervalSeconds = 0.f;
	}

	//clear avatar LOD change counter
	LLVOAvatar::sNumLODChangesThisFrame = 0;

	const F64 frame_time = LLFrameTimer::getElapsedSeconds();
	
	LLViewerObject *objectp = NULL;	
	
	// Make a copy of the list in case something in idleUpdate() messes with it
	static std::vector<LLViewerObject*> idle_list;
	if(mActiveObjects.size() > idle_list.capacity())
		idle_list.reserve( mActiveObjects.size() );

	U32 idle_count = 0;
		
	static LLTrace::BlockTimerStatHandle idle_copy("Idle Copy");

	{
		LL_RECORD_BLOCK_TIME(idle_copy);

 		for (std::vector<LLPointer<LLViewerObject> >::iterator active_iter = mActiveObjects.begin();
			active_iter != mActiveObjects.end(); active_iter++)
		{
			objectp = *active_iter;
			if (objectp)
			{
				if (idle_count >= idle_list.size())
				{
					idle_list.push_back( objectp );
				}
				else
				{
					idle_list[idle_count] = objectp;
				}
				++idle_count;
			}
			else
			{	// There shouldn't be any NULL pointers in the list, but they have caused
				// crashes before.  This may be idleUpdate() messing with the list.
				LL_WARNS() << "LLViewerObjectList::update has a NULL objectp" << LL_ENDL;
			}
		}
	}
	
	std::vector<LLViewerObject*>::iterator idle_end = idle_list.begin()+idle_count;

	static const LLCachedControl<bool> freeze_time("FreezeTime",0);
	if (freeze_time)
	{
		for (std::vector<LLViewerObject*>::iterator iter = idle_list.begin();
			iter != idle_end; iter++)
		{
			objectp = *iter;
			if (
#if ENABLE_CLASSIC_CLOUDS
				objectp->getPCode() == LLViewerObject::LL_VO_CLOUDS ||
#endif
				objectp->isAvatar())
			{
				objectp->idleUpdate(agent, world, frame_time);
			}
		}
	}
	else
	{
		for (std::vector<LLViewerObject*>::iterator idle_iter = idle_list.begin();
			idle_iter != idle_end; idle_iter++)
		{
			objectp = *idle_iter;
			llassert(objectp->isActive());
			objectp->idleUpdate(agent, world, frame_time);

		}

		//update flexible objects
		LLVolumeImplFlexible::updateClass();

		//update animated textures
		if (gAnimateTextures)
		{
			LLViewerTextureAnim::updateClass();
		}
	}



	fetchObjectCosts();
	fetchPhysicsFlags();

	mNumSizeCulled = 0;
	mNumVisCulled = 0;

	// update max computed render cost
	LLVOVolume::updateRenderComplexity();

	// compute all sorts of time-based stats
	// don't factor frames that were paused into the stats
	if (! mWasPaused)
	{
		LLViewerStats::getInstance()->updateFrameStats(time_diff.value());
	}

	/*
	// Debugging code for viewing orphans, and orphaned parents
	LLUUID id;
	for (i = 0; i < mOrphanParents.count(); i++)
	{
		id = sIndexAndLocalIDToUUID[mOrphanParents[i]];
		LLViewerObject *objectp = findObject(id);
		if (objectp)
		{
			std::string id_str;
			objectp->mID.toString(id_str);
			std::string tmpstr = std::string("Par:    ") + id_str;
			addDebugBeacon(objectp->getPositionAgent(),
							tmpstr,
							LLColor4(1.f,0.f,0.f,1.f),
							LLColor4(1.f,1.f,1.f,1.f));
		}
	}

	LLColor4 text_color;
	for (i = 0; i < mOrphanChildren.count(); i++)
	{
		OrphanInfo oi = mOrphanChildren[i];
		LLViewerObject *objectp = findObject(oi.mChildInfo);
		if (objectp)
		{
			std::string id_str;
			objectp->mID.toString(id_str);
			std::string tmpstr;
			if (objectp->getParent())
			{
				tmpstr = std::string("ChP:    ") + id_str;
				text_color = LLColor4(0.f, 1.f, 0.f, 1.f);
			}
			else
			{
				tmpstr = std::string("ChNoP:    ") + id_str;
				text_color = LLColor4(1.f, 0.f, 0.f, 1.f);
			}
			id = sIndexAndLocalIDToUUID[oi.mParentInfo];
			addDebugBeacon(objectp->getPositionAgent() + LLVector3(0.f, 0.f, -0.25f),
							tmpstr,
							LLColor4(0.25f,0.25f,0.25f,1.f),
							text_color);
		}
		i++;
	}
	*/

	LLViewerStats::getInstance()->mNumObjectsStat.addValue((S32) mObjects.size() - mNumDeadObjects);
	LLViewerStats::getInstance()->mNumActiveObjectsStat.addValue(idle_count);
	LLViewerStats::getInstance()->mNumSizeCulledStat.addValue(mNumSizeCulled);
	LLViewerStats::getInstance()->mNumVisCulledStat.addValue(mNumVisCulled);
}

void LLViewerObjectList::fetchObjectCosts()
{
	// issue http request for stale object physics costs
	if (!mStaleObjectCost.empty())
	{
		LLViewerRegion* regionp = gAgent.getRegion();

		if (regionp)
		{
			std::string url = regionp->getCapability("GetObjectCost");

			if (!url.empty())
			{
				LLSD id_list;
				U32 object_index = 0;

				for (
					auto iter = mStaleObjectCost.begin();
					iter != mStaleObjectCost.end();
					)
				{
					// Check to see if a request for this object
					// has already been made.
					if ( mPendingObjectCost.find(*iter) ==
						 mPendingObjectCost.end() )
					{
						mPendingObjectCost.insert(*iter);
						id_list[object_index++] = *iter;
					}

					mStaleObjectCost.erase(iter++);

					if (object_index >= MAX_CONCURRENT_PHYSICS_REQUESTS)
					{
						break;
					}
				}
									
				if ( id_list.size() > 0 )
				{
					LLSD post_data = LLSD::emptyMap();

					post_data["object_ids"] = id_list;
					LLHTTPClient::post(
						url,
						post_data,
						new LLObjectCostResponder(id_list));
				}
			}
			else
			{
				mStaleObjectCost.clear();
				mPendingObjectCost.clear();
			}
		}
	}
}

void LLViewerObjectList::fetchPhysicsFlags()
{
	// issue http request for stale object physics flags
	if (!mStalePhysicsFlags.empty())
	{
		LLViewerRegion* regionp = gAgent.getRegion();

		if (regionp)
		{
			std::string url = regionp->getCapability("GetObjectPhysicsData");

			if (!url.empty())
			{
				LLSD id_list;
				U32 object_index = 0;

				for (
					auto iter = mStalePhysicsFlags.begin();
					iter != mStalePhysicsFlags.end();
					)
				{
					// Check to see if a request for this object
					// has already been made.
					if ( mPendingPhysicsFlags.find(*iter) ==
						 mPendingPhysicsFlags.end() )
					{
						mPendingPhysicsFlags.insert(*iter);
						id_list[object_index++] = *iter;
					}

					mStalePhysicsFlags.erase(iter++);
					
					if (object_index >= MAX_CONCURRENT_PHYSICS_REQUESTS)
					{
						break;
					}
				}

				if ( id_list.size() > 0 )
				{
					LLSD post_data = LLSD::emptyMap();

					post_data["object_ids"] = id_list;
					LLHTTPClient::post(
						url,
						post_data,
						new LLPhysicsFlagsResponder(id_list));
				}
			}
			else
			{
				mStalePhysicsFlags.clear();
				mPendingPhysicsFlags.clear();
			}
		}
	}
}

bool LLViewerObjectList::gotObjectPhysicsFlags(LLViewerObject* objectp)
{
	// This will insert objectp in mStalePhysicsFlags if needed:
	objectp->getPhysicsShapeType();
	// Data has been retrieved if the object is not in either of the
	// two lists:
	const LLUUID& id = objectp->getID();
	return mPendingPhysicsFlags.count(id) == 0 &&
		   mStalePhysicsFlags.count(id) == 0;
}

void LLViewerObjectList::clearDebugText()
{
	for (vobj_list_t::iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
	{
		LLViewerObject *objectp = *iter;
		if (objectp->isDead())
		{
			continue;
		}
		objectp->setDebugText("");
	}
}


void LLViewerObjectList::cleanupReferences(LLViewerObject *objectp)
{
	bool new_dead_object = true;
	if (mDeadObjects.find(objectp->mID) != mDeadObjects.end())
	{
		LL_INFOS() << "Object " << objectp->mID << " already on dead list!" << LL_ENDL;	
		new_dead_object = false;
	}
	else
	{
		mDeadObjects.insert(objectp->mID);
	}

	// Cleanup any references we have to this object
	// Remove from object map so noone can look it up.

	mUUIDObjectMap.erase(objectp->mID);
	// <singu> Use the return value (number of erased elements) to determine if we were an avatar.
	if (mUUIDAvatarMap.erase(objectp->mID)) //No need to be careful here.
		if (LLFloaterIMPanel* im = find_im_floater(objectp->mID))
			im->removeDynamicFocus();
	// </singu>
	
	//if (objectp->getRegion())
	//{
	//	LL_INFOS() << "cleanupReferences removing object from table, local ID " << objectp->mLocalID << ", ip " 
	//				<< objectp->getRegion()->getHost().getAddress() << ":" 
	//				<< objectp->getRegion()->getHost().getPort() << LL_ENDL;
	//}	
	
	removeFromLocalIDTable(objectp);

	if (objectp->onActiveList())
	{
		//LL_INFOS() << "Removing " << objectp->mID << " " << objectp->getPCodeString() << " from active list in cleanupReferences." << LL_ENDL;
		objectp->setOnActiveList(FALSE);
		removeFromActiveList(objectp);
	}

	if (objectp->isOnMap())
	{
		removeFromMap(objectp);
	}

	// Don't clean up mObject references, these will be cleaned up more efficiently later!
	// Also, not cleaned up
	removeDrawable(objectp->mDrawable);

	if(new_dead_object)
	{
		mNumDeadObjects++;
	}
}

static LLTrace::BlockTimerStatHandle FTM_REMOVE_DRAWABLE("Remove Drawable");

void LLViewerObjectList::removeDrawable(LLDrawable* drawablep)
{
	LL_RECORD_BLOCK_TIME(FTM_REMOVE_DRAWABLE);

	if (!drawablep)
	{
		return;
	}

	for (S32 i = 0; i < drawablep->getNumFaces(); i++)
	{
		LLFace* facep = drawablep->getFace(i) ;
		if(facep)
		{
			   LLViewerObject* objectp = facep->getViewerObject();
			   if(objectp)
			   {
					   mSelectPickList.erase(objectp);
			   }
		}
	}
}

BOOL LLViewerObjectList::killObject(LLViewerObject *objectp)
{
	// Don't ever kill gAgentAvatarp, just force it to the agent's region
	// unless region is NULL which is assumed to mean you are logging out.
	if ((objectp == gAgentAvatarp) && gAgent.getRegion())
	{
		objectp->setRegion(gAgent.getRegion());
		return FALSE;
	}	
	
	// When we're killing objects, all we do is mark them as dead.
	// We clean up the dead objects later.

	if (objectp)
	{
		// We are going to cleanup a lot of smart pointers to this object, they might be last,
		// and object being NULLed while inside it's own function won't be pretty
		// so create a pointer to make sure object will stay alive untill markDead() finishes
		LLPointer<LLViewerObject> sp(objectp);
		sp->markDead(); // does the right thing if object already dead
		return TRUE;
	}
	return FALSE;
}

void LLViewerObjectList::killObjects(LLViewerRegion *regionp)
{
	LLTimer kill_timer;
	LLViewerObject *objectp;

	S32 count = 0;
	for (vobj_list_t::iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
	{
		objectp = *iter;
		
		if (objectp->mRegionp == regionp)
		{
			++count;
			killObject(objectp);
		}
	}

	// Have to clean right away because the region is becoming invalid.
	cleanDeadObjects(FALSE);
	LL_INFOS() << "Removed " << count << " objects for region " << regionp->getName() << ". (" << kill_timer.getElapsedTimeF64()*1000.0 << "ms)" << LL_ENDL;
}

void LLViewerObjectList::killAllObjects()
{
	// Used only on global destruction.
	LLViewerObject *objectp;

	for (vobj_list_t::iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
	{
		objectp = *iter;
		
		killObject(objectp);
		// Object must be dead, or it's the LLVOAvatarSelf which never dies.
		llassert((objectp == gAgentAvatarp) || objectp->isDead() || (objectp->asAvatar() && objectp->asAvatar()->isFrozenDead()));
	}

	cleanDeadObjects(FALSE);

	if(!mObjects.empty())
	{
		LL_WARNS() << "LLViewerObjectList::killAllObjects still has entries in mObjects: " << mObjects.size() << LL_ENDL;
		mObjects.clear();
	}

	if (!mActiveObjects.empty())
	{
		LL_WARNS() << "Some objects still on active object list!" << LL_ENDL;
		mActiveObjects.clear();
	}

	if (!mMapObjects.empty())
	{
		LL_WARNS() << "Some objects still on map object list!" << LL_ENDL;
		mMapObjects.clear();
	}
}

void LLViewerObjectList::cleanDeadObjects(BOOL use_timer)
{
	if (use_timer && mNumDeadObjects < mMinNumDeadObjects)
	{
		// Not enough dead objects, don't scan the whole object list until there are a few.
		// However, the longer it takes to reach this quota, the less demanding we are,
		// so decrease the lower limit but never demand less than 20.
		mMinNumDeadObjects = llmax(20, mMinNumDeadObjects - 1);
		return;
	}
	// If we got this many now, we might as well demand it for the next calls too (they
	// tend to come in batches). However, never demand more than 100.
	mMinNumDeadObjects = llmin(100, mNumDeadObjects);

	// Scan for all of the dead objects and remove any "global" references to them.

	// We first move all dead objects to the end of the std::vector (the swap
	// is VERY cheap) and only then call erase once: calling erase on a vector
	// is very expensive!

	vobj_list_t::iterator iter = mObjects.begin();		// Runs over all objects.
	vobj_list_t::iterator const end = mObjects.end();
	vobj_list_t::iterator last = end;					// Points to the last object that is not dead.
	S32 num_removed = 0;

	// Find the first Dead object.
	while (iter != last && !(*iter)->isDead())
		++iter;

	// While iter did not bumb into last, continue the search.
	while (iter != last)
	{
		// Find the next not Dead object from the end.
		do {
			--last;
			++num_removed;
		} while (last != iter && (*last)->isDead());
		if (iter == last)
		{
			// There no more Dead objects left before last.
			break;
		}
		// Swap both object Pointers.
		vobj_list_t::value_type::swap(*iter, *last);
		if (num_removed == mNumDeadObjects)
		{
			// There aren't any more dead objects.
			break;
		}
		// Find the next Dead object at the beginning.
		do {
			++iter;
		} while (iter != last && !(*iter)->isDead());
	}
	llassert(end - last == num_removed);
	mObjects.erase(last, end);

	// We've cleaned the global object list, now let's do some paranoia testing on objects
	// before blowing away the dead list.
	mDeadObjects.clear();
	mNumDeadObjects = 0;
}

void LLViewerObjectList::removeFromActiveList(LLViewerObject* objectp)
{
	S32 idx = objectp->getListIndex();
	if (idx != -1)
	{ //remove by moving last element to this object's position
		llassert(mActiveObjects[idx] == objectp);
		
		objectp->setListIndex(-1);

		std::vector<LLPointer<LLViewerObject> >::iterator iter = vector_replace_with_last(mActiveObjects, mActiveObjects.begin() + idx);
		if(iter != mActiveObjects.end())
			(*iter)->setListIndex(idx);

	}
}

void LLViewerObjectList::updateActive(LLViewerObject *objectp)
{
	if (objectp->isDead())
	{
		return; // We don't update dead objects!
	}

	BOOL active = objectp->isActive();
	if (active != objectp->onActiveList())
	{
		if (active)
		{
			//LL_INFOS() << "Adding " << objectp->mID << " " << objectp->getPCodeString() << " to active list." << LL_ENDL;
			S32 idx = objectp->getListIndex();
			if (idx <= -1)
			{
				mActiveObjects.push_back(objectp);
				objectp->setListIndex(mActiveObjects.size()-1);
				objectp->setOnActiveList(TRUE);
			}
			else
			{
				llassert(idx < (S32)mActiveObjects.size());
				llassert(mActiveObjects[idx] == objectp);

				if (idx >= (S32)mActiveObjects.size() ||
					mActiveObjects[idx] != objectp)
				{
					LL_WARNS() << "Invalid object list index detected!" << LL_ENDL;
				}
			}
		}
		else
		{
			//LL_INFOS() << "Removing " << objectp->mID << " " << objectp->getPCodeString() << " from active list." << LL_ENDL;
			removeFromActiveList(objectp);
			objectp->setOnActiveList(FALSE);
		}
	}

	llassert(objectp->isActive() || objectp->getListIndex() == -1);

}

void LLViewerObjectList::updateObjectCost(LLViewerObject* object)
{
	if (!object->isRoot())
	{ //always fetch cost for the parent when fetching cost for children
		mStaleObjectCost.insert(((LLViewerObject*)object->getParent())->getID());
	}
	mStaleObjectCost.insert(object->getID());
}

void LLViewerObjectList::updateObjectCost(const LLUUID& object_id, F32 object_cost, F32 link_cost, F32 physics_cost, F32 link_physics_cost)
{
	mPendingObjectCost.erase(object_id);

	LLViewerObject* object = findObject(object_id);
	if (object)
	{
		object->setObjectCost(object_cost);
		object->setLinksetCost(link_cost);
		object->setPhysicsCost(physics_cost);
		object->setLinksetPhysicsCost(link_physics_cost);
	}
}

void LLViewerObjectList::onObjectCostFetchFailure(const LLUUID& object_id)
{
	//LL_WARNS() << "Failed to fetch object cost for object: " << object_id << LL_ENDL;
	mPendingObjectCost.erase(object_id);
}

void LLViewerObjectList::updatePhysicsFlags(const LLViewerObject* object)
{
	mStalePhysicsFlags.insert(object->getID());
}

void LLViewerObjectList::updatePhysicsShapeType(const LLUUID& object_id, S32 type)
{
	mPendingPhysicsFlags.erase(object_id);
	LLViewerObject* object = findObject(object_id);
	if (object)
	{
		object->setPhysicsShapeType(type);
	}
}

void LLViewerObjectList::updatePhysicsProperties(const LLUUID& object_id, 
												F32 density,
												F32 friction,
												F32 restitution,
												F32 gravity_multiplier)
{
	mPendingPhysicsFlags.erase(object_id);

	LLViewerObject* object = findObject(object_id);
	if (object)
	{
		object->setPhysicsDensity(density);
		object->setPhysicsFriction(friction);
		object->setPhysicsGravity(gravity_multiplier);
		object->setPhysicsRestitution(restitution);
	}
}

void LLViewerObjectList::onPhysicsFlagsFetchFailure(const LLUUID& object_id)
{
	//LL_WARNS() << "Failed to fetch physics flags for object: " << object_id << LL_ENDL;
	mPendingPhysicsFlags.erase(object_id);
}

static LLTrace::BlockTimerStatHandle FTM_SHIFT_OBJECTS("Shift Objects");
static LLTrace::BlockTimerStatHandle FTM_PIPELINE_SHIFT("Pipeline Shift");
static LLTrace::BlockTimerStatHandle FTM_REGION_SHIFT("Region Shift");

void LLViewerObjectList::shiftObjects(const LLVector3 &offset)
{
	// This is called when we shift our origin when we cross region boundaries...
	// We need to update many object caches, I'll document this more as I dig through the code
	// cleaning things out...

	if (gNoRender || 0 == offset.magVecSquared())
	{
		return;
	}

	LL_RECORD_BLOCK_TIME(FTM_SHIFT_OBJECTS);

	LLViewerObject *objectp;
	for (vobj_list_t::iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
	{
		objectp = *iter;
		// There could be dead objects on the object list, so don't update stuff if the object is dead.
		if (objectp && !objectp->isDead())
		{
			objectp->updatePositionCaches();

			if (objectp->mDrawable.notNull() && !objectp->mDrawable->isDead())
			{
				gPipeline.markShift(objectp->mDrawable);
			}
		}
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_PIPELINE_SHIFT);
		gPipeline.shiftObjects(offset);
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_REGION_SHIFT);
		LLWorld::getInstance()->shiftRegions(offset);
	}
}

void LLViewerObjectList::repartitionObjects()
{
	for (vobj_list_t::iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
	{
		LLViewerObject* objectp = *iter;
		if (!objectp->isDead())
		{
			LLDrawable* drawable = objectp->mDrawable;
			if (drawable && !drawable->isDead())
			{
				drawable->updateBinRadius();
				drawable->updateSpatialExtents();
				drawable->movePartition();
			}
		}
	}
}

//debug code
bool LLViewerObjectList::hasMapObjectInRegion(LLViewerRegion* regionp) 
{
	for (vobj_list_t::iterator iter = mMapObjects.begin(); iter != mMapObjects.end(); ++iter)
	{
		LLViewerObject* objectp = *iter;

		if(objectp->isDead() || objectp->getRegion() == regionp)
		{
			return true ;
		}
	}

	return false ;
}

//make sure the region is cleaned up.
void LLViewerObjectList::clearAllMapObjectsInRegion(LLViewerRegion* regionp) 
{
	std::set<LLViewerObject*> dead_object_list ;
	std::set<LLViewerObject*> region_object_list ;
	for (vobj_list_t::iterator iter = mMapObjects.begin(); iter != mMapObjects.end(); ++iter)
	{
		LLViewerObject* objectp = *iter;

		if(objectp->isDead())
		{
			dead_object_list.insert(objectp) ;			
		}
		else if(objectp->getRegion() == regionp)
		{
			region_object_list.insert(objectp) ;
		}
	}

	if(dead_object_list.size() > 0)
	{
		LL_WARNS() << "There are " << dead_object_list.size() << " dead objects on the map!" << LL_ENDL ;

		for(std::set<LLViewerObject*>::iterator iter = dead_object_list.begin(); iter != dead_object_list.end(); ++iter)
		{
			cleanupReferences(*iter) ;
		}
	}
	if(region_object_list.size() > 0)
	{
		LL_WARNS() << "There are " << region_object_list.size() << " objects not removed from the deleted region!" << LL_ENDL ;

		for(std::set<LLViewerObject*>::iterator iter = region_object_list.begin(); iter != region_object_list.end(); ++iter)
		{
			(*iter)->markDead() ;
		}
	}
}
void LLViewerObjectList::renderObjectsForMap(LLNetMap &netmap)
{
	LLColor4 above_water_color = gColors.getColor( "NetMapOtherOwnAboveWater" );
	LLColor4 below_water_color = gColors.getColor( "NetMapOtherOwnBelowWater" );
	LLColor4 you_own_above_water_color = 
						gColors.getColor( "NetMapYouOwnAboveWater" );
	LLColor4 you_own_below_water_color = 
						gColors.getColor( "NetMapYouOwnBelowWater" );
	LLColor4 group_own_above_water_color = 
						gColors.getColor( "NetMapGroupOwnAboveWater" );
	LLColor4 group_own_below_water_color = 
						gColors.getColor( "NetMapGroupOwnBelowWater" );

	F32 max_radius = gSavedSettings.getF32("MiniMapPrimMaxRadius");
	
	const F32 agent_altitude(gAgent.getPositionGlobal()[VZ]);
	static const LLCachedControl<U32> delta("MiniMapPrimMaxAltitudeDelta");

	for (vobj_list_t::iterator iter = mMapObjects.begin(); iter != mMapObjects.end(); ++iter)
	{
		LLViewerObject* objectp = *iter;

		llassert_always(objectp);
		llassert(!objectp->isDead());
		
		if (objectp->isDead() || !objectp->getRegion() || objectp->isOrphaned() || objectp->isAttachment())
		{
			continue;
		}
		const LLVector3& scale = objectp->getScale();
		const LLVector3d pos = objectp->getPositionGlobal();
		const F64 water_height = F64( objectp->getRegion()->getWaterHeight() );
		// LLWorld::getInstance()->getWaterHeight();
		
		F32 approx_radius = (scale.mV[VX] + scale.mV[VY]) * 0.5f * 0.5f * 1.3f;  // 1.3 is a fudge

		// Limit the size of megaprims so they don't blot out everything on the minimap.
		// Attempting to draw very large megaprims also causes client lag.
		// See DEV-17370 and SNOW-79 for details.
		approx_radius = llmin(approx_radius, max_radius);

		LLColor4U color = above_water_color;
		if( objectp->permYouOwner() )
		{
			static const LLCachedControl<U32> delta("MiniMapPrimMaxAltitudeDeltaOwn");
			if (delta && static_cast<U32>(std::fabs(agent_altitude - pos[VZ])) > delta)
				continue;

			const F32 MIN_RADIUS_FOR_OWNED_OBJECTS = 2.f;
			if( approx_radius < MIN_RADIUS_FOR_OWNED_OBJECTS )
			{
				approx_radius = MIN_RADIUS_FOR_OWNED_OBJECTS;
			}
			
			if( pos.mdV[VZ] >= water_height )
			{
				if ( objectp->permGroupOwner() )
				{
					color = group_own_above_water_color;
				}
				else
				{
					color = you_own_above_water_color;
				}
			}
			else
			{
				if ( objectp->permGroupOwner() )
				{
					color = group_own_below_water_color;
				}
				else
				{
					color = you_own_below_water_color;
				}
			}
		}
		else if (delta && static_cast<U32>(std::fabs(agent_altitude - pos[VZ])) > delta)
		{
			continue;
		}
		else if ( pos.mdV[VZ] < water_height )
		{
			color = below_water_color;
		}

		netmap.renderScaledPointGlobal( 
			pos, 
			color,
			approx_radius );
	}
}

void LLViewerObjectList::renderObjectBounds(const LLVector3 &center)
{
}

void LLViewerObjectList::generatePickList(LLCamera &camera)
{
		LLViewerObject *objectp;
		S32 i;
		// Reset all of the GL names to zero.
		for (vobj_list_t::iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
		{
			(*iter)->mGLName = 0;
		}

		mSelectPickList.clear();

		std::vector<LLDrawable*> pick_drawables;

		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				if (part)
				{	
					part->cull(camera, &pick_drawables);
				}
			}
		}

		for (std::vector<LLDrawable*>::iterator iter = pick_drawables.begin();
			iter != pick_drawables.end(); iter++)
		{
			LLDrawable* drawablep = *iter;
			if( !drawablep )
				continue;

			LLViewerObject* last_objectp = NULL;
			for (S32 face_num = 0; face_num < drawablep->getNumFaces(); face_num++)
			{
				LLFace * facep = drawablep->getFace(face_num);
				if (!facep) continue;

				LLViewerObject* objectp = facep->getViewerObject();

				if (objectp && objectp != last_objectp)
				{
					mSelectPickList.insert(objectp);
					last_objectp = objectp;
				}
			}
		}

		LLHUDNameTag::addPickable(mSelectPickList);

		for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
			iter != LLCharacter::sInstances.end(); ++iter)
		{
			objectp = (LLVOAvatar*) *iter;
			if (!objectp->isDead())
			{
				if (objectp->mDrawable.notNull() && objectp->mDrawable->isVisible())
				{
					mSelectPickList.insert(objectp);
				}
			}
		}

		// add all hud objects to pick list
		if (isAgentAvatarValid())
		{
			for (LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin(); 
				 iter != gAgentAvatarp->mAttachmentPoints.end(); )
			{
				LLVOAvatar::attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachment = curiter->second;
				if (attachment->getIsHUDAttachment())
				{
					for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
						 attachment_iter != attachment->mAttachedObjects.end();
						 ++attachment_iter)
					{
						if (LLViewerObject* attached_object = (*attachment_iter))
						{
							mSelectPickList.insert(attached_object);
							LLViewerObject::const_child_list_t& child_list = attached_object->getChildren();
							for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
								 iter != child_list.end(); iter++)
							{
								LLViewerObject* childp = *iter;
								if (childp)
								{
									mSelectPickList.insert(childp);
								}
							}
						}
					}
				}
			}
		}
		
		S32 num_pickables = (S32)mSelectPickList.size() + LLHUDIcon::getNumInstances();

		if (num_pickables != 0)
		{
			S32 step = (0x000fffff - GL_NAME_INDEX_OFFSET) / num_pickables;

			std::set<LLViewerObject*>::iterator pick_it;
			i = 0;
			for (pick_it = mSelectPickList.begin(); pick_it != mSelectPickList.end();)
			{
				LLViewerObject* objp = (*pick_it);
				if (!objp || objp->isDead() || !objp->mbCanSelect)
				{
					mSelectPickList.erase(pick_it++);
					continue;
				}
				
				objp->mGLName = (i * step) + GL_NAME_INDEX_OFFSET;
				i++;
				++pick_it;
			}

			LLHUDIcon::generatePickIDs(i * step, step);
	}
}

LLViewerObject *LLViewerObjectList::getSelectedObject(const U32 object_id)
{
	std::set<LLViewerObject*>::iterator pick_it;
	for (pick_it = mSelectPickList.begin(); pick_it != mSelectPickList.end(); ++pick_it)
	{
		if ((*pick_it)->mGLName == object_id)
		{
			return (*pick_it);
		}
	}
	return NULL;
}

void LLViewerObjectList::addDebugBeacon(const LLVector3 &pos_agent,
										const std::string &string,
										const LLColor4 &color,
										const LLColor4 &text_color,
										S32 line_width)
{
	LLDebugBeacon beacon;
	beacon.mPositionAgent = pos_agent;
	beacon.mString = string;
	beacon.mColor = color;
	beacon.mTextColor = text_color;
	beacon.mLineWidth = line_width;

	mDebugBeacons.push_back(beacon);
}

void LLViewerObjectList::resetObjectBeacons()
{
	mDebugBeacons.clear();
}

LLViewerObject *LLViewerObjectList::createObjectViewer(const LLPCode pcode, LLViewerRegion *regionp, S32 flags)
{
	LLUUID fullid;
	fullid.generate();

	LLViewerObject *objectp = LLViewerObject::createObject(fullid, pcode, regionp, flags);
	if (!objectp)
	{
// 		LL_WARNS() << "Couldn't create object of type " << LLPrimitive::pCodeToString(pcode) << LL_ENDL;
		return NULL;
	}

	mUUIDObjectMap.insert_or_assign(fullid, objectp);
	if(objectp->isAvatar())
	{
		LLVOAvatar *pAvatar = dynamic_cast<LLVOAvatar*>(objectp);
		if(pAvatar)
		{
			mUUIDAvatarMap.insert_or_assign(fullid, pAvatar);
			// <singu>
			if (LLFloaterIMPanel* im = find_im_floater(fullid))
				im->addDynamicFocus();
			// </singu>
		}
	}

	mObjects.push_back(objectp);

	updateActive(objectp);

	return objectp;
}


static LLTrace::BlockTimerStatHandle FTM_CREATE_OBJECT("Create Object");

LLViewerObject *LLViewerObjectList::createObject(const LLPCode pcode, LLViewerRegion *regionp,
												 const LLUUID &uuid, const U32 local_id, const LLHost &sender)
{
	LL_RECORD_BLOCK_TIME(FTM_CREATE_OBJECT);
	
	LLUUID fullid;
	if (uuid == LLUUID::null)
	{
		fullid.generate();
	}
	else
	{
		fullid = uuid;
	}

	LLViewerObject *objectp = LLViewerObject::createObject(fullid, pcode, regionp);
	if (!objectp)
	{
// 		LL_WARNS() << "Couldn't create object of type " << LLPrimitive::pCodeToString(pcode) << " id:" << fullid << LL_ENDL;
		return NULL;
	}
	if(regionp)
	{
		regionp->addToCreatedList(local_id); 
	}

	mUUIDObjectMap.insert_or_assign(fullid, objectp);
	if(objectp->isAvatar())
	{
		LLVOAvatar *pAvatar = dynamic_cast<LLVOAvatar*>(objectp);
		if(pAvatar)
		{
			mUUIDAvatarMap.insert_or_assign(fullid, pAvatar);
			// <singu>
			if (LLFloaterIMPanel* im = find_im_floater(fullid))
				im->addDynamicFocus();
			// </singu>
		}
	}
	setUUIDAndLocal(fullid,
					local_id,
					gMessageSystem->getSenderIP(),
					gMessageSystem->getSenderPort());

	mObjects.push_back(objectp);

	updateActive(objectp);

	return objectp;
}

LLViewerObject *LLViewerObjectList::replaceObject(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
{
	LLViewerObject *old_instance = findObject(id);
	if (old_instance)
	{
		//cleanupReferences(old_instance);
		old_instance->markDead();
		
		return createObject(pcode, regionp, id, old_instance->getLocalID(), LLHost());
	}
	return NULL;
}

S32 LLViewerObjectList::findReferences(LLDrawable *drawablep) const
{
	LLViewerObject *objectp;
	S32 num_refs = 0;
	
	for (vobj_list_t::const_iterator iter = mObjects.begin(); iter != mObjects.end(); ++iter)
	{
		objectp = *iter;
		if (objectp->mDrawable.notNull())
		{
			num_refs += objectp->mDrawable->findReferences(drawablep);
		}
	}
	return num_refs;
}


void LLViewerObjectList::orphanize(LLViewerObject *childp, U32 parent_id, U32 ip, U32 port)
{
#ifdef ORPHAN_SPAM
	LL_INFOS() << "Orphaning object " << childp->getID() << " with parent " << parent_id << LL_ENDL;
#endif

	// We're an orphan, flag things appropriately.
	childp->mOrphaned = TRUE;
	if (childp->mDrawable.notNull())
	{
		bool make_invisible = true;
		LLViewerObject *parentp = (LLViewerObject *)childp->getParent();
		if (parentp)
		{
			if (parentp->getRegion() != childp->getRegion())
			{
				// This is probably an object flying across a region boundary, the
				// object probably ISN'T being reparented, but just got an object
				// update out of order (child update before parent).
				make_invisible = false;
				//LL_INFOS() << "Don't make object handoffs invisible!" << LL_ENDL;
			}
		}

		if (make_invisible)
		{
			// Make sure that this object becomes invisible if it's an orphan
			childp->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
		}
	}

	// Unknown parent, add to orpaned child list
	U64 parent_info = getIndex(parent_id, ip, port);

	if (std::find(mOrphanParents.begin(), mOrphanParents.end(), parent_info) == mOrphanParents.end())
	{
		mOrphanParents.push_back(parent_info);
	}

	LLViewerObjectList::OrphanInfo oi(parent_info, childp->mID);
	if (std::find(mOrphanChildren.begin(), mOrphanChildren.end(), oi) == mOrphanChildren.end())
	{
		mOrphanChildren.push_back(oi);
		mNumOrphans++;
	}
}


void LLViewerObjectList::findOrphans(LLViewerObject* objectp, U32 ip, U32 port)
{
	if (gNoRender)
	{
		return;
	}

	if (objectp->isDead())
	{
		LL_WARNS() << "Trying to find orphans for dead obj " << objectp->mID 
			<< ":" << objectp->getPCodeString() << LL_ENDL;
		return;
	}

	// See if we are a parent of an orphan.
	// Note:  This code is fairly inefficient but it should happen very rarely.
	// It can be sped up if this is somehow a performance issue...
	if (mOrphanParents.empty())
	{
		// no known orphan parents
		return;
	}
	if (std::find(mOrphanParents.begin(), mOrphanParents.end(), getIndex(objectp->mLocalID, ip, port)) == mOrphanParents.end())
	{
		// did not find objectp in OrphanParent list
		return;
	}

	U64 parent_info = getIndex(objectp->mLocalID, ip, port);
	BOOL orphans_found = FALSE;
	// Iterate through the orphan list, and set parents of matching children.

	for (std::vector<OrphanInfo>::iterator iter = mOrphanChildren.begin(); iter != mOrphanChildren.end(); )
	{	
		if (iter->mParentInfo != parent_info)
		{
			++iter;
			continue;
		}
		LLViewerObject *childp = findObject(iter->mChildInfo);
		if (childp)
		{
			if (childp == objectp)
			{
				LL_WARNS() << objectp->mID << " has self as parent, skipping!" 
					<< LL_ENDL;
				continue;
			}

#ifdef ORPHAN_SPAM
			LL_INFOS() << "Reunited parent " << objectp->mID 
				<< " with child " << childp->mID << LL_ENDL;
			LL_INFOS() << "Glob: " << objectp->getPositionGlobal() << LL_ENDL;
			LL_INFOS() << "Agent: " << objectp->getPositionAgent() << LL_ENDL;
			addDebugBeacon(objectp->getPositionAgent(),"");
#endif
			gPipeline.markMoved(objectp->mDrawable);                
			objectp->setChanged(LLXform::MOVED | LLXform::SILHOUETTE);

			// Flag the object as no longer orphaned
			childp->mOrphaned = FALSE;
			if (childp->mDrawable.notNull())
			{
				// Make the drawable visible again and set the drawable parent
				childp->mDrawable->clearState(LLDrawable::FORCE_INVISIBLE);
				childp->setDrawableParent(objectp->mDrawable); // LLViewerObjectList::findOrphans()
				gPipeline.markRebuild( childp->mDrawable, LLDrawable::REBUILD_ALL, TRUE );
			}

			// Make certain particles, icon and HUD aren't hidden
			childp->hideExtraDisplayItems( FALSE );

			objectp->addChild(childp);
			orphans_found = TRUE;
			++iter;
		}
		else
		{
			LL_INFOS() << "Missing orphan child, removing from list" << LL_ENDL;

			iter = mOrphanChildren.erase(iter);
		}
	}

	// Remove orphan parent and children from lists now that they've been found
	{
		std::vector<U64>::iterator iter = std::find(mOrphanParents.begin(), mOrphanParents.end(), parent_info);
		if (iter != mOrphanParents.end())
		{
			mOrphanParents.erase(iter);
		}
	}
	
	for (std::vector<OrphanInfo>::iterator iter = mOrphanChildren.begin(); iter != mOrphanChildren.end(); )
	{
		if (iter->mParentInfo == parent_info)
		{
			iter = mOrphanChildren.erase(iter);
			mNumOrphans--;
		}
		else
		{
			++iter;
		}
	}

	if (orphans_found && objectp->isSelected())
	{
		LLSelectNode* nodep = LLSelectMgr::getInstance()->getSelection()->findNode(objectp);
		if (nodep && !nodep->mIndividualSelection)
		{
			// rebuild selection with orphans
			LLSelectMgr::getInstance()->deselectObjectAndFamily(objectp);
			LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
		}
	}
}

////////////////////////////////////////////////////////////////////////////

LLViewerObjectList::OrphanInfo::OrphanInfo()
	: mParentInfo(0)
{
}

LLViewerObjectList::OrphanInfo::OrphanInfo(const U64 parent_info, const LLUUID child_info)
	: mParentInfo(parent_info), mChildInfo(child_info)
{
}

bool LLViewerObjectList::OrphanInfo::operator==(const OrphanInfo &rhs) const
{
	return (mParentInfo == rhs.mParentInfo) && (mChildInfo == rhs.mChildInfo);
}

bool LLViewerObjectList::OrphanInfo::operator!=(const OrphanInfo &rhs) const
{
	return !operator==(rhs);
}


