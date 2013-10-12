/** 
 * @file llfloaterregioninfo.h
 * @author Aaron Brashears
 * @brief Declaration of the region info and controls floater and panels.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERREGIONINFO_H
#define LL_LLFLOATERREGIONINFO_H

#include <vector>
#include "llfloater.h"
#include "llpanel.h"

#include "llenvmanager.h" // for LLEnvironmentSettings

class LLAvatarName;
struct LLEstateAccessChangeInfo;
class LLLineEditor;
class LLMessageSystem;
class LLPanelRegionInfo;
class LLTabContainer;
class LLViewerRegion;
class LLViewerTextEditor;
class LLInventoryItem;
class LLCheckBoxCtrl;
class LLComboBox;
class LLNameListCtrl;
class LLRadioGroup;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;
class AIFilePicker;

class LLPanelRegionGeneralInfo;
class LLPanelRegionDebugInfo;
class LLPanelRegionTerrainInfo;
class LLPanelEstateInfo;
class LLPanelEstateCovenant;


class LLFloaterRegionInfo : public LLFloater, public LLFloaterSingleton<LLFloaterRegionInfo>
{
	friend class LLUISingleton<LLFloaterRegionInfo, VisibilityPolicy<LLFloater> >;
public:


	/*virtual*/ void onOpen();
	/*virtual*/ BOOL postBuild();
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
	/*virtual*/ void open();
// [/RLVa:KB]

	static void processEstateOwnerRequest(LLMessageSystem* msg, void**);

	// get and process region info if necessary.
	static void processRegionInfo(LLMessageSystem* msg);

	static const LLUUID& getLastInvoice() { return sRequestInvoice; }
	static void nextInvoice() { sRequestInvoice.generate(); }
	//static S32 getSerial() { return sRequestSerial; }
	//static void incrementSerial() { sRequestSerial++; }

	static LLPanelEstateInfo* getPanelEstate();
	static LLPanelEstateCovenant* getPanelCovenant();
	static LLPanelRegionTerrainInfo* getPanelRegionTerrain();

	// from LLPanel
	virtual void refresh();
	
	void requestRegionInfo();

protected:
	LLFloaterRegionInfo(const LLSD& seed);
	~LLFloaterRegionInfo();

	void onTabSelected(const LLSD& param);
	void refreshFromRegion(LLViewerRegion* region);

	// member data
	LLTabContainer* mTab;
	typedef std::vector<LLPanelRegionInfo*> info_panels_t;
	info_panels_t mInfoPanels;
	//static S32 sRequestSerial;	// serial # of last EstateOwnerRequest
	static LLUUID sRequestInvoice;
};


// Base class for all region information panels.
class LLPanelRegionInfo : public LLPanel
{
public:
	LLPanelRegionInfo();

	void onBtnSet();
	void onChangeChildCtrl(LLUICtrl* ctrl);
	void onChangeAnything();
	static void onChangeText(LLLineEditor* caller, void* user_data);
	
	virtual bool refreshFromRegion(LLViewerRegion* region);
	virtual bool estateUpdate(LLMessageSystem* msg) { return true; }
	
	virtual BOOL postBuild();
	virtual void updateChild(LLUICtrl* child_ctrl);
	virtual void onOpen(const LLSD& key) {}
	
	void enableButton(const std::string& btn_name, BOOL enable = TRUE);
	void disableButton(const std::string& btn_name);
	
	void onClickManageTelehub();

protected:
	void initCtrl(const std::string& name);
	void initHelpBtn(const std::string& name, const std::string& xml_alert);

	// Callback for all help buttons, xml_alert is the name of XML alert to show.
	void onClickHelp(const std::string& xml_alert);
	
	// Returns TRUE if update sent and apply button should be
	// disabled.
	virtual BOOL sendUpdate() { return TRUE; }
	
	typedef std::vector<std::string> strings_t;
	//typedef std::vector<U32> integers_t;
	void sendEstateOwnerMessage(
					 LLMessageSystem* msg,
					 const std::string& request,
					 const LLUUID& invoice,
					 const strings_t& strings);
	
	// member data
	LLHost mHost;
};

/////////////////////////////////////////////////////////////////////////////
// Actual panels start here
/////////////////////////////////////////////////////////////////////////////

class LLPanelRegionGeneralInfo : public LLPanelRegionInfo
{

public:
	LLPanelRegionGeneralInfo()
		:	LLPanelRegionInfo()	{}
	~LLPanelRegionGeneralInfo() {}
	
	virtual bool refreshFromRegion(LLViewerRegion* region);
	
	// LLPanel
	virtual BOOL postBuild();

protected:
	virtual BOOL sendUpdate();
	void onClickKick();
	void onKickCommit(const uuid_vec_t& ids);
	static void onClickKickAll(void* userdata);
	bool onKickAllCommit(const LLSD& notification, const LLSD& response);
	static void onClickMessage(void* userdata);
	bool onMessageCommit(const LLSD& notification, const LLSD& response);

};

