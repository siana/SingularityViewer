/** 
 * @file llpanelclassified.cpp
 * @brief LLPanelClassified class implementation
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

// Display of a classified used both for the global view in the
// Find directory, and also for each individual user's classified in their
// profile.

#include "llviewerprecompiledheaders.h"

#include "llpanelclassified.h"

#include "lldir.h"
#include "lldispatcher.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "message.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llclassifiedflags.h"
#include "llclassifiedstatsresponder.h"
#include "llcommandhandler.h" // for classified HTML detail page click tracking
#include "lllineeditor.h"
#include "llfloaterclassified.h"
#include "lltextbox.h"
#include "llcombobox.h"
#include "llviewertexteditor.h"
#include "lltexturectrl.h"
#include "llurldispatcher.h"	// for classified HTML detail click teleports
#include "lluictrlfactory.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llworldmap.h"
#include "llfloaterworldmap.h"
#include "llviewergenericmessage.h"	// send_generic_message
#include "llviewerregion.h"
#include "llviewerwindow.h"	// for window width, height
#include "llappviewer.h"	// abortQuit()

#include "hippogridmanager.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

const S32 MATURE_UNDEFINED = -1;
const S32 MATURE_CONTENT = 1;
const S32 PG_CONTENT = 2;
const S32 DECLINE_TO_STATE = 0;

//static
LLPanelClassifiedInfo::panel_list_t LLPanelClassifiedInfo::sAllPanels;

// "classifiedclickthrough"
// strings[0] = classified_id
// strings[1] = teleport_clicks
// strings[2] = map_clicks
// strings[3] = profile_clicks
class LLDispatchClassifiedClickThrough : public LLDispatchHandler
{
public:
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& strings)
	{
		if (strings.size() != 4) return false;
		LLUUID classified_id(strings[0]);
		S32 teleport_clicks = atoi(strings[1].c_str());
		S32 map_clicks = atoi(strings[2].c_str());
		S32 profile_clicks = atoi(strings[3].c_str());

		LLPanelClassifiedInfo::setClickThrough(
			classified_id, teleport_clicks, map_clicks, profile_clicks, false);

		return true;
	}
};
static LLDispatchClassifiedClickThrough sClassifiedClickThrough;


/* Re-expose this if we need to have classified ad HTML detail
   pages.  JC

// We need to count classified teleport clicks from the search HTML detail pages,
// so we need have a teleport that also sends a click count message.
class LLClassifiedTeleportHandler : public LLCommandHandler
{
public:
	// don't allow from external browsers because it moves you immediately
	LLClassifiedTeleportHandler() : LLCommandHandler("classifiedteleport", true) { }

	bool handle(const LLSD& tokens, const LLSD& queryMap)
	{
		// Need at least classified id and region name, so 2 params
		if (tokens.size() < 2) return false;
		LLUUID classified_id = tokens[0].asUUID();
		if (classified_id.isNull()) return false;
		// *HACK: construct a SLURL to do the teleport
		std::string url("secondlife:///app/teleport/");
		// skip the uuid we took off above, rebuild URL
		// separated by slashes.
		for (S32 i = 1; i < tokens.size(); ++i)
		{
			url += tokens[i].asString();
			url += "/";
		}
		LL_INFOS() << "classified teleport to " << url << LL_ENDL;
		// *TODO: separately track old search, sidebar, and new search
		// Right now detail HTML pages count as new search.
		const bool from_search = true;
		LLPanelClassifiedInfo::sendClassifiedClickMessage(classified_id, "teleport", from_search);
		// Invoke teleport
		LLMediaCtrl* web = NULL;
		const bool trusted_browser = true;
		return LLURLDispatcher::dispatch(url, web, trusted_browser);
	}
};
// Creating the object registers with the dispatcher.
LLClassifiedTeleportHandler gClassifiedTeleportHandler;
*/

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLPanelClassifiedInfo::LLPanelClassifiedInfo(bool in_finder, bool from_search)
:	LLPanel(std::string("Classified Panel")),
	mInFinder(in_finder),
	mFromSearch(from_search),
	mDirty(false),
	mForceClose(false),
	mLocationChanged(false),
	mClassifiedID(),
	mCreatorID(),
	mPriceForListing(0),
	mDataRequested(FALSE),
	mPaidFor(FALSE),
	mPosGlobal(),
	mSnapshotCtrl(NULL),
	mNameEditor(NULL),
	mDescEditor(NULL),
	mLocationEditor(NULL),
	mCategoryCombo(NULL),
	mMatureCombo(NULL),
	mAutoRenewCheck(NULL),
	mUpdateBtn(NULL),
	mTeleportBtn(NULL),
	mMapBtn(NULL),
	mProfileBtn(NULL),
	mInfoText(NULL),
	mSetBtn(NULL),
	mClickThroughText(NULL),
	mTeleportClicksOld(0),
	mMapClicksOld(0),
	mProfileClicksOld(0),
	mTeleportClicksNew(0),
	mMapClicksNew(0),
	mProfileClicksNew(0)

