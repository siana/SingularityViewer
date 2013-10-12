/** 
 * @file llimpanel.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llimpanel.h"

#include "ascentkeyword.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llfloaterchat.h"
#include "llfloaterinventory.h"
#include "llfloaterwebcontent.h" // For web browser display of logs
#include "llgroupactions.h"
#include "llhttpclient.h"
#include "llimview.h"
#include "llinventory.h"
#include "llinventoryfunctions.h"
#include "lllineeditor.h"
#include "llmutelist.h"
#include "llnotificationsutil.h"
#include "llparticipantlist.h"
#include "llspeakers.h"
#include "llstylemap.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llviewertexteditor.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llvoicechannel.h"

#include "boost/algorithm/string.hpp"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy startConferenceChatResponder_timeout;
extern AIHTTPTimeoutPolicy sessionInviteResponder_timeout;

//
// Constants
//
const S32 LINE_HEIGHT = 16;
const S32 MIN_WIDTH = 200;
const S32 MIN_HEIGHT = 130;

//
// Statics
//
//
static std::string sTitleString = "Instant Message with [NAME]";
static std::string sTypingStartString = "[NAME]: ...";
static std::string sSessionStartString = "Starting session with [NAME] please wait.";

void session_starter_helper(
	const LLUUID& temp_session_id,
	const LLUUID& other_participant_id,
	EInstantMessage im_type)
{
	LLMessageSystem *msg = gMessageSystem;

	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBOOLFast(_PREHASH_FromGroup, FALSE);
	msg->addUUIDFast(_PREHASH_ToAgentID, other_participant_id);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addU8Fast(_PREHASH_Dialog, im_type);
	msg->addUUIDFast(_PREHASH_ID, temp_session_id);
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // no timestamp necessary

	std::string name;
	gAgent.buildFullname(name);

	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, LLStringUtil::null);
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());
}

void start_deprecated_conference_chat(
	const LLUUID& temp_session_id,
	const LLUUID& creator_id,
	const LLUUID& other_participant_id,
	const LLSD& agents_to_invite)
{
	U8* bucket;
	U8* pos;
	S32 count;
	S32 bucket_size;

	// *FIX: this could suffer from endian issues
	count = agents_to_invite.size();
	bucket_size = UUID_BYTES * count;
	bucket = new U8[bucket_size];
	pos = bucket;

	for(S32 i = 0; i < count; ++i)
	{
		LLUUID agent_id = agents_to_invite[i].asUUID();
		
		memcpy(pos, &agent_id, UUID_BYTES);
		pos += UUID_BYTES;
	}

	session_starter_helper(
		temp_session_id,
		other_participant_id,
		IM_SESSION_CONFERENCE_START);

	gMessageSystem->addBinaryDataFast(
		_PREHASH_BinaryBucket,
		bucket,
		bucket_size);

	gAgent.sendReliableMessage();
 
	delete[] bucket;
}

class LLStartConferenceChatResponder : public LLHTTPClient::ResponderIgnoreBody
{
public:
	LLStartConferenceChatResponder(
		const LLUUID& temp_session_id,
		const LLUUID& creator_id,
		const LLUUID& other_participant_id,
		const LLSD& agents_to_invite)
	{
		mTempSessionID = temp_session_id;
		mCreatorID = creator_id;
		mOtherParticipantID = other_participant_id;
		mAgents = agents_to_invite;
	}

	/*virtual*/ void error(U32 statusNum, const std::string& reason)
	{
		//try an "old school" way.
		if ( statusNum == 400 )
		{
			start_deprecated_conference_chat(
				mTempSessionID,
				mCreatorID,
				mOtherParticipantID,
				mAgents);
		}

		//else throw an error back to the client?
		//in theory we should have just have these error strings
		//etc. set up in this file as opposed to the IMMgr,
		//but the error string were unneeded here previously
		//and it is not worth the effort switching over all
		//the possible different language translations
	}

	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return startConferenceChatResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "LLStartConferenceChatResponder"; }

private:
	LLUUID mTempSessionID;
	LLUUID mCreatorID;
	LLUUID mOtherParticipantID;

	LLSD mAgents;
};

// Returns true if any messages were sent, false otherwise.
// Is sort of equivalent to "does the server need to do anything?"
bool send_start_session_messages(
	const LLUUID& temp_session_id,
	const LLUUID& other_participant_id,
	const LLDynamicArray<LLUUID>& ids,
	EInstantMessage dialog)
{
	if ( dialog == IM_SESSION_GROUP_START )
	{
		session_starter_helper(
			temp_session_id,
			other_participant_id,
			dialog);

		gMessageSystem->addBinaryDataFast(
			_PREHASH_BinaryBucket,
			EMPTY_BINARY_BUCKET,
			EMPTY_BINARY_BUCKET_SIZE);
		gAgent.sendReliableMessage();

		return true;
	}
	else if ( dialog == IM_SESSION_CONFERENCE_START )
	{
		if (ids.empty()) return true;
		LLSD agents;
		for (int i = 0; i < (S32) ids.size(); i++)
		{
			agents.append(ids.get(i));
		}

		//we have a new way of starting conference calls now
		LLViewerRegion* region = gAgent.getRegion();
		std::string url(region ? region->getCapability("ChatSessionRequest") : "");
		if (!url.empty())
		{
			LLSD data;
			data["method"] = "start conference";
			data["session-id"] = temp_session_id;

			data["params"] = agents;

			LLHTTPClient::post(
				url,
				data,
				new LLStartConferenceChatResponder(
					temp_session_id,
					gAgent.getID(),
					other_participant_id,
					data["params"]));
		}
		else
		{
			start_deprecated_conference_chat(
				temp_session_id,
				gAgent.getID(),
				other_participant_id,
				agents);
		}
	}

	return false;
}


