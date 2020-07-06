/**
 * @file llspeakers.h
 * @brief Management interface for muting and controlling volume of residents currently speaking
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
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

#ifndef LL_LLSPEAKERS_H
#define LL_LLSPEAKERS_H

#include "llevent.h"
#include "lleventtimer.h"
#include "llhandle.h"

class LLAvatarName;
class LLSpeakerMgr;
class LLVoiceChannel;

// data for a given participant in a voice channel
class LLSpeaker final : public LLRefCount, public LLOldEvents::LLObservable, public LLHandleProvider<LLSpeaker>, public boost::signals2::trackable
{
public:
	typedef enum e_speaker_type
	{
		SPEAKER_AGENT,
		SPEAKER_OBJECT,
		SPEAKER_EXTERNAL	// Speaker that doesn't map to an avatar or object (i.e. PSTN caller in a group)
	} ESpeakerType;

	typedef enum e_speaker_status
	{
		STATUS_SPEAKING,
		STATUS_HAS_SPOKEN,
		STATUS_VOICE_ACTIVE,
		STATUS_TEXT_ONLY,
		STATUS_NOT_IN_CHANNEL,
		STATUS_MUTED
	} ESpeakerStatus;

	struct speaker_entry_t
	{
		speaker_entry_t(const LLUUID& id,
			LLSpeaker::ESpeakerType type = ESpeakerType::SPEAKER_AGENT,
			LLSpeaker::ESpeakerStatus status = ESpeakerStatus::STATUS_TEXT_ONLY,
			const boost::optional<bool> moderator = boost::none,
			const boost::optional<bool> moderator_muted_text = boost::none,
			std::string name = std::string()) :
				id(id),
				type(type),
				status(status),
				moderator(moderator),
				moderator_muted_text(moderator_muted_text),
				name(name)
		{}
		const LLUUID id;
		const LLSpeaker::ESpeakerType type;
		const LLSpeaker::ESpeakerStatus status;
		const boost::optional<bool> moderator;
		const boost::optional<bool> moderator_muted_text;
		const std::string name;
	};

	LLSpeaker(const speaker_entry_t& entry);
	~LLSpeaker() = default;
	void update(const speaker_entry_t& entry);
	void lookupName();

	void onNameCache(const LLAvatarName& full_name);

	bool isInVoiceChannel();

	void setStatus(ESpeakerStatus status)
	{
		if (status != mStatus)
		{
			mStatus = status;
			mNeedsResort = true;
		}
	}
	void setName(const std::string& name)
	{
		if (name != mDisplayName)
		{
			mDisplayName = name;
			mNeedsResort = true;
		}
	}
	void setSpokenTime(F32 time)
	{
		if (mLastSpokeTime != time)
		{
			mLastSpokeTime = time;
			mNeedsResort = true;
		}
	}

	LLUUID			mID; 
	ESpeakerStatus	mStatus;			// current activity status in speech group
	ESpeakerType	mType : 2;
	bool			mIsModerator : 1;
	bool			mModeratorMutedVoice : 1;
	bool			mModeratorMutedText : 1;
	bool			mHasSpoken : 1;			// has this speaker said anything this session?
	bool			mHasLeftCurrentCall : 1;	// has this speaker left the current voice call?
	bool			mTyping : 1;
	F32				mLastSpokeTime;		// timestamp when this speaker last spoke
	F32				mSpeechVolume;		// current speech amplitude (timea average rms amplitude?)
	std::string		mDisplayName;		// cache user name for this speaker
	LLColor4		mDotColor;
	bool			mNeedsResort;
	S32				mSortIndex;
};

class LLSpeakerUpdateSpeakerEvent final : public LLOldEvents::LLEvent
{
public:
	LLSpeakerUpdateSpeakerEvent(LLSpeaker* source);
	/*virtual*/ LLSD getValue() override;
private:
	const LLUUID& mSpeakerID;
};

class LLSpeakerUpdateModeratorEvent final : public LLOldEvents::LLEvent
{
public:
	LLSpeakerUpdateModeratorEvent(LLSpeaker* source);
	/*virtual*/ LLSD getValue() override;
private:
	const LLUUID& mSpeakerID;
	BOOL mIsModerator;
};

class LLSpeakerTextModerationEvent final : public LLOldEvents::LLEvent
{
public:
	LLSpeakerTextModerationEvent(LLSpeaker* source);
	/*virtual*/ LLSD getValue() override;
};