{
    sAllPanels.push_back(this);

	std::string classified_def_file;
	if (mInFinder)
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "panel_classified.xml");
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "panel_avatar_classified.xml");
	}

	// Register dispatcher
	gGenericDispatcher.addHandler("classifiedclickthrough", &sClassifiedClickThrough);
}

LLPanelClassifiedInfo::~LLPanelClassifiedInfo()
{
	LLAvatarPropertiesProcessor::getInstance()->removeObserver(mCreatorID, this);
	sAllPanels.remove(this);
}


void LLPanelClassifiedInfo::reset()
{
	if (mInFinder || mCreatorID.notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(mCreatorID, this);
	}

	mClassifiedID.setNull();
	mCreatorID.setNull();
	mParcelID.setNull();

	// Don't request data, this isn't valid
	mDataRequested = TRUE;

	mDirty = false;
	mPaidFor = FALSE;

	mPosGlobal.clearVec();

	clearCtrls();
	resetDirty();
}

BOOL LLPanelClassifiedInfo::postBuild()
{
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	mSnapshotCtrl->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));
	mSnapshotSize = mSnapshotCtrl->getRect();

	mNameEditor = getChild<LLLineEditor>("given_name_editor");
	mNameEditor->setMaxTextLength(DB_PARCEL_NAME_LEN);
	mNameEditor->setCommitOnFocusLost(TRUE);
	mNameEditor->setFocusReceivedCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));
	mNameEditor->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));
	mNameEditor->setPrevalidate( LLLineEditor::prevalidateASCII );

	mDescEditor = getChild<LLTextEditor>("desc_editor");
	mDescEditor->setCommitOnFocusLost(TRUE);
	mDescEditor->setFocusReceivedCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));
	mDescEditor->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));
	mDescEditor->setTabsToNextField(TRUE);

	mLocationEditor = getChild<LLLineEditor>("location_editor");

	mSetBtn = getChild<LLButton>( "set_location_btn");
	mSetBtn->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::onClickSet, this));

	mTeleportBtn = getChild<LLButton>( "classified_teleport_btn");
	mTeleportBtn->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::onClickTeleport, this));

	mMapBtn = getChild<LLButton>( "classified_map_btn");
	mMapBtn->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::onClickMap, this));

	if(mInFinder)
	{
		mProfileBtn  = getChild<LLButton>( "classified_profile_btn");
		mProfileBtn->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::onClickProfile, this));
	}

	mCategoryCombo = getChild<LLComboBox>( "classified_category_combo");
	LLClassifiedInfo::cat_map::iterator iter;
	for (iter = LLClassifiedInfo::sCategories.begin();
		iter != LLClassifiedInfo::sCategories.end();
		iter++)
	{
		mCategoryCombo->add(iter->second, (void *)((intptr_t)iter->first), ADD_BOTTOM);
	}
	mCategoryCombo->setCurrentByIndex(0);
	mCategoryCombo->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));

	mMatureCombo = getChild<LLComboBox>( "classified_mature_check");
	mMatureCombo->setCurrentByIndex(0);
	mMatureCombo->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));
	if (gAgent.wantsPGOnly())
	{
		// Teens don't get to set mature flag. JC
		mMatureCombo->setVisible(FALSE);
		mMatureCombo->setCurrentByIndex(PG_CONTENT);
	}

	if (!mInFinder)
	{
		mAutoRenewCheck = getChild<LLCheckBoxCtrl>( "auto_renew_check");
		mAutoRenewCheck->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::checkDirty, this));
	}

	mUpdateBtn = getChild<LLButton>("classified_update_btn");
	mUpdateBtn->setCommitCallback(boost::bind(&LLPanelClassifiedInfo::onClickUpdate, this));

	if (!mInFinder)
	{
		mClickThroughText = getChild<LLTextBox>("click_through_text");
	}
	
	resetDirty();
	return TRUE;
}

