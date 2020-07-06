/* Copyright (c) 2009
 *
 * Modular Systems All rights reserved.
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
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
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
 */

#include "llviewerprecompiledheaders.h"

#include "scriptcounter.h"

#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llviewerregion.h"
#include "llselectmgr.h"
#include "lltrans.h"
#include "llvoavatar.h"
#include "stringize.h"

void cmdline_printchat(const std::string& message);

LLVOAvatar* find_avatar_from_object( LLViewerObject* object );

std::map<LLUUID, ScriptCounter*> ScriptCounter::sCheckMap;

ScriptCounter::ScriptCounter(bool do_delete, LLViewerObject* object)
:	LLInstanceTracker<ScriptCounter, LLUUID>(object->getID())
,	doDelete(do_delete)
,	foo(object)
,	inventories()
,	objectCount()
,	requesting(true)
,	scriptcount()
,	checking()
,	mRunningCount()
,	mMonoCount()
{
	llassert(foo); // Object to ScriptCount must not be null
}

ScriptCounter::~ScriptCounter()
{}

// Request the inventory for all parts
void ScriptCounter::requestInventories()
{
	if (foo->isAvatar()) // If it's an avatar, iterate through all the attachments
	{
		doDelete = false; // We don't support deleting all scripts in all attachments, such a feature could be dangerous.
		LLVOAvatar* av = static_cast<LLVOAvatar*>(foo);

		// Iterate through all the attachment points
		for (const auto& i : av->mAttachmentPoints)
		{
			if (LLViewerJointAttachment* attachment = i.second)
			{
				if (!attachment->getValid()) continue;

				// Iterate through all the attachments on this point
				for (const auto& object : attachment->mAttachedObjects)
					if (object)
						requestInventoriesFor(object);
			}
		}
	}
	else // Iterate through all the selected objects
	{
		auto selection = LLSelectMgr::getInstance()->getSelection();
		for (auto i = selection->valid_root_begin(), end = selection->valid_root_end(); i != end; ++i)
			if (LLSelectNode* selectNode = *i)
				if (LLViewerObject* object = selectNode->getObject())
					requestInventoriesFor(object);
	}
	if (!doDelete) cmdline_printchat(LLTrans::getString("ScriptCounting"));
	requesting = false;
}

// Request the inventories of each object and its child prims
void ScriptCounter::requestInventoriesFor(LLViewerObject* object)
{
	++objectCount;
	requestInventoryFor(object);
	for (auto child : object->getChildren())
	{
		if (child->isAvatar()) continue;
		requestInventoryFor(child);
	}
}

// Request inventory for each individual prim
void ScriptCounter::requestInventoryFor(LLViewerObject* object)
{
	//LL_INFOS() << "Requesting inventory of " << object->getID() << LL_ENDL;
	++inventories;
	object->registerInventoryListener(this, NULL);
	object->dirtyInventory();
	object->requestInventory();
}

// An inventory has been received, count/delete the scripts in it
void ScriptCounter::inventoryChanged(LLViewerObject* obj, LLInventoryObject::object_list_t* inv, S32, void*)
{
	obj->removeInventoryListener(this);
	--inventories;
	//const LLUUID& objid = obj->getID();
	//LL_INFOS() << "Counting scripts in " << objid << LL_ENDL;

	if (inv)
	{
		uuid_vec_t ids;

		for (auto asset : *inv)
		{
			const LLUUID& id = asset->getUUID();
			if (asset->getType() == LLAssetType::AT_LSL_TEXT && id.notNull())
			{
				++scriptcount;
				if (doDelete)
					ids.push_back(id);
				else
				{
					LLMessageSystem* msg = gMessageSystem;
					msg->newMessageFast(_PREHASH_GetScriptRunning);
					msg->nextBlockFast(_PREHASH_Script);
					msg->addUUIDFast(_PREHASH_ObjectID, obj->getID());
					msg->addUUIDFast(_PREHASH_ItemID, id);
					msg->sendReliable(obj->getRegion()->getHost());
					sCheckMap[id] = this;
					++checking;
				}
			}
		}

		for (const auto& id : ids)
		{
			//LL_INFOS() << "Deleting script " << id << " in " << objid << LL_ENDL;
			obj->removeInventory(id);
		}
	}

	summarize();
}

void ScriptCounter::processScriptRunningReply(LLMessageSystem* msg)
{
	if (!sCheckMap.empty())
	{
		LLUUID item_id;
		msg->getUUIDFast(_PREHASH_Script, _PREHASH_ItemID, item_id);
		auto it = sCheckMap.find(item_id);
		if (it != sCheckMap.end())
		{
			it->second->processRunningReply(msg);
			sCheckMap.erase(it);
		}
	}
}

void ScriptCounter::processRunningReply(LLMessageSystem* msg)
{
	BOOL is;
	msg->getBOOLFast(_PREHASH_Script, _PREHASH_Running, is);
	if (is) ++mRunningCount;
	msg->getBOOLFast(_PREHASH_Script, "Mono", is);
	if (is) ++mMonoCount;
	--checking;

	summarize();
}

void ScriptCounter::summarize()
{
	// Done requesting and there are no more inventories to receive
	// And we're not checking any scripts for running/mono properties
	if (!requesting && !inventories && !checking)
	{
		LLStringUtil::format_map_t args;
		args["SCRIPTS"] = stringize(scriptcount);
		args["OBJECTS"] = stringize(objectCount);
		args["RUNNING"] = stringize(mRunningCount);
		args["MONO"] = stringize(mMonoCount);
		if (foo->isAvatar())
		{
			args["NAME"] = LLAvatarActions::getSLURL(foo->getID());
			cmdline_printchat(LLTrans::getString("ScriptCountAvatar", args));
		}
		else
			cmdline_printchat(LLTrans::getString(doDelete ? "ScriptDeleteObject" : "ScriptCountObject", args));

		delete this;
	}
}
