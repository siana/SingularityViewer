/** 
 * @file llfloatertopobjects.cpp
 * @brief Shows top colliders, top scripts, etc.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloatertopobjects.h"

// library includes
#include "message.h"
#include "llfontgl.h"

#include "llagent.h"
#include "llbutton.h"
#include "llfloatergodtools.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "lltracker.h"
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llagentcamera.h"
#include "llviewerobjectlist.h"

#include "llavataractions.h"

//LLFloaterTopObjects* LLFloaterTopObjects::sInstance = NULL;

// Globals
// const U32 TIME_STR_LENGTH = 30;
/*
// static
void LLFloaterTopObjects::show()
{
	if (sInstance)
	{
		sInstance->setVisibleAndFrontmost();
		return;
	}

	sInstance = new LLFloaterTopObjects();
	sInstance->center();
}
*/
LLFloaterTopObjects::LLFloaterTopObjects()
:	LLFloater(std::string("top_objects")),
	mInitialized(FALSE),
	mtotalScore(0.f)
{
	mCommitCallbackRegistrar.add("TopObjects.ShowBeacon",		boost::bind(&LLFloaterTopObjects::onClickShowBeacon, this));
	mCommitCallbackRegistrar.add("TopObjects.ReturnSelected",	boost::bind(&LLFloaterTopObjects::onReturnSelected, this));
	mCommitCallbackRegistrar.add("TopObjects.ReturnAll",		boost::bind(&LLFloaterTopObjects::onReturnAll, this));
	mCommitCallbackRegistrar.add("TopObjects.DisableSelected",	boost::bind(&LLFloaterTopObjects::onDisableSelected, this));
	mCommitCallbackRegistrar.add("TopObjects.DisableAll",		boost::bind(&LLFloaterTopObjects::onDisableAll, this));
	mCommitCallbackRegistrar.add("TopObjects.Refresh",			boost::bind(&LLFloaterTopObjects::onRefresh, this));
	mCommitCallbackRegistrar.add("TopObjects.GetByObjectName",	boost::bind(&LLFloaterTopObjects::onGetByObjectName, this));
	mCommitCallbackRegistrar.add("TopObjects.GetByOwnerName",	boost::bind(&LLFloaterTopObjects::onGetByOwnerName, this));
	mCommitCallbackRegistrar.add("TopObjects.GetByParcelName",	boost::bind(&LLFloaterTopObjects::onGetByParcelName, this));
	mCommitCallbackRegistrar.add("TopObjects.CommitObjectsList",boost::bind(&LLFloaterTopObjects::onCommitObjectsList, this));

	mCommitCallbackRegistrar.add("TopObjects.TeleportToObject",	boost::bind(&LLFloaterTopObjects::onTeleportToObject, this));
	mCommitCallbackRegistrar.add("TopObjects.CamToObject",		boost::bind(&LLFloaterTopObjects::onCamToObject, this));
	mCommitCallbackRegistrar.add("TopObjects.Kick",				boost::bind(&LLFloaterTopObjects::onKick, this));
	mCommitCallbackRegistrar.add("TopObjects.Profile",			boost::bind(&LLFloaterTopObjects::onProfile, this));

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_top_objects.xml");
}

LLFloaterTopObjects::~LLFloaterTopObjects()
{
}

// virtual
BOOL LLFloaterTopObjects::postBuild()
{
	LLScrollListCtrl *objects_list = getChild<LLScrollListCtrl>("objects_list");
	getChild<LLUICtrl>("objects_list")->setFocus(TRUE);
	objects_list->setDoubleClickCallback(onDoubleClickObjectsList, this);
	objects_list->setCommitOnSelectionChange(TRUE);

	setDefaultBtn("show_beacon_btn");

	mCurrentMode = STAT_REPORT_TOP_SCRIPTS;
	mFlags = 0;
	mFilter.clear();
	center();

	return TRUE;
}
// static
void LLFloaterTopObjects::setMode(U32 mode)
{
	LLFloaterTopObjects* instance = instanceExists() ? getInstance() : NULL;
	if(!instance) return;
	instance->mCurrentMode = mode;
}

// static
void LLFloaterTopObjects::handle_land_reply(LLMessageSystem* msg, void** data)
{
	LLFloaterTopObjects* instance = instanceExists() ? getInstance() : NULL;
	if(!instance) return;
	// Make sure dialog is on screen
	instance->open();
	instance->handleReply(msg, data);

	//HACK: for some reason sometimes top scripts originally comes back
	//with no results even though they're there
	if (!instance->mObjectListIDs.size() && !instance->mInitialized)
	{
		instance->onRefresh();
		instance->mInitialized = TRUE;
	}

}