void LLPanelClassifiedInfo::processProperties(void* data, EAvatarProcessorType type)
{
	if(APT_CLASSIFIED_INFO == type)
	{
		LL_DEBUGS() << "processClassifiedInfoReply()" << LL_ENDL;

		LLAvatarClassifiedInfo* c_info = static_cast<LLAvatarClassifiedInfo*>(data);
		if(c_info && mClassifiedID == c_info->classified_id)
		{
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(mCreatorID, this);

			// "Location text" is actually the original
			// name that owner gave the parcel, and the location.
			std::string location_text = c_info->parcel_name;
			
			if (!location_text.empty())
				location_text.append(", ");

			S32 region_x = ll_round((F32)c_info->pos_global.mdV[VX]) % REGION_WIDTH_UNITS;
			S32 region_y = ll_round((F32)c_info->pos_global.mdV[VY]) % REGION_WIDTH_UNITS;
			S32 region_z = ll_round((F32)c_info->pos_global.mdV[VZ]);

			std::string buffer = llformat("%s (%d, %d, %d)", c_info->sim_name.c_str(), region_x, region_y, region_z);
		    location_text.append(buffer);

			//BOOL enabled = is_cf_enabled(flags);

			time_t tim = c_info->creation_date;
			tm *now=localtime(&tim);


			// Found the panel, now fill in the information
			mClassifiedID = c_info->classified_id;
			mCreatorID = c_info->creator_id;
			mParcelID = c_info->parcel_id;
			mPriceForListing = c_info->price_for_listing;
			mSimName = c_info->sim_name;
			mPosGlobal = c_info->pos_global;

			// Update UI controls
			mNameEditor->setText(c_info->name);
			mDescEditor->setText(c_info->description, false);
			mSnapshotCtrl->setImageAssetID(c_info->snapshot_id);
			mLocationEditor->setText(location_text);
			mLocationChanged = false;

			mCategoryCombo->setCurrentByIndex(c_info->category - 1);

			mMatureCombo->setCurrentByIndex(is_cf_mature(c_info->flags) ? MATURE_CONTENT : PG_CONTENT);

			if (mAutoRenewCheck)
			{
				mAutoRenewCheck->set(is_cf_auto_renew(c_info->flags));
			}

			std::string datestr;
			timeStructToFormattedString(now, gSavedSettings.getString("ShortDateFormat"), datestr);
			LLStringUtil::format_map_t string_args;
			string_args["[DATE]"] = datestr;
			string_args["[CURRENCY]"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
			string_args["[AMT]"] = llformat("%d", c_info->price_for_listing);
			childSetText("classified_info_text", getString("ad_placed_paid", string_args));

			// If we got data from the database, we know the listing is paid for.
			mPaidFor = TRUE;

			mUpdateBtn->setLabel(getString("update_txt"));

			resetDirty();
		}
	}
}

BOOL LLPanelClassifiedInfo::titleIsValid()
{
	// Disallow leading spaces, punctuation, etc. that screw up
	// sort order.
	const std::string& name = mNameEditor->getText();
	if (name.empty())
	{
		LLNotificationsUtil::add("BlankClassifiedName");
		return FALSE;
	}
	if (!isalnum(name[0]))
	{
		LLNotificationsUtil::add("ClassifiedMustBeAlphanumeric");
		return FALSE;
	}

	return TRUE;
}

void LLPanelClassifiedInfo::apply()
{
	// Apply is used for automatically saving results, so only
	// do that if there is a difference, and this is a save not create.
	if (checkDirty() && mPaidFor)
	{
		sendClassifiedInfoUpdate();
	}
}

bool LLPanelClassifiedInfo::saveCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	switch(option)
	{
		case 0: // Save
			sendClassifiedInfoUpdate();
			// fall through to close

		case 1: // Don't Save
			{
				mForceClose = true;
				// Close containing floater
				LLFloater* parent_floater = gFloaterView->getParentFloater(this);
				if (parent_floater)
				{
					parent_floater->close();
				}
			}
			break;

		case 2: // Cancel
		default:
			LLAppViewer::instance()->abortQuit();
			break;
	}
	return false;
}


