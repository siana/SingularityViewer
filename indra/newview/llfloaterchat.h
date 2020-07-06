/** 
 * @file llfloaterchat.h
 * @brief LLFloaterChat class definition
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

/*
 * Actually the "Chat History" floater.
 * Should be llfloaterchathistory, not llfloaterchat.
 */

#ifndef LL_LLFLOATERCHAT_H
#define LL_LLFLOATERCHAT_H

#include "llfloater.h"
#include "lllogchat.h"
class LLChat;
class LLChatBar;
class LLParticipantList;

class LLFloaterChat
	:	public LLFloater, public LLUISingleton<LLFloaterChat, LLFloaterChat>
{
public:
	LLFloaterChat(const LLSD& seed);
	~LLFloaterChat();

	virtual void draw();
	virtual BOOL postBuild();
	virtual void onClose(bool app_quitting);
	virtual void onFocusReceived();
	virtual void handleVisibilityChange(BOOL cur_visibility);
	virtual void setMinimized(BOOL);
	void updateConsoleVisibility();

	static void setHistoryCursorAndScrollToEnd();

	// Add chat to console and history list.
	// Color based on source, type, distance.
	static void addChat(const std::string& str, BOOL from_im = FALSE, BOOL local_agent = FALSE);
	static void addChat(LLChat& chat, BOOL from_im = FALSE, BOOL local_agent = FALSE);
	
	// Add chat to history alone.
	static void addChatHistory(const std::string& str, bool log_to_file = true);
	static void addChatHistory(LLChat& chat, bool log_to_file = true);
	
	static void triggerAlerts(const std::string& text);

	void onClickToggleShowMute(bool show_mute, class LLTextEditor* history_editor, LLTextEditor* history_editor_with_mute);
	void chatFromLogFile(LLLogChat::ELogLineType type, const std::string& line);
	static void loadHistory();
	static void* createSpeakersPanel(void* data);
	static void* createChatPanel(void* data);

	// visibility policy for LLUISingleton
	static bool visible(LLFloater* instance, const LLSD& key);
	static void show(LLFloater* instance, const LLSD& key);
	static void hide(LLFloater* instance, const LLSD& key);

	LLParticipantList* mPanel;
	BOOL mScrolledToEnd;

	BOOL    focusFirstItem(BOOL prefer_text_fields = FALSE, BOOL focus_flash = TRUE );

	CachedUICtrl<LLChatBar> mChatPanel;
};

#endif
