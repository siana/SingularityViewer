/** 
 * @file llviewermessage.cpp
 * @brief Dumping ground for viewer-side message system callbacks.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
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
#include "llviewermessage.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

#include "llanimationstates.h"
#include "llaudioengine.h" 
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llcororesponder.h"
#include "../lscript/lscript_byteformat.h"	//Need LSCRIPTRunTimePermissionBits and SCRIPT_PERMISSION_*
#include "llfocusmgr.h"
#include "llfollowcamparams.h"
#include "llinventorydefines.h"
#include "llregionhandle.h"
#include "llsdserialize.h"
#include "llteleportflags.h"
#include "lltransactionflags.h"
#include "llvfile.h"
#include "llvfs.h"
#include "llxfermanager.h"
#include "mean_collision_data.h"

#include "llagent.h"
#include "llagentbenefits.h"
#include "llagentcamera.h"
#include "llcallingcard.h"
#include "llcontrolavatar.h"
#include "llfirstuse.h"
#include "llfloaterbump.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterbuyland.h"
#include "llfloaterchat.h"
#include "llfloaterexperienceprofile.h"
#include "llfloaterland.h"
#include "llfloaterregioninfo.h"
#include "llfloaterlandholdings.h"
#include "llfloatermute.h"
#include "llfloaterpostcard.h"
#include "llfloaterpreference.h"
#include "llfloaterregionrestarting.h"
#include "llfloaterteleporthistory.h"
#include "llgroupactions.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llimprocessing.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "lllslconstants.h"
#include "llmarketplacefunctions.h"
#include "llmutelist.h"
#include "llnotify.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llpanelgrouplandmoney.h"
#include "llpanelmaininventory.h"
#include "llselectmgr.h"
#include "llstartup.h"
#include "llsky.h"
#include "llslurl.h"
#include "llstatenums.h"
#include "llstatusbar.h"
#include "llimview.h"
#include "llspeakers.h"
#include "lltrans.h"
#include "llviewerfoldertype.h"
#include "llviewermenu.h"
#include "llviewerinventory.h"
#include "llviewerjoystick.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvlmanager.h"
#include "llvoavatar.h"
#include "llworld.h"
#include "pipeline.h"
#include "llfloaterworldmap.h"
#include "llviewerdisplay.h"
#include "llkeythrottle.h"
#include "llagentui.h"
#include "llviewerregion.h"
#include  "llexperiencecache.h"

// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0a)
#include "rlvactions.h"
#include "rlvhandler.h"
#include "rlvinventory.h"
#include "rlvui.h"
// [/RLVa:KB]

#include <boost/regex.hpp>

//#include "llnotificationmanager.h" //
#include "llexperiencecache.h"

#if SHY_MOD //Command handler
# include "shcommandhandler.h"
#endif //shy_mod

#include "hippogridmanager.h"
#include "hippolimits.h"
#include "m7wlinterface.h"

#include "llgiveinventory.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/tokenizer.hpp>

#if LL_WINDOWS // For Windows specific error handler
#include "llwindebug.h"	// For the invalid message handler
#endif

// NaCl - Antispam Registry
#include "NACLantispam.h"
bool can_block(const LLUUID& id);
// NaCl - Newline flood protection
static const boost::regex NEWLINES("\\n{1}");
// NaCl End

//
// Constants
//
const F32 BIRD_AUDIBLE_RADIUS = 32.0f;
const F32 SIT_DISTANCE_FROM_TARGET = 0.25f;
const F32 CAMERA_POSITION_THRESHOLD_SQUARED = 0.001f * 0.001f;
static const F32 LOGOUT_REPLY_TIME = 3.f;	// Wait this long after LogoutReply before quitting.

// Determine how quickly residents' scripts can issue question dialogs
// Allow bursts of up to 5 dialogs in 10 seconds. 10*2=20 seconds recovery if throttle kicks in
static const U32 LLREQUEST_PERMISSION_THROTTLE_LIMIT	= 5;     // requests
static const F32 LLREQUEST_PERMISSION_THROTTLE_INTERVAL	= 10.0f; // seconds

extern BOOL gDebugClicks;
extern bool gShiftFrame;

// function prototypes
bool check_offer_throttle(const std::string& from_name, bool check_only);
bool check_asset_previewable(const LLAssetType::EType asset_type);
static void process_money_balance_reply_extended(LLMessageSystem* msg);
bool handle_trusted_experiences_notification(const LLSD&);

//inventory offer throttle globals
LLFrameTimer gThrottleTimer;
const U32 OFFER_THROTTLE_MAX_COUNT=5; //number of items per time period
const F32 OFFER_THROTTLE_TIME=10.f; //time period in seconds

//script permissions
const std::string SCRIPT_QUESTIONS[SCRIPT_PERMISSION_EOF] = 
{
	"ScriptTakeMoney",
	"ActOnControlInputs",
	"RemapControlInputs",
	"AnimateYourAvatar",
	"AttachToYourAvatar",
	"ReleaseOwnership",
	"LinkAndDelink",
	"AddAndRemoveJoints",
	"ChangePermissions",
	"TrackYourCamera",
	"ControlYourCamera",
	"TeleportYourAgent",
	"JoinAnExperience",
	"SilentlyManageEstateAccess",
	"OverrideYourAnimations",
	"ScriptReturnObjects"
};

constexpr bool SCRIPT_QUESTION_IS_CAUTION[SCRIPT_PERMISSION_EOF] = 
{
	true,	// ScriptTakeMoney,
	false,	// ActOnControlInputs
	false,	// RemapControlInputs
	false,	// AnimateYourAvatar
	false,	// AttachToYourAvatar
	false,	// ReleaseOwnership,
	false,	// LinkAndDelink,
	false,	// AddAndRemoveJoints
	false,	// ChangePermissions
	false,	// TrackYourCamera,
	false,	// ControlYourCamera
	false,	// TeleportYourAgent
	false,	// JoinAnExperience
	false,	// SilentlyManageEstateAccess
	false,	// OverrideYourAnimations
	false,	// ScriptReturnObjects
};

void accept_friendship_coro(const LLCoroResponder& responder, const LLSD& notification)
{
    const auto& status = responder.getStatus();

    if (!responder.isGoodStatus(status))
    {
        LL_WARNS("Friendship") << "HTTP status " << status << ": " << responder.getReason() <<
            ". friendship offer accept failed." << LL_ENDL;
    }
    else
    {
        {
            LLSD payload = notification["payload"];
            LL_DEBUGS("Friendship") << "Adding friend to list" << responder.getContent() << LL_ENDL;
            // add friend to recent people list
            //LLRecentPeople::instance().add(payload["from_id"]);

            LLNotificationsUtil::add("FriendshipAcceptedByMe",
                notification["substitutions"], payload);
        }
    }
}

void decline_friendship_coro(const LLCoroResponder& responder, const LLSD& notification, S32 option)
{
    const auto& status = responder.getStatus();

    if (!responder.isGoodStatus(status))
    {
        LL_WARNS("Friendship") << "HTTP status " << status << ": " << responder.getReason() <<
            ". friendship offer decline failed." << LL_ENDL;
    }
    else
    {
        {
			const auto& payload = notification["payload"];
            LL_DEBUGS("Friendship") << "Friendship declined" << responder.getContent() << LL_ENDL;
            if (option == 1)
            {
                LLNotificationsUtil::add("FriendshipDeclinedByMe",
                    notification["substitutions"], payload);
            }
            else if (option == 2)
            {
                // start IM session
                LLAvatarActions::startIM(payload["from_id"].asUUID());
            }
        }
    }
}

bool friendship_offer_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLMessageSystem* msg = gMessageSystem;
	const LLSD& payload = notification["payload"];

	switch(option)
	{
	case 0:
	{
		// accept
		LLAvatarTracker::formFriendship(payload["from_id"]);

		const LLUUID fid = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);

		// This will also trigger an onlinenotification if the user is online
            std::string url = gAgent.getRegionCapability("AcceptFriendship");
            LL_DEBUGS("Friendship") << "Cap string: " << url << LL_ENDL;
            if (!url.empty() && payload.has("online") && payload["online"].asBoolean() == false)
            {
                LL_DEBUGS("Friendship") << "Accepting friendship via capability" << LL_ENDL;
			    url += "?from=" + payload["from_id"].asString();
				url += "&agent_name=\"" + LLURI::escape(gAgentAvatarp->getFullname()) + '"';
                LLHTTPClient::post(url, LLSD(), new LLCoroResponder(
                    boost::bind(accept_friendship_coro, _1, notification)));
            }
            else if (payload.has("session_id") && payload["session_id"].asUUID().notNull())
            {
                LL_DEBUGS("Friendship") << "Accepting friendship via viewer message" << LL_ENDL;
				msg->newMessageFast(_PREHASH_AcceptFriendship);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_TransactionBlock);
				msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
				msg->nextBlockFast(_PREHASH_FolderData);
				msg->addUUIDFast(_PREHASH_FolderID, fid);
				msg->sendReliable(LLHost(payload["sender"].asString()));

                // add friend to recent people list
                //LLRecentPeople::instance().add(payload["from_id"]);
                LLNotificationsUtil::add("FriendshipAcceptedByMe",
                    notification["substitutions"], payload);
            }
            else
            {
                LL_WARNS("Friendship") << "Failed to accept friendship offer, neither capability nor transaction id are accessible" << LL_ENDL;
            }
		break;
	}
        case 1: // Decline
	    // fall-through
	    //case 3: // Send IM - decline and start IM session
		    {
		// decline
		// We no longer notify other viewers, but we DO still send
		// the rejection to the simulator to delete the pending userop.
                std::string url = gAgent.getRegionCapability("DeclineFriendship");
                LL_DEBUGS("Friendship") << "Cap string: " << url << LL_ENDL;
                if (!url.empty() && payload.has("online") && payload["online"].asBoolean() == false)
                {
                    LL_DEBUGS("Friendship") << "Declining friendship via capability" << LL_ENDL;
					url += "?from=" + payload["from_id"].asString();
                    LLHTTPClient::del(url, new LLCoroResponder(
                        boost::bind(decline_friendship_coro, _1, notification, option)));
                }
                else if (payload.has("session_id") && payload["session_id"].asUUID().notNull())
                {
                    LL_DEBUGS("Friendship") << "Declining friendship via viewer message" << LL_ENDL;
					msg->newMessageFast(_PREHASH_DeclineFriendship);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					msg->nextBlockFast(_PREHASH_TransactionBlock);
					msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
					msg->sendReliable(LLHost(payload["sender"].asString()));

                    if (option == 1) // due to fall-through
                    {
                        LLNotificationsUtil::add("FriendshipDeclinedByMe",
                            notification["substitutions"], payload);
                    }
                    /*else if (option == 3)
                    {
                        // start IM session
                        LLAvatarActions::startIM(payload["from_id"].asUUID());
                    }*/
                }
                else
                {
                    LL_WARNS("Friendship") << "Failed to decline friendship offer, neither capability nor transaction id are accessible" << LL_ENDL;
                }
	    }
	default:
		// close button probably, possibly timed out
		break;
	}

		// TODO: this set of calls has undesirable behavior under Windows OS (CHUI-985):
		// here appears three additional toasts instead one modified
		// need investigation and fix

		// LLNotificationFormPtr modified_form(new LLNotificationForm(*notification_ptr->getForm()));
		// modified_form->setElementEnabled("Accept", false);
		// modified_form->setElementEnabled("Decline", false);
		// notification_ptr->updateForm(modified_form);
// [SL:KB] - Patch: UI-Notifications | Checked: 2013-05-09 (Catznip-3.5)
//		// Assume that any offer notification with "getCanBeStored() == true" is the result of RLVa routing it to the notifcation syswell
//		/*const*/ LLNotificationsUI::LLScreenChannel* pChannel = LLNotificationsUI::LLChannelManager::instance().getNotificationScreenChannel();
//		/*const*/ LLNotificationsUI::LLToast* pToast = (pChannel) ? pChannel->getToastByNotificationID(notification["id"].asUUID()) : NULL;
//		if ( (!pToast) || (!pToast->getCanBeStored()) )
//		{
// [/SL:KB]
//			notification_ptr->repost();
// [SL:KB] - Patch: UI-Notifications | Checked: 2013-05-09 (Catznip-3.5)
//		}
// [/SL:KB]

	return false;
}

static LLNotificationFunctorRegistration friendship_offer_callback_reg("OfferFriendship", friendship_offer_callback);
static LLNotificationFunctorRegistration friendship_offer_callback_reg_nm("OfferFriendshipNoMessage", friendship_offer_callback);

// Functions
//

void give_money(const LLUUID& uuid, LLViewerRegion* region, S32 amount, BOOL is_group,
				S32 trx_type, const std::string& desc)
{
	if (0 == amount || !region) return;
	amount = abs(amount);
	LL_INFOS("Messaging") << "give_money(" << uuid << "," << amount << ")" << LL_ENDL;
	if (can_afford_transaction(amount))
	{
		//		gStatusBar->debitBalance(amount);
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_MoneyTransferRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_MoneyData);
		msg->addUUIDFast(_PREHASH_SourceID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_DestID, uuid);
		msg->addU8Fast(_PREHASH_Flags, pack_transaction_flags(FALSE, is_group));
		msg->addS32Fast(_PREHASH_Amount, amount);
		msg->addU8Fast(_PREHASH_AggregatePermNextOwner, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addU8Fast(_PREHASH_AggregatePermInventory, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addS32Fast(_PREHASH_TransactionType, trx_type);
		msg->addStringFast(_PREHASH_Description, desc);
		msg->sendReliable(region->getHost());
	}
	else
	{
		LLStringUtil::format_map_t args;
		args["CURRENCY"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
		LLFloaterBuyCurrency::buyCurrency( LLTrans::getString("giving", args)+" ", amount );
	}
}

void send_complete_agent_movement(const LLHost& sim_host)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_CompleteAgentMovement);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addU32Fast(_PREHASH_CircuitCode, msg->mOurCircuitCode);
	msg->sendReliable(sim_host);
}

void process_logout_reply(LLMessageSystem* msg, void**)
{
	// The server has told us it's ok to quit.
	LL_DEBUGS("Messaging") << "process_logout_reply" << LL_ENDL;

	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	LLUUID session_id;
	msg->getUUID("AgentData", "SessionID", session_id);
	if ((agent_id != gAgent.getID()) || (session_id != gAgent.getSessionID()))
	{
		LL_WARNS("Messaging") << "Bogus Logout Reply" << LL_ENDL;
	}

	LLInventoryModel::update_map_t parents;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_InventoryData);
	for (S32 i = 0; i < count; ++i)
	{
		LLUUID item_id;
		msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_ItemID, item_id, i);

		if ((1 == count) && item_id.isNull())
		{
			// Detect dummy item.  Indicates an empty list.
			break;
		}

		// We do not need to track the asset ids, just account for an
		// updated inventory version.
		LL_INFOS("Messaging") << "process_logout_reply itemID=" << item_id << LL_ENDL;
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item)
		{
			parents[item->getParentUUID()] = 0;
			gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		}
		else
		{
			LL_INFOS("Messaging") << "process_logout_reply item not found: " << item_id << LL_ENDL;
		}
	}
    LLAppViewer::instance()->forceQuit();
}

void process_layer_data(LLMessageSystem* mesgsys, void** user_data)
{
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(mesgsys->getSender());

	LL_DEBUGS_ONCE("SceneLoadTiming") << "Received layer data" << LL_ENDL;

	if (!regionp || gNoRender)
	{
		LL_WARNS() << "Invalid region for layer data." << LL_ENDL;
		return;
	}
	S32 size;
	S8 type;

	mesgsys->getS8Fast(_PREHASH_LayerID, _PREHASH_Type, type);
	size = mesgsys->getSizeFast(_PREHASH_LayerData, _PREHASH_Data);
	if (0 == size)
	{
		LL_WARNS("Messaging") << "Layer data has zero size." << LL_ENDL;
		return;
	}
	if (size < 0)
	{
		// getSizeFast() is probably trying to tell us about an error
		LL_WARNS("Messaging") << "getSizeFast() returned negative result: "
			<< size
			<< LL_ENDL;
		return;
	}
	U8* datap = new U8[size];
	mesgsys->getBinaryDataFast(_PREHASH_LayerData, _PREHASH_Data, datap, size);
	LLVLData* vl_datap = new LLVLData(regionp, type, datap, size);
	if (mesgsys->getReceiveCompressedSize())
	{
		gVLManager.addLayerData(vl_datap, mesgsys->getReceiveCompressedSize());
	}
	else
	{
		gVLManager.addLayerData(vl_datap, mesgsys->getReceiveSize());
	}
}

void process_derez_ack(LLMessageSystem*, void**)
{
	if (gViewerWindow) gViewerWindow->getWindow()->decBusyCount();
}

void process_places_reply(LLMessageSystem* msg, void** data)
{
	LLUUID query_id;

	msg->getUUID("AgentData", "QueryID", query_id);
	if (query_id.isNull())
	{
		LLFloaterLandHoldings::processPlacesReply(msg, data);
	}
	else if (gAgent.isInGroup(query_id))
	{
		LLPanelGroupLandMoney::processPlacesReply(msg, data);
	}
	else
	{
		LL_WARNS("Messaging") << "Got invalid PlacesReply message" << LL_ENDL;
	}
}

void send_sound_trigger(const LLUUID& sound_id, F32 gain)
{
	if (sound_id.isNull() || gAgent.getRegion() == nullptr)
	{
		// disconnected agent or zero guids don't get sent (no sound)
		return;
	}

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_SoundTrigger);
	msg->nextBlockFast(_PREHASH_SoundData);
	msg->addUUIDFast(_PREHASH_SoundID, sound_id);
	// Client untrusted, ids set on sim
	msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null);
	msg->addUUIDFast(_PREHASH_ObjectID, LLUUID::null);
	msg->addUUIDFast(_PREHASH_ParentID, LLUUID::null);

	msg->addU64Fast(_PREHASH_Handle, gAgent.getRegion()->getHandle());

	LLVector3 position = gAgent.getPositionAgent();
	msg->addVector3Fast(_PREHASH_Position, position);
	msg->addF32Fast(_PREHASH_Gain, gain);

	gAgent.sendMessage();
}

static LLSD sSavedGroupInvite;
static LLSD sSavedResponse;

void response_group_invitation_coro(const LLCoroResponder& responder, const LLUUID& group_id, bool notify_and_update)
{
    const auto& status = responder.getStatus();

    if (!responder.isGoodStatus(status))
    {
        LL_WARNS("GroupInvite") << "HTTP status " << status << ": " << responder.getReason() <<
            ". Group " << group_id << " invitation response processing failed." << LL_ENDL;
	}
	else
	{
        {
            LL_DEBUGS("GroupInvite") << "Successfully sent response to group " << group_id << " invitation" << LL_ENDL;
            if (notify_and_update)
            {
                LLNotificationsUtil::add("JoinGroupSuccess");
                gAgent.sendAgentDataUpdateRequest();

                LLGroupMgr::getInstance()->clearGroupData(group_id);
                // refresh the floater for this group, if any.
                LLGroupActions::refresh(group_id);
            }
        }
	}
}

void send_join_group_response(const LLUUID& group_id, const LLUUID& transaction_id, bool accept_invite, S32 fee, bool use_offline_cap, LLSD& payload)
{
    if (accept_invite && fee > 0)
	{
		// If there is a fee to join this group, make
		// sure the user is sure they want to join.
			LLSD args;
			args["COST"] = llformat("%d", fee);
			// Set the fee for next time to 0, so that we don't keep
			// asking about a fee.
            LLSD next_payload = payload;
			next_payload["fee"] = 0;
			LLNotificationsUtil::add("JoinGroupCanAfford",
				args,
                next_payload);
    }
    else if (use_offline_cap)
    {
        std::string url;
        if (accept_invite)
        {
            url = gAgent.getRegionCapability("AcceptGroupInvite");
		}
		else
		{
            url = gAgent.getRegionCapability("DeclineGroupInvite");
        }

        if (!url.empty())
        {
            LL_DEBUGS("GroupInvite") << "Capability url: " << url << LL_ENDL;
		    LLSD payload;
		    payload["group"] = group_id;
            LLHTTPClient::post(url, payload, new LLCoroResponder(
                boost::bind(response_group_invitation_coro, _1, group_id, accept_invite)));
        }
        else
        {
            // if sim has no this cap, we can do nothing - regular request will fail
            LL_WARNS("GroupInvite") << "No capability, can't reply to offline invitation!" << LL_ENDL;
        }
    }
    else
    {
        LL_DEBUGS("GroupInvite") << "Replying to group invite via IM message" << LL_ENDL;

        EInstantMessage type = accept_invite ? IM_GROUP_INVITATION_ACCEPT : IM_GROUP_INVITATION_DECLINE;

		send_improved_im(group_id,
			std::string("name"),
			std::string("message"),
			IM_ONLINE,
            type,
			transaction_id);
	}
}

void send_join_group_response(const LLUUID& group_id, const LLUUID& transaction_id, bool accept_invite, S32 fee, bool use_offline_cap)
{
    LLSD payload;
    if (accept_invite)
    {
        payload["group_id"] = group_id;
        payload["transaction_id"] =  transaction_id;
        payload["fee"] =  fee;
        payload["use_offline_cap"] = use_offline_cap;
    }
    send_join_group_response(group_id, transaction_id, accept_invite, fee, use_offline_cap, payload);
}

bool join_group_response(const LLSD& notification, const LLSD& response)
{
	//	A bit of variable saving and restoring is used to deal with the case where your group list is full and you
	//	receive an invitation to another group.  The data from that invitation is stored in the sSaved
	//	variables.  If you then drop a group and click on the Join button the stored data is restored and used
	//	to join the group.
	LLSD notification_adjusted = notification;
	LLSD response_adjusted = response;

	std::string action = notification["name"];

	//	Storing all the information by group id allows for the rare case of being at your maximum
	//	group count and receiving more than one invitation.
	std::string id = notification_adjusted["payload"]["group_id"].asString();

	if ("JoinGroup" == action || "JoinGroupCanAfford" == action)
	{
		sSavedGroupInvite[id] = notification;
		sSavedResponse[id] = response;
	}
	else if ("JoinedTooManyGroupsMember" == action)
	{
		S32 opt = LLNotificationsUtil::getSelectedOption(notification, response);
		if (0 == opt) // Join button pressed
		{
			notification_adjusted = sSavedGroupInvite[id];
			response_adjusted = sSavedResponse[id];
		}
		sSavedGroupInvite.erase(id);
		sSavedResponse.erase(id);
	}
	
	S32 option = LLNotificationsUtil::getSelectedOption(notification_adjusted, response_adjusted);
	bool accept_invite = false;

	LLUUID group_id = notification_adjusted["payload"]["group_id"].asUUID();
	LLUUID transaction_id = notification_adjusted["payload"]["transaction_id"].asUUID();
	std::string name = notification_adjusted["payload"]["name"].asString();
	std::string message = notification_adjusted["payload"]["message"].asString();
	S32 fee = notification_adjusted["payload"]["fee"].asInteger();
	U8 use_offline_cap = notification_adjusted["payload"]["use_offline_cap"].asInteger();

	if (option == 2 && !group_id.isNull())
	{
		LLGroupActions::show(group_id);
		LLSD args;
		args["MESSAGE"] = message;
		LLNotificationsUtil::add("JoinGroup", args, notification_adjusted["payload"]);
		return false;
	}

	if (option == 0 && !group_id.isNull())
	{
		// check for promotion or demotion.
		S32 max_groups = LLAgentBenefitsMgr::current().getGroupMembershipLimit();
		if (gAgent.isInGroup(group_id)) ++max_groups;

		if ((S32)gAgent.mGroups.size() < max_groups)
		{
			accept_invite = true;
		}
		else
		{
			LLSD args;
			args["NAME"] = name;
			args["INVITE"] = message;
			LLNotificationsUtil::add("JoinedTooManyGroupsMember", args, notification["payload"]);
			return false;
		}
	}
	send_join_group_response(group_id, transaction_id, accept_invite, fee, use_offline_cap, notification_adjusted["payload"]);

	sSavedGroupInvite[id] = LLSD::emptyMap();
	sSavedResponse[id] = LLSD::emptyMap();

	return false;
}

static void highlight_inventory_objects_in_panel(const uuid_vec_t& items, LLInventoryPanel *inventory_panel)
{
	if (nullptr == inventory_panel) return;

	for (auto item_id : items)
	{
        if (!highlight_offered_object(item_id))
		{
			continue;
		}

		LLInventoryObject* item = gInventory.getObject(item_id);
		llassert(item);
		if (!item)
		{
			continue;
		}

		LL_DEBUGS("Inventory_Move") << "Highlighting inventory item: " << item->getName() << ", " << item_id  << LL_ENDL;
		LLFolderView* fv = inventory_panel->getRootFolder();
		if (fv)
		{
			LLFolderViewItem* fv_item = fv->getItemByID(item_id);
			if (fv_item)
			{
				LLFolderViewItem* fv_folder = fv_item->getParentFolder();
				if (fv_folder)
				{
					// Parent folders can be different in case of 2 consecutive drag and drop
					// operations when the second one is started before the first one completes.
					LL_DEBUGS("Inventory_Move") << "Open folder: " << fv_folder->getName() << LL_ENDL;
					fv_folder->setOpen(TRUE);
					if (fv_folder->isSelected())
					{
						fv->changeSelection(fv_folder, FALSE);
					}
				}
				fv->changeSelection(fv_item, TRUE);
			}
		}
	}
}

static LLNotificationFunctorRegistration jgr_1("JoinGroup", join_group_response);
static LLNotificationFunctorRegistration jgr_2("JoinedTooManyGroupsMember", join_group_response);
static LLNotificationFunctorRegistration jgr_3("JoinGroupCanAfford", join_group_response);


//-----------------------------------------------------------------------------
// Instant Message
//-----------------------------------------------------------------------------
class LLOpenAgentOffer final : public LLInventoryFetchItemsObserver
{
public:
	LLOpenAgentOffer(const LLUUID& object_id,
					 const std::string& from_name) : 
		LLInventoryFetchItemsObserver(object_id),
		mFromName(from_name)
	{
	}

	/*virtual*/
	void startFetch() override
	{
		for (uuid_vec_t::const_iterator it = mIDs.begin(); it < mIDs.end(); ++it)
		{
			LLViewerInventoryCategory* cat = gInventory.getCategory(*it);
			if (cat)
			{
				mComplete.push_back((*it));
			}
		}
		LLInventoryFetchItemsObserver::startFetch();
	}

	/*virtual*/
	void done() override
	{
		open_inventory_offer(mComplete, mFromName);
		gInventory.removeObserver(this);
		delete this;
	}

private:
	std::string mFromName;
};

/**
 * Class to observe adding of new items moved from the world to user's inventory to select them in inventory.
 *
 * We can't create it each time items are moved because "drop" event is sent separately for each
 * element even while multi-dragging. We have to have the only instance of the observer. See EXT-4347.
 */
class LLViewerInventoryMoveFromWorldObserver final : public LLInventoryAddItemByAssetObserver
{
public:
	LLViewerInventoryMoveFromWorldObserver()
		: LLInventoryAddItemByAssetObserver()
	{
	}

	void setMoveIntoFolderID(const LLUUID& into_folder_uuid) { mMoveIntoFolderID = into_folder_uuid; }

private:
	/*virtual */
	void onAssetAdded(const LLUUID& asset_id) override
	{
		// Store active Inventory panel.
		if (LLInventoryPanel::getActiveInventoryPanel())
		{
			mActivePanel = LLInventoryPanel::getActiveInventoryPanel()->getHandle();
		}

		// Store selected items (without destination folder)
		mSelectedItems.clear();
		if (LLInventoryPanel::getActiveInventoryPanel())
		{
			mSelectedItems = LLInventoryPanel::getActiveInventoryPanel()->getRootFolder()->getSelectionList();
		}
		mSelectedItems.erase(mMoveIntoFolderID);
	}

	/**
	 * Selects added inventory items watched by their Asset UUIDs if selection was not changed since
	 * all items were started to watch (dropped into a folder).
	 */
	void done() override
	{
		LLInventoryPanel* active_panel = dynamic_cast<LLInventoryPanel*>(mActivePanel.get());

		// if selection is not changed since watch started lets hightlight new items.
		if (active_panel && !isSelectionChanged())
		{
			LL_DEBUGS("Inventory_Move") << "Selecting new items..." << LL_ENDL;
			active_panel->clearSelection();
			highlight_inventory_objects_in_panel(mAddedItems, active_panel);
		}
	}

	/**
	 * Returns true if selected inventory items were changed since moved inventory items were started to watch.
	 */
	bool isSelectionChanged()
	{	
		LLInventoryPanel* active_panel = dynamic_cast<LLInventoryPanel*>(mActivePanel.get());

		if (nullptr == active_panel)
		{
			return true;
		}

		// get selected items (without destination folder)
		selected_items_t selected_items = active_panel->getRootFolder()->getSelectionList();
		selected_items.erase(mMoveIntoFolderID);

		// compare stored & current sets of selected items
		selected_items_t different_items;
		std::set_symmetric_difference(mSelectedItems.begin(), mSelectedItems.end(),
			selected_items.begin(), selected_items.end(), std::inserter(different_items, different_items.begin()));

		LL_DEBUGS("Inventory_Move") << "Selected firstly: " << mSelectedItems.size()
			<< ", now: " << selected_items.size() << ", difference: " << different_items.size() << LL_ENDL;

		return !different_items.empty();
	}

	LLHandle<LLPanel> mActivePanel;
	typedef uuid_set_t selected_items_t;
	selected_items_t mSelectedItems;

	/**
	 * UUID of FolderViewFolder into which watched items are moved.
	 *
	 * Destination FolderViewFolder becomes selected while mouse hovering (when dragged items are dropped).
	 * 
	 * If mouse is moved out it set unselected and number of selected items is changed 
	 * even if selected items in Inventory stay the same.
	 * So, it is used to update stored selection list.
	 *
	 * @see onAssetAdded()
	 * @see isSelectionChanged()
	 */
	LLUUID mMoveIntoFolderID;
};

LLViewerInventoryMoveFromWorldObserver* gInventoryMoveObserver = nullptr;

void set_dad_inventory_item(LLInventoryItem* inv_item, const LLUUID& into_folder_uuid)
{
	start_new_inventory_observer();

	gInventoryMoveObserver->setMoveIntoFolderID(into_folder_uuid);
	gInventoryMoveObserver->watchAsset(inv_item->getAssetUUID());
}


/**
 * Class to observe moving of items and to select them in inventory.
 *
 * Used currently for dragging from inbox to regular inventory folders
 */

