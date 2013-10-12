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

#include "llavatarnamecache.h"
#include "llselectmgr.h"
#include "lltrans.h"
#include "llvoavatar.h"
#include "stringize.h"

void cmdline_printchat(std::string chat);

LLVOAvatar* find_avatar_from_object( LLViewerObject* object );

namespace
{
	void countedScriptsOnAvatar(LLStringUtil::format_map_t args, const LLAvatarName& av_name)
	{
		std::string name;
		LLAvatarNameCache::getPNSName(av_name, name);
		args["NAME"] = name;
		cmdline_printchat(LLTrans::getString("ScriptCountAvatar", args));
	}
}

ScriptCounter::ScriptCounter(bool do_delete, LLViewerObject* object)
:	doDelete(do_delete)
,	foo(object)
,	inventories()
,	objectCount()
,	requesting(true)
,	scriptcount()
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
		for (LLVOAvatar::attachment_map_t::iterator i = av->mAttachmentPoints.begin(); i != av->mAttachmentPoints.end(); ++i)
		{
			if (LLViewerJointAttachment* attachment = i->second)
			{
				if (!attachment->getValid()) continue;

				// Iterate through all the attachments on this point
				for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator j = attachment->mAttachedObjects.begin(); j != attachment->mAttachedObjects.end(); ++j)
					if (LLViewerObject* object = *j)
						requestInventoriesFor(object);
			}
		}
	}
	else // Iterate through all the selected objects
	{
		for (LLObjectSelection::valid_root_iterator i = LLSelectMgr::getInstance()->getSelection()->valid_root_begin(); i != LLSelectMgr::getInstance()->getSelection()->valid_root_end(); ++i)
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
	LLViewerObject::child_list_t child_list = object->getChildren();
	for (LLViewerObject::child_list_t::iterator i = child_list.begin(); i != child_list.end(); ++i)
	{
		LLViewerObject* child = *i;
		if (child->isAvatar()) continue;
		requestInventoryFor(child);
	}
}

// Request inventory for each individual prim
void ScriptCounter::requestInventoryFor(LLViewerObject* object)
{
	//llinfos << "Requesting inventory of " << object->getID() << llendl;
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
	//llinfos << "Counting scripts in " << objid << llendl;

	if (inv)
	{
		LLInventoryObject::object_list_t::const_iterator end = inv->end();
		for (LLInventoryObject::object_list_t::const_iterator i = inv->begin(); i != end; ++i)
			if (LLInventoryObject* asset = (*i))
				if (asset->getType() == LLAssetType::AT_LSL_TEXT)
				{
					++scriptcount;
					if (doDelete)
					{
						const LLUUID& id = asset->getUUID();
						if (id.notNull())
						{
							//llinfos << "Deleting script " << id << " in " << objid << llendl;
							obj->removeInventory(id);
							--i; // Avoid iteration when removing, everything has shifted
							end = inv->end();
						}
					}
				}
	}

	// Done requesting and there are no more inventories to receive
	if (!requesting && !inventories)
	{
		LLStringUtil::format_map_t args;
		args["SCRIPTS"] = stringize(scriptcount);
		args["OBJECTS"] = stringize(objectCount);
		if (foo->isAvatar())
			LLAvatarNameCache::get(foo->getID(), boost::bind(countedScriptsOnAvatar, args, _2));
		else
			cmdline_printchat(LLTrans::getString(doDelete ? "ScriptDeleteObject" : "ScriptCountObject", args));

		delete this;
	}
}
