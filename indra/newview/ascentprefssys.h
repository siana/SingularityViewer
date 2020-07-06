/** 
 * @file hbprefsinert.h
 * @brief  Ascent Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Henri Beauchamp.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef ASCENTPREFSSYS_H
#define ASCENTPREFSSYS_H


#include "llpanel.h"

class LLPrefsAscentSys : public LLPanel
{
public:
	LLPrefsAscentSys();
	~LLPrefsAscentSys();

	void apply();
	void cancel();
	void refresh();
	void refreshValues();

protected:
	void onCommitCheckBox(LLUICtrl* ctrl, const LLSD& value);
	void onCommitComboBox(LLUICtrl* ctrl, const LLSD& value);
	void onCommitTexturePicker(LLUICtrl* ctrl);

private:
	//General -----------------------------------------------------------------------------
	bool mDoubleClickTeleport;
	bool mDoubleClickAutoPilot;
	bool mResetCameraAfterTP;
	bool mOffsetTPByUserHeight;
	bool mClearBeaconAfterTeleport;
	bool mLiruFlyAfterTeleport;
	bool mLiruContinueFlying;
	bool mPreviewAnimInWorld;
	bool mSaveScriptsAsMono;
	bool mAlwaysRezInGroup;
	bool mBuildAlwaysEnabled;
	bool mAlwaysShowFly;
	bool mDisableMinZoom;
	bool mPowerUser;
	bool mFetchInventoryOnLogin;
	bool mEnableLLWind;
	bool mEnableClouds;
	bool mEnableClassicClouds;
	bool mSpeedRez;
	U32 mSpeedRezInterval;
	bool mUseContextMenus;
	bool mUseWebProfiles;
	bool mUseWebSearch;

	//Command Line ------------------------------------------------------------------------
	bool mCmdLine;
	std::string mCmdLinePos;
	std::string mCmdLineGround;
	std::string mCmdLineHeight;
	std::string mCmdLineTeleportHome;
	std::string mCmdLineRezPlatform;
	F32 mCmdPlatformSize;
	std::string mCmdLineCalc;
	std::string mCmdLineClearChat;
	std::string mCmdLineDrawDistance;
	std::string mCmdTeleportToCam;
	std::string mCmdLineKeyToName;
	std::string mCmdLineOfferTp;
	std::string mCmdLineMapTo;
	bool mCmdMapToKeepPos;
	std::string mCmdLineTP2;
	std::string mCmdLineAway;
	std::string mCmdLineRegionSay;
	std::string mCmdLineURL;
	std::string mCmdLineResync;
	std::string mCmdLineHover;

	//Security ----------------------------------------------------------------------------
	bool mBroadcastViewerEffects;
	bool mDisablePointAtAndBeam;
	bool mPrivateLookAt;
	bool mHideOwnLookAt;
	bool mShowLookAt;
	S32  mLookAtNames;
	bool mLookAtLines;
	bool mQuietSnapshotsToDisk;
	bool mAnnounceBumps;
	bool mSitOnAway;
	bool mDetachBridge;
	bool mRevokePermsOnStandUp;
	bool mDisableClickSit;
	bool mDisableClickSitOtherOwner;
	bool mDisplayScriptJumps;
	bool mReplaceLinks;
	F32 mNumScriptDiff;
	U32 mEmergencySeconds;
	std::string mLandmark;
	std::string mLandmarkBackup;

	//Build -------------------------------------------------------------------------------
	F32 mAlpha;
	LLColor4 mColor;
	BOOL mFullBright;
	F32 mGlow;
	std::string mItem;
	std::string mMaterial;
	bool mNextCopy;
	bool mNextMod;
	bool mNextTrans;
	bool mScriptNextCopy;
	bool mScriptNextMod;
	bool mScriptNextTrans;
	std::string mShiny;
	bool mTemporary;
	std::string mTexture;
	bool mPhantom;
	bool mPhysical;
	F32 mXsize;
	F32 mYsize;
	F32 mZsize;
};

#endif