class LLViewerInventoryMoveObserver final : public LLInventoryObserver
{
public:

	LLViewerInventoryMoveObserver(const LLUUID& object_id)
		: LLInventoryObserver()
		, mObjectID(object_id)
	{
		if (LLInventoryPanel::getActiveInventoryPanel())
		{
			mActivePanel = LLInventoryPanel::getActiveInventoryPanel()->getHandle();
		}
	}

	virtual ~LLViewerInventoryMoveObserver() = default;

	void changed(U32 mask) override;
	
private:
	LLUUID mObjectID;
	LLHandle<LLPanel> mActivePanel;
};

void LLViewerInventoryMoveObserver::changed(U32 mask)
{
	LLInventoryPanel* active_panel = dynamic_cast<LLInventoryPanel*>(mActivePanel.get());

	if (nullptr == active_panel)
	{
		gInventory.removeObserver(this);
		return;
	}

	if ((mask & (STRUCTURE)) != 0)
	{
		const uuid_set_t& changed_items = gInventory.getChangedIDs();

		auto id_it = changed_items.begin();
		auto id_end = changed_items.end();
		for (;id_it != id_end; ++id_it)
		{
			if ((*id_it) == mObjectID)
			{
				active_panel->clearSelection();			
				highlight_inventory_objects_in_panel({mObjectID}, active_panel);
				active_panel->getRootFolder()->scrollToShowSelection();
				
				gInventory.removeObserver(this);
				break;
			}
		}
	}
}

void set_dad_inbox_object(const LLUUID& object_id)
{
	LLViewerInventoryMoveObserver* move_observer = new LLViewerInventoryMoveObserver(object_id);
	gInventory.addObserver(move_observer);
}

//unlike the FetchObserver for AgentOffer, we only make one 
//instance of the AddedObserver for TaskOffers
//and it never dies.  We do this because we don't know the UUID of 
//task offers until they are accepted, so we don't wouldn't 
//know what to watch for, so instead we just watch for all additions.
class LLOpenTaskOffer final : public LLInventoryAddedObserver
{
protected:
	/*virtual*/
	void done() override
	{
		uuid_vec_t added;
		for (auto it : gInventory.getAddedIDs())
		{
			added.push_back(it);
		}
		for (uuid_vec_t::iterator it = added.begin(); it != added.end();)
		{
			const LLUUID& item_uuid = *it;
			bool was_moved = false;
			LLInventoryObject* added_object = gInventory.getObject(item_uuid);
			if (added_object)
			{
				// cast to item to get Asset UUID
				LLInventoryItem* added_item = dynamic_cast<LLInventoryItem*>(added_object);
				if (added_item)
				{
					const LLUUID& asset_uuid = added_item->getAssetUUID();
					if (gInventoryMoveObserver->isAssetWatched(asset_uuid))
					{
						LL_DEBUGS("Inventory_Move") << "Found asset UUID: " << asset_uuid << LL_ENDL;
						was_moved = true;
					}
				}
			}

			if (was_moved)
			{
				it = added.erase(it);
			}
			else ++it;
		}

		open_inventory_offer(added, "");
	}
};

class LLOpenTaskGroupOffer final : public LLInventoryAddedObserver
{
protected:
	/*virtual*/
	void done() override
	{
		uuid_vec_t added;
		for (auto it : gInventory.getAddedIDs())
		{
			added.push_back(it);
		}
		open_inventory_offer(added, "group_offer");
		gInventory.removeObserver(this);
		delete this;
	}
};

//one global instance to bind them
LLOpenTaskOffer* gNewInventoryObserver = nullptr;

class LLNewInventoryHintObserver final : public LLInventoryAddedObserver
{
protected:
	/*virtual*/
	void done() override
	{
		//LLFirstUse::newInventory();
	}
};

LLNewInventoryHintObserver* gNewInventoryHintObserver = nullptr;

void start_new_inventory_observer()
{
	if (!gNewInventoryObserver) //task offer observer 
	{
		// Observer is deleted by gInventory
		gNewInventoryObserver = new LLOpenTaskOffer;
		gInventory.addObserver(gNewInventoryObserver);
	}

	if (!gInventoryMoveObserver) //inventory move from the world observer 
	{
		// Observer is deleted by gInventory
		gInventoryMoveObserver = new LLViewerInventoryMoveFromWorldObserver;
		gInventory.addObserver(gInventoryMoveObserver);
	}

	if (!gNewInventoryHintObserver)
	{
		// Observer is deleted by gInventory
		gNewInventoryHintObserver = new LLNewInventoryHintObserver();
		gInventory.addObserver(gNewInventoryHintObserver);
	}
}

class LLDiscardAgentOffer final : public LLInventoryFetchItemsObserver
{
	LOG_CLASS(LLDiscardAgentOffer);

public:
	LLDiscardAgentOffer(const LLUUID& folder_id, const LLUUID& object_id) :
		LLInventoryFetchItemsObserver(object_id),
		mFolderID(folder_id),
		mObjectID(object_id)
	{
	}

	void done() override
	{
		LL_DEBUGS("Messaging") << "LLDiscardAgentOffer::done()" << LL_ENDL;

		// We're invoked from LLInventoryModel::notifyObservers().
		// If we now try to remove the inventory item, it will cause a nested
		// notifyObservers() call, which won't work.
		// So defer moving the item to trash until viewer gets idle (in a moment).
		// Use removeObject() rather than removeItem() because at this level,
		// the object could be either an item or a folder.
		LLAppViewer::instance()->addOnIdleCallback(std::bind(&LLInventoryModel::removeObject, &gInventory, mObjectID));
		gInventory.removeObserver(this);
		delete this;
	}

protected:
	LLUUID mFolderID;
	LLUUID mObjectID;
};


//Returns TRUE if we are OK, FALSE if we are throttled
//Set check_only true if you want to know the throttle status 
//without registering a hit
bool check_offer_throttle(const std::string& from_name, bool check_only)
{
	static U32 throttle_count;
	static bool throttle_logged;
	LLChat chat;

	if (!gSavedSettings.getBOOL("ShowNewInventory"))
		return false;

	if (check_only)
	{
		return gThrottleTimer.hasExpired();
	}
	
	if (gThrottleTimer.checkExpirationAndReset(OFFER_THROTTLE_TIME))
	{
		LL_DEBUGS("Messaging") << "Throttle Expired" << LL_ENDL;
		throttle_count = 1;
		throttle_logged = false;
		return true;
	}
	//has not expired
	LL_DEBUGS("Messaging") << "Throttle Not Expired, Count: " << throttle_count << LL_ENDL;
	// When downloading the initial inventory we get a lot of new items
	// coming in and can't tell that from spam.
	if (LLStartUp::getStartupState() >= STATE_STARTED
		&& throttle_count >= OFFER_THROTTLE_MAX_COUNT)
	{
		if (!throttle_logged)
		{
			// Use the name of the last item giver, who is probably the person
			// spamming you.

			LLStringUtil::format_map_t arg;
			std::string log_msg;
			std::ostringstream time;
			time << OFFER_THROTTLE_TIME;

			arg["APP_NAME"] = LLAppViewer::instance()->getSecondLifeTitle();
			arg["TIME"] = time.str();

			if (!from_name.empty())
			{
				arg["FROM_NAME"] = from_name;
				log_msg = LLTrans::getString("ItemsComingInTooFastFrom", arg);
			}
			else
			{
				log_msg = LLTrans::getString("ItemsComingInTooFast", arg);
			}

			//this is kinda important, so actually put it on screen
			chat.mText = log_msg;
			LLFloaterChat::addChat(chat, FALSE, FALSE);

			throttle_logged = true;
		}
		return false;
	}
	throttle_count++;
	return true;
}

// Return "true" if we have a preview method for that asset type, "false" otherwise
bool check_asset_previewable(const LLAssetType::EType asset_type)
{
	return	(asset_type == LLAssetType::AT_NOTECARD)  ||
			(asset_type == LLAssetType::AT_LANDMARK)  ||
			(asset_type == LLAssetType::AT_TEXTURE)   ||
			(asset_type == LLAssetType::AT_ANIMATION) ||
			(asset_type == LLAssetType::AT_SCRIPT)    ||
			(asset_type == LLAssetType::AT_SOUND);
}

void open_inventory_offer(const uuid_vec_t& objects, const std::string& from_name)
{
	if (gAgent.isDoNotDisturb()) return;
	for (auto obj_id : objects)
	{
        if (!highlight_offered_object(obj_id))
		{
			continue;
		}

		const LLInventoryObject* obj = gInventory.getObject(obj_id);
		if (!obj)
		{
			LL_WARNS() << "Cannot find object [ itemID:" << obj_id << " ] to open." << LL_ENDL;
			continue;
		}

		const LLAssetType::EType asset_type = obj->getActualType();

		// Either an inventory item or a category.
		const LLInventoryItem* item = dynamic_cast<const LLInventoryItem*>(obj);
		if (item && check_asset_previewable(asset_type))
		{
			////////////////////////////////////////////////////////////////////////////////
			// Special handling for various types.
			if (check_offer_throttle(from_name, false)) // If we are throttled, don't display
			{
				// I'm not sure this is a good idea - Definitely a bad idea. HB
				//bool show_keep_discard = item->getPermissions().getCreator() != gAgent.getID();
				bool show_keep_discard = true;
				switch(asset_type)
				{
					case LLAssetType::AT_NOTECARD:
					{
						open_notecard((LLViewerInventoryItem*)item, std::string("Note: ") + item->getName(), LLUUID::null, show_keep_discard, LLUUID::null, FALSE);
						break;
					}
					case LLAssetType::AT_LANDMARK:
					{
						open_landmark((LLViewerInventoryItem*)item, std::string("Landmark: ") + item->getName(), show_keep_discard, LLUUID::null, FALSE);
					}
					break;
					case LLAssetType::AT_TEXTURE:
					{
						open_texture(obj_id, std::string("Texture: ") + item->getName(), show_keep_discard, LLUUID::null, FALSE);
						break;
					}
					case LLAssetType::AT_ANIMATION:
					case LLAssetType::AT_SCRIPT:
					case LLAssetType::AT_SOUND:
						LLInvFVBridgeAction::doAction(asset_type, obj_id, &gInventory);
						break;
					default:
						LL_DEBUGS("Messaging") << "No preview method for previewable asset type : " << LLAssetType::lookupHumanReadable(asset_type)  << LL_ENDL;
						break;
				}
			}
		}

		//highlight item, if it's not in the trash or lost+found
		
		// Don't auto-open the inventory floater
		LLPanelMainInventory* view = LLPanelMainInventory::getActiveInventory();
		if(!view)
		{
			return;
		}

		//Trash Check
		if ((gInventory.isObjectDescendentOf(obj_id, gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH)))
		// don't select lost and found items if the user is active
		|| (gAwayTimer.getStarted() && gInventory.isObjectDescendentOf(obj_id, gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND))))
		{
			return;
		}

		if (gSavedSettings.getBOOL("ShowInInventory") &&
		   objects.size() == 1 && item != NULL &&
		   asset_type != LLAssetType::AT_CALLINGCARD &&
		   item->getInventoryType() != LLInventoryType::EType::IT_ATTACHMENT &&
		   !from_name.empty())
		{
			LLPanelMainInventory::showAgentInventory(TRUE);
		}

		if (!gSavedSettings.getBOOL("LiruHighlightNewInventory")) return;
		////////////////////////////////////////////////////////////////////////////////
		// Highlight item
		LL_DEBUGS("Messaging") << "Highlighting" << obj_id  << LL_ENDL;

		LLFocusableElement* focus_ctrl = gFocusMgr.getKeyboardFocus();
		view->getPanel()->setSelection(obj_id, TAKE_FOCUS_NO);
		gFocusMgr.setKeyboardFocus(focus_ctrl);
	}
}

bool highlight_offered_object(const LLUUID& obj_id)
{
	const LLInventoryObject* obj = gInventory.getObject(obj_id);
	if (!obj)
	{
		LL_WARNS("Messaging") << "Unable to show inventory item: " << obj_id << LL_ENDL;
		return false;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Don't highlight if it's in certain "quiet" folders which don't need UI
	// notification (e.g. trash, cof, lost-and-found).
	if (!gAgent.getAFK())
	{
		const LLViewerInventoryCategory* parent = gInventory.getFirstNondefaultParent(obj_id);
		if (parent)
		{
			const LLFolderType::EType parent_type = parent->getPreferredType();
			if (LLViewerFolderType::lookupIsQuietType(parent_type))
			{
				return false;
			}
		}
	}

	return true;
}

void inventory_offer_mute_callback(const LLUUID& blocked_id,
								   const std::string& full_name,
								   bool is_group)
{
	// *NOTE: blocks owner if the offer came from an object
	LLMute::EType mute_type = is_group ? LLMute::GROUP : LLMute::AGENT;

	LLMute mute(blocked_id, full_name, mute_type);
	if (LLMuteList::getInstance()->add(mute))
	{
		LLFloaterMute::showInstance()->selectMute(blocked_id);
	}

	// purge the message queue of any previously queued inventory offers from the same source.
	class OfferMatcher final : public LLNotifyBoxView::Matcher
	{
	public:
		OfferMatcher(const LLUUID& to_block) : blocked_id(to_block)
		{
		}

		bool matches(const LLNotificationPtr notification) const override
		{
			if(notification->getName() == "ObjectGiveItem" 
				|| notification->getName() == "ObjectGiveItemUnknownUser"
				|| notification->getName() == "UserGiveItem")
			{
				return (notification->getPayload()["from_id"].asUUID() == blocked_id);
			}
			return FALSE;
		}

	private:
		const LLUUID& blocked_id;
	};
	gNotifyBoxView->purgeMessagesMatching(OfferMatcher(blocked_id));
}

LLOfferInfo::LLOfferInfo()
 : mFromGroup(FALSE)
 , mFromObject(FALSE)
 , mIM(IM_NOTHING_SPECIAL)
 , mType(LLAssetType::AT_NONE)
{
}

LLOfferInfo::LLOfferInfo(const LLSD& sd)
{
	mIM = (EInstantMessage)sd["im_type"].asInteger();
	mFromID = sd["from_id"].asUUID();
	mFromGroup = sd["from_group"].asBoolean();
	mFromObject = sd["from_object"].asBoolean();
	mTransactionID = sd["transaction_id"].asUUID();
	mFolderID = sd["folder_id"].asUUID();
	mObjectID = sd["object_id"].asUUID();
	mType = LLAssetType::lookup(sd["type"].asString().c_str());
	mFromName = sd["from_name"].asString();
	mDesc = sd["description"].asString();
	mHost = LLHost(sd["sender"].asString());
}

LLOfferInfo::LLOfferInfo(const LLOfferInfo& info)
{
	mIM = info.mIM;
	mFromID = info.mFromID;
	mFromGroup = info.mFromGroup;
	mFromObject = info.mFromObject;
	mTransactionID = info.mTransactionID;
	mFolderID = info.mFolderID;
	mObjectID = info.mObjectID;
	mType = info.mType;
	mFromName = info.mFromName;
	mDesc = info.mDesc;
	mHost = info.mHost;
}

LLSD LLOfferInfo::asLLSD()
{
	LLSD sd;
	sd["im_type"] = mIM;
	sd["from_id"] = mFromID;
	sd["from_group"] = mFromGroup;
	sd["from_object"] = mFromObject;
	sd["transaction_id"] = mTransactionID;
	sd["folder_id"] = mFolderID;
	sd["object_id"] = mObjectID;
	sd["type"] = LLAssetType::lookup(mType);
	sd["from_name"] = mFromName;
	sd["description"] = mDesc;
	sd["sender"] = mHost.getIPandPort();
	return sd;
}

bool LLOfferInfo::inventory_offer_callback(const LLSD& notification, const LLSD& response)
{
	LLChat chat;
	std::string log_message;
	S32 button = LLNotificationsUtil::getSelectedOption(notification, response);
	if (button == 4) // profile
	{
		LLAvatarActions::showProfile(mFromID);
		LLNotification::Params p(notification["name"]);
		p.substitutions(notification["substitutions"]).payload(notification["payload"]).functor(boost::bind(&LLOfferInfo::inventory_offer_callback, this, _1, _2));
		LLNotifications::instance().add(p); //Respawn!
		return false;
	}

	chat.mSourceType = CHAT_SOURCE_SYSTEM; // There's a slim potential of a user-editable field being a url here?

	LLViewerInventoryCategory* catp = NULL;
	catp = gInventory.getCategory(mObjectID);
	LLViewerInventoryItem* itemp = NULL;
	if(!catp)
	{
		itemp = (LLViewerInventoryItem*)gInventory.getItem(mObjectID);
	}

	// For muting, we need to add the mute, then decline the offer.
	// This must be done here because:
	// * callback may be called immediately,
	// * adding the mute sends a message,
	// * we can't build two messages at once.
	if (2 == button) // Block
	{
		gCacheName->get(mFromID, mFromGroup, boost::bind(&inventory_offer_mute_callback,_1,_2,_3));
	}

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBOOLFast(_PREHASH_FromGroup, FALSE);
	msg->addUUIDFast(_PREHASH_ToAgentID, mFromID);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addUUIDFast(_PREHASH_ID, mTransactionID);
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // no timestamp necessary
	std::string name;
	LLAgentUI::buildFullname(name);
	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, ""); 
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());
	LLInventoryObserver* opener = NULL;

	std::string from_string; // Used in the pop-up.
	std::string chatHistory_string;  // Used in chat history.
	if (mFromObject == TRUE)
	{
		if (mFromGroup)
		{
			std::string group_name = LLGroupActions::getSLURL(mFromID);
			//if (gCacheName->getGroupName(mFromID, group_name))
			{
				from_string = LLTrans::getString("InvOfferAnObjectNamed") + " " + LLTrans::getString("'")
				+ mFromName + LLTrans::getString("'") + " " + LLTrans::getString("InvOfferOwnedByGroup")
				+ " " + LLTrans::getString("'") + group_name + LLTrans::getString("'");

				chatHistory_string = mFromName + " " + LLTrans::getString("InvOfferOwnedByGroup")
				+ " " + group_name + LLTrans::getString("'") + LLTrans::getString(".");
			}
			/*else
			{
				from_string = LLTrans::getString("InvOfferAnObjectNamed") + " " + LLTrans::getString("'")
				+ mFromName + LLTrans::getString("'") + " " + LLTrans::getString("InvOfferOwnedByUnknownGroup");
				chatHistory_string = mFromName + " " + LLTrans::getString("InvOfferOwnedByUnknownGroup") + LLTrans::getString(".");
			}*/
		}
		else
		{
			std::string full_name = LLAvatarActions::getSLURL(mFromID);

// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(mFromID)) )
			{
				if (gCacheName->getFullName(mFromID, full_name))
				{
					full_name = RlvStrings::getAnonym(full_name);
				}
			}

			from_string = LLTrans::getString("InvOfferAnObjectNamed") + " " + LLTrans::getString("'") + mFromName + LLTrans::getString("'") + ' ';
			chatHistory_string = mFromName + ' ';
			if (full_name.empty())
			{
				from_string += LLTrans::getString("InvOfferOwnedByUnknownUser");
				chatHistory_string += LLTrans::getString("InvOfferOwnedByUnknownUser") + LLTrans::getString(".");
			}
			else
// [/RLVa:KB]
			{
				from_string += LLTrans::getString("InvOfferOwnedBy") + full_name;
				chatHistory_string += LLTrans::getString("InvOfferOwnedBy") + " " + full_name + LLTrans::getString(".");
			}
		}
	}
	else
	{
		from_string = chatHistory_string = mFromName;
	}
	
	bool busy = gAgent.isDoNotDisturb();
	
// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1)
	bool fRlvNotifyAccepted = false;
// [/RLVa:KB]
	switch(button)
	{
	case IOR_ACCEPT:
		// ACCEPT. The math for the dialog works, because the accept
		// for inventory_offered, task_inventory_offer or
		// group_notice_inventory is 1 greater than the offer integer value.

// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1e) | Modified: RLVa-1.2.1e
		// Only treat the offer as 'Give to #RLV' if:
		//   - the user has enabled the feature
		//   - the inventory offer came from a script (and specifies a folder)
		//   - the name starts with the prefix - mDesc format: '[OBJECTNAME]'  ( http://slurl.com/... )
		if ( (rlv_handler_t::isEnabled()) && (IM_TASK_INVENTORY_OFFERED == mIM) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 1) )
		{
			fRlvNotifyAccepted = true;
			if (!RlvSettings::getForbidGiveToRLV())
			{
				const LLUUID& idRlvRoot = RlvInventory::instance().getSharedRootID();
				if (idRlvRoot.notNull())
					mFolderID = idRlvRoot;

				fRlvNotifyAccepted = false;		// "accepted_in_rlv" is sent from RlvGiveToRLVTaskOffer *after* we have the folder

				RlvGiveToRLVTaskOffer* pOfferObserver = new RlvGiveToRLVTaskOffer(mTransactionID);
				gInventory.addObserver(pOfferObserver);
			}
		}
// [/RLVa:KB]

		// Generates IM_INVENTORY_ACCEPTED, IM_TASK_INVENTORY_ACCEPTED, 
		// or IM_GROUP_NOTICE_INVENTORY_ACCEPTED
		msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 1));
		msg->addBinaryDataFast(_PREHASH_BinaryBucket, &(mFolderID.mData),
					 sizeof(mFolderID.mData));
		// send the message
		msg->sendReliable(mHost);
			
// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1)
			if (fRlvNotifyAccepted)
			{
				std::string::size_type idxToken = mDesc.find("'  ( http://");
				if (std::string::npos != idxToken)
					RlvBehaviourNotifyHandler::sendNotification("accepted_in_inv inv_offer " + mDesc.substr(1, idxToken - 1));
			}
// [/RLVa:KB]

		//don't spam them if they are getting flooded
		if (check_offer_throttle(mFromName, true))
		{
			log_message = chatHistory_string + ' ' + LLTrans::getString("InvOfferGaveYou") + ' ' + mDesc + LLTrans::getString(".");
			chat.mText = log_message;
			if (mFromObject || !mFromGroup)
				chat.mURL = mFromGroup ? LLGroupActions::getSLURL(mFromID) : LLAvatarActions::getSLURL(mFromID);
			chat.mFromName = mFromName;
			LLFloaterChat::addChatHistory(chat);
		}

		// we will want to open this item when it comes back.
		LL_DEBUGS("Messaging") << "Initializing an opener for tid: " << mTransactionID
				 << LL_ENDL;
		switch (mIM)
		{
		case IM_INVENTORY_OFFERED:
		{
			// This is an offer from an agent. In this case, the back
			// end has already copied the items into your inventory,
			// so we can fetch it out of our inventory.
// [RLVa:KB] - Checked: 2010-04-18 (RLVa-1.2.0)
			if ( (rlv_handler_t::isEnabled()) && (!RlvSettings::getForbidGiveToRLV()) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 0) )
			{
				RlvGiveToRLVAgentOffer* pOfferObserver = new RlvGiveToRLVAgentOffer(mObjectID);
				pOfferObserver->startFetch();
				if (pOfferObserver->isFinished())
					pOfferObserver->done();
				else
					gInventory.addObserver(pOfferObserver);
			}
// [/RLVa:KB]

			LLOpenAgentOffer* open_agent_offer = new LLOpenAgentOffer(mObjectID, from_string);
			open_agent_offer->startFetch();
			if(catp || (itemp && itemp->isFinished()))
			{
				open_agent_offer->done();
			}
			else
			{
				opener = open_agent_offer;
			}
		}
			break;
		case IM_TASK_INVENTORY_OFFERED:
		case IM_GROUP_NOTICE:
		case IM_GROUP_NOTICE_REQUESTED:
		{
			// This is an offer from a task or group.
			// We don't use a new instance of an opener
			// We instead use the singular observer gOpenTaskOffer
			// Since it already exists, we don't need to actually do anything
		}
		break;
		default:
			LL_WARNS("Messaging") << "inventory_offer_callback: unknown offer type" << LL_ENDL;
			break;
		}	// end switch (mIM)
		break;

	case -2: // decline silently
	{
		LLStringUtil::format_map_t args;
		args["[DESC]"] = mDesc;
		args["[NAME]"] = mFromName;
		LLFloaterChat::addChatHistory(LLTrans::getString("InvOfferDeclineSilent", args));
	}
	break;
	case -1: // accept silently
	{
		LLOpenAgentOffer* open_agent_offer = new LLOpenAgentOffer(mObjectID, from_string);
		open_agent_offer->startFetch();
		if(catp || (itemp && itemp->isFinished()))
		{
			open_agent_offer->done();
		}
		else
		{
			opener = open_agent_offer;
		}
		LLStringUtil::format_map_t args;
		args["[DESC]"] = mDesc;
		args["[NAME]"] = mFromName;
		LLFloaterChat::addChatHistory(LLTrans::getString("InvOfferAcceptSilent", args));
	}

	break;
	
	case IOR_BUSY:
		//Busy falls through to decline.  Says to make busy message.
		busy=TRUE;
	case IOR_MUTE:
		// MUTE falls through to decline
	case IOR_DECLINE:
		// DECLINE. The math for the dialog works, because the decline
		// for inventory_offered, task_inventory_offer or
		// group_notice_inventory is 2 greater than the offer integer value.
		// Generates IM_INVENTORY_DECLINED, IM_TASK_INVENTORY_DECLINED,
		// or IM_GROUP_NOTICE_INVENTORY_DECLINED
	default:
		// close button probably (or any of the fall-throughs from above)
		msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 2));
		msg->addBinaryDataFast(_PREHASH_BinaryBucket, EMPTY_BINARY_BUCKET, EMPTY_BINARY_BUCKET_SIZE);
		// send the message
		msg->sendReliable(mHost);

		if (!mFromGroup && gSavedSettings.getBOOL("LogInventoryDecline"))
		{
// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1e) | Added: RLVa-1.2.1e
			if ( (rlv_handler_t::isEnabled()) &&
				 (IM_TASK_INVENTORY_OFFERED == mIM) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 1) )
			{
				std::string::size_type idxToken = mDesc.find("'  ( http://");
				if (std::string::npos != idxToken)
					RlvBehaviourNotifyHandler::instance().sendNotification("declined inv_offer " + mDesc.substr(1, idxToken - 1));
			}
// [/RLVa:KB]

			LLStringUtil::format_map_t log_message_args;
			log_message_args["[DESC]"] = mDesc;
			log_message_args["[NAME]"] = mFromName;
			log_message = LLTrans::getString("InvOfferDecline", log_message_args);
			chat.mText = log_message;
			if( LLMuteList::getInstance()->isMuted(mFromID ) && ! LLMuteList::getInstance()->isLinden(mFromName) )  // muting for SL-42269
			{
				chat.mMuted = TRUE;
			}
			LLFloaterChat::addChatHistory(chat);
		}

		// If it's from an agent, we have to fetch the item to throw
		// it away. If it's from a task or group, just denying the 
		// request will suffice to discard the item.
		if(IM_INVENTORY_OFFERED == mIM)
		{
			LLDiscardAgentOffer* discard_agent_offer = new LLDiscardAgentOffer(mFolderID, mObjectID);
			discard_agent_offer->startFetch();
			if (catp || (itemp && itemp->isFinished()))
			{
				discard_agent_offer->done();
			}
			else
			{
				opener = discard_agent_offer;
			}
			
		}
		if (busy &&	(!mFromGroup && !mFromObject))
		{
			send_do_not_disturb_message(msg,mFromID);
		}
		break;
	}

	if(opener)
	{
		gInventory.addObserver(opener);
	}

	// Allow these to stack up, but once you deal with one, reset the
	// position.
	gFloaterView->resetStartingFloaterPosition();

	delete this;
	return false;
}

bool has_spam_bypass(bool is_friend, bool is_owned_by_me)
{
	static LLCachedControl<bool> antispam_not_mine(gSavedSettings,"AntiSpamNotMine");
	static LLCachedControl<bool> antispam_not_friend(gSavedSettings,"AntiSpamNotFriend");
	return (antispam_not_mine && is_owned_by_me) || (antispam_not_friend && is_friend);
}

bool is_spam_filtered(const EInstantMessage& dialog, bool is_friend, bool is_owned_by_me)
{
	// First, check that this doesn't bypass.
	if (has_spam_bypass(is_friend, is_owned_by_me)) return false;

	// Second, check the master filter
	static LLCachedControl<bool> antispam(gSavedSettings,"_NACL_Antispam");
	if (antispam) return true;

	// Third, check if this dialog type is even being filtered
	switch(dialog)
	{
	case IM_GROUP_NOTICE:
	case IM_GROUP_NOTICE_REQUESTED:
	{
		static const LLCachedControl<bool> filter("AntiSpamGroupNotices");
		return filter;
	}
	case IM_GROUP_INVITATION:
	{
		static const LLCachedControl<bool> filter("AntiSpamGroupInvites");
		return filter;
	}
	case IM_INVENTORY_OFFERED:
	case IM_TASK_INVENTORY_OFFERED:
	{
		static const LLCachedControl<bool> filter("AntiSpamItemOffers");
		return filter;
	}
	case IM_FROM_TASK_AS_ALERT:
	{
		static const LLCachedControl<bool> filter("AntiSpamAlerts");
		return filter;
	}
	case IM_LURE_USER:
	{
		static const LLCachedControl<bool> filter("AntiSpamTeleports");
		return filter;
	}
	case IM_TELEPORT_REQUEST:
	{
		static const LLCachedControl<bool> filter("AntiSpamTeleportRequests");
		return filter;
	}
	case IM_FRIENDSHIP_OFFERED:
	{
		static const LLCachedControl<bool> filter("AntiSpamFriendshipOffers");
		return filter;
	}
	case IM_COUNT:
	{
		// Bit of a hack, we should never get here unless we did this on purpose, though, doesn't matter because we'd do nothing anyway
		static const LLCachedControl<bool> filter( "AntiSpamScripts");
		return filter;
	}
	default:
		return false;
	}
}

bool lure_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = 0;
	if (response.isInteger()) 
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotificationsUtil::getSelectedOption(notification, response);
	}
	
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	LLUUID lure_id = notification["payload"]["lure_id"].asUUID();
	BOOL godlike = notification["payload"]["godlike"].asBoolean();

	switch (option)
	{
	case 0:
		{
			// accept
			gAgent.teleportViaLure(lure_id, godlike);
		}
		break;
	case 1:
	default:
		// decline
		send_simple_im(from_id,
					   LLStringUtil::null,
					   IM_LURE_DECLINED,
					   lure_id);
		break;
	}
	return false;
}
static LLNotificationFunctorRegistration lure_callback_reg("TeleportOffered", lure_callback);

