/** 
 * @file LLIMMgr.h
 * @brief Container for Instant Messaging
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLIMVIEW_H
#define LL_LLIMVIEW_H

#include "llmultifloater.h"
#include "llinstantmessage.h"
#include "lluuid.h"
#include "llvoicechannel.h"


class LLFloaterChatterBox;
class LLFloaterIMPanel;

class LLIMMgr final : public LLSingleton<LLIMMgr>
{
public:
	enum EInvitationType
	{
		INVITATION_TYPE_INSTANT_MESSAGE = 0,
		INVITATION_TYPE_VOICE = 1,
		INVITATION_TYPE_IMMEDIATE = 2
	};

	LLIMMgr();
	virtual ~LLIMMgr();

	// Add a message to a session. The session can keyed to sesion id
	// or agent id.
	void addMessage(const LLUUID& session_id,
					const LLUUID& target_id,
					const std::string& from,
					const std::string& msg,
					const std::string& session_name = LLStringUtil::null,
					EInstantMessage dialog = IM_NOTHING_SPECIAL,
					U32 parent_estate_id = 0,
					const LLUUID& region_id = LLUUID::null,
					const LLVector3& position = LLVector3::zero,
					bool link_name = false);

	void addSystemMessage(const LLUUID& session_id, const std::string& message_name, const LLSD& args);

	// This method returns TRUE if the local viewer has a session
	// currently open keyed to the uuid. The uuid can be keyed by
	// either session id or agent id.
	BOOL isIMSessionOpen(const LLUUID& uuid);

	// This adds a session to the talk view. The name is the local
	// name of the session, dialog specifies the type of
	// session. Since sessions can be keyed off of first recipient or
	// initiator, the session can be matched against the id
	// provided. If the session exists, it is brought forward.  This
	// method accepts a group id or an agent id. Specifying id = NULL
	// results in an im session to everyone. Returns the uuid of the
	// session.
	LLUUID addSession(const std::string& name,
					  EInstantMessage dialog,
					  const LLUUID& other_participant_id);

	// Adds a session using a specific group of starting agents
	// the dialog type is assumed correct. Returns the uuid of the session.
	LLUUID addSession(const std::string& name,
					  EInstantMessage dialog,
					  const LLUUID& other_participant_id,
					  const uuid_vec_t& ids);

	// Creates a P2P session with the requisite handle for responding to voice calls
	LLUUID addP2PSession(const std::string& name,
					  const LLUUID& other_participant_id,
					  const std::string& voice_session_handle,
					  const std::string& caller_uri = LLStringUtil::null);

	// This removes the panel referenced by the uuid, and then
	// restores internal consistency. The internal pointer is not
	// deleted.
	void removeSession(const LLUUID& session_id);

	void inviteToSession(
		const LLUUID& session_id, 
		const std::string& session_name, 
		const LLUUID& caller, 
		const std::string& caller_name,
		EInstantMessage type,
		EInvitationType inv_type, 
		const std::string& session_handle = LLStringUtil::null,
		const std::string& session_uri = LLStringUtil::null);

	//Updates a given session's session IDs.  Does not open,
	//create or do anything new.  If the old session doesn't
	//exist, then nothing happens.
	void updateFloaterSessionID(const LLUUID& old_session_id,
								const LLUUID& new_session_id);

	void processIMTypingStart(const LLUUID& from_id, const EInstantMessage im_type);
	void processIMTypingStop(const LLUUID& from_id, const EInstantMessage im_type);

	void clearNewIMNotification();

	// automatically start a call once the session has initialized
	void autoStartCallOnStartup(const LLUUID& session_id);

	// IM received that you haven't seen yet
	int getIMUnreadCount();

	void		setFloaterOpen(BOOL open);		/*Flawfinder: ignore*/
	BOOL		getFloaterOpen();

	LLFloaterChatterBox* getFloater();

	// This method is used to go through all active sessions and
	// disable all of them. This method is usally called when you are
	// forced to log out or similar situations where you do not have a
	// good connection.
	void disconnectAllSessions();

	void toggle();

	BOOL hasSession(const LLUUID& session_id);

	// This method returns the im panel corresponding to the uuid
	// provided. The uuid must be a session id. Returns NULL if there
	// is no matching panel.
	LLFloaterIMPanel* findFloaterBySession(const LLUUID& session_id);

	static LLUUID computeSessionID(EInstantMessage dialog, const LLUUID& other_participant_id);

	void clearPendingInvitation(const LLUUID& session_id);

	void processAgentListUpdates(const LLUUID& session_id, const LLSD& body);
	LLSD getPendingAgentListUpdates(const LLUUID& session_id);
	void addPendingAgentListUpdates(
		const LLUUID& sessioN_id,
		const LLSD& updates);
	void clearPendingAgentListUpdates(const LLUUID& session_id);

	//HACK: need a better way of enumerating existing session, or listening to session create/destroy events
	const std::set<LLHandle<LLFloater> >& getIMFloaterHandles() { return mFloaters; }

	void loadIgnoreGroup();
	void saveIgnoreGroup();
	void updateIgnoreGroup(const LLUUID& group_id, bool ignore);
	// Returns true if group chat is ignored for the UUID, false if not
	bool getIgnoreGroup(const LLUUID& group_id) const;

	/**
	 * Start call in a session
	 * @return false if voice channel doesn't exist
	 **/
	bool startCall(const LLUUID& session_id, LLVoiceChannel::EDirection direction = LLVoiceChannel::OUTGOING_CALL);

	/**
	 * End call in a session
	 * @return false if voice channel doesn't exist
	 **/
	bool endCall(const LLUUID& session_id);

	void addNotifiedNonFriendSessionID(const LLUUID& session_id);

	bool isNonFriendSessionNotified(const LLUUID& session_id);

	static std::string getOfflineMessage(const LLUUID& id);

private:
	// create a panel and update internal representation for
	// consistency. Returns the pointer, caller (the class instance
	// since it is a private method) is not responsible for deleting
	// the pointer.
	LLFloaterIMPanel* createFloater(const LLUUID& session_id,
									const LLUUID& target_id,
									const std::string& name,
									const EInstantMessage& dialog,
									const uuid_vec_t& ids = uuid_vec_t(),
									bool user_initiated = false);

	// This simple method just iterates through all of the ids, and
	// prints a simple message if they are not online. Used to help
	// reduce 'hello' messages to the linden employees unlucky enough
	// to have their calling card in the default inventory.
	void noteOfflineUsers(LLFloaterIMPanel* panel, const uuid_vec_t& ids);
	void noteMutedUsers(LLFloaterIMPanel* panel, const uuid_vec_t& ids);

	void processIMTypingCore(const LLUUID& from_id, const EInstantMessage im_type, BOOL typing);

private:
	std::set<LLHandle<LLFloater> > mFloaters;

	// EXP-901
	// If "Only friends and groups can IM me" option is ON but the user got message from non-friend,
	// the user should be notified that to be able to see this message the option should be OFF.
	// This set stores session IDs in which user was notified. Need to store this IDs so that the user
	// be notified only one time per session with non-friend.
	typedef uuid_set_t notified_non_friend_sessions_t;
	notified_non_friend_sessions_t mNotifiedNonFriendSessions;

	// An IM has been received that you haven't seen yet.
	int		mIMUnreadCount;

	LLSD mPendingInvitations;
	LLSD mPendingAgentListUpdates;

	std::list<LLUUID> mIgnoreGroupList;

public:

	S32 getIgnoreGroupListCount() { return mIgnoreGroupList.size(); }
};

// Globals
extern LLIMMgr *gIMMgr;

#endif  // LL_LLIMVIEW_H
