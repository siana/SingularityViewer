/**
 * @file llgroupactions.cpp
 * @brief Group-related actions (join, leave, new, delete, etc)
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llgroupactions.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloaterchatterbox.h" // for LLFloaterMyFriends
#include "llfloatergroupinfo.h"
#include "llfloatersearch.h"
#include "llgroupmgr.h"
#include "llimview.h" // for gIMMgr
#include "llnotificationsutil.h"
#include "llpanelgroup.h"
#include "llviewermessage.h"
#include "groupchatlistener.h"
#include "hippolimits.h" // for getMaxAgentGroups
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.3.0f)
#include "llslurl.h"
#include "rlvhandler.h"
// [/RLVa:KB]

//
// Globals
//
static GroupChatListener sGroupChatListener;

class LLGroupHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLGroupHandler() : LLCommandHandler("group", UNTRUSTED_THROTTLE) { }
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		/*if (!LLUI::sSettingGroups["config"]->getBOOL("EnableGroupInfo"))
		{
			LLNotificationsUtil::add("NoGroupInfo", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
			return true;
		}*/

		if (tokens.size() < 1)
		{
			return false;
		}

		if (tokens[0].asString() == "create")
		{
			LLGroupActions::createGroup();
			return true;
		}

		if (tokens.size() < 2)
		{
			return false;
		}

		if (tokens[0].asString() == "list")
		{
			if (tokens[1].asString() == "show")
			{
				// CP_TODO: get the value we pass in via the XUI name
				// of the tab instead of using a literal like this
				LLFloaterMyFriends::showInstance( 1 );

				return true;
			}
            return false;
		}

		LLUUID group_id;
		if (!group_id.set(tokens[0], FALSE))
		{
			return false;
		}

		if (tokens[1].asString() == "about")
		{
			if (group_id.isNull())
				return true;

			LLGroupActions::show(group_id);

			return true;
		}
		if (tokens[1].asString() == "inspect")
		{
			if (group_id.isNull())
				return true;
			LLGroupActions::inspect(group_id);
			return true;
		}
		return false;
	}
};
LLGroupHandler gGroupHandler;

// static
void LLGroupActions::search()
{
	//LLFloaterReg::showInstance("search", LLSD().with("category", "groups"));
	LLFloaterSearch::SearchQuery search;
	search.category = "groups";
	LLFloaterSearch::showInstance(search);
}

// static
void LLGroupActions::startCall(const LLUUID& group_id)
{
	// create a new group voice session
	LLGroupData gdata;

	if (!gAgent.getGroupData(group_id, gdata))
	{
		llwarns << "Error getting group data" << llendl;
		return;
	}

// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(group_id)) && (!gIMMgr->hasSession(group_id)) )
	{
		make_ui_sound("UISndInvalidOp");
		RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
		return;
	}
// [/RLVa:KB]

	LLUUID session_id = gIMMgr->addSession(gdata.mName, IM_SESSION_GROUP_START, group_id, true);
	if (session_id.isNull())
	{
		llwarns << "Error adding session" << llendl;
		return;
	}

	// start the call
	gIMMgr->autoStartCallOnStartup(session_id);

	make_ui_sound("UISndStartIM");
}

// static
void LLGroupActions::join(const LLUUID& group_id)
{
	if (gAgent.mGroups.count() >= gHippoLimits->getMaxAgentGroups()) //!gAgent.canJoinGroups()
	{
		LLNotificationsUtil::add("JoinedTooManyGroups");
		return;
	}

	LLGroupMgrGroupData* gdatap =
		LLGroupMgr::getInstance()->getGroupData(group_id);

	if (gdatap)
	{
		S32 cost = gdatap->mMembershipFee;
		LLSD args;
		args["COST"] = llformat("%d", cost);
		args["NAME"] = gdatap->mName;
		LLSD payload;
		payload["group_id"] = group_id;

		if (can_afford_transaction(cost))
		{
			if(cost > 0)
				LLNotificationsUtil::add("JoinGroupCanAfford", args, payload, onJoinGroup);
			else
				LLNotificationsUtil::add("JoinGroupNoCost", args, payload, onJoinGroup);
		}
		else
		{
			LLNotificationsUtil::add("JoinGroupCannotAfford", args, payload);
		}
	}
	else
	{
		llwarns << "LLGroupMgr::getInstance()->getGroupData(" << group_id
			<< ") was NULL" << llendl;
	}
}

// static
bool LLGroupActions::onJoinGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (option == 1)
	{
		// user clicked cancel
		return false;
	}

	LLGroupMgr::getInstance()->
		sendGroupMemberJoin(notification["payload"]["group_id"].asUUID());
	return false;
}

// static
void LLGroupActions::leave(const LLUUID& group_id)
{
//	if (group_id.isNull())
//		return;
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.4.1a) | Added: RLVa-1.3.0f
	if ( (group_id.isNull()) || ((gAgent.getGroupID() == group_id) && (gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP))) )
		return;
// [/RLVa:KB]

	S32 count = gAgent.mGroups.count();
	S32 i;
	for (i = 0; i < count; ++i)
	{
		if(gAgent.mGroups.get(i).mID == group_id)
			break;
	}
	if (i < count)
	{
		LLSD args;
		args["GROUP"] = gAgent.mGroups.get(i).mName;
		LLSD payload;
		payload["group_id"] = group_id;
		LLNotificationsUtil::add("GroupLeaveConfirmMember", args, payload, onLeaveGroup);
	}
}