BOOL LLPanelClassifiedInfo::canClose()
{
	if (mForceClose || !checkDirty()) 
		return TRUE;

	LLSD args;
	args["NAME"] = mNameEditor->getText();
	LLNotificationsUtil::add("ClassifiedSave", args, LLSD(), boost::bind(&LLPanelClassifiedInfo::saveCallback, this, _1, _2));
	return FALSE;
}

// Fill in some reasonable defaults for a new classified.
void LLPanelClassifiedInfo::initNewClassified()
{
	// TODO:  Don't generate this on the client.
	mClassifiedID.generate();

	mCreatorID = gAgent.getID();

	mPosGlobal = gAgent.getPositionGlobal();

	mPaidFor = FALSE;

	// Try to fill in the current parcel
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (parcel)
	{
		mNameEditor->setText(parcel->getName());
		//mDescEditor->setText(parcel->getDesc());
		mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
		//mPriceEditor->setText("0");
		mCategoryCombo->setCurrentByIndex(0);
	}

	mUpdateBtn->setLabel(getString("publish_txt"));
	
	// simulate clicking the "location" button
	onClickSet();
}


void LLPanelClassifiedInfo::setClassifiedID(const LLUUID& id)
{
	mClassifiedID = id;
	if (mInFinder) mCreatorID = LLUUID::null; // Singu Note: HACKS!
}

//static
void LLPanelClassifiedInfo::setClickThrough(
										const LLUUID& classified_id,
										S32 teleport,
										S32 map,
										S32 profile,
										bool from_new_table)
{
	for (panel_list_t::iterator iter = sAllPanels.begin(); iter != sAllPanels.end(); ++iter)
	{
		LLPanelClassifiedInfo* self = *iter;
		// For top picks, must match pick id
		if (self->mClassifiedID != classified_id)
		{
			continue;
		}

		// We need to check to see if the data came from the new stat_table 
		// or the old classified table. We also need to cache the data from 
		// the two separate sources so as to display the aggregate totals.

		if (from_new_table)
		{
			self->mTeleportClicksNew = teleport;
			self->mMapClicksNew = map;
			self->mProfileClicksNew = profile;
		}
		else
		{
			self->mTeleportClicksOld = teleport;
			self->mMapClicksOld = map;
			self->mProfileClicksOld = profile;
		}

		if (self->mClickThroughText)
		{
			std::string msg = llformat("Clicks: %d teleport, %d map, %d profile",
									self->mTeleportClicksNew + self->mTeleportClicksOld,
									self->mMapClicksNew + self->mMapClicksOld,
									self->mProfileClicksNew + self->mProfileClicksOld);
			self->mClickThroughText->setText(msg);
		}
	}
}

// Schedules the panel to request data
// from the server next time it is drawn.
void LLPanelClassifiedInfo::markForServerRequest()
{
	mDataRequested = FALSE;
}


std::string LLPanelClassifiedInfo::getClassifiedName()
{
	return mNameEditor->getText();
}


void LLPanelClassifiedInfo::sendClassifiedInfoRequest()
{
	if (mClassifiedID != mRequestedID)
	{
		LLAvatarPropertiesProcessor::getInstance()->addObserver(mCreatorID, this);
		LLAvatarPropertiesProcessor::getInstance()->sendClassifiedInfoRequest(mClassifiedID);

		mDataRequested = TRUE;

		mRequestedID = mClassifiedID;

		// While we're at it let's get the stats from the new table if that
		// capability exists.
		std::string url = gAgent.getRegion()->getCapability("SearchStatRequest");
		LLSD body;
		body["classified_id"] = mClassifiedID;

		if (!url.empty())
		{
			LL_INFOS() << "Classified stat request via capability" << LL_ENDL;
			LLHTTPClient::post(url, body, new LLClassifiedStatsResponder(((LLView*)this)->getHandle(), mClassifiedID));
		}
	}
}

