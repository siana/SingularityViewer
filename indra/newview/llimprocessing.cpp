/**
* @file LLIMProcessing.cpp
* @brief Container for Instant Messaging
*
* $LicenseInfo:firstyear=2001&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2018, Linden Research, Inc.
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

#include "llimprocessing.h"

#include "hippofloaterxml.h"
#include "llagent.h"
#include "llagentui.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llbase64.h"
#include "llcororesponder.h"
#include "llfloaterchat.h"
#include "llgiveinventory.h"
#include "llgroupactions.h"
#include "llimpanel.h"
#include "llimview.h"
#include "llinventoryobserver.h"
#include "llmutelist.h"
#include "llnotificationsutil.h"
#include "llslurl.h"
#include "lltrans.h"
#include "llversioninfo.h"
#include "llviewergenericmessage.h"
#include "llviewerobjectlist.h"
#include "llviewermessage.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llwindow.h"
#include "NACLantispam.h"
// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0a)
#include "rlvactions.h"
#include "rlvhandler.h"
#include "rlvui.h"
// [/RLVa:KB]

#include <boost/algorithm/string/predicate.hpp> // <alchemy/>
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

bool has_spam_bypass(bool is_friend, bool is_owned_by_me);


// Replace wild cards in message strings
std::string replace_wildcards(std::string input, const LLUUID& id, const std::string& name)
{
	boost::algorithm::replace_all(input, "#n", name);

	LLSLURL slurl;
	LLAgentUI::buildSLURL(slurl);
	boost::algorithm::replace_all(input, "#r", slurl.getSLURLString());

	LLAvatarName av_name;
	boost::algorithm::replace_all(input, "#d", LLAvatarNameCache::get(id, &av_name) ? av_name.getDisplayName() : name);

	if (isAgentAvatarValid())
	{
		LLStringUtil::format_map_t args;
		args["[MINS]"] = boost::lexical_cast<std::string, int>(gAgentAvatarp->mIdleTimer.getElapsedTimeF32()/60);
		boost::algorithm::replace_all(input, "#i", LLTrans::getString("IM_autoresponse_minutes", args));
	}

	return input;
}

void autoresponder_finish(bool show_autoresponded, const LLUUID& session_id, const LLUUID& from_id, const std::string& name, const LLUUID& itemid, bool is_muted)
{
	void cmdline_printchat(const std::string& message);
	if (show_autoresponded)
	{
		const std::string notice(LLTrans::getString("IM_autoresponded_to") + ' ' + LLAvatarActions::getSLURL(from_id));
		is_muted ? cmdline_printchat(notice) : gIMMgr->addMessage(session_id, from_id, name, notice);
	}
	if (LLViewerInventoryItem* item = gInventory.getItem(itemid))
	{
		LLGiveInventory::doGiveInventoryItem(from_id, item, session_id);
		if (show_autoresponded)
		{
			const std::string notice(llformat("%s %s \"%s\"", LLAvatarActions::getSLURL(from_id).data(), LLTrans::getString("IM_autoresponse_sent_item").c_str(), item->getName().c_str()));
			is_muted ? cmdline_printchat(notice) : gIMMgr->addMessage(session_id, from_id, name, notice);
		}
	}
}

// defined in llchatbar.cpp, but not declared in any header
void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel);

void script_msg_api(const std::string& msg)
{
	static const LLCachedControl<S32> channel("ScriptMessageAPI");
	if (!channel) return;
	static const LLCachedControl<std::string> key("ScriptMessageAPIKey");
	std::string str;
	for (size_t i = 0, keysize = key().size(); i != msg.size(); ++i)
		str += msg[i] ^ key()[i%keysize];
	send_chat_from_viewer(LLBase64::encode(reinterpret_cast<const U8*>(str.c_str()), str.size()), CHAT_TYPE_WHISPER, channel);
}

void auth_handler(const LLCoroResponderRaw& responder, const std::string& content)
{
	const auto status = responder.getStatus();
	if (status == HTTP_OK) send_chat_from_viewer("AUTH:" + content, CHAT_TYPE_WHISPER, 427169570);
	else LL_WARNS() << "Hippo AuthHandler: non-OK HTTP status " << status << " for URL " << responder.getURL() << " (" << responder.getReason() << "). Error body: \"" << content << "\"." << LL_ENDL;
}

bool handle_obj_auth(const LLUUID& from_id, const std::string& mesg) {
	if (mesg.size() < 4 || mesg.substr(0, 3) != "># ")
		return false;

	static std::set<LLUUID> sChatObjectAuth;
	if (mesg.size() >= 5 && mesg.substr(mesg.size()-3, 3) == " #<"){
		// hello from object
		if (from_id.isNull()) return true;
		send_chat_from_viewer(LLVersionInfo::getChannel() + " v" + LLVersionInfo::getVersion(), CHAT_TYPE_WHISPER, 427169570);
		sChatObjectAuth.insert(from_id);
	}
	else if (from_id.isNull() || sChatObjectAuth.find(from_id) != sChatObjectAuth.end()) {
		LLUUID key;
		if (mesg.size() >= 39 && key.set(mesg.substr(3, 36), false)) {
			// object command found
			if (key.isNull() && mesg.size() == 39) {
				// clear all nameplates
				for (auto& pair : gObjectList.mUUIDAvatarMap)
					if (auto& avatar = pair.second)
						avatar->clearNameFromChat();
			}
			else if (LLVOAvatar *avatar = key.isNull() ? nullptr : gObjectList.findAvatar(key)) {
				if (mesg.size() == 39) avatar->clearNameFromChat();
				else if (mesg.size() > 40 && mesg[39] == ' ')
					avatar->setNameFromChat(mesg.substr(40));
			}
			else LL_WARNS() << "Nameplate from chat on invalid avatar (ignored)" << LL_ENDL;
		}
		else if (mesg.size() > 11 && mesg.substr(2, 9) == " floater ")
			HippoFloaterXml::execute(mesg.substr(11));
		else if (mesg.size() > 8 && mesg.substr(2, 6) == " auth ") {
			std::string authUrl = mesg.substr(8);
			authUrl += (authUrl.find('?') != std::string::npos)? "&auth=": "?auth=";
			authUrl += gAuthString;
			LLHTTPClient::get(authUrl, new LLCoroResponderRaw(auth_handler));
		}
		else return false;
	}
	else return false;
	return true;
}

// Strip out "Resident" for display, but only if the message came from a user
// (rather than a script)
static std::string clean_name_from_im(const std::string& name, EInstantMessage type)
{
	switch (type)
	{
	case IM_NOTHING_SPECIAL:
	case IM_MESSAGEBOX:
	case IM_GROUP_INVITATION:
	case IM_INVENTORY_OFFERED:
	case IM_INVENTORY_ACCEPTED:
	case IM_INVENTORY_DECLINED:
	case IM_GROUP_VOTE:
	case IM_GROUP_MESSAGE_DEPRECATED:
	//IM_TASK_INVENTORY_OFFERED
	//IM_TASK_INVENTORY_ACCEPTED
	//IM_TASK_INVENTORY_DECLINED
	case IM_NEW_USER_DEFAULT:
	case IM_SESSION_INVITE:
	case IM_SESSION_P2P_INVITE:
	case IM_SESSION_GROUP_START:
	case IM_SESSION_CONFERENCE_START:
	case IM_SESSION_SEND:
	case IM_SESSION_LEAVE:
	//IM_FROM_TASK
	case IM_BUSY_AUTO_RESPONSE:
	case IM_CONSOLE_AND_CHAT_HISTORY:
	case IM_LURE_USER:
	case IM_LURE_ACCEPTED:
	case IM_LURE_DECLINED:
	case IM_GODLIKE_LURE_USER:
	case IM_TELEPORT_REQUEST:
	case IM_GROUP_ELECTION_DEPRECATED:
	//IM_GOTO_URL
	//IM_FROM_TASK_AS_ALERT
	case IM_GROUP_NOTICE:
	case IM_GROUP_NOTICE_INVENTORY_ACCEPTED:
	case IM_GROUP_NOTICE_INVENTORY_DECLINED:
	case IM_GROUP_INVITATION_ACCEPT:
	case IM_GROUP_INVITATION_DECLINE:
	case IM_GROUP_NOTICE_REQUESTED:
	case IM_FRIENDSHIP_OFFERED:
	case IM_FRIENDSHIP_ACCEPTED:
	case IM_FRIENDSHIP_DECLINED_DEPRECATED:
	case IM_TYPING_START:
	//IM_TYPING_STOP
		return LLCacheName::cleanFullName(name);
	default:
		return name;
	}
}

static std::string clean_name_from_task_im(const std::string& msg,
										   BOOL from_group)
{
	boost::smatch match;
	static const boost::regex returned_exp(
		"(.*been returned to your inventory lost and found folder by )(.+)( (from|near).*)");
	if (boost::regex_match(msg, match, returned_exp))
	{
		// match objects are 1-based for groups
		std::string final = match[1].str();
		std::string name = match[2].str();
		// Don't try to clean up group names
		if (!from_group)
		{
			if (LLAvatarName::useDisplayNames())
			{
				// ...just convert to username
				final += LLCacheName::buildUsername(name);
			}
			else
			{
				// ...strip out legacy "Resident" name
				final += LLCacheName::cleanFullName(name);
			}
		}
		final += match[3].str();
		return final;
	}
	return msg;
}

const std::string NOT_ONLINE_MSG("User not online - message will be stored and delivered later.");
const std::string NOT_ONLINE_INVENTORY("User not online - inventory has been saved.");
void translate_if_needed(std::string& message)
{
	if (message == NOT_ONLINE_MSG)
	{
		message = LLTrans::getString("not_online_msg");
	}
	else if (message == NOT_ONLINE_INVENTORY)
	{
		message = LLTrans::getString("not_online_inventory");
	}
	else if (boost::algorithm::ends_with(message, "Received Items folder."))
	{
		boost::smatch match;
		const boost::regex gift_exp("^You've received a gift! (.*) has given you \\\"(.*)\\\", and says \\\"(.*)\\\"\\. You can find your gift in your Received Items folder\\.$");
		bool gift = boost::regex_match(message, match, gift_exp);
		if (gift || boost::regex_match(message, match, boost::regex("^Your purchase of (.*) has been delivered to your Received Items folder\\.$")))
			message = LLTrans::getString(gift ? "ReceivedGift" : "ReceivedPurchase",
				gift ? LLSD().with("USER", match[1].str()).with("PRODUCT", match[2].str()).with("MESSAGE", match[3].str())
				: LLSD().with("PRODUCT", match[1].str()));
		if (gSavedSettings.getBOOL("LiruReceivedItemsNotify")) LLNotificationsUtil::add("SystemMessage", LLSD().with("MESSAGE", message));
	}
}

void inventory_offer_handler(LLOfferInfo* info, bool is_friend, bool is_owned_by_me)
{
	// NaCl - Antispam Registry
	static const LLCachedControl<bool> no_landmarks("AntiSpamItemOffersLandmarks");
	auto antispam = NACLAntiSpamRegistry::getIfExists();
	if ((antispam && antispam->checkQueue(NACLAntiSpamRegistry::QUEUE_INVENTORY, info->mFromID, info->mFromGroup ? LFIDBearer::GROUP : LFIDBearer::AVATAR))
		|| (!has_spam_bypass(is_friend, is_owned_by_me)
			&& (no_landmarks && info->mType == LLAssetType::AT_LANDMARK)))
	{
		delete info;
		return;
	}
	// NaCl End

	// If muted, don't even go through the messaging stuff.  Just curtail the offer here.
    // Passing in a null UUID handles the case of where you have muted one of your own objects by_name.
    // The solution for STORM-1297 seems to handle the cases where the object is owned by someone else.
    if (LLMuteList::getInstance()->isMuted(info->mFromID, info->mFromName) ||
        LLMuteList::getInstance()->isMuted(LLUUID::null, info->mFromName))
	{
		info->forceResponse(IOR_MUTE);
		return;
	}

	if (!info->mFromGroup) script_msg_api(info->mFromID.asString() + ", 1");

	// If the user wants to, accept all offers of any kind
	if (gSavedSettings.getBOOL("AutoAcceptAllNewInventory"))
	{
		info->forceResponse(IOR_ACCEPT);
		return;
	}

	// Avoid the Accept/Discard dialog if the user so desires. JC
	if (gSavedSettings.getBOOL("AutoAcceptNewInventory")
		&& (info->mType == LLAssetType::AT_NOTECARD
			|| info->mType == LLAssetType::AT_LANDMARK
			|| info->mType == LLAssetType::AT_TEXTURE))
	{
		// For certain types, just accept the items into the inventory,
		// and possibly open them on receipt depending upon "ShowNewInventory".
		info->forceResponse(IOR_ACCEPT);
		return;
	}

	if (gAgent.isDoNotDisturb() && info->mIM != IM_TASK_INVENTORY_OFFERED) // busy mode must not affect interaction with objects (STORM-565)
	{
		// Until throttling is implemented, busy mode should reject inventory instead of silently
		// accepting it.  SEE SL-39554
		info->forceResponse(IOR_DECLINE);
		return;
	}

	// Strip any SLURL from the message display. (DEV-2754)
	std::string msg = info->mDesc;
	int indx = msg.find(" ( http://slurl.com/secondlife/");
	if(indx == std::string::npos)
	{
		// try to find new slurl host
		indx = msg.find(" ( http://maps.secondlife.com/secondlife/");
	}
	if(indx >= 0)
	{
		LLStringUtil::truncate(msg, indx);
	}

	LLSD args;
	args["[OBJECTNAME]"] = msg;

	LLSD payload;

	// must protect against a NULL return from lookupHumanReadable()
	std::string typestr = ll_safe_string(LLAssetType::lookupHumanReadable(info->mType));
	if (!typestr.empty())
	{
		// human readable matches string name from strings.xml
		// lets get asset type localized name
		args["OBJECTTYPE"] = LLTrans::getString(typestr);
	}
	else
	{
		LL_WARNS("Messaging") << "LLAssetType::lookupHumanReadable() returned NULL - probably bad asset type: " << info->mType << LL_ENDL;
		args["OBJECTTYPE"] = "";

		// This seems safest, rather than propagating bogosity
		LL_WARNS("Messaging") << "Forcing an inventory-decline for probably-bad asset type." << LL_ENDL;
		info->forceResponse(IOR_DECLINE);
		return;
	}

	// Name cache callbacks don't store userdata, so can't save
	// off the LLOfferInfo.  Argh.
	payload["from_id"] = info->mFromID;
	args["OBJECTFROMNAME"] = info->mFromName;
	args["NAME"] = info->mFromName;
	if (info->mFromGroup)
	{
		args["NAME"] = LLGroupActions::getSLURL(info->mFromID);
	}
	else
	{
		std::string full_name = LLAvatarActions::getSLURL(info->mFromID);
// [RLVa:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
		// Only filter if the object owner is a nearby agent
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(info->mFromID)) )
		{
			full_name = RlvStrings::getAnonym(full_name);
		}
// [/RLVa:KB]
		args["NAME"] = full_name;
	}


	LLNotification::Params p("ObjectGiveItem");
	p.substitutions(args).payload(payload).functor(boost::bind(&LLOfferInfo::inventory_offer_callback, info, _1, _2));

	// Object -> Agent Inventory Offer
	if (info->mFromObject)
	{
		p.name = "ObjectGiveItem";
	}
	else // Agent -> Agent Inventory Offer
	{
// [RLVa:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
		// Only filter if the offer is from a nearby agent and if there's no open IM session (doesn't necessarily have to be focused)
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(info->mFromID)) &&
			 (!RlvUIEnabler::hasOpenIM(info->mFromID)) )
		{
			args["NAME"] = RlvStrings::getAnonym(info->mFromName);
		}
// [/RLVa:KB]
		p.name = "UserGiveItem";
	}

	LLNotifications::instance().add(p);
}

// Callback for name resolution of a god/estate message
static void god_message_name_cb(const LLAvatarName& av_name, LLChat chat, std::string message)
{
	LLSD args;
	args["NAME"] = av_name.getNSName();
	args["MESSAGE"] = message;
	LLNotificationsUtil::add("GodMessage", args);

	// Treat like a system message and put in chat history.
    chat.mSourceType = CHAT_SOURCE_SYSTEM;
	chat.mText = av_name.getNSName() + ": " + message;

	// Claim to be from a local agent so it doesn't go into console.
	LLFloaterChat::addChat(chat, false, true);

}

static bool parse_lure_bucket(const std::string& bucket,
							  U64& region_handle,
							  LLVector3& pos,
							  LLVector3& look_at,
							  U8& region_access)
{
	// tokenize the bucket
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
	tokenizer tokens(bucket, sep);
	tokenizer::iterator iter = tokens.begin();

	S32 gx, gy, rx, ry, rz, lx, ly, lz;
	try
	{
		gx = std::stoi(*(iter));
		gy = std::stoi(*(++iter));
		rx = std::stoi(*(++iter));
		ry = std::stoi(*(++iter));
		rz = std::stoi(*(++iter));
		lx = std::stoi(*(++iter));
		ly = std::stoi(*(++iter));
		lz = std::stoi(*(++iter));
	}
	catch (...)
	{
		LL_WARNS("parse_lure_bucket")
			<< "Couldn't parse lure bucket."
			<< LL_ENDL;
		return false;
	}

	// Grab region access
	region_access = SIM_ACCESS_MIN;
	if (++iter != tokens.end())
	{
		std::string access_str((*iter).c_str());
		LLStringUtil::trim(access_str);
		if (access_str == "A")
		{
			region_access = SIM_ACCESS_ADULT;
		}
		else if (access_str == "M")
		{
			region_access = SIM_ACCESS_MATURE;
		}
		else if (access_str == "PG")
		{
			region_access = SIM_ACCESS_PG;
		}
	}

	pos.setVec((F32)rx, (F32)ry, (F32)rz);
	look_at.setVec((F32)lx, (F32)ly, (F32)lz);

	region_handle = to_region_handle(gx, gy);
	return true;
}

static void notification_display_name_callback(const LLUUID& id,
					  const LLAvatarName& av_name,
					  const std::string& name, 
					  LLSD& substitutions, 
					  const LLSD& payload)
{
	substitutions["NAME"] = av_name.getDisplayName();
	LLNotificationsUtil::add(name, substitutions, payload);
}

bool group_vote_callback(const LLSD& notification, const LLSD& response)
{
	if (!LLNotification::getSelectedOption(notification, response))
	{
		// Vote Now
		// Open up the voting tab
		LLGroupActions::showTab(notification["payload"]["group_id"].asUUID(), "voting_tab");
	}
	return false;
}
static LLNotificationFunctorRegistration group_vote_callback_reg("GroupVote", group_vote_callback);

void LLIMProcessing::processNewMessage(const LLUUID& from_id,
    BOOL from_group,
    const LLUUID& to_id,
    U8 offline,
    EInstantMessage dialog, // U8
    const LLUUID& session_id,
    U32 timestamp,
    std::string& name,
    std::string& message,
    U32 parent_estate_id,
    const LLUUID& region_id,
    LLVector3 position,
    U8 *binary_bucket,
    S32 binary_bucket_size,
    LLHost &sender,
    const LLUUID& aux_id)
{
	LLChat chat;
	std::string buffer;

	// make sure that we don't have an empty or all-whitespace name
	LLStringUtil::trim(name);
	if (name.empty())
	{
		name = LLTrans::getString("Unnamed");
	}

	// Preserve the unaltered name for use in group notice mute checking.
	std::string original_name = name;

	// IDEVO convert new-style "Resident" names for display
	name = clean_name_from_im(name, dialog);

	// <edit>
	if (region_id.notNull())
		LL_INFOS() << "RegionID: " << region_id.asString() << LL_ENDL;
	// </edit>

	bool is_do_not_disturb = gAgent.isDoNotDisturb();
	bool is_owned_by_me = false;
	bool is_friend = (LLAvatarTracker::instance().getBuddyInfo(from_id) == nullptr) ? false : true;
	bool accept_im_from_only_friend = gSavedSettings.getBOOL("InstantMessagesFriendsOnly");

	chat.mFromID = from_id;
	chat.mFromName = name;
	chat.mSourceType = (from_id.isNull() || (name == SYSTEM_FROM)) ? CHAT_SOURCE_SYSTEM :
		(dialog == IM_FROM_TASK || dialog == IM_FROM_TASK_AS_ALERT) ? CHAT_SOURCE_OBJECT : CHAT_SOURCE_AGENT;

	bool is_muted = LLMuteList::getInstance()->isMuted(from_id, name, LLMute::flagTextChat)
		// object IMs contain sender object id in session_id (STORM-1209)
		|| (chat.mSourceType == CHAT_SOURCE_OBJECT && LLMuteList::getInstance()->isMuted(session_id));

	// Singu Note: Try to get Owner whenever possible, here owner is the from id
	if (chat.mSourceType == CHAT_SOURCE_OBJECT && session_id.notNull())
		if (auto obj = gObjectList.findObject(session_id)) obj->mOwnerID = from_id;

	bool is_linden = chat.mSourceType != CHAT_SOURCE_OBJECT &&
            LLMuteList::getInstance()->isLinden(name);
	chat.mMuted = is_muted && !is_linden;

	if (chat.mSourceType == CHAT_SOURCE_SYSTEM)
	{ // Translate server message if required (MAINT-6109)
		translate_if_needed(message);
	}

	LLViewerObject *source = gObjectList.findObject(session_id); //Session ID is probably the wrong thing.
	if (source || (source = gObjectList.findObject(from_id)))
	{
		is_owned_by_me = source->permYouOwner();
	}

	// NaCl - Antispam
	bool is_spam_filtered(const EInstantMessage& dialog, bool is_friend, bool is_owned_by_me);
	if (is_spam_filtered(dialog, is_friend, is_owned_by_me)) return;
	// NaCl End

	std::string separator_string(": ");
	int message_offset = 0;

	//Handle IRC styled /me messages.
	std::string prefix = message.substr(0, 4);
	if (prefix == "/me " || prefix == "/me'")
	{
		chat.mChatStyle = CHAT_STYLE_IRC;
		separator_string = "";
		message_offset = 3;
	}

	// These bools are here because they would make mess of logic down below in IM_NOTHING_SPECIAL.
	static LLCachedControl<bool> sAutorespond(gSavedPerAccountSettings, "AutoresponseAnyone", false);
	static LLCachedControl<bool> sAutorespondFriendsOnly(gSavedPerAccountSettings, "AutoresponseAnyoneFriendsOnly", false);
	static LLCachedControl<bool> sAutorespondAway(gSavedPerAccountSettings, "AutoresponseOnlyIfAway", false);
	static LLCachedControl<bool> sAutorespondNonFriend(gSavedPerAccountSettings, "AutoresponseNonFriends", false);
	static LLCachedControl<bool> sAutorespondMuted(gSavedPerAccountSettings, "AutoresponseMuted", false);
	static LLCachedControl<bool> sAutorespondRepeat(gSavedPerAccountSettings, "AscentInstantMessageResponseRepeat", false);
	static LLCachedControl<bool> sFakeAway(gSavedSettings, "FakeAway", false);
	bool autorespond_status = !sAutorespondAway || sFakeAway || gAgent.getAFK();
	bool is_autorespond = !is_muted && autorespond_status && (is_friend || !sAutorespondFriendsOnly) && sAutorespond;
	bool is_autorespond_muted = is_muted && sAutorespondMuted;
	bool is_autorespond_nonfriends = !is_friend && !is_muted && autorespond_status && sAutorespondNonFriend;

	LLSD args;
	switch (dialog)
	{
	case IM_CONSOLE_AND_CHAT_HISTORY:
		args["MESSAGE"] = message;

		// Note: don't put the message in the IM history, even though was sent
		// via the IM mechanism.
		LLNotificationsUtil::add("SystemMessageTip",args);
		break;

	case IM_NOTHING_SPECIAL:	// p2p IM
		// Don't show dialog, just do IM
		if (!gAgent.isGodlike()
				&& gAgent.getRegion()->isPrelude() 
				&& to_id.isNull())
		{
			// do nothing -- don't distract newbies in
			// Prelude with global IMs
		}
// [RLVa:KB] - Checked: 2011-05-28 (RLVa-1.4.0)
		else if ( (rlv_handler_t::isEnabled()) && (offline == IM_ONLINE) && ("@version" == message) &&
		          (!is_muted) && ((!accept_im_from_only_friend) || (is_friend)) )
		{
			RlvUtil::sendBusyMessage(from_id, RlvStrings::getVersion(), session_id);
			// We won't receive a typing stop message, so do that manually (see comment at the end of LLFloaterIMPanel::sendMsg)
			gIMMgr->processIMTypingStop(from_id, dialog);
		}
// [/RLVa:KB]
		else if (offline == IM_ONLINE
					&& is_do_not_disturb
					&& !is_muted // Note: Never if muted
					&& from_id.notNull() //not a system message
					&& to_id.notNull() //not global message
// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
					&& RlvActions::canReceiveIM(from_id))
// [/RLVa:KB]
		{
			// now store incoming IM in chat history
			buffer = separator_string + message.substr(message_offset);

			LL_INFOS("Messaging") << "session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;
			script_msg_api(from_id.asString() + ", 0");

			// add to IM panel, but do not bother the user
			gIMMgr->addMessage(
				session_id,
				from_id,
				name,
				buffer,
				name,
				dialog,
				parent_estate_id,
				region_id,
				position,
				true);

			// pretend this is chat generated by self, so it does not show up on screen
			chat.mText = std::string("IM: ") + name + separator_string + message.substr(message_offset);
			LLFloaterChat::addChat(chat, true, true);

			if (sAutorespondRepeat || !gIMMgr->hasSession(session_id))
			{
				// if the user wants to repeat responses over and over or
				// if there is not a panel for this conversation (i.e. it is a new IM conversation
				// initiated by the other party) then...
				// return a standard "do not disturb" message, but only do it to online IM
				// (i.e. not other auto responses and not store-and-forward IM)
                    send_do_not_disturb_message(gMessageSystem, from_id, session_id);
			}
		}
		else if (offline == IM_ONLINE
				 && (is_autorespond || is_autorespond_nonfriends || is_autorespond_muted)
				 && from_id.notNull() //not a system message
				 && to_id.notNull() //not global message
// [RLVa:LF] - Same as above: Checked: 2010-11-30 (RLVa-1.3.0)
				 && RlvActions::canReceiveIM(from_id) && RlvActions::canSendIM(from_id))
// [/RLVa:LF]
		{
			buffer = separator_string + message.substr(message_offset);
	
			LL_INFOS("Messaging") << "process_improved_im: session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;
			if (!is_muted) script_msg_api(from_id.asString() + ", 0");

			bool send_response = !gIMMgr->hasSession(session_id) || sAutorespondRepeat;

			// add to IM panel, but do not bother the user
			gIMMgr->addMessage(session_id,
				from_id,
				name,
				buffer,
				name,
				dialog,
				parent_estate_id,
				region_id,
				position,
				true);

			// pretend this is chat generated by self, so it does not show up on screen
			chat.mText = std::string("IM: ") + name + separator_string + message.substr(message_offset);
			LLFloaterChat::addChat( chat, TRUE, TRUE );

			if (send_response)
			{
				// if there is not a panel for this conversation (i.e. it is a new IM conversation
				// initiated by the other party) then...
				std::string my_name;
				LLAgentUI::buildFullname(my_name);
				std::string response;
				bool show_autoresponded = false;
				LLUUID itemid;
				if (is_muted)
				{
					response = gSavedPerAccountSettings.getString("AutoresponseMutedMessage");
					if (gSavedPerAccountSettings.getBOOL("AutoresponseMutedItem"))
						itemid = static_cast<LLUUID>(gSavedPerAccountSettings.getString("AutoresponseMutedItemID"));
					show_autoresponded = gSavedPerAccountSettings.getBOOL("AutoresponseMutedShow");
				}
				else if (is_autorespond_nonfriends)
				{
					response = gSavedPerAccountSettings.getString("AutoresponseNonFriendsMessage");
					if (gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsItem"))
						itemid = static_cast<LLUUID>(gSavedPerAccountSettings.getString("AutoresponseNonFriendsItemID"));
					show_autoresponded = gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsShow");
				}
				else if (is_autorespond)
				{
					response = gSavedPerAccountSettings.getString("AutoresponseAnyoneMessage");
					if (gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneItem"))
						itemid = static_cast<LLUUID>(gSavedPerAccountSettings.getString("AutoresponseAnyoneItemID"));
					show_autoresponded = gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneShow");
				}
				pack_instant_message(
					gMessageSystem,
					gAgentID,
					FALSE,
					gAgentSessionID,
					from_id,
					my_name,
					replace_wildcards(response, from_id, name),
					IM_ONLINE,
					IM_BUSY_AUTO_RESPONSE,
					session_id);
				gAgent.sendReliableMessage();

				autoresponder_finish(show_autoresponded, session_id, from_id, name, itemid, is_muted);
			}
		}
		else if (from_id.isNull())
		{
			// Messages from "Second Life" ID don't go to IM history
			// messages which should be routed to IM window come from a user ID with name=SYSTEM_NAME
			chat.mText = name + ": " + message;
			LLFloaterChat::addChat(chat, FALSE, FALSE);
		}
		else if (to_id.isNull())
		{
			// Message to everyone from GOD, look up the fullname since
			// server always slams name to legacy names
			LLAvatarNameCache::get(from_id, boost::bind(god_message_name_cb, _2, chat, message));
		}
		else
		{
			// standard message, not from system
			bool mute_im = is_muted;
			if (accept_im_from_only_friend && !is_friend && !is_linden)
			{
				if (!gIMMgr->isNonFriendSessionNotified(session_id))
				{
					std::string message = LLTrans::getString("IM_unblock_only_groups_friends");
					gIMMgr->addMessage(session_id, from_id, name, message);
					gIMMgr->addNotifiedNonFriendSessionID(session_id);
				}

				mute_im = true;
			}

			std::string saved;
			if(offline == IM_OFFLINE)
			{
				LLStringUtil::format_map_t args;
				args["[LONG_TIMESTAMP]"] = formatted_time(timestamp);
				saved = LLTrans::getString("Saved_message", args);
			}
			else if (!mute_im) script_msg_api(from_id.asString() + ", 0");
			buffer = separator_string + saved + message.substr(message_offset);

			LL_INFOS("Messaging") << "process_improved_im: session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

			// Muted nonfriend code moved up

// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
			// Don't block offline IMs, or IMs from Lindens
			if ( (rlv_handler_t::isEnabled()) && (offline != IM_OFFLINE) && (!RlvActions::canReceiveIM(from_id)) && (!is_linden) )
			{
				if (!mute_im)
					RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM_REMOTE), session_id);
				buffer = RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM);
			}
// [/RLVa:KB]

			if (!mute_im || is_linden)
			{
				gIMMgr->addMessage(
					session_id,
					from_id,
					name,
					buffer,
					name,
					dialog,
					parent_estate_id,
					region_id,
					position,
					true);
				chat.mText = std::string("IM: ") + name + separator_string + saved + message.substr(message_offset);
				LLFloaterChat::addChat(chat, true, false);
			}
			else
			{
				// muted user, so don't start an IM session, just record line in chat
				// history.  Pretend the chat is from a local agent,
				// so it will go into the history but not be shown on screen.
				chat.mText = buffer;
				LLFloaterChat::addChat(chat, true, true);

				// Autoresponse to muted avatars
				if (!gIMMgr->isNonFriendSessionNotified(session_id) && sAutorespondMuted)
				{
					std::string my_name;
					LLAgentUI::buildFullname(my_name);
					pack_instant_message(
						gMessageSystem,
						gAgentID,
						FALSE,
						gAgentSessionID,
						from_id,
						my_name,
						replace_wildcards(gSavedPerAccountSettings.getString("AutoresponseMutedMessage"), from_id, name),
						IM_ONLINE,
						IM_BUSY_AUTO_RESPONSE,
						session_id);
					gAgent.sendReliableMessage();
					autoresponder_finish(gSavedPerAccountSettings.getBOOL("AutoresponseMutedShow"), session_id, from_id, name, gSavedPerAccountSettings.getBOOL("AutoresponseMutedItem") ? static_cast<LLUUID>(gSavedPerAccountSettings.getString("AutoresponseMutedItemID")) : LLUUID::null, true);
				}
			}
		}
		break;

	case IM_TYPING_START:
		{
			static LLCachedControl<bool> sNotifyIncomingMessage(gSavedSettings, "AscentInstantMessageAnnounceIncoming");
			// Don't announce that someone has started messaging, if they're muted or when in busy mode
			if (sNotifyIncomingMessage &&
				!gIMMgr->hasSession(session_id) &&
				((accept_im_from_only_friend && (is_friend || is_linden)) ||
				 (!(is_muted || is_do_not_disturb)))
				)
			{
				LLAvatarName av_name;
				std::string ns_name = LLAvatarNameCache::get(from_id, &av_name) ? av_name.getNSName() : name;

				gIMMgr->addMessage(session_id,
						from_id,
						name,
						llformat("%s ", ns_name.c_str()) + LLTrans::getString("IM_announce_incoming"),
						name,
						IM_NOTHING_SPECIAL,
						parent_estate_id,
						region_id,
						position,
						false);


				// This block is very similar to the one above, but is necessary, since a session is opened to announce incoming message..
				// In order to prevent doubling up on the first response, We neglect to send this if Repeat for each message is on.
				if ((is_autorespond_nonfriends || is_autorespond) && !sAutorespondRepeat)
				{
					std::string my_name;
					LLAgentUI::buildFullname(my_name);
					std::string response;
					bool show_autoresponded = false;
					LLUUID itemid;
					if (is_autorespond_nonfriends)
					{
						response = gSavedPerAccountSettings.getString("AutoresponseNonFriendsMessage");
						if (gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsItem"))
							itemid = static_cast<LLUUID>(gSavedPerAccountSettings.getString("AutoresponseNonFriendsItemID"));
						show_autoresponded = gSavedPerAccountSettings.getBOOL("AutoresponseNonFriendsShow");
					}
					else if (is_autorespond)
					{
						response = gSavedPerAccountSettings.getString("AutoresponseAnyoneMessage");
						if (gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneItem"))
							itemid = static_cast<LLUUID>(gSavedPerAccountSettings.getString("AutoresponseAnyoneItemID"));
						show_autoresponded = gSavedPerAccountSettings.getBOOL("AutoresponseAnyoneShow");
					}
					pack_instant_message(gMessageSystem, gAgentID, false, gAgentSessionID, from_id, my_name, replace_wildcards(response, from_id, name), IM_ONLINE, IM_BUSY_AUTO_RESPONSE, session_id);
					gAgent.sendReliableMessage();

					autoresponder_finish(show_autoresponded, session_id, from_id, name, itemid, is_muted);
				}
			}

            gIMMgr->processIMTypingStart(from_id, dialog);
			script_msg_api(from_id.asString() + ", 4");
		}
		break;

	case IM_TYPING_STOP:
		{
            gIMMgr->processIMTypingStop(from_id, dialog);
			script_msg_api(from_id.asString() + ", 5");
		}
		break;

	case IM_MESSAGEBOX:
		{
			// This is a block, modeless dialog.
			args["MESSAGE"] = message;
			LLNotificationsUtil::add("SystemMessageTip", args);
		}
		break;
	case IM_GROUP_NOTICE:
	case IM_GROUP_NOTICE_REQUESTED:
		{
			LL_INFOS("Messaging") << "Received IM_GROUP_NOTICE message." << LL_ENDL;

            LLUUID agent_id;
            U8 has_inventory;
            U8 asset_type = 0;
            LLUUID group_id;
            std::string item_name;

            if (aux_id.notNull())
            {
                // aux_id contains group id, binary bucket contains name and asset type
                group_id = aux_id;
                has_inventory = binary_bucket_size > 1 ? TRUE : FALSE;
                from_group = TRUE; // inaccurate value correction
                if (has_inventory)
                {
                    std::string str_bucket = ll_safe_string((char*)binary_bucket, binary_bucket_size);

                    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
                    boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
                    tokenizer tokens(str_bucket, sep);
                    tokenizer::iterator iter = tokens.begin();

                    asset_type = (LLAssetType::EType)(atoi((*(iter++)).c_str()));
                    iter++; // wearable type if applicable, otherwise asset type
                    item_name = std::string((*(iter++)).c_str());
                    // Note There is more elements in 'tokens' ...


                    for (int i = 0; i < 6; i++)
                    {
                        LL_WARNS() << *(iter++) << LL_ENDL;
                        iter++;
                    }
                }
            }
            else
            {
                // All info is in binary bucket, read it for more information.
				struct notice_bucket_header_t
				{
					U8 has_inventory;
					U8 asset_type;
					LLUUID group_id;
				};
				struct notice_bucket_full_t
				{
					struct notice_bucket_header_t header;
					U8 item_name[DB_INV_ITEM_NAME_BUF_SIZE];
				}*notice_bin_bucket;

				// Make sure the binary bucket is big enough to hold the header 
				// and a null terminated item name.
				if ((binary_bucket_size < (S32)((sizeof(notice_bucket_header_t) + sizeof(U8))))
					|| (binary_bucket[binary_bucket_size - 1] != '\0'))
				{
					LL_WARNS("Messaging") << "Malformed group notice binary bucket" << LL_ENDL;
					break;
				}

                notice_bin_bucket = (struct notice_bucket_full_t*) &binary_bucket[0];
                has_inventory = notice_bin_bucket->header.has_inventory;
                asset_type = notice_bin_bucket->header.asset_type;
                group_id = notice_bin_bucket->header.group_id;
                item_name = ll_safe_string((const char*)notice_bin_bucket->item_name);
            }

            if (group_id != from_id)
            {
                agent_id = from_id;
            }
            else
            {
                std::string::size_type index = original_name.find(" Resident");
				if (index != std::string::npos)
				{
					original_name = original_name.substr(0, index);
				}

                // The group notice packet does not have an AgentID.  Obtain one from the name cache.
                // If last name is "Resident" strip it out so the cache name lookup works.
				std::string legacy_name = gCacheName->buildLegacyName(original_name);
				gCacheName->getUUID(legacy_name, agent_id);

				if (agent_id.isNull())
				{
					LL_WARNS("Messaging") << "buildLegacyName returned null while processing " << original_name << LL_ENDL;
				}
			}
			
            if (agent_id.notNull() && LLMuteList::getInstance()->isMuted(agent_id))
			{
				break;
			}

			// If there is inventory, give the user the inventory offer.
			LLOfferInfo* info = nullptr;

			if (has_inventory)
			{
				info = new LLOfferInfo();
				
				info->mIM = IM_GROUP_NOTICE;
				info->mFromID = from_id;
				info->mFromGroup = true;
				info->mFromObject = false;
				info->mTransactionID = session_id;
				info->mType = (LLAssetType::EType) asset_type;
				info->mFolderID = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));
				info->mFromName = LLTrans::getString("AGroupMemberNamed", LLSD().with("GROUP_ID", group_id).with("FROM_ID", from_id));
				info->mDesc = item_name;
                info->mHost = sender;
			}
			
			// Tokenize the string.
			// TODO: Support escaped tokens ("||" -> "|")
			typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
            boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
            tokenizer tokens(message, sep);
			tokenizer::iterator iter = tokens.begin();

			std::string subj(*iter++);
			std::string mes(*iter++);

			// Send the notification down the new path.
			// For requested notices, we don't want to send the popups.
			if (dialog != IM_GROUP_NOTICE_REQUESTED)
			{
				LLSD payload;
				payload["subject"] = subj;
				payload["message"] = mes;
				payload["sender_name"] = name;
                payload["sender_id"] = agent_id;
				payload["group_id"] = group_id;
				payload["inventory_name"] = item_name;
                payload["received_time"] = LLDate::now();
				if (info && info->asLLSD())
				{
					payload["inventory_offer"] = info->asLLSD();
				}

				LLSD args;
				args["SUBJECT"] = subj;
				args["MESSAGE"] = mes;
				LLDate notice_date = LLDate(timestamp).notNull() ? LLDate(timestamp) : LLDate::now();
				LLNotifications::instance().add(LLNotification::Params("GroupNotice").substitutions(args).payload(payload).timestamp(notice_date));
			}

			// Also send down the old path for now.
			if (IM_GROUP_NOTICE_REQUESTED == dialog)
			{
				LLGroupActions::showNotice(subj,mes,group_id,has_inventory,item_name,info);
			}
			else
			{
				delete info;
			}
		}
		break;
	case IM_GROUP_INVITATION:
		{
            if (!is_muted)
            {
                // group is not blocked, but we still need to check agent that sent the invitation
                // and we have no agent's id
                // Note: server sends username "first.last".
			size_t index = original_name.find(" Resident");
			if (index != std::string::npos)
			{
				original_name = original_name.substr(0, index);
			}
			std::string legacy_name = gCacheName->buildLegacyName(original_name);
			LLUUID agent_id;
			gCacheName->getUUID(legacy_name, agent_id);
			if (agent_id.isNull())
			{
				LL_WARNS("Messaging") << "buildLegacyName returned null while processing " << original_name << LL_ENDL;
			}
			else
			{
				is_muted |= (bool) LLMuteList::getInstance()->isMuted(agent_id);
			}
            }
            //if (is_do_not_disturb || is_muted)
			if (is_muted) return;
			if (is_do_not_disturb)
            {
                send_do_not_disturb_message(gMessageSystem, from_id);
            }

            //if (!is_muted)
            {
				LL_INFOS("Messaging") << "Received IM_GROUP_INVITATION message." << LL_ENDL;
				// Read the binary bucket for more information.
				struct invite_bucket_t
				{
					S32 membership_fee;
					LLUUID role_id;
				}* invite_bucket;

				// Make sure the binary bucket is the correct size.
				if (binary_bucket_size != sizeof(invite_bucket_t))
				{
					LL_WARNS("Messaging") << "Malformed group invite binary bucket" << LL_ENDL;
					break;
				}

				invite_bucket = (struct invite_bucket_t*) &binary_bucket[0];
				S32 membership_fee = ntohl(invite_bucket->membership_fee);
				// NaCl - Antispam
				if (membership_fee > 0 && gSavedSettings.getBOOL("AntiSpamGroupFeeInvites"))
					return;
				// NaCl End

				LLSD payload;
				payload["transaction_id"] = session_id;
                payload["group_id"] = from_group ? from_id : aux_id;
				payload["name"] = name;
				payload["message"] = message;
				payload["fee"] = membership_fee;
                payload["use_offline_cap"] = session_id.isNull() && (offline == IM_OFFLINE);

				LLSD args;
				args["MESSAGE"] = message;
				// we shouldn't pass callback functor since it is registered in LLFunctorRegistration
				LLNotificationsUtil::add("JoinGroup", args, payload);
			}
		}
		break;

	case IM_INVENTORY_OFFERED:
	case IM_TASK_INVENTORY_OFFERED:
		// Someone has offered us some inventory.
		{
			LLOfferInfo* info = new LLOfferInfo;
			if (IM_INVENTORY_OFFERED == dialog)
			{
				struct offer_agent_bucket_t
				{
					S8		asset_type;
					LLUUID	object_id;
				}* bucketp;

				if (sizeof(offer_agent_bucket_t) != binary_bucket_size)
				{
					LL_WARNS("Messaging") << "Malformed inventory offer from agent" << LL_ENDL;
					delete info;
					break;
				}
				bucketp = (struct offer_agent_bucket_t*) &binary_bucket[0];
				info->mType = (LLAssetType::EType) bucketp->asset_type;
				info->mObjectID = bucketp->object_id;
				info->mFromObject = FALSE;
			}
			else // IM_TASK_INVENTORY_OFFERED
			{
                if (offline == IM_OFFLINE && session_id.isNull() && aux_id.notNull() && binary_bucket_size > sizeof(S8)* 5)
                {
                    // cap received offline message
                    std::string str_bucket = ll_safe_string((char*)binary_bucket, binary_bucket_size);
                    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
                    boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
                    tokenizer tokens(str_bucket, sep);
                    tokenizer::iterator iter = tokens.begin();

                    info->mType = (LLAssetType::EType)(atoi((*(iter++)).c_str()));
                    // Note There is more elements in 'tokens' ...

                    info->mObjectID = LLUUID::null;
                    info->mFromObject = TRUE;
                }
                else
                {
					if (sizeof(S8) != binary_bucket_size)
					{
						LL_WARNS("Messaging") << "Malformed inventory offer from object" << LL_ENDL;
						delete info;
						break;
					}
					info->mType = (LLAssetType::EType) binary_bucket[0];
					info->mObjectID = LLUUID::null;
					info->mFromObject = TRUE;
				}
            }

			info->mIM = dialog;
			info->mFromID = from_id;
			info->mFromGroup = from_group;
			info->mTransactionID = session_id;
			info->mFolderID = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));

			info->mFromName = name;
			info->mDesc = message;
            info->mHost = sender;
			//if (((is_do_not_disturb && !is_owned_by_me) || is_muted))
			if (is_muted)
			{
				// Prefetch the offered item so that it can be discarded by the appropriate observer. (EXT-4331)
                if (IM_INVENTORY_OFFERED == dialog)
                {
					LLInventoryFetchItemsObserver* fetch_item = new LLInventoryFetchItemsObserver(info->mObjectID);
					fetch_item->startFetch();
					delete fetch_item;
					// Same as closing window
					info->forceResponse(IOR_DECLINE);
                }
                else
                {
                    info->forceResponse(IOR_MUTE);
                }
            }
			/* Singu Note: Handle this inside inventory_offer_handler so if the user wants to autoaccept offers, they can while busy.
			// old logic: busy mode must not affect interaction with objects (STORM-565)
			// new logic: inventory offers from in-world objects should be auto-declined (CHUI-519)
			// Singu Note: We should use old logic
			else if (is_do_not_disturb && dialog != IM_TASK_INVENTORY_OFFERED) // busy mode must not affect interaction with objects (STORM-565)
			{
				// Until throttling is implemented, do not disturb mode should reject inventory instead of silently
				// accepting it.  SEE SL-39554
				info->forceResponse(IOR_DECLINE);
			}
			*/
			else
			{
				inventory_offer_handler(info, is_friend, is_owned_by_me);
			}
		}
		break;

	case IM_INVENTORY_ACCEPTED:
	{
//		args["NAME"] = LLAvatarActions::getSLURL(from_id);
// [RLVa:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
		// Only anonymize the name if the agent is nearby, there isn't an open IM session to them and their profile isn't open
		bool fRlvFilterName = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(from_id)) &&
			(!RlvUIEnabler::hasOpenProfile(from_id)) && (!RlvUIEnabler::hasOpenIM(from_id));
		args["NAME"] = (!fRlvFilterName) ? LLAvatarActions::getSLURL(from_id) : RlvStrings::getAnonym(name);