//
// LLFloaterIMPanel
//
LLFloaterIMPanel::LLFloaterIMPanel(
	const std::string& log_label,
	const LLUUID& session_id,
	const LLUUID& other_participant_id,
	const EInstantMessage& dialog,
	const LLDynamicArray<LLUUID>& ids) :
	LLFloater(log_label, LLRect(), log_label),
	mStartCallOnInitialize(false),
	mInputEditor(NULL),
	mHistoryEditor(NULL),
	mSessionInitialized(false),
	mSessionStartMsgPos(0),
	mSessionType(P2P_SESSION),
	mSessionUUID(session_id),
	mLogLabel(log_label),
	mQueuedMsgsForInit(),
	mOtherParticipantUUID(other_participant_id),
	mDialog(dialog),
	mTyping(false),
	mTypingLineStartIndex(0),
	mOtherTyping(false),
	mOtherTypingName(),
	mNumUnreadMessages(0),
	mSentTypingState(true),
	mShowSpeakersOnConnect(true),
	mDing(false),
	mRPMode(false),
	mTextIMPossible(true),
	mCallBackEnabled(true),
	mSpeakers(NULL),
	mSpeakerPanel(NULL),
	mVoiceChannel(NULL),
	mFirstKeystrokeTimer(),
	mLastKeystrokeTimer()
{
	if (mOtherParticipantUUID.isNull())
	{
		llwarns << "Other participant is NULL" << llendl;
	}

	// set P2P type by default
	static LLCachedControl<bool> concise_im("UseConciseIMButtons");
	std::string xml_filename = concise_im ? "floater_instant_message_concisebuttons.xml" : "floater_instant_message.xml";


	switch(mDialog)
	{
	case IM_SESSION_GROUP_START:
	case IM_SESSION_INVITE:
	case IM_SESSION_CONFERENCE_START:
		// determine whether it is group or conference session
		if (gAgent.isInGroup(mSessionUUID))
		{
			static LLCachedControl<bool> concise("UseConciseGroupChatButtons");
			xml_filename = concise ? "floater_instant_message_group_concisebuttons.xml" : "floater_instant_message_group.xml";
			mSessionType = GROUP_SESSION;
		}
		else
		{
			static LLCachedControl<bool> concise("UseConciseConferenceButtons");
			xml_filename = concise ? "floater_instant_message_ad_hoc_concisebuttons.xml" : "floater_instant_message_ad_hoc.xml";
			mSessionType = ADHOC_SESSION;
		}
		mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel, this);
		mVoiceChannel = new LLVoiceChannelGroup(mSessionUUID, mLogLabel);
		break;
	// just received text from another user
	case IM_NOTHING_SPECIAL:
		mTextIMPossible = LLVoiceClient::getInstance()->isSessionTextIMPossible(mSessionUUID);
		mCallBackEnabled = LLVoiceClient::getInstance()->isSessionCallBackPossible(mSessionUUID);
		// fallthrough
	case IM_SESSION_P2P_INVITE:
		mVoiceChannel = new LLVoiceChannelP2P(mSessionUUID, mLogLabel, mOtherParticipantUUID);
		LLAvatarTracker::instance().addParticularFriendObserver(mOtherParticipantUUID, this);
		break;
	default:
		llwarns << "Unknown session type" << llendl;
		break;
	}

	mSpeakers = new LLIMSpeakerMgr(mVoiceChannel);

	LLUICtrlFactory::getInstance()->buildFloater(this,
								xml_filename,
								&getFactoryMap(),
								FALSE);

	// enable line history support for instant message bar
	mInputEditor->setEnableLineHistory(TRUE);

	if ( gSavedPerAccountSettings.getBOOL("LogShowHistory") )
	{
		LLLogChat::loadHistory(mLogLabel,
				       &chatFromLogFile,
				       (void *)this);
	}

	if ( !mSessionInitialized )
	{
		if ( !send_start_session_messages(
				 mSessionUUID,
				 mOtherParticipantUUID,
				 ids,
				 mDialog) )
		{
			//we don't need to need to wait for any responses
			//so we're already initialized
			mSessionInitialized = true;
		}
		else
		{
			//locally echo a little "starting session" message
			LLUIString session_start = sSessionStartString;

			session_start.setArg("[NAME]", getTitle());
			mSessionStartMsgPos = mHistoryEditor->getWText().length();

			addHistoryLine(
				session_start,
				gSavedSettings.getColor4("SystemChatColor"),
				false);
		}
	}
}

void LLFloaterIMPanel::onAvatarNameLookup(const LLAvatarName& avatar_name)
{
	std::string title;
	LLAvatarNameCache::getPNSName(avatar_name, title);
	setTitle(title);
	// Singu Note: We could set tab name here, too now.
}

LLFloaterIMPanel::~LLFloaterIMPanel()
{
	LLAvatarTracker::instance().removeParticularFriendObserver(mOtherParticipantUUID, this);

	delete mSpeakers;
	mSpeakers = NULL;
	
	// End the text IM session if necessary
	if(LLVoiceClient::instanceExists() && mOtherParticipantUUID.notNull())
	{
		switch(mDialog)
		{
			case IM_NOTHING_SPECIAL:
			case IM_SESSION_P2P_INVITE:
				LLVoiceClient::getInstance()->endUserIMSession(mOtherParticipantUUID);
			break;
			
			default:
				// Appease the compiler
			break;
		}
	}
	
	//kicks you out of the voice channel if it is currently active

	// HAVE to do this here -- if it happens in the LLVoiceChannel destructor it will call the wrong version (since the object's partially deconstructed at that point).
	mVoiceChannel->deactivate();
	
	delete mVoiceChannel;
	mVoiceChannel = NULL;

	//delete focus lost callback
	mFocusLostSignal.disconnect();
}

// virtual
void LLFloaterIMPanel::changed(U32 mask)
{
	if (mask & REMOVE|ADD) // Fix remove/add friend choices
		rebuildDynamics(getChild<LLComboBox>("instant_message_flyout"));
	/* Singu TODO: Chat UI - Online icons?
	if (mask & ONLINE)
		// Show online icon here
	else if (mask & NONE)
		// Show offline icon here
	*/
}

