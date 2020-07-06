/**
 * @file llurldispatcher.cpp
 * @brief Central registry for all URL handlers
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llurldispatcher.h"

// viewer includes
#include "llagent.h"			// teleportViaLocation()
#include "llcommandhandler.h"
#include "llfloaterurldisplay.h"
#include "llfloaterdirectory.h"
#include "llfloaterworldmap.h"
#include "llpanellogin.h"
#include "llregionhandle.h"
#include "llslurl.h"
#include "llstartup.h"			// gStartupState
#include "llweb.h"
#include "llworldmap.h"
#include "llworldmapmessage.h"
#include "llviewernetwork.h"

#include "hippogridmanager.h"

// library includes
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llsd.h"

class LLURLDispatcherImpl
{
public:
	static bool dispatch(const LLSLURL& slurl,
						 const std::string& nav_type,
						 LLMediaCtrl* web,
						 bool trusted_browser);
		// returns true if handled or explicitly blocked.

	static bool dispatchRightClick(const LLSLURL& slurl);

private:
	static bool dispatchCore(const LLSLURL& slurl, 
							 const std::string& nav_type,
							 bool right_mouse,
							 LLMediaCtrl* web,
							 bool trusted_browser);
		// handles both left and right click

	static bool dispatchHelp(const LLSLURL& slurl, bool right_mouse);
		// Handles sl://app.floater.html.help by showing Help floater.
		// Returns true if handled.

	static bool dispatchApp(const LLSLURL& slurl,
							const std::string& nav_type,
							bool right_mouse,
							LLMediaCtrl* web,
							bool trusted_browser);
		// Handles secondlife:///app/agent/<agent_id>/about and similar
		// by showing panel in Search floater.
		// Returns true if handled or explicitly blocked.

	static bool dispatchRegion(const LLSLURL& slurl, const std::string& nav_type, bool right_mouse);
		// handles secondlife://Ahern/123/45/67/
		// Returns true if handled.

	static void regionHandleCallback(U64 handle, const LLSLURL& slurl,
		const LLUUID& snapshot_id, bool teleport);
		// Called by LLWorldMap when a location has been resolved to a
	    // region name

	static void regionNameCallback(U64 handle, const LLSLURL& slurl,
		const LLUUID& snapshot_id, bool teleport);
		// Called by LLWorldMap when a region name has been resolved to a
		// location in-world, used by places-panel display.

	friend class LLTeleportHandler;
};

// static
bool LLURLDispatcherImpl::dispatchCore(const LLSLURL& slurl,
									   const std::string& nav_type,
									   bool right_mouse,
									   LLMediaCtrl* web,
									   bool trusted_browser)
{
	//if (dispatchHelp(slurl, right_mouse)) return true;
	switch(slurl.getType())
	{
		case LLSLURL::APP: 
			return dispatchApp(slurl, nav_type, right_mouse, web, trusted_browser);
		case LLSLURL::LOCATION:
			return dispatchRegion(slurl, nav_type, right_mouse);
		default:
			return false;
	}

	/*
	// Inform the user we can't handle this
	std::map<std::string, std::string> args;
	args["SLURL"] = slurl;
	r;
	*/
}

// static
bool LLURLDispatcherImpl::dispatch(const LLSLURL& slurl,
								   const std::string& nav_type,
								   LLMediaCtrl* web,
								   bool trusted_browser)
{
	const bool right_click = false;
	return dispatchCore(slurl, nav_type, right_click, web, trusted_browser);
}

// static
bool LLURLDispatcherImpl::dispatchRightClick(const LLSLURL& slurl)
{
	const bool right_click = true;
	LLMediaCtrl* web = nullptr;
	const bool trusted_browser = false;
	return dispatchCore(slurl, "clicked", right_click, web, trusted_browser);
}

// static
bool LLURLDispatcherImpl::dispatchApp(const LLSLURL& slurl, 
									  const std::string& nav_type,
									  bool right_mouse,
									  LLMediaCtrl* web,
									  bool trusted_browser)
{
	LL_INFOS() << "cmd: " << slurl.getAppCmd() << " path: " << slurl.getAppPath() << " query: " << slurl.getAppQuery() << LL_ENDL;
	const LLSD& query_map = LLURI::queryMap(slurl.getAppQuery());
	bool handled = LLCommandDispatcher::dispatch(
			slurl.getAppCmd(), slurl.getAppPath(), query_map, web, nav_type, trusted_browser);

	// alert if we didn't handle this secondlife:///app/ SLURL
	// (but still return true because it is a valid app SLURL)
	if (! handled)
	{
		LLNotificationsUtil::add("UnsupportedCommandSLURL");
	}
	return true;
}

// static
bool LLURLDispatcherImpl::dispatchRegion(const LLSLURL& slurl, const std::string& nav_type, bool right_mouse)
{
	if(slurl.getType() != LLSLURL::LOCATION)
    {
		return false;
    }
	// Before we're logged in, need to update the startup screen
	// to tell the user where they are going.
	if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
	{
		// We're at the login screen, so make sure user can see
		// the login location box to know where they are going.
		
		LLPanelLogin::setLocation(slurl);
		return true;
	}
	LLSLURL _slurl = slurl;
	const std::string& grid = slurl.getGrid();
	const std::string& current_grid = gHippoGridManager->getCurrentGrid()->getGridName();
	if (grid != current_grid)
	{
		_slurl = LLSLURL(llformat("%s:%s", grid.c_str(), slurl.getRegion().c_str()), slurl.getPosition());
	}

	// Request a region handle by name
	LLWorldMapMessage::getInstance()->sendNamedRegionRequest(slurl.getRegion(),
									  LLURLDispatcherImpl::regionNameCallback,
									  _slurl.getSLURLString(),
									  LLUI::sConfigGroup->getBOOL("SLURLTeleportDirectly"));	// don't teleport
	return true;
}

