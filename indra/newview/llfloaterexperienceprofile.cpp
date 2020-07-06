/** 
 * @file llfloaterexperienceprofile.cpp
 * @brief llfloaterexperienceprofile and related class definitions
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
#include "llfloaterexperienceprofile.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llavatarnamecache.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcommandhandler.h"
//#include "llexpandabletextbox.h"
#include "llexperiencecache.h"
//#include "llfloaterreg.h"
#include "llgroupactions.h"
#include "lluictrlfactory.h"
#include "lllayoutstack.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llslurl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltrans.h"
#include "llviewerregion.h"
#include "llevents.h"
#include "llfloatergroups.h"
#include "llnotifications.h"
#include "llfloaterreporter.h"
#include "llurlaction.h"

#define XML_PANEL_EXPERIENCE_PROFILE "floater_experienceprofile.xml"
#define TF_NAME "experience_title"
#define TF_DESC "experience_description"
#define TF_SLURL "LocationTextText"
#define TF_MRKT "marketplace"
#define TF_MATURITY "ContentRatingText"
#define TF_OWNER "OwnerText"
#define TF_GROUP "GroupText"
#define TF_GRID_WIDE "grid_wide"
#define TF_PRIVILEGED "privileged"
#define EDIT "edit_"

#define IMG_LOGO "logo"

#define PNL_TOP "top panel"
#define PNL_IMAGE "image_panel"
#define PNL_DESC "description panel"
#define PNL_LOC "location panel"
#define PNL_MRKT "marketplace panel"
#define PNL_GROUP "group_panel"
#define PNL_PERMS "perm panel"

#define BTN_ALLOW "allow_btn"
#define BTN_BLOCK "block_btn"
#define BTN_CANCEL "cancel_btn"
#define BTN_CLEAR_LOCATION "clear_btn"
#define BTN_EDIT "edit_btn"
#define BTN_ENABLE "enable_btn"
#define BTN_FORGET "forget_btn"
#define BTN_PRIVATE "private_btn"
#define BTN_REPORT "report_btn"
#define BTN_SAVE "save_btn"
#define BTN_SET_GROUP "Group_btn"
#define BTN_SET_LOCATION "location_btn"


class LLExperienceHandler : public LLCommandHandler
{
public:
	LLExperienceHandler() : LLCommandHandler("experience", UNTRUSTED_THROTTLE) { }

	bool handle(const LLSD& params, const LLSD& query_map,
        LLMediaCtrl* web) override
	{
		if (params.size() != 2 || params[1].asString() != "profile")
			return false;

        LLExperienceCache::instance().get(params[0].asUUID(), boost::bind(&LLExperienceHandler::experienceCallback, this, _1));
		return true;
	}

	void experienceCallback(const LLSD& experienceDetails)
	{
		if (!experienceDetails.has(LLExperienceCache::MISSING))
		{
            LLFloaterExperienceProfile::showInstance(experienceDetails[LLExperienceCache::EXPERIENCE_ID].asUUID());
		}
	}
};

LLExperienceHandler gExperienceHandler;

void LLFloaterExperienceProfile::showInstance(const LLSD& data)
{
	bool is_map = data.has("experience_id");
	LLFloaterExperienceProfile* floater = getInstance(is_map ? data["experience_id"].asUUID() : data.asUUID());
	if (!floater) floater = new LLFloaterExperienceProfile(data);
	else if (is_map && data.has("edit_experience") && data["edit_experience"].asBoolean())
		floater->changeToEdit();
	floater->open();
}

LLFloaterExperienceProfile::LLFloaterExperienceProfile(const LLSD& data)
    : LLFloater(), LLInstanceTracker<LLFloaterExperienceProfile, LLUUID>(data.asUUID())
    , mSaveCompleteAction(NOTHING)
    , mDirty(false)
    , mForceClose(false)
{
    if (data.has("experience_id"))
{
        mExperienceId = data["experience_id"].asUUID();
        mPostEdit = data.has("edit_experience") && data["edit_experience"].asBoolean();
}
    else
{
        mExperienceId = data.asUUID();
        mPostEdit = false;
	}
	LLUICtrlFactory::getInstance()->buildFloater(this, XML_PANEL_EXPERIENCE_PROFILE, NULL, false);
	//buildFromFile(XML_PANEL_EXPERIENCE_PROFILE);
}


LLFloaterExperienceProfile::~LLFloaterExperienceProfile()
{

}

BOOL LLFloaterExperienceProfile::postBuild()
{

	if (mExperienceId.notNull())
	{
        LLExperienceCache::instance().fetch(mExperienceId, true);
        LLExperienceCache::instance().get(mExperienceId, boost::bind(&LLFloaterExperienceProfile::experienceCallback,
					getDerivedHandle<LLFloaterExperienceProfile>(), _1)); 

		LLViewerRegion* region = gAgent.getRegion();
		if (region)
		{
            LLExperienceCache::instance().getExperienceAdmin(mExperienceId, boost::bind(
                &LLFloaterExperienceProfile::experienceIsAdmin, getDerivedHandle<LLFloaterExperienceProfile>(), _1));
		}
	}

	childSetAction(BTN_EDIT, boost::bind(&LLFloaterExperienceProfile::onClickEdit, this));
	childSetAction(BTN_ALLOW, boost::bind(&LLFloaterExperienceProfile::onClickPermission, this, "Allow"));
	childSetAction(BTN_FORGET, boost::bind(&LLFloaterExperienceProfile::onClickForget, this));
	childSetAction(BTN_BLOCK, boost::bind(&LLFloaterExperienceProfile::onClickPermission, this, "Block"));
	childSetAction(BTN_CANCEL, boost::bind(&LLFloaterExperienceProfile::onClickCancel, this));
	childSetAction(BTN_SAVE, boost::bind(&LLFloaterExperienceProfile::onClickSave, this));
	childSetAction(BTN_SET_LOCATION, boost::bind(&LLFloaterExperienceProfile::onClickLocation, this));
	childSetAction(BTN_CLEAR_LOCATION, boost::bind(&LLFloaterExperienceProfile::onClickClear, this));
	childSetAction(BTN_SET_GROUP, boost::bind(&LLFloaterExperienceProfile::onPickGroup, this));
	childSetAction(BTN_REPORT, boost::bind(&LLFloaterExperienceProfile::onReportExperience, this));

	getChild<LLTextEditor>(EDIT TF_DESC)->setKeystrokeCallback(boost::bind(&LLFloaterExperienceProfile::onFieldChanged, this));
	getChild<LLUICtrl>(EDIT TF_MATURITY)->setCommitCallback(boost::bind(&LLFloaterExperienceProfile::onFieldChanged, this));
    getChild<LLLineEditor>(EDIT TF_MRKT)->setKeystrokeCallback(boost::bind(&LLFloaterExperienceProfile::onFieldChanged, this));
    getChild<LLLineEditor>(EDIT TF_NAME)->setKeystrokeCallback(boost::bind(&LLFloaterExperienceProfile::onFieldChanged, this));

    childSetCommitCallback(EDIT BTN_ENABLE, boost::bind(&LLFloaterExperienceProfile::onFieldChanged, this), nullptr);
    childSetCommitCallback(EDIT BTN_PRIVATE, boost::bind(&LLFloaterExperienceProfile::onFieldChanged, this), nullptr);

    childSetCommitCallback(EDIT IMG_LOGO, boost::bind(&LLFloaterExperienceProfile::onFieldChanged, this), nullptr);

    if (auto logo = findChild<LLTextureCtrl>(IMG_LOGO))
    {
        void show_picture(const LLUUID& id, const std::string& name);
        LLTextBox* name = getChild<LLTextBox>(TF_NAME);
        std::function<void()> cb = [logo, name]() { show_picture(logo->getImageAssetID(), "Experience Picture: " + name->getText()); };
        logo->setMouseUpCallback(boost::bind(cb));
    }

	getChild<LLTextEditor>(EDIT TF_DESC)->setCommitOnFocusLost(TRUE);

	LLEventPumps::instance().obtain("experience_permission").listen(mExperienceId.asString()+"-profile", 
			boost::bind(&LLFloaterExperienceProfile::experiencePermission, getDerivedHandle<LLFloaterExperienceProfile>(this), _1));

    if (mPostEdit && mExperienceId.notNull())
    {
        mPostEdit = false;
        changeToEdit();
    }

	return TRUE;
}

void LLFloaterExperienceProfile::experienceCallback(LLHandle<LLFloaterExperienceProfile> handle,  const LLSD& experience )
{
	LLFloaterExperienceProfile* pllpep = handle.get();
	if (pllpep)
	{
		pllpep->refreshExperience(experience);
	}
}


bool LLFloaterExperienceProfile::experiencePermission( LLHandle<LLFloaterExperienceProfile> handle, const LLSD& permission )
{
	LLFloaterExperienceProfile* pllpep = handle.get();
	if (pllpep)
	{
		pllpep->updatePermission(permission);
	}
	return false;
}


void LLFloaterExperienceProfile::onClickEdit()
{
    changeToEdit();
}


void LLFloaterExperienceProfile::onClickCancel()
{
	changeToView();
}

void LLFloaterExperienceProfile::onClickSave()
{
	doSave(NOTHING);
}

void LLFloaterExperienceProfile::onClickPermission(const char* perm)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
		return;
    LLExperienceCache::instance().setExperiencePermission(mExperienceId, perm, boost::bind(
        &LLFloaterExperienceProfile::experiencePermissionResults, mExperienceId, _1));
}


void LLFloaterExperienceProfile::onClickForget()
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
		return;

    LLExperienceCache::instance().forgetExperiencePermission(mExperienceId, boost::bind(
        &LLFloaterExperienceProfile::experiencePermissionResults, mExperienceId, _1));
}

bool LLFloaterExperienceProfile::setMaturityString( U8 maturity, LLTextBox* child, LLComboBox* combo )
{
	//LLStyle::Params style; // Singu Note: Nope.
	std::string access;
	if (maturity <= SIM_ACCESS_PG)
	{
		//style.image(LLUI::getUIImage(getString("maturity_icon_general"))); // Singu Note: Nope.
		access = LLTrans::getString("SIM_ACCESS_PG");
		combo->setCurrentByIndex(2);
	}
	else if (maturity <= SIM_ACCESS_MATURE)
	{
		//style.image(LLUI::getUIImage(getString("maturity_icon_moderate"))); // Singu Note: Nope.
		access = LLTrans::getString("SIM_ACCESS_MATURE");
		combo->setCurrentByIndex(1);
	}
	else if (maturity <= SIM_ACCESS_ADULT)
	{
		//style.image(LLUI::getUIImage(getString("maturity_icon_adult"))); // Singu Note: Nope.
		access = LLTrans::getString("SIM_ACCESS_ADULT");
		combo->setCurrentByIndex(0);
	}
	else
	{
		return false;
	}

	/* Singu Note: Nope.
	child->setText(LLStringUtil::null);

	child->appendImageSegment(style);
	*/
	child->setText(access);

	return true;
}


