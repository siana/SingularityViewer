/** 
 * @file llpanelexperiences.cpp
 * @brief LLPanelExperiences class implementation
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


#include "llviewerprecompiledheaders.h"


#include "lluictrlfactory.h"
#include "llexperiencecache.h"
#include "llagent.h"

#include "llfloaterexperienceprofile.h"
#include "llpanelexperiences.h"
#include "lllayoutstack.h"
#include "llnamelistctrl.h"

//static LLPanelInjector<LLPanelExperiences> register_experiences_panel("experiences_panel");

LLPanelExperiences::LLPanelExperiences()
	: mExperiencesList(nullptr)
{
	//buildFromFile("panel_experiences.xml");
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_experiences.xml");
}

BOOL LLPanelExperiences::postBuild( void )
{
	mExperiencesList = getChild<LLNameListCtrl>("experiences_list");
	if (hasString("loading_experiences"))
	{
		mExperiencesList->setCommentText(getString("loading_experiences"));
	}
	else if (hasString("no_experiences"))
	{
		mExperiencesList->setCommentText(getString("no_experiences"));
	}

	return TRUE;
}

void addExperienceToList(const LLSD& experience, LLNameListCtrl* list)
{
	// Don't add missing experiences, that seems wrong
	if (experience.has(LLExperienceCache::MISSING) && experience[LLExperienceCache::MISSING].asBoolean())
		return;

	const auto& id = experience[LLExperienceCache::EXPERIENCE_ID];
	list->removeNameItem(id); // Don't add the same item twice, this can happen
	auto item = LLNameListCtrl::NameItem()
		.name(experience[LLExperienceCache::NAME].asString())
		.target(LLNameListItem::EXPERIENCE);
	item.value(id)
		.columns.add(LLScrollListCell::Params()); // Dummy column for names
	list->addNameItemRow(item);
}

void LLPanelExperiences::setExperienceList( const LLSD& experiences )
{
	mExperiencesList->setSortEnabled(false);

	if (hasString("no_experiences"))
	{
		mExperiencesList->setCommentText(getString("no_experiences"));
	}
	mExperiencesList->clear();

	auto& cache = LLExperienceCache::instance();
	for(const auto& exp : experiences.array())
	{
		LLUUID public_key = exp.asUUID();
		if (public_key.notNull())
			cache.get(public_key, boost::bind(addExperienceToList, _1, mExperiencesList));
	}

	mExperiencesList->setSortEnabled(true);
}

void LLPanelExperiences::getExperienceIdsList(uuid_vec_t& result)
{
	result = mExperiencesList->getAllIDs();
}

LLPanelExperiences* LLPanelExperiences::create(const std::string& name)
{
	LLPanelExperiences* panel= new LLPanelExperiences();
	panel->setName(name);
	return panel;
}

void LLPanelExperiences::removeExperiences( const LLSD& ids )
{
	for (const auto& id : ids.array())
	{
		removeExperience(id.asUUID());
	}
}

void LLPanelExperiences::removeExperience( const LLUUID& id )
{
	mExperiencesList->removeNameItem(id);
}

void LLPanelExperiences::addExperience( const LLUUID& id )
{
    if (!mExperiencesList->getItem(id))
    {
		LLExperienceCache::instance().get(id, boost::bind(addExperienceToList, _1, mExperiencesList));
    }
}

void LLPanelExperiences::setButtonAction(const std::string& label, const commit_signal_t::slot_type& cb )
{
	if(label.empty())
	{
		getChild<LLLayoutPanel>("button_panel")->setVisible(false);
	}
	else
	{
		getChild<LLLayoutPanel>("button_panel")->setVisible(true);
		LLButton* child = getChild<LLButton>("btn_action");
		child->setCommitCallback(cb);
		child->setLabel(getString(label));
	}
}

void LLPanelExperiences::enableButton( bool enable )
{
	getChild<LLButton>("btn_action")->setEnabled(enable);
}
