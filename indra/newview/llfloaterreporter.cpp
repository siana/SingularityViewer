/** 
 * @file llfloaterreporter.cpp
 * @brief Abuse reports.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
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

// self include
#include "llfloaterreporter.h"

#include <sstream>

// linden library includes
#include "llassetstorage.h"
#include "llavatarnamecache.h"
#include "llcachename.h"
#include "llfontgl.h"
#include "llimagej2c.h"
#include "llinventory.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llstring.h"
#include "llsys.h"
#include "message.h"
#include "v3math.h"

// viewer project includes
#include "llagent.h"
#include "llbutton.h"
#include "lltexturectrl.h"
#include "llscrolllistctrl.h"
#include "llslurl.h"
#include "lldispatcher.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llcombobox.h"
#include "lltooldraganddrop.h"
#include "lluiconstants.h"
#include "lluploaddialog.h"
#include "llcallingcard.h"
#include "llviewerobjectlist.h"
#include "lltoolobjpicker.h"
#include "lltoolmgr.h"
#include "llresourcedata.h"		// for LLResourceData
#include "llviewerwindow.h"
#include "llviewertexturelist.h"
#include "llworldmap.h"
#include "llfloateravatarpicker.h"
#include "lldir.h"
#include "llselectmgr.h"
#include "llversioninfo.h"
#include "lluictrlfactory.h"
#include "llviewernetwork.h"

#include "llassetuploadresponders.h"
#include "llagentui.h"

#include "lltrans.h"
#include "llexperiencecache.h"

#include "llcororesponder.h"

#include "rlvhandler.h"

const U32 INCLUDE_SCREENSHOT  = 0x01 << 0;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Member functions
//-----------------------------------------------------------------------------
LLFloaterReporter::LLFloaterReporter()
:	LLFloater(),
	mReportType(COMPLAINT_REPORT),
	mObjectID(),
	mScreenID(),
	mAbuserID(),
	mOwnerName(),
	mDeselectOnClose( FALSE ),
	mPicking( FALSE), 
	mPosition(),
	mCopyrightWarningSeen( FALSE ),
	mResourceDatap(new LLResourceData()),
	mAvatarNameCacheConnection()
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_report_abuse.xml");
}

// virtual
BOOL LLFloaterReporter::postBuild()
{
	LLSLURL slurl;
	LLAgentUI::buildSLURL(slurl);
	getChild<LLUICtrl>("abuse_location_edit")->setValue(slurl.getSLURLString());

	enableControls(TRUE);

	// convert the position to a string
	LLVector3d pos = gAgent.getPositionGlobal();
	LLViewerRegion *regionp = gAgent.getRegion();
	if (regionp)
	{
		getChild<LLUICtrl>("sim_field")->setValue(regionp->getName());
		pos -= regionp->getOriginGlobal();
	}
	setPosBox(pos);

	// Take a screenshot, but don't draw this floater.
	setVisible(FALSE);
	takeScreenshot();
	setVisible(TRUE);

	// Default text to be blank
	getChild<LLUICtrl>("object_name")->setValue(LLStringUtil::null);
	getChild<LLUICtrl>("owner_name")->setValue(LLUUID::null);
	mOwnerName = LLStringUtil::null;

	getChild<LLUICtrl>("summary_edit")->setFocus(TRUE);

	mDefaultSummary = getChild<LLUICtrl>("details_edit")->getValue().asString();

	/* Singu Note: We only used this to trigger the notification that's now below, there's no point to this anymore.
	// send a message and ask for information about this region -
	// result comes back in processRegionInfo(..)
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("RequestRegionInfo");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	gAgent.sendReliableMessage();
	*/
	LLNotificationsUtil::add("HelpReportAbuseEmailLL");


	// abuser name is selected from a list
	LLUICtrl* le = getChild<LLUICtrl>("abuser_name_edit");
	le->setEnabled( false );

	LLButton* pick_btn = getChild<LLButton>("pick_btn");
	// XUI: Why aren't these in viewerart.ini?
	pick_btn->setImages(std::string("UIImgFaceUUID"),
						std::string("UIImgFaceSelectedUUID") );
	childSetAction("pick_btn", onClickObjPicker, this);

	childSetAction("select_abuser", boost::bind(&LLFloaterReporter::onClickSelectAbuser, this));

	childSetAction("send_btn", onClickSend, this);
	childSetAction("cancel_btn", onClickCancel, this);

	// grab the user's name
	getChild<LLUICtrl>("reporter_field")->setValue(gAgent.getID());

	// request categories
	if (gAgent.getRegion()
		&& gAgent.getRegion()->capabilitiesReceived())
	{
		std::string cap_url = gAgent.getRegionCapability("AbuseCategories");

		if (!cap_url.empty())
		{
			std::string lang = gSavedSettings.getString("Language");
			if (lang != "default" && !lang.empty())
			{
				cap_url += "?lc=";
				cap_url += lang;
			}
			LLHTTPClient::get(cap_url, new LLCoroResponder(
				boost::bind(LLFloaterReporter::requestAbuseCategoriesCoro, _1, cap_url, this->getHandle())));
		}
	}

	center();

	return TRUE;
}