bool goto_url_callback(const LLSD& notification, const LLSD& response)
{
	std::string url = notification["payload"]["url"].asString();
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(1 == option)
	{
		LLWeb::loadURL(url);
	}
	return false;
}
static LLNotificationFunctorRegistration goto_url_callback_reg("GotoURL", goto_url_callback);

void process_improved_im(LLMessageSystem *msg, void **user_data)
{
	if (gNoRender)
	{
		return;
	}
	LLUUID from_id;
	BOOL from_group;
	LLUUID to_id;
	U8 offline;
	U8 d = 0;
	LLUUID session_id;
	U32 timestamp;
    std::string agentName;
	std::string message;
	U32 parent_estate_id = 0;
	LLUUID region_id;
	LLVector3 position;
	U8 binary_bucket[MTUBYTES];
	S32 binary_bucket_size;
	
	// *TODO: Translate - need to fix the full name to first/last (maybe)
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, from_id);
	msg->getBOOLFast(_PREHASH_MessageBlock, _PREHASH_FromGroup, from_group);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ToAgentID, to_id);
	msg->getU8Fast(_PREHASH_MessageBlock, _PREHASH_Offline, offline);
	msg->getU8Fast(_PREHASH_MessageBlock, _PREHASH_Dialog, d);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ID, session_id);
	msg->getU32Fast(_PREHASH_MessageBlock, _PREHASH_Timestamp, timestamp);
	//msg->getData("MessageBlock", "Count",		&count);
    msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_FromAgentName, agentName);
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_Message,		message);
	// NaCl - Newline flood protection
	auto antispam = NACLAntiSpamRegistry::getIfExists();
	if (antispam && can_block(from_id))
	{
		static const LLCachedControl<U32> SpamNewlines("_NACL_AntiSpamNewlines");
		boost::sregex_iterator iter(message.begin(), message.end(), NEWLINES);
		if((U32)std::abs(std::distance(iter, boost::sregex_iterator())) > SpamNewlines)
		{
			antispam->blockOnQueue(NACLAntiSpamRegistry::QUEUE_IM, from_id);
			LL_INFOS() << "[antispam] blocked owner due to too many newlines: " << from_id << LL_ENDL;
			if (gSavedSettings.getBOOL("AntiSpamNotify"))
			{
				LLSD args;
				args["SOURCE"] = from_id;
				args["AMOUNT"] = boost::lexical_cast<std::string>(SpamNewlines);
				LLNotificationsUtil::add("AntiSpamNewlineFlood", args);
			}
			return;
		}
	}
	// NaCl End
	msg->getU32Fast(_PREHASH_MessageBlock, _PREHASH_ParentEstateID, parent_estate_id);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_RegionID, region_id);
	msg->getVector3Fast(_PREHASH_MessageBlock, _PREHASH_Position, position);
	msg->getBinaryDataFast(_PREHASH_MessageBlock, _PREHASH_BinaryBucket, binary_bucket, 0, 0, MTUBYTES);
	binary_bucket_size = msg->getSizeFast(_PREHASH_MessageBlock, _PREHASH_BinaryBucket);
	EInstantMessage dialog = (EInstantMessage)d;

	// NaCl - Antispam Registry
	if (antispam && (dialog != IM_TYPING_START && dialog != IM_TYPING_STOP)
	&& antispam->checkQueue(NACLAntiSpamRegistry::QUEUE_IM, from_id))
		return;
	// NaCl End

    LLHost sender = msg->getSender();

    LLIMProcessing::processNewMessage(from_id,
        from_group,
        to_id,
        offline,
        dialog,
        session_id,
        timestamp,
        agentName,
        message,
        parent_estate_id,
        region_id,
        position,
        binary_bucket,
        binary_bucket_size,
        sender);
}

void send_do_not_disturb_message(LLMessageSystem* msg, const LLUUID& from_id, const LLUUID& session_id)
{
	if (gAgent.isDoNotDisturb())
	{
		std::string my_name;
		LLAgentUI::buildFullname(my_name);
		std::string name;
		gCacheName->getFullName(from_id, name);
		std::string response = gSavedPerAccountSettings.getString("BusyModeResponse");
		pack_instant_message(
			msg,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			from_id,
			my_name,
			replace_wildcards(response, from_id, name),
			IM_ONLINE,
			IM_BUSY_AUTO_RESPONSE,
			session_id);
		gAgent.sendReliableMessage();
		LLAvatarName av_name;
		std::string ns_name = LLAvatarNameCache::get(from_id, &av_name) ? av_name.getNSName() : name;
		const auto show = gSavedPerAccountSettings.getBOOL("BusyModeResponseShow");
		if (show) gIMMgr->addMessage(session_id, from_id, name, LLTrans::getString("IM_autoresponded_to") + ' ' + ns_name);
		if (!gSavedPerAccountSettings.getBOOL("BusyModeResponseItem")) return; // Not sending an item, finished
		if (LLViewerInventoryItem* item = gInventory.getItem(static_cast<LLUUID>(gSavedPerAccountSettings.getString("BusyModeResponseItemID"))))
		{
			LLGiveInventory::doGiveInventoryItem(from_id, item, session_id);
			if (show)
				gIMMgr->addMessage(session_id, from_id, name, llformat("%s %s \"%s\"", ns_name.c_str(), LLTrans::getString("IM_autoresponse_sent_item").c_str(), item->getName().c_str()));
		}
	}
}

bool callingcard_offer_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLUUID fid;
	LLMessageSystem* msg = gMessageSystem;
	switch (option)
	{
	case 0:
		// accept
		msg->newMessageFast(_PREHASH_AcceptCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, notification["payload"]["transaction_id"].asUUID());
		fid = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		msg->nextBlockFast(_PREHASH_FolderData);
		msg->addUUIDFast(_PREHASH_FolderID, fid);
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
		break;
	case 1:
		// decline		
		msg->newMessageFast(_PREHASH_DeclineCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, notification["payload"]["transaction_id"].asUUID());
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
		send_do_not_disturb_message(msg, notification["payload"]["source_id"].asUUID());
		break;
	default:
		// close button probably, possibly timed out
		break;
	}

	return false;
}

static LLNotificationFunctorRegistration callingcard_offer_cb_reg("OfferCallingCard", callingcard_offer_callback);

void process_offer_callingcard(LLMessageSystem* msg, void**)
{
	// NaCl - Antispam
	if (is_spam_filtered(IM_FRIENDSHIP_OFFERED, false, false))
		return;
	// NaCl End
	// someone has offered to form a friendship
	LL_DEBUGS("Messaging") << "callingcard offer" << LL_ENDL;

	LLUUID source_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, source_id);

	// NaCl - Antispam Registry
	if (auto antispam = NACLAntiSpamRegistry::getIfExists())
		if (antispam->checkQueue(NACLAntiSpamRegistry::QUEUE_CALLING_CARD, source_id))
			return;
	// NaCl End

	LLUUID tid;
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_TransactionID, tid);

	LLSD payload;
	payload["transaction_id"] = tid;
	payload["source_id"] = source_id;
	payload["sender"] = msg->getSender().getIPandPort();

	LLViewerObject* source = gObjectList.findObject(source_id);
	LLSD args;
	std::string source_name;
	if (source && source->isAvatar())
	{
		LLNameValue* nvfirst = source->getNVPair("FirstName");
		LLNameValue* nvlast  = source->getNVPair("LastName");
		if (nvfirst && nvlast)
		{
			source_name = LLCacheName::buildFullName(
				nvfirst->getString(), nvlast->getString());
		}
	}

	if (!source_name.empty())
	{
		if (gAgent.isDoNotDisturb()
			|| LLMuteList::getInstance()->isMuted(source_id, source_name, LLMute::flagTextChat))
		{
			// automatically decline offer
			LLNotifications::instance().forceResponse(LLNotification::Params("OfferCallingCard").payload(payload), 1);
		}
		else
		{
			args["NAME"] = source_name;
			LLNotificationsUtil::add("OfferCallingCard", args, payload);
		}
	}
	else
	{
		LL_WARNS("Messaging") << "Calling card offer from an unknown source." << LL_ENDL;
	}
}

void process_accept_callingcard(LLMessageSystem* msg, void**)
{
	LLNotificationsUtil::add("CallingCardAccepted");
}

void process_decline_callingcard(LLMessageSystem* msg, void**)
{
	LLNotificationsUtil::add("CallingCardDeclined");
}

#if 0	// Google translate doesn't work anymore
class ChatTranslationReceiver final : public LLTranslate::TranslationReceiver
{
public :
	ChatTranslationReceiver(const std::string &fromLang, const std::string &toLang, LLChat *chat, 
		const BOOL history)
		: LLTranslate::TranslationReceiver(fromLang, toLang),
		m_chat(chat),
		m_history(history)	
	{
	}

	static boost::intrusive_ptr<ChatTranslationReceiver> build(const std::string &fromLang, const std::string &toLang, LLChat *chat, const BOOL history)
	{
		return boost::intrusive_ptr<ChatTranslationReceiver>(new ChatTranslationReceiver(fromLang, toLang, chat, history));
	}

protected:
	void handleResponse(const std::string &translation, const std::string &detectedLanguage)
	{		
		if (m_toLang != detectedLanguage)
			m_chat->mText += " (" + translation + ")";			

		add_floater_chat(*m_chat, m_history);

		delete m_chat;
	}

	void handleFailure()
	{
		LLTranslate::TranslationReceiver::handleFailure();

		m_chat->mText += " (?)";

		add_floater_chat(*m_chat, m_history);

		delete m_chat;
	}

	char const* getName() const override { return "ChatTranslationReceiver"; }

private:
	LLChat *m_chat;
	const BOOL m_history;		
};
#endif

void add_floater_chat(LLChat &chat, const bool history)
{
	if (history)
	{
		// just add to history
		LLFloaterChat::addChatHistory(chat);
	}
	else
	{
		// show on screen and add to history
		LLFloaterChat::addChat(chat, FALSE, FALSE);
	}
}

#if 0	// Google translate doesn't work anymore
void check_translate_chat(const std::string &mesg, LLChat &chat, const bool history)
{	
	const bool translate = LLUI::sConfigGroup->getBOOL("TranslateChat");

	if (translate && chat.mSourceType != CHAT_SOURCE_SYSTEM)
	{
		// fromLang hardcoded to "" (autodetection) pending implementation of
		// SVC-4879
		const std::string &fromLang = "";
		const std::string &toLang = LLTranslate::getTranslateLanguage();
		LLChat *newChat = new LLChat(chat);

		LLHTTPClient::ResponderPtr result = ChatTranslationReceiver::build(fromLang, toLang, newChat, history);
		LLTranslate::translateMessage(result, fromLang, toLang, mesg);
	}
	else
	{
		add_floater_chat(chat, history);
	}
}
#endif

void process_chat_from_simulator(LLMessageSystem* msg, void** user_data)
{
	LLChat	chat;
	std::string		mesg;
	std::string		from_name;
	U8			source_temp;
	U8			type_temp;
	U8			audible_temp;
	LLColor4	color(1.0f, 1.0f, 1.0f, 1.0f);
	LLUUID		from_id;
	LLUUID		owner_id;
	BOOL		is_owned_by_me = FALSE;
	LLViewerObject*	chatter;

	msg->getString("ChatData", "FromName", from_name);
	// make sure that we don't have an empty or all-whitespace name
	LLStringUtil::trim(from_name);
	if (from_name.empty())
	{
		from_name = LLTrans::getString("Unnamed");
	}

	msg->getUUID("ChatData", "SourceID", from_id);
	chat.mFromID = from_id;
	
	chatter = gObjectList.findObject(from_id);
	if(chatter && chatter->isAvatar())
	{
		((LLVOAvatar*)chatter)->mIdleTimer.reset();
	}

	// Object owner for objects
	msg->getUUID("ChatData", "OwnerID", owner_id);
	bool has_owner = owner_id.notNull();
	if (chatter && has_owner) chatter->mOwnerID = owner_id; // Singu Note: Try to get Owner whenever possible

	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_SourceType, source_temp);
	chat.mSourceType = (EChatSourceType)source_temp;

	msg->getU8("ChatData", "ChatType", type_temp);
	chat.mChatType = (EChatType)type_temp;

	// NaCL - Antispam Registry
	auto antispam = NACLAntiSpamRegistry::getIfExists();
	if (antispam && chat.mChatType != CHAT_TYPE_START && chat.mChatType != CHAT_TYPE_STOP	//Chat type isn't typing
	&& (antispam->checkQueue(NACLAntiSpamRegistry::QUEUE_CHAT, from_id, !has_owner ? LFIDBearer::AVATAR : LFIDBearer::OBJECT)	// Spam from an object or avatar?
	|| (has_owner && (antispam->checkQueue(NACLAntiSpamRegistry::QUEUE_CHAT, owner_id)))))	// Spam from a resident?
		return;
	// NaCl End

	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_Audible, audible_temp);
	chat.mAudible = (EChatAudible)audible_temp;
	
	chat.mTime = LLFrameTimer::getElapsedSeconds();

	// IDEVO Correct for new-style "Resident" names
	if (chat.mSourceType == CHAT_SOURCE_AGENT)
	{
		// I don't know if it's OK to change this here, if
		// anything downstream does lookups by name, for instance

		LLAvatarName av_name;
		if (LLAvatarNameCache::get(from_id, &av_name))
		{
			chat.mFromName = av_name.getNSName();
		}
		else
		{
			chat.mFromName = LLCacheName::cleanFullName(from_name);
		}
	}
	else
	{
		// make sure that we don't have an empty or all-whitespace name
		LLStringUtil::trim(from_name);
		if (from_name.empty())
		{
			from_name = LLTrans::getString("Unnamed");
		}
		chat.mFromName = from_name;
	}

	bool is_do_not_disturb = gAgent.isDoNotDisturb();

	bool is_muted = false;
	bool is_linden = false;
	is_muted = LLMuteList::getInstance()->isMuted(
		from_id,
		from_name,
		LLMute::flagTextChat) 
		|| LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagTextChat);
	is_linden = chat.mSourceType != CHAT_SOURCE_OBJECT &&
		LLMuteList::getInstance()->isLinden(from_name);

	msg->getStringFast(_PREHASH_ChatData, _PREHASH_Message, mesg);

	bool handle_obj_auth(const LLUUID& from_id, const std::string& mesg);
	if (source_temp == CHAT_SOURCE_OBJECT && type_temp == CHAT_TYPE_OWNER && handle_obj_auth(from_id, mesg)) return;

	if (is_muted && (chat.mSourceType == CHAT_SOURCE_OBJECT))
	{
		return;
	}

	BOOL is_audible = (CHAT_AUDIBLE_FULLY == chat.mAudible);
	// <edit>
	// because I moved it to above
	//chatter = gObjectList.findObject(from_id);
	// </edit>
	if (chatter)
	{
		chat.mPosAgent = chatter->getPositionAgent();

		// Make swirly things only for talking objects. (not script debug messages, though)
//		if (chat.mSourceType == CHAT_SOURCE_OBJECT 
//			&& chat.mChatType != CHAT_TYPE_DEBUG_MSG
//			&& gSavedSettings.getBOOL("EffectScriptChatParticles") )
// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0b) | Modified: RLVa-1.0.0g
		if ( ((chat.mSourceType == CHAT_SOURCE_OBJECT) && (chat.mChatType != CHAT_TYPE_DEBUG_MSG)) &&
			 (gSavedSettings.getBOOL("EffectScriptChatParticles")) &&
			 ((!rlv_handler_t::isEnabled()) || (CHAT_TYPE_OWNER != chat.mChatType)) )
// [/RLVa:KB]
		{
			LLPointer<LLViewerPartSourceChat> psc = new LLViewerPartSourceChat(chatter->getPositionAgent());
			psc->setSourceObject(chatter);
			psc->setColor(color);
			//We set the particles to be owned by the object's owner, 
			//just in case they should be muted by the mute list
			psc->setOwnerUUID(owner_id);
			LLViewerPartSim::getInstance()->addPartSource(psc);
		}

		// record last audible utterance
		if (is_audible
			&& (is_linden || (!is_muted && !is_do_not_disturb)))
		{
			if (chat.mChatType != CHAT_TYPE_START 
				&& chat.mChatType != CHAT_TYPE_STOP)
			{
				gAgent.heardChat(chat.mFromID);
			}
		}

		is_owned_by_me = chatter->permYouOwner();
	}
	else is_owned_by_me = owner_id == gAgentID;

	U32 links_for_chatting_objects = gSavedSettings.getU32("LinksForChattingObjects");
	if (links_for_chatting_objects != 0 /*&& chatter*/ && chat.mSourceType == CHAT_SOURCE_OBJECT &&
		(!is_owned_by_me || links_for_chatting_objects == 2)
// [RLVa:KB]
		&& !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)
// [/RLVa:KB]
		)
	{
		LLSD query_string;
		query_string["name"]  = from_name;
		query_string["owner"] = owner_id;

// [RLVa:KB]
		if( !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC) )
// [/RLVa:KB]
		{
			// Fallback on the owner, if the chatter isn't present, lastly use the agent's region at the origin.
			const LLViewerObject* obj(chatter ? chatter : gObjectList.findObject(owner_id));
			// Compute the object SLURL.
			LLVector3 pos = obj ? obj->getPositionRegion() : LLVector3::zero;
			S32 x = ll_round((F32)fmod((F64)pos.mV[VX], (F64)REGION_WIDTH_METERS));
			S32 y = ll_round((F32)fmod((F64)pos.mV[VY], (F64)REGION_WIDTH_METERS));
			S32 z = ll_round((F32)pos.mV[VZ]);
			std::ostringstream location;
			location << (obj ? obj->getRegion() : gAgent.getRegion())->getName() << "/" << x << "/" << y << "/" << z;
			if (chatter != obj) location << "?owner_not_object";
			query_string["slurl"] = location.str();
		}

		std::ostringstream link;
		link << "secondlife:///app/objectim/" << from_id << LLURI::mapToQueryString(query_string);
		chat.mURL = link.str();
	}

	if (is_audible)
	{
		// NaCl - Newline flood protection
		if (antispam && can_block(from_id))
		{
			static const LLCachedControl<U32> SpamNewlines("_NACL_AntiSpamNewlines");
			boost::sregex_iterator iter(mesg.begin(), mesg.end(), NEWLINES);
			if((U32)std::abs(std::distance(iter, boost::sregex_iterator())) > SpamNewlines)
			{
				antispam->blockOnQueue(NACLAntiSpamRegistry::QUEUE_CHAT, owner_id);
				if(gSavedSettings.getBOOL("AntiSpamNotify"))
				{
					LLSD args;
					args["MESSAGE"] = "Chat: Blocked newline flood from " + LLAvatarActions::getSLURL(owner_id);
					LLNotificationsUtil::add("SystemMessageTip", args);
				}
				return;
			}
		}
		// NaCl End

		if (chatter && chatter->isAvatar())
		{
			if (LLAvatarNameCache::getNSName(from_id, from_name))
				chat.mFromName = from_name;
		}

		BOOL visible_in_chat_bubble = FALSE;
		std::string verb;

		color.setVec(1.f, 1.f, 1.f, 1.f);

// [RLVa:KB] - Checked: 2010-04-23 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
		if ( (rlv_handler_t::isEnabled()) && (CHAT_TYPE_START != chat.mChatType) && (CHAT_TYPE_STOP != chat.mChatType) )
		{
			// NOTE: chatter can be NULL (may not have rezzed yet, or could be another avie's HUD attachment)
			BOOL is_attachment = (chatter) ? chatter->isAttachment() : FALSE;

			// Filtering "rules":
			//   avatar  => filter all avie text (unless it's this avie or they're an exemption)
			//   objects => filter everything except attachments this avie owns (never filter llOwnerSay or llRegionSayTo chat)
			if ( ( (CHAT_SOURCE_AGENT == chat.mSourceType) && (from_id != gAgent.getID()) ) || 
				 ( (CHAT_SOURCE_OBJECT == chat.mSourceType) && ((!is_owned_by_me) || (!is_attachment)) && 
				   (CHAT_TYPE_OWNER != chat.mChatType) && (CHAT_TYPE_DIRECT != chat.mChatType) ) )
			{
				bool fIsEmote = RlvUtil::isEmote(mesg);
				if ((!fIsEmote) &&
					(((gRlvHandler.hasBehaviour(RLV_BHVR_RECVCHAT)) && (!gRlvHandler.isException(RLV_BHVR_RECVCHAT, from_id))) ||
					 ((gRlvHandler.hasBehaviour(RLV_BHVR_RECVCHATFROM)) && (gRlvHandler.isException(RLV_BHVR_RECVCHATFROM, from_id))) ))
				{
					if ( (gRlvHandler.filterChat(mesg, false)) && (!gSavedSettings.getBOOL("RestrainedLoveShowEllipsis")) )
						return;
				}
				else if ((fIsEmote) &&
					     (((gRlvHandler.hasBehaviour(RLV_BHVR_RECVEMOTE)) && (!gRlvHandler.isException(RLV_BHVR_RECVEMOTE, from_id))) ||
					      ((gRlvHandler.hasBehaviour(RLV_BHVR_RECVEMOTEFROM)) && (gRlvHandler.isException(RLV_BHVR_RECVEMOTEFROM, from_id))) ))
				{
					if (!gSavedSettings.getBOOL("RestrainedLoveShowEllipsis"))
						return;
					mesg = "/me ...";
				}
			}

			// Filtering "rules":
			//   avatar => filter only their name (unless it's this avie)
			//   other  => filter everything
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
			{
				if (CHAT_SOURCE_AGENT != chat.mSourceType)
				{
					RlvUtil::filterNames(chat.mFromName);
				}
				else if (chat.mFromID != gAgentID)
				{
					chat.mFromName = RlvStrings::getAnonym(chat.mFromName);
					chat.mRlvNamesFiltered = TRUE;
				}
			}

			// Create an "objectim" URL for objects if we're either @shownames or @showloc restricted
			// (we need to do this now because we won't be have enough information to do it later on)
			if ( (CHAT_SOURCE_OBJECT == chat.mSourceType) &&
				 ((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))) )
			{
				LLSD sdQuery;
				sdQuery["name"] = chat.mFromName;
				sdQuery["owner"] = owner_id;

				if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (!is_owned_by_me) )
					sdQuery["rlv_shownames"] = true;

				const LLViewerRegion* pRegion = LLWorld::getInstance()->getRegionFromPosAgent(chat.mPosAgent);
				if (pRegion)
					sdQuery["slurl"] = LLSLURL(pRegion->getName(), chat.mPosAgent).getLocationString();

				chat.mURL = LLSLURL("objectim", from_id, LLURI::mapToQueryString(sdQuery)).getSLURLString();
			}
		}
// [/RLVa:KB]

		BOOL ircstyle = FALSE;

		// Look for IRC-style emotes here so chatbubbles work
		std::string prefix = mesg.substr(0, 4);
		if (boost::iequals(prefix, "/me ") || boost::iequals(prefix, "/me'")) // <alchemy/>
		{
			chat.mText = chat.mFromName;
			mesg = mesg.substr(3);
			ircstyle = TRUE;
			// This block was moved up to allow bubbles with italicized chat
			// set CHAT_STYLE_IRC to avoid adding Avatar Name as author of message. See EXT-656
			chat.mChatStyle = CHAT_STYLE_IRC;
		}
		chat.mText += mesg;

		// Look for the start of typing so we can put "..." in the bubbles.
		if (CHAT_TYPE_START == chat.mChatType)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, TRUE);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->startTyping();
			}
			return;
		}
		if (CHAT_TYPE_STOP == chat.mChatType)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, FALSE);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->stopTyping();
			}
			return;
		}

		// We have a real utterance now, so can stop showing "..." and proceed.
		if (chatter && chatter->isAvatar())
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, FALSE);
			static_cast<LLVOAvatar*>(chatter)->stopTyping();

			if (!is_muted /*&& !is_do_not_disturb*/)
			{
				static const LLCachedControl<bool> use_chat_bubbles("UseChatBubbles",false);
				visible_in_chat_bubble = use_chat_bubbles;
				static_cast<LLVOAvatar*>(chatter)->addChat(chat);
			}
		}

		// Look for IRC-style emotes
		if (ircstyle)
		{
			// Do nothing, ircstyle is fixed above for chat bubbles
		}
		else
		{
			switch (chat.mChatType)
			{
			case CHAT_TYPE_WHISPER:
				verb = " " + LLTrans::getString("whisper") + " ";
				break;
			case CHAT_TYPE_OWNER:
// [RLVa:KB] - Checked: 2010-02-XX (RLVa-1.2.0a) | Modified: RLVa-1.1.0f
				// TODO-RLVa: [RLVa-1.2.0] consider rewriting this before a RLVa-1.2.0 release
				if ( (rlv_handler_t::isEnabled()) && (mesg.length() > 3) && (RLV_CMD_PREFIX == mesg[0]) && (CHAT_TYPE_OWNER == chat.mChatType) )
				{
					mesg.erase(0, 1);
					LLStringUtil::toLower(mesg);

					std::string strExecuted, strFailed, strRetained, *pstr;

					boost_tokenizer tokens(mesg, boost::char_separator<char>(",", "", boost::drop_empty_tokens));
					for (auto itToken = tokens.begin(); itToken != tokens.end(); ++itToken)
					{
						std::string strCmd = *itToken;

						ERlvCmdRet eRet = gRlvHandler.processCommand(from_id, strCmd, true);
						if ( (RlvSettings::getDebug()) &&
							 ( (!RlvSettings::getDebugHideUnsetDup()) || 
							   ((RLV_RET_SUCCESS_UNSET != eRet) && (RLV_RET_SUCCESS_DUPLICATE != eRet)) ) )
						{
							if ( RLV_RET_SUCCESS == (eRet & RLV_RET_SUCCESS) )	
								pstr = &strExecuted;
							else if ( RLV_RET_FAILED == (eRet & RLV_RET_FAILED) )
								pstr = &strFailed;
							else if (RLV_RET_RETAINED == eRet)
								pstr = &strRetained;
							else
							{
								RLV_ASSERT(false);
								pstr = &strFailed;
							}

							const char* pstrSuffix = RlvStrings::getStringFromReturnCode(eRet);
							if (pstrSuffix)
								strCmd.append(" (").append(pstrSuffix).append(")");

							if (!pstr->empty())
								pstr->push_back(',');
							pstr->append(strCmd);
						}
					}

					if (RlvForceWear::instanceExists())
						RlvForceWear::instance().done();

					if ( (!RlvSettings::getDebug()) || ((strExecuted.empty()) && (strFailed.empty()) && (strRetained.empty())) )
						return;

					// Silly people want comprehensive debug messages, blah :p
					if ( (!strExecuted.empty()) && (strFailed.empty()) && (strRetained.empty()) )
					{
						verb = " executes: @";
						mesg = strExecuted;
					}
					else if ( (strExecuted.empty()) && (!strFailed.empty()) && (strRetained.empty()) )
					{
						verb = " failed: @";
						mesg = strFailed;
					}
					else if ( (strExecuted.empty()) && (strFailed.empty()) && (!strRetained.empty()) )
					{
						verb = " retained: @";
						mesg = strRetained;
					}
					else
					{
						verb = ": @";
						if (!strExecuted.empty())
							mesg += "\n    - executed: @" + strExecuted;
						if (!strFailed.empty())
							mesg += "\n    - failed: @" + strFailed;
						if (!strRetained.empty())
							mesg += "\n    - retained: @" + strRetained;
					}

					break;
				}
// [/RLVa:KB]
#if SHY_MOD //Command handler
				if(SHCommandHandler::handleCommand(false,mesg,from_id,chatter)) 
					return;
#endif //shy_mod
// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0b) | Modified: RLVa-1.0.0g
				// Copy/paste from above
				if  ( (rlv_handler_t::isEnabled()) && (chatter) && (chat.mSourceType == CHAT_SOURCE_OBJECT) &&
					  (gSavedSettings.getBOOL("EffectScriptChatParticles")) )
				{
					LLPointer<LLViewerPartSourceChat> psc = new LLViewerPartSourceChat(chatter->getPositionAgent());
					psc->setSourceObject(chatter);
					psc->setColor(color);
					//We set the particles to be owned by the object's owner, 
					//just in case they should be muted by the mute list
					psc->setOwnerUUID(owner_id);
					LLViewerPartSim::getInstance()->addPartSource(psc);
				}
// [/RLVa:KB]
			case CHAT_TYPE_DEBUG_MSG:
			case CHAT_TYPE_NORMAL:
			case CHAT_TYPE_DIRECT:
				verb = ": ";
				break;
			case CHAT_TYPE_SHOUT:
				verb = " " + LLTrans::getString("shout") + " ";
				break;
			case CHAT_TYPE_START:
			case CHAT_TYPE_STOP:
				LL_WARNS("Messaging") << "Got chat type start/stop in main chat processing." << LL_ENDL;
				break;
			default:
				LL_WARNS("Messaging") << "Unknown type " << chat.mChatType << " in chat!" << LL_ENDL;
				verb = ": ";
				break;
			}

			chat.mText = chat.mFromName + verb + mesg;
		}

		if (chatter)
		{
			chat.mPosAgent = chatter->getPositionAgent();
		}

		chat.mMuted = is_muted && !is_linden;
		bool only_history = visible_in_chat_bubble || (!is_linden && !is_owned_by_me && is_do_not_disturb);
#if 0	// Google translate doesn't work anymore
		if (!chat.mMuted)
		{
			check_translate_chat(mesg, chat, only_history);
		}
#else
		add_floater_chat(chat, only_history);
#endif
	}
}


