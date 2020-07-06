/**
 * @file   llavatarrenderinfoaccountant.cpp
 * @author Dave Simmons
 * @date   2013-02-28
 * @brief  
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

// Precompiled header
#include "llviewerprecompiledheaders.h"
// associated header
#include "llavatarrenderinfoaccountant.h"
// STL headers
// std headers
// external library headers
// other Linden headers
#include "llcharacter.h"
#include "llhttpclient.h"
#include "lltimer.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llworld.h"


static	const std::string KEY_AGENTS = "agents";			// map
static 	const std::string KEY_WEIGHT = "weight";			// integer
static 	const std::string KEY_TOO_COMPLEX  = "tooComplex";  // bool
static  const std::string KEY_OVER_COMPLEXITY_LIMIT = "overlimit";  // integer
static  const std::string KEY_REPORTING_COMPLEXITY_LIMIT = "reportinglimit";  // integer

static	const std::string KEY_IDENTIFIER = "identifier";
static	const std::string KEY_MESSAGE = "message";
static	const std::string KEY_ERROR = "error";


static const F32 SECS_BETWEEN_REGION_SCANS   =  5.f;		// Scan the region list every 5 seconds
static const F32 SECS_BETWEEN_REGION_REQUEST = 15.0;		// Look for new avs every 15 seconds
static const F32 SECS_BETWEEN_REGION_REPORTS = 60.0;		// Update each region every 60 seconds

// Send data updates about once per minute, only need per-frame resolution
LLFrameTimer LLAvatarRenderInfoAccountant::sRenderInfoReportTimer;


// HTTP responder class for GET request for avatar render weight information
class LLAvatarRenderInfoGetResponder : public LLHTTPClient::ResponderWithResult
{
public:
	LLAvatarRenderInfoGetResponder(U64 region_handle) : mRegionHandle(region_handle)
	{
	}

	virtual void httpFailure();

	virtual void httpSuccess();
	/*virtual*/ char const* getName() const { return "This is dumb."; }

private:
	U64		mRegionHandle;
};


void LLAvatarRenderInfoGetResponder::httpFailure()
{
	const S32 statusNum = getStatus();
	const std::string& reason = getReason();
	LLViewerRegion * regionp = LLWorld::getInstance()->getRegionFromHandle(mRegionHandle);
	if (regionp)
	{
		LL_WARNS() << "HTTP error result for avatar weight GET: " << statusNum 
				<< ", " << reason
				<< " returned by region " << regionp->getName()
				<< LL_ENDL;
	}
	else
	{
		LL_WARNS() << "Avatar render weight GET error recieved but region not found for " 
			<< mRegionHandle 
			<< ", error " << statusNum 
			<< ", " << reason
			<< LL_ENDL;
	}
}


void LLAvatarRenderInfoGetResponder::httpSuccess()
{
	LLViewerRegion * regionp = LLWorld::getInstance()->getRegionFromHandle(mRegionHandle);
	if (!regionp)
	{
		LL_WARNS("AvatarRenderInfoAccountant") << "Avatar render weight info received but region not found for "
			<< mRegionHandle << LL_ENDL;
		return;
	}

	if (LLAvatarRenderInfoAccountant::logRenderInfo())
	{
		LL_INFOS() << "LRI: Result for avatar weights request for region " << regionp->getName() << ":" << LL_ENDL;
	}

	const LLSD& result = getContent();
	if (result.isMap())
	{
		if (result.has(KEY_AGENTS))
		{
			const LLSD & agents = result[KEY_AGENTS];
			if (agents.isMap())
			{
				for (LLSD::map_const_iterator agent_iter = agents.beginMap();
					agent_iter != agents.endMap();
					agent_iter++
					)
				{
					LLUUID target_agent_id = LLUUID(agent_iter->first);
					LLVOAvatar* avatarp = gObjectList.findAvatar(target_agent_id);
					if (avatarp &&
						!avatarp->isControlAvatar() &&
						avatarp->isAvatar())

					{
						const LLSD & agent_info_map = agent_iter->second;
						if (agent_info_map.isMap())
						{
							// Extract the data for this avatar
							if (LLAvatarRenderInfoAccountant::logRenderInfo())
							{
								LL_INFOS() << "LRI:  Agent " << target_agent_id
									<< ": " << agent_info_map << LL_ENDL;
							}
							if (agent_info_map.has(KEY_WEIGHT))
							{
								avatarp->setReportedVisualComplexity(agent_info_map[KEY_WEIGHT].asInteger());
							}
						}
						else
						{
							LL_WARNS("AvatarRenderInfo") << "agent entry invalid"
								<< " agent " << target_agent_id
								<< " map " << agent_info_map
								<< LL_ENDL;
						}
					}
					else
					{
						LL_DEBUGS("AvatarRenderInfo") << "Unknown agent " << target_agent_id << LL_ENDL;
					}
				}
			}
			else
			{
				LL_WARNS("AvatarRenderInfo") << "malformed get response '" << KEY_AGENTS << "' is not map" << LL_ENDL;
			}
		}
		else
		{
			LL_INFOS("AvatarRenderInfo") << "no '" << KEY_AGENTS << "' key in get response" << LL_ENDL;
		}

		if (result.has(KEY_REPORTING_COMPLEXITY_LIMIT)
			&& result.has(KEY_OVER_COMPLEXITY_LIMIT))
		{
			U32 reporting = result[KEY_REPORTING_COMPLEXITY_LIMIT].asInteger();
			U32 overlimit = result[KEY_OVER_COMPLEXITY_LIMIT].asInteger();

			LL_DEBUGS("AvatarRenderInfo") << "complexity limit: " << reporting << " reporting, " << overlimit << " over limit" << LL_ENDL;
		}
		if (result.has(KEY_ERROR))
		{
			const LLSD & error = result[KEY_ERROR];
			LL_WARNS() << "Avatar render info GET error: "
				<< error[KEY_IDENTIFIER]
				<< ": " << error[KEY_MESSAGE]
				<< " from region " << regionp->getName()
				<< LL_ENDL;
		}
	}
}


