/** 
 * @file llinventorymodel.cpp
 * @brief Implementation of the inventory model used to track agent inventory.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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
#include "llinventorymodelbackgroundfetch.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llcallbacklist.h"
#include "llinventorypanel.h"
#include "llinventorymodel.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermessage.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "hippogridmanager.h"

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy BGItemHttpHandler_timeout;
extern AIHTTPTimeoutPolicy BGFolderHttpHandler_timeout;

const F32 MAX_TIME_FOR_SINGLE_FETCH = 10.f;
const S32 MAX_FETCH_RETRIES = 10;


///----------------------------------------------------------------------------
/// Class <anonymous>::BGItemHttpHandler
///----------------------------------------------------------------------------

//
// Http request handler class for single inventory item requests.
//
// We'll use a handler-per-request pattern here rather than
// a shared handler.  Mainly convenient as this was converted
// from a Responder class model.
//
// Derives from and is identical to the normal FetchItemHttpHandler
// except that:  1) it uses the background request object which is
// updated more slowly than the foreground and 2) keeps a count of
// active requests on the LLInventoryModelBackgroundFetch object
// to indicate outstanding operations are in-flight.
//
class BGItemHttpHandler : public LLInventoryModel::FetchItemHttpHandler
{
	LOG_CLASS(BGItemHttpHandler);
	
public:
	BGItemHttpHandler(const LLSD & request_sd)
		: LLInventoryModel::FetchItemHttpHandler(request_sd)
		{
			LLInventoryModelBackgroundFetch::instance().incrFetchCount(1);
		}

	virtual ~BGItemHttpHandler()
		{
			LLInventoryModelBackgroundFetch::instance().incrFetchCount(-1);
		}
	
	/*virtual*/ AICapabilityType capability_type(void) const { return cap_inventory; }
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return BGItemHttpHandler_timeout; }
	/*virtual*/ char const* getName(void) const { return "BGItemHttpHandler"; }
protected:
	BGItemHttpHandler(const BGItemHttpHandler &);				// Not defined
	void operator=(const BGItemHttpHandler &);					// Not defined
};


///----------------------------------------------------------------------------
/// Class <anonymous>::BGFolderHttpHandler
///----------------------------------------------------------------------------

// Http request handler class for folders.
//
// Handler for FetchInventoryDescendents2 and FetchLibDescendents2
// caps requests for folders.
//
class BGFolderHttpHandler : public LLHTTPClient::ResponderWithResult
{
	LOG_CLASS(BGFolderHttpHandler);
	
public:
	BGFolderHttpHandler(const LLSD & request_sd, const uuid_vec_t & recursive_cats)
		: LLHTTPClient::ResponderWithResult(),
		  mRequestSD(request_sd),
		  mRecursiveCatUUIDs(recursive_cats)
		{
			LLInventoryModelBackgroundFetch::instance().incrFetchCount(1);
		}

	virtual ~BGFolderHttpHandler()
		{
			LLInventoryModelBackgroundFetch::instance().incrFetchCount(-1);
		}

	/*virtual*/ AICapabilityType capability_type(void) const { return cap_inventory; }
	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return BGFolderHttpHandler_timeout; }
	/*virtual*/ char const* getName(void) const { return "BGFolderHttpHandler"; }

protected:
	BGFolderHttpHandler(const BGFolderHttpHandler &);			// Not defined
	void operator=(const BGFolderHttpHandler &);				// Not defined
	BOOL getIsRecursive(const LLUUID& cat_id) const;
private:
	/*virtual*/ void httpSuccess(void);
	/*virtual*/ void httpFailure(void);
	LLSD mRequestSD;
	uuid_vec_t mRecursiveCatUUIDs; // hack for storing away which cat fetches are recursive
};


const char * const LOG_INV("Inventory");


LLInventoryModelBackgroundFetch::LLInventoryModelBackgroundFetch() :
	mBackgroundFetchActive(FALSE),
	mFolderFetchActive(false),
	mFetchCount(0),
	mAllFoldersFetched(FALSE),
	mRecursiveInventoryFetchStarted(FALSE),
	mRecursiveLibraryFetchStarted(FALSE),
	mNumFetchRetries(0),
	mMinTimeBetweenFetches(0.3f),
	mMaxTimeBetweenFetches(10.f),
	mTimelyFetchPending(FALSE)
{
}