// virtual
BOOL LLFloaterIMPanel::postBuild() 
{
	requires<LLLineEditor>("chat_editor");
	requires<LLTextEditor>("im_history");

	if (checkRequirements())
	{
		setTitle(mLogLabel);
		if (mSessionType == P2P_SESSION && LLVoiceClient::getInstance()->isParticipantAvatar(mSessionUUID))
			LLAvatarNameCache::get(mOtherParticipantUUID, boost::bind(&LLFloaterIMPanel::onAvatarNameLookup, this, _2));

		mInputEditor = getChild<LLLineEditor>("chat_editor");
		mInputEditor->setFocusReceivedCallback( boost::bind(&LLFloaterIMPanel::onInputEditorFocusReceived, this) );
		mFocusLostSignal = mInputEditor->setFocusLostCallback(boost::bind(&LLFloaterIMPanel::setTyping, this, false));
		mInputEditor->setKeystrokeCallback( boost::bind(&LLFloaterIMPanel::onInputEditorKeystroke, this, _1) );
		mInputEditor->setCommitCallback( boost::bind(&LLFloaterIMPanel::onSendMsg,this) );
		mInputEditor->setCommitOnFocusLost( FALSE );
		mInputEditor->setRevertOnEsc( FALSE );
		mInputEditor->setReplaceNewlinesWithSpaces( FALSE );
		mInputEditor->setPassDelete( TRUE );

		if (LLComboBox* flyout = findChild<LLComboBox>("instant_message_flyout"))
		{
			flyout->setCommitCallback(boost::bind(&LLFloaterIMPanel::onFlyoutCommit, this, flyout, _2));
			addDynamics(flyout);
		}
		if (LLUICtrl* ctrl = findChild<LLUICtrl>("tp_btn"))
			ctrl->setCommitCallback(boost::bind(static_cast<void(*)(const LLUUID&)>(LLAvatarActions::offerTeleport), mOtherParticipantUUID));
		if (LLUICtrl* ctrl = findChild<LLUICtrl>("pay_btn"))
			ctrl->setCommitCallback(boost::bind(LLAvatarActions::pay, mOtherParticipantUUID));
		if (LLButton* btn = findChild<LLButton>("group_info_btn"))
			btn->setCommitCallback(boost::bind(LLGroupActions::show, mSessionUUID));
		if (LLUICtrl* ctrl = findChild<LLUICtrl>("history_btn"))
			ctrl->setCommitCallback(boost::bind(&LLFloaterIMPanel::onClickHistory, this));

		getChild<LLButton>("start_call_btn")->setCommitCallback(boost::bind(&LLIMMgr::startCall, gIMMgr, mSessionUUID, LLVoiceChannel::OUTGOING_CALL));
		getChild<LLButton>("end_call_btn")->setCommitCallback(boost::bind(&LLIMMgr::endCall, gIMMgr, mSessionUUID));
		getChild<LLButton>("send_btn")->setCommitCallback(boost::bind(&LLFloaterIMPanel::onSendMsg,this));
		if (LLButton* btn = findChild<LLButton>("toggle_active_speakers_btn"))
			btn->setCommitCallback(boost::bind(&LLFloaterIMPanel::onClickToggleActiveSpeakers, this, _2));

		mHistoryEditor = getChild<LLViewerTextEditor>("im_history");
		mHistoryEditor->setParseHTML(TRUE);
		mHistoryEditor->setParseHighlights(TRUE);

		sTitleString = getString("title_string");
		sTypingStartString = getString("typing_start_string");
		sSessionStartString = getString("session_start_string");

		if (mSpeakerPanel)
		{
			mSpeakerPanel->refreshSpeakers();
		}

		if (mDialog == IM_NOTHING_SPECIAL)
		{
			getChild<LLUICtrl>("mute_btn")->setCommitCallback(boost::bind(&LLFloaterIMPanel::onClickMuteVoice, this));
			getChild<LLUICtrl>("speaker_volume")->setCommitCallback(boost::bind(&LLVoiceClient::setUserVolume, LLVoiceClient::getInstance(), mOtherParticipantUUID, _2));
		}

		setDefaultBtn("send_btn");

		mVolumeSlider.connect(this,"speaker_volume");
		mEndCallBtn.connect(this,"end_call_btn");
		mStartCallBtn.connect(this,"start_call_btn");
		mSendBtn.connect(this,"send_btn");
		mMuteBtn.connect(this,"mute_btn");

		return TRUE;
	}

	return FALSE;
}

void* LLFloaterIMPanel::createSpeakersPanel(void* data)
{
	LLFloaterIMPanel* floaterp = (LLFloaterIMPanel*)data;
	floaterp->mSpeakerPanel = new LLParticipantList(floaterp->mSpeakers, true);
	return floaterp->mSpeakerPanel;
}

void LLFloaterIMPanel::onClickMuteVoice()
{
	LLMute mute(mOtherParticipantUUID, getTitle(), LLMute::AGENT);
	if (!LLMuteList::getInstance()->isMuted(mOtherParticipantUUID, LLMute::flagVoiceChat))
	{
		LLMuteList::getInstance()->add(mute, LLMute::flagVoiceChat);
	}
	else
	{
		LLMuteList::getInstance()->remove(mute, LLMute::flagVoiceChat);
	}
}


