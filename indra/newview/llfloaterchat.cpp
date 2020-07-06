/** 
 * @file llfloaterchat.cpp
 * @brief LLFloaterChat class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

/**
 * Actually the "Chat History" floater.
 * Should be llfloaterchathistory, not llfloaterchat.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterchat.h"

// linden library includes
#include "llaudioengine.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lltextparser.h"
#include "lltrans.h"
#include "llurlregistry.h"
#include "llwindow.h"

// project include
#include "ascentkeyword.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llchatbar.h"
#include "llconsole.h"
#include "llfloaterchatterbox.h"
#include "llfloatermute.h"
#include "llfloaterscriptdebug.h"
#include "lllogchat.h"
#include "llmutelist.h"
#include "llparticipantlist.h"
#include "llspeakers.h"
#include "llstylemap.h"
#include "lluictrlfactory.h"
#include "llviewermessage.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llweb.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

#include <boost/algorithm/string/predicate.hpp>

//
// Global statics
//
LLColor4 agent_chat_color(const LLUUID& id, const std::string&, bool local_chat = true);
LLColor4 get_text_color(const LLChat& chat, bool from_im = false);
void show_log_browser(const std::string&, const LLUUID&);

//
// Member Functions
//
LLFloaterChat::LLFloaterChat(const LLSD& seed)
:	LLFloater(std::string("chat floater"), std::string("FloaterChatRect"), LLStringUtil::null, 
			  RESIZE_YES, 440, 100, DRAG_ON_TOP, MINIMIZE_NO, CLOSE_YES),
	mPanel(NULL)
{
	mFactoryMap["chat_panel"] = LLCallbackMap(createChatPanel, NULL);
	mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel, NULL);
	// do not automatically open singleton floaters (as result of getInstance())
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_chat_history.xml", &getFactoryMap(), /*no_open =*/false);

	LLTextEditor* history_editor_with_mute = getChild<LLTextEditor>("Chat History Editor with mute");
	getChild<LLUICtrl>("show mutes")->setCommitCallback(boost::bind(&LLFloaterChat::onClickToggleShowMute, this, _2, getChild<LLTextEditor>("Chat History Editor"), history_editor_with_mute));
	getChild<LLUICtrl>("chat_history_open")->setCommitCallback(boost::bind(show_log_browser, "chat", LLUUID::null));
}

LLFloaterChat::~LLFloaterChat()
{
	// Children all cleaned up by default view destructor.
}

void LLFloaterChat::draw()
{
	mChatPanel->refresh();
	mPanel->refreshSpeakers();
	LLFloater::draw();
}

BOOL LLFloaterChat::postBuild()
{
	mPanel = getChild<LLParticipantList>("active_speakers_panel");

// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
	getChild<LLUICtrl>("toggle_active_speakers_btn")->setCommitCallback(boost::bind(&LLView::setVisible, mPanel, boost::bind(std::logical_and<bool>(), _2, !boost::bind(&RlvHandler::hasBehaviour, boost::ref(gRlvHandler), RLV_BHVR_SHOWNAMES))));
// [/RLVa:KB]
	//getChild<LLUICtrl>("toggle_active_speakers_btn")->setCommitCallback(boost::bind(&LLView::setVisible, mPanel, _2));

	mChatPanel.connect(this,"chat_panel");
	mChatPanel->setGestureCombo(getChild<LLComboBox>( "Gesture"));
	return TRUE;
}

// public virtual
void LLFloaterChat::onClose(bool app_quitting)
{
	if (!app_quitting)
	{
		gSavedSettings.setBOOL("ShowChatHistory", FALSE);
	}
	setVisible(FALSE);
}

void LLFloaterChat::handleVisibilityChange(BOOL new_visibility)
{
	// Hide the chat overlay when our history is visible.
	updateConsoleVisibility();

	// stop chat history tab from flashing when it appears
	if (new_visibility)
	{
		LLFloaterChatterBox::getInstance()->setFloaterFlashing(this, FALSE);
	}

	LLFloater::handleVisibilityChange(new_visibility);
}

