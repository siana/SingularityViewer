/**
 * @file llparticipantlist.h
 * @brief LLParticipantList intended to update view(LLAvatarList) according to incoming messages
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

#ifndef LL_PARTICIPANTLIST_H
#define LL_PARTICIPANTLIST_H

#include "lllayoutstack.h"

class LLSpeakerMgr;
class LLScrollListCtrl;
class LLUICtrl;

class LLParticipantList : public LLLayoutPanel
{
	LOG_CLASS(LLParticipantList);
public:

	typedef boost::function<bool (const LLUUID& speaker_id)> validate_speaker_callback_t;

	LLParticipantList(LLSpeakerMgr* data_source, bool show_text_chatters);
	/*virtual*/ BOOL postBuild();
	~LLParticipantList();

	/**
	 * Adds specified avatar ID to the existing list if it is not Agent's ID
	 *
	 * @param[in] avatar_id - Avatar UUID to be added into the list
	 */
	void addAvatarIDExceptAgent(const LLUUID& avatar_id);

	/**
	 * Refreshes the participant list.
	 */
	void refreshSpeakers();

	/**
	 * Set a callback to be called before adding a speaker. Invalid speakers will not be added.
	 *
	 * If the callback is unset all speakers are considered as valid.
	 *
	 * @see onAddItemEvent()
	 */
	void setValidateSpeakerCallback(validate_speaker_callback_t cb);

	void setVoiceModerationCtrlMode(const bool& moderated_voice);

protected:
	/**
	 * LLSpeakerMgr event handlers
	 */
	bool onAddItemEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	bool onRemoveItemEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	bool onClearListEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	//bool onModeratorUpdateEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	bool onSpeakerMuteEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);

	/**
	 * List of listeners implementing LLOldEvents::LLSimpleListener.
	 * There is no way to handle all the events in one listener as LLSpeakerMgr registers
	 * listeners in such a way that one listener can handle only one type of event
	 **/
	class BaseSpeakerListener : public LLOldEvents::LLSimpleListener
	{
	public:
		BaseSpeakerListener(LLParticipantList& parent) : mParent(parent) {}
	protected:
		LLParticipantList& mParent;
	};

	class SpeakerAddListener : public BaseSpeakerListener
	{
	public:
		SpeakerAddListener(LLParticipantList& parent) : BaseSpeakerListener(parent) {}
		/*virtual*/ bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	};

	class SpeakerRemoveListener : public BaseSpeakerListener
	{
	public:
		SpeakerRemoveListener(LLParticipantList& parent) : BaseSpeakerListener(parent) {}
		/*virtual*/ bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	};

	class SpeakerClearListener : public BaseSpeakerListener
	{
	public:
		SpeakerClearListener(LLParticipantList& parent) : BaseSpeakerListener(parent) {}
		/*virtual*/ bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	};

	/*class SpeakerModeratorUpdateListener : public BaseSpeakerListener
	{
	public:
		SpeakerModeratorUpdateListener(LLParticipantList& parent) : BaseSpeakerListener(parent) {}
		*virtual* bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	};*/

	class SpeakerMuteListener : public BaseSpeakerListener
	{
	public:
		SpeakerMuteListener(LLParticipantList& parent) : BaseSpeakerListener(parent) {}

		/*virtual*/ bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	};

	/**
	 * Menu used in the participant list.
	class LLParticipantListMenu : public LLListContextMenu
	{
	 */
	private:
		void toggleAllowTextChat(const LLSD& userdata);
		void toggleMute(const LLSD& userdata, U32 flags);
		void toggleMuteText();
		void toggleMuteVoice();

		/**
		 * Processes Voice moderation menu items.
		 *
		 * It calls either moderateVoiceParticipant() or moderateVoiceParticipant() depend on
		 * passed parameter.
		 *
		 * @param userdata can be "selected" or "others".
		 *
		 * @see moderateVoiceParticipant()
		 * @see moderateVoiceAllParticipants()
		 */
		void moderateVoice(const LLSD& userdata);

		/**
		 * Mutes/Unmutes avatar for current group voice chat.
		 *
		 * It only marks avatar as muted for session and does not use local Agent's Block list.
		 * It does not mute Agent itself.
		 *
		 * @param[in] avatar_id UUID of avatar to be processed
		 * @param[in] unmute if true - specified avatar will be muted, otherwise - unmuted.
		 *
		 * @see moderateVoiceAllParticipants()
		 */
		void moderateVoiceParticipant(const LLSD& param);

		/**
		 * Mutes/Unmutes all avatars for current group voice chat.
		 *
		 * It only marks avatars as muted for session and does not use local Agent's Block list.
		 *
		 * @param[in] unmute if true - avatars will be muted, otherwise - unmuted.
		 *
		 * @see moderateVoiceParticipant()
		 */
		void moderateVoiceAllParticipants(const LLSD& param);

		//static void confirmMuteAllCallback(const LLSD& notification, const LLSD& response);
	//};

private:
	void onAvatarListDoubleClicked();

	/**
	 * Adjusts passed participant to work properly.
	 *
	 * Adds SpeakerMuteListener to process moderation actions.
	 */
	void adjustParticipant(const LLUUID& speaker_id);

	// Singu Note: The following callbacks are not found upstream
	void handleSpeakerSelect();
	void onClickProfile();
	void onSortChanged();
	void onVolumeChange(const LLSD& param);

	LLSpeakerMgr*		mSpeakerMgr;
	LLScrollListCtrl*	mAvatarList;
	bool				mShowTextChatters;

	LLPointer<SpeakerAddListener>				mSpeakerAddListener;
	LLPointer<SpeakerRemoveListener>			mSpeakerRemoveListener;
	LLPointer<SpeakerClearListener>				mSpeakerClearListener;
	//LLPointer<SpeakerModeratorUpdateListener>	mSpeakerModeratorListener;
	LLPointer<SpeakerMuteListener>				mSpeakerMuteListener;

	validate_speaker_callback_t mValidateSpeakerCallback;
};

#endif // LL_PARTICIPANTLIST_H

