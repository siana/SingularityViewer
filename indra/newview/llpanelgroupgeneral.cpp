/** 
 * @file llpanelgroupgeneral.cpp
 * @brief General information about a group.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llpanelgroupgeneral.h"

#include "llagent.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

// UI elements
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldbstrings.h"
#include "llavataractions.h"
#include "llgroupactions.h"
#include "llimview.h"
#include "lllineeditor.h"
#include "llnamebox.h"
#include "llnamelistctrl.h"
#include "llnameeditor.h"
#include "llnotificationsutil.h"
#include "llspinctrl.h"
#include "llstatusbar.h"	// can_afford_transaction()
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltrans.h"
#include "llviewerwindow.h"

#include "hippogridmanager.h"

// consts
const S32 MATURE_CONTENT = 1;
const S32 NON_MATURE_CONTENT = 2;
const S32 DECLINE_TO_STATE = 0;

// static
void* LLPanelGroupGeneral::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupGeneral("panel group general", *group_id);
}


LLPanelGroupGeneral::LLPanelGroupGeneral(const std::string& name, 
										 const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mPendingMemberUpdate(FALSE),
	mChanged(FALSE),
	mFirstUse(TRUE),
	mGroupNameEditor(NULL),
	mFounderName(NULL),
	mInsignia(NULL),
	mGroupName(NULL),
	mEditCharter(NULL),
	mBtnJoinGroup(NULL),
	mListVisibleMembers(NULL),
	mCtrlShowInGroupList(NULL),
	mComboMature(NULL),
	mCtrlOpenEnrollment(NULL),
	mCtrlEnrollmentFee(NULL),
	mSpinEnrollmentFee(NULL),
	mCtrlReceiveNotices(NULL),
	mCtrlReceiveChat(NULL),
	mCtrlListGroup(NULL),
	mActiveTitleLabel(NULL),
	mComboActiveTitle(NULL)
{

}

LLPanelGroupGeneral::~LLPanelGroupGeneral()
{
}

BOOL LLPanelGroupGeneral::postBuild()
{
	llinfos << "LLPanelGroupGeneral::postBuild()" << llendl;

	bool recurse = true;

	// General info
	mGroupNameEditor = getChild<LLLineEditor>("group_name_editor", recurse);
	mGroupName = getChild<LLTextBox>("group_name", recurse);
	
	mInsignia = getChild<LLTextureCtrl>("insignia", recurse);
	if (mInsignia)
	{
		mInsignia->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny,this));
		mDefaultIconID = mInsignia->getImageAssetID();
	}
	
	mEditCharter = getChild<LLTextEditor>("charter", recurse);
	if(mEditCharter)
	{
		mEditCharter->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny,this));
		mEditCharter->setFocusReceivedCallback(boost::bind(&LLPanelGroupGeneral::onFocusEdit, this));
		mEditCharter->setFocusChangedCallback(boost::bind(&LLPanelGroupGeneral::onFocusEdit, this));
	}

	mBtnJoinGroup = getChild<LLButton>("join_button", recurse);
	if ( mBtnJoinGroup )
	{
		mBtnJoinGroup->setClickedCallback(boost::bind(LLGroupActions::join, mGroupID));
	}

	mBtnInfo = getChild<LLButton>("info_button", recurse);
	if ( mBtnInfo )
	{
		mBtnInfo->setClickedCallback(boost::bind(LLGroupActions::show, mGroupID));
	}

	mFounderName = getChild<LLNameBox>("founder_name");

	mListVisibleMembers = getChild<LLNameListCtrl>("visible_members", recurse);
	if (mListVisibleMembers)
	{
		mListVisibleMembers->setDoubleClickCallback(boost::bind(LLAvatarActions::showProfile, boost::bind(&LLScrollListCtrl::getCurrentID, mListVisibleMembers), false));
	}

	// Options
	mCtrlShowInGroupList = getChild<LLCheckBoxCtrl>("show_in_group_list", recurse);
	if (mCtrlShowInGroupList)
	{
		mCtrlShowInGroupList->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny,this));
	}

	mComboMature = getChild<LLComboBox>("group_mature_check", recurse);	
	if(mComboMature)
	{
		mComboMature->setCurrentByIndex(0);
		mComboMature->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny,this));
		if (gAgent.isTeen())
		{
			// Teens don't get to set mature flag. JC
			mComboMature->setVisible(FALSE);
			mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
		}
	}
	mCtrlOpenEnrollment = getChild<LLCheckBoxCtrl>("open_enrollement", recurse);
	if (mCtrlOpenEnrollment)
	{
		mCtrlOpenEnrollment->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny,this));
	}

	mCtrlEnrollmentFee = getChild<LLCheckBoxCtrl>("check_enrollment_fee", recurse);
	if (mCtrlEnrollmentFee)
	{
		mCtrlEnrollmentFee->setLabelArg("[CURRENCY]", gHippoGridManager->getConnectedGrid()->getCurrencySymbol());
		mCtrlEnrollmentFee->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitEnrollment,this));
	}

	mSpinEnrollmentFee = getChild<LLSpinCtrl>("spin_enrollment_fee", recurse);
	if (mSpinEnrollmentFee)
	{
		mSpinEnrollmentFee->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny,this));
		mSpinEnrollmentFee->setPrecision(0);
		mSpinEnrollmentFee->resetDirty();
	}

	BOOL accept_notices = FALSE;
	BOOL list_in_profile = FALSE;
	LLGroupData data;
	if(gAgent.getGroupData(mGroupID,data))
	{
		accept_notices = data.mAcceptNotices;
		list_in_profile = data.mListInProfile;
	}
	mCtrlReceiveNotices = getChild<LLCheckBoxCtrl>("receive_notices", recurse);
	if (mCtrlReceiveNotices)
	{
		mCtrlReceiveNotices->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitUserOnly,this));
		mCtrlReceiveNotices->set(accept_notices);
		mCtrlReceiveNotices->setEnabled(data.mID.notNull());
		mCtrlReceiveNotices->resetDirty();
	}

	mCtrlReceiveChat = getChild<LLCheckBoxCtrl>("receive_chat", recurse);
	if (mCtrlReceiveChat)
	{
		mCtrlReceiveChat->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitUserOnly,this));
		mCtrlReceiveChat->set(!gIMMgr->getIgnoreGroup(mGroupID));
		mCtrlReceiveChat->setEnabled(mGroupID.notNull());
		mCtrlReceiveChat->resetDirty();
	}
	
	mCtrlListGroup = getChild<LLCheckBoxCtrl>("list_groups_in_profile", recurse);
	if (mCtrlListGroup)
	{
		mCtrlListGroup->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitUserOnly,this));
		mCtrlListGroup->set(list_in_profile);
		mCtrlListGroup->setEnabled(data.mID.notNull());
		mCtrlListGroup->resetDirty();
	}

	mActiveTitleLabel = getChild<LLTextBox>("active_title_label", recurse);
	
	mComboActiveTitle = getChild<LLComboBox>("active_title", recurse);
	if (mComboActiveTitle)
	{
		mComboActiveTitle->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitTitle,this));
		mComboActiveTitle->resetDirty();
	}

	LLStringUtil::format_map_t args;
	args["[GROUPCREATEFEE]"] = gHippoGridManager->getConnectedGrid()->getGroupCreationFee();
	mIncompleteMemberDataStr = getString("incomplete_member_data_str");
	mConfirmGroupCreateStr = getString("confirm_group_create_str", args);

	// If the group_id is null, then we are creating a new group
	if (mGroupID.isNull())
	{
		mGroupNameEditor->setEnabled(TRUE);
		getChildView("copy_uri")->setVisible(false); // New group has no uri
		mEditCharter->setEnabled(TRUE);

		mCtrlShowInGroupList->setEnabled(TRUE);
		mComboMature->setEnabled(TRUE);
		mCtrlOpenEnrollment->setEnabled(TRUE);
		mCtrlEnrollmentFee->setEnabled(TRUE);
		mSpinEnrollmentFee->setEnabled(TRUE);

		mBtnJoinGroup->setVisible(FALSE);
		mBtnInfo->setVisible(FALSE);
		mGroupName->setVisible(FALSE);
	}

	return LLPanelGroupTab::postBuild();
}

void LLPanelGroupGeneral::onFocusEdit()
{
	updateChanged();
	notifyObservers();
}

void LLPanelGroupGeneral::onCommitAny()
{
	updateChanged();
	notifyObservers();
}

// static
void LLPanelGroupGeneral::onCommitUserOnly()
{
	mChanged = TRUE;
	notifyObservers();
}


void LLPanelGroupGeneral::onCommitEnrollment()
{
	onCommitAny();

	// Make sure both enrollment related widgets are there.
	if (!mCtrlEnrollmentFee || !mSpinEnrollmentFee)
	{
		return;
	}

	// Make sure the agent can change enrollment info.
	if (!gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS))
	{
		return;
	}

	if (mCtrlEnrollmentFee->get())
	{
		mSpinEnrollmentFee->setEnabled(TRUE);
	}
	else
	{
		mSpinEnrollmentFee->setEnabled(FALSE);
		mSpinEnrollmentFee->set(0);
	}
}

void LLPanelGroupGeneral::onCommitTitle()
{
	if (mGroupID.isNull()) return;
	LLGroupMgr::getInstance()->sendGroupTitleUpdate(mGroupID,mComboActiveTitle->getCurrentID());
	update(GC_TITLES);
	mComboActiveTitle->resetDirty();
}

bool LLPanelGroupGeneral::needsApply(std::string& mesg)
{ 
	updateChanged();
	mesg = getString("group_info_unchanged");
	return mChanged || mGroupID.isNull();
}

void LLPanelGroupGeneral::activate()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (mGroupID.notNull()
		&& (!gdatap || mFirstUse))
	{
		LLGroupMgr::getInstance()->sendGroupTitlesRequest(mGroupID);
		LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mGroupID);

		if (!gdatap || !gdatap->isMemberDataComplete() )
		{
			LLGroupMgr::getInstance()->sendCapGroupMembersRequest(mGroupID);
		}

		mFirstUse = FALSE;
	}
	mChanged = FALSE;
	
	update(GC_ALL);
}

void LLPanelGroupGeneral::draw()
{
	LLPanelGroupTab::draw();

	if (mPendingMemberUpdate)
	{
		updateMembers();
	}
}

bool LLPanelGroupGeneral::apply(std::string& mesg)
{
	BOOL has_power_in_group = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);

	if (has_power_in_group || mGroupID.isNull())
	{
		llinfos << "LLPanelGroupGeneral::apply" << llendl;

		// Check to make sure mature has been set
		if(mComboMature &&
		   mComboMature->getCurrentIndex() == DECLINE_TO_STATE)
		{
			LLNotificationsUtil::add("SetGroupMature", LLSD(), LLSD(), 
											boost::bind(&LLPanelGroupGeneral::confirmMatureApply, this, _1, _2));
			return false;
		}

		if (mGroupID.isNull())
		{
			// Validate the group name length.
			S32 group_name_len = mGroupNameEditor->getText().size();
			if ( group_name_len < DB_GROUP_NAME_MIN_LEN 
				|| group_name_len > DB_GROUP_NAME_STR_LEN)
			{
				std::ostringstream temp_error;
				temp_error << "A group name must be between " << DB_GROUP_NAME_MIN_LEN
					<< " and " << DB_GROUP_NAME_STR_LEN << " characters.";
				mesg = temp_error.str();
				return false;
			}

			LLSD args;
			args["MESSAGE"] = mConfirmGroupCreateStr;
			LLNotificationsUtil::add("GenericAlertYesCancel", args, LLSD(), boost::bind(&LLPanelGroupGeneral::createGroupCallback, this, _1, _2));

			return false;
		}

		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
		if (!gdatap)
		{
			mesg = LLTrans::getString("NoGroupDataFound");
			mesg.append(mGroupID.asString());
			return false;
		}
		bool can_change_ident = false;
		bool can_change_member_opts = false;
		can_change_ident = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);
		can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS);

		if (can_change_ident)
		{
			if (mEditCharter) gdatap->mCharter = mEditCharter->getText();
			if (mInsignia) gdatap->mInsigniaID = mInsignia->getImageAssetID();
			if (mComboMature)
			{
				if (!gAgent.isTeen())
				{
					gdatap->mMaturePublish = 
						mComboMature->getCurrentIndex() == MATURE_CONTENT;
				}
				else
				{
					gdatap->mMaturePublish = FALSE;
				}
			}
			if (mCtrlShowInGroupList) gdatap->mShowInList = mCtrlShowInGroupList->get();
		}

		if (can_change_member_opts)
		{
			if (mCtrlOpenEnrollment) gdatap->mOpenEnrollment = mCtrlOpenEnrollment->get();
			if (mCtrlEnrollmentFee && mSpinEnrollmentFee)
			{
				gdatap->mMembershipFee = (mCtrlEnrollmentFee->get()) ? 
					(S32) mSpinEnrollmentFee->get() : 0;
				// Set to the used value, and reset initial value used for isdirty check
				mSpinEnrollmentFee->set( (F32)gdatap->mMembershipFee );
			}
		}

		if (can_change_ident || can_change_member_opts)
		{
			LLGroupMgr::getInstance()->sendUpdateGroupInfo(mGroupID);
		}
	}

	BOOL receive_notices = false;
	BOOL list_in_profile = false;
	if (mCtrlReceiveNotices)
	{
		receive_notices = mCtrlReceiveNotices->get();
		mCtrlReceiveNotices->resetDirty();	//resetDirty() here instead of in update because this is where the settings
											//are actually being applied. onCommitUserOnly doesn't call updateChanged directly.
	}
	if (mCtrlListGroup) 
	{
		list_in_profile = mCtrlListGroup->get();
		mCtrlListGroup->resetDirty();		//resetDirty() here instead of in update because this is where the settings
											//are actually being applied. onCommitUserOnly doesn't call updateChanged directly.
	}

	gAgent.setUserGroupFlags(mGroupID, receive_notices, list_in_profile);

	if (mCtrlReceiveChat)
	{
		bool receive_chat = mCtrlReceiveChat->get();
		gIMMgr->updateIgnoreGroup(mGroupID, !receive_chat);
		// Save here too in case we crash somewhere down the road -- MC
		gIMMgr->saveIgnoreGroup();
		mCtrlReceiveChat->resetDirty();
	}

	mChanged = FALSE;

	return true;
}

void LLPanelGroupGeneral::cancel()
{
	mChanged = FALSE;

	//cancel out all of the click changes to, although since we are
	//shifting tabs or closing the floater, this need not be done...yet
	notifyObservers();
}

// invoked from callbackConfirmMature
bool LLPanelGroupGeneral::confirmMatureApply(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	// 0 == Yes
	// 1 == No
	// 2 == Cancel
	switch(option)
	{
	case 0:
		mComboMature->setCurrentByIndex(MATURE_CONTENT);
		break;
	case 1:
		mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
		break;
	default:
		return false;
	}

	// If we got here it means they set a valid value
	std::string mesg = "";
	apply(mesg);
	return false;
}

// static
bool LLPanelGroupGeneral::createGroupCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch(option)
	{
	case 0:
		{
			// Yay!  We are making a new group!
			U32 enrollment_fee = (mCtrlEnrollmentFee->get() ? 
									(U32) mSpinEnrollmentFee->get() : 0);
		
			LLGroupMgr::getInstance()->sendCreateGroupRequest(mGroupNameEditor->getText(),
												mEditCharter->getText(),
												mCtrlShowInGroupList->get(),
												mInsignia->getImageAssetID(),
												enrollment_fee,
												mCtrlOpenEnrollment->get(),
												false,
												mComboMature->getCurrentIndex() == MATURE_CONTENT);

		}
		break;
	case 1:
	default:
		break;
	}
	return false;
}

static F32 sSDTime = 0.0f;
static F32 sElementTime = 0.0f;
static F32 sAllTime = 0.0f;

// virtual
void LLPanelGroupGeneral::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	LLGroupData agent_gdatap;
	bool is_member = false;
	if (gAgent.getGroupData(mGroupID,agent_gdatap)) is_member = true;

	if (mComboActiveTitle)
	{
		mComboActiveTitle->setVisible(is_member);
		
		if ( mActiveTitleLabel) mActiveTitleLabel->setVisible(is_member);

		if (is_member)
		{
			LLUUID current_title_role;

			mComboActiveTitle->clear();
			mComboActiveTitle->removeall();
			bool has_selected_title = false;

			if (1 == gdatap->mTitles.size())
			{
				// Only the everyone title.  Don't bother letting them try changing this.
				mComboActiveTitle->setEnabled(FALSE);
			}
			else
			{
				mComboActiveTitle->setEnabled(TRUE);
			}

			std::vector<LLGroupTitle>::const_iterator citer = gdatap->mTitles.begin();
			std::vector<LLGroupTitle>::const_iterator end = gdatap->mTitles.end();
			
			for ( ; citer != end; ++citer)
			{
				mComboActiveTitle->add(citer->mTitle,citer->mRoleID, (citer->mSelected ? ADD_TOP : ADD_BOTTOM));
				if (citer->mSelected)
				{
					mComboActiveTitle->setCurrentByID(citer->mRoleID);
					has_selected_title = true;
				}
			}
			
			if (!has_selected_title)
			{
				mComboActiveTitle->setCurrentByID(LLUUID::null);
			}
		}

		mComboActiveTitle->resetDirty();
	}

	// After role member data was changed in Roles->Members
	// need to update role titles. See STORM-918.
	if (gc == GC_ROLE_MEMBER_DATA)
		LLGroupMgr::getInstance()->sendGroupTitlesRequest(mGroupID);

	// If this was just a titles update, we are done.
	if (gc == GC_TITLES) return;

	bool can_change_ident = false;
	bool can_change_member_opts = false;
	can_change_ident = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);
	can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS);

	if (mCtrlShowInGroupList) 
	{
		mCtrlShowInGroupList->set(gdatap->mShowInList);
		mCtrlShowInGroupList->setEnabled(can_change_ident);
		mCtrlShowInGroupList->resetDirty();

	}
	if (mComboMature)
	{
		if(gdatap->mMaturePublish)
		{
			mComboMature->setCurrentByIndex(MATURE_CONTENT);
		}
		else
		{
			mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
		}
		mComboMature->setEnabled(can_change_ident);
		mComboMature->setVisible( !gAgent.isTeen() );
		mComboMature->resetDirty();
	}
	if (mCtrlOpenEnrollment) 
	{
		mCtrlOpenEnrollment->set(gdatap->mOpenEnrollment);
		mCtrlOpenEnrollment->setEnabled(can_change_member_opts);
		mCtrlOpenEnrollment->resetDirty();
	}
	if (mCtrlEnrollmentFee) 
	{	
		mCtrlEnrollmentFee->set(gdatap->mMembershipFee > 0);
		mCtrlEnrollmentFee->setEnabled(can_change_member_opts);
		mCtrlEnrollmentFee->resetDirty();
	}
	
	if (mSpinEnrollmentFee)
	{
		S32 fee = gdatap->mMembershipFee;
		mSpinEnrollmentFee->set((F32)fee);
		mSpinEnrollmentFee->setEnabled(fee && can_change_member_opts);
		mSpinEnrollmentFee->resetDirty();
	}
	if ( mBtnJoinGroup )
	{
		std::string fee_buff;
		bool visible;

		visible = !is_member && gdatap->mOpenEnrollment;
		mBtnJoinGroup->setVisible(visible);

		if ( visible )
		{
			fee_buff = llformat( "Join (%s%d)",
				gHippoGridManager->getConnectedGrid()->getCurrencySymbol().c_str(),
				gdatap->mMembershipFee);
			mBtnJoinGroup->setLabelSelected(fee_buff);
			mBtnJoinGroup->setLabelUnselected(fee_buff);
		}
	}
	if ( mBtnInfo )
	{
		mBtnInfo->setVisible(is_member && !mAllowEdit);
	}

	if(gc == GC_ALL || gc == GC_PROPERTIES)
	{
		if (mCtrlReceiveNotices)
		{
			mCtrlReceiveNotices->setVisible(is_member);
			if (is_member)
			{
				if(!mCtrlReceiveNotices->isDirty())	//If the user hasn't edited this then refresh it. Value may have changed in groups panel, etc.
				{
					mCtrlReceiveNotices->set(agent_gdatap.mAcceptNotices);
					mCtrlReceiveNotices->resetDirty();
				}
			}
		}

		if (mCtrlListGroup)
		{
			mCtrlListGroup->setVisible(is_member);
			if (is_member)
			{
				if(!mCtrlListGroup->isDirty())	//If the user hasn't edited this then refresh it. Value may have changed in groups panel, etc.
				{
					mCtrlListGroup->set(agent_gdatap.mListInProfile);
					mCtrlListGroup->resetDirty();
				}
			}
		}

		if (mCtrlReceiveChat)
		{
			mCtrlReceiveChat->setVisible(is_member);
			if (is_member)
			{
				if(!mCtrlReceiveChat->isDirty())	//If the user hasn't edited this then refresh it. Value may have changed in groups panel, etc.
				{
					mCtrlReceiveChat->set(!gIMMgr->getIgnoreGroup(mGroupID));
					mCtrlReceiveChat->resetDirty();
				}
			}
		}

		if (mInsignia) mInsignia->setEnabled(can_change_ident);
		if (mEditCharter) mEditCharter->setEnabled(can_change_ident);
	
		if (mGroupName) mGroupName->setText(gdatap->mName);
		if (mGroupNameEditor) mGroupNameEditor->setVisible(FALSE);
		if (mFounderName) mFounderName->setNameID(gdatap->mFounderID,FALSE);

		LLNameEditor* key_edit = getChild<LLNameEditor>("group_key");
		if(key_edit)
		{
			key_edit->setText(gdatap->getID().asString());
		}

		if (mInsignia)
		{
			if (gdatap->mInsigniaID.notNull())
			{
				mInsignia->setImageAssetID(gdatap->mInsigniaID);
			}
			else
			{
				mInsignia->setImageAssetID(mDefaultIconID);
			}
		}

		if (mEditCharter)
		{
			mEditCharter->setText(gdatap->mCharter);
			mEditCharter->resetDirty();
		}
	}
	
	if (mListVisibleMembers)
	{
		mListVisibleMembers->deleteAllItems();

		if (gdatap->isMemberDataComplete())
		{
			mMemberProgress = gdatap->mMembers.begin();
			mPendingMemberUpdate = TRUE;

			sSDTime = 0.0f;
			sElementTime = 0.0f;
			sAllTime = 0.0f;
		}
		else
		{
			std::stringstream pending;
			pending << "Retrieving member list (" << gdatap->mMembers.size() << "\\" << gdatap->mMemberCount  << ")";

			LLSD row;
			row["columns"][0]["value"] = pending.str();
			row["columns"][0]["column"] = "name";

			mListVisibleMembers->setEnabled(FALSE);
			mListVisibleMembers->addElement(row);
		}
	}
}

void LLPanelGroupGeneral::updateMembers()
{
	mPendingMemberUpdate = FALSE;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!mListVisibleMembers 
		|| !gdatap 
		|| !gdatap->isMemberDataComplete()
		|| gdatap->mMembers.empty())
	{
		return;
	}

	LLTimer update_time;
	update_time.setTimerExpirySec(UPDATE_MEMBERS_SECONDS_PER_FRAME);

	LLAvatarName av_name;

	for( ; mMemberProgress != gdatap->mMembers.end() && !update_time.hasExpired(); 
			++mMemberProgress)
	{
		LLGroupMemberData* member = mMemberProgress->second;
		if (!member)
		{
			continue;
		}

		if (LLAvatarNameCache::get(mMemberProgress->first, &av_name))
		{
			addMember(mMemberProgress->second);
		}
		else
		{
			// If name is not cached, onNameCache() should be called when it is cached and add this member to list.
			LLAvatarNameCache::get(mMemberProgress->first, 
									boost::bind(&LLPanelGroupGeneral::onNameCache,
												this, gdatap->getMemberVersion(), member, _2));
		}
	}

	if (mMemberProgress == gdatap->mMembers.end())
	{
		lldebugs << "   member list completed." << llendl;
		mListVisibleMembers->setEnabled(TRUE);
	}
	else
	{
		mPendingMemberUpdate = TRUE;
		mListVisibleMembers->setEnabled(FALSE);
	}
}

void LLPanelGroupGeneral::addMember(LLGroupMemberData* member)
{
	// Owners show up in bold.
	std::string style = "NORMAL";
	if ( member->isOwner() )
	{
		style = "BOLD";
	}
	LLNameListCtrl::NameItem item_params;
	item_params.value = member->getID();

	LLScrollListCell::Params column;
	item_params.columns.add().column("name").font/*.name*/("SANSSERIF_SMALL").font_style(style);

	item_params.columns.add().column("title").value(member->getTitle()).font/*.name*/("SANSSERIF_SMALL").font_style(style);

	static const LLCachedControl<std::string> format(gSavedSettings, "ShortDateFormat");
	static const std::string online(LLTrans::getString("group_member_status_online"));
	item_params.columns.add().column("online").value(member->getOnlineStatus())
		.format(format).type(member->getOnlineStatus() == online ? "text" : "date")
		.font/*.name*/("SANSSERIF_SMALL").font_style(style);

	/*LLScrollListItem* member_row =*/ mListVisibleMembers->addNameItemRow(item_params);
}

void LLPanelGroupGeneral::onNameCache(const LLUUID& update_id, LLGroupMemberData* member, const LLAvatarName& av_name)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap
		|| !gdatap->isMemberDataComplete()
		|| gdatap->getMemberVersion() != update_id)
	{
		// Stale data
		return;
	}

	addMember(member);
}

void LLPanelGroupGeneral::updateChanged()
{
	// List all the controls we want to check for changes...
	LLUICtrl *check_list[] =
	{
		mGroupNameEditor,
		mGroupName,
		mFounderName,
		mInsignia,
		mEditCharter,
		mCtrlShowInGroupList,
		mComboMature,
		mCtrlOpenEnrollment,
		mCtrlEnrollmentFee,
		mSpinEnrollmentFee,
		mCtrlReceiveNotices,
		mCtrlReceiveChat,
		mCtrlListGroup,
		mActiveTitleLabel,
		mComboActiveTitle
	};

	mChanged = FALSE;

	for( int i= 0; i< LL_ARRAY_SIZE(check_list); i++ )
	{
		if( check_list[i] && check_list[i]->isDirty() )
		{
			mChanged = TRUE;
			break;
		}
	}
}
