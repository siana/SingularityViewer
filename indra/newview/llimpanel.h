/** 
 * @file llimpanel.h
 * @brief LLIMPanel class definition
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_IMPANEL_H
#define LL_IMPANEL_H

#include "llcallingcard.h"
#include "llfloater.h"
#include "lllogchat.h"
#include "llmutelist.h"

class LLAvatarName;
class LLIMSpeakerMgr;
class LLInventoryCategory;
class LLInventoryItem;
class LLLineEditor;
class LLParticipantList;
class LLViewerTextEditor;
class LLVoiceChannel;

class LLFloaterIMPanel : public LLFloater, public LLFriendObserver, public LLMuteListObserver
{
public:

	// The session id is the id of the session this is for. The target
	// refers to the user (or group) that where this session serves as
	// the default. For example, if you open a session though a
	// calling card, a new session id will be generated, but the
	// target_id will be the agent referenced by the calling card.
	LLFloaterIMPanel(const std::string& log_label,
					 const LLUUID& session_id,
					 const LLUUID& target_id,
					 const EInstantMessage& dialog,
					 const uuid_vec_t& ids = uuid_vec_t());
	virtual ~LLFloaterIMPanel();

	void onAvatarNameLookup(const LLAvatarName& avatar_name);

	/*virtual*/ void changed(U32 mask); // From LLFriendObserver, check friend status
	/*virtual*/ void onChange() {}
	/*virtual*/ void onChangeDetailed(const LLMute& mute); // From LLMuteListObserver, check for mute status changes for OtherParticipant
	/*virtual*/ BOOL postBuild();

	// Check typing timeout timer.
	/*virtual*/ void draw();
	/*virtual*/ void onClose(bool app_quitting = FALSE);
	/*virtual*/ void handleVisibilityChange(BOOL new_visibility);

	// add target ids to the session. 
	// Return TRUE if successful, otherwise FALSE.
	bool inviteToSession(const uuid_vec_t& agent_ids);

	void addHistoryLine(const std::string &utf8msg, 
						LLColor4 incolor = LLColor4::white, 
						bool log_to_file = true,
						const LLUUID& source = LLUUID::null,
						const std::string& name = LLStringUtil::null);

	void setInputFocus(bool b);

	void setVisible(BOOL b);

	S32 getNumUnreadMessages() { return mNumUnreadMessages; }

	BOOL handleKeyHere(KEY key, MASK mask);
	BOOL handleDragAndDrop(S32 x, S32 y, MASK mask,
						   BOOL drop, EDragAndDropType cargo_type,
						   void *cargo_data, EAcceptance *accept,
						   std::string& tooltip_msg);

	BOOL    focusFirstItem(BOOL prefer_text_fields = FALSE, BOOL focus_flash = TRUE );

	void            onFocusReceived();
	void			onInputEditorFocusReceived();
	void			onInputEditorKeystroke(LLLineEditor* caller);

	void			onClickHistory();
	void			onFlyoutCommit(class LLComboBox* flyout, const LLSD& value);
	static void*	createSpeakersPanel(void* data);

	//callbacks for P2P muting and volume control
	void onClickMuteVoice();

	enum SType
	{
		P2P_SESSION,
		GROUP_SESSION,
		SUPPORT_SESSION,
		ADHOC_SESSION
	};
	const SType& getSessionType() const { return mSessionType; }
	const LLUUID& getSessionID() const { return mSessionUUID; }
	void processSessionUpdate(const LLSD& update);
	LLVoiceChannel* getVoiceChannel() { return mVoiceChannel; }
	LLIMSpeakerMgr* getSpeakerManager() const { return mSpeakers; } // Singu TODO: LLIMModel::getSpeakerManager

	void sessionInitReplyReceived(const LLUUID& im_session_id);

	// Handle other participant in the session typing.
	void processIMTyping(const LLUUID& from_id, BOOL typing);
	void chatFromLogFile(LLLogChat::ELogLineType type, const std::string& line);

	//show error statuses to the user
	void showSessionStartError(const std::string& error_string);
	void showSessionEventError(
		const std::string& event_string,
		const std::string& error_string);
	void showSessionForceClose(const std::string& reason);

	static bool onConfirmForceCloseError(const LLSD& notification, const LLSD& response);

	// LLIMModel Functionality
	bool getSessionInitialized() const { return mSessionInitialized; }
	bool mStartCallOnInitialize;

