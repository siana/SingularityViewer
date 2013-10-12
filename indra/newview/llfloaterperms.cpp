/** 
 * @file llfloaterperms.cpp
 * @brief Asset creation permission preferences.
 * @author Coco
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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
#include "lfsimfeaturehandler.h"
#include "llcheckboxctrl.h"
#include "llfloaterperms.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "lluictrlfactory.h"
#include "llpermissions.h"
#include "hippogridmanager.h"

namespace
{
	bool everyone_export;
	void handle_checkboxes(LLUICtrl* ctrl, const LLSD& value)
	{
		LLPanel* view = static_cast<LLPanel*>(ctrl->getParent());
		if (ctrl->getName() == "everyone_export")
		{
			view->childSetEnabled("next_owner_copy", !value);
			view->childSetEnabled("next_owner_modify", !value);
			view->childSetEnabled("next_owner_transfer", !value);
		}
		else
		{
			if (ctrl->getName() == "next_owner_copy")
			{
				if (!value) // Implements fair use
					gSavedSettings.setBOOL("NextOwnerTransfer", true);
				view->childSetEnabled("next_owner_transfer", value);
			}
			if (!value) // If any of these are unchecked, export can no longer be checked.
				view->childSetEnabled("everyone_export", false);
			else
				view->childSetEnabled("everyone_export", LFSimFeatureHandler::instance().simSupportsExport() && (LLFloaterPerms::getNextOwnerPerms() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED);
		}
	}
}

LLFloaterPerms::LLFloaterPerms(const LLSD& seed)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_perm_prefs.xml");
}

BOOL LLFloaterPerms::postBuild()
{
	//handle_checkboxes
	{
		bool export_support = LFSimFeatureHandler::instance().simSupportsExport();
		const U32 next_owner_perms = getNextOwnerPerms();
		childSetEnabled("everyone_export", export_support && (next_owner_perms & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED);
		if (!gHippoGridManager->getCurrentGrid()->isSecondLife())
			childSetVisible("everyone_export", false);

		if (!(next_owner_perms & PERM_COPY))
		{
			childSetEnabled("next_owner_transfer", false);
		}
		else if (export_support)
		{
			bool export_off = !gSavedPerAccountSettings.getBOOL("EveryoneExport");
			childSetEnabled("next_owner_copy", export_off);
			childSetEnabled("next_owner_modify", export_off);
			childSetEnabled("next_owner_transfer", export_off);
		}
		else // Set EveryoneExport false, just in case.
			gSavedPerAccountSettings.setBOOL("EveryoneExport", false);
	}
	childSetAction("help",   onClickHelp,   this);
	childSetAction("ok",     onClickOK,     this);
	childSetAction("cancel", onClickCancel, this);
	getChild<LLUICtrl>("next_owner_copy")->setCommitCallback(handle_checkboxes);
	getChild<LLUICtrl>("next_owner_modify")->setCommitCallback(handle_checkboxes);
	getChild<LLUICtrl>("next_owner_transfer")->setCommitCallback(handle_checkboxes);
	getChild<LLUICtrl>("everyone_export")->setCommitCallback(handle_checkboxes);

	refresh();
	
	return TRUE;
}

//static 
void LLFloaterPerms::onClickOK(void* data)
{
	LLFloaterPerms* self = static_cast<LLFloaterPerms*>(data);
	self->ok();
	self->close();
}

//static 
void LLFloaterPerms::onClickCancel(void* data)
{
	LLFloaterPerms* self = static_cast<LLFloaterPerms*>(data);
	self->cancel();
	self->close();
}

void LLFloaterPerms::ok()
{
	refresh(); // Changes were already applied to saved settings. Refreshing internal values makes it official.
}

void LLFloaterPerms::cancel()
{
	gSavedSettings.setBOOL("ShareWithGroup",    mShareWithGroup);
	gSavedSettings.setBOOL("EveryoneCopy",      mEveryoneCopy);
	gSavedSettings.setBOOL("NextOwnerCopy",     mNextOwnerCopy);
	gSavedSettings.setBOOL("NextOwnerModify",   mNextOwnerModify);
	gSavedSettings.setBOOL("NextOwnerTransfer", mNextOwnerTransfer);
	gSavedPerAccountSettings.setBOOL("EveryoneExport", everyone_export);
}

void LLFloaterPerms::refresh()
{
	mShareWithGroup    = gSavedSettings.getBOOL("ShareWithGroup");
	mEveryoneCopy      = gSavedSettings.getBOOL("EveryoneCopy");
	mNextOwnerCopy     = gSavedSettings.getBOOL("NextOwnerCopy");
	mNextOwnerModify   = gSavedSettings.getBOOL("NextOwnerModify");
	mNextOwnerTransfer = gSavedSettings.getBOOL("NextOwnerTransfer");
	everyone_export    = gSavedPerAccountSettings.getBOOL("EveryoneExport");
}

void LLFloaterPerms::onClose(bool app_quitting)
{
	// Cancel any unsaved changes before closing. 
	// Note: when closed due to the OK button this amounts to a no-op.
	cancel();
	LLFloater::onClose(app_quitting);
}

//static 
U32 LLFloaterPerms::getGroupPerms(std::string prefix)
{	
	return gSavedSettings.getBOOL(prefix+"ShareWithGroup") ? PERM_COPY : PERM_NONE;
}

//static 
U32 LLFloaterPerms::getEveryonePerms(std::string prefix)
{
	U32 flags = PERM_NONE;
	if (LFSimFeatureHandler::instance().simSupportsExport() && prefix.empty() && gSavedPerAccountSettings.getBOOL("EveryoneExport")) // TODO: Bulk enable export?
		flags |= PERM_EXPORT;
	if (gSavedSettings.getBOOL(prefix+"EveryoneCopy"))
		flags |= PERM_COPY;
	return flags;
}

//static 
U32 LLFloaterPerms::getNextOwnerPerms(std::string prefix)
{
	U32 flags = PERM_MOVE;
	if ( gSavedSettings.getBOOL(prefix+"NextOwnerCopy") )
	{
		flags |= PERM_COPY;
	}
	if ( gSavedSettings.getBOOL(prefix+"NextOwnerModify") )
	{
		flags |= PERM_MODIFY;
	}
	if ( gSavedSettings.getBOOL(prefix+"NextOwnerTransfer") )
	{
		flags |= PERM_TRANSFER;
	}
	return flags;
}


//static
void LLFloaterPerms::onClickHelp(void* data)
{
	LLNotificationsUtil::add("ClickUploadHelpPermissions");
}