void LLPanelClassifiedInfo::sendClassifiedInfoUpdate()
{
	LLAvatarClassifiedInfo c_data;

	// If we don't have a classified id yet, we'll need to generate one,
	// otherwise we'll keep overwriting classified_id 00000 in the database.
	if (mClassifiedID.isNull())
	{
		// TODO:  Don't do this on the client.
		mClassifiedID.generate();
	}

	c_data.agent_id = gAgent.getID();
	c_data.classified_id = mClassifiedID;
	c_data.category = mCategoryCombo->getCurrentIndex() + 1;
	c_data.name = mNameEditor->getText();
	c_data.description = mDescEditor->getText();
	c_data.parcel_id = mParcelID;
	c_data.snapshot_id = mSnapshotCtrl->getImageAssetID();
	c_data.pos_global = mPosGlobal;
	BOOL auto_renew = mAutoRenewCheck && mAutoRenewCheck->get();
	c_data.flags = pack_classified_flags_request(auto_renew, false, mMatureCombo->getCurrentIndex() == MATURE_CONTENT, false);
	c_data.price_for_listing = mPriceForListing;
	c_data.parent_estate = 0;	//probably not required.

	LLAvatarPropertiesProcessor::getInstance()->sendClassifiedInfoUpdate(&c_data);

	mDirty = false;
}

void LLPanelClassifiedInfo::draw()
{
	refresh();

	LLPanel::draw();
}


void LLPanelClassifiedInfo::refresh()
{
	if (!mDataRequested)
	{
		sendClassifiedInfoRequest();
	}

	// Check for god mode
	BOOL godlike = gAgent.isGodlike();
	BOOL is_self = (gAgent.getID() == mCreatorID);

	// Set button visibility/enablement appropriately
	if (mInFinder)
	{

		// End user doesn't ned to see price twice, or date posted.

		mSnapshotCtrl->setEnabled(godlike);
		if (godlike)
		{
			//make it smaller, so text is more legible
			mSnapshotCtrl->setOrigin(20, 175);
			mSnapshotCtrl->reshape(300, 200);
		}
		else
		{
			mSnapshotCtrl->setOrigin(mSnapshotSize.mLeft, mSnapshotSize.mBottom);
			mSnapshotCtrl->reshape(mSnapshotSize.getWidth(), mSnapshotSize.getHeight());
			//normal
		}
		mNameEditor->setEnabled(godlike);
		mDescEditor->setEnabled(godlike);
		mCategoryCombo->setEnabled(godlike);
		mCategoryCombo->setVisible(godlike);

		mMatureCombo->setEnabled(godlike);
		mMatureCombo->setVisible(godlike);

		// Jesse (who is the only one who uses this, as far as we can tell
		// Says that he does not want a set location button - he has used it
		// accidently in the past.
		mSetBtn->setVisible(FALSE);
		mSetBtn->setEnabled(FALSE);

		mUpdateBtn->setEnabled(godlike);
		mUpdateBtn->setVisible(godlike);
	}
	else
	{
		mSnapshotCtrl->setEnabled(is_self);
		mNameEditor->setEnabled(is_self);
		mDescEditor->setEnabled(is_self);
		//mPriceEditor->setEnabled(is_self);
		mCategoryCombo->setEnabled(is_self);
		mMatureCombo->setEnabled(is_self);

		if( is_self )
		{
			if( mMatureCombo->getCurrentIndex() == 0 )
			{
				// It's a new panel.
				// PG regions should have PG classifieds. AO should have mature.

				setDefaultAccessCombo();
			}
		}

		if (mAutoRenewCheck)
		{
			mAutoRenewCheck->setEnabled(is_self);
			mAutoRenewCheck->setVisible(is_self);
		}

		mClickThroughText->setEnabled(is_self);
		mClickThroughText->setVisible(is_self);

		mSetBtn->setVisible(is_self);
		//mSetBtn->setEnabled(is_self);
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
		mSetBtn->setEnabled(is_self && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) );
// [/RLVa:KB]

		mUpdateBtn->setEnabled(is_self && checkDirty());
		mUpdateBtn->setVisible(is_self);
	}
}

void LLPanelClassifiedInfo::onClickUpdate()
{
	// Disallow leading spaces, punctuation, etc. that screw up
	// sort order.
	if (!titleIsValid())
	{
		return;
	}

	// If user has not set mature, do not allow publish
	if (mMatureCombo->getCurrentIndex() == DECLINE_TO_STATE)
	{
		// Tell user about it
		LLNotificationsUtil::add("SetClassifiedMature", 
				LLSD(), 
				LLSD(), 
				boost::bind(&LLPanelClassifiedInfo::confirmMature, this, _1, _2));
		return;
	}

	// Mature content flag is set, proceed
	gotMature();
}

