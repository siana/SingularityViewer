/** 
 * @file llscrollingpanelparam.cpp
 * @brief UI panel for a list of visual param panels
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llscrollingpanelparam.h"
#include "llviewerjointmesh.h"
#include "llviewervisualparam.h"
#include "llwearable.h"
#include "llviewervisualparam.h"
#include "lltoolmorph.h"
#include "lltrans.h"
#include "llbutton.h"
#include "llsliderctrl.h"
#include "llagent.h"
#include "llviewborder.h"
#include "llvoavatarself.h"
#include "llagentwearables.h"
#include "llfloatercustomize.h"

// Constants for LLPanelVisualParam
const F32 LLScrollingPanelParam::PARAM_STEP_TIME_THRESHOLD = 0.25f;

const S32 LLScrollingPanelParam::PARAM_HINT_WIDTH = 128;
const S32 LLScrollingPanelParam::PARAM_HINT_HEIGHT = 128;

const S32 BTN_BORDER = 2;
const S32 PARAM_HINT_LABEL_HEIGHT = 16;
const S32 PARAM_PANEL_WIDTH = 2 * (3* BTN_BORDER + LLScrollingPanelParam::PARAM_HINT_WIDTH +  LLPANEL_BORDER_WIDTH);
const S32 PARAM_PANEL_HEIGHT = 2 * BTN_BORDER + LLScrollingPanelParam::PARAM_HINT_HEIGHT + PARAM_HINT_LABEL_HEIGHT + 4 * LLPANEL_BORDER_WIDTH; 

// LLScrollingPanelParam
//static
S32 LLScrollingPanelParam::sUpdateDelayFrames = 0;

LLScrollingPanelParam::LLScrollingPanelParam( const std::string& name,
											  LLViewerJointMesh* mesh, LLViewerVisualParam* param, BOOL allow_modify, LLWearable* wearable, bool bVisualHint )
	: LLScrollingPanelParamBase( name, mesh, param, allow_modify, wearable, bVisualHint, LLRect( 0, PARAM_PANEL_HEIGHT, PARAM_PANEL_WIDTH, 0 )),
	  mLess(NULL),
	  mMore(NULL)
{

	if(bVisualHint)
	{
		S32 pos_x = 2 * LLPANEL_BORDER_WIDTH;
		S32 pos_y = 3 * LLPANEL_BORDER_WIDTH + SLIDERCTRL_HEIGHT;
		F32 min_weight = param->getMinWeight();
		F32 max_weight = param->getMaxWeight();

		mHintMin = new LLVisualParamHint( pos_x, pos_y, PARAM_HINT_WIDTH, PARAM_HINT_HEIGHT, mesh, param, mWearable, min_weight);
		pos_x += PARAM_HINT_WIDTH + 3 * BTN_BORDER;
		mHintMax = new LLVisualParamHint( pos_x, pos_y, PARAM_HINT_WIDTH, PARAM_HINT_HEIGHT, mesh, param, mWearable, max_weight );

		mHintMin->setAllowsUpdates( FALSE );
		mHintMax->setAllowsUpdates( FALSE );

		std::string min_name = LLTrans::getString(param->getMinDisplayName());
		std::string max_name = LLTrans::getString(param->getMaxDisplayName());
		childSetValue("min param text", min_name);
		childSetValue("max param text", max_name);
		mLess = getChild<LLButton>("less");
		mLess->setMouseDownCallback( boost::bind(&LLScrollingPanelParam::onHintMouseDown, this, false) );
		mLess->setMouseUpCallback( boost::bind(&LLScrollingPanelParam::onHintMouseUp, this, false) );
		mLess->setHeldDownCallback( boost::bind(&LLScrollingPanelParam::onHintHeldDown, this, false) );
		mLess->setHeldDownDelay( PARAM_STEP_TIME_THRESHOLD );

		mMore = getChild<LLButton>("more");
		mMore->setMouseDownCallback( boost::bind(&LLScrollingPanelParam::onHintMouseDown, this, true) );
		mMore->setMouseUpCallback( boost::bind(&LLScrollingPanelParam::onHintMouseUp, this, true) );
		mMore->setHeldDownCallback( boost::bind(&LLScrollingPanelParam::onHintHeldDown, this, true) );
		mMore->setHeldDownDelay( PARAM_STEP_TIME_THRESHOLD );
	}

	setVisible(FALSE);
	setBorderVisible( FALSE );
}

LLScrollingPanelParam::~LLScrollingPanelParam()
{
	mHintMin = NULL;
	mHintMax = NULL;
}

void LLScrollingPanelParam::updatePanel(BOOL allow_modify)
{
	if(!mWearable)
	{
		// not editing a wearable just now, no update necessary
		return;
	}

	LLScrollingPanelParamBase::updatePanel(allow_modify);

	if(mHintMin)
		mHintMin->requestUpdate( sUpdateDelayFrames++ );
	if(mHintMax)
		mHintMax->requestUpdate( sUpdateDelayFrames++ );

	if(mLess)
		mLess->setEnabled(mAllowModify);
	if(mMore)
		mMore->setEnabled(mAllowModify);
}

void LLScrollingPanelParam::setVisible( BOOL visible )
{
	if( getVisible() != visible )
	{
		LLPanel::setVisible( visible );
		if(mHintMin)
			mHintMin->setAllowsUpdates( visible );
		if(mHintMax)
			mHintMax->setAllowsUpdates( visible );

		if( visible )
		{
			if(mHintMin)
				mHintMin->setUpdateDelayFrames( sUpdateDelayFrames++ );
			if(mHintMax)
				mHintMax->setUpdateDelayFrames( sUpdateDelayFrames++ );
		}
	}
}

void LLScrollingPanelParam::draw()
{
	if( !mWearable || !LLFloaterCustomize::instanceExists() || LLFloaterCustomize::getInstance()->isMinimized() )
	{
		return;
	}
	
	if(mLess)
		mLess->setVisible(mHintMin && mHintMin->getVisible());
	if(mMore)
		mMore->setVisible(mHintMax && mHintMax->getVisible());

	// Draw all the children except for the labels
	childSetVisible( "min param text", FALSE );
	childSetVisible( "max param text", FALSE );
	LLPanel::draw();

	// Draw the hints over the "less" and "more" buttons.
	if(mHintMin)
	{
		gGL.pushUIMatrix();
		{
			const LLRect& r = mHintMin->getRect();
			F32 left = (F32)(r.mLeft + BTN_BORDER);
			F32 bot  = (F32)(r.mBottom + BTN_BORDER);
			gGL.translateUI(left, bot, 0.f);
			mHintMin->draw();
		}
		gGL.popUIMatrix();
	}

	if(mHintMax)
	{
		gGL.pushUIMatrix();
		{
			const LLRect& r = mHintMax->getRect();
			F32 left = (F32)(r.mLeft + BTN_BORDER);
			F32 bot  = (F32)(r.mBottom + BTN_BORDER);
			gGL.translateUI(left, bot, 0.f);
			mHintMax->draw();
		}
		gGL.popUIMatrix();
	}


	// Draw labels on top of the buttons
	childSetVisible( "min param text", TRUE );
	drawChild(getChild<LLView>("min param text"), BTN_BORDER, BTN_BORDER);

	childSetVisible( "max param text", TRUE );
	drawChild(getChild<LLView>("max param text"), BTN_BORDER, BTN_BORDER);
}

// static
void LLScrollingPanelParam::onSliderMouseDown(LLUICtrl* ctrl, void* userdata)
{
}

// static
void LLScrollingPanelParam::onSliderMouseUp(LLUICtrl* ctrl, void* userdata)
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	LLVisualParamHint::requestHintUpdates( self->mHintMin, self->mHintMax );
}

void LLScrollingPanelParam::onHintMouseDown( bool max )
{
	LLVisualParamHint* hint = max ? mHintMax : mHintMin;
	LLViewerVisualParam* param = hint->getVisualParam();

	if(!mWearable || !param)
	{
		return;
	}

	// morph towards this result
	F32 current_weight = mWearable->getVisualParamWeight( hint->getVisualParam()->getID() );

	// if we have maxed out on this morph, we shouldn't be able to click it
	if( hint->getVisualParamWeight() != current_weight )
	{
		mMouseDownTimer.reset();
		mLastHeldTime = 0.f;
	}
}
	
void LLScrollingPanelParam::onHintHeldDown( bool max )
{
	LLVisualParamHint* hint = max ? mHintMax : mHintMin;
	LLViewerVisualParam* param = hint->getVisualParam();

	if(!mWearable || !param)
	{
		return;
	}

	F32 current_weight = mWearable->getVisualParamWeight( param->getID() );

	if (current_weight != hint->getVisualParamWeight() )
	{
		const F32 FULL_BLEND_TIME = 2.f;
		F32 elapsed_time = mMouseDownTimer.getElapsedTimeF32() - mLastHeldTime;
		mLastHeldTime += elapsed_time;

		F32 new_weight;
		if (current_weight > hint->getVisualParamWeight() )
		{
			new_weight = current_weight - (elapsed_time / FULL_BLEND_TIME);
		}
		else
		{
			new_weight = current_weight + (elapsed_time / FULL_BLEND_TIME);
		}

		// Make sure we're not taking the slider out of bounds
		// (this is where some simple UI limits are stored)
		F32 new_percent = weightToPercent(new_weight);
		LLSliderCtrl* slider = getChild<LLSliderCtrl>("param slider");
		if (slider)
		{
			if (slider->getMinValue() < new_percent
				&& new_percent < slider->getMaxValue())
			{
				mWearable->setVisualParamWeight(param->getID(), new_weight, FALSE);
				mWearable->writeToAvatar(gAgentAvatarp);
				gAgentAvatarp->updateVisualParams();

				slider->setValue( weightToPercent( new_weight ) );
			}
		}
	}
}

void LLScrollingPanelParam::onHintMouseUp( bool max )
{
	F32 elapsed_time = mMouseDownTimer.getElapsedTimeF32();

	if (isAgentAvatarValid())
	{
		LLVisualParamHint* hint			= max ? mHintMax : mHintMin;

		if (elapsed_time < PARAM_STEP_TIME_THRESHOLD)
		{
			LLViewerVisualParam* param = hint->getVisualParam();

			if(mWearable)
			{
				// step in direction
				F32 current_weight = mWearable->getVisualParamWeight( param->getID() );
				F32 range = mHintMax->getVisualParamWeight() - mHintMin->getVisualParamWeight();
				//if min, range should be negative.
				if(!max)
					range *= -1.f;
				// step a fraction in the negative direction
				F32 new_weight = current_weight + (range / 10.f);
				F32 new_percent = weightToPercent(new_weight);
				LLSliderCtrl* slider = getChild<LLSliderCtrl>("param slider");
				if (slider)
				{
					if (slider->getMinValue() < new_percent
						&& new_percent < slider->getMaxValue())
					{
						mWearable->setVisualParamWeight(param->getID(), new_weight, FALSE);
						mWearable->writeToAvatar(gAgentAvatarp);
						slider->setValue( weightToPercent( new_weight ) );
					}
				}
			}
		}
	}

	LLVisualParamHint::requestHintUpdates( mHintMin, mHintMax );
}

