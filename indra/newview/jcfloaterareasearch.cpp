/* Copyright (c) 2009
 *
 * Modular Systems Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems Ltd nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS LTD AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified, debugged, optimized and improved by Henri Beauchamp Feb 2010.
 */

#include "llviewerprecompiledheaders.h"

#include "jcfloaterareasearch.h"

#include "llfiltereditor.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "lltracker.h"
#include "llviewerobjectlist.h"

const std::string request_string = "JCFloaterAreaSearch::Requested_\xF8\xA7\xB5";
const F32 min_refresh_interval = 0.25f;	// Minimum interval between list refreshes in seconds.

JCFloaterAreaSearch::JCFloaterAreaSearch(const LLSD& data) :
	LLFloater(),
	mCounterText(0),
	mResultList(0),
	mLastRegion(0),
	mStopped(false)
{
	mLastUpdateTimer.reset();
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_area_search.xml");
}

JCFloaterAreaSearch::~JCFloaterAreaSearch()
{
}

void JCFloaterAreaSearch::close(bool app)
{
	if (app || mStopped)
	{
		LLFloater::close(app);
	}
	else
	{
		setVisible(FALSE);
	}
}

BOOL JCFloaterAreaSearch::postBuild()
{
	mResultList = getChild<LLScrollListCtrl>("result_list");
	mResultList->setDoubleClickCallback(boost::bind(&JCFloaterAreaSearch::onDoubleClick,this));
	mResultList->sortByColumn("Name", TRUE);
	auto tp = getChild<LLButton>("TP");
	auto look = getChild<LLButton>("Look");
	mResultList->setCommitOnSelectionChange(true);
	mResultList->setCommitCallback([=](LLUICtrl* ctrl, const LLSD& param){
		bool enabled = mResultList->getNumSelected() == 1;
		tp->setEnabled(enabled);
		look->setEnabled(enabled);
	});

	mCounterText = getChild<LLTextBox>("counter");

	getChild<LLButton>("Refresh")->setClickedCallback(boost::bind(&JCFloaterAreaSearch::onRefresh,this));
	getChild<LLButton>("Stop")->setClickedCallback(boost::bind(&JCFloaterAreaSearch::onStop,this));
	tp->setClickedCallback(boost::bind(&JCFloaterAreaSearch::teleportToSelected, this));
	look->setClickedCallback(boost::bind(&JCFloaterAreaSearch::lookAtSelected, this));

	getChild<LLFilterEditor>("Name query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_NAME));
	getChild<LLFilterEditor>("Description query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_DESC));
	getChild<LLFilterEditor>("Owner query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_OWNER));
	getChild<LLFilterEditor>("Group query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_GROUP));

	return TRUE;
}

void JCFloaterAreaSearch::onOpen()
{
	checkRegion();
	results();
}

void JCFloaterAreaSearch::checkRegion(bool force_clear)
{
	// Check if we changed region, and if we did, clear the object details cache.
	LLViewerRegion* region = gAgent.getRegion();
	if (force_clear || region != mLastRegion)
	{
		mLastRegion = region;
		mPendingObjects.clear();
		mCachedObjects.clear();
		mResultList->deleteAllItems();
		mCounterText->setText(std::string("Listed/Pending/Total"));
	}
}

LLViewerObject* JCFloaterAreaSearch::getSelectedObject()
{
	if (LLScrollListItem* item = mResultList->getFirstSelected())
		return gObjectList.findObject(item->getUUID());
	return NULL;
}

void JCFloaterAreaSearch::onDoubleClick()
{
	if (LLViewerObject* objectp = getSelectedObject())
		LLTracker::trackLocation(objectp->getPositionGlobal(), mCachedObjects[objectp->getID()].name, "", LLTracker::LOCATION_ITEM);
}

void JCFloaterAreaSearch::teleportToSelected()
{
	if (LLViewerObject* objectp = getSelectedObject())
		gAgent.teleportViaLocation(objectp->getPositionGlobal());
}

void JCFloaterAreaSearch::lookAtSelected()
{
	if (LLScrollListItem* item = mResultList->getFirstSelected())
		gAgentCamera.lookAtObject(item->getUUID(), false);
}


void JCFloaterAreaSearch::onStop()
{
	mStopped = true;
	mPendingObjects.clear();
	mCounterText->setText(std::string("Stopped"));
}

void JCFloaterAreaSearch::onRefresh()
{
	//LL_INFOS() << "Clicked search" << LL_ENDL;
	mStopped = false;
	checkRegion(true);
	results();
}

void JCFloaterAreaSearch::onCommitLine(LLUICtrl* caller, const LLSD& value, OBJECT_COLUMN_ORDER type)
{
	std::string text = value.asString();
	LLStringUtil::toLower(text);
	caller->setValue(text);
 	mFilterStrings[type] = text;
	//LL_INFOS() << "loaded " << name << " with "<< text << LL_ENDL;
	checkRegion();
	results();
}

bool JCFloaterAreaSearch::requestIfNeeded(LLUUID object_id)
{
	if (!mCachedObjects.count(object_id) && !mPendingObjects.count(object_id))
	{
		if(mStopped)
			return true;

		//LL_INFOS() << "not in list" << LL_ENDL;
		mPendingObjects.insert(object_id);

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_RequestFlags, 0 );
		msg->addUUIDFast(_PREHASH_ObjectID, object_id);
		gAgent.sendReliableMessage();
		//LL_INFOS() << "Sent data request for object " << object_id << LL_ENDL;
		return true;
	}
	return false;
}

void JCFloaterAreaSearch::results()
{
	if (!getVisible()) return;

	if (mPendingObjects.size() > 0 && mLastUpdateTimer.getElapsedTimeF32() < min_refresh_interval) return;
	//LL_INFOS() << "results()" << LL_ENDL;
	uuid_vec_t selected = mResultList->getSelectedIDs();
	S32 scrollpos = mResultList->getScrollPos();
	mResultList->deleteAllItems();
	S32 i;
	S32 total = gObjectList.getNumObjects();

	LLViewerRegion* our_region = gAgent.getRegion();
	for (i = 0; i < total; i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (objectp)
		{
			if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
				!objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
			{
				LLUUID object_id = objectp->getID();
				if(!requestIfNeeded(object_id))
				{
					auto it = mCachedObjects.find(object_id);
					if(it != mCachedObjects.end())
					{
						//LL_INFOS() << "all entries are \"\" or we have data" << LL_ENDL;
						std::string object_name = it->second.name;
						std::string object_desc = it->second.desc;
						std::string object_owner;
						std::string object_group;
						gCacheName->getFullName(it->second.owner_id, object_owner);
						gCacheName->getGroupName(it->second.group_id, object_group);
						//LL_INFOS() << "both names are loaded or aren't needed" << LL_ENDL;
						std::string onU = object_owner;
						std::string cnU = object_group;
						LLStringUtil::toLower(object_name);
						LLStringUtil::toLower(object_desc);
						LLStringUtil::toLower(object_owner);
						LLStringUtil::toLower(object_group);
						if ((mFilterStrings[LIST_OBJECT_NAME].empty() || object_name.find(mFilterStrings[LIST_OBJECT_NAME]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_DESC].empty() || object_desc.find(mFilterStrings[LIST_OBJECT_DESC]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_OWNER].empty() || object_owner.find(mFilterStrings[LIST_OBJECT_OWNER]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_GROUP].empty() || object_group.find(mFilterStrings[LIST_OBJECT_GROUP]) != std::string::npos))
						{
							//LL_INFOS() << "pass" << LL_ENDL;
							LLSD element;
							element["id"] = object_id;
							element["columns"][LIST_OBJECT_NAME]["column"] = "Name";
							element["columns"][LIST_OBJECT_NAME]["type"] = "text";
							element["columns"][LIST_OBJECT_NAME]["value"] = it->second.name;
							element["columns"][LIST_OBJECT_DESC]["column"] = "Description";
							element["columns"][LIST_OBJECT_DESC]["type"] = "text";
							element["columns"][LIST_OBJECT_DESC]["value"] = it->second.desc;
							element["columns"][LIST_OBJECT_OWNER]["column"] = "Owner";
							element["columns"][LIST_OBJECT_OWNER]["type"] = "text";
							element["columns"][LIST_OBJECT_OWNER]["value"] = onU;
							element["columns"][LIST_OBJECT_GROUP]["column"] = "Group";
							element["columns"][LIST_OBJECT_GROUP]["type"] = "text";
							element["columns"][LIST_OBJECT_GROUP]["value"] = cnU;			//ai->second;
							mResultList->addElement(element, ADD_BOTTOM);
						}
						
					}
				}
			}
		}
	}

	mResultList->updateSort();
	mResultList->selectMultiple(selected);
	mResultList->setScrollPos(scrollpos);
	mCounterText->setText(llformat("%d listed/%d pending/%d total", mResultList->getItemCount(), mPendingObjects.size(), mPendingObjects.size()+mCachedObjects.size()));
	mLastUpdateTimer.reset();
}

// static
void JCFloaterAreaSearch::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{
	JCFloaterAreaSearch* floater = findInstance();
	if(!floater)
		return;
	floater->checkRegion();

	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id);

	auto it = floater->mPendingObjects.find(object_id);
	if(it != floater->mPendingObjects.end())
		floater->mPendingObjects.erase(it);
	//else if(floater->mCachedObjects.count(object_id)) //Let entries update.
	//	return;

	ObjectData* data = &floater->mCachedObjects[object_id];
	// We cache unknown objects (to avoid having to request them later)
	// and requested objects.
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, data->owner_id);
	if (auto obj = gObjectList.findObject(object_id)) obj->mOwnerID = data->owner_id; // Singu Note: Try to get Owner whenever possible
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, data->group_id);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, data->name);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, data->desc);
	gCacheName->get(data->owner_id, false, boost::bind(&JCFloaterAreaSearch::results,floater));
	gCacheName->get(data->group_id, true, boost::bind(&JCFloaterAreaSearch::results,floater));
	//LL_INFOS() << "Got info for " << (exists ? "requested" : "unknown") << " object " << object_id << LL_ENDL;
}