// HTTP responder class for POST request for avatar render weight information
class LLAvatarRenderInfoPostResponder : public LLHTTPClient::ResponderWithResult
{
public:
	LLAvatarRenderInfoPostResponder(U64 region_handle) : mRegionHandle(region_handle)
	{
	}

	virtual void httpFailure();

	virtual void httpSuccess();
	/*virtual*/ char const* getName() const { return "This is also dumb."; }

private:
	U64		mRegionHandle;
};


// static 
// Send request for one region, no timer checks
void LLAvatarRenderInfoAccountant::sendRenderInfoToRegion(LLViewerRegion * regionp)
{
	std::string url = regionp->getCapability("AvatarRenderInfo");
	if (!url.empty())
	{
	if (logRenderInfo())
	{
		LL_INFOS() << "LRI: Sending avatar render info to region "
			<< regionp->getName() 
			<< " from " << url
			<< LL_ENDL;
	}

	U32 num_avs = 0;
	// Build the render info to POST to the region
	LLSD agents = LLSD::emptyMap();
				
	std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
	while( iter != LLCharacter::sInstances.end() )
	{
		LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(*iter);
		if (avatar &&
			avatar->getRezzedStatus() >= 2 &&					// Mostly rezzed (maybe without baked textures downloaded)
			!avatar->isDead() &&								// Not dead yet
			!avatar->isControlAvatar() &&						// Not part of an animated object
			avatar->getObjectHost() == regionp->getHost())		// Ensure it's on the same region
		{
			avatar->calculateUpdateRenderComplexity();			// Make sure the numbers are up-to-date

			LLSD info = LLSD::emptyMap();
			U32 avatar_complexity = avatar->getVisualComplexity();
			if (avatar_complexity > 0)
			{
				// the weight/complexity is unsigned, but LLSD only stores signed integers,
				// so if it's over that (which would be ridiculously high), just store the maximum signed int value
				info[KEY_WEIGHT] = (S32)(avatar_complexity < S32_MAX ? avatar_complexity : S32_MAX);
				info[KEY_TOO_COMPLEX]  = LLSD::Boolean(avatar->isTooComplex());
				agents[avatar->getID().asString()] = info;
				
				if (logRenderInfo())
				{
					LL_INFOS("AvatarRenderInfo") << "Sending avatar render info for " << avatar->getID()
							<< ": " << info << LL_ENDL;
				}
				num_avs++;
			}
		}
		iter++;
	}

	if (num_avs == 0)
		 return; // nothing to report

		LLSD report = LLSD::emptyMap();
		report[KEY_AGENTS] = agents;
		if (agents.size() > 0)
		{
			LLHTTPClient::post(url, report, new LLAvatarRenderInfoPostResponder(regionp->getHandle()));
		}
	}
}

void LLAvatarRenderInfoPostResponder::httpSuccess()
{
	LLViewerRegion * regionp = LLWorld::getInstance()->getRegionFromHandle(mRegionHandle);
    if (!regionp)
    {
        LL_INFOS("AvatarRenderInfoAccountant") << "Avatar render weight POST result received but region not found for "
                << mRegionHandle << LL_ENDL;
        return;
    }
	const LLSD& result = getContent();
	if (result.isMap())
	{
		if (result.has(KEY_ERROR))
		{
			const LLSD & error = result[KEY_ERROR];
			LL_WARNS("AvatarRenderInfoAccountant") << "POST error: "
				<< error[KEY_IDENTIFIER]
				<< ": " << error[KEY_MESSAGE] 
				<< " from region " << regionp->getName()
				<< LL_ENDL;
		}
		else
		{
			LL_DEBUGS("AvatarRenderInfoAccountant")
				<< "POST result for region " << regionp->getName()
				<< ": " << result
				 << LL_ENDL;
		}
	}
	else
    {
        LL_WARNS("AvatarRenderInfoAccountant") << "Malformed POST response from region '" << regionp->getName()
                                               << LL_ENDL;
    }
}