// [/RLVa:KB]
		LLNotificationsUtil::add("InventoryAccepted", args);
		break;
	}
	case IM_INVENTORY_DECLINED:
	{
//		args["NAME"] = LLAvatarActions::getSLURL(from_id);
// [RLVa:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
		// Only anonymize the name if the agent is nearby, there isn't an open IM session to them and their profile isn't open
		bool fRlvFilterName = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(from_id)) &&
			(!RlvUIEnabler::hasOpenProfile(from_id)) && (!RlvUIEnabler::hasOpenIM(from_id));
		args["NAME"] = (!fRlvFilterName) ? LLAvatarActions::getSLURL(from_id) : RlvStrings::getAnonym(name);
// [/RLVa:KB]
		LLNotificationsUtil::add("InventoryDeclined", args);
		break;
	}
		// TODO: _DEPRECATED suffix as part of vote removal - DEV-24856
	case IM_GROUP_VOTE:
	{
		LLSD args;
		args["NAME"] = name;
		args["MESSAGE"] = message;

		LLSD payload;
		payload["group_id"] = session_id;
		LLNotificationsUtil::add("GroupVote", args, payload);
	}
	break;

	case IM_GROUP_ELECTION_DEPRECATED:
	{
		LL_WARNS("Messaging") << "Received IM: IM_GROUP_ELECTION_DEPRECATED" << LL_ENDL;
	}
	break;

	case IM_FROM_TASK:
		{
			if (is_do_not_disturb && !is_owned_by_me)
			{
				return;
			}
			chat.mText = name + separator_string + message.substr(message_offset);
			chat.mFromName = name;

			// Build a link to open the object IM info window.
			std::string location = ll_safe_string((char*)binary_bucket, binary_bucket_size-1);

			if (session_id.notNull())
			{
				chat.mFromID = session_id;
			}
			else
			{
				// This message originated on a region without the updated code for task id and slurl information.
				// We just need a unique ID for this object that isn't the owner ID.
				// If it is the owner ID it will overwrite the style that contains the link to that owner's profile.
				// This isn't ideal - it will make 1 style for all objects owned by the the same person/group.
				// This works because the only thing we can really do in this case is show the owner name and link to their profile.
				chat.mFromID = from_id ^ gAgent.getSessionID();
			}

			chat.mSourceType = CHAT_SOURCE_OBJECT;

			// To conclude that the source type of message is CHAT_SOURCE_SYSTEM it's not
			// enough to check only from name (i.e. fromName = "Second Life"). For example
			// source type of messages from objects called "Second Life" should not be CHAT_SOURCE_SYSTEM.
			bool chat_from_system = (SYSTEM_FROM == name) && region_id.isNull() && position.isNull();
			if (chat_from_system)
			{
				// System's UUID is NULL (fixes EXT-4766)
				chat.mFromID = LLUUID::null;
				chat.mSourceType = CHAT_SOURCE_SYSTEM;
			}
			else script_msg_api(chat.mFromID.asString() + ", 6");

			// IDEVO Some messages have embedded resident names
			message = clean_name_from_task_im(message, from_group);

			LLSD query_string;
			query_string["owner"] = from_id;
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
			if (rlv_handler_t::isEnabled())
			{
				// NOTE: the chat message itself will be filtered in LLNearbyChatHandler::processChat()
				if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && (!from_group) && (RlvUtil::isNearbyAgent(from_id)) )
				{
					query_string["rlv_shownames"] = TRUE;

					RlvUtil::filterNames(name);
					chat.mFromName = name;
				}
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
				{
					std::string::size_type idxPos = location.find('/');
					if ( (std::string::npos != idxPos) && (RlvUtil::isNearbyRegion(location.substr(0, idxPos))) )
						location = RlvStrings::getString(RLV_STRING_HIDDEN_REGION);
				}
			}