// Simulator we're on is informing the viewer that the agent
// is starting to teleport (perhaps to another sim, perhaps to the 
// same sim). If we initiated the teleport process by sending some kind 
// of TeleportRequest, then this info is redundant, but if the sim 
// initiated the teleport (via a script call, being killed, etc.) 
// then this info is news to us.
void process_teleport_start(LLMessageSystem* msg, void**)
{
	// on teleport, don't tell them about destination guide anymore
	//LLFirstUse::notUsingDestinationGuide(false);
	U32 teleport_flags = 0x0;
	msg->getU32("Info", "TeleportFlags", teleport_flags);

	if (gAgent.getTeleportState() == LLAgent::TELEPORT_MOVING)
	{
		// Race condition?
		LL_WARNS("Messaging") << "Got TeleportStart, but teleport already in progress. TeleportFlags=" << teleport_flags << LL_ENDL;
	}

	LL_DEBUGS("Messaging") << "Got TeleportStart with TeleportFlags=" << teleport_flags << ". gTeleportDisplay: " << gTeleportDisplay << ", gAgent.mTeleportState: " << gAgent.getTeleportState() << LL_ENDL;

//	if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL)
// [RLVa:KB] - Checked: 2010-04-07 (RLVa-1.2.0d) | Added: RLVa-0.2.0b
	if ( (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL) || (!gRlvHandler.getCanCancelTp()) )
// [/RLVa:KB]
	{
		gViewerWindow->setProgressCancelButtonVisible(FALSE);
	}
	else
	{
		gViewerWindow->setProgressCancelButtonVisible(TRUE, LLTrans::getString("Cancel"));
	}

	// Freeze the UI and show progress bar
	// Note: could add data here to differentiate between normal teleport and death.

	if (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
	{
		gTeleportDisplay = TRUE;
		gAgent.setTeleportState(LLAgent::TELEPORT_START);

		make_ui_sound("UISndTeleportOut");
		
		LL_INFOS("Messaging") << "Teleport initiated by remote TeleportStart message with TeleportFlags: " <<  teleport_flags << LL_ENDL;

		// Don't call LLFirstUse::useTeleport here because this could be
		// due to being killed, which would send you home, not to a Telehub
	}
}

void process_teleport_progress(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if ((gAgent.getID() != agent_id)
	   || (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE))
	{
		LL_WARNS("Messaging") << "Unexpected teleport progress message." << LL_ENDL;
		return;
	}
	U32 teleport_flags = 0x0;
	msg->getU32("Info", "TeleportFlags", teleport_flags);
//	if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL)
// [RLVa:KB] - Checked: 2010-04-07 (RLVa-1.2.0d) | Added: RLVa-0.2.0b
	if ( (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL) || (!gRlvHandler.getCanCancelTp()) )
// [/RLVa:KB]
	{
		gViewerWindow->setProgressCancelButtonVisible(FALSE);
	}
	else
	{
		gViewerWindow->setProgressCancelButtonVisible(TRUE, LLTrans::getString("Cancel"));
	}
	std::string buffer;
	msg->getString("Info", "Message", buffer);
	LL_DEBUGS("Messaging") << "teleport progress: " << buffer << " flags: " << teleport_flags << LL_ENDL;

	//Sorta hacky...default to using simulator raw messages
	//if we don't find the coresponding mapping in our progress mappings
	std::string message = buffer;

	if (LLAgent::sTeleportProgressMessages.find(buffer) != 
		LLAgent::sTeleportProgressMessages.end())
	{
		message = LLAgent::sTeleportProgressMessages[buffer];
	}

	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages[message]);
}

class LLFetchInWelcomeArea final : public LLInventoryFetchDescendentsObserver
{
public:
	LLFetchInWelcomeArea(const uuid_vec_t& ids) :
		LLInventoryFetchDescendentsObserver(ids)
	{
	}

	void done() override
	{
		LLIsType is_landmark(LLAssetType::AT_LANDMARK);
		LLIsType is_card(LLAssetType::AT_CALLINGCARD);

		LLInventoryModel::cat_array_t	card_cats;
		LLInventoryModel::item_array_t	card_items;
		LLInventoryModel::cat_array_t	land_cats;
		LLInventoryModel::item_array_t	land_items;

		uuid_vec_t::iterator it = mComplete.begin();
		uuid_vec_t::iterator end = mComplete.end();
		for (; it != end; ++it)
		{
			gInventory.collectDescendentsIf(
				(*it),
				land_cats,
				land_items,
				LLInventoryModel::EXCLUDE_TRASH,
				is_landmark);
			gInventory.collectDescendentsIf(
				(*it),
				card_cats,
				card_items,
				LLInventoryModel::EXCLUDE_TRASH,
				is_card);
		}
		LLSD args;
		if (!land_items.empty())
		{	// Show notification that they can now teleport to landmarks.  Use a random landmark from the inventory
			S32 random_land = ll_rand(land_items.size() - 1);
			args["NAME"] = land_items[random_land]->getName();
			LLNotificationsUtil::add("TeleportToLandmark",args);
		}
		if (!card_items.empty())
		{	// Show notification that they can now contact people.  Use a random calling card from the inventory
			S32 random_card = ll_rand(card_items.size() - 1);
			args["NAME"] = card_items[random_card]->getName();
			LLNotificationsUtil::add("TeleportToPerson",args);
		}

		gInventory.removeObserver(this);
		delete this;
	}
};


class LLPostTeleportNotifiers final : public LLEventTimer 
{
public:
	LLPostTeleportNotifiers();
	virtual ~LLPostTeleportNotifiers();

	//function to be called at the supplied frequency
	BOOL tick() override;
};

LLPostTeleportNotifiers::LLPostTeleportNotifiers() : LLEventTimer(2.0)
{
};

LLPostTeleportNotifiers::~LLPostTeleportNotifiers()
{
}

BOOL LLPostTeleportNotifiers::tick()
{
	BOOL all_done = FALSE;
	if (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
	{
		// get callingcards and landmarks available to the user arriving.
		uuid_vec_t folders;
		const LLUUID callingcard_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		if (callingcard_id.notNull())
			folders.push_back(callingcard_id);
		const LLUUID folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
		if (folder_id.notNull())
			folders.push_back(folder_id);
		if (!folders.empty())
		{
			LLFetchInWelcomeArea* fetcher = new LLFetchInWelcomeArea(folders);
			fetcher->startFetch();
			if (fetcher->isFinished())
			{
				fetcher->done();
			}
			else
			{
				gInventory.addObserver(fetcher);
			}
		}
		all_done = TRUE;
	}

	return all_done;
}


// Teleport notification from the simulator
// We're going to pretend to be a new agent
void process_teleport_finish(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("Messaging") << "Got teleport location message" << LL_ENDL;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Got teleport notification for wrong agent!" << LL_ENDL;
		return;
	}
	
	// Teleport is finished; it can't be cancelled now.
	gViewerWindow->setProgressCancelButtonVisible(FALSE);

	//gPipeline.doResetVertexBuffers(true); // Animesh+

	// Do teleport effect for where you're leaving
	// VEFFECT: TeleportStart
	LLHUDEffectSpiral* effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINT, TRUE);
	effectp->setPositionGlobal(gAgent.getPositionGlobal());
	effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	LLHUDManager::getInstance()->sendEffects();

	U32 location_id;
	U32 sim_ip;
	U16 sim_port;
	LLVector3 pos, look_at;
	U64 region_handle;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	msg->getIPAddrFast(_PREHASH_Info, _PREHASH_SimIP, sim_ip);
	msg->getIPPortFast(_PREHASH_Info, _PREHASH_SimPort, sim_port);
	//msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	//msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
	msg->getU64Fast(_PREHASH_Info, _PREHASH_RegionHandle, region_handle);
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);


	std::string seedCap;
	msg->getStringFast(_PREHASH_Info, _PREHASH_SeedCapability, seedCap);

	// update home location if we are teleporting out of prelude - specific to teleporting to welcome area 
	if ((teleport_flags & TELEPORT_FLAGS_SET_HOME_TO_TARGET)
	   && (!gAgent.isGodlike()))
	{
		gAgent.setHomePosRegion(region_handle, pos);

		// Create a timer that will send notices when teleporting is all finished.  Since this is 
		// based on the LLEventTimer class, it will be managed by that class and not orphaned or leaked.
		new LLPostTeleportNotifiers();
	}

	LLHost sim_host(sim_ip, sim_port);

	// Viewer trusts the simulator.
	gMessageSystem->enableCircuit(sim_host, TRUE);
	if (!gHippoGridManager->getConnectedGrid()->isSecondLife())
	{
		U32 region_size_x = 256;
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeX, region_size_x);
		U32 region_size_y = 256;
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeY, region_size_y);
		LLWorld::getInstance()->setRegionSize(region_size_x, region_size_y);
	}
	LLViewerRegion* regionp =  LLWorld::getInstance()->addRegion(region_handle, sim_host);

	M7WindlightInterface::getInstance()->receiveReset();

	/*
	// send camera update to new region
	gAgentCamera.updateCamera();

	// likewise make sure the camera is behind the avatar
	gAgentCamera.resetView(TRUE);
	LLVector3 shift_vector = regionp->getPosRegionFromGlobal(gAgent.getRegion()->getOriginGlobal());
	gAgent.setRegion(regionp);
	gObjectList.shiftObjects(shift_vector);

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->clearChatText();
		gAgentCamera.slamLookAt(look_at);
	}
	gAgent.setPositionAgent(pos);
	gAssetStorage->setUpstream(sim);
	gCacheName->setUpstream(sim);
*/

	// Make sure we're standing
	gAgent.standUp();

	// now, use the circuit info to tell simulator about us!
	LL_INFOS("Messaging") << "process_teleport_finish() Enabling "
			<< sim_host << " with code " << msg->mOurCircuitCode << LL_ENDL;
	msg->newMessageFast(_PREHASH_UseCircuitCode);
	msg->nextBlockFast(_PREHASH_CircuitCode);
	msg->addU32Fast(_PREHASH_Code, msg->getOurCircuitCode());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_ID, gAgent.getID());
	msg->sendReliable(sim_host);

	send_complete_agent_movement(sim_host);
	gAgent.setTeleportState(LLAgent::TELEPORT_MOVING);
	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["contacting"]);

	LL_DEBUGS("CrossingCaps") << "Calling setSeedCapability from process_teleport_finish(). Seed cap == "
		<< seedCap << LL_ENDL;
	regionp->setSeedCapability(seedCap);

	// Don't send camera updates to the new region until we're
	// actually there...


	// Now do teleport effect for where you're going.
	// VEFFECT: TeleportEnd
	effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINT, TRUE);
	effectp->setPositionGlobal(gAgent.getPositionGlobal());

	effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	LLHUDManager::getInstance()->sendEffects();

	//	gTeleportDisplay = TRUE;
	//	gTeleportDisplayTimer.reset();
	//	gViewerWindow->setShowProgress(TRUE);
}

// stuff we have to do every time we get an AvatarInitComplete from a sim
/*
void process_avatar_init_complete(LLMessageSystem* msg, void**)
{
	LLVector3 agent_pos;
	msg->getVector3Fast(_PREHASH_AvatarData, _PREHASH_Position, agent_pos);
	agent_movement_complete(msg->getSender(), agent_pos);
}
*/

void process_agent_movement_complete(LLMessageSystem* msg, void**)
{
	gShiftFrame = true;
	gAgentMovementCompleted = true;

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if ((gAgent.getID() != agent_id) || (gAgent.getSessionID() != session_id))
	{
		LL_WARNS("Messaging") << "Incorrect id in process_agent_movement_complete()"
				<< LL_ENDL;
		return;
	}

	LL_DEBUGS("Messaging") << "process_agent_movement_complete()" << LL_ENDL;

	// *TODO: check timestamp to make sure the movement completion
	// makes sense.
	LLVector3 agent_pos;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_Position, agent_pos);
	LLVector3 look_at;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_LookAt, look_at);
	U64 region_handle;
	msg->getU64Fast(_PREHASH_Data, _PREHASH_RegionHandle, region_handle);
	
	std::string version_channel;
	msg->getString("SimData", "ChannelVersion", version_channel);

	if (!isAgentAvatarValid())
	{
		// Could happen if you were immediately god-teleported away on login,
		// maybe other cases.  Continue, but warn.
		LL_WARNS("Messaging") << "agent_movement_complete() with NULL avatarp." << LL_ENDL;
	}

	F32 x, y;
	from_region_handle(region_handle, &x, &y);
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegionFromHandle(region_handle);
	if (!regionp)
	{
		if (gAgent.getRegion())
		{
			LL_WARNS("Messaging") << "current region " << gAgent.getRegion()->getOriginGlobal() << LL_ENDL;
		}

		LL_WARNS("Messaging") << "Agent being sent to invalid home region: " 
			<< x << ":" << y 
			<< " current pos " << gAgent.getPositionGlobal()
			<< LL_ENDL;
		LLAppViewer::instance()->forceDisconnect(LLTrans::getString("SentToInvalidRegion"));
		return;
	}

	LL_INFOS("Messaging") << "Changing home region to " << x << ":" << y << LL_ENDL;

	// set our upstream host the new simulator and shuffle things as
	// appropriate.
	LLVector3 shift_vector = regionp->getPosRegionFromGlobal(
		gAgent.getRegion()->getOriginGlobal());
	gAgent.setRegion(regionp);
	gObjectList.shiftObjects(shift_vector);
	// Is this a really long jump?
	if (shift_vector.length() > 2048.f * 256.f)
	{
		regionp->reInitPartitions();
		gAgent.setRegion(regionp);
		// Kill objects in the regions we left behind
		for (auto r : LLWorld::getInstance()->getRegionList())
		{
			if (r != regionp)
			{
				gObjectList.killObjects(r);
			}
		}
	}
	gAssetStorage->setUpstream(msg->getSender());
	gCacheName->setUpstream(msg->getSender());
	gViewerThrottle.sendToSim();
	gViewerWindow->sendShapeToSim();

	bool is_teleport = gAgent.getTeleportState() == LLAgent::TELEPORT_MOVING;

	if (is_teleport)
	{
		if (gAgent.getTeleportKeepsLookAt())
		{
			// *NOTE: the LookAt data we get from the sim here doesn't
			// seem to be useful, so get it from the camera instead
			look_at = LLViewerCamera::getInstance()->getAtAxis();
		}
		// Force the camera back onto the agent, don't animate.
		gAgentCamera.setFocusOnAvatar(TRUE, FALSE);
		gAgentCamera.slamLookAt(look_at);
		gAgentCamera.updateCamera();

		gAgent.setTeleportState(LLAgent::TELEPORT_START_ARRIVAL);

		// set the appearance on teleport since the new sim does not
		// know what you look like.
		gAgent.sendAgentSetAppearance();

		if (isAgentAvatarValid())
		{
			// Chat the "back" SLURL. (DEV-4907)
			LLSLURL slurl;
			gAgent.getTeleportSourceSLURL(slurl);
			LLChat chat(LLTrans::getString("completed_from") + " " + slurl.getSLURLString());
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::addChatHistory(chat);

			// Set the new position
			gAgentAvatarp->setPositionAgent(agent_pos);
			gAgentAvatarp->clearChat();
			gAgentAvatarp->slamPosition();
		}

		// add teleport destination to the list of visited places
		LLFloaterTeleportHistory::getInstance()->addPendingEntry(regionp->getName(), (S16)agent_pos.mV[VX], (S16)agent_pos.mV[VY], (S16)agent_pos.mV[VZ]);
	}
	else
	{
		// This is initial log-in or a region crossing
		gAgent.setTeleportState(LLAgent::TELEPORT_NONE);

		if (LLStartUp::getStartupState() < STATE_STARTED)
		{	// This is initial log-in, not a region crossing:
			// Set the camera looking ahead of the AV so send_agent_update() below 
			// will report the correct location to the server.
			LLVector3 look_at_point = look_at;
			look_at_point = agent_pos + look_at_point.rotVec(gAgent.getQuat());

			static LLVector3 up_direction(0.0f, 0.0f, 1.0f);
			LLViewerCamera::getInstance()->lookAt(agent_pos, look_at_point, up_direction);
		}
	}

	if (LLTracker::isTracking())
	{
		// Check distance to beacon, if < 5m, remove beacon
		LLVector3d beacon_pos = LLTracker::getTrackedPositionGlobal();
		LLVector3 beacon_dir(agent_pos.mV[VX] - (F32)fmod(beacon_pos.mdV[VX], 256.0), agent_pos.mV[VY] - (F32)fmod(beacon_pos.mdV[VY], 256.0), 0);
		if (beacon_dir.magVecSquared() < 25.f)
		{
			LLTracker::stopTracking(false);
		}
		else if (is_teleport)
		{
			if (!gAgent.getTeleportKeepsLookAt() && look_at.isExactlyZero())
			{
				//look at the beacon
				LLVector3 global_agent_pos = agent_pos;
				global_agent_pos[0] += x;
				global_agent_pos[1] += y;
				look_at = (LLVector3)beacon_pos - global_agent_pos;
				look_at.normVec();
				gAgentCamera.slamLookAt(look_at);
			}
			if (gSavedSettings.getBOOL("ClearBeaconAfterTeleport"))
			{
				LLTracker::stopTracking(false);
			}
		}
	}

	// TODO: Put back a check for flying status! DK 12/19/05
	// Sim tells us whether the new position is off the ground
	/*
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING)
	{
		gAgent.setFlying(TRUE);
	}
	else
	{
		gAgent.setFlying(FALSE);
	}
	*/

	send_agent_update(TRUE, TRUE);

	if (gAgent.getRegion()->getBlockFly())
	{
		gAgent.setFlying(false/*gAgent.canFly()*/);
	}

	// force simulator to recognize do not disturb state

	gAgent.setDoNotDisturb(gAgent.isDoNotDisturb());

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->mFootPlane.clearVec();
	}
	
	// send walk-vs-run status
//	gAgent.sendWalkRun(gAgent.getRunning() || gAgent.getAlwaysRun());
// [RLVa:KB] - Checked: 2011-05-11 (RLVa-1.3.0i) | Added: RLVa-1.3.0i
	gAgent.sendWalkRun();
// [/RLVa:KB]

	// If the server version has changed, display an info box and offer
	// to display the release notes, unless this is the initial log in.
	if (gLastVersionChannel == version_channel)
	{
		return;
	}

	if (!gLastVersionChannel.empty() && gSavedSettings.getBOOL("SGServerVersionChangedNotification"))
	{
		LLSD args;
		args["OLD_VERSION"] = gLastVersionChannel;
		args["NEW_VERSION"] = version_channel;
		LLNotificationsUtil::add("ServerVersionChanged", args);
	}

	gLastVersionChannel = version_channel;
}

void process_crossed_region(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if ((gAgent.getID() != agent_id) || (gAgent.getSessionID() != session_id))
	{
		LL_WARNS("Messaging") << "Incorrect id in process_crossed_region()"
				<< LL_ENDL;
		return;
	}
	LL_INFOS("Messaging") << "process_crossed_region()" << LL_ENDL;
	gAgentAvatarp->resetRegionCrossingTimer();
	gAgent.setIsCrossingRegion(false); // Attachments getting lost on TP, region crossing hook


	U32 sim_ip;
	msg->getIPAddrFast(_PREHASH_RegionData, _PREHASH_SimIP, sim_ip);
	U16 sim_port;
	msg->getIPPortFast(_PREHASH_RegionData, _PREHASH_SimPort, sim_port);
	LLHost sim_host(sim_ip, sim_port);
	U64 region_handle;
	msg->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	
	std::string seedCap;
	msg->getStringFast(_PREHASH_RegionData, _PREHASH_SeedCapability, seedCap);

	send_complete_agent_movement(sim_host);

	if (!gHippoGridManager->getConnectedGrid()->isSecondLife())
	{
		U32 region_size_x = 256;
		msg->getU32(_PREHASH_RegionData, _PREHASH_RegionSizeX, region_size_x);
		U32 region_size_y = 256;
		msg->getU32(_PREHASH_RegionData, _PREHASH_RegionSizeY, region_size_y);
		LLWorld::getInstance()->setRegionSize(region_size_x, region_size_y);
	}
	LLViewerRegion* regionp = LLWorld::getInstance()->addRegion(region_handle, sim_host);

	LL_DEBUGS("CrossingCaps") << "Calling setSeedCapability from process_crossed_region(). Seed cap == "
		<< seedCap << LL_ENDL;
	regionp->setSeedCapability(seedCap);
}


// Sends avatar and camera information to simulator.
// Sent roughly once per frame, or 20 times per second, whichever is less often

const F32 THRESHOLD_HEAD_ROT_QDOT = 0.9997f;	// ~= 2.5 degrees -- if its less than this we need to update head_rot
const F32 MAX_HEAD_ROT_QDOT = 0.99999f;			// ~= 0.5 degrees -- if its greater than this then no need to update head_rot
												// between these values we delay the updates (but no more than one second)

static LLTrace::BlockTimerStatHandle FTM_AGENT_UPDATE_SEND("Send Message");

void send_agent_update(BOOL force_send, BOOL send_reliable)
{
	if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
	{
		// We don't care if they want to send an agent update, they're not allowed to until the simulator
		// that's the target is ready to receive them (after avatar_init_complete is received)
		return;
	}

	// We have already requested to log out.  Don't send agent updates.
	if (LLAppViewer::instance()->logoutRequestSent())
	{
		return;
	}

	// no region to send update to
	if (gAgent.getRegion() == nullptr)
	{
		return;
	}

	const F32 TRANSLATE_THRESHOLD = 0.01f;

	// NOTA BENE: This is (intentionally?) using the small angle sine approximation to test for rotation
	//			  Plus, there is an extra 0.5 in the mix since the perpendicular between last_camera_at and getAtAxis() bisects cam_rot_change
	//			  Thus, we're actually testing against 0.2 degrees
	const F32 ROTATION_THRESHOLD = 0.1f * 2.f * F_PI / 360.f; //  Rotation thresh 0.2 deg, see note above

	const U8 DUP_MSGS = 1;				//  HACK!  number of times to repeat data on motionless agent

	//  Store data on last sent update so that if no changes, no send
	static LLVector3 last_camera_pos_agent, 
					 last_camera_at, 
					 last_camera_left,
					 last_camera_up;
	
	static LLVector3 cam_center_chg,
					 cam_rot_chg;

	static LLQuaternion last_head_rot;
	static U32 last_control_flags = 0;
	static U8 last_render_state;
	static U8 duplicate_count = 0;
	static F32 head_rot_chg = 1.0;
	static U8 last_flags;

	LLMessageSystem* msg = gMessageSystem;
	LLVector3		camera_pos_agent;				// local to avatar's region
	U8				render_state;

	LLQuaternion body_rotation = gAgent.getFrameAgent().getQuaternion();
	LLQuaternion head_rotation = gAgent.getHeadRotation();

	camera_pos_agent = gAgentCamera.getCameraPositionAgent();

	render_state = gAgent.getRenderState();

	U32		control_flag_change = 0;
	U8		flag_change = 0;

	cam_center_chg = last_camera_pos_agent - camera_pos_agent;
	cam_rot_chg = last_camera_at - LLViewerCamera::getInstance()->getAtAxis();

	// If a modifier key is held down, turn off
	// LBUTTON and ML_LBUTTON so that using the camera (alt-key) doesn't
	// trigger a control event.
	U32 control_flags = gAgent.getControlFlags();

	// <alchemy>
	static LLCachedControl<bool> alchemyPrejump(gSavedSettings, "Nimble", false);
	if (alchemyPrejump)
	{
		control_flags |= AGENT_CONTROL_FINISH_ANIM;
	}
	// </alchemy>

	MASK	key_mask = gKeyboard->currentMask(TRUE);

	if (key_mask & MASK_ALT || key_mask & MASK_CONTROL)
	{
		control_flags &= ~(AGENT_CONTROL_LBUTTON_DOWN |
			AGENT_CONTROL_ML_LBUTTON_DOWN);
		control_flags |= 	AGENT_CONTROL_LBUTTON_UP |
			AGENT_CONTROL_ML_LBUTTON_UP;
	}

	control_flag_change = last_control_flags ^ control_flags;

	U8 flags = AU_FLAGS_NONE;
	if (gAgent.isGroupTitleHidden())
	{
		flags |= AU_FLAGS_HIDETITLE;
	}

	flag_change = last_flags ^ flags;

	head_rot_chg = dot(last_head_rot, head_rotation);

	//static S32 msg_number = 0;		// Used for diagnostic log messages

	if (force_send || 
		(cam_center_chg.magVec() > TRANSLATE_THRESHOLD) || 
		(head_rot_chg < THRESHOLD_HEAD_ROT_QDOT) ||	
		(last_render_state != render_state) ||
		(cam_rot_chg.magVec() > ROTATION_THRESHOLD) ||
		control_flag_change != 0 ||
		flag_change != 0)  
	{
		/* Diagnotics to show why we send the AgentUpdate message.  Also un-commment the msg_number code above and below this block
		msg_number += 1;
		if (head_rot_chg < THRESHOLD_HEAD_ROT_QDOT)
		{
			//LL_INFOS("Messaging") << "head rot " << head_rotation << LL_ENDL;
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", head_rot_chg " << head_rot_chg << LL_ENDL;
		}
		if (cam_rot_chg.magVec() > ROTATION_THRESHOLD) 
		{
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", cam rot " <<  cam_rot_chg.magVec() << LL_ENDL;
		}
		if (cam_center_chg.magVec() > TRANSLATE_THRESHOLD)
		{
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", cam center " << cam_center_chg.magVec() << LL_ENDL;
		}
//		if (drag_delta_chg.magVec() > TRANSLATE_THRESHOLD)
//		{
//			LL_INFOS("Messaging") << "drag delta " << drag_delta_chg.magVec() << LL_ENDL;
//		}
		if (control_flag_change)
		{
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", dcf = " << control_flag_change << LL_ENDL;
		}
*/

		duplicate_count = 0;
	}
	else
	{
		duplicate_count++;

		if (head_rot_chg < MAX_HEAD_ROT_QDOT  &&  duplicate_count < AGENT_UPDATES_PER_SECOND)
		{
			// The head_rotation is sent for updating things like attached guns.
			// We only trigger a new update when head_rotation deviates beyond
			// some threshold from the last update, however this can break fine
			// adjustments when trying to aim an attached gun, so what we do here
			// (where we would normally skip sending an update when nothing has changed)
			// is gradually reduce the threshold to allow a better update to 
			// eventually get sent... should update to within 0.5 degrees in less 
			// than a second.
			if (head_rot_chg < THRESHOLD_HEAD_ROT_QDOT + (MAX_HEAD_ROT_QDOT - THRESHOLD_HEAD_ROT_QDOT) * duplicate_count / AGENT_UPDATES_PER_SECOND)
			{
				duplicate_count = 0;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	if (duplicate_count < DUP_MSGS && !gDisconnected)
	{
		/* More diagnostics to count AgentUpdate messages
		static S32 update_sec = 0;
		static S32 update_count = 0;
		static S32 max_update_count = 0;
		S32 cur_sec = lltrunc( LLTimer::getTotalSeconds() );
		update_count += 1;
		if (cur_sec != update_sec)
		{
			if (update_sec != 0)
			{
				update_sec = cur_sec;
				//msg_number = 0;
				max_update_count = llmax(max_update_count, update_count);
				LL_INFOS() << "Sent " << update_count << " AgentUpdate messages per second, max is " << max_update_count << LL_ENDL;
			}
			update_sec = cur_sec;
			update_count = 0;
		}
		*/

		LL_RECORD_BLOCK_TIME(FTM_AGENT_UPDATE_SEND);
		// Build the message
		msg->newMessageFast(_PREHASH_AgentUpdate);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->addQuatFast(_PREHASH_BodyRotation, body_rotation);
		msg->addQuatFast(_PREHASH_HeadRotation, head_rotation);
		msg->addU8Fast(_PREHASH_State, render_state);
		msg->addU8Fast(_PREHASH_Flags, flags);

		//		if (camera_pos_agent.mV[VY] > 255.f)
		//		{
		//			LL_INFOS("Messaging") << "Sending camera center " << camera_pos_agent << LL_ENDL;
		//		}
		
		msg->addVector3Fast(_PREHASH_CameraCenter, camera_pos_agent);
		msg->addVector3Fast(_PREHASH_CameraAtAxis, LLViewerCamera::getInstance()->getAtAxis());
		msg->addVector3Fast(_PREHASH_CameraLeftAxis, LLViewerCamera::getInstance()->getLeftAxis());
		msg->addVector3Fast(_PREHASH_CameraUpAxis, LLViewerCamera::getInstance()->getUpAxis());
		msg->addF32Fast(_PREHASH_Far, gAgentCamera.mDrawDistance);
		
		msg->addU32Fast(_PREHASH_ControlFlags, control_flags);

		if (gDebugClicks)
		{
			if (control_flags & AGENT_CONTROL_LBUTTON_DOWN)
			{
				LL_INFOS("Messaging") << "AgentUpdate left button down" << LL_ENDL;
			}

			if (control_flags & AGENT_CONTROL_LBUTTON_UP)
			{
				LL_INFOS("Messaging") << "AgentUpdate left button up" << LL_ENDL;
			}
		}

		gAgent.enableControlFlagReset();

		if (!send_reliable)
		{
			gAgent.sendMessage();
		}
		else
		{
			gAgent.sendReliableMessage();
		}

		//		LL_DEBUGS("Messaging") << "agent " << avatar_pos_agent << " cam " << camera_pos_agent << LL_ENDL;

		// Copy the old data 
		last_head_rot = head_rotation;
		last_render_state = render_state;
		last_camera_pos_agent = camera_pos_agent;
		last_camera_at = LLViewerCamera::getInstance()->getAtAxis();
		last_camera_left = LLViewerCamera::getInstance()->getLeftAxis();
		last_camera_up = LLViewerCamera::getInstance()->getUpAxis();
		last_control_flags = control_flags;
		last_flags = flags;
	}
}


// sounds can arrive before objects, store them for a short time
// Note: this is a workaround for MAINT-4743, real fix would be to make
// server send sound along with object update that creates (rezes) the object
class PostponedSoundData
{
public:
    PostponedSoundData() :
        mExpirationTime(0)
    {}
    PostponedSoundData(const LLUUID &object_id, const LLUUID &sound_id, const LLUUID& owner_id, const F32 gain, const U8 flags);
    bool hasExpired() { return LLFrameTimer::getTotalSeconds() > mExpirationTime; }

    LLUUID mObjectId;
    LLUUID mSoundId;
    LLUUID mOwnerId;
    F32 mGain;
    U8 mFlags;
    static const F64 MAXIMUM_PLAY_DELAY;

private:
    F64 mExpirationTime; //seconds since epoch
};
const F64 PostponedSoundData::MAXIMUM_PLAY_DELAY = 15.0;
static F64 postponed_sounds_update_expiration = 0.0;
static std::map<LLUUID, PostponedSoundData> postponed_sounds;

void set_attached_sound(LLViewerObject *objectp, const LLUUID &object_id, const LLUUID &sound_id, const LLUUID& owner_id, const F32 gain, const U8 flags)
{
    if (LLMuteList::getInstance()->isMuted(object_id)) return;

    if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;

    // Don't play sounds from a region with maturity above current agent maturity
    LLVector3d pos = objectp->getPositionGlobal();
    if (!gAgent.canAccessMaturityAtGlobal(pos))
    {
        return;
    }

    objectp->setAttachedSound(sound_id, owner_id, gain, flags);
}

PostponedSoundData::PostponedSoundData(const LLUUID &object_id, const LLUUID &sound_id, const LLUUID& owner_id, const F32 gain, const U8 flags)
    :
    mObjectId(object_id),
    mSoundId(sound_id),
    mOwnerId(owner_id),
    mGain(gain),
    mFlags(flags),
    mExpirationTime(LLFrameTimer::getTotalSeconds() + MAXIMUM_PLAY_DELAY)
{
}

// static
void update_attached_sounds()
{
    if (postponed_sounds.empty())
    {
        return;
    }

    std::map<LLUUID, PostponedSoundData>::iterator iter = postponed_sounds.begin();
    std::map<LLUUID, PostponedSoundData>::iterator end = postponed_sounds.end();
    while (iter != end)
    {
        std::map<LLUUID, PostponedSoundData>::iterator cur_iter = iter++;
        PostponedSoundData* data = &cur_iter->second;
        if (data->hasExpired())
        {
            postponed_sounds.erase(cur_iter);
        }
        else
        {
            LLViewerObject *objectp = gObjectList.findObject(data->mObjectId);
            if (objectp)
            {
                set_attached_sound(objectp, data->mObjectId, data->mSoundId, data->mOwnerId, data->mGain, data->mFlags);
                postponed_sounds.erase(cur_iter);
            }
        }
    }
    postponed_sounds_update_expiration = LLFrameTimer::getTotalSeconds() + 2 * PostponedSoundData::MAXIMUM_PLAY_DELAY;
}

//static
void clear_expired_postponed_sounds()
{
    if (postponed_sounds_update_expiration > LLFrameTimer::getTotalSeconds())
    {
        return;
    }
    std::map<LLUUID, PostponedSoundData>::iterator iter = postponed_sounds.begin();
    std::map<LLUUID, PostponedSoundData>::iterator end = postponed_sounds.end();
    while (iter != end)
    {
        std::map<LLUUID, PostponedSoundData>::iterator cur_iter = iter++;
        PostponedSoundData* data = &cur_iter->second;
        if (data->hasExpired())
        {
            postponed_sounds.erase(cur_iter);
        }
    }
    postponed_sounds_update_expiration = LLFrameTimer::getTotalSeconds() + 2 * PostponedSoundData::MAXIMUM_PLAY_DELAY;
}

// *TODO: Remove this dependency, or figure out a better way to handle
// this hack.
extern U32Bits gObjectData;

void process_object_update(LLMessageSystem* mesgsys, void** user_data)
{	
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += U32Bytes(mesgsys->getReceiveCompressedSize());
	}
	else
	{
		gObjectData += U32Bytes(mesgsys->getReceiveSize());
	}

	// Update the object...
	S32 old_num_objects = gObjectList.mNumNewObjects;
	gObjectList.processObjectUpdate(mesgsys, user_data, OUT_FULL);
	if (old_num_objects != gObjectList.mNumNewObjects)
	{
		update_attached_sounds();
	}
}

void process_compressed_object_update(LLMessageSystem* mesgsys, void** user_data)
{
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += U32Bytes(mesgsys->getReceiveCompressedSize());
	}
	else
	{
		gObjectData += U32Bytes(mesgsys->getReceiveSize());
	}

	// Update the object...
	S32 old_num_objects = gObjectList.mNumNewObjects;
	gObjectList.processCompressedObjectUpdate(mesgsys, user_data, OUT_FULL_COMPRESSED);
	if (old_num_objects != gObjectList.mNumNewObjects)
	{
		update_attached_sounds();
	}
}

void process_cached_object_update(LLMessageSystem* mesgsys, void** user_data)
{
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += U32Bytes(mesgsys->getReceiveCompressedSize());
	}
	else
	{
		gObjectData += U32Bytes(mesgsys->getReceiveSize());
	}

	// Update the object...
	gObjectList.processCachedObjectUpdate(mesgsys, user_data, OUT_FULL_CACHED);
}