// Callback from a dialog indicating response to mature notification
bool LLPanelClassifiedInfo::confirmMature(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	
	// 0 == Yes
	// 1 == No
	// 2 == Cancel
	switch(option)
	{
	case 0:
		mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
		break;
	case 1:
		mMatureCombo->setCurrentByIndex(PG_CONTENT);
		break;
	default:
		return false;
	}
	
	// If we got here it means they set a valid value
	gotMature();
	return false;
}

// Called after we have determined whether this classified has
// mature content or not.
void LLPanelClassifiedInfo::gotMature()
{
	// if already paid for, just do the update
	if (mPaidFor)
	{
		LLNotification::Params params("PublishClassified");
		params.functor(boost::bind(&LLPanelClassifiedInfo::confirmPublish, this, _1, _2));
		LLNotifications::instance().forceResponse(params, 0);
	}
	else
	{
		// Ask the user how much they want to pay
		new LLFloaterPriceForListing(boost::bind(&LLPanelClassifiedInfo::callbackGotPriceForListing, this, _1));
	}
}

void LLPanelClassifiedInfo::callbackGotPriceForListing(const std::string& text)
{
	S32 price_for_listing = strtol(text.c_str(), NULL, 10);
	const HippoGridInfo& grid(*gHippoGridManager->getConnectedGrid());
	const int& min_fee(grid.getClassifiedFee());
	if (price_for_listing < min_fee)
	{
		LLSD args;
		args["MIN_PRICE"] = llformat("%d", min_fee);
		LLNotificationsUtil::add("MinClassifiedPrice", args);
		return;
	}

	// price is acceptable, put it in the dialog for later read by 
	// update send
	mPriceForListing = price_for_listing;

	LLSD args;
	args["AMOUNT"] = llformat("%d", price_for_listing);
	args["CURRENCY"] = grid.getCurrencySymbol();
	LLNotificationsUtil::add("PublishClassified", args, LLSD(), 
									boost::bind(&LLPanelClassifiedInfo::confirmPublish, this, _1, _2));
}

void LLPanelClassifiedInfo::resetDirty()
{
	// Tell all the widgets to reset their dirty state since the ad was just saved
	if (mSnapshotCtrl)
		mSnapshotCtrl->resetDirty();
	if (mNameEditor)
		mNameEditor->resetDirty();
	if (mDescEditor)
		mDescEditor->resetDirty();
	if (mLocationEditor)
		mLocationEditor->resetDirty();
	mLocationChanged = false;
	if (mCategoryCombo)
		mCategoryCombo->resetDirty();
	if (mMatureCombo)
		mMatureCombo->resetDirty();
	if (mAutoRenewCheck)
		mAutoRenewCheck->resetDirty();
}

// invoked from callbackConfirmPublish
bool LLPanelClassifiedInfo::confirmPublish(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	// Option 0 = publish
	if (option != 0) return false;

	sendClassifiedInfoUpdate();

	// Big hack - assume that top picks are always in a browser,
	// and non-finder-classifieds are always in a tab container.
	if (mInFinder)
	{
		// TODO: enable this
		//LLPanelDirClassifieds* panel = (LLPanelDirClassifieds*)getParent();
		//panel->renameClassified(mClassifiedID, mNameEditor->getText());
	}
	else
	{
		LLTabContainer* tab = (LLTabContainer*)getParent();
		tab->setCurrentTabName(mNameEditor->getText());
	}

	resetDirty();
	return false;
}

void LLPanelClassifiedInfo::onClickTeleport()
{
	if (!mPosGlobal.isExactlyZero())
	{
		gAgent.teleportViaLocation(mPosGlobal);
		gFloaterWorldMap->trackLocation(mPosGlobal);

		sendClassifiedClickMessage("teleport");
	}
}

void LLPanelClassifiedInfo::onClickMap()
{
	gFloaterWorldMap->trackLocation(mPosGlobal);
	LLFloaterWorldMap::show(true);

	sendClassifiedClickMessage("map");
}

void LLPanelClassifiedInfo::onClickProfile()
{
	LLAvatarActions::showProfile(mCreatorID);
	sendClassifiedClickMessage("profile");
}

/*
void LLPanelClassifiedInfo::onClickLandmark()
{
	create_landmark(mNameEditor->getText(), "", mPosGlobal);
}
*/

void LLPanelClassifiedInfo::onClickSet()
{
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		return;
	}
