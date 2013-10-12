/** 
 * @file llpanelmsgs.cpp
 * @brief Message popup preferences panel
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llpanelmsgs.h"

#include "llnotificationtemplate.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lluictrlfactory.h"

#include "llfirstuse.h"

LLPanelMsgs::LLPanelMsgs()
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_popups.xml");
}


LLPanelMsgs::~LLPanelMsgs()
{
}

BOOL LLPanelMsgs::postBuild()
{
	getChild<LLUICtrl>("enable_popup")->setCommitCallback(boost::bind(&LLPanelMsgs::onClickEnablePopup, this));
	getChild<LLUICtrl>("disable_popup")->setCommitCallback(boost::bind(&LLPanelMsgs::onClickDisablePopup, this));
	getChild<LLUICtrl>("reset_dialogs_btn")->setCommitCallback(boost::bind(&LLPanelMsgs::onClickResetDialogs, this));
	getChild<LLUICtrl>("skip_dialogs_btn")->setCommitCallback(boost::bind(&LLPanelMsgs::onClickSkipDialogs, this));
	getChild<LLUICtrl>("skip_frst_btn")->setCommitCallback(boost::bind(&LLPanelMsgs::onClickSkipFirstTime, this));

	buildPopupLists();

	childSetValue("accept_new_inventory", gSavedSettings.getBOOL("AutoAcceptAllNewInventory"));
	childSetValue("show_new_inventory", gSavedSettings.getBOOL("ShowNewInventory"));
	childSetValue("show_in_inventory", gSavedSettings.getBOOL("ShowInInventory"));

	return TRUE;
}

void LLPanelMsgs::draw()
{
	BOOL has_first_selected = (getChildRef<LLScrollListCtrl>("disabled_popups").getFirstSelected()!=NULL);
	childSetEnabled("enable_popup", has_first_selected);

	has_first_selected = (getChildRef<LLScrollListCtrl>("enabled_popups").getFirstSelected()!=NULL);
	childSetEnabled("disable_popup", has_first_selected);

	LLPanel::draw();
}

void LLPanelMsgs::buildPopupLists() //void LLFloaterPreference::buildPopupLists() in v3
{
	LLScrollListCtrl& disabled_popups =
		getChildRef<LLScrollListCtrl>("disabled_popups");
	LLScrollListCtrl& enabled_popups =
		getChildRef<LLScrollListCtrl>("enabled_popups");
	
	disabled_popups.deleteAllItems();
	enabled_popups.deleteAllItems();

	for (LLNotificationTemplates::TemplateMap::const_iterator iter = LLNotificationTemplates::instance().templatesBegin();
		iter != LLNotificationTemplates::instance().templatesEnd();
		++iter)
	{
		LLNotificationTemplatePtr templatep = iter->second;
		LLNotificationFormPtr formp = templatep->mForm;

		LLNotificationForm::EIgnoreType ignore = formp->getIgnoreType();
		if (ignore == LLNotificationForm::IGNORE_NO)
				continue;

		LLSD params;
		params["name"] = (*iter).first;
		LLNotificationPtr notification = LLNotificationPtr(new LLNotification(params));

		LLSD row;
		std::string ignore_msg = formp->getIgnoreMessage();
		LLStringUtil::format(ignore_msg,notification->getSubstitutions());
		row["columns"][0]["value"] = ignore_msg;
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		row["columns"][0]["width"] = 500;

		LLScrollListItem* item = NULL;

		bool show_popup = !formp->getIgnored();
		if (!show_popup)
		{
			if (ignore == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
			{
				LLSD last_response = LLUI::sConfigGroup->getLLSD("Default" + templatep->mName);
				if (!last_response.isUndefined())
				{
					for (LLSD::map_const_iterator it = last_response.beginMap();
						it != last_response.endMap();
						++it)
					{
						if (it->second.asBoolean())
						{
							row["columns"][1]["value"] = formp->getElement(it->first)["ignore"].asString();
							break;
						}
					}
				}
				row["columns"][1]["font"] = "SANSSERIF_SMALL";
				row["columns"][1]["width"] = 160;
			}
			item = disabled_popups.addElement(row, ADD_SORTED);
		}
		else
		{
			item = enabled_popups.addElement(row, ADD_SORTED);
		}

		if (item)
		{
			item->setUserdata((void*)&iter->first);
		}
	}
}


void LLPanelMsgs::apply()
{
	gSavedSettings.setBOOL("AutoAcceptAllNewInventory", childGetValue("accept_new_inventory"));
	gSavedSettings.setBOOL("ShowNewInventory", childGetValue("show_new_inventory"));
	gSavedSettings.setBOOL("ShowInInventory", childGetValue("show_in_inventory"));
}

void LLPanelMsgs::cancel()
{
}

void LLPanelMsgs::onClickEnablePopup()
{
	LLScrollListCtrl& disabled_popups = getChildRef<LLScrollListCtrl>("disabled_popups");

	std::vector<LLScrollListItem*> items = disabled_popups.getAllSelected();
	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		LLNotificationTemplatePtr templatep = LLNotificationTemplates::instance().getTemplate(*(std::string*)((*itor)->getUserdata()));
		gSavedSettings.setWarning(templatep->mName, TRUE);
	}

	buildPopupLists();
}

void LLPanelMsgs::onClickDisablePopup()
{
	LLScrollListCtrl& enabled_popups = getChildRef<LLScrollListCtrl>("enabled_popups");

	std::vector<LLScrollListItem*> items = enabled_popups.getAllSelected();
	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		LLNotificationTemplatePtr templatep = LLNotificationTemplates::instance().getTemplate(*(std::string*)((*itor)->getUserdata()));
		gSavedSettings.setWarning(templatep->mName, false);
	}

	buildPopupLists();
}

void LLPanelMsgs::resetAllIgnored()
{
	for (LLNotificationTemplates::TemplateMap::const_iterator iter = LLNotificationTemplates::instance().templatesBegin();
		iter != LLNotificationTemplates::instance().templatesEnd();
		++iter)
	{
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(iter->first, TRUE);
		}
	}
}

void LLPanelMsgs::setAllIgnored()
{
	for (LLNotificationTemplates::TemplateMap::const_iterator iter = LLNotificationTemplates::instance().templatesBegin();
		iter != LLNotificationTemplates::instance().templatesEnd();
		++iter)
	{
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(iter->first, FALSE);
		}
	}
}

void LLPanelMsgs::onClickResetDialogs()
{
	resetAllIgnored();
	LLFirstUse::resetFirstUse();
	buildPopupLists();
}

void LLPanelMsgs::onClickSkipDialogs()
{
	setAllIgnored();
	LLFirstUse::disableFirstUse();
	buildPopupLists();
}

void LLPanelMsgs::onClickSkipFirstTime()
{
	LLFirstUse::disableFirstUse();
	buildPopupLists();
}