void process_terse_object_update_improved(LLMessageSystem* mesgsys, void** user_data)
{
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += U32Bytes(mesgsys->getReceiveCompressedSize());
	}
	else
	{
		gObjectData += U32Bytes(mesgsys->getReceiveSize());
	}

	S32 old_num_objects = gObjectList.mNumNewObjects;
	gObjectList.processCompressedObjectUpdate(mesgsys, user_data, OUT_TERSE_IMPROVED);
	if (old_num_objects != gObjectList.mNumNewObjects)
	{
		update_attached_sounds();
	}
}

static LLTrace::BlockTimerStatHandle FTM_PROCESS_OBJECTS("Process Kill Objects");

void process_kill_object(LLMessageSystem* mesgsys, void** user_data)
{
	LL_RECORD_BLOCK_TIME(FTM_PROCESS_OBJECTS);

	auto agent_region = gAgent.getRegion();
	if (!agent_region) return;

	LLUUID		id;

	U32 ip = mesgsys->getSenderIP();
	U32 port = mesgsys->getSenderPort();

	bool different_region = mesgsys->getSender().getIPandPort() != agent_region->getHost().getIPandPort();

	S32 num_objects = mesgsys->getNumberOfBlocksFast(_PREHASH_ObjectData);
	for (S32 i = 0; i < num_objects; ++i)
	{
		U32 local_id;
		mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);

		LLViewerObjectList::getUUIDFromLocal(id, local_id, ip, port);
		if (id == LLUUID::null)
		{
			LL_DEBUGS("Messaging") << "Unknown kill for local " << local_id << LL_ENDL;
			++gObjectList.mNumUnknownKills;
			continue;
		}
		else
		{
			LL_DEBUGS("Messaging") << "Kill message for local " << local_id << LL_ENDL;
		}

		if (id == gAgentID)
		{
			// never kill our avatar
			continue;
		}

		LLViewerObject* objectp = gObjectList.findObject(id);
		if (objectp)
		{
			if (gAgentAvatarp == objectp->getAvatar())
			{
				if (different_region)
				{
					LL_WARNS() << "Region other than our own killing our attachments!!" << LL_ENDL;
					continue;
				}
				else if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
				{
					LL_WARNS() << "Region killing our attachments during teleport!!" << LL_ENDL;
					continue;
				}
				else if (gAgent.isCrossingRegion())
				{
					LL_WARNS() << "Region killing our attachments during region cross!!" << LL_ENDL;
					continue;
				}
			}

			// Display green bubble on kill
			if (gShowObjectUpdates)
			{
				LLColor4 color(0.f, 1.f, 0.f, 1.f);
				gPipeline.addDebugBlip(objectp->getPositionAgent(), color);
			}

			// Do the kill
			gObjectList.killObject(objectp);
		}
		else
		{
			LL_WARNS("Messaging") << "Object in UUID lookup, but not on object list in kill!" << LL_ENDL;
			gObjectList.mNumUnknownKills++;
		}

		// We should remove the object from selection after it is marked dead by gObjectList to make LLToolGrab,
        // which is using the object, release the mouse capture correctly when the object dies.
        // See LLToolGrab::handleHoverActive() and LLToolGrab::handleHoverNonPhysical().
		LLSelectMgr::getInstance()->removeObjectFromSelections(id);
	}
}

void process_time_synch(LLMessageSystem* mesgsys, void** user_data)
{
	LLVector3 sun_direction;
	LLVector3 sun_ang_velocity;
	F32 phase;
	U64	space_time_usec;

    U32 seconds_per_day;
    U32 seconds_per_year;

	// "SimulatorViewerTimeMessage"
	mesgsys->getU64Fast(_PREHASH_TimeInfo, _PREHASH_UsecSinceStart, space_time_usec);
	mesgsys->getU32Fast(_PREHASH_TimeInfo, _PREHASH_SecPerDay, seconds_per_day);
	mesgsys->getU32Fast(_PREHASH_TimeInfo, _PREHASH_SecPerYear, seconds_per_year);

	// This should eventually be moved to an "UpdateHeavenlyBodies" message
	mesgsys->getF32Fast(_PREHASH_TimeInfo, _PREHASH_SunPhase, phase);
	mesgsys->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunDirection, sun_direction);
	mesgsys->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunAngVelocity, sun_ang_velocity);

	LLWorld::getInstance()->setSpaceTimeUSec(space_time_usec);

	LL_DEBUGS("WindlightSync") << "Sun phase: " << phase << " rad = " << fmodf(phase / F_TWO_PI + 0.25, 1.f) * 24.f << " h" << LL_ENDL;

	gSky.setSunPhase(phase);
	gSky.setSunTargetDirection(sun_direction, sun_ang_velocity);
	if (!gNoRender && !(gSavedSettings.getBOOL("SkyOverrideSimSunPosition") || gSky.getOverrideSun()))
	{
		gSky.setSunDirection(sun_direction, sun_ang_velocity);
	}
}

void process_sound_trigger(LLMessageSystem* msg, void**)
{
	if (!gAudiop)
	{
#if !LL_LINUX
		LL_WARNS("AudioEngine") << "LLAudioEngine instance doesn't exist!" << LL_ENDL;
#endif
		return;
	}

	U64		region_handle = 0;
	F32		gain = 0;
	LLUUID	sound_id;
	LLUUID	owner_id;
	LLUUID	object_id;
	LLUUID	parent_id;
	LLVector3	pos_local;

	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_OwnerID, owner_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ObjectID, object_id);

	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instanceExists())
	{
		auto& antispam = NACLAntiSpamRegistry::instance();
		static const LLCachedControl<U32> _NACL_AntiSpamSoundMulti("_NACL_AntiSpamSoundMulti");
		if (owner_id.isNull())
		{
			bool is_collision_sound(const std::string & sound);
			if (!is_collision_sound(sound_id.asString())
				&& antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SOUND, object_id, LFIDBearer::OBJECT, _NACL_AntiSpamSoundMulti))
				return;
		}
		else if (antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SOUND, owner_id, LFIDBearer::AVATAR, _NACL_AntiSpamSoundMulti)) return;
	}
	// NaCl End

	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ParentID, parent_id);
	msg->getU64Fast(_PREHASH_SoundData, _PREHASH_Handle, region_handle);
	msg->getVector3Fast(_PREHASH_SoundData, _PREHASH_Position, pos_local);
	msg->getF32Fast(_PREHASH_SoundData, _PREHASH_Gain, gain);

	// adjust sound location to true global coords
	LLVector3d	pos_global = from_region_handle(region_handle);
	pos_global.mdV[VX] += pos_local.mV[VX];
	pos_global.mdV[VY] += pos_local.mV[VY];
	pos_global.mdV[VZ] += pos_local.mV[VZ];

	// Don't play a trigger sound if you can't hear it due
	// to parcel "local audio only" settings.
	if (!LLViewerParcelMgr::getInstance()->canHearSound(pos_global)) return;

	// Don't play sounds triggered by someone you muted.
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;
	
	// Don't play sounds from an object you muted
	if (LLMuteList::getInstance()->isMuted(object_id)) return;

	// Don't play sounds from an object whose parent you muted
	if (parent_id.notNull()
		&& LLMuteList::getInstance()->isMuted(parent_id))
	{
		return;
	}

	// Don't play sounds from a region with maturity above current agent maturity
	if (!gAgent.canAccessMaturityInRegion(region_handle))
	{
		return;
	}

	// Don't play sounds from gestures if they are not enabled.
	if (object_id == owner_id)
	{
		if (!gSavedSettings.getBOOL("EnableGestureSounds"))
		{
			// Don't mute own gestures, if they're not muted.
			if (owner_id != gAgentID || !gSavedSettings.getBOOL("EnableGestureSoundsSelf"))
				return;
		}
	}
	else if (!gSavedSettings.getBOOL("EnableNongestureSounds"))
	{
		// Don't mute own non-gestures, if they're not muted.
		if (owner_id != gAgentID || !gSavedSettings.getBOOL("EnableNongestureSoundsSelf"))
			return;
	}

	// <edit>
	//gAudiop->triggerSound(sound_id, owner_id, gain, LLAudioEngine::AUDIO_TYPE_SFX, pos_global);
	gAudiop->triggerSound(sound_id, owner_id, gain, LLAudioEngine::AUDIO_TYPE_SFX, pos_global, object_id);
	// </edit>
}

void process_preload_sound(LLMessageSystem* msg, void** user_data)
{
	if (!gAudiop)
	{
#if !LL_LINUX
		LL_WARNS("AudioEngine") << "LLAudioEngine instance doesn't exist!" << LL_ENDL;
#endif
		return;
	}

	LLUUID sound_id;
	LLUUID object_id;
	LLUUID owner_id;

	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);

	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instanceExists())
	{
		auto& antispam = NACLAntiSpamRegistry::instance();
		static const LLCachedControl<U32> _NACL_AntiSpamSoundPreloadMulti("_NACL_AntiSpamSoundPreloadMulti");
		if ((owner_id.isNull()
			&& antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SOUND_PRELOAD, object_id, LFIDBearer::OBJECT, _NACL_AntiSpamSoundPreloadMulti))
			|| antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SOUND_PRELOAD, owner_id, LFIDBearer::AVATAR, _NACL_AntiSpamSoundPreloadMulti))
			return;
	}
	// NaCl End

	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (!objectp) return;

	if (LLMuteList::getInstance()->isMuted(object_id)) return;
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;
	
	LLAudioSource* sourcep = objectp->getAudioSource(owner_id);
	if (!sourcep) return;
	
	// Note that I don't actually do any loading of the
	// audio data into a buffer at this point, as it won't actually
	// help us out.

	// Don't play sounds from a region with maturity above current agent maturity
	LLVector3d pos_global = objectp->getPositionGlobal();
	if (gAgent.canAccessMaturityAtGlobal(pos_global))
	{
		// Preload starts a transfer internally.
		sourcep->preload(sound_id);
	}
}

void process_attached_sound(LLMessageSystem* msg, void** user_data)
{
	F32 gain = 0;
	LLUUID sound_id;
	LLUUID object_id;
	LLUUID owner_id;
	U8 flags;

	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);

	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instanceExists())
	{
		auto& antispam = NACLAntiSpamRegistry::instance();
		if ((owner_id.isNull()
			&& antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SOUND, object_id, LFIDBearer::OBJECT))
			|| antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SOUND, owner_id))
			return;
	}
	// NaCl End

	msg->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);
	msg->getU8Fast(_PREHASH_DataBlock, _PREHASH_Flags, flags);

	LLViewerObject *objectp = gObjectList.findObject(object_id);
	if (objectp)
	{
		set_attached_sound(objectp, object_id, sound_id, owner_id, gain, flags);
	}
	else if (sound_id.notNull())
	{
		// we don't know about this object yet, probably it has yet to arrive
		// std::map for dupplicate prevention.
		if (!LLMuteList::getInstance()->isMuted(object_id) && !LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds))
			postponed_sounds[object_id] = (PostponedSoundData(object_id, sound_id, owner_id, gain, flags));
		clear_expired_postponed_sounds();
	}
	else
	{
		std::map<LLUUID, PostponedSoundData>::iterator iter = postponed_sounds.find(object_id);
		if (iter != postponed_sounds.end())
		{
			postponed_sounds.erase(iter);
		}
	}
}

void process_attached_sound_gain_change(LLMessageSystem *mesgsys, void **user_data)
{
	F32 gain = 0;
	LLUUID object_guid;
	LLViewerObject* objectp = nullptr;

	mesgsys->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_guid);

	if (!((objectp = gObjectList.findObject(object_guid))))
	{
		// we don't know about this object, just bail
		return;
	}

 	mesgsys->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);

	objectp->adjustAudioGain(gain);
}


void process_health_message(LLMessageSystem* mesgsys, void** user_data)
{
	F32 health;

	mesgsys->getF32Fast(_PREHASH_HealthData, _PREHASH_Health, health);

	if (gStatusBar)
	{
		gStatusBar->setHealth((S32)health);
	}
}


void process_sim_stats(LLMessageSystem* msg, void** user_data)
{
	static LLHost sLastHost;
	auto& stats = LLViewerStats::instance();
	if (msg->getSender() != sLastHost)
	{
		sLastHost = msg->getSender();
		stats.mSimTimeDilation.reset();
		stats.mSimFPS.reset();
		stats.mSimPhysicsFPS.reset();
		stats.mSimAgentUPS.reset();
		stats.mSimFrameMsec.reset();
		stats.mSimNetMsec.reset();
		stats.mSimSimOtherMsec.reset();
		stats.mSimSimPhysicsMsec.reset();
		stats.mSimAgentMsec.reset();
		stats.mSimImagesMsec.reset();
		stats.mSimScriptMsec.reset();
		stats.mSimObjects.reset();
		stats.mSimActiveObjects.reset();
		stats.mSimMainAgents.reset();
		stats.mSimChildAgents.reset();
		stats.mSimActiveScripts.reset();
		stats.mSimScriptEPS.reset();
		stats.mSimInPPS.reset();
		stats.mSimOutPPS.reset();
		stats.mSimPendingDownloads.reset();
		stats.mSimPendingUploads.reset();
		stats.mSimPendingLocalUploads.reset();
		stats.mSimTotalUnackedBytes.reset();
		stats.mPhysicsPinnedTasks.reset();
		stats.mPhysicsLODTasks.reset();
		stats.mSimSimPhysicsStepMsec.reset();
		stats.mSimSimPhysicsShapeUpdateMsec.reset();
		stats.mSimSimPhysicsOtherMsec.reset();
		stats.mPhysicsMemoryAllocated.reset();
		stats.mSimSpareMsec.reset();
		stats.mSimSleepMsec.reset();
		stats.mSimPumpIOMsec.reset();
		stats.mSimPctScriptsRun.reset();
		stats.mSimSimAIStepMsec.reset();
		stats.mSimSimSkippedSilhouetteSteps.reset();
		stats.mSimSimPctSteppedCharacters.reset();
	}
	S32 count = msg->getNumberOfBlocks("Stat");
	for (S32 i = 0; i < count; ++i)
	{
		U32 stat_id;
		F32 stat_value;
		msg->getU32("Stat", "StatID", stat_id, i);
		msg->getF32("Stat", "StatValue", stat_value, i);
		switch (stat_id)
		{
		case LL_SIM_STAT_TIME_DILATION:
			stats.mSimTimeDilation.addValue(stat_value);
			break;
		case LL_SIM_STAT_FPS:
			stats.mSimFPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_PHYSFPS:
			stats.mSimPhysicsFPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_AGENTUPS:
			stats.mSimAgentUPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_FRAMEMS:
			stats.mSimFrameMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_NETMS:
			stats.mSimNetMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMOTHERMS:
			stats.mSimSimOtherMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSMS:
			stats.mSimSimPhysicsMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_AGENTMS:
			stats.mSimAgentMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_IMAGESMS:
			stats.mSimImagesMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SCRIPTMS:
			stats.mSimScriptMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMTASKS:
			stats.mSimObjects.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMTASKSACTIVE:
			stats.mSimActiveObjects.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMAGENTMAIN:
			stats.mSimMainAgents.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMAGENTCHILD:
			stats.mSimChildAgents.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMSCRIPTSACTIVE:
			if (gSavedSettings.getBOOL("AscentDisplayTotalScriptJumps"))
			{
				auto control = gSavedSettings.getControl("Ascentnumscripts");
				const auto& numscripts = control->get().asFloat();
				if(abs(stat_value-numscripts)>gSavedSettings.getF32("Ascentnumscriptdiff"))
				{
					std::stringstream os;
					os << (U32)numscripts << " to " << (U32)stat_value << " (";
					const S32 tdiff = stat_value - numscripts;
					if (tdiff > 0) os << "+";
					os << tdiff << ')';
					LLChat chat("Total scripts jumped from " + os.str());
					LLFloaterChat::addChat(chat, FALSE, FALSE);
				}
				control->set(stat_value);
			}
			stats.mSimActiveScripts.addValue(stat_value);
			break;
		case LL_SIM_STAT_SCRIPT_EPS:
			stats.mSimScriptEPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_INPPS:
			stats.mSimInPPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_OUTPPS:
			stats.mSimOutPPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_PENDING_DOWNLOADS:
			stats.mSimPendingDownloads.addValue(stat_value);
			break;
		case LL_SIM_STAT_PENDING_UPLOADS:
			stats.mSimPendingUploads.addValue(stat_value);
			break;
		case LL_SIM_STAT_PENDING_LOCAL_UPLOADS:
			stats.mSimPendingLocalUploads.addValue(stat_value);
			break;
		case LL_SIM_STAT_TOTAL_UNACKED_BYTES:
			stats.mSimTotalUnackedBytes.addValue(stat_value / 1024.f);
			break;
		case LL_SIM_STAT_PHYSICS_PINNED_TASKS:
			stats.mPhysicsPinnedTasks.addValue(stat_value);
			break;
		case LL_SIM_STAT_PHYSICS_LOD_TASKS:
			stats.mPhysicsLODTasks.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSSTEPMS:
			stats.mSimSimPhysicsStepMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSSHAPEMS:
			stats.mSimSimPhysicsShapeUpdateMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSOTHERMS:
			stats.mSimSimPhysicsOtherMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSMEMORY:
			stats.mPhysicsMemoryAllocated.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMSPARETIME:
			stats.mSimSpareMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMSLEEPTIME:
			stats.mSimSleepMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_IOPUMPTIME:
			stats.mSimPumpIOMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_PCTSCRIPTSRUN:
			stats.mSimPctScriptsRun.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMAISTEPTIMEMS:
			stats.mSimSimAIStepMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SKIPPEDAISILSTEPS_PS:
			stats.mSimSimSkippedSilhouetteSteps.addValue(stat_value);
			break;
		case LL_SIM_STAT_PCTSTEPPEDCHARACTERS:
			stats.mSimSimPctSteppedCharacters.addValue(stat_value);
			break;
		default:
			// Used to be a commented out warning.
 			LL_DEBUGS("Messaging") << "Unknown stat id" << stat_id << LL_ENDL;
		  break;
		}
	}

	//
	// Various hacks that aren't statistics, but are being handled here.
	//
	U32 max_tasks_per_region;
	U64 region_flags;
	msg->getU32("Region", "ObjectCapacity", max_tasks_per_region);

	if (msg->has(_PREHASH_RegionInfo))
	{
		msg->getU64("RegionInfo", "RegionFlagsExtended", region_flags);
	}
	else
	{
		U32 flags = 0;
		msg->getU32("Region", "RegionFlags", flags);
		region_flags = flags;
	}

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		BOOL was_flying = gAgent.getFlying();
		regionp->setRegionFlags(region_flags);
		regionp->setMaxTasks(max_tasks_per_region);
		// HACK: This makes agents drop from the sky if the region is 
		// set to no fly while people are still in the sim.
		if (was_flying && regionp->getBlockFly())
		{
			gAgent.setFlying(gAgent.canFly());
		}
	}
}


void process_avatar_animation(LLMessageSystem* mesgsys, void** user_data)
{
	LLUUID	animation_id;
	LLUUID	uuid;
	S32		anim_sequence_id;

	mesgsys->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	LLVOAvatar* avatarp = gObjectList.findAvatar(uuid);

	if (!avatarp)
	{
		// no agent by this ID...error?
		LL_WARNS("Messaging") << "Received animation state for unknown avatar " << uuid << LL_ENDL;
		return;
	}

	S32 num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_AnimationList);
	S32 num_source_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_AnimationSourceList);

	LL_DEBUGS("Messaging", "Motion") << "Processing " << num_blocks << " Animations" << LL_ENDL;

	//clear animation flags
	avatarp->mSignaledAnimations.clear();
	
	if (avatarp->isSelf())
	{
		LLUUID object_id;

		for (S32 i = 0; i < num_blocks; i++)
		{
			mesgsys->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id, i);
			mesgsys->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID, anim_sequence_id, i);

			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;

			// *HACK: Disabling flying mode if it has been enabled shortly before the agent
			// stand up animation is signaled. In this case we don't get a signal to start
			// flying animation from server, the AGENT_CONTROL_FLY flag remains set but the
			// avatar does not play flying animation, so we switch flying mode off.
			// See LLAgent::setFlying(). This may cause "Stop Flying" button to blink.
			// See EXT-2781.
			if (animation_id == ANIM_AGENT_STANDUP && gAgent.getFlying())
			{
				gAgent.setFlying(FALSE);
			}

			if (i < num_source_blocks)
			{
				mesgsys->getUUIDFast(_PREHASH_AnimationSourceList, _PREHASH_ObjectID, object_id, i);
			
				LLViewerObject* object = gObjectList.findObject(object_id);
				if (object)
				{
					object->setFlagsWithoutUpdate(FLAGS_ANIM_SOURCE, TRUE);

					BOOL anim_found = FALSE;
					LLVOAvatar::AnimSourceIterator anim_it = avatarp->mAnimationSources.find(object_id);
					for (; anim_it != avatarp->mAnimationSources.end(); ++anim_it)
					{
						if (anim_it->second == animation_id)
						{
							anim_found = TRUE;
							break;
						}
					}

					if (!anim_found)
					{
						avatarp->mAnimationSources.insert(LLVOAvatar::AnimationSourceMap::value_type(object_id, animation_id));
					}
				}
				LL_DEBUGS("Messaging", "Motion") << "Anim sequence ID: " << anim_sequence_id
									<< " Animation id: " << animation_id
									<< " From block: " << object_id << LL_ENDL;
			}
			else
			{
				LL_DEBUGS("Messaging", "Motion") << "Anim sequence ID: " << anim_sequence_id
									<< " Animation id: " << animation_id << LL_ENDL;
			}
		}
	}
	else
	{
		for (S32 i = 0; i < num_blocks; i++)
		{
			mesgsys->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id, i);
			mesgsys->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID, anim_sequence_id, i);
			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;
		}
	}

	//if (num_blocks)  Singu note: commented out; having blocks or not is totally irrelevant!
	{
		avatarp->processAnimationStateChanges();
	}
}


void process_object_animation(LLMessageSystem *mesgsys, void **user_data)
{
	LLUUID	animation_id;
	LLUUID	uuid;
	S32		anim_sequence_id;
	
	mesgsys->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

    LL_DEBUGS("AnimatedObjectsNotify") << "Received animation state for object " << uuid << LL_ENDL;

    signaled_animation_map_t signaled_anims;
	S32 num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_AnimationList);
	LL_DEBUGS("AnimatedObjectsNotify") << "processing object animation requests, num_blocks " << num_blocks << " uuid " << uuid << LL_ENDL;
    for( S32 i = 0; i < num_blocks; i++ )
    {
        mesgsys->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id, i);
        mesgsys->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID, anim_sequence_id, i);
        signaled_anims[animation_id] = anim_sequence_id;
        LL_DEBUGS("AnimatedObjectsNotify") << "added signaled_anims animation request for object " 
                                    << uuid << " animation id " << animation_id << LL_ENDL;
    }
    LLObjectSignaledAnimationMap::instance().getMap()[uuid] = signaled_anims;
    
    LLViewerObject *objp = gObjectList.findObject(uuid);
    if (!objp)
    {
		LL_DEBUGS("AnimatedObjectsNotify") << "Received animation state for unknown object " << uuid << LL_ENDL;
        return;
    }
    
	LLVOVolume *volp = objp->asVolume();
    if (!volp)
    {
		LL_DEBUGS("AnimatedObjectsNotify") << "Received animation state for non-volume object " << uuid << LL_ENDL;
        return;
    }

    if (!volp->isAnimatedObject())
    {
		LL_DEBUGS("AnimatedObjectsNotify") << "Received animation state for non-animated object " << uuid << LL_ENDL;
        return;
    }

    volp->updateControlAvatar();
    LLControlAvatar *avatarp = volp->getControlAvatar();
    if (!avatarp)
    {
        LL_DEBUGS("AnimatedObjectsNotify") << "Received animation request for object with no control avatar, ignoring " << uuid << LL_ENDL;
        return;
    }
    
    if (!avatarp->mPlaying)
    {
        avatarp->mPlaying = true;
        //if (!avatarp->mRootVolp->isAnySelected())
        {
            avatarp->updateVolumeGeom();
            avatarp->mRootVolp->recursiveMarkForUpdate(TRUE);
        }
    }
        
    avatarp->updateAnimations();
}


void process_avatar_appearance(LLMessageSystem* mesgsys, void** user_data)
{
	LLUUID uuid;
	mesgsys->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	LLVOAvatar* avatarp = gObjectList.findAvatar(uuid);
	if (avatarp)
	{
		avatarp->processAvatarAppearance(mesgsys);
	}
	else
	{
		LL_WARNS("Messaging") << "avatar_appearance sent for unknown avatar " << uuid << LL_ENDL;
	}
}

void process_camera_constraint(LLMessageSystem* mesgsys, void** user_data)
{
	static LLCachedControl<bool> disableSimConst(gSavedSettings, "AlchemyDisableSimCamConstraint");
	if (disableSimConst)
		return;

	LLVector4 cameraCollidePlane;
	mesgsys->getVector4Fast(_PREHASH_CameraCollidePlane, _PREHASH_Plane, cameraCollidePlane);

	gAgentCamera.setCameraCollidePlane(cameraCollidePlane);
}

void near_sit_object(BOOL success, void* data)
{
	if (success)
	{
		// Send message to sit on object
		gMessageSystem->newMessageFast(_PREHASH_AgentSit);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gAgent.sendReliableMessage();
	}
}

void process_avatar_sit_response(LLMessageSystem* mesgsys, void** user_data)
{
	LLVector3 sitPosition;
	LLQuaternion sitRotation;
	LLUUID sitObjectID;
	BOOL use_autopilot;
	mesgsys->getUUIDFast(_PREHASH_SitObject, _PREHASH_ID, sitObjectID);
	mesgsys->getBOOLFast(_PREHASH_SitTransform, _PREHASH_AutoPilot, use_autopilot);
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_SitPosition, sitPosition);
	mesgsys->getQuatFast(_PREHASH_SitTransform, _PREHASH_SitRotation, sitRotation);
	LLVector3 camera_eye;
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraEyeOffset, camera_eye);
	LLVector3 camera_at;
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraAtOffset, camera_at);
	BOOL force_mouselook;
	mesgsys->getBOOLFast(_PREHASH_SitTransform, _PREHASH_ForceMouselook, force_mouselook);

	if (isAgentAvatarValid() && dist_vec_squared(camera_eye, camera_at) > CAMERA_POSITION_THRESHOLD_SQUARED)
	{
		gAgentCamera.setSitCamera(sitObjectID, camera_eye, camera_at);
	}
	
	gAgentCamera.setForceMouselook(force_mouselook);
	// Forcing turning off flying here to prevent flying after pressing "Stand"
	// to stand up from an object. See EXT-1655.
	// Unless the user wants to.
	if (!gSavedSettings.getBOOL("LiruContinueFlyingOnUnsit"))
		gAgent.setFlying(FALSE);

	LLViewerObject* object = gObjectList.findObject(sitObjectID);
	if (object)
	{
		LLVector3 sit_spot = object->getPositionAgent() + (sitPosition * object->getRotation());
		if (!use_autopilot || (isAgentAvatarValid() && gAgentAvatarp->isSitting() && gAgentAvatarp->getRoot() == object->getRoot()))
		{
			//we're already sitting on this object, so don't autopilot
		}
		else
		{
			gAgent.startAutoPilotGlobal(gAgent.getPosGlobalFromAgent(sit_spot), "Sit", &sitRotation, near_sit_object, nullptr, 0.5f);
		}
	}
	else
	{
		LL_WARNS("Messaging") << "Received sit approval for unknown object " << sitObjectID << LL_ENDL;
	}
}

