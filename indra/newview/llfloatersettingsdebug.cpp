/** 
 * @file llfloatersettingsdebug.cpp
 * @brief floater for debugging internal viewer settings
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

#include "llfloatersettingsdebug.h"

#include "llcolorswatch.h"
//#include "llfirstuse.h"
#include "llfloater.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llspinctrl.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llwindow.h"

// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a)
#include "rlvhandler.h"
#include "rlvextensions.h"
// [/RLVa:KB]

LLFloaterSettingsDebug::LLFloaterSettingsDebug()
:	LLFloater(std::string("Configuration Editor"))
,	mCurrentControlVariable(NULL)
,	mOldControlVariable(NULL)
,	mOldSearchTerm(std::string("---"))
{
	mCommitCallbackRegistrar.add("SettingSelect",	boost::bind(&LLFloaterSettingsDebug::onSettingSelect, this));
	mCommitCallbackRegistrar.add("CommitSettings",	boost::bind(&LLFloaterSettingsDebug::onCommitSettings, this));
	mCommitCallbackRegistrar.add("ClickDefault",	boost::bind(&LLFloaterSettingsDebug::onClickDefault, this));
	mCommitCallbackRegistrar.add("UpdateFilter",	boost::bind(&LLFloaterSettingsDebug::onUpdateFilter, this, _2));
	mCommitCallbackRegistrar.add("ClickCopy",		boost::bind(&LLFloaterSettingsDebug::onCopyToClipboard, this));

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_settings_debug.xml");
}

LLFloaterSettingsDebug::~LLFloaterSettingsDebug()
{
	if (mOldControlVariable)
		mOldControlVariable->getCommitSignal()->disconnect(boost::bind(&LLFloaterSettingsDebug::onSettingSelect, this));
}

BOOL LLFloaterSettingsDebug::postBuild()
{
	mSettingsScrollList = getChild<LLScrollListCtrl>("settings_scroll_list");

	struct f : public LLControlGroup::ApplyFunctor
	{
		settings_map_t* map;
		f(settings_map_t* m) : map(m) {}
		virtual void apply(const std::string& name, LLControlVariable* control)
		{
			if (!control->isHiddenFromSettingsEditor())
			{
				(*map)[name]=control;
			}
		}
	} func(&mSettingsMap);

	gSavedSettings.applyToAll(&func);
	gSavedPerAccountSettings.applyToAll(&func);
	gColors.applyToAll(&func);

	// Populate the list
	{
		for(settings_map_t::iterator it = mSettingsMap.begin(); it != mSettingsMap.end(); it++)
		{
			LLSD item;
			item["columns"][0]["value"] = it->second->getName();
			mSettingsScrollList->addElement(item, ADD_BOTTOM, it->second);
		}
	}
	mSettingsScrollList->sortByColumnIndex(0, true);

	llinfos << mSettingsScrollList->getItemCount() << " total debug settings displayed." << llendl;

	mComment = getChild<LLTextEditor>("comment_text");
	return TRUE;
}

void LLFloaterSettingsDebug::draw()
{
	// check for changes in control visibility, like RLVa does
	if(mCurrentControlVariable && mCurrentControlVariable->isHiddenFromSettingsEditor() != mOldVisibility)
		updateControl();

	LLFloater::draw();
}

LLControlVariable* LLFloaterSettingsDebug::getControlVariable()
{
	LLScrollListItem* item = mSettingsScrollList->getFirstSelected();
	if (!item) return NULL;

	LLControlVariable* controlp = static_cast<LLControlVariable*>(item->getUserdata());

	return controlp ? controlp->getCOAActive() : NULL;
}

void LLFloaterSettingsDebug::onSettingSelect()
{
	mCurrentControlVariable = getControlVariable();

	if (mOldControlVariable == mCurrentControlVariable) return;

	// unbind change control signal from previously selected control
	if(mOldControlVariable)
		mOldControlVariable->getCommitSignal()->disconnect(boost::bind(&LLFloaterSettingsDebug::onSettingSelect, this));

	// bind change control signal, so we can see updates to the current control in realtime
	if(mCurrentControlVariable)
		mCurrentControlVariable->getCommitSignal()->connect(boost::bind(&LLFloaterSettingsDebug::onSettingSelect, this));

	mOldControlVariable = mCurrentControlVariable;

	updateControl();
}

void LLFloaterSettingsDebug::onCommitSettings()
{
	if (!mCurrentControlVariable)
		return;

	LLVector3 vector;
	LLVector3d vectord;
	LLRect rect;
	LLColor4 col4;
	LLColor3 col3;
	LLColor4U col4U;
	LLColor4 color_with_alpha;

	switch(mCurrentControlVariable->type())
	{		
	  case TYPE_U32:
		mCurrentControlVariable->set(getChild<LLUICtrl>("val_spinner_1")->getValue());
		break;
	  case TYPE_S32:
		mCurrentControlVariable->set(getChild<LLUICtrl>("val_spinner_1")->getValue());
		break;
	  case TYPE_F32:
		mCurrentControlVariable->set(LLSD(getChild<LLUICtrl>("val_spinner_1")->getValue().asReal()));
		break;
	  case TYPE_BOOLEAN:
		mCurrentControlVariable->set(getChild<LLUICtrl>("boolean_combo")->getValue());
		break;
	  case TYPE_STRING:
		mCurrentControlVariable->set(LLSD(getChild<LLUICtrl>("val_text")->getValue().asString()));
		break;
	  case TYPE_VEC3:
		vector.mV[VX] = (F32)getChild<LLUICtrl>("val_spinner_1")->getValue().asReal();
		vector.mV[VY] = (F32)getChild<LLUICtrl>("val_spinner_2")->getValue().asReal();
		vector.mV[VZ] = (F32)getChild<LLUICtrl>("val_spinner_3")->getValue().asReal();
		mCurrentControlVariable->set(vector.getValue());
		break;
	  case TYPE_VEC3D:
		vectord.mdV[VX] = getChild<LLUICtrl>("val_spinner_1")->getValue().asReal();
		vectord.mdV[VY] = getChild<LLUICtrl>("val_spinner_2")->getValue().asReal();
		vectord.mdV[VZ] = getChild<LLUICtrl>("val_spinner_3")->getValue().asReal();
		mCurrentControlVariable->set(vectord.getValue());
		break;
	  case TYPE_RECT:
		rect.mLeft = getChild<LLUICtrl>("val_spinner_1")->getValue().asInteger();
		rect.mRight = getChild<LLUICtrl>("val_spinner_2")->getValue().asInteger();
		rect.mBottom = getChild<LLUICtrl>("val_spinner_3")->getValue().asInteger();
		rect.mTop = getChild<LLUICtrl>("val_spinner_4")->getValue().asInteger();
		mCurrentControlVariable->set(rect.getValue());
		break;
	  case TYPE_COL4:
		col3.setValue(getChild<LLUICtrl>("val_color_swatch")->getValue());
		col4 = LLColor4(col3, (F32)getChild<LLUICtrl>("val_spinner_4")->getValue().asReal());
		mCurrentControlVariable->set(col4.getValue());
		break;
	  case TYPE_COL3:
		mCurrentControlVariable->set(getChild<LLUICtrl>("val_color_swatch")->getValue());
		//col3.mV[VRED] = (F32)getChild<LLUICtrl>("val_spinner_1")->getValue().asC();
		//col3.mV[VGREEN] = (F32)getChild<LLUICtrl>("val_spinner_2")->getValue().asReal();
		//col3.mV[VBLUE] = (F32)getChild<LLUICtrl>("val_spinner_3")->getValue().asReal();
		//mCurrentControlVariable->set(col3.getValue());
		break;
	  case TYPE_COL4U:
		col3.setValue(getChild<LLUICtrl>("val_color_swatch")->getValue());
		col4U.setVecScaleClamp(col3);
		col4U.mV[VALPHA] = getChild<LLUICtrl>("val_spinner_4")->getValue().asInteger();
		mCurrentControlVariable->set(col4U.getValue());
		break;
	  default:
		break;
	}
}

void LLFloaterSettingsDebug::onClickDefault()
{
	if (mCurrentControlVariable)
	{
		mCurrentControlVariable->resetToDefault(true);
		updateControl();
	}
}

void LLFloaterSettingsDebug::onCopyToClipboard()
{
	if (mCurrentControlVariable)
		getWindow()->copyTextToClipboard(utf8str_to_wstring(mCurrentControlVariable->getName()));
}

// we've switched controls, so update spinners, etc.
void LLFloaterSettingsDebug::updateControl()
{
	LLSpinCtrl* spinner1 = getChild<LLSpinCtrl>("val_spinner_1");
	LLSpinCtrl* spinner2 = getChild<LLSpinCtrl>("val_spinner_2");
	LLSpinCtrl* spinner3 = getChild<LLSpinCtrl>("val_spinner_3");
	LLSpinCtrl* spinner4 = getChild<LLSpinCtrl>("val_spinner_4");
	LLColorSwatchCtrl* color_swatch = getChild<LLColorSwatchCtrl>("val_color_swatch");
	LLUICtrl* bool_ctrl = getChild<LLUICtrl>("boolean_combo");

	if (!spinner1 || !spinner2 || !spinner3 || !spinner4 || !color_swatch)
	{
		llwarns << "Could not find all desired controls by name"
			<< llendl;
		return;
	}

	spinner1->setVisible(FALSE);
	spinner2->setVisible(FALSE);
	spinner3->setVisible(FALSE);
	spinner4->setVisible(FALSE);
	color_swatch->setVisible(FALSE);
	getChildView("val_text")->setVisible( FALSE);
	mComment->setText(LLStringUtil::null);
	childSetEnabled("copy_btn", false);
	childSetEnabled("default_btn", false);
	bool_ctrl->setVisible(false);

	if (mCurrentControlVariable)
	{
// [RLVa:KB] - Checked: 2011-05-28 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
		// If "HideFromEditor" was toggled while the floater is open then we need to manually disable access to the control
		mOldVisibility = mCurrentControlVariable->isHiddenFromSettingsEditor();
		spinner1->setEnabled(!mOldVisibility);
		spinner2->setEnabled(!mOldVisibility);
		spinner3->setEnabled(!mOldVisibility);
		spinner4->setEnabled(!mOldVisibility);
		color_swatch->setEnabled(!mOldVisibility);
		childSetEnabled("val_text", !mOldVisibility);
		bool_ctrl->setEnabled(!mOldVisibility);
		childSetEnabled("default_btn", !mOldVisibility);
// [/RLVa:KB]

		childSetEnabled("copy_btn", true);

		eControlType type = mCurrentControlVariable->type();

		mComment->setText(mCurrentControlVariable->getName() + std::string(": ") + mCurrentControlVariable->getComment());

		spinner1->setMaxValue(F32_MAX);
		spinner2->setMaxValue(F32_MAX);
		spinner3->setMaxValue(F32_MAX);
		spinner4->setMaxValue(F32_MAX);
		spinner1->setMinValue(-F32_MAX);
		spinner2->setMinValue(-F32_MAX);
		spinner3->setMinValue(-F32_MAX);
		spinner4->setMinValue(-F32_MAX);
		if (!spinner1->hasFocus())
		{
			spinner1->setIncrement(0.1f);
		}
		if (!spinner2->hasFocus())
		{
			spinner2->setIncrement(0.1f);
		}
		if (!spinner3->hasFocus())
		{
			spinner3->setIncrement(0.1f);
		}
		if (!spinner4->hasFocus())
		{
			spinner4->setIncrement(0.1f);
		}

		LLSD sd = mCurrentControlVariable->get();
		switch(type)
		{
		  case TYPE_U32:
			spinner1->setVisible(TRUE);
			spinner1->setLabel(std::string("value")); // Debug, don't translate
			if (!spinner1->hasFocus())
			{
				spinner1->setValue(sd);
				spinner1->setMinValue((F32)U32_MIN);
				spinner1->setMaxValue((F32)U32_MAX);
				spinner1->setIncrement(1.f);
				spinner1->setPrecision(0);
			}
			break;
		  case TYPE_S32:
			spinner1->setVisible(TRUE);
			spinner1->setLabel(std::string("value")); // Debug, don't translate
			if (!spinner1->hasFocus())
			{
				spinner1->setValue(sd);
				spinner1->setMinValue((F32)S32_MIN);
				spinner1->setMaxValue((F32)S32_MAX);
				spinner1->setIncrement(1.f);
				spinner1->setPrecision(0);
			}
			break;
		  case TYPE_F32:
			spinner1->setVisible(TRUE);
			spinner1->setLabel(std::string("value")); // Debug, don't translate
			if (!spinner1->hasFocus())
			{
				spinner1->setPrecision(3);
				spinner1->setValue(sd);
			}
			break;
		  case TYPE_BOOLEAN:
			bool_ctrl->setVisible(true);
			if (!bool_ctrl->hasFocus())
			{
				if (sd.asBoolean())
				{
					bool_ctrl->setValue(LLSD("TRUE"));
				}
				else
				{
					bool_ctrl->setValue(LLSD("FALSE"));
				}
			}
			break;
		  case TYPE_STRING:
			getChildView("val_text")->setVisible( TRUE);
			if (!getChild<LLUICtrl>("val_text")->hasFocus())
			{
				getChild<LLUICtrl>("val_text")->setValue(sd);
			}
			break;
		  case TYPE_VEC3:
		  {
			LLVector3 v;
			v.setValue(sd);
			spinner1->setVisible(TRUE);
			spinner1->setLabel(std::string("X"));
			spinner2->setVisible(TRUE);
			spinner2->setLabel(std::string("Y"));
			spinner3->setVisible(TRUE);
			spinner3->setLabel(std::string("Z"));
			if (!spinner1->hasFocus())
			{
				spinner1->setPrecision(3);
				spinner1->setValue(v[VX]);
			}
			if (!spinner2->hasFocus())
			{
				spinner2->setPrecision(3);
				spinner2->setValue(v[VY]);
			}
			if (!spinner3->hasFocus())
			{
				spinner3->setPrecision(3);
				spinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_VEC3D:
		  {
			LLVector3d v;
			v.setValue(sd);
			spinner1->setVisible(TRUE);
			spinner1->setLabel(std::string("X"));
			spinner2->setVisible(TRUE);
			spinner2->setLabel(std::string("Y"));
			spinner3->setVisible(TRUE);
			spinner3->setLabel(std::string("Z"));
			if (!spinner1->hasFocus())
			{
				spinner1->setPrecision(3);
				spinner1->setValue(v[VX]);
			}
			if (!spinner2->hasFocus())
			{
				spinner2->setPrecision(3);
				spinner2->setValue(v[VY]);
			}
			if (!spinner3->hasFocus())
			{
				spinner3->setPrecision(3);
				spinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_RECT:
		  {
			LLRect r;
			r.setValue(sd);
			spinner1->setVisible(TRUE);
			spinner1->setLabel(std::string("Left"));
			spinner2->setVisible(TRUE);
			spinner2->setLabel(std::string("Right"));
			spinner3->setVisible(TRUE);
			spinner3->setLabel(std::string("Bottom"));
			spinner4->setVisible(TRUE);
			spinner4->setLabel(std::string("Top"));
			if (!spinner1->hasFocus())
			{
				spinner1->setPrecision(0);
				spinner1->setValue(r.mLeft);
			}
			if (!spinner2->hasFocus())
			{
				spinner2->setPrecision(0);
				spinner2->setValue(r.mRight);
			}
			if (!spinner3->hasFocus())
			{
				spinner3->setPrecision(0);
				spinner3->setValue(r.mBottom);
			}
			if (!spinner4->hasFocus())
			{
				spinner4->setPrecision(0);
				spinner4->setValue(r.mTop);
			}

			spinner1->setMinValue((F32)S32_MIN);
			spinner1->setMaxValue((F32)S32_MAX);
			spinner1->setIncrement(1.f);

			spinner2->setMinValue((F32)S32_MIN);
			spinner2->setMaxValue((F32)S32_MAX);
			spinner2->setIncrement(1.f);

			spinner3->setMinValue((F32)S32_MIN);
			spinner3->setMaxValue((F32)S32_MAX);
			spinner3->setIncrement(1.f);

			spinner4->setMinValue((F32)S32_MIN);
			spinner4->setMaxValue((F32)S32_MAX);
			spinner4->setIncrement(1.f);
			break;
		  }
		  case TYPE_COL4:
		  {
			LLColor4 clr;
			clr.setValue(sd);
			color_swatch->setVisible(TRUE);
			// only set if changed so color picker doesn't update
			if(clr != LLColor4(color_swatch->getValue()))
			{
				color_swatch->set(LLColor4(sd), TRUE, FALSE);
			}
			spinner4->setVisible(TRUE);
			spinner4->setLabel(std::string("Alpha"));
			if (!spinner4->hasFocus())
			{
				spinner4->setPrecision(3);
				spinner4->setMinValue(0.0);
				spinner4->setMaxValue(1.f);
				spinner4->setValue(clr.mV[VALPHA]);
			}
			break;
		  }
		  case TYPE_COL3:
		  {
			LLColor3 clr;
			clr.setValue(sd);
			color_swatch->setVisible(TRUE);
			color_swatch->setValue(sd);
			break;
		  }
		  case TYPE_COL4U:
		  {
			LLColor4U clr;
			clr.setValue(sd);
			color_swatch->setVisible(TRUE);
			if(LLColor4(clr) != LLColor4(color_swatch->getValue()))
			{
				color_swatch->set(LLColor4(clr), TRUE, FALSE);
			}
			spinner4->setVisible(TRUE);
			spinner4->setLabel(std::string("Alpha"));
			if(!spinner4->hasFocus())
			{
				spinner4->setPrecision(0);
				spinner4->setValue(clr.mV[VALPHA]);
			}

			spinner4->setMinValue(0);
			spinner4->setMaxValue(255);
			spinner4->setIncrement(1.f);

			break;
		  }
		  default:
			mComment->setText(std::string("unknown"));
			break;
		}
	}

}

void LLFloaterSettingsDebug::onUpdateFilter(const LLSD& value)
{
	updateFilter(value.asString());
}

void LLFloaterSettingsDebug::updateFilter(std::string searchTerm)
{
	// make sure not to reselect the first item in the list on focus restore
	if (searchTerm == mOldSearchTerm) return;

	mOldSearchTerm = searchTerm;

	LLStringUtil::toLower(searchTerm);

	mSettingsScrollList->deleteAllItems();

	for(settings_map_t::iterator it = mSettingsMap.begin(); it != mSettingsMap.end(); it++)
	{
		bool addItem = searchTerm.empty();
		if (!addItem)
		{
			std::string itemValue = it->second->getName();

			LLStringUtil::toLower(itemValue);

			if (itemValue.find(searchTerm, 0) != std::string::npos)
			{
				addItem = true;
			}
			else // performance: broken out to save toLower calls on comments
			{
				std::string itemComment = it->second->getComment();
				LLStringUtil::toLower(itemComment);
				if (itemComment.find(searchTerm, 0) != std::string::npos)
					addItem = true;
			}
		}

		if (addItem)
		{
			LLSD item;
			item["columns"][0]["value"] = it->second->getName();
			mSettingsScrollList->addElement(item, ADD_BOTTOM, it->second);
		}
	}
	mSettingsScrollList->sortByColumnIndex(0, true);

	// if at least one match was found, highlight and select the topmost entry in the list
	// but only if actually a search term was given
	if (mSettingsScrollList->getItemCount() && !searchTerm.empty())
		mSettingsScrollList->selectFirstItem();

	onSettingSelect();
}