// [/RLVa:KB]

	// Save location for later.
	mPosGlobal = gAgent.getPositionGlobal();

	std::string location_text;
	std::string regionName = "(will update after publish)";
	LLViewerRegion* pRegion = gAgent.getRegion();
	if (pRegion)
	{
		regionName = pRegion->getName();
	}
	location_text.assign(regionName);
	location_text.append(", ");

	S32 region_x = ll_round((F32)mPosGlobal.mdV[VX]) % REGION_WIDTH_UNITS;
	S32 region_y = ll_round((F32)mPosGlobal.mdV[VY]) % REGION_WIDTH_UNITS;
	S32 region_z = ll_round((F32)mPosGlobal.mdV[VZ]);

	location_text.append(mSimName);
	location_text.append(llformat(" (%d, %d, %d)", region_x, region_y, region_z));

	mLocationEditor->setText(location_text);
	mLocationChanged = true;
	
	setDefaultAccessCombo();

	// Set this to null so it updates on the next save.
	mParcelID.setNull();

	checkDirty();
}


BOOL LLPanelClassifiedInfo::checkDirty()
{
	mDirty = FALSE;
	if	( mSnapshotCtrl )			mDirty |= mSnapshotCtrl->isDirty();
	if	( mNameEditor )				mDirty |= mNameEditor->isDirty();
	if	( mDescEditor )				mDirty |= mDescEditor->isDirty();
	if	( mLocationEditor )			mDirty |= mLocationEditor->isDirty();
	if	( mLocationChanged )		mDirty |= TRUE;
	if	( mCategoryCombo )			mDirty |= mCategoryCombo->isDirty();
	if	( mMatureCombo )			mDirty |= mMatureCombo->isDirty();
	if	( mAutoRenewCheck )			mDirty |= mAutoRenewCheck->isDirty();

	return mDirty;
}


void LLPanelClassifiedInfo::sendClassifiedClickMessage(const std::string& type)
{
	// You're allowed to click on your own ads to reassure yourself
	// that the system is working.
	LLSD body;
	body["type"] = type;
	body["from_search"] = mFromSearch;
	body["classified_id"] = mClassifiedID;
	body["parcel_id"] = mParcelID;
	body["dest_pos_global"] = mPosGlobal.getValue();
	body["region_name"] = mSimName;

	std::string url = gAgent.getRegion()->getCapability("SearchStatTracking");
	LL_INFOS() << "LLPanelClassifiedInfo::sendClassifiedClickMessage via capability" << LL_ENDL;
	LLHTTPClient::post(url, body, new LLHTTPClient::ResponderIgnore);
}

////////////////////////////////////////////////////////////////////////////////////////////

LLFloaterPriceForListing::LLFloaterPriceForListing(const signal_t::slot_type& cb)
:	LLFloater(std::string("PriceForListing")),
	mSignal(new signal_t)
{
	// Builds and adds to gFloaterView
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_price_for_listing.xml");
	center();

	mSignal->connect(cb);
}

//virtual
LLFloaterPriceForListing::~LLFloaterPriceForListing()
{
	delete mSignal;
}

//virtual
BOOL LLFloaterPriceForListing::postBuild()
{
	LLLineEditor* edit = getChild<LLLineEditor>("price_edit");
	if (edit)
	{
		edit->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);
		std::string min_price = llformat("%d", gHippoGridManager->getConnectedGrid()->getClassifiedFee());
		edit->setText(min_price);
		edit->selectAll();
		edit->setFocus(TRUE);
	}

	getChild<LLUICtrl>("set_price_btn")->setCommitCallback(boost::bind(&LLFloaterPriceForListing::buttonCore, this));

	getChild<LLUICtrl>("cancel_btn")->setCommitCallback(boost::bind(&LLFloater::close, this, false));

	setDefaultBtn("set_price_btn");
	return TRUE;
}

void LLFloaterPriceForListing::buttonCore()
{
	(*mSignal)(childGetText("price_edit"));
	close();
}

void LLPanelClassifiedInfo::setDefaultAccessCombo()
{
	// PG regions should have PG classifieds. AO should have mature.

	LLViewerRegion *regionp = gAgent.getRegion();

	switch( regionp->getSimAccess() )
	{
		case SIM_ACCESS_PG:	
			mMatureCombo->setCurrentByIndex(PG_CONTENT);
			break;
		case SIM_ACCESS_ADULT:
			mMatureCombo->setCurrentByIndex(MATURE_CONTENT);
			break;
		default:
			// You are free to move about the cabin.
			break;
	}
}

//EOF