void LLFloaterExperienceProfile::refreshExperience( const LLSD& experience )
{
	mExperienceDetails = experience;
	mPackage = experience;


    LLLayoutPanel* imagePanel = getChild<LLLayoutPanel>(PNL_IMAGE);
    LLLayoutPanel* descriptionPanel = getChild<LLLayoutPanel>(PNL_DESC);
    LLLayoutPanel* locationPanel = getChild<LLLayoutPanel>(PNL_LOC);
    LLLayoutPanel* marketplacePanel = getChild<LLLayoutPanel>(PNL_MRKT);  
    LLLayoutPanel* topPanel = getChild<LLLayoutPanel>(PNL_TOP);  


	imagePanel->setVisible(FALSE);
	descriptionPanel->setVisible(FALSE);
	locationPanel->setVisible(FALSE);
	marketplacePanel->setVisible(FALSE);
	topPanel->setVisible(FALSE);


	LLTextBox* child = getChild<LLTextBox>(TF_NAME);
	//child->setText(experience[LLExperienceCache::NAME].asString());
	child->setValue(experience[LLExperienceCache::EXPERIENCE_ID].asUUID());

	LLLineEditor* linechild = getChild<LLLineEditor>(EDIT TF_NAME);
	linechild->setText(experience[LLExperienceCache::NAME].asString());

	std::string value = experience[LLExperienceCache::DESCRIPTION].asString();
	LLTextEditor* exchild = getChild<LLTextEditor>(TF_DESC);
	exchild->setText(value);
	descriptionPanel->setVisible(value.length()>0);

	LLTextEditor* edit_child = getChild<LLTextEditor>(EDIT TF_DESC);
	edit_child->setText(value);

	mLocationSLURL = experience[LLExperienceCache::SLURL].asString();
	edit_child = getChild<LLTextEditor>(TF_SLURL);
	bool has_slurl = !mLocationSLURL.empty() && mLocationSLURL != "last";
	locationPanel->setVisible(has_slurl);
	if (has_slurl) mLocationSLURL = LLSLURL(mLocationSLURL).getSLURLString();
	edit_child->setText(mLocationSLURL);


	edit_child = getChild<LLTextEditor>(EDIT TF_SLURL);
	if (has_slurl)
	{
		edit_child->setText(mLocationSLURL);
	}
	else
	{
		edit_child->setText(getString("empty_slurl"));
	}

	setMaturityString((U8)(experience[LLExperienceCache::MATURITY].asInteger()), getChild<LLTextBox>(TF_MATURITY), getChild<LLComboBox>(EDIT TF_MATURITY));

	LLUUID agent_id = experience[LLExperienceCache::AGENT_ID].asUUID();
	getChild<LLTextBox>(TF_OWNER)->setValue(agent_id);

	LLUUID id = experience[LLExperienceCache::GROUP_ID].asUUID();
	bool id_null = id.isNull();
	if (!id_null)
	{
		getChild<LLTextBox>(TF_GROUP)->setValue(id);
	}
	getChild<LLLayoutPanel>(PNL_GROUP)->setVisible(!id_null);

	setEditGroup(id);

	getChild<LLButton>(BTN_SET_GROUP)->setEnabled(agent_id == gAgentID);

	LLCheckBoxCtrl* enable = getChild<LLCheckBoxCtrl>(EDIT BTN_ENABLE);
	S32 properties = mExperienceDetails[LLExperienceCache::PROPERTIES].asInteger();
	enable->set(!(properties & LLExperienceCache::PROPERTY_DISABLED)); 

	enable = getChild<LLCheckBoxCtrl>(EDIT BTN_PRIVATE);
	enable->set(properties & LLExperienceCache::PROPERTY_PRIVATE);  

	topPanel->setVisible(TRUE);
	child=getChild<LLTextBox>(TF_GRID_WIDE);
	child->setVisible(TRUE);

    if(properties & LLExperienceCache::PROPERTY_GRID)
    {
        child->setText(LLTrans::getString("Grid-Scope"));
    }
	else 
	{
        child->setText(LLTrans::getString("Land-Scope"));
	}

	if (getChild<LLButton>(BTN_EDIT)->getVisible())
	{
		topPanel->setVisible(TRUE);
	}

	if (properties & LLExperienceCache::PROPERTY_PRIVILEGED)
	{
		child = getChild<LLTextBox>(TF_PRIVILEGED);
		child->setVisible(TRUE);
	}
	else
	{
		LLViewerRegion* region = gAgent.getRegion();
		if (region)
		{
            LLExperienceCache::instance().getExperiencePermission(mExperienceId, boost::bind(
                &LLFloaterExperienceProfile::experiencePermissionResults, mExperienceId, _1));
		}
	}

	value=experience[LLExperienceCache::METADATA].asString();
	if (value.empty())
		return;

	LLPointer<LLSDParser> parser = new LLSDXMLParser();

	LLSD data;

	std::istringstream is(value);
	if (LLSDParser::PARSE_FAILURE != parser->parse(is, data, value.size()))
	{
        value.clear();
		if (data.has(TF_MRKT))
		{
			value=data[TF_MRKT].asString();

			edit_child = getChild<LLTextEditor>(TF_MRKT);
			edit_child->setText(value);
            if(!value.empty())
			{
				marketplacePanel->setVisible(TRUE);
			}
			else
			{
				marketplacePanel->setVisible(FALSE);
			}
		}
		else
		{
			marketplacePanel->setVisible(FALSE);
		}

		linechild = getChild<LLLineEditor>(EDIT TF_MRKT);
		linechild->setText(value);

		if (data.has(IMG_LOGO))
		{
			LLTextureCtrl* logo = getChild<LLTextureCtrl>(IMG_LOGO);

			LLUUID id = data[IMG_LOGO].asUUID();
			logo->setImageAssetID(id);
			imagePanel->setVisible(TRUE);

			logo = getChild<LLTextureCtrl>(EDIT IMG_LOGO);
			logo->setImageAssetID(data[IMG_LOGO].asUUID());

			imagePanel->setVisible(id.notNull());
		}
	}
	else
	{
		marketplacePanel->setVisible(FALSE);
		imagePanel->setVisible(FALSE);
	}

	mDirty=false;
	mForceClose = false;
	getChild<LLButton>(BTN_SAVE)->setEnabled(mDirty);
}