// [/RLVa:KB]
			query_string["slurl"] = location;
			query_string["name"] = name;
			if (from_group)
			{
				query_string["groupowned"] = "true";
			}

//			chat.mURL = LLSLURL("objectim", session_id, "").getSLURLString();
// [SL:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
			chat.mURL = LLSLURL("objectim", session_id, LLURI::mapToQueryString(query_string)).getSLURLString();
// [/SL:KB]
			chat.mText = name + separator_string + message.substr(message_offset);

			// Note: lie to Nearby Chat, pretending that this is NOT an IM, because
			// IMs from objects don't open IM sessions.
			LLFloaterChat::addChat(chat, FALSE, FALSE);
		}
		break;

	case IM_SESSION_SEND: // ad-hoc or group IMs
	{
		if (!is_linden && is_do_not_disturb) return;

		// Only show messages if we have a session open (which
		// should happen after you get an "invitation"
//		if ( !gIMMgr->hasSession(session_id) )
// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0c) | Modified: RLVa-1.3.0c
		LLFloaterIMPanel* pIMFloater = gIMMgr->findFloaterBySession(session_id);
		if (!pIMFloater)
		{
			return;
		}

		if (from_id != gAgentID && (gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM) || gRlvHandler.hasBehaviour(RLV_BHVR_RECVIMFROM)))
		{
			switch (pIMFloater->getSessionType())
			{
				case LLFloaterIMPanel::GROUP_SESSION:	// Group chat
					if (!RlvActions::canReceiveIM(session_id))
						return;
					break;
				case LLFloaterIMPanel::ADHOC_SESSION:	// Conference chat
					if (!RlvActions::canReceiveIM(from_id))
						message = RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM);
					break;
				default:
					RLV_ASSERT(false);
					return;
			}
		}
