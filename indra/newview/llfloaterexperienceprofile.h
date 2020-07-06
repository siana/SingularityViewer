/** 
 * @file llfloaterexperienceprofile.h
 * @brief llfloaterexperienceprofile and related class definitions
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2013, Linden Research, Inc.
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



#ifndef LL_LLFLOATEREXPERIENCEPROFILE_H
#define LL_LLFLOATEREXPERIENCEPROFILE_H

#include "llfloater.h"
#include "lluuid.h"
#include "llsd.h"

class LLLayoutPanel;
class LLTextBox;
class LLComboBox;

class LLFloaterExperienceProfile final : public LLFloater
, public LLInstanceTracker<LLFloaterExperienceProfile, LLUUID>
{
	LOG_CLASS(LLFloaterExperienceProfile);
public:
	enum PostSaveAction
	{
		NOTHING,
		CLOSE,
		VIEW,
	};

    static void experiencePermissionResults(const LLUUID& exprienceId, const LLSD& result);

	static void showInstance(const LLSD& data);
	LLFloaterExperienceProfile(const LLSD& data);
	virtual ~LLFloaterExperienceProfile();

	LLUUID getExperienceId() const { return mExperienceId; }
	void setPreferences( const LLSD& content );


	void refreshExperience(const LLSD& experience);
	void onSaveComplete( const LLSD& content );
	BOOL canClose() override;

	void onClose(bool app_quitting) override;
protected:
	void onClickEdit();
	void onClickPermission(const char* permission);
	void onClickForget();
	void onClickCancel();
	void onClickSave();
	void onClickLocation();
	void onClickClear();
	void onPickGroup();
	void onFieldChanged();
	void onReportExperience();

	void setEditGroup(LLUUID group_id);

	void changeToView();
    void changeToEdit();

	void experienceForgotten();
	void experienceBlocked();
	void experienceAllowed();

	static void experienceCallback(LLHandle<LLFloaterExperienceProfile> handle, const LLSD& experience);
	static bool experiencePermission(LLHandle<LLFloaterExperienceProfile> handle, const LLSD& permission);

    BOOL postBuild() override;
	bool setMaturityString(U8 maturity, LLTextBox* child, LLComboBox* combo);
	bool handleSaveChangesDialog(const LLSD& notification, const LLSD& response, PostSaveAction action);
	void doSave( int success_action );

	void updatePackage();

	void updatePermission( const LLSD& permission );
	LLUUID mExperienceId;
	LLSD mExperienceDetails;
	LLSD mPackage;
	std::string mLocationSLURL;
	int mSaveCompleteAction;
	bool mDirty;
	bool mForceClose;
    bool mPostEdit; // edit experience after opening and updating it
private:
    static bool hasPermission(const LLSD& content, const std::string &name, const LLUUID &test);
    static void experienceIsAdmin(LLHandle<LLFloaterExperienceProfile> handle, const LLSD &result);
    static void experienceUpdateResult(LLHandle<LLFloaterExperienceProfile> handle, const LLSD &result);
};

#endif // LL_LLFLOATEREXPERIENCEPROFILE_H
