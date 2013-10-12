/** 
 * @file llpanelplace.cpp
 * @brief Display of a place in the Find directory.
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

#include "llpanelplace.h"

#include "llviewercontrol.h"
#include "llqueryflags.h"
#include "message.h"
#include "llui.h"
#include "llsecondlifeurls.h"
#include "llfloater.h"

#include "llagent.h"
#include "llviewerwindow.h"
#include "llbutton.h"
#include "llfloaterworldmap.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "lluiconstants.h"
#include "lltextbox.h"
#include "llviewertexteditor.h"
#include "lltexturectrl.h"
#include "llworldmap.h"
#include "llviewerregion.h"
#include "lluictrlfactory.h"
//#include "llviewermenu.h"	// create_landmark()
#include "llweb.h"
#include "llsdutil.h"
#include "llsdutil_math.h"

#include "hippogridmanager.h"

LLPanelPlace::LLPanelPlace()
:	LLPanel(std::string("Places Panel")),
	mParcelID(),
	mRequestedID(),
	mRegionID(),
	mPosGlobal(),
	mPosRegion(),
	mAuctionID(0),
	mLandmarkAssetID()
{}


LLPanelPlace::~LLPanelPlace()
{
	if (mParcelID.isNull()) return;

	LLRemoteParcelInfoProcessor::getInstance()->removeObserver(mParcelID, this);
}


BOOL LLPanelPlace::postBuild()
{
	// Since this is only used in the directory browser, always
	// disable the snapshot control. Otherwise clicking on it will
	// open a texture picker.
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setEnabled(FALSE);

    mNameEditor = getChild<LLTextBox>("name_editor");
	// Text boxes appear to have a " " in them by default.  This breaks the
	// emptiness test for filling in data from the network.  Slam to empty.
	mNameEditor->setText( LLStringUtil::null );

    mDescEditor = getChild<LLTextEditor>("desc_editor");

	mInfoEditor = getChild<LLTextBox>("info_editor");
	mLandTypeEditor = findChild<LLTextBox>("land_type_display");

    mLocationDisplay = getChild<LLTextBox>("location_editor");

	mTeleportBtn = getChild<LLButton>( "teleport_btn");
	mTeleportBtn->setClickedCallback(boost::bind(&LLPanelPlace::onClickTeleport,this));

	mMapBtn = getChild<LLButton>( "map_btn");
	mMapBtn->setClickedCallback(boost::bind(&LLPanelPlace::onClickMap,this));

	//mLandmarkBtn = getChild<LLButton>( "landmark_btn");
	//mLandmarkBtn->setClickedCallback(boost::bind(&LLPanelPlace::onClickLandmark,this));

	mAuctionBtn = getChild<LLButton>( "auction_btn");
	mAuctionBtn->setClickedCallback(boost::bind(&LLPanelPlace::onClickAuction,this));

	// Default to no auction button.  We'll show it if we get an auction id
	mAuctionBtn->setVisible(FALSE);

	// Temporary text to explain why the description panel is blank.
	// mDescEditor->setText("Parcel information not available without server update");

	return TRUE;
}

void LLPanelPlace::displayItemInfo(const LLInventoryItem* pItem)
{
	mNameEditor->setText(pItem->getName());
	mDescEditor->setText(pItem->getDescription());
}

// Use this for search directory clicks, because we are totally
// recycling the panel and don't need to use what's there.
//
// For SLURL clicks, don't call this, because we need to cache
// the location info from the user.
void LLPanelPlace::resetLocation()
{
	mParcelID.setNull();
	mRequestedID.setNull();
	mRegionID.setNull();
	mLandmarkAssetID.setNull();
	mPosGlobal.clearVec();
	mPosRegion.clearVec();
	mAuctionID = 0;
	mNameEditor->setText( LLStringUtil::null );
	mDescEditor->setText( LLStringUtil::null );
	mInfoEditor->setText( LLStringUtil::null );
	if (mLandTypeEditor)
		mLandTypeEditor->setText(LLStringUtil::null);
	mLocationDisplay->setText( LLStringUtil::null );
}


// Set the name and clear other bits of info.  Used for SLURL clicks
void LLPanelPlace::resetName(const std::string& name)
{
	setName(name);
	if(mDescEditor)
	{
		mDescEditor->setText( LLStringUtil::null );
	}
	if(mNameEditor)
	{
		llinfos << "Clearing place name" << llendl;
		mNameEditor->setText( LLStringUtil::null );
	}
	if(mInfoEditor)
	{
		mInfoEditor->setText( LLStringUtil::null );
	}
	if(mLandTypeEditor)
	{
		mLandTypeEditor->setText( LLStringUtil::null );
	}
}

void LLPanelPlace::setParcelID(const LLUUID& parcel_id)
{
	if (parcel_id.isNull()) return;

	if(mParcelID.notNull())
		LLRemoteParcelInfoProcessor::getInstance()->removeObserver(mParcelID, this);

	mParcelID = parcel_id;
	LLRemoteParcelInfoProcessor::getInstance()->addObserver(mParcelID, this);
	LLRemoteParcelInfoProcessor::getInstance()->sendParcelInfoRequest(mParcelID);
}

void LLPanelPlace::setSnapshot(const LLUUID& snapshot_id)
{
	mSnapshotCtrl->setImageAssetID(snapshot_id);
}


void LLPanelPlace::setLocationString(const std::string& location)
{
	mLocationDisplay->setText(location);
}

void LLPanelPlace::setLandTypeString(const std::string& land_type)
{
	if (mLandTypeEditor)
		mLandTypeEditor->setText(land_type);
}

void LLPanelPlace::setErrorStatus(U32 status, const std::string& reason)
{
	// We only really handle 404 and timeout errors
	std::string error_text;
	if (status == HTTP_NOT_FOUND)
	{	
		error_text = getString("server_error_text");
	}
	else if (status == HTTP_UNAUTHORIZED)		// AIFIXME: Is this indeed the error we get when we don't have access rights for this?
	{
		error_text = getString("server_forbidden_text");
	}
	else if (status == HTTP_INTERNAL_ERROR_LOW_SPEED || status == HTTP_INTERNAL_ERROR_CURL_TIMEOUT)
	{
		error_text = getString("internal_timeout_text");
	}
	else
	{
		llwarns << "Unexpected error (" << status << "): " << reason << llendl;
		error_text = llformat("Unexpected Error (%u): %s", status, reason.c_str());
	}
	mDescEditor->setText(error_text);
}

void LLPanelPlace::processParcelInfo(const LLParcelData& parcel_data)
{
	mAuctionID = parcel_data.auction_id;

	if (parcel_data.snapshot_id.notNull())
	{
		setSnapshot(parcel_data.snapshot_id);
	}

	// Only assign the name and description if they are not empty and there is not a
	// value present (passed in from a landmark, e.g.)

	if( !parcel_data.name.empty()
	   && mNameEditor && mNameEditor->getText().empty())
	{
		mNameEditor->setText(parcel_data.name);
	}

	if( !parcel_data.desc.empty()
		&& mDescEditor && mDescEditor->getText().empty())
	{
		mDescEditor->setText(parcel_data.desc);
	}

	std::string info_text;
	LLUIString traffic = getString("traffic_text");
	traffic.setArg("[TRAFFIC]", llformat("%d ", (int)parcel_data.dwell));
	info_text = traffic;
	LLUIString area = getString("area_text");
	area.setArg("[AREA]", llformat("%d", parcel_data.actual_area));
	info_text += area;
	if (parcel_data.flags & DFQ_FOR_SALE)
	{
		LLUIString forsale = getString("forsale_text");
		forsale.setArg("[CURRENCY]", gHippoGridManager->getConnectedGrid()->getCurrencySymbol());
		forsale.setArg("[PRICE]", llformat("%d", parcel_data.sale_price));
		info_text += forsale;
	}
	if (parcel_data.auction_id != 0)
	{
		LLUIString auction = getString("auction_text");
		auction.setArg("[ID]", llformat("%010d ", parcel_data.auction_id));
		info_text += auction;
	}
	if (mInfoEditor)
	{
		mInfoEditor->setText(info_text);
	}

	// HACK: Flag 0x2 == adult region,
	// Flag 0x1 == mature region, otherwise assume PG
	std::string rating = LLViewerRegion::accessToString(SIM_ACCESS_PG);
	if (parcel_data.flags & 0x2)
	{
		rating = LLViewerRegion::accessToString(SIM_ACCESS_ADULT);
	}
	else if (parcel_data.flags & 0x1)
	{
		rating = LLViewerRegion::accessToString(SIM_ACCESS_MATURE);
	}

	// Just use given region position for display
	S32 region_x = llround(mPosRegion.mV[0]);
	S32 region_y = llround(mPosRegion.mV[1]);
	S32 region_z = llround(mPosRegion.mV[2]);

	// If the region position is zero, grab position from the global
	if(mPosRegion.isExactlyZero())
	{
		region_x = llround(parcel_data.global_x) % REGION_WIDTH_UNITS;
		region_y = llround(parcel_data.global_y) % REGION_WIDTH_UNITS;
		region_z = llround(parcel_data.global_z);
	}

	if(mPosGlobal.isExactlyZero())
	{
		mPosGlobal.setVec(parcel_data.global_x, parcel_data.global_y, parcel_data.global_z);
	}

	std::string location = llformat("%s %d, %d, %d (%s)",
									parcel_data.sim_name.c_str(), region_x, region_y, region_z, rating.c_str());
	if (mLocationDisplay)
	{
		mLocationDisplay->setText(location);
	}

	BOOL show_auction = (parcel_data.auction_id > 0);
		mAuctionBtn->setVisible(show_auction);
}


void LLPanelPlace::displayParcelInfo(const LLVector3& pos_region,
									 const LLUUID& landmark_asset_id,
									 const LLUUID& region_id,
									 const LLVector3d& pos_global)
{
	LLSD body;
	mPosRegion = pos_region;
	mPosGlobal = pos_global;
	mLandmarkAssetID = landmark_asset_id;
	std::string url = gAgent.getRegion()->getCapability("RemoteParcelRequest");
	if (!url.empty())
	{
		body["location"] = ll_sd_from_vector3(pos_region);
		if (!region_id.isNull())
		{
			body["region_id"] = region_id;
		}
		if (!pos_global.isExactlyZero())
		{
			U64 region_handle = to_region_handle(pos_global);
			body["region_handle"] = ll_sd_from_U64(region_handle);
		}
		LLHTTPClient::post(url, body, new LLRemoteParcelRequestResponder(this->getObserverHandle()));
	}
	else
	{
		mDescEditor->setText(getString("server_update_text"));
	}
	mSnapshotCtrl->setImageAssetID(LLUUID::null);
	mSnapshotCtrl->setFallbackImageName("default_land_picture.j2c");
}


// static
void LLPanelPlace::onClickTeleport(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;

	LLView* parent_viewp = self->getParent();
	LLFloater* parent_floaterp = dynamic_cast<LLFloater*>(parent_viewp);
	if (parent_floaterp)
	{
		parent_floaterp->close();
	}
	// LLFloater* parent_floaterp = (LLFloater*)self->getParent();
	parent_viewp->setVisible(false);
	if(self->mLandmarkAssetID.notNull())
	{
		gAgent.teleportViaLandmark(self->mLandmarkAssetID);
		gFloaterWorldMap->trackLandmark(self->mLandmarkAssetID);

	}
	else if (!self->mPosGlobal.isExactlyZero())
	{
		gAgent.teleportViaLocation(self->mPosGlobal);
		gFloaterWorldMap->trackLocation(self->mPosGlobal);
	}
}

// static
void LLPanelPlace::onClickMap(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;
	if (!self->mPosGlobal.isExactlyZero())
	{
		gFloaterWorldMap->trackLocation(self->mPosGlobal);
		LLFloaterWorldMap::show(true);
	}
}

// static
/*
void LLPanelPlace::onClickLandmark(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;

	create_landmark(self->mNameEditor->getText(), "", self->mPosGlobal);
}
*/


// static
void LLPanelPlace::onClickAuction(void* data)
{
	LLPanelPlace* self = (LLPanelPlace*)data;
	LLSD payload;
	payload["auction_id"] = self->mAuctionID;

	LLNotificationsUtil::add("GoToAuctionPage", LLSD(), payload, callbackAuctionWebPage);
}

// static
bool LLPanelPlace::callbackAuctionWebPage(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		std::string url;
		S32 auction_id = notification["payload"]["auction_id"].asInteger();
		url = AUCTION_URL + llformat("%010d", auction_id );

		llinfos << "Loading auction page " << url << llendl;

		LLWeb::loadURL(url);
	}
	return false;
}