protected:
	friend class LLViewerObjectList;
	void addDynamicFocus();
	void removeDynamicFocus();

private:
	friend class LLSpeakerMgr;
	// Called by UI methods.
	void onSendMsg();

	// Build the flyout list
	void rebuildDynamics(LLComboBox* flyout) { removeDynamics(flyout); addDynamics(flyout); }
	void removeDynamics(LLComboBox* flyout);
	void addDynamics(LLComboBox* flyout);

	// Called by handleKeyHere
	void closeIfNotPinned();

	// for adding agents via the UI. Return TRUE if possible, do it if 
	BOOL dropCallingCard(LLInventoryItem* item, BOOL drop);
	BOOL dropCategory(LLInventoryCategory* category, BOOL drop);

	// test if local agent can add agents.
	bool isInviteAllowed() const;

	void onAddButtonClicked();
	void addSessionParticipants(const uuid_vec_t& uuids);
	void addP2PSessionParticipants(const LLSD& notification, const LLSD& response, const uuid_vec_t& uuids);
	bool canAddSelectedToChat(const uuid_vec_t& uuids) const;

	// Called whenever the user starts or stops typing.
	// Sends the typing state to the other user if necessary.
	void setTyping(bool typing);

	// Add the "User is typing..." indicator.
	void addTypingIndicator(const LLUUID& from_id);

	// Remove the "User is typing..." indicator.
	void removeTypingIndicator(const LLUUID& from_id = LLUUID::null);

	void sendTypingState(bool typing);

	const bool isModerator(const LLUUID& speaker_id);
	
private:
	LLLineEditor* mInputEditor;
	LLViewerTextEditor* mHistoryEditor;

	bool mSessionInitialized;

	// Where does the "Starting session..." line start and how long is it?
	std::pair<S32, S32> mSessionStartMsgPos;

	SType mSessionType;

	// The value of the mSessionUUID depends on how the IM session was started:
	//   one-on-one  ==> random id
	//   group ==> group_id
	//   inventory folder ==> folder item_id 
	//   911 ==> Gaurdian_Angel_Group_ID ^ gAgent.getID()
	LLUUID mSessionUUID;

	// mLogLabel is the name of the log file before directories and extensions
	std::string mLogLabel;

	LLSD mQueuedMsgsForInit;

	// The value mOtherParticipantUUID depends on how the IM session was started:
	//   one-on-one = recipient's id
	//   group ==> group_id
	//   inventory folder ==> first target id in list
	//   911 ==> sender
	LLUUID mOtherParticipantUUID;
	uuid_vec_t mInitialTargetIDs;

	EInstantMessage mDialog;

	// Are you currently typing?
	bool mTyping;

	// Where does the "User is typing..." line start?
	S32 mTypingLineStartIndex;

	// Is other user currently typing?
	bool mOtherTyping;

	// name of other user who is currently typing
	std::string mOtherTypingName;

	S32 mNumUnreadMessages;

	bool mSentTypingState;

	bool mShowSpeakersOnConnect;

	bool mDing; // Whether or not to play a ding on new messages
	bool mRPMode;

	bool mTextIMPossible;
	bool mCallBackEnabled;

	LLIMSpeakerMgr* mSpeakers;
	LLParticipantList* mSpeakerPanel;
	LLVoiceChannel*	mVoiceChannel;
	
	// Optimization:  Don't send "User is typing..." until the
	// user has actually been typing for a little while.  Prevents
	// extra IMs for brief "lol" type utterences.
	LLFrameTimer mFirstKeystrokeTimer;

	// Timer to detect when user has stopped typing.
	LLFrameTimer mLastKeystrokeTimer;

	CachedUICtrl<LLUICtrl> mVolumeSlider;
	CachedUICtrl<LLUICtrl> mEndCallBtn;
	CachedUICtrl<LLUICtrl> mStartCallBtn;
	CachedUICtrl<LLUICtrl> mSendBtn;
	CachedUICtrl<LLUICtrl> mMuteBtn;
};


#endif  // LL_IMPANEL_H