void LLFloaterExperienceProfile::setPreferences( const LLSD& content )
{
	S32 properties = mExperienceDetails[LLExperienceCache::PROPERTIES].asInteger();
	if (properties & LLExperienceCache::PROPERTY_PRIVILEGED)
	{
		return;
	}

	const LLSD& experiences = content["experiences"];
	const LLSD& blocked = content["blocked"];


	for(const auto& exp : experiences.array())
	{
		if (exp.asUUID()==mExperienceId)
		{
			experienceAllowed();
			return;
		}
	}

	for(const auto& exp : blocked.array())
	{
		if (exp.asUUID()==mExperienceId)
		{
			experienceBlocked();
			return;
		}
	}

	experienceForgotten();
}

void LLFloaterExperienceProfile::onFieldChanged()
{
	updatePackage();

	if (!getChild<LLButton>(BTN_EDIT)->getVisible())
	{
		return;
	}

	mDirty = mPackage.size() != mExperienceDetails.size();
	if (!mDirty)
	{
		LLSD::map_const_iterator st = mExperienceDetails.beginMap();
		LLSD::map_const_iterator dt = mPackage.beginMap();
		LLSD::map_const_iterator ste = mExperienceDetails.endMap();
		LLSD::map_const_iterator dte = mPackage.endMap();
		for (; st != ste && dt != dte; ++st, ++dt)
			if (mDirty = st->first != dt->first || st->second.asString() != dt->second.asString())
				break;
	}

	getChild<LLButton>(BTN_SAVE)->setEnabled(mDirty);
}