void process_clear_follow_cam_properties(LLMessageSystem* mesgsys, void** user_data)
{
	LLUUID		source_id;

	mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);

	LLFollowCamMgr::removeFollowCamParams(source_id);
}

void process_set_follow_cam_properties(LLMessageSystem* mesgsys, void** user_data)
{
	S32			type;
	F32			value;
	bool		settingPosition = false;
	bool		settingFocus	= false;
	bool		settingFocusOffset = false;
	LLVector3	position;
	LLVector3	focus;
	LLVector3	focus_offset;

	LLUUID		source_id;

	mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);

	LLViewerObject* objectp = gObjectList.findObject(source_id);
	if (objectp)
	{
		objectp->setFlagsWithoutUpdate(FLAGS_CAMERA_SOURCE, TRUE);
	}

	S32 num_objects = mesgsys->getNumberOfBlocks("CameraProperty");
	for (S32 block_index = 0; block_index < num_objects; block_index++)
	{
		mesgsys->getS32("CameraProperty", "Type", type, block_index);
		mesgsys->getF32("CameraProperty", "Value", value, block_index);
		switch (type)
		{
		case FOLLOWCAM_PITCH:
			LLFollowCamMgr::setPitch(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_OFFSET_X:
			focus_offset.mV[VX] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_FOCUS_OFFSET_Y:
			focus_offset.mV[VY] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_FOCUS_OFFSET_Z:
			focus_offset.mV[VZ] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_POSITION_LAG:
			LLFollowCamMgr::setPositionLag(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_LAG:
			LLFollowCamMgr::setFocusLag(source_id, value);
			break;
		case FOLLOWCAM_DISTANCE:
			LLFollowCamMgr::setDistance(source_id, value);
			break;
		case FOLLOWCAM_BEHINDNESS_ANGLE:
			LLFollowCamMgr::setBehindnessAngle(source_id, value);
			break;
		case FOLLOWCAM_BEHINDNESS_LAG:
			LLFollowCamMgr::setBehindnessLag(source_id, value);
			break;
		case FOLLOWCAM_POSITION_THRESHOLD:
			LLFollowCamMgr::setPositionThreshold(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_THRESHOLD:
			LLFollowCamMgr::setFocusThreshold(source_id, value);
			break;
		case FOLLOWCAM_ACTIVE:
			//if 1, set using followcam,. 
			LLFollowCamMgr::setCameraActive(source_id, value != 0.f);
			break;
		case FOLLOWCAM_POSITION_X:
			settingPosition = true;
			position.mV[0] = value;
			break;
		case FOLLOWCAM_POSITION_Y:
			settingPosition = true;
			position.mV[1] = value;
			break;
		case FOLLOWCAM_POSITION_Z:
			settingPosition = true;
			position.mV[2] = value;
			break;
		case FOLLOWCAM_FOCUS_X:
			settingFocus = true;
			focus.mV[0] = value;
			break;
		case FOLLOWCAM_FOCUS_Y:
			settingFocus = true;
			focus.mV[1] = value;
			break;
		case FOLLOWCAM_FOCUS_Z:
			settingFocus = true;
			focus.mV[2] = value;
			break;
		case FOLLOWCAM_POSITION_LOCKED:
			LLFollowCamMgr::setPositionLocked(source_id, value != 0.f);
			break;
		case FOLLOWCAM_FOCUS_LOCKED:
			LLFollowCamMgr::setFocusLocked(source_id, value != 0.f);
			break;

		default:
			break;
		}
	}

	if (settingPosition)
	{
		LLFollowCamMgr::setPosition(source_id, position);
	}
	if (settingFocus)
	{
		LLFollowCamMgr::setFocus(source_id, focus);
	}
	if (settingFocusOffset)
	{
		LLFollowCamMgr::setFocusOffset(source_id, focus_offset);
	}
}

//end Ventrella 


// Culled from newsim lltask.cpp
void process_name_value(LLMessageSystem* mesgsys, void** user_data)
{
	std::string	temp_str;
	LLUUID	id;
	S32		i, num_blocks;

	mesgsys->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);

	LLViewerObject* object = gObjectList.findObject(id);

	if (object)
	{
		num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (i = 0; i < num_blocks; i++)
		{
			mesgsys->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair, temp_str, i);
			LL_INFOS("Messaging") << "Added to object Name Value: " << temp_str << LL_ENDL;
			object->addNVPair(temp_str);
		}
	}
	else
	{
		LL_INFOS("Messaging") << "Can't find object " << id << " to add name value pair" << LL_ENDL;
	}
}

void process_remove_name_value(LLMessageSystem* mesgsys, void** user_data)
{
	std::string	temp_str;
	LLUUID	id;
	S32		i, num_blocks;

	mesgsys->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);

	LLViewerObject* object = gObjectList.findObject(id);

	if (object)
	{
		num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (i = 0; i < num_blocks; i++)
		{
			mesgsys->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair, temp_str, i);
			LL_INFOS("Messaging") << "Removed from object Name Value: " << temp_str << LL_ENDL;
			object->removeNVPair(temp_str);
		}
	}
	else
	{
		LL_INFOS("Messaging") << "Can't find object " << id << " to remove name value pair" << LL_ENDL;
	}
}

void process_kick_user(LLMessageSystem* msg, void** /*user_data*/)
{
	std::string message;

	msg->getStringFast(_PREHASH_UserInfo, _PREHASH_Reason, message);

	LLAppViewer::instance()->forceDisconnect(message);
}


/*
void process_user_list_reply(LLMessageSystem *msg, void **user_data)
{
	LLUserList::processUserListReply(msg, user_data);
	return;
	char	firstname[MAX_STRING+1];
	char	lastname[MAX_STRING+1];
	U8		status;
	S32		user_count;

	user_count = msg->getNumberOfBlocks("UserBlock");

	for (S32 i = 0; i < user_count; i++)
	{
		msg->getData("UserBlock", i, "FirstName", firstname);
		msg->getData("UserBlock", i, "LastName", lastname);
		msg->getData("UserBlock", i, "Status", &status);

		if (status & 0x01)
		{
			dialog_friends_add_friend(buffer, TRUE);
		}
		else
		{
			dialog_friends_add_friend(buffer, FALSE);
		}
	}

	dialog_friends_done_adding();
}
*/

// this is not handled in processUpdateMessage
/*
void process_time_dilation(LLMessageSystem *msg, void **user_data)
{
	// get the time_dilation
	U16 foo;
	msg->getData("TimeDilation", "TimeDilation", &foo);
	F32 time_dilation = ((F32) foo) / 65535.f;

	// get the pointer to the right region
	U32 ip = msg->getSenderIP();
	U32 port = msg->getSenderPort();
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegion(ip, port);
	if (regionp)
	{
		regionp->setTimeDilation(time_dilation);
	}
}
*/


void process_money_balance_reply(LLMessageSystem* msg, void**)
{
	S32 balance = 0;
	S32 credit = 0;
	S32 committed = 0;
	std::string desc;
	LLUUID tid;

	msg->getUUID("MoneyData", "TransactionID", tid);
	msg->getS32("MoneyData", "MoneyBalance", balance);
	msg->getS32("MoneyData", "SquareMetersCredit", credit);
	msg->getS32("MoneyData", "SquareMetersCommitted", committed);
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_Description, desc);
	LL_INFOS("Messaging") << "L$, credit, committed: " << balance << " " << credit << " "
			<< committed << LL_ENDL;

	if (gStatusBar)
	{
		S32 old_balance = gStatusBar->getBalance();

		// This is an update, not the first transmission of balance
		if (old_balance != 0)
		{
			// this is actually an update
			if (balance > old_balance)
			{
				LLFirstUse::useBalanceIncrease(balance - old_balance);
			}
			else if (balance < old_balance)
			{
				LLFirstUse::useBalanceDecrease(balance - old_balance);
			}
		}

		gStatusBar->setBalance(balance);
		gStatusBar->setLandCredit(credit);
		gStatusBar->setLandCommitted(committed);
	}

	if (desc.empty()
		|| !gSavedSettings.getBOOL("NotifyMoneyChange"))
	{
		// ...nothing to display
		return;
	}

	// Suppress duplicate messages about the same transaction
	static std::deque<LLUUID> recent;
	if (std::find(recent.rbegin(), recent.rend(), tid) != recent.rend())
	{
		return;
	}

	// Once the 'recent' container gets large enough, chop some
	// off the beginning.
	const U32 MAX_LOOKBACK = 30;
	const S32 POP_FRONT_SIZE = 12;
	if (recent.size() > MAX_LOOKBACK)
	{
		LL_DEBUGS("Messaging") << "Removing oldest transaction records" << LL_ENDL;
		recent.erase(recent.begin(), recent.begin() + POP_FRONT_SIZE);
	}
	//LL_DEBUGS("Messaging") << "Pushing back transaction " << tid << LL_ENDL;
	recent.push_back(tid);

	if (msg->has("TransactionInfo"))
	{
		// ...message has extended info for localization
		process_money_balance_reply_extended(msg);
	}
	else
	{
		// Only old dev grids will not supply the TransactionInfo block,
		// so we can just use the hard-coded English string.
		LLSD args;
		args["MESSAGE"] = desc;
		LLNotificationsUtil::add("SystemMessage", args);

		// Also send notification to chat -- MC
		LLFloaterChat::addChat(desc);
	}
}

static std::string reason_from_transaction_type(S32 transaction_type,
												const std::string& item_desc)
{
	// *NOTE: The keys for the reason strings are unusual because
	// an earlier version of the code used English language strings
	// extracted from hard-coded server English descriptions.
	// Keeping them so we don't have to re-localize them.
	switch (transaction_type)
	{
		case TRANS_OBJECT_SALE:
		{
			LLStringUtil::format_map_t arg;
			arg["ITEM"] = item_desc;
			return LLTrans::getString("for item", arg);
		}
		case TRANS_LAND_SALE:
			return LLTrans::getString("for a parcel of land");

		case TRANS_LAND_PASS_SALE:
			return LLTrans::getString("for a land access pass");

		case TRANS_GROUP_LAND_DEED:
			return LLTrans::getString("for deeding land");

		case TRANS_GROUP_CREATE:
			return LLTrans::getString("to create a group");

		case TRANS_GROUP_JOIN:
			return LLTrans::getString("to join a group");

		case TRANS_UPLOAD_CHARGE:
			return LLTrans::getString("to upload");

		case TRANS_CLASSIFIED_CHARGE:
			return LLTrans::getString("to publish a classified ad");

	case TRANS_GIFT:
		// Simulator returns "Payment" if no custom description has been entered
		return (item_desc == "Payment" ? std::string() : item_desc);

		// These have no reason to display, but are expected and should not
		// generate warnings
		case TRANS_PAY_OBJECT:
		case TRANS_OBJECT_PAYS:
			return std::string();

		default:
			LL_WARNS() << "Unknown transaction type "
				<< transaction_type << LL_ENDL;
			return std::string();
	}
}

static void process_money_balance_reply_extended(LLMessageSystem* msg)
{
	// Added in server 1.40 and viewer 2.1, support for localization
	// and agent ids for name lookup.
	S32 transaction_type = 0;
	LLUUID source_id;
	BOOL is_source_group = FALSE;
	LLUUID dest_id;
	BOOL is_dest_group = FALSE;
	S32 amount = 0;
	std::string item_description;
	BOOL success = FALSE;

	msg->getS32("TransactionInfo", "TransactionType", transaction_type);
	msg->getUUID("TransactionInfo", "SourceID", source_id);
	msg->getBOOL("TransactionInfo", "IsSourceGroup", is_source_group);
	msg->getUUID("TransactionInfo", "DestID", dest_id);
	msg->getBOOL("TransactionInfo", "IsDestGroup", is_dest_group);
	msg->getS32("TransactionInfo", "Amount", amount);
	msg->getString("TransactionInfo", "ItemDescription", item_description);
	msg->getBOOL("MoneyData", "TransactionSuccess", success);
	LL_INFOS("Money") << "MoneyBalanceReply source " << source_id
		<< " dest " << dest_id
		<< " type " << transaction_type
		<< " item " << item_description << LL_ENDL;

	if (source_id.isNull() && dest_id.isNull())
	{
		// this is a pure balance update, no notification required
		return;
	}

	if ((U32)amount < gSavedSettings.getU32("LiruShowTransactionThreshold")) return; // <singu/> Don't show transactions of small amounts the user doesn't care about.

	std::string reason =
		reason_from_transaction_type(transaction_type, item_description);

	LLStringUtil::format_map_t args;
	args["REASON"] = reason; // could be empty
	args["AMOUNT"] = llformat("%d", amount);

	// Need to delay until name looked up, so need to know whether or not
	// is group
	bool is_name_group = false;
	LLUUID name_id;
	std::string message;
	LLSD payload;

	bool you_paid_someone = (source_id == gAgentID);
	std::string gift_suffix = (transaction_type == TRANS_GIFT ? "_gift" : "");
	if (you_paid_someone)
	{
		is_name_group = is_dest_group;
		name_id = dest_id;
		if (!reason.empty())
		{
			if (dest_id.notNull())
			{
				message = success ? "you_paid_ldollars" + gift_suffix :
					          "you_paid_failure_ldollars" + gift_suffix;
			}
			else
			{
				// transaction fee to the system, eg, to create a group
				message = success ? "you_paid_ldollars_no_name" :
									"you_paid_failure_ldollars_no_name";
			}
		}
		else
		{
			if (dest_id.notNull())
			{
				message = success ? "you_paid_ldollars_no_reason" :
									"you_paid_failure_ldollars_no_reason";
			}
			else
			{
				// no target, no reason, you just paid money
				message = success ? "you_paid_ldollars_no_info" :
									"you_paid_failure_ldollars_no_info";
			}
		}
	}
	else
	{
		// ...someone paid you
		is_name_group = is_source_group;
		name_id = source_id;
		if (!reason.empty())
		{
			message = "paid_you_ldollars" + gift_suffix;
		}
		else
		{
			message = "paid_you_ldollars_no_reason";
		}

		// make notification loggable
		payload["from_id"] = source_id;

		void script_msg_api(const std::string& msg);
		if (!is_source_group) script_msg_api(source_id.asString() + ", 7");
	}

	// Despite using SLURLs, wait until the name is available before
	// showing the notification, otherwise the UI layout is strange and
	// the user sees a "Loading..." message
	// Singu Note: Wat? SLURLs resolve over time, not the end of the world.
	bool no_transaction_clutter = gSavedSettings.getBOOL("LiruNoTransactionClutter");
	std::string notification = no_transaction_clutter ? "Payment" : "SystemMessage";
	args["NAME"] = is_name_group ? LLGroupActions::getSLURL(name_id) : LLAvatarActions::getSLURL(name_id);
	LLSD msg_args;
	msg_args["MESSAGE"] = LLTrans::getString(message,args);
	LLNotificationsUtil::add(notification,msg_args,payload);

	if (!no_transaction_clutter) LLFloaterChat::addChat(msg_args["MESSAGE"].asString()); // Alerts won't automatically log to chat.
}

bool handle_prompt_for_maturity_level_change_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (0 == option)
	{
		// set the preference to the maturity of the region we're calling
		U8 preferredMaturity = static_cast<U8>(notification["payload"]["_region_access"].asInteger());
		gSavedSettings.setU32("PreferredMaturity", static_cast<U32>(preferredMaturity));
	}

	return false;
}

bool handle_prompt_for_maturity_level_change_and_reteleport_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	
	if (0 == option)
	{
		// set the preference to the maturity of the region we're calling
		U8 preferredMaturity = static_cast<U8>(notification["payload"]["_region_access"].asInteger());
		gSavedSettings.setU32("PreferredMaturity", static_cast<U32>(preferredMaturity));
		gAgent.setMaturityRatingChangeDuringTeleport(preferredMaturity);
		gAgent.restartFailedTeleportRequest();
	}
	else
	{
		gAgent.clearTeleportRequest();
	}
	
	return false;
}

// some of the server notifications need special handling. This is where we do that.
bool handle_special_notification(std::string notificationID, LLSD& llsdBlock)
{
	bool returnValue = false;
	if (llsdBlock.has("_region_access"))
	{
		U8 regionAccess = static_cast<U8>(llsdBlock["_region_access"].asInteger());
		std::string regionMaturity = LLViewerRegion::accessToString(regionAccess);
		LLStringUtil::toLower(regionMaturity);
		llsdBlock["REGIONMATURITY"] = regionMaturity;
		LLNotificationPtr maturityLevelNotification;
		std::string notifySuffix = "_Notify";
		if (regionAccess == SIM_ACCESS_MATURE)
		{
			if (gAgent.isTeen())
			{
				gAgent.clearTeleportRequest();
				maturityLevelNotification = LLNotificationsUtil::add(notificationID + "_AdultsOnlyContent", llsdBlock);
				returnValue = true;

				notifySuffix = "_NotifyAdultsOnly";
			}
			else if (gAgent.prefersPG())
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID + "_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
				returnValue = true;
			}
			else if (LLStringUtil::compareStrings(notificationID, "RegionEntryAccessBlocked") == 0)
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID + "_PreferencesOutOfSync", llsdBlock, llsdBlock);
				returnValue = true;
			}
		}
		else if (regionAccess == SIM_ACCESS_ADULT)
		{
			if (!gAgent.isAdult())
			{
				gAgent.clearTeleportRequest();
				maturityLevelNotification = LLNotificationsUtil::add(notificationID + "_AdultsOnlyContent", llsdBlock);
				returnValue = true;

				notifySuffix = "_NotifyAdultsOnly";
			}
			else if (gAgent.prefersPG() || gAgent.prefersMature())
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID + "_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
				returnValue = true;
			}
			else if (LLStringUtil::compareStrings(notificationID, "RegionEntryAccessBlocked") == 0)
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID + "_PreferencesOutOfSync", llsdBlock, llsdBlock);
				returnValue = true;
			}
		}

		if ((maturityLevelNotification == nullptr) || maturityLevelNotification->isIgnored())
		{
			// Given a simple notification if no maturityLevelNotification is set or it is ignore
			LLNotificationsUtil::add(notificationID + notifySuffix, llsdBlock);
		}
	}

	return returnValue;
}

bool handle_trusted_experiences_notification(const LLSD& llsdBlock)
{
	if (llsdBlock.has("trusted_experiences"))
	{
		std::ostringstream str;
		const LLSD& experiences = llsdBlock["trusted_experiences"];
		LLSD::array_const_iterator it = experiences.beginArray();
		for (/**/; it != experiences.endArray(); ++it)
		{
			str << LLSLURL("experience", it->asUUID(), "profile").getSLURLString() << "\n";
		}
		std::string str_list = str.str();
		if (!str_list.empty())
		{
			LLNotificationsUtil::add("TrustedExperiencesAvailable", LLSD::emptyMap().with("EXPERIENCE_LIST", (LLSD)str_list));
			return true;
		}
	}
	return false;
}

// some of the server notifications need special handling. This is where we do that.
bool handle_teleport_access_blocked(LLSD& llsdBlock, const std::string& notificationID, const std::string& defaultMessage)
{
	bool returnValue = false;
	if (llsdBlock.has("_region_access"))
	{
		U8 regionAccess = static_cast<U8>(llsdBlock["_region_access"].asInteger());
		std::string regionMaturity = LLViewerRegion::accessToString(regionAccess);
		LLStringUtil::toLower(regionMaturity);
		llsdBlock["REGIONMATURITY"] = regionMaturity;

		LLNotificationPtr tp_failure_notification;
		std::string notifySuffix;

		if (notificationID == std::string("TeleportEntryAccessBlocked"))
		{
			notifySuffix = "_Notify";
			if (regionAccess == SIM_ACCESS_MATURE)
			{
				if (gAgent.isTeen())
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID + "_AdultsOnlyContent", llsdBlock);
					returnValue = true;

					notifySuffix = "_NotifyAdultsOnly";
				}
				else if (gAgent.prefersPG())
				{
					if (gAgent.hasRestartableFailedTeleportRequest())
					{
						tp_failure_notification = LLNotificationsUtil::add(notificationID + "_ChangeAndReTeleport", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_and_reteleport_callback);
						returnValue = true;
					}
					else
					{
						gAgent.clearTeleportRequest();
						tp_failure_notification = LLNotificationsUtil::add(notificationID + "_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
						returnValue = true;
					}
				}
				else
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID + "_PreferencesOutOfSync", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
					returnValue = true;
				}
			}
			else if (regionAccess == SIM_ACCESS_ADULT)
			{
				if (!gAgent.isAdult())
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID + "_AdultsOnlyContent", llsdBlock);
					returnValue = true;

					notifySuffix = "_NotifyAdultsOnly";
				}
				else if (gAgent.prefersPG() || gAgent.prefersMature())
				{
					if (gAgent.hasRestartableFailedTeleportRequest())
					{
						tp_failure_notification = LLNotificationsUtil::add(notificationID + "_ChangeAndReTeleport", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_and_reteleport_callback);
						returnValue = true;
					}
					else
					{
						gAgent.clearTeleportRequest();
						tp_failure_notification = LLNotificationsUtil::add(notificationID + "_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
						returnValue = true;
					}
				}
				else
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID + "_PreferencesOutOfSync", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
					returnValue = true;
				}
			}
		}		// End of special handling for "TeleportEntryAccessBlocked"
		else
		{	// Normal case, no message munging
			gAgent.clearTeleportRequest();
			if (LLNotificationTemplates::getInstance()->templateExists(notificationID))
			{
				tp_failure_notification = LLNotificationsUtil::add(notificationID, llsdBlock, llsdBlock);
			}
			else
			{
				llsdBlock["MESSAGE"] = defaultMessage;
				tp_failure_notification = LLNotificationsUtil::add("GenericAlertOK", llsdBlock);
			}
			returnValue = true;
		}

		if ((tp_failure_notification == nullptr) || tp_failure_notification->isIgnored())
		{
			// Given a simple notification if no tp_failure_notification is set or it is ignore
			LLNotificationsUtil::add(notificationID + notifySuffix, llsdBlock);
		}
	}

	handle_trusted_experiences_notification(llsdBlock);
	return returnValue;
}

void home_position_set()
{
	// save the home location image to disk
	std::string snap_filename = gDirUtilp->getLindenUserDir();
	snap_filename += gDirUtilp->getDirDelimiter();
	snap_filename += SCREEN_HOME_FILENAME;
	gViewerWindow->saveSnapshot(snap_filename, gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw(), FALSE, FALSE);
}

void update_region_restart(const LLSD& llsdBlock)
{
	const U32 seconds = llsdBlock.has("MINUTES")
		? (60U * static_cast<U32>(llsdBlock["MINUTES"].asInteger()))
		: static_cast<U32>(llsdBlock["SECONDS"].asInteger());
	if (LLFloaterRegionRestarting* restarting_floater = LLFloaterRegionRestarting::findInstance())
	{
		restarting_floater->updateTime(seconds);
		/*if (!restarting_floater->isMinimized())
			restarting_floater->center();*/
	}
	else
	{
		LLSD params;
		params["NAME"] = llsdBlock["NAME"];
		params["SECONDS"] = (LLSD::Integer)seconds;
		LLFloaterRegionRestarting::showInstance(params);
		if (gSavedSettings.getBOOL("LiruRegionRestartMinimized"))
			LLFloaterRegionRestarting::findInstance()->setMinimized(true);
	}
}

bool attempt_standard_notification(LLMessageSystem* msgsystem)
{
	// if we have additional alert data
	if (msgsystem->has(_PREHASH_AlertInfo) && msgsystem->getNumberOfBlocksFast(_PREHASH_AlertInfo) > 0)
	{
		// notification was specified using the new mechanism, so we can just handle it here
		std::string notificationID;
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, notificationID);
		if (!LLNotificationTemplates::getInstance()->templateExists(notificationID))
		{
			return false;
		}

		std::string llsdRaw;
		LLSD llsdBlock;
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams, llsdRaw);
		if (llsdRaw.length())
		{
			std::istringstream llsdData(llsdRaw);
			if (!LLSDSerialize::deserialize(llsdBlock, llsdData, llsdRaw.length()))
			{
				LL_WARNS() << "attempt_standard_notification: Attempted to read notification parameter data into LLSD but failed:" << llsdRaw << LL_ENDL;
			}
		}


		handle_trusted_experiences_notification(llsdBlock);
		
		if (
			(notificationID == "RegionEntryAccessBlocked") ||
			(notificationID == "LandClaimAccessBlocked") ||
			(notificationID == "LandBuyAccessBlocked")
		   )
		{
			/*---------------------------------------------------------------------
			 (Commented so a grep will find the notification strings, since
			 we construct them on the fly; if you add additional notifications,
			 please update the comment.)
			 
			 Could throw any of the following notifications:
			 
				RegionEntryAccessBlocked
				RegionEntryAccessBlocked_Notify
				RegionEntryAccessBlocked_NotifyAdultsOnly
				RegionEntryAccessBlocked_Change
				RegionEntryAccessBlocked_AdultsOnlyContent
				RegionEntryAccessBlocked_ChangeAndReTeleport
				LandClaimAccessBlocked 
				LandClaimAccessBlocked_Notify 
				LandClaimAccessBlocked_NotifyAdultsOnly
				LandClaimAccessBlocked_Change 
				LandClaimAccessBlocked_AdultsOnlyContent 
				LandBuyAccessBlocked
				LandBuyAccessBlocked_Notify
				LandBuyAccessBlocked_NotifyAdultsOnly
				LandBuyAccessBlocked_Change
				LandBuyAccessBlocked_AdultsOnlyContent
			 
			-----------------------------------------------------------------------*/ 
			if (handle_special_notification(notificationID, llsdBlock))
			{
				return true;
			}
		}
		else if (notificationID == "expired_region_handoff" || notificationID == "invalid_region_handoff") // borked region handoff
		{
			gAgent.setIsCrossingRegion(false); // Attachments getting lost on TP
		}
		else
		// HACK -- handle callbacks for specific alerts.
		if (notificationID == "HomePositionSet")
		{
			home_position_set();
		}
		else if (notificationID == "YouDiedAndGotTPHome")
		{
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_KILLED_COUNT);
		}
		else if (notificationID == "RegionRestartMinutes" ||
				 notificationID == "RegionRestartSeconds")
		{
			update_region_restart(llsdBlock);
			LLUI::sAudioCallback(LLUUID(gSavedSettings.getString("UISndRestart")));
			return true; // Floater is enough.
		}
		else
		// Special Marketplace update notification
		if (notificationID == "SLM_UPDATE_FOLDER")
		{
			std::string state = llsdBlock["state"].asString();
			if (state == "deleted")
			{
				// Perform the deletion viewer side, no alert shown in this case
				LLMarketplaceData::instance().deleteListing(llsdBlock["listing_id"].asInteger());
				return true;
			}
			// In general, no message will be displayed, all we want is to get the listing updated in the marketplace floater
			// If getListing() fails though, the message of the alert will be shown by the caller of attempt_standard_notification()
			return LLMarketplaceData::instance().getListing(llsdBlock["listing_id"].asInteger());
		}

        // Error Notification can come with and without reason
        if (notificationID == "JoinGroupError")
        {
            if (llsdBlock.has("reason"))
            {
                LLNotificationsUtil::add("JoinGroupErrorReason", llsdBlock);
                return true;
            }
            if (llsdBlock.has("group_id"))
            {
                LLGroupData agent_gdatap;
                bool is_member = gAgent.getGroupData(llsdBlock["group_id"].asUUID(), agent_gdatap);
                if (is_member)
                {
                    LLSD args;
                    args["reason"] = LLTrans::getString("AlreadyInGroup");
                    LLNotificationsUtil::add("JoinGroupErrorReason", args);
                    return true;
                }
            }
		}

		LLNotificationsUtil::add(notificationID, llsdBlock);
		return true;
	}	
	return false;
}


static void process_special_alert_messages(const std::string& message)
{
	// Do special handling for alert messages.   This is a legacy hack, and any actual displayed
	// text should be altered in the notifications.xml files.
	if (message == "You died and have been teleported to your home location")
	{
		LLViewerStats::getInstance()->incStat(LLViewerStats::ST_KILLED_COUNT);
	}
	else if (message == "Home position set.")
	{
		home_position_set();
	}
}


void process_agent_alert_message(LLMessageSystem* msgsystem, void** user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
	
	std::string message;
	msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, message);

	process_special_alert_messages(message);

	if (!attempt_standard_notification(msgsystem))
	{
		BOOL modal = FALSE;
		msgsystem->getBOOL("AlertData", "Modal", modal);
		process_alert_core(message, modal);
	}
}

// The only difference between this routine and the previous is the fact that
// for this routine, the modal parameter is always false. Sadly, for the message
// handled by this routine, there is no "Modal" parameter on the message, and
// there's no API to tell if a message has the given parameter or not.
// So we can't handle the messages with the same handler.
void process_alert_message(LLMessageSystem* msgsystem, void** user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
		
	std::string message;
	msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, message);
	process_special_alert_messages(message);

	if (!attempt_standard_notification(msgsystem))
	{
		BOOL modal = FALSE;
		process_alert_core(message, modal);
	}
}

bool handle_not_age_verified_alert(const std::string& pAlertName)
{
	LLNotificationPtr notification = LLNotificationsUtil::add(pAlertName);
	if ((notification == nullptr) || notification->isIgnored())
	{
		LLNotificationsUtil::add(pAlertName + "_Notify");
	}

	return true;
}

bool handle_special_alerts(const std::string& pAlertName)
{
	bool isHandled = false;
	if (LLStringUtil::compareStrings(pAlertName, "NotAgeVerified") == 0)
	{
		isHandled = handle_not_age_verified_alert(pAlertName);
	}

	return isHandled;
}