/////////////////////////////////////////////////////////////////////////////

class LLPanelRegionDebugInfo : public LLPanelRegionInfo
{
public:
	LLPanelRegionDebugInfo()
		:	LLPanelRegionInfo(), mTargetAvatar() {}
	~LLPanelRegionDebugInfo() {}
	// LLPanel
	virtual BOOL postBuild();
	
	virtual bool refreshFromRegion(LLViewerRegion* region);
	
protected:
	virtual BOOL sendUpdate();

	void onClickChooseAvatar();
	void callbackAvatarID(const uuid_vec_t& ids, const std::vector<LLAvatarName>& names);
	static void onClickReturn(void *);
	bool callbackReturn(const LLSD& notification, const LLSD& response);
	static void onClickTopColliders(void*);
	static void onClickTopScripts(void*);
	static void onClickRestart(void* data);
	bool callbackRestart(const LLSD& notification, const LLSD& response);
	static void onClickCancelRestart(void* data);
	
private:
	LLUUID mTargetAvatar;
};

/////////////////////////////////////////////////////////////////////////////

class LLPanelRegionTerrainInfo : public LLPanelRegionInfo
{
	LOG_CLASS(LLPanelRegionTerrainInfo);

public:
	LLPanelRegionTerrainInfo() : LLPanelRegionInfo() {}
	~LLPanelRegionTerrainInfo() {}

	virtual BOOL postBuild();												// LLPanel
	
	virtual bool refreshFromRegion(LLViewerRegion* region);					// refresh local settings from region update from simulator

	BOOL validateTextureSizes();

protected:

	//static void onChangeAnything(LLUICtrl* ctrl, void* userData);			// callback for any change, to enable commit button

	virtual BOOL sendUpdate();

	static void onClickDownloadRaw(void*);
	void onClickDownloadRaw_continued(AIFilePicker* filepicker);
	static void onClickUploadRaw(void*);
	void onClickUploadRaw_continued(AIFilePicker* filepicker);
	static void onClickBakeTerrain(void*);
	bool callbackBakeTerrain(const LLSD& notification, const LLSD& response);
};

/////////////////////////////////////////////////////////////////////////////

class LLPanelEstateInfo : public LLPanelRegionInfo
{
public:
	static void initDispatch(LLDispatcher& dispatch);
	
	void onChangeFixedSun();
	void onChangeUseGlobalTime();
	
	void onClickEditSky();
	void onClickEditSkyHelp();
	void onClickEditDayCycle();
	void onClickEditDayCycleHelp();

	void onClickAddAllowedAgent();
	void onClickRemoveAllowedAgent();
	void onClickAddAllowedGroup();
	void onClickRemoveAllowedGroup();
	void onClickAddBannedAgent();
	void onClickRemoveBannedAgent();
	void onClickAddEstateManager();
	void onClickRemoveEstateManager();
	void onClickKickUser();

	// Group picker callback is different, can't use core methods below
	bool addAllowedGroup(const LLSD& notification, const LLSD& response);
	void addAllowedGroup2(LLUUID id);

	// Core methods for all above add/remove button clicks
	static void accessAddCore(U32 operation_flag, const std::string& dialog_name);
	static bool accessAddCore2(const LLSD& notification, const LLSD& response);
	static void accessAddCore3(const uuid_vec_t& ids, LLEstateAccessChangeInfo* change_info);

	static void accessRemoveCore(U32 operation_flag, const std::string& dialog_name, const std::string& list_ctrl_name);
	static bool accessRemoveCore2(const LLSD& notification, const LLSD& response);

	// used for both add and remove operations
	static bool accessCoreConfirm(const LLSD& notification, const LLSD& response);
	bool kickUserConfirm(const LLSD& notification, const LLSD& response);

	// Send the actual EstateOwnerRequest "estateaccessdelta" message
	static void sendEstateAccessDelta(U32 flags, const LLUUID& agent_id);

	void onKickUserCommit(const uuid_vec_t& ids, const std::vector<LLAvatarName>& names);
	static void onClickMessageEstate(void* data);
	bool onMessageCommit(const LLSD& notification, const LLSD& response);
	
	LLPanelEstateInfo();
	~LLPanelEstateInfo() {}
	
	void updateControls(LLViewerRegion* region);
	
	static void updateEstateName(const std::string& name);
	static void updateEstateOwnerName(const std::string& name);

	virtual bool refreshFromRegion(LLViewerRegion* region);
	virtual bool estateUpdate(LLMessageSystem* msg);
	
	// LLPanel
	virtual BOOL postBuild();
	virtual void updateChild(LLUICtrl* child_ctrl);
	virtual void refresh();

	void refreshFromEstate();
	