BOOL LLFloaterExperienceProfile::canClose()
{
	if (mForceClose || !mDirty)
	{
		return TRUE;
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotificationsUtil::add("SaveChanges", LLSD(), LLSD(), boost::bind(&LLFloaterExperienceProfile::handleSaveChangesDialog, this, _1, _2, CLOSE));
		return FALSE;
	}
}

bool LLFloaterExperienceProfile::handleSaveChangesDialog( const LLSD& notification, const LLSD& response, PostSaveAction action )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch( option )
	{
	case 0:  // "Yes"
		// close after saving
		doSave( action );
		break;

	case 1:  // "No"
		if (action != NOTHING)
		{
			mForceClose = TRUE;
			if (action==CLOSE)
			{
				close();
			}
			else
			{
				changeToView();
			}
		}
		break;

	case 2: // "Cancel"
	default:
		// If we were quitting, we didn't really mean it.
		LLAppViewer::instance()->abortQuit();
		break;
	}
	return false;
}

void LLFloaterExperienceProfile::doSave( int success_action )
{
	mSaveCompleteAction=success_action;

	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
		return;

    LLExperienceCache::instance().updateExperience(mPackage, boost::bind(
            &LLFloaterExperienceProfile::experienceUpdateResult, 
            getDerivedHandle<LLFloaterExperienceProfile>(), _1));
}