void LLFloaterTopObjects::handleReply(LLMessageSystem *msg, void** data)
{
	U32 request_flags;
	U32 total_count;

	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_RequestFlags, request_flags);
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_TotalObjectCount, total_count);
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_ReportType, mCurrentMode);

	LLScrollListCtrl *list = getChild<LLScrollListCtrl>("objects_list");

	S32 block_count = msg->getNumberOfBlocks("ReportData");
	for (S32 block = 0; block < block_count; ++block)
	{
		U32 task_local_id;
		U32 time_stamp = 0;
		LLUUID task_id;
		F32 location_x, location_y, location_z;
		F32 score;
		std::string name_buf;
		std::string owner_buf;
		std::string parcel_buf("unknown");
		F32 mono_score = 0.f;
		bool have_extended_data = false;
		S32 public_urls = 0;
		F32 script_memory = 0.f;

		msg->getU32Fast(_PREHASH_ReportData, _PREHASH_TaskLocalID, task_local_id, block);
		msg->getUUIDFast(_PREHASH_ReportData, _PREHASH_TaskID, task_id, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationX, location_x, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationY, location_y, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationZ, location_z, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_Score, score, block);
		msg->getStringFast(_PREHASH_ReportData, _PREHASH_TaskName, name_buf, block);
		msg->getStringFast(_PREHASH_ReportData, _PREHASH_OwnerName, owner_buf, block);

		if(msg->has("DataExtended"))
		{
			have_extended_data = true;
			msg->getU32("DataExtended", "TimeStamp", time_stamp, block);
			msg->getF32("DataExtended", "MonoScore", mono_score, block);
			msg->getS32("DataExtended", "PublicURLs", public_urls, block);
			if (msg->getSize("DataExtended", "ParcelName") > 0)
			{
				msg->getString("DataExtended", "ParcelName", parcel_buf, block);
				msg->getF32("DataExtended", "Size", script_memory, block);
			}
		}

		LLSD element;

		element["id"] = task_id;

		LLSD columns;
		S32 column_num = 0;
		columns[column_num]["column"] = "score";
		columns[column_num]["value"] = llformat("%0.3f", score);
		columns[column_num++]["font"] = "SANSSERIF";
		
		columns[column_num]["column"] = "name";
		columns[column_num]["value"] = name_buf;
		columns[column_num++]["font"] = "SANSSERIF";

		columns[column_num]["column"] = "owner";
		columns[column_num]["value"] = owner_buf;
		columns[column_num++]["font"] = "SANSSERIF";

		columns[column_num]["column"] = "location";
		columns[column_num]["value"] = llformat("<%0.1f,%0.1f,%0.1f>", location_x, location_y, location_z);
		columns[column_num++]["font"] = "SANSSERIF";

		columns[column_num]["column"] = "parcel";
		columns[column_num]["value"] = parcel_buf;
		columns[column_num++]["font"] = "SANSSERIF";

		columns[column_num]["column"] = "time";
		columns[column_num]["value"] = formatted_time((time_t)time_stamp);
		columns[column_num++]["font"] = "SANSSERIF";

		if (mCurrentMode == STAT_REPORT_TOP_SCRIPTS
			&& have_extended_data)
		{
			columns[column_num]["column"] = "memory";
			columns[column_num]["value"] = llformat("%0.0f", (script_memory / 1000.f));
			columns[column_num++]["font"] = "SANSSERIF";

			columns[column_num]["column"] = "URLs";
			columns[column_num]["value"] = llformat("%d", public_urls);
			columns[column_num++]["font"] = "SANSSERIF";
		}
		element["columns"] = columns;
		// Singu Note: We diverge slightly here to retain the legacy coloring of avatar names. when updating be sure that getColumn uses the same number as "name"
		LLScrollListItem* item =
		list->addElement(element);
		if (name_buf == owner_buf) item->getColumn(1)->setColor(LLColor4::red);
		
		mObjectListData.append(element);
		mObjectListIDs.push_back(task_id);

		mtotalScore += score;
	}

	if (total_count == 0 && list->getItemCount() == 0)
	{
		list->setCommentText(getString("none_descriptor"));
	}
	else
	{
		list->selectFirstItem();
	}

	if (mCurrentMode == STAT_REPORT_TOP_SCRIPTS)
	{
		setTitle(getString("top_scripts_title"));
		list->setColumnLabel("score", getString("scripts_score_label"));
		
		LLUIString format = getString("top_scripts_text");
		format.setArg("[COUNT]", llformat("%d", total_count));
		format.setArg("[TIME]", llformat("%0.3f", mtotalScore));
		getChild<LLUICtrl>("title_text")->setValue(LLSD(format));
	}
	else
	{
		setTitle(getString("top_colliders_title"));
		list->setColumnLabel("score", getString("colliders_score_label"));
		list->setColumnLabel("URLs", "");
		list->setColumnLabel("memory", "");
		LLUIString format = getString("top_colliders_text");
		format.setArg("[COUNT]", llformat("%d", total_count));
		getChild<LLUICtrl>("title_text")->setValue(LLSD(format));
	}
}

