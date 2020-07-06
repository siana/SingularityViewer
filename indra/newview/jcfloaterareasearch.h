/* Copyright (c) 2009
 *
 * Modular Systems. All rights reserved.
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
 *
 * Modified, debugged, optimized and improved by Henri Beauchamp Feb 2010.
 */

#ifndef JC_FLOATERAREASEARCH_H
#define JC_FLOATERAREASEARCH_H

#include "llfloater.h"
#include "lluuid.h"
#include "llstring.h"
#include "llframetimer.h"
#include <boost/unordered_map.hpp>

class LLTextBox;
class LLScrollListCtrl;
class LLViewerRegion;

class JCFloaterAreaSearch : public LLFloater, public LLFloaterSingleton<JCFloaterAreaSearch>
{
public:
	JCFloaterAreaSearch(const LLSD& data);
	virtual ~JCFloaterAreaSearch();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void close(bool app = false);
	/*virtual*/ void onOpen();

	void results();
	static void processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data);
	struct ObjectData
	{
		LLUUID id;
		std::string name;
		std::string desc;
		LLUUID owner_id;
		LLUUID group_id;
	};

	const ObjectData* getObjectData(const LLUUID& id) const
	{
		const auto& it = mCachedObjects.find(id);
		return it != mCachedObjects.end() ? &it->second : nullptr;
	}

private:

	enum OBJECT_COLUMN_ORDER
	{
		LIST_OBJECT_NAME = 0,
		LIST_OBJECT_DESC,
		LIST_OBJECT_OWNER,
		LIST_OBJECT_GROUP,
		LIST_OBJECT_COUNT
	};

	void checkRegion(bool force_clear = false);
	void onStop();
	void onRefresh();
	void onCommitLine(LLUICtrl* caller, const LLSD& value, OBJECT_COLUMN_ORDER type);
	bool requestIfNeeded(LLUUID object_id);
	class LLViewerObject* getSelectedObject();
	void onDoubleClick();
	void teleportToSelected();
	void lookAtSelected();

	LLTextBox* mCounterText;
	LLScrollListCtrl* mResultList;
	LLFrameTimer mLastUpdateTimer;
	LLViewerRegion* mLastRegion;
	bool mStopped;

	uuid_set_t mPendingObjects;
	boost::unordered_map<LLUUID, ObjectData> mCachedObjects;

	std::string mFilterStrings[LIST_OBJECT_COUNT];
};

#endif
