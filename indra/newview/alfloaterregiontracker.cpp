/**
* @file alfloaterregiontracker.cpp
* @brief Region tracking floater
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Alchemy Viewer Source Code
* Copyright (C) 2014, Alchemy Viewer Project.
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
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "alfloaterregiontracker.h"

// library
#include "llbutton.h"
#include "lldir.h"
#include "llfile.h"
#include "llscrolllistctrl.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsdserialize_xml.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

// newview
#include "hippogridmanager.h"
#include "llagent.h"
#include "llfloaterworldmap.h"
//#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llviewermessage.h"
#include "llworldmap.h"
#include "llworldmapmessage.h"

const std::string TRACKER_FILE = "tracked_regions.json";

ALFloaterRegionTracker::ALFloaterRegionTracker(const LLSD&)
	: LLFloater(),
	  LLEventTimer(5.f)
{
	LLUICtrlFactory::instance().buildFloater(this, "floater_region_tracker.xml");
	loadFromJSON();
}

ALFloaterRegionTracker::~ALFloaterRegionTracker()
{
	saveToJSON();
}

BOOL ALFloaterRegionTracker::postBuild()
{
	mRefreshRegionListBtn = getChild<LLButton>("refresh");
	mRefreshRegionListBtn->setClickedCallback(boost::bind(&ALFloaterRegionTracker::refresh, this));

	mRemoveRegionBtn = getChild<LLButton>("remove");
	mRemoveRegionBtn->setClickedCallback(boost::bind(&ALFloaterRegionTracker::removeRegions, this));

	mOpenMapBtn = getChild<LLButton>("open_map");
	mOpenMapBtn->setClickedCallback(boost::bind(&ALFloaterRegionTracker::openMap, this));

	mRegionScrollList = getChild<LLScrollListCtrl>("region_list");
	mRegionScrollList->setCommitOnSelectionChange(TRUE);
	mRegionScrollList->setCommitCallback(boost::bind(&ALFloaterRegionTracker::updateHeader, this));
	mRegionScrollList->setDoubleClickCallback(boost::bind(&ALFloaterRegionTracker::openMap, this));

	updateHeader();

	return LLFloater::postBuild();
}

void ALFloaterRegionTracker::onOpen(/*const LLSD& key*/)
{
	requestRegionData();
	mEventTimer.start();
}

void ALFloaterRegionTracker::onClose(bool app_quitting)
{
	mEventTimer.stop();
	app_quitting ? destroy() : setVisible(false);
}

void ALFloaterRegionTracker::updateHeader()
{
	S32 num_selected(mRegionScrollList->getNumSelected());
	mRefreshRegionListBtn->setEnabled(mRegionMap.size() != 0);
	mRemoveRegionBtn->setEnabled(!!num_selected);
	mOpenMapBtn->setEnabled(num_selected == 1);
}

void ALFloaterRegionTracker::refresh()
{
	if (!mRegionMap.size())
	{
		updateHeader();
		return;
	}

	std::vector<std::string> saved_selected_values;
	for(const auto* item : mRegionScrollList->getAllSelected())
	{
		saved_selected_values.push_back(item->getValue().asString());
	}
	S32 saved_scroll_pos = mRegionScrollList->getScrollPos();
	auto sort_column_name = mRegionScrollList->getSortColumnName();
	auto sort_asending = mRegionScrollList->getSortAscending();
	mRegionScrollList->deleteAllItems();

	const std::string& cur_region_name = gAgent.getRegion()->getName();

	for (LLSD::map_const_iterator it = mRegionMap.beginMap(); it != mRegionMap.endMap(); it++)
	{
		const std::string& sim_name = it->first;
		const LLSD& data = it->second;
		if (data.isMap()) // Assume the rest is correct.
		{
			LLScrollListCell::Params label;
			LLScrollListCell::Params maturity;
			LLScrollListCell::Params region;
			LLScrollListCell::Params count;
			label.column("region_label").type("text").value(data["label"].asString());
			maturity.column("region_maturity_icon").type("icon").font_halign(LLFontGL::HCENTER);
			region.column("region_name").type("text").value(sim_name);
			count.column("region_agent_count").type("text").value("...");
			if (LLSimInfo* info = LLWorldMap::getInstance()->simInfoFromName(sim_name))
			{
				maturity.value(info->getAccessIcon());
				maturity.tool_tip(info->getShortAccessString());

				info->updateAgentCount(LLTimer::getElapsedSeconds());
				S32 agent_count = info->getAgentCount();
				if (info->isDown())
				{
					label.color(LLColor4::red);
					maturity.color(LLColor4::red);
					region.color(LLColor4::red);
					count.color(LLColor4::red);
					count.value(0);
				}
				else
					count.value((sim_name == cur_region_name) ? agent_count + 1 : agent_count);
			}
			else
			{
				label.color(LLColor4::grey);
				maturity.color(LLColor4::grey);
				region.color(LLColor4::grey);
				count.color(LLColor4::grey);

				LLWorldMapMessage::getInstance()->sendNamedRegionRequest(sim_name);
				if (!mEventTimer.getStarted()) mEventTimer.start();
			}
			LLScrollListItem::Params row;
			row.value = sim_name;
			row.columns.add(label);
			row.columns.add(maturity);
			row.columns.add(region);
			row.columns.add(count);
			mRegionScrollList->addRow(row);
		}
	}

	mRegionScrollList->sortByColumn(sort_column_name, sort_asending);
	if (!saved_selected_values.empty())
		mRegionScrollList->selectMultiple(saved_selected_values);
	mRegionScrollList->setScrollPos(saved_scroll_pos);
}

