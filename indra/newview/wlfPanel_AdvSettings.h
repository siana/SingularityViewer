/* Copyright (c) 2010
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS AS IS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LL_wlfPanel_AdvSettings_H
#define LL_wlfPanel_AdvSettings_H

#include "llpanel.h"
#include <boost/ptr_container/ptr_list.hpp>

// [RLVa:KB]
#include "rlvdefines.h"
// [/RLVa:KB]

class LLComboBox;
class LLSliderCtrl;

class wlfPanel_AdvSettings : public LLPanel, public LLSingleton<wlfPanel_AdvSettings>
{
public:
	wlfPanel_AdvSettings();
	
	~wlfPanel_AdvSettings();
	BOOL postBuild();
	void draw();

	void updateRlvVisibility();
	void onRlvBehaviorChange(ERlvBehaviour eBhvr, ERlvParamType eType);

	static void updateClass();

	void onClickExpandBtn();
	void onChangeCameraPreset(LLUICtrl* ctrl, const LLSD& param);
	void onChangeWWPresetName(const LLSD& value);
	void onChangeWLPresetName(const LLSD& value);
	const bool& isExpanded() const { return mExpanded; }

protected:
	void build();

	void onUseRegionSettings(const LLSD& value);
	static void onClickArrow(LLComboBox* combo, bool next);
	void onChangeDayTime(const LLSD& value);

	void refreshLists(); /// update controls with user prefs

	void populateWaterPresetsList();
	void populateSkyPresetsList();
	// void populateDayCyclePresetsList();
	void updateTimeSlider();

	void onRegionChanged();
	void onSimulatorFeaturesReceived(const LLUUID& region_id);
	void updateEditHoverEnabled();

	LLComboBox*		mWaterPresetCombo;
	LLComboBox*		mSkyPresetCombo;
	// LLComboBox*		mDayCyclePresetCombo;
	LLSliderCtrl*		mTimeSlider;
	LLSliderCtrl*		mHoverHeight;

	bool mExpanded;

	boost::ptr_list<boost::signals2::scoped_connection> mConnections;
	boost::signals2::connection mRegionChangedSlot;
};

#endif // LL_wlfPanel_AdvSettings_H