// virtual
LLFloaterReporter::~LLFloaterReporter()
{
	if (mAvatarNameCacheConnection.connected())
	{
		mAvatarNameCacheConnection.disconnect();
	}

	// child views automatically deleted
	mObjectID 		= LLUUID::null;

	if (mPicking)
	{
		closePickTool(this);
	}

	mPosition.setVec(0.0f, 0.0f, 0.0f);

	std::for_each(mMCDList.begin(), mMCDList.end(), DeletePointer() );
	mMCDList.clear();

	delete mResourceDatap;
}

void LLFloaterReporter::enableControls(BOOL enable)
{
	getChildView("category_combo")->setEnabled(enable);
	getChildView("screenshot")->setEnabled(FALSE);
	getChildView("pick_btn")->setEnabled(enable);
	getChildView("summary_edit")->setEnabled(enable);
	getChildView("details_edit")->setEnabled(enable);
	getChildView("send_btn")->setEnabled(enable);
	getChildView("cancel_btn")->setEnabled(enable);
}

void LLFloaterReporter::getExperienceInfo(const LLUUID& experience_id)
{
	mExperienceID = experience_id;

	if (LLUUID::null != mExperienceID)
	{
        const LLSD& experience = LLExperienceCache::instance().get(mExperienceID);
		std::stringstream desc;

		if (experience.isDefined())
		{
			setFromAvatarID(experience[LLExperienceCache::AGENT_ID]);
			desc << "Experience id: " << mExperienceID;
		}
		else
		{
			desc << "Unable to retrieve details for id: "<< mExperienceID;
		}

		LLUICtrl* details = getChild<LLUICtrl>("details_edit");
		details->setValue(desc.str());
	}
}

void LLFloaterReporter::getObjectInfo(const LLUUID& object_id)
{
	// TODO -- 
	// 1 need to send to correct simulator if object is not 
	//   in same simulator as agent
	// 2 display info in widget window that gives feedback that
	//   we have recorded the object info
	// 3 can pick avatar ==> might want to indicate when a picked 
	//   object is an avatar, attachment, or other category

	mObjectID = object_id;

	if (mObjectID.notNull())
	{
		// get object info for the user's benefit
		if (LLViewerObject* objectp = gObjectList.findObject(mObjectID))
		{
			if ( objectp->isAttachment() )
			{
				objectp = (LLViewerObject*)objectp->getRoot();
				mObjectID = objectp->getID();
			}

			// correct the region and position information
			LLViewerRegion *regionp = objectp->getRegion();
			if (regionp)
			{
				getChild<LLUICtrl>("sim_field")->setValue(regionp->getName());
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
/*
				if ( (rlv_handler_t::isEnabled()) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) )
				{
					childSetText("sim_field", RlvStrings::getString(RLV_STRING_HIDDEN_REGION));
				}
*/
// [/RLVa:KB]
				LLVector3d global_pos;
				global_pos.setVec(objectp->getPositionRegion());
				setPosBox(global_pos);
			}
	
			if (objectp->isAvatar())
			{
				setFromAvatarID(mObjectID);
			}
			else
			{
				// we have to query the simulator for information 
				// about this object
				LLMessageSystem* msg = gMessageSystem;
				U32 request_flags = COMPLAINT_REPORT_REQUEST;
				msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_RequestFlags, request_flags );
				msg->addUUIDFast(_PREHASH_ObjectID, 	mObjectID);
				LLViewerRegion* regionp = objectp->getRegion();
				msg->sendReliable( regionp->getHost() );
			}
		}
	}
}