LLInventoryModelBackgroundFetch::~LLInventoryModelBackgroundFetch()
{
}

bool LLInventoryModelBackgroundFetch::isBulkFetchProcessingComplete() const
{
	return mFetchQueue.empty() && mFetchCount<=0;
}

bool LLInventoryModelBackgroundFetch::libraryFetchStarted() const
{
	return mRecursiveLibraryFetchStarted;
}

bool LLInventoryModelBackgroundFetch::libraryFetchCompleted() const
{
	return libraryFetchStarted() && fetchQueueContainsNoDescendentsOf(gInventory.getLibraryRootFolderID());
}

bool LLInventoryModelBackgroundFetch::libraryFetchInProgress() const
{
	return libraryFetchStarted() && !libraryFetchCompleted();
}
	
bool LLInventoryModelBackgroundFetch::inventoryFetchStarted() const
{
	return mRecursiveInventoryFetchStarted;
}

bool LLInventoryModelBackgroundFetch::inventoryFetchCompleted() const
{
	return inventoryFetchStarted() && fetchQueueContainsNoDescendentsOf(gInventory.getRootFolderID());
}

bool LLInventoryModelBackgroundFetch::inventoryFetchInProgress() const
{
	return inventoryFetchStarted() && !inventoryFetchCompleted();
}

bool LLInventoryModelBackgroundFetch::isEverythingFetched() const
{
	return mAllFoldersFetched;
}

BOOL LLInventoryModelBackgroundFetch::folderFetchActive() const
{
	return mFolderFetchActive;
}

void LLInventoryModelBackgroundFetch::addRequestAtFront(const LLUUID & id, BOOL recursive, bool is_category)
{
	mFetchQueue.push_front(FetchQueueInfo(id, recursive, is_category));
}

void LLInventoryModelBackgroundFetch::addRequestAtBack(const LLUUID & id, BOOL recursive, bool is_category)
{
	mFetchQueue.push_back(FetchQueueInfo(id, recursive, is_category));
}

void LLInventoryModelBackgroundFetch::start(const LLUUID& id, BOOL recursive)
{
	LLViewerInventoryCategory * cat(gInventory.getCategory(id));

	if (cat || (id.isNull() && ! isEverythingFetched()))
	{
		// it's a folder, do a bulk fetch
		LL_DEBUGS(LOG_INV) << "Start fetching category: " << id << ", recursive: " << recursive << LL_ENDL;

		mFolderFetchActive = true;
		if (id.isNull())
		{
			if (!mRecursiveInventoryFetchStarted)
			{
				mRecursiveInventoryFetchStarted |= recursive;
				mFetchQueue.push_back(FetchQueueInfo(gInventory.getRootFolderID(), recursive));
				gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
				mBackgroundFetchActive = TRUE;
			}
			if (!mRecursiveLibraryFetchStarted)
			{
				mRecursiveLibraryFetchStarted |= recursive;
				mFetchQueue.push_back(FetchQueueInfo(gInventory.getLibraryRootFolderID(), recursive));
				gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
				mBackgroundFetchActive = TRUE;
			}
		}
		else
		{
			// Specific folder requests go to front of queue.
			if (mFetchQueue.empty() || mFetchQueue.front().mUUID != id)
			{
				mFetchQueue.push_front(FetchQueueInfo(id, recursive));
				gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
				mBackgroundFetchActive = TRUE;
			}
			if (id == gInventory.getLibraryRootFolderID())
			{
				mRecursiveLibraryFetchStarted |= recursive;
			}
			if (id == gInventory.getRootFolderID())
			{
				mRecursiveInventoryFetchStarted |= recursive;
			}
		}
	}
	else if (LLViewerInventoryItem* itemp = gInventory.getItem(id))
	{
		if (!itemp->mIsComplete && (mFetchQueue.empty() || mFetchQueue.front().mUUID != id))
		{
			mBackgroundFetchActive = TRUE;

			mFetchQueue.push_front(FetchQueueInfo(id, false, false));
			gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
		}
	}
}

