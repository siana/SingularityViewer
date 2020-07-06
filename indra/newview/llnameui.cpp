/**
 * @file llnameui.cpp
 * @brief Name UI refreshes a name and bears a menu for interacting with it
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc. 2019, Liru Færs
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

#include "llnameui.h"

#include "llagentdata.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llexperiencecache.h"
#include "llfloaterexperienceprofile.h"
#include "llgroupactions.h"
#include "lltrans.h"

#include "rlvactions.h"
#include "rlvcommon.h"

// statics
std::set<LLNameUI*> LLNameUI::sInstances;

LLNameUI::LLNameUI(const std::string& loading, bool rlv_sensitive, const LLUUID& id, const Type& type, const std::string& name_system, bool click_for_profile)
: mNameID(id), mRLVSensitive(rlv_sensitive), mType(NONE), mAllowInteract(false)
, mNameSystem(name_system.empty() ? "PhoenixNameSystem" : name_system), mInitialValue(!loading.empty() ? loading : LLTrans::getString("LoadingData"))
, mClickForProfile(click_for_profile)
{
	setType(type);
}

void LLNameUI::setType(const Type& type)
{
	// Disconnect active connections if needed
	for (auto& connection : mConnections)
		connection.disconnect();

	if (mType != type)
	{
		if (type == GROUP)
			sInstances.insert(this);
		else
		{
			sInstances.erase(this);
			if (type == AVATAR)
				mConnections[1] = gSavedSettings.getControl(mNameSystem)->getCommitSignal()->connect(boost::bind(&LLNameUI::setNameText, this));
		}
		mType = type;
	}
}

void LLNameUI::setNameID(const LLUUID& name_id, const Type& type)
{
	mNameID = name_id;
	setType(type);

	if (mAllowInteract = mNameID.notNull())
	{
		setNameText();
	}
	else
	{
		setText(LLTrans::getString(mType == GROUP ? "GroupNameNone" :
			mType == AVATAR ? "AvatarNameNobody" : "ExperienceNameNull"));
		displayAsLink(false);
	}
}

void LLNameUI::setNameText()
{
	std::string name;
	bool got_name = false;

	if (mType == GROUP)
	{
		got_name = gCacheName->getGroupName(mNameID, name);
	}
	else if (mType == EXPERIENCE)
	{
		auto& cache = LLExperienceCache::instance();
		const auto& exp = cache.get(mNameID);
		if (got_name = exp.isMap())
			name = exp.has(LLExperienceCache::MISSING) && exp[LLExperienceCache::MISSING] ? LLTrans::getString("ExperienceNameNull") : exp[LLExperienceCache::NAME].asString();
		else
			cache.get(mNameID, boost::bind(&LLNameUI::setNameText, this));
	}
	else
	{
		LLAvatarName av_name;
		if (got_name = LLAvatarNameCache::get(mNameID, &av_name))
			name = mNameSystem.empty() ? av_name.getNSName() : av_name.getNSName(gSavedSettings.getS32(mNameSystem));
		else
			mConnections[0] = LLAvatarNameCache::get(mNameID, boost::bind(&LLNameUI::setNameText, this));
	}

	if (mType == AVATAR && got_name && mRLVSensitive) // Filter if needed
	{
		if ((RlvActions::hasBehaviour(RLV_BHVR_SHOWNAMES) || RlvActions::hasBehaviour(RLV_BHVR_SHOWNAMETAGS))
			&& mNameID != gAgentID && RlvUtil::isNearbyAgent(mNameID))
		{
			mAllowInteract = false;
			name = RlvStrings::getAnonym(name);
		}
		else mAllowInteract = true;
	}

	displayAsLink(mAllowInteract);

	// Got the name already? Set it.
	// Otherwise it will be set later in refresh().
	setText(got_name ? name : mInitialValue);
}

void LLNameUI::refresh(const LLUUID& id, const std::string& full_name)
{
	if (id == mNameID)
	{
		setNameText();
	}
}

void LLNameUI::refreshAll(const LLUUID& id, const std::string& full_name)
{
	for (auto box : sInstances)
	{
		box->refresh(id, full_name);
	}
}

void LLNameUI::showProfile()
{
	if (!mAllowInteract) return;

	switch (mType)
	{
	case LFIDBearer::GROUP: LLGroupActions::show(mNameID); break;
	case LFIDBearer::AVATAR: LLAvatarActions::showProfile(mNameID); break;
	case LFIDBearer::EXPERIENCE: LLFloaterExperienceProfile::showInstance(mNameID); break;
	default: break;
	}
}