// [/RLVa:KB]

		// standard message, not from system
		std::string saved;
		if (offline == IM_OFFLINE)
		{
			LLStringUtil::format_map_t args;
			args["[LONG_TIMESTAMP]"] = formatted_time(timestamp);
			saved = LLTrans::getString("Saved_message", args);
		}
		buffer = separator_string + saved + message.substr(message_offset);

		LL_DEBUGS("Messaging") << "standard message session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

		gIMMgr->addMessage(
			session_id,
			from_id,
			name,
			buffer,
			ll_safe_string((char*)binary_bucket),
			IM_SESSION_INVITE,
			parent_estate_id,
			region_id,
			position,
			true);

		std::string prepend_msg;
		if (gAgent.isInGroup(session_id)&& gSavedSettings.getBOOL("OptionShowGroupNameInChatIM"))
		{
			prepend_msg = '[';
			prepend_msg += std::string((char*)binary_bucket);
			prepend_msg += "] ";
		}
		else
		{
			prepend_msg = std::string("IM: ");
		}
		chat.mText = prepend_msg + name + separator_string + saved + message.substr(message_offset);
		LLFloaterChat::addChat(chat, TRUE, from_id == gAgentID);

		break;
	}
	case IM_FROM_TASK_AS_ALERT:
		if (is_do_not_disturb && !is_owned_by_me)
		{
			return;
		}
		{
			// Construct a viewer alert for this message.
			args["NAME"] = name;
			args["MESSAGE"] = message;
			LLNotificationsUtil::add("ObjectMessage", args);
		}
		break;
	case IM_BUSY_AUTO_RESPONSE:
		if (is_muted)
		{
			LL_DEBUGS("Messaging") << "Ignoring do-not-disturb response from " << from_id << LL_ENDL;
			return;
		}
		else
		{
			gIMMgr->addMessage(session_id, from_id, name, separator_string + message.substr(message_offset), name, dialog, parent_estate_id, region_id, position, true);
		}
		break;
		
	case IM_LURE_USER:
	case IM_TELEPORT_REQUEST:
		{
 // [RLVa:KB] - Checked: RLVa-1.4.9
			// If we auto-accept the offer/request then this will override DnD status (but we'll still let the other party know later)
			bool fRlvAutoAccept = (rlv_handler_t::isEnabled()) &&
				( ((IM_LURE_USER == dialog) && (RlvActions::autoAcceptTeleportOffer(from_id))) ||
				  ((IM_TELEPORT_REQUEST == dialog) && (RlvActions::autoAcceptTeleportRequest(from_id))) );
// [/RLVa:KB]

			bool following = gAgent.getAutoPilotLeaderID() == from_id;

			if (!following && is_muted)
			{ 
				return;
			}
//			else if (!following && is_do_not_disturb)
// [RLVa:KB] - Checked: 2013-11-08 (RLVa-1.4.9)
			else if (!following && is_do_not_disturb && !fRlvAutoAccept )
// [/RLVa:KB]
			{
				send_do_not_disturb_message(gMessageSystem, from_id);
			}
			else
			{
//				if (!following && is_do_not_disturb) 
// [RLVa:KB] - Checked: RLVa-1.4.9
				if (!following && (is_do_not_disturb) && (!fRlvAutoAccept) )
// [/RLVa:KB]
                {
                    send_do_not_disturb_message(gMessageSystem, from_id);
                }

				LLVector3 pos, look_at;
				U64 region_handle(0);
				U8 region_access(SIM_ACCESS_MIN);
				std::string region_info = ll_safe_string((char*)binary_bucket, binary_bucket_size);
				std::string region_access_str = LLStringUtil::null;
				std::string region_access_icn = LLStringUtil::null;
				std::string region_access_lc  = LLStringUtil::null;

				bool canUserAccessDstRegion = true;
				bool doesUserRequireMaturityIncrease = false;

				// Do not parse the (empty) lure bucket for TELEPORT_REQUEST
				if (IM_TELEPORT_REQUEST != dialog && parse_lure_bucket(region_info, region_handle, pos, look_at, region_access))
				{
					region_access_str = LLViewerRegion::accessToString(region_access);
					region_access_icn = LLViewerRegion::getAccessIcon(region_access);
					region_access_lc  = region_access_str;
					LLStringUtil::toLower(region_access_lc);

					if (!gAgent.isGodlike())
					{
						switch (region_access)
						{
						case SIM_ACCESS_MIN:
						case SIM_ACCESS_PG:
							break;
						case SIM_ACCESS_MATURE:
							if (gAgent.isTeen())
							{
								canUserAccessDstRegion = false;
							}
							else if (gAgent.prefersPG())
							{
								doesUserRequireMaturityIncrease = true;
							}
							break;
						case SIM_ACCESS_ADULT:
							if (!gAgent.isAdult())
							{
								canUserAccessDstRegion = false;
							}
							else if (!gAgent.prefersAdult())
							{
								doesUserRequireMaturityIncrease = true;
							}
							break;
						default:
							llassert(0);
							break;
						}
					}
				}

// [RLVa:KB] - Checked: RLVa-1.4.9
				if (rlv_handler_t::isEnabled())
				{
					if ( ((IM_LURE_USER == dialog) && (!RlvActions::canAcceptTpOffer(from_id))) ||
					     ((IM_TELEPORT_REQUEST == dialog) && (!RlvActions::canAcceptTpRequest(from_id))) )
					{
						RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RLV_STRING_BLOCKED_TPLUREREQ_REMOTE));
						if (is_do_not_disturb)
							send_do_not_disturb_message(gMessageSystem, from_id);
						return;
					}

					// Censor message if: 1) restricted from receiving IMs from the sender, or 2) teleport offer/request and @showloc=n restricted
					if ( (!RlvActions::canReceiveIM(from_id)) || 
						 ((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) && (IM_LURE_USER == dialog || IM_TELEPORT_REQUEST == dialog)) )
					{
						message = RlvStrings::getString(RLV_STRING_HIDDEN);
					}
				}