void LLFloaterReporter::onClickSelectAbuser()
{
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&LLFloaterReporter::callbackAvatarID, this, _1, _2), FALSE, TRUE );
	if (picker)
	{
		gFloaterView->getParentFloater(this)->addDependentFloater(picker);
	}
}

void LLFloaterReporter::callbackAvatarID(const uuid_vec_t& ids, const std::vector<LLAvatarName>& names)
{
	if (ids.empty() || names.empty()) return;

	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS))
		getChild<LLUICtrl>("abuser_name_edit")->setValue(RlvStrings::getString(RLV_STRING_HIDDEN));
	else
		getChild<LLUICtrl>("abuser_name_edit")->setValue(names[0].getCompleteName());
		
	mAbuserID = ids[0];

	refresh();

}

void LLFloaterReporter::setFromAvatarID(const LLUUID& avatar_id)
{
	mAbuserID = mObjectID = avatar_id;
	getChild<LLUICtrl>("owner_name")->setValue(mObjectID);

	if (mAvatarNameCacheConnection.connected())
	{
		mAvatarNameCacheConnection.disconnect();
	}
	mAvatarNameCacheConnection = LLAvatarNameCache::get(avatar_id, boost::bind(&LLFloaterReporter::onAvatarNameCache, this, _1, _2));
}