// virtual
void LLFloaterChat::onFocusReceived()
{
	LLUICtrl* chat_editor = getChild<LLUICtrl>("Chat Editor");
	if (getVisible() && chat_editor->getVisible())
	{
		gFocusMgr.setKeyboardFocus(chat_editor);
		chat_editor->setFocus(true);
	}

	LLFloater::onFocusReceived();
}

void LLFloaterChat::setMinimized(BOOL minimized)
{
	LLFloater::setMinimized(minimized);
	updateConsoleVisibility();
}


void LLFloaterChat::updateConsoleVisibility()
{
	// determine whether we should show console due to not being visible
	gConsole->setVisible( !isInVisibleChain()								// are we not in part of UI being drawn?
							|| isMinimized()								// are we minimized?
							|| (getHost() && getHost()->isMinimized() ));	// are we hosted in a minimized floater?
}

void add_timestamped_line(LLViewerTextEditor* edit, LLChat chat, const LLColor4& color)
{
	std::string line = chat.mText;
	bool prepend_newline = true;
	if (gSavedSettings.getBOOL("ChatShowTimestamps"))
	{
		edit->appendTime(prepend_newline);
		prepend_newline = false;
	}

	// If the msg is from an agent (not yourself though),
	// extract out the sender name and replace it with the hotlinked name.
	if (chat.mSourceType == CHAT_SOURCE_AGENT &&
//		chat.mFromID.notNull())
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
		chat.mFromID.notNull() &&
		(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) )
// [/RLVa:KB]
	{
		chat.mURL = LLAvatarActions::getSLURL(chat.mFromID);
	}

	if (chat.mSourceType == CHAT_SOURCE_OBJECT)
	{
		LLStringUtil::trim(chat.mFromName);
		if (chat.mFromName.empty())
		{
			chat.mFromName = LLTrans::getString("Unnamed");
			line = chat.mFromName + line;
		}
	}

	static const LLCachedControl<bool> italicize("LiruItalicizeActions");
	bool is_irc = italicize && chat.mChatStyle == CHAT_STYLE_IRC;
	// If the chat line has an associated url, link it up to the name.
	if (!chat.mURL.empty()
		&& boost::algorithm::starts_with(line, chat.mFromName))
	{
		line = line.substr(chat.mFromName.length());
		LLStyleSP sourceStyle = LLStyleMap::instance().lookup(chat.mFromID, chat.mURL);
		sourceStyle->mItalic = is_irc;
		edit->appendText(chat.mFromName, false, prepend_newline, sourceStyle, false);
		prepend_newline = false;
	}
	LLStyleSP style(new LLStyle);
	style->setColor(color);
	style->mItalic = is_irc;
	style->mBold = chat.mChatType == CHAT_TYPE_SHOUT;
	edit->appendText(line, false, prepend_newline, style, chat.mSourceType == CHAT_SOURCE_SYSTEM);
}

void LLFloaterChat::addChatHistory(const std::string& str, bool log_to_file)
{
	LLChat chat(str);
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	addChatHistory(chat, log_to_file);
}

void log_chat_text(const LLChat& chat)
{
	std::string histstr;
	if (gSavedPerAccountSettings.getBOOL("LogChatTimestamp"))
		histstr = LLLogChat::timestamp(gSavedPerAccountSettings.getBOOL("LogTimestampDate")) + chat.mText;
	else
		histstr = chat.mText;

	LLLogChat::saveHistory("chat", LLUUID::null, histstr);
}
// static
void LLFloaterChat::addChatHistory(LLChat& chat, bool log_to_file)
{	
// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
	if (rlv_handler_t::isEnabled())
	{
		// TODO-RLVa: we might cast too broad a net by filtering here, needs testing
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) && (!chat.mRlvLocFiltered) && (CHAT_SOURCE_AGENT != chat.mSourceType) )
		{
			RlvUtil::filterLocation(chat.mText);
			chat.mRlvLocFiltered = TRUE;
		}
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (!chat.mRlvNamesFiltered) )
		{
			// NOTE: this will also filter inventory accepted/declined text in the chat history
			if (CHAT_SOURCE_AGENT != chat.mSourceType)
			{
				// Filter object and system chat (names are filtered elsewhere to save ourselves an gObjectList lookup)
				RlvUtil::filterNames(chat.mText);
			}
			chat.mRlvNamesFiltered = TRUE;
		}
	}