/*static*/
void LLURLDispatcherImpl::regionNameCallback(U64 region_handle, const LLSLURL& slurl, const LLUUID& snapshot_id, bool teleport)
{
      
  if(slurl.getType() == LLSLURL::LOCATION)
    {        
      regionHandleCallback(region_handle, slurl, snapshot_id, teleport);
    }
}

/* static */
void LLURLDispatcherImpl::regionHandleCallback(U64 region_handle, const LLSLURL& slurl, const LLUUID& snapshot_id, bool teleport)
{

  // we can't teleport cross grid at this point
	HippoGridInfo* new_grid = gHippoGridManager->getGrid(slurl.getGrid());
	if(   new_grid
	   != gHippoGridManager->getCurrentGrid())
	{
		LLSD args;
		args["SLURL"] = slurl.getLocationString();
		args["CURRENT_GRID"] = gHippoGridManager->getCurrentGrid()->getGridName();

		std::string grid_label = new_grid ? new_grid->getGridName() : "";
		
		if(!grid_label.empty())
		{
			args["GRID"] = grid_label;
		}
		else 
		{
			args["GRID"] = slurl.getGrid() + " (Unrecognized)";
		}
		LLNotificationsUtil::add("CantTeleportToGrid", args);
		return;
	}
	
	LLVector3d global_pos = from_region_handle(region_handle);
	LLVector3 local_pos = slurl.getPosition();
	global_pos += LLVector3d(local_pos);
	if (teleport)
	{
		
		gAgent.teleportViaLocation(global_pos);
		if(gFloaterWorldMap)
		{
			gFloaterWorldMap->trackLocation(global_pos);
		}
	}
	else
	{
		// display informational floater, allow user to click teleport btn
		LLFloaterURLDisplay* url_displayp = LLFloaterURLDisplay::getInstance(LLSD());


		url_displayp->displayParcelInfo(region_handle, local_pos);
		if(snapshot_id.notNull())
		{
			url_displayp->setSnapshotDisplay(snapshot_id);
		}
		std::string locationString = llformat("%s %i, %i, %i", slurl.getRegion().c_str(), (S32)local_pos.mV[VX],(S32)local_pos.mV[VY],(S32)local_pos.mV[VZ]);
		url_displayp->setLocationString(locationString);
	}
}

//---------------------------------------------------------------------------
// Teleportation links are handled here because they are tightly coupled
// to SLURL parsing and sim-fragment parsing

class LLTeleportHandler : public LLCommandHandler
{
public:
	// Teleport requests *must* come from a trusted browser
	// inside the app, otherwise a malicious web page could
	// cause a constant teleport loop.  JC
	LLTeleportHandler() : LLCommandHandler("teleport", UNTRUSTED_THROTTLE) { }


	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web) override
	{
		// construct a "normal" SLURL, resolve the region to
		// a global position, and teleport to it
		if (tokens.size() < 1) return false;

		LLVector3 coords(128, 128, 0);
		if (tokens.size() <= 4)
		{
			coords = LLVector3(tokens[1].asReal(), 
							   tokens[2].asReal(), 
							   tokens[3].asReal());
		}

		// Region names may be %20 escaped.
		std::string region_name = LLURI::unescape(tokens[0]);

		LLSD args;
		args["LOCATION"] = region_name;

		LLSD payload;
		payload["region_name"] = region_name;
		payload["callback_url"] = LLSLURL(region_name, coords).getSLURLString();

		LLNotificationsUtil::add("TeleportViaSLAPP", args, payload);
		return true;
	}

	static void teleport_via_slapp(std::string region_name, std::string callback_url)
	{

		LLWorldMapMessage::getInstance()->sendNamedRegionRequest(region_name,
			LLURLDispatcherImpl::regionHandleCallback,
			callback_url,
			true);	// teleport
	}

	static bool teleport_via_slapp_callback(const LLSD& notification, const LLSD& response)
	{
		S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

		std::string region_name = notification["payload"]["region_name"].asString();
		std::string callback_url = notification["payload"]["callback_url"].asString();

		if (option == 0)
		{
			teleport_via_slapp(region_name, callback_url);
			return true;
		}

		return false;
	}

};
LLTeleportHandler gTeleportHandler;
static LLNotificationFunctorRegistration open_landmark_callback_reg("TeleportViaSLAPP", LLTeleportHandler::teleport_via_slapp_callback);



//---------------------------------------------------------------------------

// static
bool LLURLDispatcher::dispatch(const std::string& slurl,
							   const std::string& nav_type,
							   LLMediaCtrl* web,
							   bool trusted_browser)
{
	return LLURLDispatcherImpl::dispatch(LLSLURL(slurl), nav_type, web, trusted_browser);
}

// static
bool LLURLDispatcher::dispatchRightClick(const std::string& slurl)
{
	return LLURLDispatcherImpl::dispatchRightClick(LLSLURL(slurl));
}

// static
bool LLURLDispatcher::dispatchFromTextEditor(const std::string& slurl, bool trusted_content)
{
	// *NOTE: Text editors are considered sources of trusted URLs
	// in order to make avatar profile links in chat history work.
	// While a malicious resident could chat an app SLURL, the
	// receiving resident will see it and must affirmatively
	// click on it.
	// *TODO: Make this trust model more refined.  JC

	LLMediaCtrl* web = nullptr;
	return LLURLDispatcherImpl::dispatch(LLSLURL(slurl), "clicked", web, trusted_content);
}