void LLFloaterExperienceProfile::onSaveComplete( const LLSD& content )
{
	LLUUID id = getExperienceId();

	if (content.has("removed"))
	{
		const LLSD& removed = content["removed"];
		for(const auto& it : removed.map())
		{
			const std::string& field = it.first;
			if (field == LLExperienceCache::EXPERIENCE_ID)
			{
				//this message should be removed by the experience api
				continue;
			}
			const LLSD& data = it.second;
			std::string error_tag = data["error_tag"].asString()+ "ExperienceProfileMessage";
			LLSD fields;
			if (LLNotificationTemplates::instance().templateExists(error_tag))
			{
				fields["field"] = field;
				fields["extra_info"] = data["extra_info"];
				LLNotificationsUtil::add(error_tag, fields);
			}
			else
			{
				fields["MESSAGE"] = data["en"];
				LLNotificationsUtil::add("GenericAlert", fields);
			}
		}		
	}

	if (!content.has("experience_keys"))
	{
        LL_WARNS() << "LLFloaterExperienceProfile::onSaveComplete called with bad content" << LL_ENDL;
		return;
	}

	const LLSD& experiences = content["experience_keys"];

    if (experiences.size() == 0)
    {
        LL_WARNS() << "LLFloaterExperienceProfile::onSaveComplete called with empty content" << LL_ENDL;
        return;
    }

    const auto& exp = experiences[0];
    if (!exp.has(LLExperienceCache::EXPERIENCE_ID) || (exp[LLExperienceCache::EXPERIENCE_ID].asUUID() != id))
    {
        LL_WARNS() << "LLFloaterExperienceProfile::onSaveComplete called with unexpected experience id" << LL_ENDL;
        return;
    }

    refreshExperience(exp);
    LLExperienceCache::instance().insert(exp);
    LLExperienceCache::instance().fetch(id, true);

	if (mSaveCompleteAction == VIEW)
	{
		LLTabContainer* tabs = getChild<LLTabContainer>("tab_container");
		tabs->selectTabByName("panel_experience_info");
	}
	else if (mSaveCompleteAction == CLOSE)
	{
		close();
	}   
}

