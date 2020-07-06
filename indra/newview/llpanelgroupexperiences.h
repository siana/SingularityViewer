/** 
 * @file llpanelgroupexperiences.h
 * @brief List of experiences owned by a group.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#ifndef LL_LLPANELGROUPEXPERIENCES_H
#define LL_LLPANELGROUPEXPERIENCES_H

#include "llpanelgroup.h"

class LLPanelGroupExperiences final : public LLPanelGroupTab
{
public:
	static void* createTab(void* data);
	LLPanelGroupExperiences(const std::string& name, const LLUUID& id);
	virtual ~LLPanelGroupExperiences();

	// LLPanelGroupTab
	void activate() override;
	BOOL isVisibleByAgent(LLAgent* agentp) override;
	
	BOOL postBuild() override;
	
	void setGroupID(const LLUUID& id) override;
	
	void setExperienceList(const LLSD& experiences);

protected:
	class LLNameListCtrl* mExperiencesList;

private:
    static void groupExperiencesResults(LLHandle<LLPanelGroupExperiences>, const LLSD &);
};

#endif
