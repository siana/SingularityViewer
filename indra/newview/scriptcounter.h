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
#ifndef SCRIPTCOUNTER_H
#define SCRIPTCOUNTER_H

#include "llvoinventorylistener.h"

class ScriptCounter final : public LLInstanceTracker<ScriptCounter, LLUUID>, public LLVOInventoryListener
{
public:
	ScriptCounter(bool do_delete, LLViewerObject* object);
	~ScriptCounter();

	void inventoryChanged(LLViewerObject* obj, LLInventoryObject::object_list_t* inv, S32, void*) override;
	void requestInventories();
	static void processScriptRunningReply(LLMessageSystem* msg);

private:
	void requestInventoriesFor(LLViewerObject* object);
	void requestInventoryFor(LLViewerObject* object);
	void processRunningReply(LLMessageSystem* msg);
	void summarize(); // Check if finished, if so, output and destroy.

	bool doDelete;
	LLViewerObject* foo;
	int inventories;
	int objectCount;
	bool requesting;
	int scriptcount;
	static std::map<LLUUID, ScriptCounter*> sCheckMap; // Map of scripts being checked running/mono and by which instance
	int checking; // Number of scripts being counter by this instance
	int mRunningCount;
	int mMonoCount;
};

#endif //SCRIPTCOUNTER_H