void LLFloaterExperienceProfile::changeToView()
{
	if (mForceClose || !mDirty)
	{
		refreshExperience(mExperienceDetails);
		LLTabContainer* tabs = getChild<LLTabContainer>("tab_container");

		tabs->selectTabByName("panel_experience_info");
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotificationsUtil::add("SaveChanges", LLSD(), LLSD(), boost::bind(&LLFloaterExperienceProfile::handleSaveChangesDialog, this, _1, _2, VIEW));
	}
}

void LLFloaterExperienceProfile::changeToEdit()
{
    LLTabContainer* tabs = getChild<LLTabContainer>("tab_container");

    tabs->selectTabByName("edit_panel_experience_info");
}

void LLFloaterExperienceProfile::onClickLocation()
{
	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
		auto child = getChild<LLTextEditor>(EDIT TF_SLURL);
		mLocationSLURL = LLSLURL(region->getName(), gAgent.getPositionGlobal()).getSLURLString();
		child->setText(mLocationSLURL);
		onFieldChanged();
	}
}

void LLFloaterExperienceProfile::onClickClear()
{
	auto child = getChild<LLTextEditor>(EDIT TF_SLURL);
	mLocationSLURL.clear();
	child->setText(getString("empty_slurl"));
	onFieldChanged();
}

void LLFloaterExperienceProfile::updatePermission(const LLSD& permission)
{
	if (permission.has("experience"))
	{
		if (permission["experience"].asUUID() != mExperienceId)
		{
			return;
		}

		std::string str = permission[mExperienceId.asString()]["permission"].asString();
		if (str == "Allow")
		{
			experienceAllowed();
		}
		else if (str == "Block")
		{
			experienceBlocked();
		}
		else if (str == "Forget")
		{
			experienceForgotten();
		}
	}
	else
	{
		setPreferences(permission);
	}
}

void LLFloaterExperienceProfile::experienceAllowed()
{
	LLButton* button=getChild<LLButton>(BTN_ALLOW);
	button->setEnabled(FALSE);

	button=getChild<LLButton>(BTN_FORGET);
	button->setEnabled(TRUE);

	button=getChild<LLButton>(BTN_BLOCK);
	button->setEnabled(TRUE);
}

void LLFloaterExperienceProfile::experienceForgotten()
{
	LLButton* button=getChild<LLButton>(BTN_ALLOW);
	button->setEnabled(TRUE);

	button=getChild<LLButton>(BTN_FORGET);
	button->setEnabled(FALSE);

	button=getChild<LLButton>(BTN_BLOCK);
	button->setEnabled(TRUE);
}

void LLFloaterExperienceProfile::experienceBlocked()
{
	LLButton* button=getChild<LLButton>(BTN_ALLOW);
	button->setEnabled(TRUE);

	button=getChild<LLButton>(BTN_FORGET);
	button->setEnabled(TRUE);

	button=getChild<LLButton>(BTN_BLOCK);
	button->setEnabled(FALSE);
}

void LLFloaterExperienceProfile::onClose( bool app_quitting )
{
	LLEventPumps::instance().obtain("experience_permission").stopListening(mExperienceId.asString()+"-profile");
	LLFloater::onClose(app_quitting);
}