void LLFloaterReporter::onAvatarNameCache(const LLUUID& avatar_id, const LLAvatarName& av_name)
{
	mAvatarNameCacheConnection.disconnect();

	if (mObjectID == avatar_id)
	{
		mOwnerName = av_name.getNSName();
		const std::string& name(((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && RlvUtil::isNearbyAgent(avatar_id)) ? RlvStrings::getString(RLV_STRING_HIDDEN) : mOwnerName);
		getChild<LLUICtrl>("owner_name")->setValue(avatar_id);
		getChild<LLUICtrl>("object_name")->setValue(name);
		getChild<LLUICtrl>("abuser_name_edit")->setValue(name);
	}
}

void LLFloaterReporter::requestAbuseCategoriesCoro(const LLCoroResponder& responder, std::string url, LLHandle<LLFloater> handle)
{
    LLSD result = responder.getContent();

    if (!responder.isGoodStatus(responder.getStatus()) || !result.has("categories")) // success = httpResults["success"].asBoolean();
    {
        LL_WARNS() << "Error requesting Abuse Categories from capability: " << url << LL_ENDL;
        return;
    }

    if (handle.isDead())
    {
        // nothing to do
        return;
    }

    LLFloater* floater = handle.get();
    LLComboBox* combo = floater->getChild<LLComboBox>("category_combo");
    if (!combo)
    {
        LL_WARNS() << "categories category_combo not found!" << LL_ENDL;
        return;
    }

    //get selection (in case capability took a while)
    S32 selection = combo->getCurrentIndex();

    // Combobox should have a "Select category" element;
    // This is a bit of workaround since there is no proper and simple way to save array of
    // localizable strings in xml along with data (value). For now combobox is initialized along
    // with placeholders, and first element is "Select category" which we want to keep, so remove
    // everything but first element.
    // Todo: once sim with capability fully releases, just remove this string and all unnecessary
    // items from combobox since they will be obsolete (or depending on situation remake this to
    // something better, for example move "Select category" to separate string)
    while (combo->remove(1));

    LLSD contents = result["categories"];

    LLSD::array_iterator i = contents.beginArray();
    LLSD::array_iterator iEnd = contents.endArray();
    for (; i != iEnd; ++i)
    {
        const LLSD &message_data(*i);
        std::string label = message_data["description_localized"];
        const auto& cat = message_data["category"];
        combo->add(label, cat);
        switch(cat.asInteger())
        {
            // Fraud
            case 47: combo->add(floater->getString("Ridiculous3"), 1000); break;
            // Harassment
            case 51: combo->add(floater->getString("Ridiculous1"), 1000); break;
            // Land &gt; Encroachment
            case 63: combo->add(floater->getString("Ridiculous2"), 1000); break;
            default: break;
        }
    }

    //restore selection
    combo->selectNthItem(selection);
}

// static
void LLFloaterReporter::onClickSend(void *userdata)
{
	LLFloaterReporter *self = (LLFloaterReporter *)userdata;
	
	if (self->mPicking)
	{
		closePickTool(self);
	}

	if(self->validateReport())
	{
		const int IP_CONTENT_REMOVAL = 66;
		const int IP_PERMISSONS_EXPLOIT = 37;
		LLComboBox* combo = self->getChild<LLComboBox>( "category_combo");
		int category_value = combo->getSelectedValue().asInteger();

		if ( ! self->mCopyrightWarningSeen )
		{
			std::string details_lc = self->getChild<LLUICtrl>("details_edit")->getValue().asString();
			LLStringUtil::toLower( details_lc );
			std::string summary_lc = self->getChild<LLUICtrl>("summary_edit")->getValue().asString();
			LLStringUtil::toLower( summary_lc );
			if ( details_lc.find( "copyright" ) != std::string::npos ||
				summary_lc.find( "copyright" ) != std::string::npos  ||
				category_value == IP_CONTENT_REMOVAL ||
				category_value == IP_PERMISSONS_EXPLOIT)
			{
				LLNotificationsUtil::add("HelpReportAbuseContainsCopyright");
				self->mCopyrightWarningSeen = TRUE;
				return;
			}
		}
		else if (category_value == IP_CONTENT_REMOVAL)
		{
			// IP_CONTENT_REMOVAL *always* shows the dialog -
			// ergo you can never send that abuse report type.
			LLNotificationsUtil::add("HelpReportAbuseContainsCopyright");
			return;
		}

		LLUploadDialog::modalUploadDialog(LLTrans::getString("uploading_abuse_report"));
		// *TODO don't upload image if checkbox isn't checked
		std::string url = gAgent.getRegion()->getCapability("SendUserReport");
		std::string sshot_url = gAgent.getRegion()->getCapability("SendUserReportWithScreenshot");
		if(!url.empty() || !sshot_url.empty())
		{
			self->sendReportViaCaps(url, sshot_url, self->gatherReport());
			self->close();
		}
		else
		{
			self->getChildView("send_btn")->setEnabled(FALSE);
			self->getChildView("cancel_btn")->setEnabled(FALSE);
			// the callback from uploading the image calls sendReportViaLegacy()
			self->uploadImage();
		}
	}
}


// static
void LLFloaterReporter::onClickCancel(void *userdata)
{
	LLFloaterReporter *self = (LLFloaterReporter *)userdata;

	// reset flag in case the next report also contains this text
	self->mCopyrightWarningSeen = FALSE;

	if (self->mPicking)
	{
		closePickTool(self);
	}
	self->close();
}


// static 
void LLFloaterReporter::onClickObjPicker(void *userdata)
{
	LLFloaterReporter *self = (LLFloaterReporter *)userdata;
	LLToolObjPicker::getInstance()->setExitCallback(LLFloaterReporter::closePickTool, self);
	LLToolMgr::getInstance()->setTransientTool(LLToolObjPicker::getInstance());
	self->mPicking = TRUE;
	self->getChild<LLUICtrl>("object_name")->setValue(LLStringUtil::null);
	self->getChild<LLUICtrl>("owner_name")->setValue(LLUUID::null);
	self->mOwnerName = LLStringUtil::null;
	LLButton* pick_btn = self->getChild<LLButton>("pick_btn");
	if (pick_btn) pick_btn->setToggleState(TRUE);
}


// static
void LLFloaterReporter::closePickTool(void *userdata)
{
	LLFloaterReporter *self = (LLFloaterReporter *)userdata;

	LLUUID object_id = LLToolObjPicker::getInstance()->getObjectID();
	self->getObjectInfo(object_id);

	LLToolMgr::getInstance()->clearTransientTool();
	self->mPicking = FALSE;
	LLButton* pick_btn = self->getChild<LLButton>("pick_btn");
	if (pick_btn) pick_btn->setToggleState(FALSE);
}


// static
void LLFloaterReporter::showFromMenu(EReportType report_type)
{
	if (COMPLAINT_REPORT != report_type)
	{
		LL_WARNS() << "Unknown LLViewerReporter type : " << report_type << LL_ENDL;
		return;
	}

	// Prevent menu from appearing in screen shot.
	LLMenuGL::sMenuContainer->hideMenus();
	LLFloaterReporter* f = getInstance();
	if (f)
	{
		f->setReportType(report_type);
	}
}

// static
void LLFloaterReporter::show(const LLUUID& object_id, const std::string& avatar_name, const LLUUID& experience_id)
{
	LLFloaterReporter* f = getInstance();

	if (avatar_name.empty())
	{
		// Request info for this object
		f->getObjectInfo(object_id);
	}
	else
	{
		f->setFromAvatarID(object_id);
	}
	if (experience_id.notNull())
	{
		f->getExperienceInfo(experience_id);
	}

	// Need to deselect on close
	f->mDeselectOnClose = TRUE;

	f->open();		/* Flawfinder: ignore */
}



void LLFloaterReporter::showFromExperience(const LLUUID& experience_id)
{
	LLFloaterReporter* f = getInstance();
	f->getExperienceInfo(experience_id);

	// Need to deselect on close
	f->mDeselectOnClose = TRUE;

	f->open();		/* Flawfinder: ignore */
}


// static
void LLFloaterReporter::showFromObject(const LLUUID& object_id, const LLUUID& experience_id)
{
	show(object_id, LLStringUtil::null, experience_id);
}

// static
void LLFloaterReporter::showFromAvatar(const LLUUID& avatar_id, const std::string avatar_name)
{
	show(avatar_id, avatar_name);
}

void LLFloaterReporter::setPickedObjectProperties(const std::string& object_name, const std::string& owner_name, const LLUUID owner_id)
{
	getChild<LLUICtrl>("object_name")->setValue(object_name);


	if ((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && RlvUtil::isNearbyAgent(owner_id))
	{
		const std::string& rlv_hidden(RlvStrings::getString(RLV_STRING_HIDDEN));
		getChild<LLUICtrl>("owner_name")->setValue(rlv_hidden);
		getChild<LLUICtrl>("abuser_name_edit")->setValue(rlv_hidden);
	}
	else
	{
		getChild<LLUICtrl>("owner_name")->setValue(owner_id);
		getChild<LLUICtrl>("abuser_name_edit")->setValue(owner_name);
	}
	mAbuserID = owner_id;
	mOwnerName = owner_name;
}


bool LLFloaterReporter::validateReport()
{
	// Ensure user selected a category from the list
	LLSD category_sd = getChild<LLUICtrl>("category_combo")->getValue();
	U8 category = (U8)category_sd.asInteger();
	if(category == 1000) //This is here for reasons (like shenanigans)
	{
		LLNotificationsUtil::add("HelpReportNope");
		return false;
	}
	if (category == 0)
	{
		LLNotificationsUtil::add("HelpReportAbuseSelectCategory");
		return false;
	}


	if ( getChild<LLUICtrl>("abuser_name_edit")->getValue().asString().empty() )
	{
		LLNotificationsUtil::add("HelpReportAbuseAbuserNameEmpty");
		return false;
	}

	if ( getChild<LLUICtrl>("abuse_location_edit")->getValue().asString().empty() )
	{
		LLNotificationsUtil::add("HelpReportAbuseAbuserLocationEmpty");
		return false;
	}

	if ( getChild<LLUICtrl>("abuse_location_edit")->getValue().asString().empty() )
	{
		LLNotificationsUtil::add("HelpReportAbuseAbuserLocationEmpty");
		return false;
	}


	if ( getChild<LLUICtrl>("summary_edit")->getValue().asString().empty() )
	{
		LLNotificationsUtil::add("HelpReportAbuseSummaryEmpty");
		return false;
	};

	if ( getChild<LLUICtrl>("details_edit")->getValue().asString() == mDefaultSummary )
	{
			LLNotificationsUtil::add("HelpReportAbuseDetailsEmpty");
		return false;
	};
	return true;
}

LLSD LLFloaterReporter::gatherReport()
{	
	LLViewerRegion *regionp = gAgent.getRegion();
	if (!regionp) return LLSD(); // *TODO handle this failure case more gracefully

	// reset flag in case the next report also contains this text
	mCopyrightWarningSeen = FALSE;

	std::ostringstream summary;
	if (!LLViewerLogin::getInstance()->isInProductionGrid())
	{
		summary << "Preview ";
	}

	std::string category_name;
	LLComboBox* combo = getChild<LLComboBox>( "category_combo");
	if (combo)
	{
		category_name = combo->getSelectedItemLabel(); // want label, not value
	}

#if LL_WINDOWS
	const char* platform = "Win";
#elif LL_DARWIN
	const char* platform = "Mac";
#elif LL_LINUX
	const char* platform = "Lnx";
#elif LL_SOLARIS
	const char* platform = "Sol";
	const char* short_platform = "O:S";
#else
	const char* platform = "???";
#endif



	summary << ""
		<< " |" << regionp->getName() << "|"								// region reporter is currently in.
		<< " (" << getChild<LLUICtrl>("abuse_location_edit")->getValue().asString() << ")"				// region abuse occured in (freeform text - no LLRegionPicker tool)
		<< " [" << category_name << "] "									// updated category
		<< " {" << getChild<LLUICtrl>("abuser_name_edit")->getValue().asString() << "} "					// name of abuse entered in report (chosen using LLAvatarPicker)
		<< " \"" << getChild<LLUICtrl>("summary_edit")->getValue().asString() << "\"";		// summary as entered


	std::ostringstream details;

	details << 'V' << LLVersionInfo::getVersion() << "\n\n";	// client version moved to body of email for abuse reports

	std::string object_name = getChild<LLUICtrl>("object_name")->getValue().asString();
	if (!object_name.empty() && !mOwnerName.empty())
	{
		details << "Object: " << object_name << "\n";
		details << "Owner: " << mOwnerName << "\n";
	}


	details << "Abuser name: " << getChild<LLUICtrl>("abuser_name_edit")->getValue().asString() << " \n";
	details << "Abuser location: " << getChild<LLUICtrl>("abuse_location_edit")->getValue().asString() << " \n";

	details << getChild<LLUICtrl>("details_edit")->getValue().asString();

	std::string version_string;
	version_string = llformat(
			"%s %s %s %s %s",
			LLVersionInfo::getShortVersion().c_str(),
			platform,
			gSysCPU.getFamily().c_str(),
			gGLManager.mGLRenderer.c_str(),
			gGLManager.mDriverVersionVendorString.c_str());

	// only send a screenshot ID if we're asked to and the email is
	// going to LL - Estate Owners cannot see the screenshot asset
	LLUUID screenshot_id = getChild<LLUICtrl>("screenshot")->getValue().asUUID();

	LLSD report = LLSD::emptyMap();
	report["report-type"] = (U8) mReportType;
	report["category"] = getChild<LLUICtrl>("category_combo")->getValue();
	report["position"] = mPosition.getValue();
	report["check-flags"] = (U8)0; // this is not used
	report["screenshot-id"] = screenshot_id;
	report["object-id"] = mObjectID;
	report["abuser-id"] = mAbuserID;
	report["abuse-region-name"] = "";
	report["abuse-region-id"] = LLUUID::null;
	report["summary"] = summary.str();
	report["version-string"] = version_string;
	report["details"] = details.str();
	return report;
}

void LLFloaterReporter::sendReportViaLegacy(const LLSD & report)
{
	LLViewerRegion *regionp = gAgent.getRegion();
	if (!regionp) return;
	LLMessageSystem *msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_UserReport);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	
	msg->nextBlockFast(_PREHASH_ReportData);
	msg->addU8Fast(_PREHASH_ReportType, report["report-type"].asInteger());
	msg->addU8(_PREHASH_Category, report["category"].asInteger());
	msg->addVector3Fast(_PREHASH_Position, 	LLVector3(report["position"]));
	msg->addU8Fast(_PREHASH_CheckFlags, report["check-flags"].asInteger());
	msg->addUUIDFast(_PREHASH_ScreenshotID, report["screenshot-id"].asUUID());
	msg->addUUIDFast(_PREHASH_ObjectID, report["object-id"].asUUID());
	msg->addUUID("AbuserID", report["abuser-id"].asUUID());
	msg->addString("AbuseRegionName", report["abuse-region-name"].asString());
	msg->addUUID("AbuseRegionID", report["abuse-region-id"].asUUID());

	msg->addStringFast(_PREHASH_Summary, report["summary"].asString());
	msg->addString("VersionString", report["version-string"]);
	msg->addStringFast(_PREHASH_Details, report["details"] );
	
	msg->sendReliable(regionp->getHost());
}

class LLUserReportScreenshotResponder final : public LLAssetUploadResponder
{
public:
	LLUserReportScreenshotResponder(const LLSD & post_data, 
									const LLUUID & vfile_id, 
									LLAssetType::EType asset_type):
	LLAssetUploadResponder(post_data, vfile_id, asset_type)
	{
	}
	void uploadFailed(const LLSD& content)
	{
		// *TODO pop up a dialog so the user knows their report screenshot didn't make it
		LLUploadDialog::modalUploadFinished();
	}
	void uploadComplete(const LLSD& content)
	{
		// we don't care about what the server returns from this post, just clean up the UI
		LLUploadDialog::modalUploadFinished();
	}

	char const* getName() const override { return "LLUserReportScreenshotResponder"; }
};

class LLUserReportResponder final : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(LLUserReportResponder);
public:
	LLUserReportResponder() { }

private:
	void httpCompleted() override
	{
		if (!isGoodStatus(mStatus))
		{
			// *TODO do some user messaging here
			LL_WARNS("UserReport") << dumpResponse() << LL_ENDL;
		}
		// we don't care about what the server returns
		LLUploadDialog::modalUploadFinished();
	}
	char const* getName() const override { return "LLUserReportResponder"; }
};

