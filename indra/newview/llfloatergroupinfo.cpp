/** 
 * @file llfloatergroupinfo.cpp
 * @brief LLFloaterGroupInfo class implementation
 * Floater used both for display of group information and for
 * creating new groups.
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

#include "llviewerprecompiledheaders.h"

#include "llfloatergroupinfo.h"

#include "llcachename.h"
#include "llpanelgroup.h"

const char FLOATER_TITLE[] = "Group Information";

//-----------------------------------------------------------------------------
// Implementation
//-----------------------------------------------------------------------------
LLFloaterGroupInfo::LLFloaterGroupInfo(const LLUUID& group_id)
:	LLFloater(FLOATER_TITLE, LLRect(0, 530, 420, 0), FLOATER_TITLE), LLInstanceTracker<LLFloaterGroupInfo, LLUUID>(group_id)
{
	// Construct the filename of the group panel xml definition file.
	mPanelGroupp = new LLPanelGroup(group_id);
	addChild(mPanelGroupp);
	if (group_id.notNull())
	{
		// Look up the group name. The callback will insert it into the title.
		gCacheName->get(group_id, true, boost::bind(&LLFloaterGroupInfo::callbackLoadGroupName, this, _2));
	}
}

// virtual
LLFloaterGroupInfo::~LLFloaterGroupInfo()
{
}

BOOL LLFloaterGroupInfo::canClose()
{
	// Ask the panel if it is ok to close.
	if ( mPanelGroupp )
	{
		return mPanelGroupp->canClose();
	}
	return TRUE;
}

void LLFloaterGroupInfo::selectTabByName(std::string tab_name)
{
	mPanelGroupp->selectTab(tab_name);
}

void LLFloaterGroupInfo::callbackLoadGroupName(const std::string& full_name)
{
	// Build a new title including the group name.
	std::ostringstream title;
	title << full_name << " - " << FLOATER_TITLE;
	setTitle(title.str());
}