void LLInventoryModelBackgroundFetch::findLostItems()
{
	mBackgroundFetchActive = TRUE;
	mFolderFetchActive = true;
    mFetchQueue.push_back(FetchQueueInfo(LLUUID::null, TRUE));
    gIdleCallbacks.addFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
}

void LLInventoryModelBackgroundFetch::setAllFoldersFetched()
{
	if (mRecursiveInventoryFetchStarted &&
		mRecursiveLibraryFetchStarted)
	{
		mAllFoldersFetched = TRUE;
	}
	mFolderFetchActive = false;

	if (mBackgroundFetchActive)
	{
	  gIdleCallbacks.deleteFunction(&LLInventoryModelBackgroundFetch::backgroundFetchCB, NULL);
	}
	mBackgroundFetchActive = false;
	LL_INFOS(LOG_INV) << "Inventory background fetch completed" << LL_ENDL;
}

void LLInventoryModelBackgroundFetch::backgroundFetchCB(void *)
{
	LLInventoryModelBackgroundFetch::instance().backgroundFetch();
}

bool use_http_inventory()
{
	return gHippoGridManager->getConnectedGrid()->isSecondLife() || gSavedSettings.getBOOL("UseHTTPInventory");
}

void LLInventoryModelBackgroundFetch::backgroundFetch()
{
	LLViewerRegion* region = gAgent.getRegion();
	if (mBackgroundFetchActive && region && region->capabilitiesReceived())
	{
		if (use_http_inventory())
		{
			// If we'll be using the capability, we'll be sending batches and the background thing isn't as important.
			std::string url = region->getCapability("FetchInventory2");
			if (!url.empty())
			{
				bool mPerServicePtr_initialized = !!mPerServicePtr;
				if (!mPerServicePtr_initialized)
				{
					// One time initialization needed for bulkFetch().
					std::string servicename = AIPerService::extract_canonical_servicename(url);
					if (!servicename.empty())
					{
						LL_INFOS() << "Initialized service name for bulk inventory fetching with \"" << servicename << "\"." << LL_ENDL;
						mPerServicePtr = AIPerService::instance(servicename);
						mPerServicePtr_initialized = true;
					}
				}
				if (mPerServicePtr_initialized)
				{
					bulkFetch();
					return;
				}
			}
	  }

#if 1
		//--------------------------------------------------------------------------------
		// DEPRECATED OLD CODE
		//

		// No more categories to fetch, stop fetch process.
		if (mFetchQueue.empty())
		{
			LL_INFOS() << "Inventory fetch completed" << LL_ENDL;
			setAllFoldersFetched();

			return;
		}

		F32 fast_fetch_time = lerp(mMinTimeBetweenFetches, mMaxTimeBetweenFetches, 0.1f);
		F32 slow_fetch_time = lerp(mMinTimeBetweenFetches, mMaxTimeBetweenFetches, 0.5f);
		if (mTimelyFetchPending && mFetchTimer.getElapsedTimeF32() > slow_fetch_time)
		{
			// double timeouts on failure
			mMinTimeBetweenFetches = llmin(mMinTimeBetweenFetches * 2.f, 10.f);
			mMaxTimeBetweenFetches = llmin(mMaxTimeBetweenFetches * 2.f, 120.f);
			LL_DEBUGS() << "Inventory fetch times grown to (" << mMinTimeBetweenFetches << ", " << mMaxTimeBetweenFetches << ")" << LL_ENDL;
			// fetch is no longer considered "timely" although we will wait for full time-out.
			mTimelyFetchPending = FALSE;
		}

		while(1)
		{
			if (mFetchQueue.empty())
			{
				break;
			}

			if(gDisconnected)
			{
				// just bail if we are disconnected.
				break;
			}

			const FetchQueueInfo info = mFetchQueue.front();

			if (info.mIsCategory)
			{

				LLViewerInventoryCategory* cat = gInventory.getCategory(info.mUUID);

				// Category has been deleted, remove from queue.
				if (!cat)
				{
					mFetchQueue.pop_front();
					continue;
				}
			
				if (mFetchTimer.getElapsedTimeF32() > mMinTimeBetweenFetches && 
					LLViewerInventoryCategory::VERSION_UNKNOWN == cat->getVersion())
				{
					// Category exists but has no children yet, fetch the descendants
					// for now, just request every time and rely on retry timer to throttle.
					if (cat->fetch())
					{
						mFetchTimer.reset();
						mTimelyFetchPending = TRUE;
					}
					else
					{
						//  The catagory also tracks if it has expired and here it says it hasn't
						//  yet.  Get out of here because nothing is going to happen until we
						//  update the timers.
						break;
					}
				}
				// Do I have all my children?
				else if (gInventory.isCategoryComplete(info.mUUID))
				{
					// Finished with this category, remove from queue.
					mFetchQueue.pop_front();

					// Add all children to queue.
					LLInventoryModel::cat_array_t* categories;
					gInventory.getDirectDescendentsOf(cat->getUUID(), categories);
					for (LLInventoryModel::cat_array_t::const_iterator it = categories->begin();
						 it != categories->end();
						 ++it)
					{
						mFetchQueue.push_back(FetchQueueInfo((*it)->getUUID(),info.mRecursive));
					}

					// We received a response in less than the fast time.
					if (mTimelyFetchPending && mFetchTimer.getElapsedTimeF32() < fast_fetch_time)
					{
						// Shrink timeouts based on success.
						mMinTimeBetweenFetches = llmax(mMinTimeBetweenFetches * 0.8f, 0.3f);
						mMaxTimeBetweenFetches = llmax(mMaxTimeBetweenFetches * 0.8f, 10.f);
						LL_DEBUGS() << "Inventory fetch times shrunk to (" << mMinTimeBetweenFetches << ", " << mMaxTimeBetweenFetches << ")" << LL_ENDL;
					}

					mTimelyFetchPending = FALSE;
					continue;
				}
				else if (mFetchTimer.getElapsedTimeF32() > mMaxTimeBetweenFetches)
				{
					// Received first packet, but our num descendants does not match db's num descendants
					// so try again later.
					mFetchQueue.pop_front();

					if (mNumFetchRetries++ < MAX_FETCH_RETRIES)
					{
						// push on back of queue
						mFetchQueue.push_back(info);
					}
					mTimelyFetchPending = FALSE;
					mFetchTimer.reset();
					break;
				}

				// Not enough time has elapsed to do a new fetch
				break;
			}
			else
			{
				LLViewerInventoryItem* itemp = gInventory.getItem(info.mUUID);

				mFetchQueue.pop_front();
				if (!itemp) 
				{
					continue;
				}

				if (mFetchTimer.getElapsedTimeF32() > mMinTimeBetweenFetches)
				{
					itemp->fetchFromServer();
					mFetchTimer.reset();
					mTimelyFetchPending = TRUE;
				}
				else if (itemp->mIsComplete)
				{
					mTimelyFetchPending = FALSE;
				}
				else if (mFetchTimer.getElapsedTimeF32() > mMaxTimeBetweenFetches)
				{
					mFetchQueue.push_back(info);
					mFetchTimer.reset();
					mTimelyFetchPending = FALSE;
				}
				// Not enough time has elapsed to do a new fetch
				break;
			}
		}
		
		//
		// DEPRECATED OLD CODE
		//--------------------------------------------------------------------------------
#endif
	}
}

