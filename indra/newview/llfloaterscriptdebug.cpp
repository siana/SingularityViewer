/** 
 * @file llfloaterscriptdebug.cpp
 * @brief Chat window for showing script errors and warnings
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterscriptdebug.h"

#include "lluictrlfactory.h"

// project include
#include "llslurl.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llvoavatarself.h"

//
// Statics
//
LLFloaterScriptDebug*	LLFloaterScriptDebug::sInstance = NULL;

//
// Member Functions
//
LLFloaterScriptDebug::LLFloaterScriptDebug() : 
	LLMultiFloater()
{
	// avoid resizing of the window to match 
	// the initial size of the tabbed-childs, whenever a tab is opened or closed
	mAutoResize = FALSE;
}

LLFloaterScriptDebug::~LLFloaterScriptDebug()
{
	sInstance = NULL;
}

void LLFloaterScriptDebug::show(const LLUUID& object_id)
{
	LLFloater* floaterp = addOutputWindow(object_id);
	if (sInstance)
	{
		sInstance->open();		/* Flawfinder: ignore */
		sInstance->showFloater(floaterp);
	}
}

BOOL LLFloaterScriptDebug::postBuild()
{
	LLMultiFloater::postBuild();

	if (mTabContainer)
	{
		// *FIX: apparantly fails for tab containers?
// 		mTabContainer->requires<LLFloater>("all_scripts");
// 		mTabContainer->checkRequirements();
		return TRUE;
	}

	return FALSE;
}

void* getOutputWindow(void* data)
{
	return new LLFloaterScriptDebugOutput();
}

LLFloater* LLFloaterScriptDebug::addOutputWindow(const LLUUID &object_id)
{
	if (!sInstance)
	{
		sInstance = new LLFloaterScriptDebug();
		LLCallbackMap::map_t factory_map;
		factory_map["all_scripts"] = LLCallbackMap(getOutputWindow, NULL);
		LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_script_debug.xml", &factory_map);
		sInstance->setVisible(FALSE);
	}

	LLFloater* floaterp = NULL;
	LLFloater::setFloaterHost(sInstance);
	{
		floaterp = LLFloaterScriptDebugOutput::show(object_id);
	}
	LLFloater::setFloaterHost(NULL);

	// Tabs sometimes overlap resize handle
	sInstance->moveResizeHandlesToFront();

	return floaterp;
}

void LLFloaterScriptDebug::addScriptLine(LLChat& chat, const LLColor4& color)
{
	const auto& utf8mesg = chat.mText;
	const auto& user_name = chat.mFromName;
	const auto& source_id = chat.mFromID;

	LLViewerObject* objectp = gObjectList.findObject(source_id);
	std::string floater_label;

	// Handle /me messages.
	std::string prefix = utf8mesg.substr(0, 4);
	std::string message = (prefix == "/me " || prefix == "/me'") ? user_name + utf8mesg.substr(3) : utf8mesg;

	LLSD sdQuery = LLSD().with("name", user_name);
	if (objectp)
	{
		if(objectp->isHUDAttachment())
		{
			if (isAgentAvatarValid())
			{
				((LLViewerObject*)gAgentAvatarp)->setIcon(LLViewerTextureManager::getFetchedTextureFromFile("script_error.j2c", FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_UI));
			}
		}
		else
		{
			objectp->setIcon(LLViewerTextureManager::getFetchedTextureFromFile("script_error.j2c", FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_UI));
		}

		const auto& pos = objectp->getPositionRegion();
		sdQuery.with("owner", objectp->mOwnerID)
			.with("groupowned", objectp->flagObjectGroupOwned())
			.with("slurl", LLSLURL(objectp->getRegion()->getName(), pos).getLocationString());

		floater_label = llformat("%s(%.0f, %.0f, %.0f)",
						user_name.c_str(),
						pos.mV[VX],
						pos.mV[VY],
						pos.mV[VZ]);
	}
	else
	{
		floater_label = user_name;
	}

	chat.mURL = LLSLURL("objectim", source_id, LLURI::mapToQueryString(sdQuery)).getSLURLString();

	addOutputWindow(LLUUID::null);
	addOutputWindow(source_id);

	// add to "All" floater
	if (auto floaterp = LLFloaterScriptDebugOutput::getFloaterByID(LLUUID::null))
	{
		floaterp->addLine(chat, message, color);
	}
	
	// add to specific script instance floater
	if (auto floaterp = LLFloaterScriptDebugOutput::getFloaterByID(source_id))
	{
		floaterp->setTitle(floater_label); // Update title
		floaterp->addLine(chat, message, color);
	}
}