// static
void LLGroupActions::activate(const LLUUID& group_id)
{
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.4.1a) | Added: RLVa-1.3.0f
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP)) && (gRlvHandler.getAgentGroup() != group_id) )
	{
		return;
	}
// [/RLVa:KB]

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ActivateGroup);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	gAgent.sendReliableMessage();
}

/* Singu Note: this function serves no purpose to us, it is here as an honorable mention
static bool isGroupUIVisible()
{
	static LLPanel* panel = 0;
	if(!panel)
		panel = LLFloaterSidePanelContainer::getPanel("people", "panel_group_info_sidetray");
	if(!panel)
		return false;
	return panel->isInVisibleChain();
}
*/

// static
void LLGroupActions::inspect(const LLUUID& group_id)
{
	//LLFloaterReg::showInstance("inspect_group", LLSD().with("group_id", group_id));
	openGroupProfile(group_id);
}


// static
void LLGroupActions::show(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;

	/*
	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";

	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	*/
	openGroupProfile(group_id);
}

// static
void LLGroupActions::showTab(const LLUUID& group_id, const std::string& tab_name)
{
	if (group_id.isNull()) return;

	openGroupProfile(group_id)->selectTabByName(tab_name);
}

// static
void LLGroupActions::showNotice(const std::string& subj, const std::string& mes, const LLUUID& group_id, const bool& has_inventory, const std::string& item_name, LLOfferInfo* info)
{
	if (LLFloaterGroupInfo* fgi = LLFloaterGroupInfo::getInstance(group_id))
	{
		fgi->mPanelGroupp->showNotice(subj, mes, has_inventory, item_name, info);
	}
	else
	{
		// We need to clean up that inventory offer.
		if (info)
		{
			info->forceResponse(IOR_DECLINE);
		}
	}
}

/* Singu Note: How could this ever work with a null id, it's only used by llnotificationgrouphandler.cpp which we don't have
void LLGroupActions::refresh_notices()
{
	if(!isGroupUIVisible())
		return;

	LLSD params;
	params["group_id"] = LLUUID::null;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh_notices";

	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
}
*/

//static
void LLGroupActions::refresh(const LLUUID& group_id)
{
	/*
	if(!isGroupUIVisible())
		return;

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh";

	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	*/
	if (LLFloaterGroupInfo* fgi = LLFloaterGroupInfo::getInstance(group_id))
		if (LLPanelGroup* pg = fgi->mPanelGroupp)
			pg->refreshData();
}

//static
void LLGroupActions::createGroup()
{
	/*
	LLSD params;
	params["group_id"] = LLUUID::null;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "create";

	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	*/
	openGroupProfile(LLUUID::null);
}
//static
void LLGroupActions::closeGroup(const LLUUID& group_id)
{
	/*
	if(!isGroupUIVisible())
		return;

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "close";

	LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	*/
	if (LLFloaterGroupInfo* fgi = LLFloaterGroupInfo::getInstance(group_id))
		fgi->close();
}


// static
LLUUID LLGroupActions::startIM(const LLUUID& group_id)
{
	if (group_id.isNull()) return LLUUID::null;

// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	if ( (rlv_handler_t::isEnabled()) && (!gRlvHandler.canStartIM(group_id)) && (!gIMMgr->hasSession(group_id)) )
	{
		make_ui_sound("UISndInvalidOp");
		RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
		return LLUUID::null;
	}
// [/RLVa:KB]

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		static LLCachedControl<bool> tear_off("OtherChatsTornOff");
		if (!tear_off) gIMMgr->setFloaterOpen(TRUE);
		LLUUID session_id = gIMMgr->addSession(
			group_data.mName,
			IM_SESSION_GROUP_START,
			group_id);
		make_ui_sound("UISndStartIM");
		return session_id;
	}
	else
	{
		// this should never happen, as starting a group IM session
		// relies on you belonging to the group and hence having the group data
		make_ui_sound("UISndInvalidOp");
		return LLUUID::null;
	}
}

// static
void LLGroupActions::endIM(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;

	LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
	if (session_id.notNull())
	{
		gIMMgr->removeSession(session_id);
	}
}

// static
bool LLGroupActions::isInGroup(const LLUUID& group_id)
{
	// *TODO: Move all the LLAgent group stuff into another class, such as
	// this one.
	return gAgent.isInGroup(group_id);
}

// static
bool LLGroupActions::isAvatarMemberOfGroup(const LLUUID& group_id, const LLUUID& avatar_id)
{
	if(group_id.isNull() || avatar_id.isNull())
	{
		return false;
	}

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->getGroupData(group_id);
	if(!group_data)
	{
		return false;
	}

	if(group_data->mMembers.end() == group_data->mMembers.find(avatar_id))
	{
		return false;
	}

	return true;
}

//-- Private methods ----------------------------------------------------------

// static
bool LLGroupActions::onLeaveGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLUUID group_id = notification["payload"]["group_id"].asUUID();
	if(option == 0)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_LeaveGroupRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_GroupData);
		msg->addUUIDFast(_PREHASH_GroupID, group_id);
		gAgent.sendReliableMessage();
	}
	return false;
}

// Singu helper function, open a profile and center it
// static
LLFloaterGroupInfo* LLGroupActions::openGroupProfile(const LLUUID& group_id)
{
	LLFloaterGroupInfo* fgi = LLFloaterGroupInfo::getInstance(group_id);
	if (!fgi) fgi = new LLFloaterGroupInfo(group_id);
	fgi->center();
	fgi->open();
	return fgi;
}

