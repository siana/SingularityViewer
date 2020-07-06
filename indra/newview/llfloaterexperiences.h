/** 
 * @file llfloaterexperiences.h
 * @brief LLFloaterExperiences class definition
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#ifndef LL_LLFLOATEREXPERIENCES_H
#define LL_LLFLOATEREXPERIENCES_H

#include "llfloater.h"

class LLPanelExperiences;

class LLFloaterExperiences final :
	public LLFloater
,	public LLFloaterSingleton<LLFloaterExperiences>
{
	friend class LLUISingleton<LLFloaterExperiences, VisibilityPolicy<LLFloater> >;
public:
	LLFloaterExperiences(const LLSD& data);
	void onClose(bool app_quitting) override;

	void onOpen(/*const LLSD& key*/) override;

protected:
    typedef std::map<std::string, std::string> NameMap_t;
    typedef std::function<void(LLPanelExperiences*, const LLSD&)> Callback_t;

	void clearFromRecent(const LLSD& ids);
	void resizeToTabs();
	/*virtual*/ BOOL	postBuild() override;
	void refreshContents();
	void setupRecentTabs();
	LLPanelExperiences* addTab(const std::string& name, bool select);

	bool updatePermissions(const LLSD& permission);
	void sendPurchaseRequest();
	void checkPurchaseInfo(LLPanelExperiences* panel, const LLSD& content)const;
    void checkAndOpen(LLPanelExperiences* panel, const LLSD& content) const;
	void updateInfo(std::string experiences, std::string tab);

    void retrieveExperienceList(const std::string &url, const LLHandle<LLFloaterExperiences> &hparent, const NameMap_t &tabMapping,
        const std::string &errorNotify = std::string("ErrorMessage"), Callback_t cback = Callback_t(), bool post = false);

    void requestNewExperience(const std::string &url, const LLHandle<LLFloaterExperiences> &hparent, const NameMap_t &tabMapping,
        const std::string &errorNotify, Callback_t cback)
    {
    	retrieveExperienceList(url, hparent, tabMapping, errorNotify, cback, true);
    }

private:
    static void retrieveExperienceListCoro(const struct LLCoroResponder& responder, LLHandle<LLFloaterExperiences> hparent,
        NameMap_t tabMapping, std::string errorNotify, Callback_t cback);
    uuid_vec_t mPrepurchaseIds;
};

#endif //LL_LLFLOATEREXPERIENCES_H

