/** 
 * @file llfloatergroupinfo.h
 * @brief LLFloaterGroupInfo class definition
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
 * Floater used both for display of group information and for
 * creating new groups.
 */

#ifndef LL_LLFLOATERGROUPINFO_H
#define LL_LLFLOATERGROUPINFO_H

#include "llfloater.h"
#include "llinstancetracker.h"

class LLPanelGroup;

class LLFloaterGroupInfo
: public LLFloater, public LLInstanceTracker<LLFloaterGroupInfo, LLUUID>
{
public:
	LLFloaterGroupInfo(const LLUUID& group_id);
	virtual ~LLFloaterGroupInfo();

	void selectTabByName(std::string tab_name);

	// This allows us to block the user from closing the floater if there is information that needs to be applied.
	virtual BOOL canClose();

protected:
	friend class LLGroupActions;
	LLPanelGroup*	mPanelGroupp;

private:
	void callbackLoadGroupName(const std::string& full_name);
};

#endif