class LLSpeakerVoiceModerationEvent final : public LLOldEvents::LLEvent
{
public:
	LLSpeakerVoiceModerationEvent(LLSpeaker* source);
	/*virtual*/ LLSD getValue() override;
};

class LLSpeakerListChangeEvent final : public LLOldEvents::LLEvent
{
public:
	LLSpeakerListChangeEvent(LLSpeakerMgr* source, const LLUUID& speaker_id);
	/*virtual*/ LLSD getValue() override;

private:
	const LLUUID& mSpeakerID;
};

/**
 * class LLSpeakerActionTimer
 *
 * Implements a timer that calls stored callback action for stored speaker after passed period.
 *
 * Action is called until callback returns "true".
 * In this case the timer will be removed via LLEventTimer::updateClass().
 * Otherwise it should be deleted manually in place where it is used.
 * If action callback is not set timer will tick only once and deleted.
 */
class LLSpeakerActionTimer final : public LLEventTimer
{
public:
	typedef std::function<bool(const LLUUID&)>	action_callback_t;
	typedef std::map<LLUUID, LLSpeakerActionTimer*> action_timers_map_t;
	typedef action_timers_map_t::value_type			action_value_t;
	typedef action_timers_map_t::const_iterator		action_timer_const_iter_t;
	typedef action_timers_map_t::iterator			action_timer_iter_t;

	/**
	 * Constructor.
	 *
	 * @param action_cb - callback which will be called each time after passed action period.
	 * @param action_period - time in seconds timer should tick.
	 * @param speaker_id - LLUUID of speaker which will be passed into action callback.
	 */
	LLSpeakerActionTimer(action_callback_t action_cb, F32 action_period, const LLUUID& speaker_id);
	virtual ~LLSpeakerActionTimer() = default;

	/**
	 * Implements timer "tick".
	 *
	 * If action callback is not specified returns true. Instance will be deleted by LLEventTimer::updateClass().
	 */
	BOOL tick() override;

	/**
	 * Clears the callback.
	 *
	 * Use this instead of deleteing this object.
	 * The next call to tick() will return true and that will destroy this object.
	 */
	void unset();
private:
	action_callback_t	mActionCallback;
	LLUUID				mSpeakerId;
};

/**
 * Represents a functionality to store actions for speakers with delay.
 * Is based on LLSpeakerActionTimer.
 */
class LLSpeakersDelayActionsStorage
{
public:
	LLSpeakersDelayActionsStorage(LLSpeakerActionTimer::action_callback_t action_cb, F32 action_delay);
	~LLSpeakersDelayActionsStorage();

	/**
	 * Sets new LLSpeakerActionTimer with passed speaker UUID.
	 */
	void setActionTimer(const LLUUID& speaker_id);

	/**
	 * Removes stored LLSpeakerActionTimer for passed speaker UUID from internal map and optionally deletes it.
	 *
	 * @see onTimerActionCallback()
	 */
	void unsetActionTimer(const LLUUID& speaker_id);

	void removeAllTimers();

	bool isTimerStarted(const LLUUID& speaker_id);
private:
	/**
	 * Callback of the each instance of LLSpeakerActionTimer.
	 *
	 * Unsets an appropriate timer instance and calls action callback for specified speacker_id.
	 *
	 * @see unsetActionTimer()
	 */
	bool onTimerActionCallback(const LLUUID& speaker_id);

	LLSpeakerActionTimer::action_timers_map_t	mActionTimersMap;
	LLSpeakerActionTimer::action_callback_t		mActionCallback;

	/**
	 * Delay to call action callback for speakers after timer was set.
	 */
	F32	mActionDelay;

};


class LLSpeakerMgr : public LLOldEvents::LLObservable
{
	LOG_CLASS(LLSpeakerMgr);

public:
	typedef LLSpeaker::speaker_entry_t speaker_entry_t;

	LLSpeakerMgr(LLVoiceChannel* channelp);
	virtual ~LLSpeakerMgr();

	LLPointer<LLSpeaker> findSpeaker(const LLUUID& avatar_id);
	void update(BOOL resort_ok);
	void setSpeakerTyping(const LLUUID& speaker_id, BOOL typing);
	void speakerChatted(const LLUUID& speaker_id);

	void setSpeakers(const std::vector<speaker_entry_t>& speakers);
	LLPointer<LLSpeaker> setSpeaker(const speaker_entry_t& speakers);

	BOOL isVoiceActive() const;