// [/RLVa:KB]

	if (gSavedPerAccountSettings.getBOOL("LogChat") && log_to_file)
	{
		log_chat_text(chat);
	}
	
	LLColor4 color = get_text_color(chat);
	
	if (!log_to_file) color = gSavedSettings.getColor("LogChatColor");	//Recap from log file.

	if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
	{
		LLFloaterScriptDebug::addScriptLine(chat, color);
		if (!gSavedSettings.getBOOL("ScriptErrorsAsChat"))
		{
			return;
		}
	}
	else if (chat.mChatType == CHAT_TYPE_OWNER && gSavedSettings.getBOOL("SinguOwnerSayAsErrors"))
	{
		LLFloaterScriptDebug::addScriptLine(chat, color);
		return;
	}
	
	// could flash the chat button in the status bar here. JC
	LLFloaterChat* chat_floater = LLFloaterChat::getInstance(LLSD());
	LLViewerTextEditor* history_editor = chat_floater->getChild<LLViewerTextEditor>("Chat History Editor");
	LLViewerTextEditor* history_editor_with_mute = chat_floater->getChild<LLViewerTextEditor>("Chat History Editor with mute");

	history_editor->setParseHTML(TRUE);
	history_editor_with_mute->setParseHTML(TRUE);

	if (!chat.mMuted)
	{
		add_timestamped_line(history_editor, chat, color);
		add_timestamped_line(history_editor_with_mute, chat, color);
	}
	else
	{
		static LLCachedControl<bool> color_muted_chat("ColorMutedChat");
		// desaturate muted chat
		LLColor4 muted_color = lerp(color, color_muted_chat ? gSavedSettings.getColor4("AscentMutedColor") : LLColor4::grey, 0.5f);
		add_timestamped_line(history_editor_with_mute, chat, muted_color);
	}
	
	// add objects as transient speakers that can be muted
	if (chat.mSourceType == CHAT_SOURCE_OBJECT)
	{
		LLLocalSpeakerMgr::getInstance()->setSpeaker({ chat.mFromID, LLSpeaker::SPEAKER_OBJECT, LLSpeaker::STATUS_NOT_IN_CHANNEL, boost::none, boost::none, chat.mFromName });
	}

	// start tab flashing on incoming text from other users (ignoring system text, etc)
	if (!chat_floater->isInVisibleChain() && chat.mSourceType == CHAT_SOURCE_AGENT)
	{
		LLFloaterChatterBox::getInstance()->setFloaterFlashing(chat_floater, TRUE);
	}
}

// static
void LLFloaterChat::setHistoryCursorAndScrollToEnd()
{
	if (LLViewerTextEditor* editor = getInstance()->findChild<LLViewerTextEditor>("Chat History Editor")) 
	{
		editor->setCursorAndScrollToEnd();
	}
	if (LLViewerTextEditor* editor = getInstance()->findChild<LLViewerTextEditor>("Chat History Editor with mute"))
	{
		 editor->setCursorAndScrollToEnd();
	}
}


//static
void LLFloaterChat::onClickToggleShowMute(bool show_mute, LLTextEditor* history_editor, LLTextEditor* history_editor_with_mute)
{
	(show_mute ? history_editor_with_mute : history_editor)->setCursorAndScrollToEnd();
}

void LLFloaterChat::addChat(const std::string& str, BOOL from_im, BOOL local_agent)
{
	LLChat chat(str);
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	addChat(chat, from_im, local_agent);
}