void LLFloaterTopObjects::onCommitObjectsList()
{
	updateSelectionInfo();
}

void LLFloaterTopObjects::updateSelectionInfo()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");

	if (!list) return;

	LLUUID object_id = list->getCurrentID();
	if (object_id.isNull()) return;

	std::string object_id_string = object_id.asString();

	getChild<LLUICtrl>("id_editor")->setValue(LLSD(object_id_string));
	LLScrollListItem* sli = list->getFirstSelected();
	llassert(sli);
	if (sli)
	{
		getChild<LLUICtrl>("object_name_editor")->setValue(sli->getColumn(1)->getValue().asString());
		getChild<LLUICtrl>("owner_name_editor")->setValue(sli->getColumn(2)->getValue().asString());
		getChild<LLUICtrl>("parcel_name_editor")->setValue(sli->getColumn(4)->getValue().asString());
	}
}

// static
void LLFloaterTopObjects::onDoubleClickObjectsList(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	self->showBeacon();
}

// static
void LLFloaterTopObjects::onClickShowBeacon()
{
	showBeacon();
}

void LLFloaterTopObjects::doToObjects(int action, bool all)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = gAgent.getRegion();
	if (!region) return;

	const auto list = getChild<LLScrollListCtrl>("objects_list");
	if (!list || list->getItemCount() == 0) return;

	bool start_message = true;
	auto func = [&](const LLUUID& task_id){
		if (start_message)
		{
			if (action == ACTION_RETURN)
			{
				msg->newMessageFast(_PREHASH_ParcelReturnObjects);
			}
			else
			{
				msg->newMessageFast(_PREHASH_ParcelDisableObjects);
			}
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
			msg->nextBlockFast(_PREHASH_ParcelData);
			msg->addS32Fast(_PREHASH_LocalID, -1); // Whole region
			msg->addS32Fast(_PREHASH_ReturnType, RT_NONE);
			msg->nextBlockFast(_PREHASH_TaskIDs);
			start_message = false;
		}
		msg->addUUIDFast(_PREHASH_TaskID, task_id);

		if (msg->isSendFullFast(_PREHASH_TaskIDs))
		{
			msg->sendReliable(region->getHost());
			start_message = true;
		}
	};

	if (all) for (const auto& id : mObjectListIDs) func(id);
	else for (const auto& item : list->getAllSelected()) func(item->getUUID());

	if (!start_message)
	{
		msg->sendReliable(region->getHost());
	}
}

//static
bool LLFloaterTopObjects::callbackReturnAll(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLFloaterTopObjects* instance = instanceExists() ? getInstance() : NULL;
	if(!instance) return false;
	if (option == 0)
	{
		instance->doToObjects(ACTION_RETURN, true);
	}
	return false;
}

void LLFloaterTopObjects::onReturnAll()
{	
	LLNotificationsUtil::add("ReturnAllTopObjects", LLSD(), LLSD(), &callbackReturnAll);
}


void LLFloaterTopObjects::onReturnSelected()
{
	doToObjects(ACTION_RETURN, false);
}


//static
bool LLFloaterTopObjects::callbackDisableAll(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLFloaterTopObjects* instance = instanceExists() ? getInstance() : NULL;
	if(!instance) return false;
	if (option == 0)
	{
		instance->doToObjects(ACTION_DISABLE, true);
	}
	return false;
}

void LLFloaterTopObjects::onDisableAll()
{
	LLNotificationsUtil::add("DisableAllTopObjects", LLSD(), LLSD(), callbackDisableAll);
}

void LLFloaterTopObjects::onDisableSelected()
{
	doToObjects(ACTION_DISABLE, false);
}


void LLFloaterTopObjects::clearList()
{
	LLCtrlListInterface *list = childGetListInterface("objects_list");
	
	if (list) 
	{
		list->operateOnAll(LLCtrlListInterface::OP_DELETE);
	}

	mObjectListData.clear();
	mObjectListIDs.clear();
	mtotalScore = 0.f;
}