void LLFloaterReporter::sendReportViaCaps(std::string url, std::string sshot_url, const LLSD& report)
{
	if(!sshot_url.empty())
	{
		// try to upload screenshot
		LLHTTPClient::post(sshot_url, report, new LLUserReportScreenshotResponder(report, 
															mResourceDatap->mAssetInfo.mUuid, 
															mResourceDatap->mAssetInfo.mType));			
	}
	else
	{
		// screenshot not wanted or we don't have screenshot cap
		LLHTTPClient::post(url, report, new LLUserReportResponder);			
	}
}

void LLFloaterReporter::takeScreenshot()
{
	// Warning: This crops left and right in case of wide-screen monitor:
	const S32 IMAGE_WIDTH = 1024; //gViewerWindow->getWindowWidthRaw();
	const S32 IMAGE_HEIGHT = 768; //gViewerWindow->getWindowHeightRaw();

	LLPointer<LLImageRaw> raw = new LLImageRaw;
	if( !gViewerWindow->rawSnapshot(raw, IMAGE_WIDTH, IMAGE_HEIGHT, (F32)IMAGE_WIDTH / IMAGE_HEIGHT, TRUE, FALSE))
	{
		LL_WARNS() << "Unable to take screenshot" << LL_ENDL;
		return;
	}
	LLPointer<LLImageJ2C> upload_data = LLViewerTextureList::convertToUploadFile(raw);

	// create a resource data
	mResourceDatap->mInventoryType = LLInventoryType::IT_NONE;
	mResourceDatap->mNextOwnerPerm = 0; // not used
	mResourceDatap->mExpectedUploadCost = 0; // we expect that abuse screenshots are free
	mResourceDatap->mAssetInfo.mTransactionID.generate();
	mResourceDatap->mAssetInfo.mUuid = mResourceDatap->mAssetInfo.mTransactionID.makeAssetID(gAgent.getSecureSessionID());

	if (COMPLAINT_REPORT == mReportType)
	{
		mResourceDatap->mAssetInfo.mType = LLAssetType::AT_TEXTURE;
		mResourceDatap->mPreferredLocation = LLFolderType::EType(LLResourceData::INVALID_LOCATION);
	}
	else
	{
		LL_WARNS() << "Unknown LLFloaterReporter type" << LL_ENDL;
	}
	mResourceDatap->mAssetInfo.mCreatorID = gAgentID;
	mResourceDatap->mAssetInfo.setName("screenshot_name");
	mResourceDatap->mAssetInfo.setDescription("screenshot_descr");

	// store in VFS
	LLVFile::writeFile(upload_data->getData(), 
						upload_data->getDataSize(), 
						gVFS, 
						mResourceDatap->mAssetInfo.mUuid, 
						mResourceDatap->mAssetInfo.mType);

	// store in the image list so it doesn't try to fetch from the server
	LLPointer<LLViewerFetchedTexture> image_in_list = 
		LLViewerTextureManager::getFetchedTexture(mResourceDatap->mAssetInfo.mUuid, FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_NONE, LLViewerTexture::FETCHED_TEXTURE);
	image_in_list->createGLTexture(0, raw, nullptr, TRUE, LLViewerTexture::OTHER);

	// the texture picker then uses that texture
	LLTextureCtrl* texture = getChild<LLTextureCtrl>("screenshot");
	if (texture)
	{
		texture->setImageAssetID(mResourceDatap->mAssetInfo.mUuid);
		texture->setDefaultImageAssetID(mResourceDatap->mAssetInfo.mUuid);
		texture->setCaption(getString("Screenshot"));
	}

}

