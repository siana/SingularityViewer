/** 
 * @file lleventnotifier.cpp
 * @brief Viewer code for managing event notifications
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "lleventnotifier.h"

#include "llnotificationsutil.h"
#include "message.h"

#include "llnotify.h"
#include "lleventinfo.h"
#include "llfloaterevent.h"
#include "llfloaterworldmap.h"
#include "llagent.h"
#include "llappviewer.h"	// for gPacificDaylightTime
#include "llcommandhandler.h"	// secondlife:///app/... support
#include "llviewercontrol.h"

class LLEventHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLEventHandler() : LLCommandHandler("event", UNTRUSTED_THROTTLE) { }
	bool handle(const LLSD& params, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (params.size() < 2)
		{
			return false;
		}
		std::string event_command = params[1].asString();
		S32 event_id = params[0].asInteger();
		if (event_command == "about" || event_command == "details")
		{
			LLFloaterEventInfo::show(event_id);
			return true;
		}
		else if(event_command == "notify")
		{
			// we're adding or removing a notification, so grab the date, name and notification bool
			if (params.size() < 3)
			{
				return false;
			}
			if(params[2].asString() == "enable")
			{
				gEventNotifier.add(event_id);
				// tell the server to modify the database as this was a slurl event notification command
				gEventNotifier.serverPushRequest(event_id, true);
			}
			else
			{
				gEventNotifier.remove(event_id);
			}
			return true;
		}

		return false;
	}
};
LLEventHandler gEventHandler;


LLEventNotifier gEventNotifier;

LLEventNotifier::LLEventNotifier()
{
}


LLEventNotifier::~LLEventNotifier()
{
	en_map::iterator iter;

	for (iter = mEventNotifications.begin();
		 iter != mEventNotifications.end();
		 iter++)
	{
		delete iter->second;
	}
}


void LLEventNotifier::update()
{
	if (mNotificationTimer.getElapsedTimeF32() > 30.f)
	{
		// Check our notifications again and send out updates
		// if they happen.

		time_t alert_time = time_corrected() + 5 * 60;
		en_map::iterator iter;
		for (iter = mEventNotifications.begin();
			 iter != mEventNotifications.end();)
		{
			LLEventNotification *np = iter->second;

			iter++;
			if (np->getEventDate() < (alert_time))
			{
				LLSD args;
				args["NAME"] = np->getEventName();
				args["DATE"] = np->getEventDateStr();
				LLNotificationsUtil::add("EventNotification", args, LLSD(),
					boost::bind(&LLEventNotifier::handleResponse, this, np->getEventID(), np->getEventPosGlobal(), _1, _2));
				remove(np->getEventID());
			}
		}
		mNotificationTimer.reset();
	}
}

bool LLEventNotifier::handleResponse(U32 eventId, LLVector3d eventPos, const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch (option)
	{
	case 0:
		gAgent.teleportViaLocation(eventPos);
		gFloaterWorldMap->trackLocation(eventPos);
		break;
	case 1:
		LLFloaterEventInfo::show(eventId);
		break;
	case 2:
		break;
	}

	// We could clean up the notification on the server now if we really wanted to.
	return true;
}

void LLEventNotifier::load(const LLSD& event_options)
{
	for(LLSD::array_const_iterator resp_it = event_options.beginArray(),
		end = event_options.endArray(); resp_it != end; ++resp_it)
	{
		LLSD response = *resp_it;
		LLEventNotification *new_enp = new LLEventNotification();

		if (!new_enp->load(response))
		{
			delete new_enp;
			continue;
		}
		
		mEventNotifications[new_enp->getEventID()] = new_enp;
	}
		
}

void LLEventNotifier::add(U32 eventId)
{
	
	gMessageSystem->newMessageFast(_PREHASH_EventInfoRequest);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );
	gMessageSystem->nextBlockFast(_PREHASH_EventData);
	gMessageSystem->addU32Fast(_PREHASH_EventID, eventId);
	gAgent.sendReliableMessage();

}

BOOL LLEventNotifier::hasNotification(const U32 event_id)
{
	if (mEventNotifications.find(event_id) != mEventNotifications.end())
	{
		return TRUE;
	}
	return FALSE;
}

void LLEventNotifier::add(LLEventInfo &event_info)
{
	// We need to tell the simulator that we want to pay attention to
	// this event, as well as add it to our list.

	if (mEventNotifications.find(event_info.mID) != mEventNotifications.end())
	{
		// We already have a notification for this event, don't bother.
		return;
	}

	serverPushRequest(event_info.mID, true);

	LLEventNotification *enp = new LLEventNotification;
	enp->load(event_info);
	mEventNotifications[event_info.mID] = enp;
}

void LLEventNotifier::remove(const U32 event_id)
{
	en_map::iterator iter;
	iter = mEventNotifications.find(event_id);
	if (iter == mEventNotifications.end())
	{
		// We don't have a notification for this event, don't bother.
		return;
	}

	serverPushRequest(event_id, false);
	delete iter->second;
	mEventNotifications.erase(iter);
}


void LLEventNotifier::serverPushRequest(U32 event_id, bool add)
{
	// Push up a message to tell the server we have this notification.
	gMessageSystem->newMessage(add?"EventNotificationAddRequest":"EventNotificationRemoveRequest");
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlock("EventData");
	gMessageSystem->addU32("EventID", event_id);
	gAgent.sendReliableMessage();
}

LLEventNotification::LLEventNotification() :
	mEventID(0),
	mEventName("")
{
}


LLEventNotification::~LLEventNotification()
{
}


BOOL LLEventNotification::load(const LLSD& response)
{
	BOOL event_ok = TRUE;
	LLSD event_id = response["event_id"];
	if (event_id.isDefined())
	{
		mEventID = event_id.asInteger();
	}
	else
	{
		event_ok = FALSE;
	}

	LLSD event_name = response["event_name"];
	if (event_name.isDefined())
	{
		mEventName = event_name.asString();
		LL_INFOS() << "Event: " << mEventName << LL_ENDL;
	}
	else
	{
		event_ok = FALSE;
	}
/*
	LLSD event_date = response["event_date"];
	if (event_date.isDefined())
	{
		mEventDate = event_date.asString();
		LL_INFOS() << "EventDate: " << mEventDate << LL_ENDL;
	}
	else
	{
		event_ok = FALSE;
	}
*/
	LLSD event_date_ut = response["event_date_ut"];
	if (event_date_ut.isDefined())
	{
		std::string date = event_date_ut.asString();
		LL_INFOS() << "EventDate: " << date << LL_ENDL;
		mEventDate = strtoul(date.c_str(), NULL, 10);

		// Convert to Pacific, based on server's opinion of whether
		// it's daylight savings time there.
		struct tm* t = utc_to_pacific_time(mEventDate, gPacificDaylightTime);
		timeStructToFormattedString(t, gSavedSettings.getString("TimestampFormat"), mEventDateStr);
		if (gPacificDaylightTime)
		{
			mEventDateStr += " PDT";
		}
		else
		{
			mEventDateStr += " PST";
		}
	}
	else
	{
		event_ok = FALSE;
	}

	S32 grid_x = 0;
	S32 grid_y = 0;
	S32 x_region = 0;
	S32 y_region = 0;

	LLSD grid_x_sd = response["grid_x"];
	if (grid_x_sd.isDefined())
	{
		grid_x= grid_x_sd.asInteger();
		LL_INFOS() << "GridX: " << grid_x << LL_ENDL;
	}
	else
	{
		event_ok = FALSE;
	}

	LLSD grid_y_sd = response["grid_y"];
	if (grid_y_sd.isDefined())
	{
		grid_y= grid_y_sd.asInteger();
		LL_INFOS() << "GridY: " << grid_y << LL_ENDL;
	}
	else
	{
		event_ok = FALSE;
	}

	LLSD x_region_sd = response["x_region"];
	if (x_region_sd.isDefined())
	{	
		x_region = x_region_sd.asInteger();
		LL_INFOS() << "RegionX: " << x_region << LL_ENDL;
	}
	else
	{
		event_ok = FALSE;
	}

	LLSD y_region_sd = response["y_region"];
	if (y_region_sd.isDefined())
	{
		y_region = y_region_sd.asInteger();
		LL_INFOS() << "RegionY: " << y_region << LL_ENDL;
	}
	else
	{
		event_ok = FALSE;
	}

	mEventPosGlobal.mdV[VX] = grid_x * 256 + x_region;
	mEventPosGlobal.mdV[VY] = grid_y * 256 + y_region;
	mEventPosGlobal.mdV[VZ] = 0.f;

	return event_ok;
}

BOOL LLEventNotification::load(const LLEventInfo &event_info)
{

	mEventID = event_info.mID;
	mEventName = event_info.mName;
	mEventDateStr = event_info.mTimeStr;
	mEventDate = event_info.mUnixTime;
	mEventPosGlobal = event_info.mPosGlobal;
	return TRUE;
}

