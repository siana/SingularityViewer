/**
 * @file llfloaterregionrestarting.cpp
 * @brief Shows countdown timer during region restart
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterregionrestarting.h"

#include "lluictrlfactory.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"

// <singu> For emergency teleports
#include "llinventorymodel.h"
#include "lllandmarklist.h"
#include "llviewerregion.h"
#include "llworldmap.h"
void emergency_teleport(const U32& seconds)
{
	static const LLCachedControl<U32> time(gSavedSettings, "EmergencyTeleportSeconds");
	if (seconds > time) return;

	static const LLCachedControl<std::string> landmark(gSavedPerAccountSettings, "EmergencyTeleportLandmark");
	if (landmark().empty()) return;
	LLUUID id(landmark);
	if (id.isNull()) return;
	bool use_backup = false;
	LLViewerInventoryItem* item = gInventory.getItem(id);
	if (item)
	{
		if (LLLandmark* lm = gLandmarkList.getAsset(item->getAssetUUID()))
		{
			if (LLViewerRegion* region = gAgent.getRegion())
				use_backup = !lm->getRegionID(id) || id == region->getRegionID(); // LM's Region id null or same as current region
			if (!use_backup)
			{
				LLVector3d pos_global;
				if (lm->getGlobalPos(pos_global))
				{
					if (LLSimInfo* sim_info = LLWorldMap::instance().simInfoFromPosGlobal(pos_global))
					{
						use_backup = sim_info->isDown();
					}
				}
				else use_backup = true; // No coords, this will fail!
			}
		}
		else use_backup = true;
	}
	else use_backup = true;

	if (use_backup) // Something is wrong with the first provided landmark, fallback.
	{
		static const LLCachedControl<std::string> landmark(gSavedPerAccountSettings, "EmergencyTeleportLandmarkBackup");
		if (landmark().empty()) return;
		if (!id.set(landmark)) return;
		if (!(item = gInventory.getItem(id))) return;
	}

	gAgent.teleportViaLandmark(item->getAssetUUID());
}
// </singu>

enum shake_state
{
	SHAKE_START,
	SHAKE_LEFT,
	SHAKE_UP,
	SHAKE_RIGHT,
	SHAKE_DOWN,
	SHAKE_DONE
};
static shake_state sShakeState;

LLFloaterRegionRestarting::LLFloaterRegionRestarting(const LLSD& key) :
	LLEventTimer(1)
,	mRestartSeconds(NULL)
,	mSeconds(key["SECONDS"].asInteger())
,	mName(key.has("NAME") ? key["NAME"].asString() : gAgent.getRegion()->getName()) // <alchemy/>
,	mShakeIterations()
,	mShakeMagnitude()
{
	//buildFromFile("floater_region_restarting.xml");
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_region_restarting.xml");
	center();

}

LLFloaterRegionRestarting::~LLFloaterRegionRestarting()
{
	if (sShakeState != SHAKE_DONE && sShakeState != SHAKE_START) // Finish shake if needed
	{
		gAgentCamera.resetView(TRUE, TRUE);
		sShakeState = SHAKE_DONE;
	}
	mRegionChangedConnection.disconnect();
}

BOOL LLFloaterRegionRestarting::postBuild()
{
	mRegionChangedConnection = gAgent.addRegionChangedCallback(boost::bind(&LLFloaterRegionRestarting::regionChange, this));
	emergency_teleport(mSeconds); // <singu/> For emergency teleports

	LLStringUtil::format_map_t args;
	std::string text;

	args["[NAME]"] = mName;
	text = getString("RegionName", args);
	LLTextBox* textbox = getChild<LLTextBox>("region_name");
	textbox->setValue(text);

	mRestartSeconds = getChild<LLTextBox>("restart_seconds");

	setBackgroundColor(gColors.getColor("NotifyCautionBoxColor"));
	sShakeState = SHAKE_START;

	refresh();

	return TRUE;
}

void LLFloaterRegionRestarting::regionChange()
{
	close();
}

BOOL LLFloaterRegionRestarting::tick()
{
	refresh();

	return FALSE;
}

void LLFloaterRegionRestarting::refresh()
{
	LLStringUtil::format_map_t args;
	args["[SECONDS]"] = llformat("%d", mSeconds);
	mRestartSeconds->setValue(getString("RestartSeconds", args));

	emergency_teleport(mSeconds); // <singu/> For emergency teleports
	if (!mSeconds) return; // Zero means we're done.
	--mSeconds;
}

void LLFloaterRegionRestarting::draw()
{
	LLFloater::draw();

	static const LLCachedControl<bool> alchemyRegionShake(gSavedSettings, "AlchemyRegionRestartShake", true);
	if (!alchemyRegionShake || isMinimized()) // If we're minimized, leave the user alone
		return;

	const F32 SHAKE_INTERVAL = 0.025f;
	const F32 SHAKE_TOTAL_DURATION = 1.8f; // the length of the default alert tone for this
	const F32 SHAKE_INITIAL_MAGNITUDE = 1.5f;
	const F32 SHAKE_HORIZONTAL_BIAS = 0.25f;
	F32 time_shaking;

	if (SHAKE_START == sShakeState)
	{
		mShakeTimer.setTimerExpirySec(SHAKE_INTERVAL);
		sShakeState = SHAKE_LEFT;
		mShakeIterations = 0;
		mShakeMagnitude = SHAKE_INITIAL_MAGNITUDE;
	}

	if (SHAKE_DONE != sShakeState && mShakeTimer.hasExpired())
	{
		gAgentCamera.unlockView();

		switch(sShakeState)
		{
			case SHAKE_LEFT:
				gAgentCamera.setPanLeftKey(mShakeMagnitude * SHAKE_HORIZONTAL_BIAS);
				sShakeState = SHAKE_UP;
				break;

			case SHAKE_UP:
				gAgentCamera.setPanUpKey(mShakeMagnitude);
				sShakeState = SHAKE_RIGHT;
				break;

			case SHAKE_RIGHT:
				gAgentCamera.setPanRightKey(mShakeMagnitude * SHAKE_HORIZONTAL_BIAS);
				sShakeState = SHAKE_DOWN;
				break;

			case SHAKE_DOWN:
				gAgentCamera.setPanDownKey(mShakeMagnitude);
				mShakeIterations++;
				time_shaking = SHAKE_INTERVAL * (mShakeIterations * 4 /* left, up, right, down */);
				if (SHAKE_TOTAL_DURATION <= time_shaking)
				{
					sShakeState = SHAKE_DONE;
					mShakeMagnitude = 0.0;
				}
				else
				{
					sShakeState = SHAKE_LEFT;
					F32 percent_done_shaking = (SHAKE_TOTAL_DURATION - time_shaking) / SHAKE_TOTAL_DURATION;
					mShakeMagnitude = SHAKE_INITIAL_MAGNITUDE * (percent_done_shaking * percent_done_shaking); // exponential decay
				}
				break;

			default:
				break;
		}
		mShakeTimer.setTimerExpirySec(SHAKE_INTERVAL);
	}
}

void LLFloaterRegionRestarting::updateTime(const U32& time)
{
	mSeconds = time;
	emergency_teleport(mSeconds); // <singu/> For emergency teleports
	sShakeState = SHAKE_START;
}
