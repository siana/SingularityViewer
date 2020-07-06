/** 
 * @file llcompilequeue.h
 * @brief LLCompileQueue class header file
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

#ifndef LL_LLCOMPILEQUEUE_H
#define LL_LLCOMPILEQUEUE_H

#include "llinventory.h"
#include "llviewerobject.h"
#include "llvoinventorylistener.h"
#include "llmap.h"
#include "lluuid.h"

#include "llfloater.h"

#include "llviewerinventory.h"

class LLScrollListCtrl;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterScriptQueue
//
// This class provides a mechanism of adding objects to a list that
// will go through and execute action for the scripts on each object. The
// objects will be accessed serially and the scripts may be
// manipulated in parallel. For example, selecting two objects each
// with three scripts will result in the first object having all three
// scripts manipulated.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterScriptQueue : public LLFloater, public LLVOInventoryListener
{
public:
	// find an instance by ID. Return NULL if it does not exist.
	static LLFloaterScriptQueue* findInstance(const LLUUID& id);
	LLFloaterScriptQueue(const std::string& name, const LLRect& rect,
						 const std::string& title, const std::string& start_string);
	virtual ~LLFloaterScriptQueue();

	/*virtual*/ BOOL postBuild();

	void setMono(bool mono) { mMono = mono; }

	// addObject() accepts an object id.
	void addObject(const LLUUID& id);

	// start() returns TRUE if the queue has started, otherwise FALSE.
	BOOL start();

protected:
	// This is the callback method for the viewer object currently
	// being worked on.
	/*virtual*/ void inventoryChanged(LLViewerObject* obj,
								  LLInventoryObject::object_list_t* inv,
								 S32 serial_num,
								 void* queue);

	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								 LLInventoryObject::object_list_t* inv) = 0;

	static void onCloseBtn(void* user_data);

	// returns true if this is done
	BOOL isDone() const;

	virtual BOOL startQueue();

	// go to the next object. If no objects left, it falls out
	// silently and waits to be killed by the deleteIfDone() callback.
	BOOL nextObject();
	BOOL popNext();

	void setStartString(const std::string& s) { mStartString = s; }

	// Get this instance's ID.
	const LLUUID& getID() const { return mID; } 

protected:
	// UI
	LLScrollListCtrl* mMessages;
	LLButton* mCloseBtn;

	// Object Queue
	uuid_vec_t mObjectIDs;
	LLUUID mCurrentObjectID;
	bool mDone;

	LLUUID mID;
	static LLMap<LLUUID, LLFloaterScriptQueue*> sInstances;

	std::string mStartString;
	bool mMono;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterCompileQueue
//
// This script queue will recompile each script.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct LLCompileQueueData
{
	LLUUID mQueueID;
	LLUUID mItemId;
	LLCompileQueueData(const LLUUID& q_id, const LLUUID& item_id) :
		mQueueID(q_id), mItemId(item_id) {}
};

class LLAssetUploadQueue;

class LLFloaterCompileQueue : public LLFloaterScriptQueue
{
public:
	// Use this method to create a compile queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterCompileQueue* create(bool mono);

	// remove any object in mScriptScripts with the matching uuid.
	void removeItemByItemID(const LLUUID& item_id);

	LLAssetUploadQueue* getUploadQueue() { return mUploadQueue; }

	void experienceIdsReceived(const LLSD& content);
	BOOL hasExperience(const LLUUID& id) const;

protected:
	LLFloaterCompileQueue(const std::string& name, const LLRect& rect);
	virtual ~LLFloaterCompileQueue();
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								 LLInventoryObject::object_list_t* inv);

	static void requestAsset(struct LLScriptQueueData* datap, const LLSD& experience);


	// This is the callback for when each script arrives
	static void scriptArrived(LLVFS *vfs, const LLUUID& asset_id,
								LLAssetType::EType type,
								void* user_data, S32 status, LLExtStat ext_status);
	
	// remove any object in mScriptScripts with the matching uuid.
	void removeItemByAssetID(const LLUUID& asset_id);

	// save the items indicated by the item id.
	void saveItemByItemID(const LLUUID& item_id);

	// find InventoryItem given item id.
	const LLInventoryItem* findItemByItemID(const LLUUID& item_id) const;

	virtual BOOL startQueue();
protected:
	LLViewerInventoryItem::item_array_t mCurrentScripts;

private:
	LLAssetUploadQueue* mUploadQueue;
	uuid_list_t mExperienceIds;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterResetQueue
//
// This script queue will reset each script.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterResetQueue : public LLFloaterScriptQueue
{
public:
	// Use this method to create a reset queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterResetQueue* create();

protected:
	LLFloaterResetQueue(const std::string& name, const LLRect& rect);
	virtual ~LLFloaterResetQueue();

	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								 LLInventoryObject::object_list_t* inv);	
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterRunQueue
//
// This script queue will set each script as running.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterRunQueue : public LLFloaterScriptQueue
{
public:
	// Use this method to create a run queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterRunQueue* create();

protected:
	LLFloaterRunQueue(const std::string& name, const LLRect& rect);
	virtual ~LLFloaterRunQueue();

	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								 LLInventoryObject::object_list_t* inv);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterNotRunQueue
//
// This script queue will set each script as not running.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterNotRunQueue : public LLFloaterScriptQueue
{
public:
	// Use this method to create a not run queue. Once created, it
	// will be responsible for it's own destruction.
	static LLFloaterNotRunQueue* create();

protected:
	LLFloaterNotRunQueue(const std::string& name, const LLRect& rect);
	virtual ~LLFloaterNotRunQueue();
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								 LLInventoryObject::object_list_t* inv);
};

#endif // LL_LLCOMPILEQUEUE_H