void LLFloaterReporter::uploadImage()
{
	LL_INFOS() << "*** Uploading: " << LL_ENDL;
	LL_INFOS() << "Type: " << LLAssetType::lookup(mResourceDatap->mAssetInfo.mType) << LL_ENDL;
	LL_INFOS() << "UUID: " << mResourceDatap->mAssetInfo.mUuid << LL_ENDL;
	LL_INFOS() << "Name: " << mResourceDatap->mAssetInfo.getName() << LL_ENDL;
	LL_INFOS() << "Desc: " << mResourceDatap->mAssetInfo.getDescription() << LL_ENDL;

	gAssetStorage->storeAssetData(mResourceDatap->mAssetInfo.mTransactionID,
									mResourceDatap->mAssetInfo.mType,
									LLFloaterReporter::uploadDoneCallback,
									(void*)mResourceDatap, TRUE);
}


// static
void LLFloaterReporter::uploadDoneCallback(const LLUUID &uuid, void *user_data, S32 result, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLUploadDialog::modalUploadFinished();

	LLResourceData* data = (LLResourceData*)user_data;

	if(result < 0)
	{
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
		LLNotificationsUtil::add("ErrorUploadingReportScreenshot", args);

		std::string err_msg("There was a problem uploading a report screenshot");
		err_msg += " due to the following reason: " + args["REASON"].asString();
		LL_WARNS() << err_msg << LL_ENDL;
		return;
	}

	EReportType report_type = UNKNOWN_REPORT;
	if (data->mPreferredLocation == LLResourceData::INVALID_LOCATION)
	{
		report_type = COMPLAINT_REPORT;
	}
	else 
	{
		LL_WARNS() << "Unknown report type : " << data->mPreferredLocation << LL_ENDL;
	}

	LLFloaterReporter *self = getInstance();
	if (self)
	{
		self->mScreenID = uuid;
		LL_INFOS() << "Got screen shot " << uuid << LL_ENDL;
		self->sendReportViaLegacy(self->gatherReport());
		self->close();
	}
}