// Put a line of chat in all the right places
void LLFloaterChat::addChat(LLChat& chat,
			  BOOL from_instant_message, 
			  BOOL local_agent)
{
	LLColor4 text_color = get_text_color(chat, from_instant_message);

	BOOL invisible_script_debug_chat = 
			chat.mChatType == CHAT_TYPE_DEBUG_MSG
			&& !gSavedSettings.getBOOL("ScriptErrorsAsChat");

// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
	if (rlv_handler_t::isEnabled())
	{
		// TODO-RLVa: we might cast too broad a net by filtering here, needs testing
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) && (!chat.mRlvLocFiltered) && (CHAT_SOURCE_AGENT != chat.mSourceType) )
		{
			if (!from_instant_message)
				RlvUtil::filterLocation(chat.mText);
			chat.mRlvLocFiltered = TRUE;
		}
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (!chat.mRlvNamesFiltered) )
		{
			if ( (!from_instant_message) && (CHAT_SOURCE_AGENT != chat.mSourceType) )
			{
				// Filter object and system chat (names are filtered elsewhere to save ourselves an gObjectList lookup)
				RlvUtil::filterNames(chat.mText);
			}
			chat.mRlvNamesFiltered = TRUE;
		}
	}
// [/RLVa:KB]

	if (!invisible_script_debug_chat 
		&& !chat.mMuted 
		&& gConsole 
		&& !local_agent)
	{
		// We display anything if it's not an IM. If it's an IM, check pref...
		if	( !from_instant_message || gSavedSettings.getBOOL("IMInChatConsole") ) 
		{
			// Replace registered urls in the console so it looks right.
			std::string chit(chat.mText), // Read through this
						chat; // Add parts to this
			LLUrlMatch match;
			while (!chit.empty() && LLUrlRegistry::instance().findUrl(chit, match))
			{
				const auto start(match.getStart()), length(match.getEnd()+1-start);
				if (start > 0) chat += chit.substr(0, start); // Add up to the start of the match
				chat += match.getLabel() + match.getQuery(); // Add the label and the query
				chit.erase(0, start+length); // remove the url match and all before it
			}
			if (!chit.empty()) chat += chit; // Add any leftovers
			gConsole->addConsoleLine(chat, text_color);
		}
	}

	if(from_instant_message && gSavedPerAccountSettings.getBOOL("LogChatIM"))
		log_chat_text(chat);
	
	if(from_instant_message && gSavedSettings.getBOOL("IMInChatHistory")) 	 
		addChatHistory(chat,false);

	triggerAlerts(chat.mText);

	if(!from_instant_message)
		addChatHistory(chat);
}

// Moved from lltextparser.cpp to break llui/llaudio library dependency.
//static
void LLFloaterChat::triggerAlerts(const std::string& text)
{
	// Cannot instantiate LLTextParser before logging in.
	if (gDirUtilp->getLindenUserDir(true).empty())
		return;

	LLTextParser* parser = LLTextParser::getInstance();
//    bool spoken=FALSE;
	for (S32 i=0;i<parser->mHighlights.size();i++)
	{
		LLSD& highlight = parser->mHighlights[i];
		if (parser->findPattern(text,highlight) >= 0 )
		{
			if(gAudiop)
			{
				if ((std::string)highlight["sound_lluuid"] != LLUUID::null.asString())
				{
					if(gAudiop)
					gAudiop->triggerSound(highlight["sound_lluuid"].asUUID(), 
						gAgent.getID(),
						1.f,
						LLAudioEngine::AUDIO_TYPE_UI,
						gAgent.getPositionGlobal() );
				}
/*				
				if (!spoken) 
				{
					LLTextToSpeech* text_to_speech = NULL;
					text_to_speech = LLTextToSpeech::getInstance();
					spoken = text_to_speech->speak((LLString)highlight["voice"],text); 
				}
 */
			}
			if (highlight["flash"])
			{
				LLWindow* viewer_window = gViewerWindow->getWindow();
				if (viewer_window && viewer_window->getMinimized())
				{
					viewer_window->flashIcon(5.f);
				}
			}
		}
	}
}