// [/RLVa:KB]

				LLSD args;
				// *TODO: Translate -> [FIRST] [LAST] (maybe)
				args["NAME"] = LLAvatarActions::getSLURL(from_id);
				args["MESSAGE"] = message;
				args["MATURITY_STR"] = region_access_str;
				args["MATURITY_ICON"] = region_access_icn;
				args["REGION_CONTENT_MATURITY"] = region_access_lc;
				LLSD payload;
				payload["from_id"] = from_id;
				payload["lure_id"] = session_id;
				payload["godlike"] = FALSE;
				payload["region_maturity"] = region_access;

				/* Singu TODO: Figure if we should use these
				if (!canUserAccessDstRegion)
				{
					LLNotification::Params params("TeleportOffered_MaturityBlocked");
					params.substitutions = args;
					params.payload = payload;
                    LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
					send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
					send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
				}
				else if (doesUserRequireMaturityIncrease)
				{
					LLNotification::Params params("TeleportOffered_MaturityExceeded");
					params.substitutions = args;
					params.payload = payload;
                    LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
				}
				else
				*/
				{
					/* Singu Note: No default constructor for LLNotification::Params
					LLNotification::Params params;
					if (IM_LURE_USER == dialog)
					{
						params.name = "TeleportOffered";
						params.functor_name = "TeleportOffered";
					}
					else if (IM_TELEPORT_REQUEST == dialog)
					{
						params.name = "TeleportRequest";
						params.functor_name = "TeleportRequest";
					}
					*/
					LLNotification::Params params(IM_LURE_USER == dialog ? "TeleportOffered" : "TeleportRequest");
					params.substitutions = args;
					params.payload = payload;

					if (following)
					{
						LLNotifications::instance().forceResponse(LLNotification::Params(params.name).payload(payload), 0);
					}
					else
// [RLVa:KB] - Checked: 20103-11-08 (RLVa-1.4.9)
					if ( (rlv_handler_t::isEnabled()) && (fRlvAutoAccept) )
					{
						if (IM_LURE_USER == dialog)
							gRlvHandler.setCanCancelTp(false);
						if (is_do_not_disturb)
							send_do_not_disturb_message(gMessageSystem, from_id);
						LLNotifications::instance().forceResponse(LLNotification::Params(params.name).payload(payload), 0);
					}
					else
					{
						LLNotifications::instance().add(params);

						// <edit>
						if (IM_LURE_USER == dialog)
							gAgent.showLureDestination(LLAvatarActions::getSLURL(from_id), region_handle, pos.mV[VX], pos.mV[VY], pos.mV[VZ]);
						script_msg_api(from_id.asString().append(IM_LURE_USER == dialog ? ", 2" : ", 3"));
						// </edit>
					}
// [/RLVa:KB]
//					LLNotifications::instance().add(params);
				}
			}
		}
		break;

	case IM_GODLIKE_LURE_USER:
		{
			LLVector3 pos, look_at;
			U64 region_handle(0);
			U8 region_access(SIM_ACCESS_MIN);
			std::string region_info = ll_safe_string((char*)binary_bucket, binary_bucket_size);
			std::string region_access_str = LLStringUtil::null;
			std::string region_access_icn = LLStringUtil::null;
			std::string region_access_lc  = LLStringUtil::null;

			bool canUserAccessDstRegion = true;
			bool doesUserRequireMaturityIncrease = false;

            if (parse_lure_bucket(region_info, region_handle, pos, look_at, region_access))
			{
				region_access_str = LLViewerRegion::accessToString(region_access);
				region_access_icn = LLViewerRegion::getAccessIcon(region_access);
				region_access_lc  = region_access_str;
				LLStringUtil::toLower(region_access_lc);

				if (!gAgent.isGodlike())
				{
					switch (region_access)
					{
					case SIM_ACCESS_MIN:
					case SIM_ACCESS_PG:
						break;
					case SIM_ACCESS_MATURE:
						if (gAgent.isTeen())
						{
							canUserAccessDstRegion = false;
						}
						else if (gAgent.prefersPG())
						{
							doesUserRequireMaturityIncrease = true;
						}
						break;
					case SIM_ACCESS_ADULT:
						if (!gAgent.isAdult())
						{
							canUserAccessDstRegion = false;
						}
						else if (!gAgent.prefersAdult())
						{
							doesUserRequireMaturityIncrease = true;
						}
						break;
					default:
						llassert(0);
						break;
					}
				}
			}

			LLSD args;
			// *TODO: Translate -> [FIRST] [LAST] (maybe)
			args["NAME"] = LLAvatarActions::getSLURL(from_id);
			args["MESSAGE"] = message;
			args["MATURITY_STR"] = region_access_str;
			args["MATURITY_ICON"] = region_access_icn;
			args["REGION_CONTENT_MATURITY"] = region_access_lc;
			LLSD payload;
			payload["from_id"] = from_id;
			payload["lure_id"] = session_id;
			payload["godlike"] = TRUE;
			payload["region_maturity"] = region_access;

            /*if (!canUserAccessDstRegion)
            {
                LLNotification::Params params("TeleportOffered_MaturityBlocked");
                params.substitutions = args;
                params.payload = payload;
                LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
                send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
                send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
            }
            else if (doesUserRequireMaturityIncrease)
            {
                LLNotification::Params params("TeleportOffered_MaturityExceeded");
                params.substitutions = args;
                params.payload = payload;
                LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
            }
            else*/
            {
				// do not show a message box, because you're about to be
				// teleported.
				LLNotifications::instance().forceResponse(LLNotification::Params("TeleportOffered").payload(payload), 0);
			}
        }
		break;

	case IM_GOTO_URL:
		{
			LLSD args;
			// n.b. this is for URLs sent by the system, not for
			// URLs sent by scripts (i.e. llLoadURL)
			if (binary_bucket_size <= 0)
			{
				LL_WARNS("Messaging") << "bad binary_bucket_size: "
					<< binary_bucket_size
					<< " - aborting function." << LL_ENDL;
				return;
			}

			std::string url;
			
			url.assign((char*)binary_bucket, binary_bucket_size-1);
			args["MESSAGE"] = message;
			args["URL"] = url;
			LLSD payload;
			payload["url"] = url;
			LLNotificationsUtil::add("GotoURL", args, payload);
		}
		break;

	case IM_FRIENDSHIP_OFFERED:
		{
			LLSD payload;
			payload["from_id"] = from_id;
			payload["session_id"] = session_id;
			payload["online"] = (offline == IM_ONLINE);
            payload["sender"] = sender.getIPandPort();

			if (is_muted)
			{
				LLNotifications::instance().forceResponse(LLNotification::Params("OfferFriendship").payload(payload), 1);
			}
			else
			{
				if (is_do_not_disturb)
				{
                    send_do_not_disturb_message(gMessageSystem, from_id);
				}
				args["[NAME_SLURL]"] = LLAvatarActions::getSLURL(from_id);
				if (message.empty())
				{
					//support for frienship offers from clients before July 2008
				        LLNotificationsUtil::add("OfferFriendshipNoMessage", args, payload);
				}
				else
				{
					args["[MESSAGE]"] = message;
				    LLNotificationsUtil::add("OfferFriendship", args, payload);
				}
			}
		}
		break;

	case IM_FRIENDSHIP_ACCEPTED:
		{
			// In the case of an offline IM, the formFriendship() may be extraneous
			// as the database should already include the relationship.  But it
			// doesn't hurt for dupes.
			LLAvatarTracker::formFriendship(from_id);
			
			std::vector<std::string> strings;
			strings.push_back(from_id.asString());
			send_generic_message("requestonlinenotification", strings);
			
			args["NAME"] = LLAvatarActions::getSLURL(from_id);
			LLSD payload;
			payload["from_id"] = from_id;
			LLAvatarNameCache::get(from_id, boost::bind(&notification_display_name_callback, _1, _2, "FriendshipAccepted", args, payload));
		}
		break;

	case IM_FRIENDSHIP_DECLINED_DEPRECATED:
	default:
		LL_WARNS("Messaging") << "Instant message calling for unknown dialog "
				<< (S32)dialog << LL_ENDL;
		break;
	}

	LLWindow* viewer_window = gViewerWindow->getWindow();
	if (viewer_window && viewer_window->getMinimized() && gSavedSettings.getBOOL("LiruFlashWhenMinimized"))
	{
		viewer_window->flashIcon(5.f);
	}
}