void LLFloaterExperienceProfile::updatePackage()
{
	mPackage[LLExperienceCache::NAME] = getChild<LLLineEditor>(EDIT TF_NAME)->getText();
	mPackage[LLExperienceCache::DESCRIPTION] = getChild<LLTextEditor>(EDIT TF_DESC)->getText();
	if (mLocationSLURL.empty())
	{
		mPackage[LLExperienceCache::SLURL] = LLStringUtil::null;
	}
	else
	{
		mPackage[LLExperienceCache::SLURL] = mLocationSLURL;
	}

	mPackage[LLExperienceCache::MATURITY] = getChild<LLComboBox>(EDIT TF_MATURITY)->getSelectedValue().asInteger();

	LLSD metadata;

	metadata[TF_MRKT] = getChild<LLLineEditor>(EDIT TF_MRKT)->getText();
	metadata[IMG_LOGO] = getChild<LLTextureCtrl>(EDIT IMG_LOGO)->getImageAssetID();

	LLPointer<LLSDXMLFormatter> formatter = new LLSDXMLFormatter();

	std::ostringstream os;
	if (formatter->format(metadata, os))
	{
		mPackage[LLExperienceCache::METADATA] = os.str();
	}

	int properties = mPackage[LLExperienceCache::PROPERTIES].asInteger();
	LLCheckBoxCtrl* enable = getChild<LLCheckBoxCtrl>(EDIT BTN_ENABLE);
	if (enable->get())
	{
		properties &= ~LLExperienceCache::PROPERTY_DISABLED;
	}
	else
	{
		properties |= LLExperienceCache::PROPERTY_DISABLED;
	}

	enable = getChild<LLCheckBoxCtrl>(EDIT BTN_PRIVATE);
	if (enable->get())
	{
		properties |= LLExperienceCache::PROPERTY_PRIVATE;
	}
	else
	{
		properties &= ~LLExperienceCache::PROPERTY_PRIVATE;
	}

	mPackage[LLExperienceCache::PROPERTIES] = properties;
}

void LLFloaterExperienceProfile::onPickGroup()
{
	LLFloater* parent_floater = gFloaterView->getParentFloater(this);

	LLFloaterGroupPicker* widget = LLFloaterGroupPicker::showInstance(gAgent.getID());
	if (widget)
	{
		widget->setSelectGroupCallback(boost::bind(&LLFloaterExperienceProfile::setEditGroup, this, _1));
		if (parent_floater)
		{
			LLRect new_rect = gFloaterView->findNeighboringPosition(parent_floater, widget);
			widget->setOrigin(new_rect.mLeft, new_rect.mBottom);
			parent_floater->addDependentFloater(widget);
		}
	}
}

void LLFloaterExperienceProfile::setEditGroup( LLUUID group_id )
{
	LLTextBox* child = getChild<LLTextBox>(EDIT TF_GROUP);
	child->setValue(group_id);
	mPackage[LLExperienceCache::GROUP_ID] = group_id;
	onFieldChanged();
}

void LLFloaterExperienceProfile::onReportExperience()
{
	LLFloaterReporter::showFromExperience(mExperienceId);
}

/*static*/
bool LLFloaterExperienceProfile::hasPermission(const LLSD& content, const std::string &name, const LLUUID &test)
{
    if (!content.has(name))
        return false;

    const LLSD& list = content[name];
    for (const auto& it : list.array())
    {
        if (it.asUUID() == test)
        {
            return true;
        }
    }
    return false;
}

/*static*/
void LLFloaterExperienceProfile::experiencePermissionResults(const LLUUID& exprienceId, const LLSD& result)
{
    std::string permission("Forget");
    if (hasPermission(result, "experiences", exprienceId))
        permission = "Allow";
    else if (hasPermission(result, "blocked", exprienceId))
        permission = "Block";

    LLSD experience;
    LLSD message;
    experience["permission"] = permission;
    message["experience"] = exprienceId;
    message[exprienceId.asString()] = experience;

    LLEventPumps::instance().obtain("experience_permission").post(message);
}

/*static*/
void LLFloaterExperienceProfile::experienceIsAdmin(LLHandle<LLFloaterExperienceProfile> handle, const LLSD &result)
{
    LLFloaterExperienceProfile* parent = handle.get();
    if (!parent)
        return;

    bool enabled = true;
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        enabled = false;
    }
    else
    {
        std::string url = region->getCapability("UpdateExperience");
        if (url.empty())
            enabled = false;
    }
    if (enabled && result["status"].asBoolean())
    {
        parent->getChild<LLLayoutPanel>(PNL_TOP)->setVisible(TRUE);
        parent->getChild<LLButton>(BTN_EDIT)->setVisible(TRUE);
    }
}

/*static*/
void LLFloaterExperienceProfile::experienceUpdateResult(LLHandle<LLFloaterExperienceProfile> handle, const LLSD &result)
{
    LLFloaterExperienceProfile* parent = handle.get();
    if (parent)
    {
        parent->onSaveComplete(result);
    }
}