void process_alert_core(const std::string& message, BOOL modal)
{
	const std::string ALERT_PREFIX("ALERT: ");
	const std::string NOTIFY_PREFIX("NOTIFY: ");
	if (message.find(ALERT_PREFIX) == 0)
	{
		// Allow the server to spawn a named alert so that server alerts can be
		// translated out of English.
		std::string alert_name(message.substr(ALERT_PREFIX.length()));
		if (!handle_special_alerts(alert_name))
		{
			LLNotificationsUtil::add(alert_name);
		}
	}
	else if (message.find(NOTIFY_PREFIX) == 0)
	{
		// Allow the server to spawn a named notification so that server notifications can be
		// translated out of English.
		std::string notify_name(message.substr(NOTIFY_PREFIX.length()));
		LLNotificationsUtil::add(notify_name);
	}
	else if (message[0] == '/')
	{
		// System message is important, show in upper-right box not tip
		std::string text(message.substr(1));
		LLSD args;

		if (text.substr(0,17) == "RESTART_X_MINUTES")
		{
			S32 mins = 0;
			LLStringUtil::convertToS32(text.substr(18), mins);
			args["MINUTES"] = llformat("%d",mins);
			update_region_restart(args);
			//LLNotificationsUtil::add("RegionRestartMinutes", args); // Floater is enough.
			LLUI::sAudioCallback(LLUUID(gSavedSettings.getString("UISndRestart")));
		}
		else if (text.substr(0,17) == "RESTART_X_SECONDS")
		{
			S32 secs = 0;
			LLStringUtil::convertToS32(text.substr(18), secs);
			args["SECONDS"] = llformat("%d",secs);
			update_region_restart(args);
			//LLNotificationsUtil::add("RegionRestartSeconds", args); // Floater is enough.
			LLUI::sAudioCallback(LLUUID(gSavedSettings.getString("UISndRestart")));
		}
		else
		{
		// *NOTE: If the text from the server ever changes this line will need to be adjusted.
		std::string restart_cancelled = "Region restart cancelled.";
		if (text.substr(0, restart_cancelled.length()) == restart_cancelled)
			{
				LLFloaterRegionRestarting::hideInstance();
			}

			std::string new_msg =LLNotificationTemplates::instance().getGlobalString(text);
// [RLVa:KB] - Checked: 2012-02-07 (RLVa-1.4.5) | Added: RLVa-1.4.5
			if ( (new_msg == text) && (rlv_handler_t::isEnabled()) )
			{
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
					RlvUtil::filterLocation(new_msg);
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
					RlvUtil::filterNames(new_msg);
			}
// [/RLVa:KB]
			args["MESSAGE"] = new_msg;
			LLNotificationsUtil::add("SystemMessage", args);
		}
	}
	else if (modal)
	{
		LLSD args;
		std::string new_msg =LLNotificationTemplates::instance().getGlobalString(message);
// [RLVa:KB] - Checked: 2012-02-07 (RLVa-1.4.5) | Added: RLVa-1.4.5
		if ( (new_msg == message) && (rlv_handler_t::isEnabled()) )
		{
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
				RlvUtil::filterLocation(new_msg);
			if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
				RlvUtil::filterNames(new_msg);
		}
// [/RLVa:KB]
		args["ERROR_MESSAGE"] = new_msg;
		LLNotificationsUtil::add("ErrorMessage", args);
	}
	else
	{
		// Hack fix for EXP-623 (blame fix on RN :)) to avoid a sim deploy
		const std::string AUTOPILOT_CANCELED_MSG("Autopilot canceled");
		if (message.find(AUTOPILOT_CANCELED_MSG) == std::string::npos )
		{
			LLSD args;
			std::string new_msg =LLNotificationTemplates::instance().getGlobalString(message);

			std::string localized_msg;
			bool is_message_localized = LLTrans::findString(localized_msg, new_msg);

// [RLVa:KB] - Checked: 2012-02-07 (RLVa-1.4.5) | Added: RLVa-1.4.5
			if ( (new_msg == message) && (rlv_handler_t::isEnabled()) )
			{
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
					RlvUtil::filterLocation(new_msg);
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
					RlvUtil::filterNames(new_msg);
			}
// [/RLVa:KB]

			args["MESSAGE"] = is_message_localized ? localized_msg : new_msg;
			LLNotificationsUtil::add("SystemMessageTip", args);
		}
	}
}

mean_collision_list_t				gMeanCollisionList;
time_t								gLastDisplayedTime = 0;

void handle_show_mean_events(void*)
{
	if (gNoRender)
	{
		return;
	}

	LLFloaterBump::show(nullptr);
}

void mean_name_callback(const LLUUID& id, const LLAvatarName& av_name)
{
	if (gNoRender)
	{
		return;
	}

	static const U32 max_collision_list_size = 20;
	if (gMeanCollisionList.size() > max_collision_list_size)
	{
		mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
		for (U32 i = 0; i < max_collision_list_size; i++) ++iter;
		for_each(iter, gMeanCollisionList.end(), DeletePointer());
		gMeanCollisionList.erase(iter, gMeanCollisionList.end());
	}

	for (auto mcd : gMeanCollisionList)
	{
		if (mcd->mPerp == id)
		{
			mcd->mFullName = av_name.getUserName();
		}
	}
}

void chat_mean_collision(const LLUUID& id, const LLAvatarName& avname, const EMeanCollisionType& type, const F32& mag)
{
	LLStringUtil::format_map_t args;
	if (type == MEAN_BUMP)
		args["ACT"] = LLTrans::getString("bump");
	else if (type == MEAN_LLPUSHOBJECT)
		args["ACT"] = LLTrans::getString("llpushobject");
	else if (type == MEAN_SELECTED_OBJECT_COLLIDE)
		args["ACT"] = LLTrans::getString("selected_object_collide");
	else if (type == MEAN_SCRIPTED_OBJECT_COLLIDE)
		args["ACT"] = LLTrans::getString("scripted_object_collide");
	else if (type == MEAN_PHYSICAL_OBJECT_COLLIDE)
		args["ACT"] = LLTrans::getString("physical_object_collide");
	else
		return; // How did we get here? I used to know you so well.
	const std::string name(avname.getNSName());
	args["NAME"] = name;
	args["MAG"] = llformat("%f", mag);
	LLChat chat(LLTrans::getString("BumpedYou", args));
	chat.mFromName = name;
	chat.mURL = LLAvatarActions::getSLURL(id);
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLFloaterChat::addChat(chat);
}

void process_mean_collision_alert_message(LLMessageSystem* msgsystem, void** user_data)
{
	if (gAgent.inPrelude())
	{
		// In prelude, bumping is OK.  This dialog is rather confusing to 
		// newbies, so we don't show it.  Drop the packet on the floor.
		return;
	}

	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();

	LLUUID perp;
	U32	   time;
	U8	   u8type;
	EMeanCollisionType	   type;
	F32    mag;

	S32 i, num = msgsystem->getNumberOfBlocks(_PREHASH_MeanCollision);

	for (i = 0; i < num; i++)
	{
		msgsystem->getUUIDFast(_PREHASH_MeanCollision, _PREHASH_Perp, perp);
		msgsystem->getU32Fast(_PREHASH_MeanCollision, _PREHASH_Time, time);
		msgsystem->getF32Fast(_PREHASH_MeanCollision, _PREHASH_Mag, mag);
		msgsystem->getU8Fast(_PREHASH_MeanCollision, _PREHASH_Type, u8type);

		type = (EMeanCollisionType)u8type;
		static const LLCachedControl<bool> chat_collision("AnnounceBumps");
		if (chat_collision) LLAvatarNameCache::get(perp, boost::bind(chat_mean_collision, _1, _2, type, mag));

		BOOL b_found = FALSE;

		for (auto mcd : gMeanCollisionList)
		{
			if ((mcd->mPerp == perp) && (mcd->mType == type))
			{
				mcd->mTime = time;
				mcd->mMag = mag;
				b_found = TRUE;
				break;
			}
		}

		if (!b_found)
		{
			LLMeanCollisionData* mcd = new LLMeanCollisionData(gAgentID, perp, time, type, mag);
			gMeanCollisionList.push_front(mcd);
			LLAvatarNameCache::get(perp, boost::bind(&mean_name_callback, _1, _2));
		}
	}
}

void process_frozen_message(LLMessageSystem* msgsystem, void** user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
	BOOL b_frozen;
	
	msgsystem->getBOOL("FrozenData", "Data", b_frozen);

	// TODO: make being frozen change view
	if (b_frozen)
	{
	}
	else
	{
	}
}

// do some extra stuff once we get our economy data
void process_economy_data(LLMessageSystem* msg, void** /*user_data*/)
{
	auto& grid = *gHippoGridManager->getConnectedGrid();
	if (grid.isSecondLife() || !LLAgentBenefitsMgr::isCurrent("NonSL")) return; // Quick hack to allow other grids benefits management
	LLAgentBenefitsMgr::current().processEconomyData(msg);
}

void notify_cautioned_script_question(const LLSD& notification, const LLSD& response, S32 orig_questions, BOOL granted)
{
	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instanceExists())
	{
		if (NACLAntiSpamRegistry::instance().checkQueue(NACLAntiSpamRegistry::QUEUE_SCRIPT_DIALOG, notification["payload"]["task_id"].asUUID(), LFIDBearer::OBJECT))
			return;
	}
	// NaCl End
	// only continue if at least some permissions were requested
	if (orig_questions)
	{
		// check to see if the person we are asking

		// "'[OBJECTNAME]', an object owned by '[OWNERNAME]', 
		// located in [REGIONNAME] at [REGIONPOS], 
		// has been <granted|denied> permission to: [PERMISSIONS]."

		LLUIString notice(LLTrans::getString(granted ? "ScriptQuestionCautionChatGranted" : "ScriptQuestionCautionChatDenied"));

		// always include the object name and owner name 
		notice.setArg("[OBJECTNAME]", notification["payload"]["object_name"].asString());
		notice.setArg("[OWNERNAME]", notification["payload"]["owner_name"].asString());

		// try to lookup viewerobject that corresponds to the object that
		// requested permissions (here, taskid->requesting object id)
		BOOL foundpos = FALSE;
		LLViewerObject* viewobj = gObjectList.findObject(notification["payload"]["task_id"].asUUID());
		if (viewobj)
		{
			// found the viewerobject, get it's position in its region
			LLVector3 objpos(viewobj->getPosition());
			
			// try to lookup the name of the region the object is in
			LLViewerRegion* viewregion = viewobj->getRegion();
			if (viewregion)
			{
				// got the region, so include the region and 3d coordinates of the object
				notice.setArg("[REGIONNAME]", viewregion->getName());
				std::string formatpos = llformat("%.1f, %.1f,%.1f", objpos[VX], objpos[VY], objpos[VZ]);
				notice.setArg("[REGIONPOS]", formatpos);

				foundpos = TRUE;
			}
		}

// [RLVa:KB] - Checked: 2010-04-23 (RLVa-1.2.0g) | Modified: RLVa-1.0.0a
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		{
			notice.setArg("[REGIONNAME]", RlvStrings::getString(RLV_STRING_HIDDEN_REGION));
			notice.setArg("[REGIONPOS]", RlvStrings::getString(RLV_STRING_HIDDEN));
		}
		else if (!foundpos)
// [/RLVa:KB]
//		if (!foundpos)
		{
			// unable to determine location of the object
			notice.setArg("[REGIONNAME]", "(unknown region)");
			notice.setArg("[REGIONPOS]", "(unknown position)");
		}

		// check each permission that was requested, and list each 
		// permission that has been flagged as a caution permission
		BOOL caution = FALSE;
		S32 count = 0;
		std::string perms;
		for (S32 i = 0; i < SCRIPT_PERMISSION_EOF; i++)
		{
//			if ((orig_questions & LSCRIPTRunTimePermissionBits[i]) && SCRIPT_QUESTION_IS_CAUTION[i])
// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
			if ( (orig_questions & LSCRIPTRunTimePermissionBits[i]) && 
				 ((SCRIPT_QUESTION_IS_CAUTION[i]) || (notification["payload"]["rlv_notify"].asBoolean())) )
// [/RLVa:KB]
			{
				count++;
				caution = TRUE;

				// add a comma before the permission description if it is not the first permission
				// added to the list or the last permission to check
				if ((count > 1) && (i < SCRIPT_PERMISSION_EOF))
				{
					perms.append(", ");
				}

				perms.append(LLTrans::getString(SCRIPT_QUESTIONS[i]));
			}
		}

		notice.setArg("[PERMISSIONS]", perms);

		// log a chat message as long as at least one requested permission
		// is a caution permission
// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
		if (caution)
		{
			LLChat chat_msg(notice.getString());
			chat_msg.mFromName = SYSTEM_FROM;
			chat_msg.mFromID = LLUUID::null;
			chat_msg.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::addChat(chat_msg);
		}
// [/RLVa:KB]
//		if (caution)
//		{
//			LLChat chat(notice.getString());
//			LLFloaterChat::addChat(chat, FALSE, FALSE);
//		}
	}
}

void script_question_mute(const LLUUID& item_id, const std::string& object_name);

void experiencePermissionBlock(LLUUID experience, LLSD result)
{
	LLSD permission;
	LLSD data;
	permission["permission"] = "Block";
	data[experience.asString()] = permission;
	data["experience"] = experience;
	LLEventPumps::instance().obtain("experience_permission").post(data);
}

bool script_question_cb(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLMessageSystem* msg = gMessageSystem;
	S32 orig = notification["payload"]["questions"].asInteger();
	S32 new_questions = orig;

	if (response["Details"])
	{
		// respawn notification...
		LLNotificationsUtil::add(notification["name"], notification["substitutions"], notification["payload"]);

		// ...with description on top
		LLNotificationsUtil::add("DebitPermissionDetails");
		return false;
	}

	LLUUID experience;
	if (notification["payload"].has("experience"))
	{
		experience = notification["payload"]["experience"].asUUID();
	}

	// check whether permissions were granted or denied
	BOOL allowed = TRUE;
	// the "yes/accept" button is the first button in the template, making it button 0
	// if any other button was clicked, the permissions were denied
	if (option != 0)
	{
		new_questions = 0;
		allowed = FALSE;
	}
	else if (experience.notNull())
	{
		LLSD permission;
		LLSD data;
		permission["permission"] = "Allow";

		data[experience.asString()] = permission;
		data["experience"] = experience;
		LLEventPumps::instance().obtain("experience_permission").post(data);
	}

	LLUUID task_id = notification["payload"]["task_id"].asUUID();
	LLUUID item_id = notification["payload"]["item_id"].asUUID();

	// reply with the permissions granted or denied
	msg->newMessageFast(_PREHASH_ScriptAnswerYes);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_TaskID, task_id);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->addS32Fast(_PREHASH_Questions, new_questions);
	msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));

	// only log a chat message if caution prompts are enabled
//	if (gSavedSettings.getBOOL("PermissionsCautionEnabled"))
// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
	if ( (gSavedSettings.getBOOL("PermissionsCautionEnabled")) || (notification["payload"]["rlv_notify"].asBoolean()) )
// [/RLVa:KB]
	{
		// log a chat message, if appropriate
		notify_cautioned_script_question(notification, response, orig, allowed);
	}

// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
	if ( (allowed) && (notification["payload"].has("rlv_blocked")) )
	{
		RlvUtil::notifyBlocked(notification["payload"]["rlv_blocked"], LLSD().with("OBJECT", notification["payload"]["object_name"]));
	}
// [/RLVa:KB]

	if (response["Mute"]) // mute
	{
		script_question_mute(task_id, notification["payload"]["object_name"].asString());
	}
	if (response["BlockExperience"])
	{
		if (experience.notNull())
		{
			LLViewerRegion* region = gAgent.getRegion();
			if (!region)
				return false;

			LLExperienceCache::instance().setExperiencePermission(experience, std::string("Block"), boost::bind(&experiencePermissionBlock, experience, _1));
		}
	}
	return false;
}

void script_question_mute(const LLUUID& task_id, const std::string& object_name)
{
	LLMuteList::getInstance()->add(LLMute(task_id, object_name, LLMute::OBJECT));

	// purge the message queue of any previously queued requests from the same source. DEV-4879
	class OfferMatcher final : public LLNotifyBoxView::Matcher
	{
	public:
		OfferMatcher(const LLUUID& to_block) : blocked_id(to_block)
		{
		}

		bool matches(const LLNotificationPtr notification) const override
		{
			if (notification->getName() == "ScriptQuestionCaution"
                || notification->getName() == "ScriptQuestion")
			{
			return (notification->getPayload()["task_id"].asUUID() == blocked_id);
			}
			return false;
		}

	private:
		const LLUUID& blocked_id;
	};

	// should do this via the channel
	gNotifyBoxView->purgeMessagesMatching(OfferMatcher(task_id));
}

static LLNotificationFunctorRegistration script_question_cb_reg_1("ScriptQuestion", script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_2("ScriptQuestionCaution", script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_3("ScriptQuestionExperience", script_question_cb);

void process_script_experience_details(const LLSD& experience_details, LLSD args, LLSD payload)
{
	if (experience_details[LLExperienceCache::PROPERTIES].asInteger() & LLExperienceCache::PROPERTY_GRID)
	{
		args["GRID_WIDE"] = LLTrans::getString("Grid-Scope");
	}
	else
	{
		args["GRID_WIDE"] = LLTrans::getString("Land-Scope");
	}
	args["EXPERIENCE"] = LLSLURL("experience", experience_details[LLExperienceCache::EXPERIENCE_ID].asUUID(), "profile").getSLURLString();

	LLNotificationsUtil::add("ScriptQuestionExperience", args, payload);
}

void process_script_question(LLMessageSystem* msg, void** user_data)
{
	// *TODO: Translate owner name -> [FIRST] [LAST]

	LLHost sender = msg->getSender();

	LLUUID taskid;
	LLUUID itemid;
	S32		questions;
	std::string object_name;
	std::string owner_name;
	LLUUID experienceid;

	// taskid -> object key of object requesting permissions
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_TaskID, taskid);
	// itemid -> script asset key of script requesting permissions
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_ItemID, itemid);

	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instanceExists())
	{
		auto& antispam = NACLAntiSpamRegistry::instance();
		if ((taskid.isNull()
			&& antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SCRIPT_DIALOG, itemid, LFIDBearer::NONE))
			|| antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SCRIPT_DIALOG, taskid, LFIDBearer::OBJECT))
			return;
	}
	// NaCl End

	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectName, object_name);
	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectOwner, owner_name);
	msg->getS32Fast(_PREHASH_Data, _PREHASH_Questions, questions);

	if (msg->has(_PREHASH_Experience))
	{
		msg->getUUIDFast(_PREHASH_Experience, _PREHASH_ExperienceID, experienceid);
	}

	// Special case. If the objects are owned by this agent, throttle per-object instead
	// of per-owner. It's common for residents to reset a ton of scripts that re-request
	// permissions, as with tier boxes. UUIDs can't be valid agent names and vice-versa,
	// so we'll reuse the same namespace for both throttle types.
	std::string throttle_name = owner_name;
	std::string self_name;
	LLAgentUI::buildFullname(self_name);
	// NaCl - Antispam
	if (is_spam_filtered(IM_COUNT, false, owner_name == self_name)) return;
	// NaCl End
	if (owner_name == self_name)
	{
		throttle_name = taskid.getString();
	}
	
	// don't display permission requests if this object is muted
	if (LLMuteList::getInstance()->isMuted(taskid)) return;
	
	// throttle excessive requests from any specific user's scripts
	typedef LLKeyThrottle<std::string> LLStringThrottle;
	static LLStringThrottle question_throttle(LLREQUEST_PERMISSION_THROTTLE_LIMIT, LLREQUEST_PERMISSION_THROTTLE_INTERVAL);

	switch (question_throttle.noteAction(throttle_name))
	{
		case LLStringThrottle::THROTTLE_NEWLY_BLOCKED:
			LL_INFOS("Messaging") << "process_script_question throttled"
					<< " owner_name:" << owner_name
					<< LL_ENDL;
			// Fall through

		case LLStringThrottle::THROTTLE_BLOCKED:
			// Escape altogether until we recover
			return;

		case LLStringThrottle::THROTTLE_OK:
			break;
	}

	std::string script_question;
	if (questions)
	{
		bool caution = false;
		S32 count = 0;
		LLSD args;
		const std::string get_obj_slurl(const LLUUID& id, const std::string& name);
		const std::string get_obj_owner_slurl(const LLUUID& obj_id, const std::string& name, bool* group_ownedp = nullptr);
		args["OBJECTNAME"] = get_obj_slurl(taskid, object_name);
		args["NAME"] = get_obj_owner_slurl(taskid, owner_name);
		S32 known_questions = 0;
		bool has_not_only_debit = questions ^ LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_DEBIT];
		// check the received permission flags against each permission
		for (S32 i = 0; i < SCRIPT_PERMISSION_EOF; i++)
		{
			if (questions & LSCRIPTRunTimePermissionBits[i])
			{
				count++;
				known_questions |= LSCRIPTRunTimePermissionBits[i];
				// check whether permission question should cause special caution dialog
				caution |= (SCRIPT_QUESTION_IS_CAUTION[i]);

				if (("ScriptTakeMoney" ==  SCRIPT_QUESTIONS[i]) && has_not_only_debit)
					continue;

				if (SCRIPT_QUESTIONS[i] == "JoinAnExperience")
				{ // Some experience only permissions do not have an explicit permission bit.  Add them here.
					script_question += "    " + LLTrans::getString("ForceSitAvatar") + "\n";
				}

				script_question += "    " + LLTrans::getString(SCRIPT_QUESTIONS[i]) + "\n";
			}
		}

		args["QUESTIONS"] = script_question;

		if (known_questions != questions)
		{
			// This is in addition to the normal dialog.
			// Viewer got a request for not supported/implemented permission 
			LL_WARNS("Messaging") << "Object \"" << object_name << "\" requested " << script_question
								<< " permission. Permission is unknown and can't be granted. Item id: " << itemid
								<< " taskid:" << taskid << LL_ENDL;
		}

		if (known_questions)
		{
			LLSD payload;
			payload["task_id"] = taskid;
			payload["item_id"] = itemid;
			payload["sender"] = sender.getIPandPort();
			payload["questions"] = known_questions;
			payload["object_name"] = object_name;
			payload["owner_name"] = owner_name;

			// check whether cautions are even enabled or not
			const char* notification = "ScriptQuestion";

// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
			if (rlv_handler_t::isEnabled())
			{
				RlvUtil::filterScriptQuestions(questions, payload);

				if ( (questions) && (gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTPERMISSION)) )
				{
					const LLViewerObject* pObj = gObjectList.findObject(taskid);
					if (pObj)
					{
						if ( (pObj->permYouOwner()) && (!pObj->isAttachment()) )
						{
							questions &= ~(LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TAKE_CONTROLS] |
								LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_ATTACH]);
						}
						else
						{
							questions &= ~(LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TAKE_CONTROLS]);
						}
						payload["rlv_notify"] = !pObj->permYouOwner();
					}
				}
			}

			if ( (!caution) && (!questions) )
			{
				LLNotifications::instance().forceResponse(
					LLNotification::Params(notification).substitutions(args).payload(payload), 0/*YES*/);
				return;
			}
// [/RLVa:KB]
			if (caution && gSavedSettings.getBOOL("PermissionsCautionEnabled"))
			{
				args["FOOTERTEXT"] = (count > 1) ? LLTrans::getString("AdditionalPermissionsRequestHeader") + '\n' + script_question : LLStringUtil::null;
				notification = "ScriptQuestionCaution";
			}
			else if (experienceid.notNull())
			{
				payload["experience"] = experienceid;
					LLExperienceCache::instance().get(experienceid, boost::bind(process_script_experience_details, _1, args, payload));
				return;
			}

			LLNotificationsUtil::add(notification, args, payload);
		}
	}
}


void process_derez_container(LLMessageSystem* msg, void**)
{
	LL_WARNS("Messaging") << "call to deprecated process_derez_container" << LL_ENDL;
}

void container_inventory_arrived(LLViewerObject* object,
								 LLInventoryObject::object_list_t* inventory,
								 S32 serial_num,
								 void* data)
{
	LL_DEBUGS("Messaging") << "container_inventory_arrived()" << LL_ENDL;
	if (gAgentCamera.cameraMouselook())
	{
		gAgentCamera.changeCameraToDefault();
	}

	LLInventoryPanel* active_panel = LLInventoryPanel::getActiveInventoryPanel();

	if (inventory->size() > 2)
	{
		// create a new inventory category to put this in
		LLUUID cat_id;
		cat_id = gInventory.createNewCategory(gInventory.getRootFolderID(),
											  LLFolderType::FT_NONE,
											  LLTrans::getString("AcquiredItems"));

		LLInventoryObject::object_list_t::const_iterator it = inventory->begin();
		LLInventoryObject::object_list_t::const_iterator end = inventory->end();
		for (; it != end; ++it)
		{
			if ((*it)->getType() != LLAssetType::AT_CATEGORY)
			{
				LLInventoryObject* obj = (LLInventoryObject*)(*it);
				LLInventoryItem* item = (LLInventoryItem*)(obj);
				LLUUID item_id;
				item_id.generate();
				time_t creation_date_utc = time_corrected();
				LLPointer<LLViewerInventoryItem> new_item
					= new LLViewerInventoryItem(item_id,
												cat_id,
												item->getPermissions(),
												item->getAssetUUID(),
												item->getType(),
												item->getInventoryType(),
												item->getName(),
												item->getDescription(),
												LLSaleInfo::DEFAULT,
												item->getFlags(),
												creation_date_utc);
				new_item->updateServer(TRUE);
				gInventory.updateItem(new_item);
			}
		}
		gInventory.notifyObservers();
		if (active_panel)
		{
			active_panel->setSelection(cat_id, TAKE_FOCUS_NO);
		}
	}
	else if (inventory->size() == 2)
	{
		// we're going to get one fake root category as well as the
		// one actual object
		LLInventoryObject::object_list_t::iterator it = inventory->begin();

		if ((*it)->getType() == LLAssetType::AT_CATEGORY)
		{
			++it;
		}

		LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
		const LLUUID category = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(item->getType()));

		LLUUID item_id;
		item_id.generate();
		time_t creation_date_utc = time_corrected();
		LLPointer<LLViewerInventoryItem> new_item
			= new LLViewerInventoryItem(item_id, category,
										item->getPermissions(),
										item->getAssetUUID(),
										item->getType(),
										item->getInventoryType(),
										item->getName(),
										item->getDescription(),
										LLSaleInfo::DEFAULT,
										item->getFlags(),
										creation_date_utc);
		new_item->updateServer(TRUE);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
		if (active_panel)
		{
			active_panel->setSelection(item_id, TAKE_FOCUS_NO);
		}
	}

	// we've got the inventory, now delete this object if this was a take
	BOOL delete_object = (BOOL)(intptr_t)data;
	LLViewerRegion* region = gAgent.getRegion();
	if (delete_object && region)
	{
		gMessageSystem->newMessageFast(_PREHASH_ObjectDelete);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		const U8 NO_FORCE = 0;
		gMessageSystem->addU8Fast(_PREHASH_Force, NO_FORCE);
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		gMessageSystem->sendReliable(region->getHost());
	}
}

// method to format the time.
std::string formatted_time(const time_t& the_time)
{
	std::string timestr;
	timeToFormattedString(the_time, gSavedSettings.getString("TimestampFormat"), timestr);
	char buffer[30]; /* Flawfinder: ignore */
	LLStringUtil::copy(buffer, timestr.c_str(), 30);
	buffer[24] = '\0';
	return std::string(buffer);
}


void process_teleport_failed(LLMessageSystem* msg, void**)
{
	std::string message_id;		// Tag from server, like "RegionEntryAccessBlocked"
	std::string big_reason;		// Actual message to display
	LLSD args;

	// Let the interested parties know that teleport failed.
	LLViewerParcelMgr::getInstance()->onTeleportFailed();

	// if we have additional alert data
	if (msg->has(_PREHASH_AlertInfo) && msg->getSizeFast(_PREHASH_AlertInfo, _PREHASH_Message) > 0)
	{
		// Get the message ID
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, message_id);
		big_reason = LLAgent::sTeleportErrorMessages[message_id];
		if (big_reason.empty())
		{
			// Nothing found in the map - use what the server returned in the original message block
			msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, big_reason);
		}
		args["REASON"] = big_reason;

		LLSD llsd_block;
		std::string llsd_raw;
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams, llsd_raw);
		if (!llsd_raw.empty())
		{
			std::istringstream llsd_data(llsd_raw);
			if (!LLSDSerialize::deserialize(llsd_block, llsd_data, llsd_raw.length()))
			{
				LL_WARNS() << "process_teleport_failed: Attempted to read alert parameter data into LLSD but failed:" << llsd_raw << LL_ENDL;
			}
			else
			{
				if(llsd_block.has("REGION_NAME"))
				{
					std::string region_name = llsd_block["REGION_NAME"].asString();
					if(!region_name.empty())
					{
						LLStringUtil::format_map_t name_args;
						name_args["[REGION_NAME]"] = region_name;
						LLStringUtil::format(big_reason, name_args);
						args["REASON"] = big_reason;
					}
				}
				// change notification name in this special case
				if (handle_teleport_access_blocked(llsd_block, message_id, args["REASON"]))
				{
					if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
					{
						gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
					}
					return;
				}
			}
		}

	}
	else
	{	// Extra message payload not found - use what the simulator sent
		msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, message_id);

		big_reason = LLAgent::sTeleportErrorMessages[message_id];
		if (!big_reason.empty())
		{	// Substitute verbose reason from the local map
			args["REASON"] = big_reason;
		}
		else
		{	// Nothing found in the map - use what the server returned
			args["REASON"] = message_id;
		}
	}

	LLNotificationsUtil::add("CouldNotTeleportReason", args);

	if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
	{
		gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
	}
}

void process_teleport_local(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Got teleport notification for wrong agent!" << LL_ENDL;
		return;
	}

	U32 location_id;
	LLVector3 pos, look_at;
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);

	if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
	{
		if (gAgent.getTeleportState() == LLAgent::TELEPORT_LOCAL)
		{
			// To prevent TeleportStart messages re-activating the progress screen right
			// after tp, keep the teleport state and let progress screen clear it after a short delay
			// (progress screen is active but not visible)  *TODO: remove when SVC-5290 is fixed
			gTeleportDisplayTimer.reset();
			gTeleportDisplay = TRUE;
		}
		else
		{
			gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
		}
	}

	// Sim tells us whether the new position is off the ground
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING || gSavedSettings.getBOOL("LiruFlyAfterTeleport"))
	{
		gAgent.setFlying(TRUE);
	}
	else
	{
		gAgent.setFlying(FALSE);
	}

	gAgent.setPositionAgent(pos);
	gAgentCamera.slamLookAt(look_at);

	if (!(gAgent.getTeleportKeepsLookAt() && LLViewerJoystick::getInstance()->getOverrideCamera()) && gSavedSettings.getBOOL("OptionRotateCamAfterLocalTP"))
	{
		gAgentCamera.resetView(TRUE, TRUE);
	}

	// send camera update to new region
	gAgentCamera.updateCamera();

	send_agent_update(TRUE, TRUE);

	// Let the interested parties know we've teleported.
	// Vadim *HACK: Agent position seems to get reset (to render position?)
	//              on each frame, so we have to pass the new position manually.
	LLViewerParcelMgr::getInstance()->onTeleportFinished(true, gAgent.getPosGlobalFromAgent(pos));
}