BOOL ALFloaterRegionTracker::tick()
{
	refresh();
	return FALSE;
}

void ALFloaterRegionTracker::requestRegionData()
{
	if (!mRegionMap.size())
		return;

	for (LLSD::map_const_iterator it = mRegionMap.beginMap(); it != mRegionMap.endMap(); it++)
	{
		const auto& name = it->first;
		if (LLSimInfo* info = LLWorldMap::getInstance()->simInfoFromName(name))
		{
			info->updateAgentCount(LLTimer::getElapsedSeconds());
		}
		else
		{
			LLWorldMapMessage::getInstance()->sendNamedRegionRequest(name);
		}
	}
	mEventTimer.start();
}

void ALFloaterRegionTracker::removeRegions()
{
	for (const auto* item : mRegionScrollList->getAllSelected())
	{
		mRegionMap.erase(item->getValue().asString());
	}
	mRegionScrollList->deleteSelectedItems();
	saveToJSON();
	updateHeader();
}

std::string getGridSpecificFile(const std::string& file, const char& sep = '_')
{
	const HippoGridInfo& grid(*gHippoGridManager->getConnectedGrid());
	if (grid.isSecondLife()) return file;
	return file + sep + grid.getGridNick();
}

bool ALFloaterRegionTracker::saveToJSON()
{
	const std::string& filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, TRACKER_FILE);
	llofstream out_file;
	out_file.open(getGridSpecificFile(filename));
	if (out_file.is_open())
	{
		LLSDSerialize::toPrettyNotation(mRegionMap, out_file);
		out_file.close();
		return true;
	}
	return false;
}

bool ALFloaterRegionTracker::loadFromJSON()
{
	const std::string& filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, TRACKER_FILE);
	llifstream in_file;
	in_file.open(getGridSpecificFile(filename));
	if (in_file.is_open())
	{
		LLSDSerialize::fromNotation(mRegionMap, in_file, LLSDSerialize::SIZE_UNLIMITED);
		in_file.close();
		return true;
	}
	return false;
}

std::string ALFloaterRegionTracker::getRegionLabelIfExists(const std::string& name)
{
	return mRegionMap.get(name)["label"].asString();
}

void ALFloaterRegionTracker::onRegionAddedCallback(const LLSD& notification, const LLSD& response)
{
	const S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		const std::string& name = notification["payload"]["name"].asString();
		std::string label = response["label"].asString();
		LLStringUtil::trim(label);
		if (!name.empty() && !label.empty())
		{
			if (mRegionMap.has(name))
			{
				for (LLSD::map_iterator it = mRegionMap.beginMap(); it != mRegionMap.endMap(); it++)
					if (it->first == name) it->second["label"] = label;
			}
			else
			{
				LLSD region;
				region["label"] = label;
				mRegionMap.insert(name, region);
			}
			saveToJSON();
			refresh();
		}
	}
}

void ALFloaterRegionTracker::openMap()
{
	const std::string& region = mRegionScrollList->getFirstSelected()->getValue().asString();
	LLFloaterWorldMap* worldmap_floaterp = gFloaterWorldMap;
	if (!region.empty() && worldmap_floaterp)
	{
		worldmap_floaterp->trackURL(region, 128, 128, 0);
		worldmap_floaterp->show(true);
	}
}