void LLAvatarRenderInfoPostResponder::httpFailure()
{
	const S32 statusNum = getStatus();
	const std::string& reason = getReason();
	LLViewerRegion * regionp = LLWorld::getInstance()->getRegionFromHandle(mRegionHandle);
	if (regionp)
	{
		LL_WARNS() << "HTTP error result for avatar weight POST: " << statusNum 
				<< ", " << reason
			<< " returned by region " << regionp->getName()
			<< LL_ENDL;
	}
	else
	{
		LL_WARNS() << "Avatar render weight POST error recieved but region not found for " 
			<< mRegionHandle 
			<< ", error " << statusNum 
			<< ", " << reason
			<< LL_ENDL;
	}
}

// static 
// Send request for one region, no timer checks
void LLAvatarRenderInfoAccountant::getRenderInfoFromRegion(LLViewerRegion * regionp)
{
	std::string url = regionp->getCapability("AvatarRenderInfo");
	if (!url.empty())
	{
		if (logRenderInfo())
		{
			LL_INFOS() << "LRI: Requesting avatar render info for region "
				<< regionp->getName() 
				<< " from " << url
				<< LL_ENDL;
		}

		// First send a request to get the latest data
		LLHTTPClient::get(url, new LLAvatarRenderInfoGetResponder(regionp->getHandle()));
	}
}


// static
// Called every frame - send render weight requests to every region
void LLAvatarRenderInfoAccountant::idle()
{
	if (sRenderInfoReportTimer.hasExpired())
	{
		S32 num_avs = LLCharacter::sInstances.size();

		if (logRenderInfo())
		{
			LL_INFOS() << "LRI: Scanning all regions and checking for render info updates"
				<< LL_ENDL;
		}

		// Check all regions and see if it's time to fetch/send data
		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
				iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* regionp = *iter;
			if (regionp &&
				regionp->isAlive() &&
				regionp->capabilitiesReceived() &&						// Region has capability URLs available
				regionp->getRenderInfoRequestTimer().hasExpired())		// Time to make request
			{
				sendRenderInfoToRegion(regionp);
				getRenderInfoFromRegion(regionp);

				// Reset this regions timer, moving to longer intervals if there are lots of avatars around
				regionp->getRenderInfoRequestTimer().resetWithExpiry(SECS_BETWEEN_REGION_REQUEST + (2.f * num_avs));
			}
		}

		// We scanned all the regions, reset the request timer.
		sRenderInfoReportTimer.resetWithExpiry(SECS_BETWEEN_REGION_SCANS);
	}

	/* Singu TODO: RenderAutoMuteFunctions
	static LLCachedControl<U32> render_auto_mute_functions(gSavedSettings, "RenderAutoMuteFunctions", 0);
	static U32 prev_render_auto_mute_functions = (U32) -1;
	if (prev_render_auto_mute_functions != render_auto_mute_functions)
	{
		prev_render_auto_mute_functions = render_auto_mute_functions;

		// Adjust menus
		BOOL show_items = (BOOL)(render_auto_mute_functions & 0x04);
		gMenuAvatarOther->setItemVisible( std::string("Normal"), show_items);
		gMenuAvatarOther->setItemVisible( std::string("Always use impostor"), show_items);
		gMenuAvatarOther->setItemVisible( std::string("Never use impostor"), show_items);
		gMenuAvatarOther->setItemVisible( std::string("Impostor seperator"), show_items);
		
		gMenuAttachmentOther->setItemVisible( std::string("Normal"), show_items);
		gMenuAttachmentOther->setItemVisible( std::string("Always use impostor"), show_items);
		gMenuAttachmentOther->setItemVisible( std::string("Never use impostor"), show_items);
		gMenuAttachmentOther->setItemVisible( std::string("Impostor seperator"), show_items);

		if (!show_items)
		{	// Turning off visual muting
			for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
					iter != LLCharacter::sInstances.end(); ++iter)
			{	// Make sure all AVs have the setting cleared
				LLVOAvatar* inst = (LLVOAvatar*) *iter;
				inst->setCachedVisualMute(false);
			}
		}
	}*/
}


// static
// Make sRenderInfoReportTimer expire so the next call to idle() will scan and query a new region
// called via LLViewerRegion::setCapabilitiesReceived() boost signals when the capabilities
// are returned for a new LLViewerRegion, and is the earliest time to get render info
void LLAvatarRenderInfoAccountant::expireRenderInfoReportTimer(const LLUUID& region_id)
{
	if (logRenderInfo())
	{
		LL_INFOS() << "LRI: Viewer has new region capabilities, clearing global render info timer" 
			<< " and timer for region " << region_id
			<< LL_ENDL;
	}

	// Reset the global timer so it will scan regions immediately
	sRenderInfoReportTimer.reset();
	
	LLViewerRegion* regionp = LLWorld::instance().getRegionFromID(region_id);
	if (regionp)
	{	// Reset the region's timer so it will request data immediately
		regionp->getRenderInfoRequestTimer().reset();
	}
}

// static 
bool LLAvatarRenderInfoAccountant::logRenderInfo()
{
	static LLCachedControl<bool> render_mute_logging_enabled(gSavedSettings, "RenderAutoMuteLogging", false);
	return render_mute_logging_enabled;
}