void send_simple_im(const LLUUID& to_id,
					const std::string& message,
					EInstantMessage dialog,
					const LLUUID& id)
{
	std::string my_name;
	LLAgentUI::buildFullname(my_name);
	send_improved_im(to_id,
					 my_name,
					 message,
					 IM_ONLINE,
					 dialog,
					 id,
					 NO_TIMESTAMP,
					 (U8*)EMPTY_BINARY_BUCKET,
					 EMPTY_BINARY_BUCKET_SIZE);
}

void send_group_notice(const LLUUID& group_id,
					   const std::string& subject,
					   const std::string& message,
					   const LLInventoryItem* item)
{
	// Put this notice into an instant message form.
	// This will mean converting the item to a binary bucket,
	// and the subject/message into a single field.
	std::string my_name;
	LLAgentUI::buildFullname(my_name);

	// Combine subject + message into a single string.
	std::ostringstream subject_and_message;
	// TODO: turn all existing |'s into ||'s in subject and message.
	subject_and_message << subject << "|" << message;

	// Create an empty binary bucket.
	U8 bin_bucket[MAX_INVENTORY_BUFFER_SIZE];
	U8* bucket_to_send = bin_bucket;
	bin_bucket[0] = '\0';
	S32 bin_bucket_size = EMPTY_BINARY_BUCKET_SIZE;
	// If there is an item being sent, pack it into the binary bucket.
	if (item)
	{
		LLSD item_def;
		item_def["item_id"] = item->getUUID();
		item_def["owner_id"] = item->getPermissions().getOwner();
		std::ostringstream ostr;
		LLSDSerialize::serialize(item_def, ostr, LLSDSerialize::LLSD_XML);
		bin_bucket_size = ostr.str().copy(
			(char*)bin_bucket, ostr.str().size());
		bin_bucket[bin_bucket_size] = '\0';
	}
	else
	{
		bucket_to_send = (U8*)EMPTY_BINARY_BUCKET;
	}
   

	send_improved_im(
			group_id,
			my_name,
			subject_and_message.str(),
			IM_ONLINE,
			IM_GROUP_NOTICE,
			LLUUID::null,
			NO_TIMESTAMP,
			bucket_to_send,
			bin_bucket_size);
}

void send_lures(const LLSD& notification, const LLSD& response)
{
	std::string text = response["message"].asString();
	LLSLURL slurl;
	LLAgentUI::buildSLURL(slurl);
	text.append("\r\n").append(slurl.getSLURLString());

// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
	const std::string& rlv_hidden(RlvStrings::getString(RLV_STRING_HIDDEN));
	if ( (RlvActions::hasBehaviour(RLV_BHVR_SENDIM)) || (RlvActions::hasBehaviour(RLV_BHVR_SENDIMTO)) )
	{
		// Filter the lure message if one of the recipients of the lure can't be sent an IM to
		for (auto const& entry : notification["payload"]["ids"].array())
		{
			if (!RlvActions::canSendIM(entry.asUUID()))
			{
				text = rlv_hidden;
				break;
			}
		}
	}
// [/RLVa:KB]

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_StartLure);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_Info);
	msg->addU8Fast(_PREHASH_LureType, (U8)0); // sim will fill this in.
	msg->addStringFast(_PREHASH_Message, text);
// [RLVa:KB] - Checked: 2014-03-31 (Catznip-3.6)
	bool fRlvHideName = gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES);
	bool fRlvNoNearbyNames = gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS);
// [/RLVa:KB]
	for (auto const& entry : notification["payload"]["ids"].array())
	{
		LLUUID target_id = entry.asUUID();

		msg->nextBlockFast(_PREHASH_TargetData);
		msg->addUUIDFast(_PREHASH_TargetID, target_id);

		// Record the offer.
		if (notification["payload"]["ids"].size() < 10) // Singu Note: Do NOT spam chat!
		{
// [RLVa:KB] - Checked: 2014-03-31 (Catznip-3.6)
			fRlvHideName |= notification["payload"]["rlv_shownames"].asBoolean();
// [/RLVa:KB]
			std::string target_name;
			gCacheName->getFullName(target_id, target_name);  // for im log filenames
			LLSD args;
// [RLVa:KB] - Checked: 2014-03-31 (Catznip-3.6)
			if (fRlvNoNearbyNames && RlvUtil::isNearbyAgent(target_id))
				target_name = rlv_hidden;
			else if (fRlvHideName)
				target_name = RlvStrings::getAnonym(target_name);
			else
// [/RLVa:KB]
				target_name = LLAvatarActions::getSLURL(target_id);
			args["TO_NAME"] = target_name;

			LLSD payload;

			//*TODO please rewrite all keys to the same case, lower or upper
			payload["from_id"] = target_id;
			payload["SUPPRESS_TOAST"] = true;
			LLNotificationsUtil::add("TeleportOfferSent", args, payload);

			/* Singu TODO?
// [RLVa:KB] - Checked: 2014-03-31 (Catznip-3.6)
			if (!fRlvHideName)
				LLRecentPeople::instance().add(target_id);
// [/RLVa:KB]
//			LLRecentPeople::instance().add(target_id);
			*/
		}
	}
	gAgent.sendReliableMessage();
}

bool handle_lure_callback(const LLSD& notification, const LLSD& response)
{
	static const unsigned OFFER_RECIPIENT_LIMIT = 250;
	if (notification["payload"]["ids"].size() > OFFER_RECIPIENT_LIMIT)
	{
		// More than OFFER_RECIPIENT_LIMIT targets will overload the message
		// producing an llerror.
		LLSD args;
		args["OFFERS"] = notification["payload"]["ids"].size();
		args["LIMIT"] = static_cast<int>(OFFER_RECIPIENT_LIMIT);
		LLNotificationsUtil::add("TooManyTeleportOffers", args);
		return false;
	}

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (0 == option)
	{
		send_lures(notification, response);
	}

	return false;
}

void handle_lure(const LLUUID& invitee)
{
	handle_lure(uuid_vec_t{invitee});
}

// Prompt for a message to the invited user.
void handle_lure(const uuid_vec_t& ids)
{
	if (ids.empty()) return;

	if (!gAgent.getRegion()) return;

	LLSD edit_args;
// [RLVa:KB] - Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.0.0a
	edit_args["REGION"] = (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) ? gAgent.getRegion()->getName() : RlvStrings::getString(RLV_STRING_HIDDEN);
// [/RLVa:KB]
//	edit_args["REGION"] = gAgent.getRegion()->getName();

	LLSD payload;
// [RLVa:KB] - Checked: RLVa-2.0.1
	bool fRlvShouldHideNames = false;
	for (const LLUUID& idAgent : ids)
	{
		// Only allow offering teleports if everyone is a @tplure exception or able to map this avie under @showloc=n
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		{
			const LLRelationship* pBuddyInfo = LLAvatarTracker::instance().getBuddyInfo(idAgent);
			if ( (!gRlvHandler.isException(RLV_BHVR_TPLURE, idAgent, RLV_CHECK_PERMISSIVE)) &&
				 ((!pBuddyInfo) || (!pBuddyInfo->isOnline()) || (!pBuddyInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION))) )
			{
				return;
			}
		}
		fRlvShouldHideNames |= !RlvActions::canShowName(RlvActions::SNC_TELEPORTOFFER);
		payload["ids"].append(idAgent);
	}
	payload["rlv_shownames"] = fRlvShouldHideNames;
// [/RLVa:KB]
// 	for (std::vector<LLUUID>::const_iterator it = ids.begin();
// 		it != ids.end();
// 		++it)
// 	{
// 		payload["ids"].append(*it);
// 	}
	if (gAgent.isGodlike())
	{
		LLNotificationsUtil::add("OfferTeleportFromGod", edit_args, payload, handle_lure_callback);
	}
	else
	{
		LLNotificationsUtil::add("OfferTeleport", edit_args, payload, handle_lure_callback);
	}
}

bool teleport_request_callback(const LLSD& notification, const LLSD& response)
{
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	if (from_id.isNull())
	{
		LL_WARNS() << "from_id is NULL" << LL_ENDL;
		return false;
	}

	std::string from_name;
	gCacheName->getFullName(from_id, from_name);

	if (LLMuteList::getInstance()->isMuted(from_id) && !LLMuteList::getInstance()->isLinden(from_name))
	{
		return false;
	}

	S32 option = 0;
	if (response.isInteger())
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotificationsUtil::getSelectedOption(notification, response);
	}

	switch (option)
	{
	// Yes
	case 0:
		{
			LLSD dummy_notification;
			dummy_notification["payload"]["ids"][0] = from_id;

			LLSD dummy_response;
			dummy_response["message"] = response["message"];

			send_lures(dummy_notification, dummy_response);
		}
		break;

	// No
	case 1:
	default:
		break;
	}

	return false;
}

static LLNotificationFunctorRegistration teleport_request_callback_reg("TeleportRequest", teleport_request_callback);

void send_improved_im(const LLUUID& to_id,
							const std::string& name,
							const std::string& message,
							U8 offline,
							EInstantMessage dialog,
							const LLUUID& id,
							U32 timestamp, 
							const U8* binary_bucket,
							S32 binary_bucket_size)
{
	pack_instant_message(
		gMessageSystem,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		to_id,
		name,
		message,
		offline,
		dialog,
		id,
		0,
		LLUUID::null,
		gAgent.getPositionAgent(),
		timestamp,
		binary_bucket,
		binary_bucket_size);
	gAgent.sendReliableMessage();
}


void send_places_query(const LLUUID& query_id,
					   const LLUUID& trans_id,
					   const std::string& query_text,
					   U32 query_flags,
					   S32 category,
					   const std::string& sim_name)
{
	LLMessageSystem* msg = gMessageSystem;

	msg->newMessage("PlacesQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->addUUID("QueryID", query_id);
	msg->nextBlock("TransactionData");
	msg->addUUID("TransactionID", trans_id);
	msg->nextBlock("QueryData");
	msg->addString("QueryText", query_text);
	msg->addU32("QueryFlags", query_flags);
	msg->addS8("Category", (S8)category);
	msg->addString("SimName", sim_name);
	gAgent.sendReliableMessage();
}

// Deprecated in favor of cap "UserInfo"
void process_user_info_reply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "process_user_info_reply - "
				<< "wrong agent id." << LL_ENDL;
	}
	
	BOOL im_via_email;
	msg->getBOOLFast(_PREHASH_UserData, _PREHASH_IMViaEMail, im_via_email);
	std::string email;
	msg->getStringFast(_PREHASH_UserData, _PREHASH_EMail, email);
	std::string dir_visibility;
	msg->getString("UserData", "DirectoryVisibility", dir_visibility);

    // For Message based user info information the is_verified is assumed to be false.
	LLFloaterPreference::updateUserInfo(dir_visibility, im_via_email, email);
	LLFloaterPostcard::updateUserInfo(email);
}


//---------------------------------------------------------------------------
// Script Dialog
//---------------------------------------------------------------------------

const S32 SCRIPT_DIALOG_MAX_BUTTONS = 12;
const char* SCRIPT_DIALOG_HEADER = "Script Dialog:\n";

bool callback_script_dialog(const LLSD& notification, const LLSD& response)
{
	LLNotificationForm form(notification["form"]);

	std::string rtn_text;
	S32 button_idx = LLNotification::getSelectedOption(notification, response);

	if (notification["payload"].has("textbox"))
	{
		rtn_text = response["message"].asString();
	}
	else
	{
		rtn_text = LLNotification::getSelectedOptionName(response);
	}

	// Button -2 = Mute
	// Button -1 = Ignore - no processing needed for this button
	// Buttons 0 and above = dialog choices

	if (-2 == button_idx)
	{
		std::string object_name = notification["payload"]["object_name"].asString();
		LLUUID object_id = notification["payload"]["object_id"].asUUID();
		LLMute mute(object_id, object_name, LLMute::OBJECT);
		if (LLMuteList::getInstance()->add(mute))
		{
			// This call opens the sidebar, displays the block list, and highlights the newly blocked
			// object in the list so the user can see that their block click has taken effect.
			LLFloaterMute::showInstance()->selectMute(object_id);
		}
	}

	if (0 <= button_idx)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("ScriptDialogReply");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("ObjectID", notification["payload"]["object_id"].asUUID());
		msg->addS32("ChatChannel", notification["payload"]["chat_channel"].asInteger());
		msg->addS32("ButtonIndex", button_idx);
		msg->addString("ButtonLabel", rtn_text);
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
	}

	return false;
}

static LLNotificationFunctorRegistration callback_script_dialog_reg_1("ScriptDialog", callback_script_dialog);
static LLNotificationFunctorRegistration callback_script_dialog_reg_2("ScriptDialogGroup", callback_script_dialog);

void process_script_dialog(LLMessageSystem* msg, void**)
{
	S32 i;
	LLSD payload;

	LLUUID object_id;
	msg->getUUID("Data", "ObjectID", object_id);

	// NaCl - Antispam Registry
	auto antispam = NACLAntiSpamRegistry::getIfExists();
	if (antispam && antispam->checkQueue(NACLAntiSpamRegistry::QUEUE_SCRIPT_DIALOG, object_id, LFIDBearer::OBJECT))
		return;
	// NaCl End

	std::string first_name;
	msg->getString("Data", "FirstName", first_name);
	bool const is_group = first_name.empty();

	//	For compability with OS grids first check for presence of extended packet before fetching data.
    LLUUID owner_id;
	if (gMessageSystem->getNumberOfBlocks("OwnerData") > 0)
	{
    msg->getUUID("OwnerData", "OwnerID", owner_id);

	// NaCl - Antispam Registry
	if (antispam && antispam->checkQueue(NACLAntiSpamRegistry::QUEUE_SCRIPT_DIALOG, owner_id, is_group ? LFIDBearer::GROUP : LFIDBearer::AVATAR))
		return;
	// NaCl End
	}

	bool has_owner = owner_id.notNull();

	// NaCl - Antispam
	if (!has_owner ? is_spam_filtered(IM_COUNT, LLAvatarActions::isFriend(object_id), object_id == gAgentID)
		: is_spam_filtered(IM_COUNT, LLAvatarActions::isFriend(owner_id), !is_group && owner_id == gAgentID)) return;
	// NaCl End

	if (LLMuteList::getInstance()->isMuted(object_id) || LLMuteList::getInstance()->isMuted(owner_id))
	{
		return;
	}

	auto chatter = gObjectList.findObject(object_id);
	// Singu Note: Try to get Owner whenever possible
	if (chatter && has_owner) chatter->mOwnerID = owner_id;

	std::string message; 
	std::string last_name;
	std::string object_name;

	S32 chat_channel;
	msg->getString("Data", "LastName", last_name);
	msg->getString("Data", "ObjectName", object_name);
	msg->getString("Data", "Message", message);
	msg->getS32("Data", "ChatChannel", chat_channel);

		// unused for now
	LLUUID image_id;
	msg->getUUID("Data", "ImageID", image_id);

	payload["sender"] = msg->getSender().getIPandPort();
	payload["object_id"] = object_id;
	payload["object_name"] = object_name;
	payload["chat_channel"] = chat_channel;

	// build up custom form
	S32 button_count = msg->getNumberOfBlocks("Buttons");
	if (button_count > SCRIPT_DIALOG_MAX_BUTTONS)
	{
		LL_WARNS() << "Too many script dialog buttons - omitting some" << LL_ENDL;
		button_count = SCRIPT_DIALOG_MAX_BUTTONS;
	}

	bool is_text_box = false;
	LLNotificationForm form;
	for (i = 0; i < button_count; i++)
	{
		std::string tdesc;
		msg->getString("Buttons", "ButtonLabel", tdesc, i);
		if (is_text_box = tdesc == TEXTBOX_MAGIC_TOKEN)
			break;
		form.addElement("button", std::string(tdesc));
	}

	LLSD query_string;
	query_string["owner"] = owner_id;
	if (rlv_handler_t::isEnabled() && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && !is_group && RlvUtil::isNearbyAgent(owner_id))
	{
		query_string["rlv_shownames"] = true;
	}

	if (const auto& obj = chatter ? chatter : gObjectList.findObject(owner_id)) // Fallback on the owner, if the chatter isn't present
	{
		auto& slurl = query_string["slurl"];
		const auto& region = obj->getRegion();
		if (rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC) && LLWorld::instance().isRegionListed(region))
			slurl = RlvStrings::getString(RLV_STRING_HIDDEN_REGION);
		else
		{
			const auto& pos = obj->getPositionRegion();
			S32 x = ll_round((F32)fmod((F64)pos.mV[VX], (F64)REGION_WIDTH_METERS));
			S32 y = ll_round((F32)fmod((F64)pos.mV[VY], (F64)REGION_WIDTH_METERS));
			S32 z = ll_round((F32)pos.mV[VZ]);
			std::ostringstream location;
			location << region->getName() << '/' << x << '/' << y << '/' << z;
			if (chatter != obj) location << "?owner_not_object";
			slurl = location.str();
		}
	}
	query_string["name"] = object_name;
	query_string["groupowned"] = is_group;
	object_name = LLSLURL("objectim", object_id, LLURI::mapToQueryString(query_string)).getSLURLString();

	LLSD args;
	args["TITLE"] = object_name;
	args["MESSAGE"] = LLStringUtil::null;
	args["SCRIPT_MESSAGE"] = message;
	args["CHANNEL"] = chat_channel;
	LLNotificationPtr notification;
	char const* name = (is_group && !is_text_box) ? "GROUPNAME" : "NAME";
	args[name] = has_owner ? is_group ? LLGroupActions::getSLURL(owner_id) : LLAvatarActions::getSLURL(owner_id) :
		is_group ? last_name : LLCacheName::buildFullName(first_name, last_name);
	if (is_text_box)
	{
		payload["textbox"] = true;
		LLNotificationsUtil::add("ScriptTextBoxDialog", args, payload, callback_script_dialog);
	}
	else if (!is_group)
	{
		notification = LLNotifications::instance().add(
			LLNotification::Params("ScriptDialog").substitutions(args).payload(payload).form_elements(form.asLLSD()));
	}
	else
	{
		notification = LLNotifications::instance().add(
			LLNotification::Params("ScriptDialogGroup").substitutions(args).payload(payload).form_elements(form.asLLSD()));
	}
}

//---------------------------------------------------------------------------


std::vector<LLSD> gLoadUrlList;

bool callback_load_url(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (0 == option)
	{
		LLWeb::loadURL(notification["payload"]["url"].asString());
	}

	return false;
}

static LLNotificationFunctorRegistration callback_load_url_reg("LoadWebPage", callback_load_url);

// We've got the name of the person or group that owns the object hurling the url.
// Display confirmation dialog.
void callback_load_url_name(const LLUUID& id, const std::string& full_name, bool is_group)
{
	std::vector<LLSD>::iterator it;
	for (it = gLoadUrlList.begin(); it != gLoadUrlList.end();)
	{
		LLSD load_url_info = *it;
		if (load_url_info["owner_id"].asUUID() == id)
		{
			it = gLoadUrlList.erase(it);

			std::string owner_name;
			if (is_group)
			{
				owner_name = full_name + LLTrans::getString("Group");
			}
			else
			{
				owner_name = full_name;
			}

			// For legacy name-only mutes.
			if (LLMuteList::getInstance()->isMuted(LLUUID::null, owner_name))
			{
				continue;
			}
			LLSD args;
			args["URL"] = load_url_info["url"].asString();
			args["MESSAGE"] = load_url_info["message"].asString();
			args["OBJECTNAME"] = load_url_info["object_name"].asString();
			args["NAME"] = owner_name;

			LLNotificationsUtil::add("LoadWebPage", args, load_url_info);
		}
		else
		{
			++it;
		}
	}
}

// We've got the name of the person who owns the object hurling the url.
void callback_load_url_avatar_name(const LLUUID& id, const LLAvatarName& av_name)
{
	callback_load_url_name(id, av_name.getUserName(), false);
}

void process_load_url(LLMessageSystem* msg, void**)
{
	LLUUID object_id;
	LLUUID owner_id;
	BOOL owner_is_group;
	char object_name[256];		/* Flawfinder: ignore */
	char message[256];		/* Flawfinder: ignore */
	char url[256];		/* Flawfinder: ignore */

	msg->getString("Data", "ObjectName", 256, object_name);
	msg->getUUID("Data", "ObjectID", object_id);
	msg->getUUID("Data", "OwnerID", owner_id);

	// NaCl - Antispam
	if (owner_id.isNull() ? is_spam_filtered(IM_COUNT, LLAvatarActions::isFriend(object_id), object_id == gAgentID)
		: is_spam_filtered(IM_COUNT, LLAvatarActions::isFriend(owner_id), owner_id == gAgentID)) return;
	// NaCl End

	msg->getBOOL("Data", "OwnerIsGroup", owner_is_group);
	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instanceExists())
	{
		auto& antispam = NACLAntiSpamRegistry::instance();
		if ((owner_id.isNull()
			&& antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SCRIPT_DIALOG, object_id, LFIDBearer::OBJECT))
			|| antispam.checkQueue(NACLAntiSpamRegistry::QUEUE_SCRIPT_DIALOG, owner_id, owner_is_group ? LFIDBearer::GROUP : LFIDBearer::AVATAR))
			return;
	}
	// NaCl End

	msg->getString("Data", "Message", 256, message);
	msg->getString("Data", "URL", 256, url);

	LLSD payload;
	payload["object_id"] = object_id;
	payload["owner_id"] = owner_id;
	payload["owner_is_group"] = owner_is_group;
	payload["object_name"] = object_name;
	payload["message"] = message;
	payload["url"] = url;

	// URL is safety checked in load_url above

	// Check if object or owner is muted
	if (LLMuteList::getInstance()->isMuted(object_id, object_name) ||
	    LLMuteList::getInstance()->isMuted(owner_id))
	{
		LL_INFOS("Messaging") << "Ignoring load_url from muted object/owner." << LL_ENDL;
		return;
	}

	// Add to list of pending name lookups
	gLoadUrlList.push_back(payload);

	if (owner_is_group)
	{
		gCacheName->getGroup(owner_id, boost::bind(&callback_load_url_name, _1, _2, _3));
	}
	else
	{
		LLAvatarNameCache::get(owner_id, boost::bind(&callback_load_url_avatar_name, _1, _2));
	}
}


void callback_download_complete(void** data, S32 result, LLExtStat ext_status)
{
	std::string* filepath = (std::string*)data;
	LLSD args;
	args["DOWNLOAD_PATH"] = *filepath;
	LLNotificationsUtil::add("FinishedRawDownload", args);
	delete filepath;
}


void process_initiate_download(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Initiate download for wrong agent" << LL_ENDL;
		return;
	}

	std::string sim_filename;
	std::string viewer_filename;
	msg->getString("FileData", "SimFilename", sim_filename);
	msg->getString("FileData", "ViewerFilename", viewer_filename);

	if (!gXferManager->validateFileForRequest(viewer_filename))
	{
		LL_WARNS() << "SECURITY: Unauthorized download to local file " << viewer_filename << LL_ENDL;
		return;
	}
	gXferManager->requestFile(viewer_filename,
		sim_filename,
		LL_PATH_NONE,
		msg->getSender(),
		FALSE,	// don't delete remote
		callback_download_complete,
		(void**)new std::string(viewer_filename));
}


void process_script_teleport_request(LLMessageSystem* msg, void**)
{
	if (!gSavedSettings.getBOOL("ScriptsCanShowUI")) return;
	
	// NaCl - Antispam
	if (is_spam_filtered(IM_COUNT, false, false)) return;
	// NaCl End

	std::string object_name;
	std::string sim_name;
	LLVector3 pos;
	LLVector3 look_at;

	msg->getString("Data", "ObjectName", object_name);
	msg->getString("Data", "SimName", sim_name);
	msg->getVector3("Data", "SimPosition", pos);
	msg->getVector3("Data", "LookAt", look_at);

	gFloaterWorldMap->trackURL(sim_name, (S32)pos.mV[VX], (S32)pos.mV[VY], (S32)pos.mV[VZ]);
	LLFloaterWorldMap::show(true);

	// remove above two lines and replace with below line
	// to re-enable parcel browser for llMapDestination()
	// LLURLDispatcher::dispatch(LLSLURL::buildSLURL(sim_name, (S32)pos.mV[VX], (S32)pos.mV[VY], (S32)pos.mV[VZ]), FALSE);
}

void process_covenant_reply(LLMessageSystem* msg, void**)
{
	LLUUID covenant_id, estate_owner_id;
	std::string estate_name;
	U32 covenant_timestamp;
	msg->getUUID("Data", "CovenantID", covenant_id);
	msg->getU32("Data", "CovenantTimestamp", covenant_timestamp);
	msg->getString("Data", "EstateName", estate_name);
	msg->getUUID("Data", "EstateOwnerID", estate_owner_id);

	LLPanelEstateCovenant::updateEstateName(estate_name);
	LLPanelLandCovenant::updateEstateName(estate_name);
	LLPanelEstateInfo::updateEstateName(estate_name);
	LLFloaterBuyLand::updateEstateName(estate_name);

	LLPanelEstateCovenant::updateEstateOwnerID(estate_owner_id);
	LLPanelLandCovenant::updateEstateOwnerID(estate_owner_id);
	LLPanelEstateInfo::updateEstateOwnerID(estate_owner_id);
	LLFloaterBuyLand::updateEstateOwnerID(estate_owner_id);

	// standard message, not from system
	std::string last_modified;
	if (covenant_timestamp == 0)
	{
		last_modified = LLTrans::getString("covenant_never_modified");
	}
	else
	{
		last_modified = LLTrans::getString("covenant_modified") + ' ' + formatted_time((time_t)covenant_timestamp);
	}

	LLPanelEstateCovenant::updateLastModified(last_modified);
	LLPanelLandCovenant::updateLastModified(last_modified);
	LLFloaterBuyLand::updateLastModified(last_modified);
	
	// load the actual covenant asset data
	const BOOL high_priority = TRUE;
	if (covenant_id.notNull())
	{
		gAssetStorage->getEstateAsset(gAgent.getRegionHost(),
									gAgent.getID(),
									gAgent.getSessionID(),
									covenant_id,
                                    LLAssetType::AT_NOTECARD,
									ET_Covenant,
                                    onCovenantLoadComplete,
		                              nullptr,
									high_priority);
	}
	else
	{
		std::string covenant_text;
		if (estate_owner_id.isNull())
		{
			// mainland
			covenant_text = LLTrans::getString("RegionNoCovenant");
		}
		else
		{
			covenant_text = LLTrans::getString("RegionNoCovenantOtherOwner");
		}
		LLPanelEstateCovenant::updateCovenantText(covenant_text, covenant_id);
		LLPanelLandCovenant::updateCovenantText(covenant_text);
		LLFloaterBuyLand::updateCovenantText(covenant_text, covenant_id);
	}
}

void onCovenantLoadComplete(LLVFS* vfs,
					const LLUUID& asset_uuid,
					LLAssetType::EType type,
					void* user_data, S32 status, LLExtStat ext_status)
{
	LL_DEBUGS("Messaging") << "onCovenantLoadComplete()" << LL_ENDL;
	std::string covenant_text;
	if (0 == status)
	{
		LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
		
		S32 file_length = file.getSize();
		
		std::vector<char> buffer(file_length + 1);
		file.read((U8*)&buffer[0], file_length);
		// put a EOS at the end
		buffer[file_length] = '\0';

		if ((file_length > 19) && !strncmp(&buffer[0], "Linden text version", 19))
		{
			LLViewerTextEditor * editor = new LLViewerTextEditor(std::string("temp"), LLRect(0,0,0,0), file_length+1);
			if( !editor->importBuffer( &buffer[0], file_length+1 ) )
			{
				LL_WARNS("Messaging") << "Problem importing estate covenant." << LL_ENDL;
				covenant_text = "Problem importing estate covenant.";
			}
			else
			{
				// Version 0 (just text, doesn't include version number)
				covenant_text = editor->getText();
			}
			delete editor;
		}
		else
		{
			LL_WARNS("Messaging") << "Problem importing estate covenant: Covenant file format error." << LL_ENDL;
			covenant_text = "Problem importing estate covenant: Covenant file format error.";
		}
	}
	else
	{
		LLViewerStats::getInstance()->incStat( LLViewerStats::ST_DOWNLOAD_FAILED );
		
		if (LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
		    LL_ERR_FILE_EMPTY == status)
		{
			covenant_text = "Estate covenant notecard is missing from database.";
		}
		else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
		{
			covenant_text = "Insufficient permissions to view estate covenant.";
		}
		else
		{
			covenant_text = "Unable to load estate covenant at this time.";
		}
		
		LL_WARNS("Messaging") << "Problem loading notecard: " << status << LL_ENDL;
	}
	LLPanelEstateCovenant::updateCovenantText(covenant_text, asset_uuid);
	LLPanelLandCovenant::updateCovenantText(covenant_text);
	LLFloaterBuyLand::updateCovenantText(covenant_text, asset_uuid);
}


void process_feature_disabled_message(LLMessageSystem* msg, void**)
{
	// Handle Blacklisted feature simulator response...
	LLUUID	agentID;
	LLUUID	transactionID;
	std::string	messageText;
	msg->getStringFast(_PREHASH_FailureInfo, _PREHASH_ErrorMessage, messageText, 0);
	msg->getUUIDFast(_PREHASH_FailureInfo, _PREHASH_AgentID, agentID);
	msg->getUUIDFast(_PREHASH_FailureInfo, _PREHASH_TransactionID, transactionID);
	
	LL_WARNS("Messaging") << "Blacklisted Feature Response:" << messageText << LL_ENDL;
}

// ------------------------------------------------------------
// Message system exception callbacks
// ------------------------------------------------------------

void invalid_message_callback(LLMessageSystem* msg,
								   void*,
								   EMessageException exception)
{
    LLAppViewer::instance()->badNetworkHandler();
}

// Please do not add more message handlers here. This file is huge.
// Put them in a file related to the functionality you are implementing.

void LLOfferInfo::forceResponse(InventoryOfferResponse response)
{
	LLNotification::Params params("UserGiveItem");
	params.functor(boost::bind(&LLOfferInfo::inventory_offer_callback, this, _1, _2));
	LLNotifications::instance().forceResponse(params, response);
}