//
// LLFloaterScriptDebugOutput
//

std::map<LLUUID, LLFloaterScriptDebugOutput*> LLFloaterScriptDebugOutput::sInstanceMap;

LLFloaterScriptDebugOutput::LLFloaterScriptDebugOutput()
: mObjectID(LLUUID::null)
{
	sInstanceMap[mObjectID] = this;
}

LLFloaterScriptDebugOutput::LLFloaterScriptDebugOutput(const LLUUID& object_id)
: LLFloater(std::string("script instance floater"), LLRect(0, 200, 200, 0), std::string("Script"), TRUE), mObjectID(object_id),
	mHistoryEditor(nullptr)
{
	S32 y = getRect().getHeight() - LLFLOATER_HEADER_SIZE - LLFLOATER_VPAD;
	S32 x = LLFLOATER_HPAD;
	// History editor
	// Give it a border on the top
	LLRect history_editor_rect(
		x,
		y,
		getRect().getWidth() - LLFLOATER_HPAD,
				LLFLOATER_VPAD );
	mHistoryEditor = new LLViewerTextEditor( std::string("Chat History Editor"), 
										history_editor_rect, S32_MAX, LLStringUtil::null, LLFontGL::getFontSansSerif());
	mHistoryEditor->setWordWrap( TRUE );
	mHistoryEditor->setFollowsAll();
	mHistoryEditor->setEnabled( FALSE );
	mHistoryEditor->setTabStop( TRUE );  // We want to be able to cut or copy from the history.
	mHistoryEditor->setParseHTML(true);
	addChild(mHistoryEditor);
}

void LLFloaterScriptDebugOutput::initFloater(const std::string& title, BOOL resizable, 
						S32 min_width, S32 min_height, BOOL drag_on_left,
						BOOL minimizable, BOOL close_btn)
{
	LLFloater::initFloater(title, resizable, min_width, min_height, drag_on_left, minimizable, close_btn);
	S32 y = getRect().getHeight() - LLFLOATER_HEADER_SIZE - LLFLOATER_VPAD;
	S32 x = LLFLOATER_HPAD;
	// History editor
	// Give it a border on the top
	LLRect history_editor_rect(
		x,
		y,
		getRect().getWidth() - LLFLOATER_HPAD,
				LLFLOATER_VPAD );
	mHistoryEditor = new LLViewerTextEditor( std::string("Chat History Editor"), 
										history_editor_rect, S32_MAX, LLStringUtil::null, LLFontGL::getFontSansSerif());
	mHistoryEditor->setWordWrap( TRUE );
	mHistoryEditor->setFollowsAll();
	mHistoryEditor->setEnabled( FALSE );
	mHistoryEditor->setTabStop( TRUE );  // We want to be able to cut or copy from the history.
	mHistoryEditor->setParseHTML(true);
	addChild(mHistoryEditor);
}

LLFloaterScriptDebugOutput::~LLFloaterScriptDebugOutput()
{
	sInstanceMap.erase(mObjectID);
}

void LLFloaterScriptDebugOutput::addLine(const LLChat& chat, std::string message, const LLColor4& color)
{
	if (!chat.mURL.empty())
	{
		message.erase(0, chat.mFromName.size());
		mHistoryEditor->appendText(chat.mURL, false, true);
	}
	LLStyleSP style(new LLStyle(true, color, LLStringUtil::null));
	mHistoryEditor->appendText(message, false, false, style, false);
}

//static
LLFloaterScriptDebugOutput* LLFloaterScriptDebugOutput::show(const LLUUID& object_id)
{
	LLFloaterScriptDebugOutput* floaterp = NULL;
	instance_map_t::iterator found_it = sInstanceMap.find(object_id);
	if (found_it == sInstanceMap.end())
	{
		floaterp = new LLFloaterScriptDebugOutput(object_id);
		sInstanceMap[object_id] = floaterp;
		floaterp->open();		/* Flawfinder: ignore*/

		if (object_id.isNull())
		{
			//floaterp->setTitle("[All scripts]");
			floaterp->setCanTearOff(FALSE);
			floaterp->setCanClose(FALSE);
		}
	}
	else
	{
		floaterp = found_it->second;
	}

	return floaterp;
}

//static 
LLFloaterScriptDebugOutput* LLFloaterScriptDebugOutput::getFloaterByID(const LLUUID& object_id)
{
	LLFloaterScriptDebugOutput* floaterp = NULL;
	instance_map_t::iterator found_it = sInstanceMap.find(object_id);
	if (found_it != sInstanceMap.end())
	{
		floaterp = found_it->second;
	}

	return floaterp;
}