	static bool isLindenEstate();
	
	const std::string getOwnerName() const;
	void setOwnerName(const std::string& name);
	void setOwnerPNSName(const LLUUID& agent_id, const LLAvatarName& av_name);

protected:
	virtual BOOL sendUpdate();
	// confirmation dialog callback
	bool callbackChangeLindenEstate(const LLSD& notification, const LLSD& response);

	void commitEstateAccess();
	void commitEstateManagers();
	
	void clearAccessLists();
	BOOL checkSunHourSlider(LLUICtrl* child_ctrl);

	U32 mEstateID;
};

/////////////////////////////////////////////////////////////////////////////

class LLPanelEstateCovenant : public LLPanelRegionInfo
{
public:
	LLPanelEstateCovenant();
	~LLPanelEstateCovenant() {}
	
	// LLPanel
	virtual BOOL postBuild();
	virtual void updateChild(LLUICtrl* child_ctrl);
	virtual bool refreshFromRegion(LLViewerRegion* region);
	virtual bool estateUpdate(LLMessageSystem* msg);

	// LLView overrides
	BOOL handleDragAndDrop(S32 x, S32 y, MASK mask,
						   BOOL drop, EDragAndDropType cargo_type,
						   void *cargo_data, EAcceptance *accept,
						   std::string& tooltip_msg);
	static bool confirmChangeCovenantCallback(const LLSD& notification, const LLSD& response);
	static void resetCovenantID(void* userdata);
	static bool confirmResetCovenantCallback(const LLSD& notification, const LLSD& response);
	void sendChangeCovenantID(const LLUUID &asset_id);
	void loadInvItem(LLInventoryItem *itemp);
	static void onLoadComplete(LLVFS *vfs,
							   const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);

	// Accessor functions
	static void updateCovenantText(const std::string& string, const LLUUID& asset_id);
	static void updateEstateName(const std::string& name);
	static void updateLastModified(const std::string& text);
	static void updateEstateOwnerName(const std::string& name);

	const LLUUID& getCovenantID() const { return mCovenantID; }
	void setCovenantID(const LLUUID& id) { mCovenantID = id; }
	std::string getEstateName() const;
	void setEstateName(const std::string& name);
	std::string getOwnerName() const;
	void setOwnerName(const std::string& name);
	void setCovenantTextEditor(const std::string& text);

	typedef enum e_asset_status
	{
		ASSET_ERROR,
		ASSET_UNLOADED,
		ASSET_LOADING,
		ASSET_LOADED
	} EAssetStatus;

protected:
	virtual BOOL sendUpdate();
	LLTextBox*				mEstateNameText;
	LLTextBox*				mEstateOwnerText;
	LLTextBox*				mLastModifiedText;
	// CovenantID from sim
	LLUUID					mCovenantID;
	LLViewerTextEditor*		mEditor;
	EAssetStatus			mAssetStatus;
};

/////////////////////////////////////////////////////////////////////////////

class LLPanelEnvironmentInfo : public LLPanelRegionInfo
{
	LOG_CLASS(LLPanelEnvironmentInfo);

public:
	LLPanelEnvironmentInfo();

	// LLPanel
	/*virtual*/ BOOL postBuild();

	// LLPanelRegionInfo
	/*virtual*/ void onOpen(const LLSD& key);

	// LLView
	/*virtual*/ void handleVisibilityChange(BOOL new_visibility);

	// LLPanelRegionInfo
	/*virtual*/ bool refreshFromRegion(LLViewerRegion* region);

private:
	void refresh();
	void setControlsEnabled(bool enabled);
	void setApplyProgress(bool started);
	void setDirty(bool dirty);

	void sendRegionSunUpdate();
	void fixEstateSun();

	void populateWaterPresetsList();
	void populateSkyPresetsList();
	void populateDayCyclesList();

	bool getSelectedWaterParams(LLSD& water_params);
	bool getSelectedSkyParams(LLSD& sky_params, std::string& preset_name);
	bool getSelectedDayCycleParams(LLSD& day_cycle, LLSD& sky_map, short& scope);

	void onSwitchRegionSettings();
	void onSwitchDayCycle();

	void onSelectWaterPreset();
	void onSelectSkyPreset();
	void onSelectDayCycle();

	void onBtnApply();
	void onBtnCancel();

	void onRegionSettingschange();
	void onRegionSettingsApplied(bool ok);

	/// New environment settings that are being applied to the region.
	LLEnvironmentSettings	mNewRegionSettings;

	bool			mEnableEditing;

	LLRadioGroup*	mRegionSettingsRadioGroup;
	LLRadioGroup*	mDayCycleSettingsRadioGroup;

	LLComboBox*		mWaterPresetCombo;
	LLComboBox*		mSkyPresetCombo;
	LLComboBox*		mDayCyclePresetCombo;
};

#endif