// virtual
void LLFloaterIMPanel::draw()
{	
	LLViewerRegion* region = gAgent.getRegion();
	
	bool enable_connect = (region && !region->getCapability("ChatSessionRequest").empty())
					  && mSessionInitialized
					  && LLVoiceClient::getInstance()->voiceEnabled()
					  && mCallBackEnabled;

	// hide/show start call and end call buttons
	mEndCallBtn->setVisible(LLVoiceClient::getInstance()->voiceEnabled() && mVoiceChannel->getState() >= LLVoiceChannel::STATE_CALL_STARTED);
	mStartCallBtn->setVisible(LLVoiceClient::getInstance()->voiceEnabled() && mVoiceChannel->getState() < LLVoiceChannel::STATE_CALL_STARTED);
	mStartCallBtn->setEnabled(enable_connect);
	mSendBtn->setEnabled(!mInputEditor->getValue().asString().empty());
	
	LLPointer<LLSpeaker> self_speaker = mSpeakers->findSpeaker(gAgent.getID());
	if(!mTextIMPossible)
	{
		mInputEditor->setEnabled(FALSE);
		mInputEditor->setLabel(getString("unavailable_text_label"));
	}
	else if (self_speaker.notNull() && self_speaker->mModeratorMutedText)
	{
		mInputEditor->setEnabled(FALSE);
		mInputEditor->setLabel(getString("muted_text_label"));
	}
	else
	{
		mInputEditor->setEnabled(TRUE);
		mInputEditor->setLabel(getString("default_text_label"));
	}

	// show speakers window when voice first connects
	if (mShowSpeakersOnConnect && mVoiceChannel->isActive())
	{
		if (mSpeakerPanel) mSpeakerPanel->setVisible(true);
		mShowSpeakersOnConnect = false;
	}
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("toggle_active_speakers_btn"))
		ctrl->setValue(getChildView("active_speakers_panel")->getVisible());

	if (mTyping)
	{
		// Time out if user hasn't typed for a while.
		if (mLastKeystrokeTimer.getElapsedTimeF32() > LLAgent::TYPING_TIMEOUT_SECS)
		{
			setTyping(false);
		}

		// If we are typing, and it's been a little while, send the
		// typing indicator
		if (!mSentTypingState
			&& mFirstKeystrokeTimer.getElapsedTimeF32() > 1.f)
		{
			sendTypingState(true);
			mSentTypingState = true;
		}
	}

	// use embedded panel if available
	if (mSpeakerPanel)
	{
		if (mSpeakerPanel->getVisible())
		{
			mSpeakerPanel->refreshSpeakers();
		}
	}
	else
	{
		// refresh volume and mute checkbox
		mVolumeSlider->setVisible(LLVoiceClient::getInstance()->voiceEnabled() && mVoiceChannel->isActive());
		mVolumeSlider->setValue(LLVoiceClient::getInstance()->getUserVolume(mOtherParticipantUUID));

		mMuteBtn->setValue(LLMuteList::getInstance()->isMuted(mOtherParticipantUUID, LLMute::flagVoiceChat));
		mMuteBtn->setVisible(LLVoiceClient::getInstance()->voiceEnabled() && mVoiceChannel->isActive());
	}
	LLFloater::draw();
}

class LLSessionInviteResponder : public LLHTTPClient::ResponderIgnoreBody
{
public:
	LLSessionInviteResponder(const LLUUID& session_id)
	{
		mSessionID = session_id;
	}

	/*virtual*/ void error(U32 statusNum, const std::string& reason)
	{
		llwarns << "Error inviting all agents to session [status:"
				<< statusNum << "]: " << reason << llendl;
		//throw something back to the viewer here?
	}

	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return sessionInviteResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "LLSessionInviteResponder"; }

private:
	LLUUID mSessionID;
};

bool LLFloaterIMPanel::inviteToSession(const LLDynamicArray<LLUUID>& ids)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
	{
		return FALSE;
	}
	
	S32 count = ids.count();

	if( isInviteAllowed() && (count > 0) )
	{
		llinfos << "LLFloaterIMPanel::inviteToSession() - inviting participants" << llendl;

		std::string url = region->getCapability("ChatSessionRequest");

		LLSD data;

		data["params"] = LLSD::emptyArray();
		for (int i = 0; i < count; i++)
		{
			data["params"].append(ids.get(i));
		}

		data["method"] = "invite";
		data["session-id"] = mSessionUUID;
		LLHTTPClient::post(
			url,
			data,
			new LLSessionInviteResponder(
				mSessionUUID));		
	}
	else
	{
		llinfos << "LLFloaterIMPanel::inviteToSession -"
				<< " no need to invite agents for "
				<< mDialog << llendl;
		// successful add, because everyone that needed to get added
		// was added.
	}

	return TRUE;
}

void LLFloaterIMPanel::addHistoryLine(const std::string &utf8msg, LLColor4 incolor, bool log_to_file, const LLUUID& source, const std::string& name)
{
	static const LLCachedControl<bool> mKeywordsChangeColor(gSavedPerAccountSettings, "KeywordsChangeColor", false);
	static const LLCachedControl<LLColor4> mKeywordsColor(gSavedPerAccountSettings, "KeywordsColor", LLColor4(1.f, 1.f, 1.f, 1.f));

	if (gAgentID != source)
	{
		if (mKeywordsChangeColor)
		{
			if (AscentKeyword::hasKeyword(utf8msg, 2))
			{
				incolor = mKeywordsColor;
			}
		}

		if (mDing && (!hasFocus() || !gFocusMgr.getAppHasFocus()))
		{
			static const LLCachedControl<std::string> ding("LiruNewMessageSound");
			static const LLCachedControl<std::string> dong("LiruNewMessageSoundForSystemMessages");
			LLUI::sAudioCallback(LLUUID(source.notNull() ? ding : dong));
		}
	}

	const LLColor4& color = incolor;
	// start tab flashing when receiving im for background session from user
	if (source.notNull())
	{
		LLMultiFloater* hostp = getHost();
		if( !isInVisibleChain() 
			&& hostp 
			&& source != gAgentID)
		{
			hostp->setFloaterFlashing(this, TRUE);
		}
	}

	// Now we're adding the actual line of text, so erase the 
	// "Foo is typing..." text segment, and the optional timestamp
	// if it was present. JC
	removeTypingIndicator(NULL);

	// Actually add the line
	bool prepend_newline = true;
	if (gSavedSettings.getBOOL("IMShowTimestamps"))
	{
		mHistoryEditor->appendTime(prepend_newline);
		prepend_newline = false;
	}

	std::string show_name = name;
	bool is_irc = false;
	// 'name' is a sender name that we want to hotlink so that clicking on it opens a profile.
	if (!name.empty()) // If name exists, then add it to the front of the message.
	{
		// Don't hotlink any messages from the system (e.g. "Second Life:"), so just add those in plain text.
		if (name == SYSTEM_FROM)
		{
			mHistoryEditor->appendColoredText(name,false,prepend_newline,color);
		}
		else
		{
			// IRC style text starts with a colon here; empty names and system messages aren't irc style.
			static const LLCachedControl<bool> italicize("LiruItalicizeActions");
			is_irc = italicize && utf8msg[0] != ':';
			if (source.notNull())
				LLAvatarNameCache::getPNSName(source, show_name);
			// Convert the name to a hotlink and add to message.
			LLStyleSP source_style = LLStyleMap::instance().lookupAgent(source);
			source_style->mItalic = is_irc;
			mHistoryEditor->appendStyledText(show_name,false,prepend_newline,source_style);
		}
		prepend_newline = false;
	}

	// Append the chat message in style
	{
		LLStyleSP style(new LLStyle);
		style->setColor(color);
		style->mItalic = is_irc;
		style->mBold = gSavedSettings.getBOOL("SingularityBoldGroupModerator") && isModerator(source);
		mHistoryEditor->appendStyledText(utf8msg, false, prepend_newline, style);
	}

	if (log_to_file
		&& gSavedPerAccountSettings.getBOOL("LogInstantMessages") ) 
	{
		std::string histstr;
		if (gSavedPerAccountSettings.getBOOL("IMLogTimestamp"))
			histstr = LLLogChat::timestamp(gSavedPerAccountSettings.getBOOL("LogTimestampDate")) + show_name + utf8msg;
		else
			histstr = show_name + utf8msg;

		// [Ansariel: Display name support]
		// Floater title contains display name -> bad idea to use that as filename
		// mLogLabel, however, is the old legacy name
		//LLLogChat::saveHistory(getTitle(),histstr);
		LLLogChat::saveHistory(mLogLabel, histstr);
		// [/Ansariel: Display name support]
	}

	if (source.notNull())
	{
		if (!isInVisibleChain() || (!hasFocus() && getParent() == gFloaterView))
		{
			mNumUnreadMessages++;
		}

		mSpeakers->speakerChatted(source);
		mSpeakers->setSpeakerTyping(source, FALSE);
	}
}