LLColor4 get_text_color(const LLChat& chat, bool from_im)
{
	LLColor4 text_color;

	if(chat.mMuted)
	{
		static LLCachedControl<bool> color_muted_chat("ColorMutedChat");
		if (color_muted_chat)
			text_color = gSavedSettings.getColor4("AscentMutedColor");
		else
			text_color.setVec(0.8f, 0.8f, 0.8f, 1.f);
	}
	else
	{
		switch(chat.mSourceType)
		{
		case CHAT_SOURCE_SYSTEM:
			text_color = gSavedSettings.getColor4("SystemChatColor");
			break;
		case CHAT_SOURCE_AGENT:
			text_color = agent_chat_color(chat.mFromID, chat.mFromName, !from_im);
			break;
		case CHAT_SOURCE_OBJECT:
			if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
			{
				text_color = gSavedSettings.getColor4("ScriptErrorColor");
			}
			else if ( chat.mChatType == CHAT_TYPE_OWNER )
			{
				text_color = gSavedSettings.getColor4("llOwnerSayChatColor");
			}
			else
			{
				text_color = gSavedSettings.getColor4("ObjectChatColor");
			}
			break;
		default:
			text_color.setToWhite();
		}

		if (!chat.mPosAgent.isExactlyZero())
		{
			LLVector3 pos_agent = gAgent.getPositionAgent();
			F32 distance = dist_vec(pos_agent, chat.mPosAgent);
			if (distance > gAgent.getNearChatRadius())
			{
				// diminish far-off chat
				text_color.mV[VALPHA] = 0.8f;
			}
		}
	}

	static const LLCachedControl<bool> sKeywordsChangeColor(gSavedPerAccountSettings, "KeywordsChangeColor", false);
	if (sKeywordsChangeColor && gAgentID != chat.mFromID && AscentKeyword::hasKeyword((chat.mSourceType != CHAT_SOURCE_SYSTEM && boost::starts_with(chat.mText, chat.mFromName)) ? chat.mText.substr(chat.mFromName.length()) : chat.mText, 1))
	{
		static const LLCachedControl<LLColor4> sKeywordsColor(gSavedPerAccountSettings, "KeywordsColor", LLColor4(1.f, 1.f, 1.f, 1.f));
		text_color = sKeywordsColor;
	}

	return text_color;
}

//static
void LLFloaterChat::loadHistory()
{
	LLLogChat::loadHistory("chat", LLUUID::null, boost::bind(&LLFloaterChat::chatFromLogFile, getInstance(), _1, _2));
}


void LLFloaterChat::chatFromLogFile(LLLogChat::ELogLineType type, const std::string& line)
{
	bool log_line = type == LLLogChat::LOG_LINE;
	if (log_line || gSavedPerAccountSettings.getBOOL("LogChat"))
	{
		LLStyleSP style(new LLStyle(true, gSavedSettings.getColor4("LogChatColor"), LLStringUtil::null));
		const auto text = log_line ? line : getString(type == LLLogChat::LOG_END ? "IM_end_log_string" : "IM_logging_string");
		for (const auto& ed_name : { "Chat History Editor", "Chat History Editor with mute" })
			getChild<LLTextEditor>(ed_name)->appendText(text, false, true, style, false);
	}
}

//static
void* LLFloaterChat::createSpeakersPanel(void* data)
{
	return new LLParticipantList(LLLocalSpeakerMgr::getInstance(), true);
}

//static
void* LLFloaterChat::createChatPanel(void* data)
{
	return new LLChatBar;
}

//static 
bool LLFloaterChat::visible(LLFloater* instance, const LLSD& key)
{
	return VisibilityPolicy<LLFloater>::visible(instance, key);
}

//static 
void LLFloaterChat::show(LLFloater* instance, const LLSD& key)
{
	VisibilityPolicy<LLFloater>::show(instance, key);
}

//static 
void LLFloaterChat::hide(LLFloater* instance, const LLSD& key)
{
	if(instance->getHost())
	{
		LLFloaterChatterBox::hideInstance();
	}
	else
	{
		VisibilityPolicy<LLFloater>::hide(instance, key);
	}
}

BOOL LLFloaterChat::focusFirstItem(BOOL prefer_text_fields, BOOL focus_flash )
{
	LLUICtrl* chat_editor = getChild<LLUICtrl>("Chat Editor");
	if (getVisible() && chat_editor->getVisible())
	{
		gFocusMgr.setKeyboardFocus(chat_editor);

        chat_editor->setFocus(TRUE);

		return TRUE;
	}

	return LLUICtrl::focusFirstItem(prefer_text_fields, focus_flash);
}

