/** 
 * @file llfloateractivespeakers.h
 * @brief Management interface for muting and controlling volume of residents currently speaking
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERACTIVESPEAKERS_H
#define LL_LLFLOATERACTIVESPEAKERS_H

#include "llfloater.h"
#include "llvoiceclient.h"

class LLParticipantList;

class LLFloaterActiveSpeakers : 
	public LLFloaterSingleton<LLFloaterActiveSpeakers>, 
	public LLFloater, 
	public LLVoiceClientParticipantObserver
{
	// friend of singleton class to allow construction inside getInstance() since constructor is protected
	// to enforce singleton constraint
	friend class LLUISingleton<LLFloaterActiveSpeakers, VisibilityPolicy<LLFloater> >;
public:
	virtual ~LLFloaterActiveSpeakers();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen();
	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ void draw();

	/*virtual*/ void onParticipantsChanged();

	static void* createSpeakersPanel(void* data);

protected:
	LLFloaterActiveSpeakers(const LLSD& seed);

	LLParticipantList* mPanel;
};

#endif // LL_LLFLOATERACTIVESPEAKERS_H