void LLInventoryModelBackgroundFetch::incrFetchCount(S32 fetching) 
{  
	mFetchCount += fetching; 
	if (mFetchCount < 0)
	{
		LL_WARNS_ONCE(LOG_INV) << "Inventory fetch count fell below zero (0)." << LL_ENDL;
		mFetchCount = 0; 
	}
}

// Bundle up a bunch of requests to send all at once.
void LLInventoryModelBackgroundFetch::bulkFetch()
{
	//Background fetch is called from gIdleCallbacks in a loop until background fetch is stopped.
	//If there are items in mFetchQueue, we want to check the time since the last bulkFetch was 
	//sent.  If it exceeds our retry time, go ahead and fire off another batch.  
	LLViewerRegion * region(gAgent.getRegion());
	if (! region || gDisconnected)
	{
		return;
	}

	// *TODO:  These values could be tweaked at runtime to effect
	// a fast/slow fetch throttle.  Once login is complete and the scene
	U32 const max_batch_size = 5;


	U32 item_count(0);
	U32 folder_count(0);

	const U32 sort_order(gSavedSettings.getU32(LLInventoryPanel::DEFAULT_SORT_ORDER) & 0x1);

	uuid_vec_t recursive_cats;

	U32 folder_lib_count=0;
	U32 item_lib_count=0;

	// This function can do up to four requests at once.
	LLPointer<AIPerService::Approvement> approved_folder;
	LLPointer<AIPerService::Approvement> approved_folder_lib;
	LLPointer<AIPerService::Approvement> approved_item;
	LLPointer<AIPerService::Approvement> approved_item_lib;

	LLSD folder_request_body;
	LLSD folder_request_body_lib;
	LLSD item_request_body;
	LLSD item_request_body_lib;

	while (! mFetchQueue.empty())
	{
		const FetchQueueInfo & fetch_info(mFetchQueue.front());
		if (fetch_info.mIsCategory)
		{
			const LLUUID & cat_id(fetch_info.mUUID);
			if (cat_id.isNull()) //DEV-17797
			{
				LLSD folder_sd;
				folder_sd["folder_id"]		= LLUUID::null.asString();
				folder_sd["owner_id"]		= gAgent.getID();
				folder_sd["sort_order"]		= LLSD::Integer(sort_order);
				folder_sd["fetch_folders"]	= LLSD::Boolean(FALSE);
				folder_sd["fetch_items"]	= LLSD::Boolean(TRUE);
				folder_request_body["folders"].append(folder_sd);
				folder_count++;
			}
			else
			{
				const LLViewerInventoryCategory * cat(gInventory.getCategory(cat_id));
		
				if (cat)
				{
					if (LLViewerInventoryCategory::VERSION_UNKNOWN == cat->getVersion())
					{
						LLSD folder_sd;
						folder_sd["folder_id"]		= cat->getUUID();
						folder_sd["owner_id"]		= cat->getOwnerID();
						folder_sd["sort_order"]		= LLSD::Integer(sort_order);
						folder_sd["fetch_folders"]	= LLSD::Boolean(TRUE); //(LLSD::Boolean)sFullFetchStarted;
						folder_sd["fetch_items"]	= LLSD::Boolean(TRUE);
				    
						if (ALEXANDRIA_LINDEN_ID == cat->getOwnerID())
						{
							if (folder_lib_count == max_batch_size ||
								(folder_lib_count == 0 &&
								 !(approved_folder_lib = AIPerService::approveHTTPRequestFor(mPerServicePtr, cap_inventory))))
							{
								break;
							}
							folder_request_body_lib["folders"].append(folder_sd);
							++folder_lib_count;
						}
						else
						{
							if (folder_count == max_batch_size ||
								(folder_count == 0 &&
								 !(approved_folder = AIPerService::approveHTTPRequestFor(mPerServicePtr, cap_inventory))))
							{
								break;
							}
							folder_request_body["folders"].append(folder_sd);
							++folder_count;
						}
					}
					// May already have this folder, but append child folders to list.
					if (fetch_info.mRecursive)
					{	
						LLInventoryModel::cat_array_t * categories(NULL);
						LLInventoryModel::item_array_t * items(NULL);
						gInventory.getDirectDescendentsOf(cat->getUUID(), categories, items);
						for (LLInventoryModel::cat_array_t::const_iterator it = categories->begin();
							 it != categories->end();
							 ++it)
						{
							mFetchQueue.push_back(FetchQueueInfo((*it)->getUUID(), fetch_info.mRecursive));
						}
					}
				}
			}
			if (fetch_info.mRecursive)
			{
				recursive_cats.push_back(cat_id);
			}
		}
		else
		{
			LLViewerInventoryItem * itemp(gInventory.getItem(fetch_info.mUUID));

			if (itemp)
			{
				LLSD item_sd;
				item_sd["owner_id"] = itemp->getPermissions().getOwner();
				item_sd["item_id"] = itemp->getUUID();
				if (itemp->getPermissions().getOwner() == gAgent.getID())
				{
					if (item_count == max_batch_size ||
						(item_count == 0 &&
						 !(approved_item = AIPerService::approveHTTPRequestFor(mPerServicePtr, cap_inventory))))
					{
						break;
					}
					item_request_body.append(item_sd);
					++item_count;
				}
				else
				{
					if (item_lib_count == max_batch_size ||
						(item_lib_count == 0 &&
						 !(approved_item_lib = AIPerService::approveHTTPRequestFor(mPerServicePtr, cap_inventory))))
					{
						break;
					}
					item_request_body_lib.append(item_sd);
					++item_lib_count;
				}
			}
		}

		mFetchQueue.pop_front();
	}
		
	if (item_count + folder_count + item_lib_count + folder_lib_count > 0)
	{
		if (folder_count)
		{
			if (folder_request_body["folders"].size())
			{
				const std::string url(region->getCapability("FetchInventoryDescendents2"));

				if (! url.empty())
				{
					BGFolderHttpHandler * handler(new BGFolderHttpHandler(folder_request_body, recursive_cats));
					LLHTTPClient::post_approved(url, folder_request_body, handler, approved_folder);
				}
			}
		}
		if (folder_lib_count)
		{
			if (folder_request_body_lib["folders"].size())
			{
				const std::string url(region->getCapability("FetchLibDescendents2"));

				if (! url.empty())
				{
					BGFolderHttpHandler * handler(new BGFolderHttpHandler(folder_request_body_lib, recursive_cats));
					LLHTTPClient::post_approved(url, folder_request_body_lib, handler, approved_folder_lib);
				}
			}
		}

		if (item_count)
		{
			if (item_request_body.size())
			{
				const std::string url(region->getCapability("FetchInventory2"));

				if (! url.empty())
				{
					LLSD body;
					body["agent_id"] = gAgent.getID();
					body["items"] = item_request_body;
					BGItemHttpHandler * handler(new BGItemHttpHandler(body));
					LLHTTPClient::post_approved(url, body, handler, approved_item);
				}
			}
		}
		if (item_lib_count)
		{
			if (item_request_body_lib.size())
			{
				const std::string url(region->getCapability("FetchLib2"));

				if (! url.empty())
				{
					LLSD body;
					body["agent_id"] = gAgent.getID();
					body["items"] = item_request_body_lib;
					BGItemHttpHandler * handler(new BGItemHttpHandler(body));
					LLHTTPClient::post_approved(url, body, handler, approved_item_lib);
				}
			}
		}
		mFetchTimer.reset();
	}
	else if (isBulkFetchProcessingComplete())
	{
		LL_INFOS() << "Inventory fetch completed" << LL_ENDL;
		setAllFoldersFetched();
	}
}