void LLFloaterIMPanel::setVisible(BOOL b)
{
	LLPanel::setVisible(b);

	LLMultiFloater* hostp = getHost();
	if( b && hostp )
	{
		hostp->setFloaterFlashing(this, FALSE);
	}
}


void LLFloaterIMPanel::setInputFocus(bool b)
{
	mInputEditor->setFocus( b );
}

BOOL LLFloaterIMPanel::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;
	if (KEY_RETURN == key)
	{
		onSendMsg();
		handled = TRUE;

		// Close talk panels on hitting return
		// without holding a modifier key
		if (mask == MASK_NONE)
			closeIfNotPinned();
	}
	else if (KEY_ESCAPE == key && mask == MASK_NONE)
	{
		handled = TRUE;
		gFocusMgr.setKeyboardFocus(NULL);

		// Close talk panel with escape
		closeIfNotPinned();
	}

	// May need to call base class LLPanel::handleKeyHere if not handled
	// in order to tab between buttons.  JNC 1.2.2002
	return handled;
}

void LLFloaterIMPanel::closeIfNotPinned()
{
	if (gSavedSettings.getBOOL("PinTalkViewOpen")) return;

	if (getParent() == gFloaterView) // Just minimize, if popped out
		setMinimized(true);
	else
		gIMMgr->toggle();
}

