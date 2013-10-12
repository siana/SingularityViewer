/** 
 * @file llfloaterobjectiminfo.cpp
 * @brief A floater with information about an object that sent an IM.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterobjectiminfo.h"

#include "llagentdata.h"
#include "llavataractions.h"
#include "llcachename.h"
#include "llcommandhandler.h"
#include "llfloatergroupinfo.h"
#include "llfloatermute.h"
#include "llgroupactions.h"
#include "llmutelist.h"
#include "llslurl.h"
#include "lltrans.h"
#include "llui.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llurlaction.h"
#include "llweb.h"

// [RLVa:KB] - Version: 1.23.4
#include "rlvhandler.h"
// [/RLVa:KB]

////////////////////////////////////////////////////////////////////////////
// LLFloaterObjectIMInfo

LLFloaterObjectIMInfo::LLFloaterObjectIMInfo(const LLSD& seed)
: mObjectID(), mName(), mSLurl(), mOwnerID(), mGroupOwned(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_object_im_info.xml");
	
	if (getRect().mLeft == 0 
		&& getRect().mBottom == 0)
	{
		center();
	}
}

BOOL LLFloaterObjectIMInfo::postBuild(void)
{
	childSetAction("Mute",onClickMute,this);
	childSetActionTextbox("OwnerName",onClickOwner, this);
	childSetActionTextbox("Slurl",onClickMap, this);

	return true;
}

void LLFloaterObjectIMInfo::update(LLSD& data)
{
	// Extract appropriate object information from input LLSD
	// (Eventually, it might be nice to query server for details
	// rather than require caller to pass in the information.)
	mObjectID   = data["object_id"].asUUID();
	mName       = data["name"].asString();
	mOwnerID    = data["owner_id"].asUUID();
	mGroupOwned = data["group_owned"].asBoolean();
	mSLurl      = data["slurl"].asString();

	// When talking to an old region we won't have a slurl.
	// The object id isn't really the object id either but we don't use it so who cares.
	//bool have_slurl = !slurl.empty();
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a) | Added: RLVa-0.2.0g
	bool have_slurl = (!mSLurl.empty()) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));
// [/RLVa:KB]
	childSetVisible("Unknown_Slurl",!have_slurl);
	childSetVisible("Slurl",have_slurl);

	childSetText("ObjectName",mName);
	childSetText("Slurl",mSLurl);
	childSetText("OwnerName",std::string(""));

//	bool my_object = (owner_id == gAgentID);
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	bool my_object = (mOwnerID == gAgentID) || ((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(mOwnerID)));
// [/RLVa:KB]
	childSetEnabled("Mute",!my_object);
	
	if (gCacheName) gCacheName->get(mOwnerID,mGroupOwned,boost::bind(&LLFloaterObjectIMInfo::nameCallback,this,_1,_2,_3));
}

//static 
void LLFloaterObjectIMInfo::onClickMap(void* data)
{
	LLFloaterObjectIMInfo* self = (LLFloaterObjectIMInfo*)data;

	std::string url = "secondlife://" + self->mSLurl;
	LLUrlAction::showLocationOnMap(url);
}

//static 
void LLFloaterObjectIMInfo::onClickOwner(void* data)
{
	LLFloaterObjectIMInfo* self = (LLFloaterObjectIMInfo*)data;
	if (self->mGroupOwned)
	{
		LLGroupActions::show(self->mOwnerID);
	}
//	else
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	else if ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) || (!RlvUtil::isNearbyAgent(self->mOwnerID)) )
// [/RLVa:KB]
	{
		LLAvatarActions::showProfile(self->mOwnerID);
	}
}

//static 
void LLFloaterObjectIMInfo::onClickMute(void* data)
{
	LLFloaterObjectIMInfo* self = (LLFloaterObjectIMInfo*)data;

	LLMute::EType mute_type = (self->mGroupOwned) ? LLMute::GROUP : LLMute::AGENT;
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	if ( (LLMute::GROUP != mute_type) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(self->mOwnerID)) )
	{
		return;
	}
// [/RLVa:KB]

	LLMute mute(self->mOwnerID, self->mName, mute_type);
	LLMuteList::getInstance()->add(mute);
	LLFloaterMute::showInstance();
	self->close();
}

//static 
void LLFloaterObjectIMInfo::nameCallback(const LLUUID& id, const std::string& full_name, bool is_group)
{
	mName = full_name;
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Added: RLVa-0.2.0g
	if ( (!is_group) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(id)) )
	{
		mName = RlvStrings::getAnonym(mName);
	}
// [/RLVa:KB]

	childSetText("OwnerName", mName);
}

////////////////////////////////////////////////////////////////////////////
// LLObjectIMInfoHandler
//moved to llchathistory.cpp in v2
class LLObjectIMInfoHandler : public LLCommandHandler
{
public:
	LLObjectIMInfoHandler() : LLCommandHandler("objectim", UNTRUSTED_THROTTLE) { }

	bool handle(const LLSD& params, const LLSD& query_map,LLMediaCtrl* web)
	{
		if (params.size() < 1)
		{
			return false;
		}

		LLUUID object_id;
		if (!object_id.set(params[0], FALSE))
		{
			return false;
		}

		LLSD payload;
		payload["object_id"] = object_id;
		payload["owner_id"] = query_map["owner"];
		payload["name"] = query_map["name"];
		payload["slurl"] = LLWeb::escapeURL(query_map["slurl"]);
		payload["group_owned"] = query_map["groupowned"];
	
		LLFloaterObjectIMInfo::showInstance()->update(payload);

		return true;
	}
};

// Creating the object registers with the dispatcher.
LLObjectIMInfoHandler gObjectIMHandler;
