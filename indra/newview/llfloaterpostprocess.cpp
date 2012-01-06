/** 
 * @file llfloaterpostprocess.cpp
 * @brief LLFloaterPostProcess class definition
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

#include "llfloaterpostprocess.h"

#include "llsliderctrl.h"
#include "llcheckboxctrl.h"
#include "llnotificationsutil.h"
#include "lluictrlfactory.h"
#include "llviewerdisplay.h"
#include "llpostprocess.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llviewerwindow.h"
#if LL_MSVC
// disable boost::lexical_cast warning
#pragma warning (disable:4702)
#endif
#include <boost/lexical_cast.hpp>

LLFloaterPostProcess* LLFloaterPostProcess::sPostProcess = NULL;


LLFloaterPostProcess::LLFloaterPostProcess() : LLFloater(std::string("Post-Process Floater"))
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_post_process.xml");

	LLTabContainer *panel_list = getChild<LLTabContainer>("Post-Process Tabs",false,false);	//Contains a tab for each shader
	if(panel_list)
	{
		//Iterate down tabs to access each panel
		for(S32 i = 0; i<panel_list->getTabCount(); ++i)	
		{
			//Get the panel via index
			LLPanel *shader_panel = panel_list->getPanelByIndex(i);		

			//'Extra' panel controls loading/saving. No uniforms should be altered in this panel.
			if(shader_panel->getName() == "Extras")	
				continue;

			//Iterate down children elements of each panel.
			for ( child_list_const_iter_t child_it = shader_panel->getChildList()->begin(); child_it != shader_panel->getChildList()->end(); ++child_it)
			{
				//Hacky, but for now checkboxes and sliders are assumed to link to shader uniforms.
				if(dynamic_cast<LLSliderCtrl*>(*child_it) || dynamic_cast<LLCheckBoxCtrl*>(*child_it))
				{
					LLUICtrl *ctrl = dynamic_cast<LLUICtrl*>(*child_it);
					ctrl->setCommitCallback(boost::bind(&LLFloaterPostProcess::onControlChanged, _1, (void*)ctrl->getName().c_str()));
				}
			}
		}
	}

	// Effect loading and saving.
	LLComboBox* comboBox = getChild<LLComboBox>("PPEffectsCombo");
	childSetAction("PPLoadEffect", &LLFloaterPostProcess::onLoadEffect, comboBox);
	comboBox->setCommitCallback(onChangeEffectName);

	LLLineEditor* editBox = getChild<LLLineEditor>("PPEffectNameEditor");
	childSetAction("PPSaveEffect", &LLFloaterPostProcess::onSaveEffect, editBox);

	syncMenu();
	
}

LLFloaterPostProcess::~LLFloaterPostProcess()
{


}

LLFloaterPostProcess* LLFloaterPostProcess::instance()
{
	// if we don't have our singleton instance, create it
	if (!sPostProcess)
	{
		sPostProcess = new LLFloaterPostProcess();
		sPostProcess->open();
		sPostProcess->setFocus(TRUE);
	}
	return sPostProcess;
}


void LLFloaterPostProcess::onControlChanged(LLUICtrl* ctrl, void* userData)
{
	char const *VariableName = (char const *)userData;
	char buf[256];
	S32 elem=0;
	if(sscanf(VariableName,"%255[^[][%d]", buf, &elem) == 2)
	{
		LLPostProcess::getInstance()->tweaks[(const char*)buf][elem] = ctrl->getValue();
	}
	else
	{
		LLPostProcess::getInstance()->tweaks[VariableName] = ctrl->getValue();
	}
}

void LLFloaterPostProcess::onLoadEffect(void* userData)
{
	LLComboBox* comboBox = static_cast<LLComboBox*>(userData);

	LLSD::String effectName(comboBox->getSelectedValue().asString());

	LLPostProcess::getInstance()->setSelectedEffect(effectName);

	sPostProcess->syncMenu();
}

void LLFloaterPostProcess::onSaveEffect(void* userData)
{
	LLLineEditor* editBox = static_cast<LLLineEditor*>(userData);

	std::string effectName(editBox->getValue().asString());

	if (LLPostProcess::getInstance()->mAllEffects.has(effectName))
	{
		LLSD payload;
		payload["effect_name"] = effectName;
		LLNotificationsUtil::add("PPSaveEffectAlert", LLSD(), payload, &LLFloaterPostProcess::saveAlertCallback);
	}
	else
	{
		LLPostProcess::getInstance()->saveEffect(effectName);
		sPostProcess->syncMenu();
	}
}

void LLFloaterPostProcess::onChangeEffectName(LLUICtrl* ctrl, void * userData)
{
	// get the combo box and name
	LLComboBox * comboBox = static_cast<LLComboBox*>(ctrl);
	LLLineEditor* editBox = sPostProcess->getChild<LLLineEditor>("PPEffectNameEditor");

	// set the parameter's new name
	editBox->setValue(comboBox->getSelectedValue());
}

bool LLFloaterPostProcess::saveAlertCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	// if they choose save, do it.  Otherwise, don't do anything
	if (option == 0)
	{
		LLPostProcess::getInstance()->saveEffect(notification["payload"]["effect_name"].asString());

		sPostProcess->syncMenu();
	}
	return false;
}

void LLFloaterPostProcess::show()
{
	// get the instance, make sure the values are synced
	// and open the menu
	LLFloaterPostProcess* postProcess = instance();
	postProcess->syncMenu();
	postProcess->open();
}

// virtual
void LLFloaterPostProcess::onClose(bool app_quitting)
{
	// just set visibility to false, don't get fancy yet
	if (sPostProcess)
	{
		sPostProcess->setVisible(FALSE);
	}
}

void LLFloaterPostProcess::syncMenu()
{
	// add the combo boxe contents
	LLComboBox* comboBox = getChild<LLComboBox>("PPEffectsCombo");

	comboBox->removeall();

	LLSD::map_const_iterator currEffect;
	for(currEffect = LLPostProcess::getInstance()->mAllEffects.beginMap();
		currEffect != LLPostProcess::getInstance()->mAllEffects.endMap();
		++currEffect) 
	{
		comboBox->add(currEffect->first);
	}

	// set the current effect as selected.
	comboBox->selectByValue(LLPostProcess::getInstance()->getSelectedEffect());

	LLSD &tweaks = LLPostProcess::getInstance()->tweaks;
	//Iterate down all uniforms handled by post-process shaders. Update any linked ui elements.
	for (LLSD::map_const_iterator it = tweaks.beginMap(); it != tweaks.endMap(); ++it)
	{
		if(it->second.isArray())
		{
			//llsd["uniform"][0]=>"uniform[0]"
			//llsd["uniform"][1]=>"uniform[1]"
			for(S32 i=0;i<it->second.size();++i)
			{
				childSetValue(it->first+"["+boost::lexical_cast<std::string>(i)+"]",it->second[i]);
			}
		}
		else
		{
			//llsd["uniform"]=>"uniform"
			childSetValue(it->first,it->second);
		}
	}
}