BOOL LLFloaterIMPanel::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								  EDragAndDropType cargo_type,
								  void* cargo_data,
								  EAcceptance* accept,
								  std::string& tooltip_msg)
{

	if (mDialog == IM_NOTHING_SPECIAL)
	{
		LLToolDragAndDrop::handleGiveDragAndDrop(mOtherParticipantUUID, mSessionUUID, drop,
												 cargo_type, cargo_data, accept);
	}
	
	// handle case for dropping calling cards (and folders of calling cards) onto invitation panel for invites
	else if (isInviteAllowed())
	{
		*accept = ACCEPT_NO;
		
		if (cargo_type == DAD_CALLINGCARD)
		{
			if (dropCallingCard((LLInventoryItem*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
		else if (cargo_type == DAD_CATEGORY)
		{
			if (dropCategory((LLInventoryCategory*)cargo_data, drop))
			{
				*accept = ACCEPT_YES_MULTI;
			}
		}
	}
	return TRUE;
} 

BOOL LLFloaterIMPanel::dropCallingCard(LLInventoryItem* item, BOOL drop)
{
	BOOL rv = isInviteAllowed();
	if(rv && item && item->getCreatorUUID().notNull())
	{
		if(drop)
		{
			LLDynamicArray<LLUUID> ids;
			ids.put(item->getCreatorUUID());
			inviteToSession(ids);
		}
	}
	else
	{
		// set to false if creator uuid is null.
		rv = FALSE;
	}
	return rv;
}

BOOL LLFloaterIMPanel::dropCategory(LLInventoryCategory* category, BOOL drop)
{
	BOOL rv = isInviteAllowed();
	if(rv && category)
	{
		LLInventoryModel::cat_array_t cats;
		LLInventoryModel::item_array_t items;
		LLUniqueBuddyCollector buddies;
		gInventory.collectDescendentsIf(category->getUUID(),
										cats,
										items,
										LLInventoryModel::EXCLUDE_TRASH,
										buddies);
		S32 count = items.count();
		if(count == 0)
		{
			rv = FALSE;
		}
		else if(drop)
		{
			LLDynamicArray<LLUUID> ids;
			for(S32 i = 0; i < count; ++i)
			{
				ids.put(items.get(i)->getCreatorUUID());
			}
			inviteToSession(ids);
		}
	}
	return rv;
}

bool LLFloaterIMPanel::isInviteAllowed() const
{

	return ( (IM_SESSION_CONFERENCE_START == mDialog) 
			 || (IM_SESSION_INVITE == mDialog) );
}

void LLFloaterIMPanel::removeDynamics(LLComboBox* flyout)
{
	flyout->remove(mDing ? getString("ding on") : getString("ding off"));
	flyout->remove(mRPMode ? getString("rp mode on") : getString("rp mode off"));
	flyout->remove(LLAvatarActions::isFriend(mOtherParticipantUUID) ? getString("remove friend") : getString("add friend"));
	//flyout->remove(LLAvatarActions::isBlocked(mOtherParticipantUUID) ? getString("unmute") : getString("mute"));
}

void LLFloaterIMPanel::addDynamics(LLComboBox* flyout)
{
	flyout->add(mDing ? getString("ding on") : getString("ding off"), 6);
	flyout->add(mRPMode ? getString("rp mode on") : getString("rp mode off"), 7);
	flyout->add(LLAvatarActions::isFriend(mOtherParticipantUUID) ? getString("remove friend") : getString("add friend"), 8);
	//flyout->add(LLAvatarActions::isBlocked(mOtherParticipantUUID) ? getString("unmute") : getString("mute"), 9);
}

void LLFloaterIMPanel::onFlyoutCommit(LLComboBox* flyout, const LLSD& value)
{
	if (value.isUndefined())
	{
		LLAvatarActions::showProfile(mOtherParticipantUUID);
		return;
	}

	int option = value.asInteger();
	if (option == 1) onClickHistory();
	else if (option == 2) LLAvatarActions::offerTeleport(mOtherParticipantUUID);
	else if (option == 3) LLAvatarActions::teleportRequest(mOtherParticipantUUID);
	else if (option == 4) LLAvatarActions::pay(mOtherParticipantUUID);
	else if (option == 5) LLAvatarActions::inviteToGroup(mOtherParticipantUUID);
	else if (option >= 6) // Options that use dynamic items
	{
		// First remove them all
		removeDynamics(flyout);

		// Toggle as requested, adjust the strings
		if (option == 6) mDing = !mDing;
		else if (option == 7) mRPMode = !mRPMode;
		else if (option == 8) LLAvatarActions::isFriend(mOtherParticipantUUID) ? LLAvatarActions::removeFriendDialog(mOtherParticipantUUID) : LLAvatarActions::requestFriendshipDialog(mOtherParticipantUUID);
		//else if (option == 9) LLAvatarActions::toggleBlock(mOtherParticipantUUID);

		// Last add them back
		addDynamics(flyout);
	}
}

void show_log_browser(const std::string& name, const std::string& id)
{
	if (gSavedSettings.getBOOL("LiruLegacyLogLaunch"))
	{
		gViewerWindow->getWindow()->ShellEx("\"" + LLLogChat::makeLogFileName(name) + "\"");
		return;
	}
	LLFloaterWebContent::Params p;
	p.url("file:///" + LLLogChat::makeLogFileName(name));
	p.id(id);
	p.show_chrome(false);
	p.trusted_content(true);
	LLFloaterWebContent::showInstance("log", p); // If we passed id instead of "log", there would be no control over how many log browsers opened at once.
}

void LLFloaterIMPanel::onClickHistory()
{
	if (mOtherParticipantUUID.notNull())
	{
		// [Ansariel: Display name support]
		//show_log_browser(getTitle(), mOtherParticipantUUID.asString());
		show_log_browser(mLogLabel, mOtherParticipantUUID.asString());
		// [/Ansariel: Display name support]
	}
}

void LLFloaterIMPanel::onClickToggleActiveSpeakers(const LLSD& value)
{
	childSetVisible("active_speakers_panel", !value);
}

void LLFloaterIMPanel::onInputEditorFocusReceived()
{
	mHistoryEditor->setCursorAndScrollToEnd();
}

void LLFloaterIMPanel::onInputEditorKeystroke(LLLineEditor* caller)
{
	// Deleting all text counts as stopping typing.
	setTyping(!caller->getText().empty());
}

void LLFloaterIMPanel::onClose(bool app_quitting)
{
	setTyping(false);

	if(mSessionUUID.notNull())
	{
		std::string name;
		gAgent.buildFullname(name);
		pack_instant_message(
			gMessageSystem,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			mOtherParticipantUUID,
			name, 
			LLStringUtil::null,
			IM_ONLINE,
			IM_SESSION_LEAVE,
			mSessionUUID);
		gAgent.sendReliableMessage();
	}
	gIMMgr->removeSession(mSessionUUID);

	destroy();
}

void LLFloaterIMPanel::handleVisibilityChange(BOOL new_visibility)
{
	if (new_visibility)
	{
		mNumUnreadMessages = 0;
	}
}

void deliver_message(const std::string& utf8_text,
					 const LLUUID& im_session_id,
					 const LLUUID& other_participant_id,
					 EInstantMessage dialog)
{
	std::string name;
	bool sent = false;
	gAgent.buildFullname(name);

	const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(other_participant_id);

	U8 offline = (!info || info->isOnline()) ? IM_ONLINE : IM_OFFLINE;

	if((offline == IM_OFFLINE) && (LLVoiceClient::getInstance()->isOnlineSIP(other_participant_id)))
	{
		// User is online through the OOW connector, but not with a regular viewer.  Try to send the message via SLVoice.
		sent = LLVoiceClient::getInstance()->sendTextMessage(other_participant_id, utf8_text);
	}

	if(!sent)
	{
		// Send message normally.

		// default to IM_SESSION_SEND unless it's nothing special - in
		// which case it's probably an IM to everyone.
		U8 new_dialog = dialog;

		if ( dialog != IM_NOTHING_SPECIAL )
		{
			new_dialog = IM_SESSION_SEND;
		}
		pack_instant_message(
			gMessageSystem,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			other_participant_id,
			name.c_str(),
			utf8_text.c_str(),
			offline,
			(EInstantMessage)new_dialog,
			im_session_id);
		gAgent.sendReliableMessage();
	}

	// If there is a mute list and this is not a group chat...
	if ( LLMuteList::getInstance() )
	{
		// ... the target should not be in our mute list for some message types.
		// Auto-remove them if present.
		switch( dialog )
		{
		case IM_NOTHING_SPECIAL:
		case IM_GROUP_INVITATION:
		case IM_INVENTORY_OFFERED:
		case IM_SESSION_INVITE:
		case IM_SESSION_P2P_INVITE:
		case IM_SESSION_CONFERENCE_START:
		case IM_SESSION_SEND: // This one is marginal - erring on the side of hearing.
		case IM_LURE_USER:
		case IM_GODLIKE_LURE_USER:
		case IM_FRIENDSHIP_OFFERED:
			LLMuteList::getInstance()->autoRemove(other_participant_id, LLMuteList::AR_IM);
			break;
		default: ; // do nothing
		}
	}
}

bool convert_roleplay_text(std::string& text); // Returns true if text is an action

void LLFloaterIMPanel::onSendMsg()
{
	if (!gAgent.isGodlike() 
		&& (mDialog == IM_NOTHING_SPECIAL)
		&& mOtherParticipantUUID.isNull())
	{
		llinfos << "Cannot send IM to everyone unless you're a god." << llendl;
		return;
	}

	if (mInputEditor)
	{
		LLWString text = mInputEditor->getConvertedText();
		if(!text.empty())
		{
			// store sent line in history, duplicates will get filtered
			if (mInputEditor) mInputEditor->updateHistory();
			// Truncate and convert to UTF8 for transport
			std::string utf8_text = wstring_to_utf8str(text);
			bool action = convert_roleplay_text(utf8_text);
			if (!action && mRPMode)
				utf8_text = "((" + utf8_text + "))";
// [RLVa:KB] - Checked: 2011-09-17 (RLVa-1.1.4b) | Modified: RLVa-1.1.4b
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SENDIMTO)) )
			{
				bool fRlvFilter = false;
				switch (mSessionType)
				{
					case P2P_SESSION:	// One-on-one IM
						fRlvFilter = !gRlvHandler.canSendIM(mOtherParticipantUUID);
						break;
					case GROUP_SESSION:	// Group chat
						fRlvFilter = !gRlvHandler.canSendIM(mSessionUUID);
						break;
					case ADHOC_SESSION:	// Conference chat: allow if all participants can be sent an IM
						{
							if (!mSpeakers)
							{
								fRlvFilter = true;
								break;
							}

							LLSpeakerMgr::speaker_list_t speakers;
							mSpeakers->getSpeakerList(&speakers, TRUE);
							for (LLSpeakerMgr::speaker_list_t::const_iterator itSpeaker = speakers.begin(); 
									itSpeaker != speakers.end(); ++itSpeaker)
							{
								const LLSpeaker* pSpeaker = *itSpeaker;
								if ( (gAgentID != pSpeaker->mID) && (!gRlvHandler.canSendIM(pSpeaker->mID)) )
								{
									fRlvFilter = true;
									break;
								}
							}
						}
						break;
					default:
						fRlvFilter = true;
						break;
				}

				if (fRlvFilter)
					utf8_text = RlvStrings::getString(RLV_STRING_BLOCKED_SENDIM);
			}
// [/RLVa:KB]

			if ( mSessionInitialized )
			{
				// Split messages that are too long, same code like in llimpanel.cpp
				U32 split = MAX_MSG_BUF_SIZE - 1;
				U32 pos = 0;
				U32 total = utf8_text.length();

				while (pos < total)
				{
					U32 next_split = split;

					if (pos + next_split > total)
					{
						next_split = total - pos;
					}
					else
					{
						// don't split utf-8 bytes
						while (U8(utf8_text[pos + next_split]) != 0x20	// space
							&& U8(utf8_text[pos + next_split]) != 0x21	// !
							&& U8(utf8_text[pos + next_split]) != 0x2C	// ,
							&& U8(utf8_text[pos + next_split]) != 0x2E	// .
							&& U8(utf8_text[pos + next_split]) != 0x3F	// ?
							&& next_split > 0)
						{
							--next_split;
						}

						if (next_split == 0)
						{
							next_split = split;
							LL_WARNS("Splitting") << "utf-8 couldn't be split correctly" << LL_ENDL;
						}
						else
						{
							++next_split;
						}
					}

					std::string send = utf8_text.substr(pos, next_split);
					pos += next_split;
					LL_DEBUGS("Splitting") << "Pos: " << pos << " next_split: " << next_split << LL_ENDL;

					deliver_message(send,
									mSessionUUID,
									mOtherParticipantUUID,
									mDialog);
				}

				// local echo
				if((mDialog == IM_NOTHING_SPECIAL) && 
				   (mOtherParticipantUUID.notNull()))
				{
					std::string name;
					gAgent.buildFullname(name);

					// Look for actions here.
					if (action)
					{
						utf8_text.replace(0,3,"");
					}
					else
					{
						utf8_text.insert(0, ": ");
					}

					bool other_was_typing = mOtherTyping;
					addHistoryLine(utf8_text, gSavedSettings.getColor("UserChatColor"), true, gAgentID, name);
					if (other_was_typing) addTypingIndicator(mOtherTypingName);
				}
			}
			else
			{
				//queue up the message to send once the session is
				//initialized
				mQueuedMsgsForInit.append(utf8_text);
			}
		}

		LLViewerStats::getInstance()->incStat(LLViewerStats::ST_IM_COUNT);

		mInputEditor->setText(LLStringUtil::null);
	}

	// Don't need to actually send the typing stop message, the other
	// client will infer it from receiving the message.
	mTyping = false;
	mSentTypingState = true;
}

void LLFloaterIMPanel::processSessionUpdate(const LLSD& session_update)
{
	if (
		session_update.has("moderated_mode") &&
		session_update["moderated_mode"].has("voice") )
	{
		bool voice_moderated = session_update["moderated_mode"]["voice"];

		if (voice_moderated)
		{
			setTitle(mLogLabel + std::string(" ") + getString("moderated_chat_label"));
		}
		else
		{
			setTitle(mLogLabel);
		}


		//update the speakers dropdown too
		mSpeakerPanel->setVoiceModerationCtrlMode(session_update);
	}
}

void LLFloaterIMPanel::sessionInitReplyReceived(const LLUUID& session_id)
{
	mSessionUUID = session_id;
	mVoiceChannel->updateSessionID(session_id);
	mSessionInitialized = true;

	//we assume the history editor hasn't moved at all since
	//we added the starting session message
	//so, we count how many characters to remove
	S32 chars_to_remove = mHistoryEditor->getWText().length() - mSessionStartMsgPos;
	mHistoryEditor->removeTextFromEnd(chars_to_remove);

	//and now, send the queued msg
	for (LLSD::array_iterator iter = mQueuedMsgsForInit.beginArray();
		  iter != mQueuedMsgsForInit.endArray();
		  ++iter)
	{
		deliver_message(
			iter->asString(),
			mSessionUUID,
			mOtherParticipantUUID,
			mDialog);
	}

	// auto-start the call on session initialization?
	if (mStartCallOnInitialize)
	{
		gIMMgr->startCall(mSessionUUID);
	}
}

void LLFloaterIMPanel::setTyping(bool typing)
{
	if (typing)
	{
		// Every time you type something, reset this timer
		mLastKeystrokeTimer.reset();

		if (!mTyping)
		{
			// You just started typing.
			mFirstKeystrokeTimer.reset();

			// Will send typing state after a short delay.
			mSentTypingState = false;
		}

		mSpeakers->setSpeakerTyping(gAgent.getID(), TRUE);
	}
	else
	{
		if (mTyping)
		{
			// you just stopped typing, send state immediately
			sendTypingState(false);
			mSentTypingState = true;
		}
		mSpeakers->setSpeakerTyping(gAgent.getID(), FALSE);
	}

	mTyping = typing;
}

void LLFloaterIMPanel::sendTypingState(bool typing)
{
	if(gSavedSettings.getBOOL("AscentHideTypingNotification"))
		return;
	// Don't want to send typing indicators to multiple people, potentially too
	// much network traffic.  Only send in person-to-person IMs.
	if (mDialog != IM_NOTHING_SPECIAL) return;

	std::string name;
	gAgent.buildFullname(name);

	pack_instant_message(
		gMessageSystem,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		mOtherParticipantUUID,
		name,
		std::string("typing"),
		IM_ONLINE,
		(typing ? IM_TYPING_START : IM_TYPING_STOP),
		mSessionUUID);
	gAgent.sendReliableMessage();
}


void LLFloaterIMPanel::processIMTyping(const LLIMInfo* im_info, bool typing)
{
	if (typing)
	{
		// other user started typing
		std::string name;
		if (!LLAvatarNameCache::getPNSName(im_info->mFromID, name)) name = im_info->mName;
		addTypingIndicator(name);
	}
	else
	{
		// other user stopped typing
		removeTypingIndicator(im_info);
	}
}


void LLFloaterIMPanel::addTypingIndicator(const std::string &name)
{
	// we may have lost a "stop-typing" packet, don't add it twice
	if (!mOtherTyping)
	{
		mTypingLineStartIndex = mHistoryEditor->getWText().length();
		LLUIString typing_start = sTypingStartString;
		typing_start.setArg("[NAME]", name);
		addHistoryLine(typing_start, gSavedSettings.getColor4("SystemChatColor"), false);
		mOtherTypingName = name;
		mOtherTyping = true;
	}
	// MBW -- XXX -- merge from release broke this (argument to this function changed from an LLIMInfo to a name)
	// Richard will fix.
//	mSpeakers->setSpeakerTyping(im_info->mFromID, TRUE);
}


void LLFloaterIMPanel::removeTypingIndicator(const LLIMInfo* im_info)
{
	if (mOtherTyping)
	{
		// Must do this first, otherwise addHistoryLine calls us again.
		mOtherTyping = false;

		S32 chars_to_remove = mHistoryEditor->getWText().length() - mTypingLineStartIndex;
		mHistoryEditor->removeTextFromEnd(chars_to_remove);
		if (im_info)
		{
			mSpeakers->setSpeakerTyping(im_info->mFromID, FALSE);
		}
	}
}

//static
void LLFloaterIMPanel::chatFromLogFile(LLLogChat::ELogLineType type, std::string line, void* userdata)
{
	LLFloaterIMPanel* self = (LLFloaterIMPanel*)userdata;
	std::string message = line;

	switch (type)
	{
	case LLLogChat::LOG_EMPTY:
		// add warning log enabled message
		if (gSavedPerAccountSettings.getBOOL("LogInstantMessages"))
		{
			message = LLFloaterChat::getInstance()->getString("IM_logging_string");
		}
		break;
	case LLLogChat::LOG_END:
		// add log end message
		if (gSavedPerAccountSettings.getBOOL("LogInstantMessages"))
		{
			message = LLFloaterChat::getInstance()->getString("IM_end_log_string");
		}
		break;
	case LLLogChat::LOG_LINE:
		// just add normal lines from file
		break;
	default:
		// nothing
		break;
	}

	//self->addHistoryLine(line, LLColor4::grey, FALSE);
	self->mHistoryEditor->appendColoredText(message, false, true, LLColor4::grey);
}

void LLFloaterIMPanel::showSessionStartError(
	const std::string& error_string)
{
	LLSD args;
	args["REASON"] = LLTrans::getString(error_string);
	args["RECIPIENT"] = getTitle();

	LLSD payload;
	payload["session_id"] = mSessionUUID;

	LLNotifications::instance().add(
		"ChatterBoxSessionStartError",
		args,
		payload,
		onConfirmForceCloseError);
}

void LLFloaterIMPanel::showSessionEventError(
	const std::string& event_string,
	const std::string& error_string)
{
	LLSD args;
	LLStringUtil::format_map_t event_args;

	event_args["RECIPIENT"] = getTitle();

	args["REASON"] =
		LLTrans::getString(error_string);
	args["EVENT"] =
		LLTrans::getString(event_string, event_args);

	LLNotifications::instance().add(
		"ChatterBoxSessionEventError",
		args);
}

void LLFloaterIMPanel::showSessionForceClose(
	const std::string& reason_string)
{
	LLSD args;

	args["NAME"] = getTitle();
	args["REASON"] = LLTrans::getString(reason_string);

	LLSD payload;
	payload["session_id"] = mSessionUUID;

	LLNotifications::instance().add(
		"ForceCloseChatterBoxSession",
		args,
		payload,
		LLFloaterIMPanel::onConfirmForceCloseError);

}

bool LLFloaterIMPanel::onConfirmForceCloseError(const LLSD& notification, const LLSD& response)
{
	//only 1 option really
	LLUUID session_id = notification["payload"]["session_id"];

	if (gIMMgr)
	{
		LLFloaterIMPanel* floaterp = gIMMgr->findFloaterBySession(session_id);

		if (floaterp) floaterp->close(FALSE);
	}
	return false;
}


//Kadah
const bool LLFloaterIMPanel::isModerator(const LLUUID& speaker_id)
{
	if (mSpeakers)
	{
		LLPointer<LLSpeaker> speakerp = mSpeakers->findSpeaker(speaker_id);
		return speakerp && speakerp->mIsModerator;
	}
	return false;
}

BOOL LLFloaterIMPanel::focusFirstItem(BOOL prefer_text_fields, BOOL focus_flash )
{
	if (getVisible() && mInputEditor->getVisible())
	{
		setInputFocus(true);
		return TRUE;
	}

	return LLUICtrl::focusFirstItem(prefer_text_fields, focus_flash);
}

void LLFloaterIMPanel::onFocusReceived()
{
	mNumUnreadMessages = 0;
	if (getVisible() && mInputEditor->getVisible())
	{
		setInputFocus(true);
	}

	LLFloater::onFocusReceived();
}