void LLFloaterReporter::setPosBox(const LLVector3d &pos)
{
	mPosition.setVec(pos);
	std::string pos_string = llformat("{%.1f, %.1f, %.1f}",
		mPosition.mV[VX],
		mPosition.mV[VY],
		mPosition.mV[VZ]);
	getChild<LLUICtrl>("pos_field")->setValue(pos_string);
}


//void LLFloaterReporter::setDescription(const std::string& description, LLMeanCollisionData *mcd)
//{
//	LLFloaterReporter *self = getInstance();
//	if (self)
//	{
// 		self->getChild<LLUICtrl>("details_edit")->setValue(description);

//		for_each(self->mMCDList.begin(), self->mMCDList.end(), DeletePointer());
//		self->mMCDList.clear();
//		if (mcd)
//		{
//			self->mMCDList.push_back(new LLMeanCollisionData(mcd));
//		}
//	}
//}

//void LLFloaterReporter::addDescription(const std::string& description, LLMeanCollisionData *mcd)
//{
//	LLFloaterReporter *self = getInstance();
//	if (self)
//	{
//		LLTextEditor* text = self->getChild<LLTextEditor>("details_edit");
//		if (text)
//		{
//			text->insertText(description);
//		}
//		if (mcd)
//		{
//			self->mMCDList.push_back(new LLMeanCollisionData(mcd));
//		}
//	}
//}