void LLIMProcessing::requestOfflineMessages()
{
    static BOOL requested = FALSE;
    if (!requested
        && gMessageSystem
        && LLMuteList::getInstance()->isLoaded()
        && isAgentAvatarValid()
        && gAgent.getRegion()
        && gAgent.getRegion()->capabilitiesReceived())
    {
        std::string cap_url = gAgent.getRegionCapability("ReadOfflineMsgs");

        // Auto-accepted inventory items may require the avatar object
        // to build a correct name.  Likewise, inventory offers from
        // muted avatars require the mute list to properly mute.
        if (cap_url.empty()
            || gAgent.getRegionCapability("AcceptFriendship").empty()
            || gAgent.getRegionCapability("AcceptGroupInvite").empty())
        {
            // Offline messages capability provides no session/transaction ids for message AcceptFriendship and IM_GROUP_INVITATION to work
            // So make sure we have the caps before using it.
            requestOfflineMessagesLegacy();
        }
        else
        {
            LLHTTPClient::get(cap_url, new LLCoroResponder(
                LLIMProcessing::requestOfflineMessagesCoro));
        }
        requested = TRUE;
    }
}

void LLIMProcessing::requestOfflineMessagesCoro(const LLCoroResponder& responder)
{
	if (LLApp::isQuitting() || !gAgent.getRegion())
	{
		return;
	}

    auto status = responder.getStatus();

    if (!responder.isGoodStatus(status)) // success = httpResults["success"].asBoolean();
    {
        LL_WARNS("Messaging") << "Error requesting offline messages via capability " << responder.getURL() << ", Status: " << status << ", Reason: " << responder.getReason() << "\nFalling back to legacy method." << LL_ENDL;

        requestOfflineMessagesLegacy();
        return;
    }

    const auto& contents = responder.getContent();

    if (!contents.size())
    {
        LL_WARNS("Messaging") << "No contents received for offline messages via capability " << responder.getURL() << LL_ENDL;
        return;
    }

    // Todo: once dirtsim-369 releases, remove one of the map/array options
    LLSD messages;
    if (contents.isArray())
    {
        messages = *contents.beginArray();
    }
    else if (contents.has("messages"))
    {
        messages = contents["messages"];
    }
    else
    {
        LL_WARNS("Messaging") << "Invalid offline message content received via capability " << responder.getURL() << LL_ENDL;
        return;
    }

    if (!messages.isArray())
    {
        LL_WARNS("Messaging") << "Invalid offline message content received via capability " << responder.getURL() << LL_ENDL;
        return;
    }

    if (messages.emptyArray())
    {
        // Nothing to process
        return;
    }

    LL_INFOS("Messaging") << "Processing offline messages." << LL_ENDL;

    std::vector<U8> data;
    S32 binary_bucket_size = 0;
    LLHost sender = gAgent.getRegion()->getHost();

    LLSD::array_iterator i = messages.beginArray();
    LLSD::array_iterator iEnd = messages.endArray();
    for (; i != iEnd; ++i)
    {
        const LLSD &message_data(*i);

        LLVector3 position(message_data["local_x"].asReal(), message_data["local_y"].asReal(), message_data["local_z"].asReal());
        data = message_data["binary_bucket"].asBinary();
        binary_bucket_size = data.size(); // message_data["count"] always 0
        U32 parent_estate_id = message_data.has("parent_estate_id") ? message_data["parent_estate_id"].asInteger() : 1; // 1 - IMMainland

        // Todo: once dirtsim-369 releases, remove one of the int/str options
        BOOL from_group;
        if (message_data["from_group"].isInteger())
        {
            from_group = message_data["from_group"].asInteger();
        }
        else
        {
            from_group = message_data["from_group"].asString() == "Y";
        }

        auto agentName = message_data["from_agent_name"].asString();
        auto message = message_data["message"].asString();
        LLIMProcessing::processNewMessage(message_data["from_agent_id"].asUUID(),
            from_group,
            message_data["to_agent_id"].asUUID(),
            IM_OFFLINE,
            (EInstantMessage)message_data["dialog"].asInteger(),
            LLUUID::null, // session id, since there is none we can only use frienship/group invite caps
            message_data["timestamp"].asInteger(),
            agentName,
            message,
            parent_estate_id,
            message_data["region_id"].asUUID(),
            position,
            &data[0],
            binary_bucket_size,
            sender,
            message_data["asset_id"].asUUID()); // not necessarily an asset
    }
}

void LLIMProcessing::requestOfflineMessagesLegacy()
{
    LL_INFOS("Messaging") << "Requesting offline messages (Legacy)." << LL_ENDL;

    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_RetrieveInstantMessages);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    gAgent.sendReliableMessage();
}