	typedef std::vector<LLPointer<LLSpeaker> > speaker_list_t;
	void getSpeakerList(speaker_list_t* speaker_list, BOOL include_text);
	LLVoiceChannel* getVoiceChannel() { return mVoiceChannel; }
	const LLUUID getSessionID() const;
	bool isSpeakerToBeRemoved(const LLUUID& speaker_id) const;

	/**
	 * Removes avaline speaker.
	 *
	 * This is a HACK due to server does not send information that Avaline caller ends call.
	 * It can be removed when server is updated. See EXT-4301 for details
	 */
	bool removeAvalineSpeaker(const LLUUID& speaker_id) { return removeSpeaker(speaker_id); }

	/**
	 * Initializes mVoiceModerated depend on LLSpeaker::mModeratorMutedVoice of agent's participant.
	 *
	 * Is used only to implement workaround to initialize mVoiceModerated on first join to group chat. See EXT-6937
	 */
	void initVoiceModerateMode();

protected:
	virtual void updateSpeakerList();
	void setSpeakerNotInChannel(LLPointer<LLSpeaker> speackerp);
	bool removeSpeaker(const LLUUID& speaker_id);

	typedef std::map<LLUUID, LLPointer<LLSpeaker> > speaker_map_t;
	speaker_map_t		mSpeakers;
	bool				mSpeakerListUpdated;
	LLTimer				mGetListTime;

	speaker_list_t		mSpeakersSorted;
	LLFrameTimer		mSpeechTimer;
	LLVoiceChannel*		mVoiceChannel;

	/**
	 * time out speakers when they are not part of current session
	 */
	LLSpeakersDelayActionsStorage* mSpeakerDelayRemover;

	// *TODO: should be moved back into LLIMSpeakerMgr when a way to request the current voice channel
	// moderation mode is implemented: See EXT-6937
	bool mVoiceModerated;

	// *TODO: To be removed when a way to request the current voice channel
	// moderation mode is implemented: See EXT-6937
	bool mModerateModeHandledFirstTime;
};

class LLIMSpeakerMgr final : public LLSpeakerMgr
{
	LOG_CLASS(LLIMSpeakerMgr);

public:
	LLIMSpeakerMgr(LLVoiceChannel* channel);

	void updateSpeakers(const LLSD& update);
	void setSpeakers(const LLSD& speakers);

	void toggleAllowTextChat(const LLUUID& speaker_id);

	/**
	 * Mutes/Unmutes avatar for current group voice chat.
	 *
	 * It only marks avatar as muted for session and does not use local Agent's Block list.
	 * It does not mute Agent itself.
	 *
	 * @param[in] avatar_id UUID of avatar to be processed
	 * @param[in] unmute if false - specified avatar will be muted, otherwise - unmuted.
	 *
	 * @see moderateVoiceAllParticipants()
	 */
	void moderateVoiceParticipant(const LLUUID& avatar_id, bool unmute);

	/**
	 * Mutes/Unmutes all avatars for current group voice chat.
	 *
	 * It only marks avatars as muted for session and does not use local Agent's Block list.
	 * It calls forceVoiceModeratedMode() in case of session is already in requested state.
	 *
	 * @param[in] unmute_everyone if false - avatars will be muted, otherwise - unmuted.
	 *
	 * @see moderateVoiceParticipant()
	 */
	void moderateVoiceAllParticipants(bool unmute_everyone);

	void processSessionUpdate(const LLSD& session_update);

protected:
	void updateSpeakerList() override;

	void moderateVoiceSession(const LLUUID& session_id, bool disallow_voice);

	/**
	 * Process all participants to mute/unmute them according to passed voice session state.
	 */
	void forceVoiceModeratedMode(bool should_be_muted);

};

class LLActiveSpeakerMgr final : public LLSpeakerMgr, public LLSingleton<LLActiveSpeakerMgr>
{
	LOG_CLASS(LLActiveSpeakerMgr);

public:
	LLActiveSpeakerMgr();
protected:
	void updateSpeakerList() override;
};

class LLLocalSpeakerMgr final : public LLSpeakerMgr, public LLSingleton<LLLocalSpeakerMgr>
{
	LOG_CLASS(LLLocalSpeakerMgr);
public:
	LLLocalSpeakerMgr();
	~LLLocalSpeakerMgr ();
protected:
	void updateSpeakerList() override;
};

#endif // LL_LLSPEAKERS_H