bool LLInventoryModelBackgroundFetch::fetchQueueContainsNoDescendentsOf(const LLUUID& cat_id) const
{
	for (fetch_queue_t::const_iterator it = mFetchQueue.begin();
		 it != mFetchQueue.end();
		 ++it)
	{
		const LLUUID& fetch_id = (*it).mUUID;
		if (gInventory.isObjectDescendentOf(fetch_id, cat_id))
			return false;
	}
	return true;
}
// If we get back a normal response, handle it here.
void BGFolderHttpHandler::httpSuccess(void)
{
	LLInventoryModelBackgroundFetch *fetcher = LLInventoryModelBackgroundFetch::getInstance();
	const LLSD& content = mContent;
	// in response as an application-level error.

	// Instead, we assume success and attempt to extract information.
	if (content.has("folders"))	
	{
		LLSD folders(content["folders"]);
		
		for (LLSD::array_const_iterator folder_it = folders.beginArray();
			folder_it != folders.endArray();
			++folder_it)
		{	
			LLSD folder_sd(*folder_it);

			//LLUUID agent_id = folder_sd["agent_id"];

			//if(agent_id != gAgent.getID())	//This should never happen.
			//{
			//	LL_WARNS(LOG_INV) << "Got a UpdateInventoryItem for the wrong agent."
			//			<< LL_ENDL;
			//	break;
			//}

			LLUUID parent_id(folder_sd["folder_id"].asUUID());
			LLUUID owner_id(folder_sd["owner_id"].asUUID());
			S32    version(folder_sd["version"].asInteger());
			S32    descendents(folder_sd["descendents"].asInteger());
			LLPointer<LLViewerInventoryCategory> tcategory = new LLViewerInventoryCategory(owner_id);

            if (parent_id.isNull())
            {
				LLSD items(folder_sd["items"]);
			    LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
				
			    for (LLSD::array_const_iterator item_it = items.beginArray();
				    item_it != items.endArray();
				    ++item_it)
			    {	
                    const LLUUID lost_uuid(gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND));

                    if (lost_uuid.notNull())
                    {
				        LLSD item(*item_it);

				        titem->unpackMessage(item);
				
                        LLInventoryModel::update_list_t update;
                        LLInventoryModel::LLCategoryUpdate new_folder(lost_uuid, 1);
                        update.push_back(new_folder);
                        gInventory.accountForUpdate(update);

                        titem->setParent(lost_uuid);
                        titem->updateParentOnServer(FALSE);
                        gInventory.updateItem(titem);
                        gInventory.notifyObservers();
                    }
                }
            }

	        LLViewerInventoryCategory * pcat(gInventory.getCategory(parent_id));
			if (! pcat)
			{
				continue;
			}

			LLSD categories(folder_sd["categories"]);
			for (LLSD::array_const_iterator category_it = categories.beginArray();
				category_it != categories.endArray();
				++category_it)
			{	
				LLSD category(*category_it);
				tcategory->fromLLSD(category); 
				
				const bool recursive(getIsRecursive(tcategory->getUUID()));
				if (recursive)
				{
					fetcher->addRequestAtBack(tcategory->getUUID(), recursive, true);
				}
				else if (! gInventory.isCategoryComplete(tcategory->getUUID()))
				{
					gInventory.updateCategory(tcategory);
				}
			}

			LLSD items(folder_sd["items"]);
			LLPointer<LLViewerInventoryItem> titem = new LLViewerInventoryItem;
			for (LLSD::array_const_iterator item_it = items.beginArray();
				 item_it != items.endArray();
				 ++item_it)
			{	
				LLSD item(*item_it);
				titem->unpackMessage(item);
				
				gInventory.updateItem(titem);
			}

			// Set version and descendentcount according to message.
			LLViewerInventoryCategory * cat(gInventory.getCategory(parent_id));
			if (cat)
			{
				cat->setVersion(version);
				cat->setDescendentCount(descendents);
				cat->determineFolderType();
			}
		}
	}
		
	if (content.has("bad_folders"))
	{
		LLSD bad_folders(content["bad_folders"]);
		for (LLSD::array_const_iterator folder_it = bad_folders.beginArray();
			 folder_it != bad_folders.endArray();
			 ++folder_it)
		{
			// *TODO: Stop copying data [ed:  this isn't copying data]
			LLSD folder_sd(*folder_it);
			
			// These folders failed on the dataserver.  We probably don't want to retry them.
			LL_WARNS(LOG_INV) << "Folder " << folder_sd["folder_id"].asString() 
							  << "Error: " << folder_sd["error"].asString() << LL_ENDL;
		}
	}
	
	if (fetcher->isBulkFetchProcessingComplete())
	{
		LL_INFOS() << "Inventory fetch completed" << LL_ENDL;
		fetcher->setAllFoldersFetched();
	}
	
	gInventory.notifyObservers();
}

//If we get back an error (not found, etc...), handle it here
void BGFolderHttpHandler::httpFailure(void)
{
	LLInventoryModelBackgroundFetch *fetcher = LLInventoryModelBackgroundFetch::getInstance();

	LL_INFOS() << "BGFolderHttpHandler::error "
		<< mStatus << ": " << mReason << LL_ENDL;

	if (is_internal_http_error_that_warrants_a_retry(mStatus)) // timed out
	{
		for (auto const& entry : mRequestSD["folders"].array())
		{
			LLUUID folder_id(entry["folder_id"].asUUID());
			const BOOL recursive = getIsRecursive(folder_id);
			fetcher->addRequestAtFront(folder_id, recursive, true);
		}
	}
	else
	{
		if (fetcher->isBulkFetchProcessingComplete())
		{
			fetcher->setAllFoldersFetched();
		}
	}
	gInventory.notifyObservers();
}

BOOL BGFolderHttpHandler::getIsRecursive(const LLUUID& cat_id) const
{
	return (std::find(mRecursiveCatUUIDs.begin(),mRecursiveCatUUIDs.end(), cat_id) != mRecursiveCatUUIDs.end());
}