void LLFloaterTopObjects::onRefresh()
{
	U32 mode = STAT_REPORT_TOP_SCRIPTS;
	U32 flags = 0;
	std::string filter = "";

	mode   = mCurrentMode;
	flags  = mFlags;
	filter = mFilter;
	clearList();

	LLMessageSystem *msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_LandStatRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );
	msg->nextBlockFast(_PREHASH_RequestData);
	msg->addU32Fast(_PREHASH_ReportType, mode);
	msg->addU32Fast(_PREHASH_RequestFlags, flags);
	msg->addStringFast(_PREHASH_Filter, filter);
	msg->addS32Fast(_PREHASH_ParcelLocalID, 0);

	msg->sendReliable(gAgent.getRegionHost());

	mFilter.clear();
	mFlags = 0;
}

void LLFloaterTopObjects::onGetByObjectName()
{
	mFlags  = STAT_FILTER_BY_OBJECT;
	mFilter = getChild<LLUICtrl>("object_name_editor")->getValue().asString();
	onRefresh();
}

void LLFloaterTopObjects::onGetByOwnerName()
{
	mFlags  = STAT_FILTER_BY_OWNER;
	mFilter = getChild<LLUICtrl>("owner_name_editor")->getValue().asString();
	onRefresh();
}


void LLFloaterTopObjects::onGetByParcelName()
{
	mFlags  = STAT_FILTER_BY_PARCEL_NAME;
	mFilter = getChild<LLUICtrl>("parcel_name_editor")->getValue().asString();
	onRefresh();
}


void LLFloaterTopObjects::showBeacon()
{
	LLVector3d pos_global = getSelectedPosition();
	if (pos_global.isExactlyZero()) return;
	LLScrollListItem* first_selected = getChild<LLScrollListCtrl>("objects_list")->getFirstSelected();
	std::string name = first_selected->getColumn(1)->getValue().asString();
	LLTracker::trackLocation(pos_global, name, LLStringUtil::null, LLTracker::LOCATION_ITEM);

	const LLUUID& taskid = first_selected->getUUID();
	if(LLVOAvatar* voavatar = gObjectList.findAvatar(taskid))
	{
		gAgentCamera.setFocusOnAvatar(FALSE, FALSE);
		gAgentCamera.changeCameraToThirdPerson();
		gAgentCamera.setFocusGlobal(voavatar->getPositionGlobal(),taskid);
		gAgentCamera.setCameraPosAndFocusGlobal(voavatar->getPositionGlobal() + LLVector3d(3.5,1.35,0.75) * voavatar->getRotation(), voavatar->getPositionGlobal(), taskid);
	}
}

LLVector3d LLFloaterTopObjects::getSelectedPosition() const
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
	if (!list) return LLVector3d::zero;

	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return LLVector3d::zero;

	std::string pos_string =  first_selected->getColumn(3)->getValue().asString();

	F32 x, y, z;
	S32 matched = sscanf(pos_string.c_str(), "<%g,%g,%g>", &x, &y, &z);
	if (matched != 3) return LLVector3d::zero;

	LLVector3 pos_agent(x, y, z);
	return gAgent.getPosGlobalFromAgent(pos_agent);
}

void LLFloaterTopObjects::onTeleportToObject()
{
	LLVector3d pos_global = getSelectedPosition();
	if (pos_global.isExactlyZero()) return;
	gAgent.teleportViaLocation( pos_global );
}

void LLFloaterTopObjects::onCamToObject()
{
	LLVector3d pos_global = getSelectedPosition();
	if (pos_global.isExactlyZero()) return;
	const LLUUID& id = getChild<LLScrollListCtrl>("objects_list")->getFirstSelected()->getUUID();
	gAgentCamera.setCameraPosAndFocusGlobal(pos_global + LLVector3d(3.5,1.35,0.75), pos_global, id);
}

void LLFloaterTopObjects::onKick()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
		if (!list) return;

	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;

	const LLUUID& objectId = first_selected->getUUID();
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("EstateOwnerMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", "kickestate");
	msg->addUUID("Invoice", LLUUID::null);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", objectId.asString().c_str());
	msg->sendReliable(gAgent.getRegionHost());
}

void LLFloaterTopObjects::onProfile()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
		if (!list) return;

	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;

	const LLUUID& objectId = first_selected->getUUID();
	LLAvatarActions::showProfile(objectId);
}
