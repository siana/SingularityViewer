/** 
 * @file llviewerstats.h
 * @brief LLViewerStats class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERSTATS_H
#define LL_LLVIEWERSTATS_H

#include "llstat.h"
#include "lltextureinfo.h"

class LLViewerStats : public LLSingleton<LLViewerStats>
{
public:
	LLStat	mKBitStat,
			mLayersKBitStat,
			mObjectKBitStat,
			mAssetKBitStat,
			mHTTPTextureKBitStat,
			mUDPTextureKBitStat,
			mVFSPendingOperations,
			mObjectsDrawnStat,
			mObjectsCulledStat,
			mObjectsTestedStat,
			mObjectsComparedStat,
			mObjectsOccludedStat,
			mFPSStat,
			mPacketsInStat,
			mPacketsLostStat,
			mPacketsOutStat,
			mPacketsLostPercentStat,
			mTexturePacketsStat,
			mActualInKBitStat,	// From the packet ring (when faking a bad connection)
			mActualOutKBitStat,	// From the packet ring (when faking a bad connection)
			mTrianglesDrawnStat,
			mMallocStat;

	// Simulator stats
	LLStat	mSimTimeDilation,

			mSimFPS,
			mSimPhysicsFPS,
			mSimAgentUPS,
			mSimScriptEPS,

			mSimFrameMsec,
			mSimNetMsec,
			mSimSimOtherMsec,
			mSimSimPhysicsMsec,

			mSimSimPhysicsStepMsec,
			mSimSimPhysicsShapeUpdateMsec,
			mSimSimPhysicsOtherMsec,
			mSimSimAIStepMsec,
			mSimSimSkippedSilhouetteSteps,
			mSimSimPctSteppedCharacters,

			mSimAgentMsec,
			mSimImagesMsec,
			mSimScriptMsec,
			mSimSpareMsec,
			mSimSleepMsec,
			mSimPumpIOMsec,

			mSimMainAgents,
			mSimChildAgents,
			mSimObjects,
			mSimActiveObjects,
			mSimActiveScripts,
			mSimPctScriptsRun,

			mSimInPPS,
			mSimOutPPS,
			mSimPendingDownloads,
			mSimPendingUploads,
			mSimPendingLocalUploads,
			mSimTotalUnackedBytes,

			mPhysicsPinnedTasks,
			mPhysicsLODTasks,
			mPhysicsMemoryAllocated,

			mSimPingStat,

			mNumImagesStat,
			mNumRawImagesStat,
			mGLTexMemStat,
			mGLBoundMemStat,
			mRawMemStat,
			mFormattedMemStat,

			mNumObjectsStat,
			mNumActiveObjectsStat,
			mNumNewObjectsStat,
			mNumSizeCulledStat,
			mNumVisCulledStat;

	void resetStats();
public:
	// If you change this, please also add a corresponding text label in llviewerstats.cpp
	enum EStatType
	{
		ST_VERSION = 0,
		ST_AVATAR_EDIT_SECONDS = 1,
		ST_TOOLBOX_SECONDS = 2,
		ST_CHAT_COUNT = 3,
		ST_IM_COUNT = 4,
		ST_FULLSCREEN_BOOL = 5,
		ST_RELEASE_COUNT= 6,
		ST_CREATE_COUNT = 7,
		ST_REZ_COUNT = 8,
		ST_FPS_10_SECONDS = 9,
		ST_FPS_2_SECONDS = 10,
		ST_MOUSELOOK_SECONDS = 11,
		ST_FLY_COUNT = 12,
		ST_TELEPORT_COUNT = 13,
		ST_OBJECT_DELETE_COUNT = 14,
		ST_SNAPSHOT_COUNT = 15,
		ST_UPLOAD_SOUND_COUNT = 16,
		ST_UPLOAD_TEXTURE_COUNT = 17,
		ST_EDIT_TEXTURE_COUNT = 18,
		ST_KILLED_COUNT = 19,
		ST_FRAMETIME_JITTER = 20,
		ST_FRAMETIME_SLEW = 21,
		ST_INVENTORY_TOO_LONG = 22,
		ST_WEARABLES_TOO_LONG = 23,
		ST_LOGIN_SECONDS = 24,
		ST_LOGIN_TIMEOUT_COUNT = 25,
		ST_HAS_BAD_TIMER = 26,
		ST_DOWNLOAD_FAILED = 27,
		ST_LSL_SAVE_COUNT = 28,
		ST_UPLOAD_ANIM_COUNT = 29,
		ST_FPS_8_SECONDS = 30,
		ST_SIM_FPS_20_SECONDS = 31,
		ST_PHYS_FPS_20_SECONDS = 32,
		ST_LOSS_05_SECONDS = 33,
		ST_FPS_DROP_50_RATIO = 34,
		ST_ENABLE_VBO = 35,
		ST_DELTA_BANDWIDTH = 36,
		ST_MAX_BANDWIDTH = 37,
		ST_LIGHTING_DETAIL = 38,
		ST_VISIBLE_AVATARS = 39,
		ST_SHADER_OBJECTS = 40,
		ST_SHADER_ENVIRONMENT = 41,
		ST_DRAW_DIST = 42,
		ST_CHAT_BUBBLES = 43,
		ST_SHADER_AVATAR = 44,
		ST_FRAME_SECS = 45,
		ST_UPDATE_SECS = 46,
		ST_NETWORK_SECS = 47,
		ST_IMAGE_SECS = 48,
		ST_REBUILD_SECS = 49,
		ST_RENDER_SECS = 50,
		ST_CROSSING_AVG = 51,
		ST_CROSSING_MAX = 52,
		ST_LIBXUL_WIDGET_USED = 53, // Unused
		ST_WINDOW_WIDTH = 54,
		ST_WINDOW_HEIGHT = 55,
		ST_TEX_BAKES = 56,
		ST_TEX_REBAKES = 57,
		
		ST_COUNT = 58
	};


	LLViewerStats();
	~LLViewerStats();

	// all return latest value of given stat
	F64 getStat(EStatType type) const;
	F64 setStat(EStatType type, F64 value);		// set the stat to value
	F64 incStat(EStatType type, F64 value = 1.f);	// add value to the stat

	void updateFrameStats(const F64 time_diff);
	
	void addToMessage(LLSD &body) const;

	struct  StatsAccumulator
	{
		S32 mCount;
		F32 mSum;
		F32 mSumOfSquares;
		F32 mMinValue;
		F32 mMaxValue;
		U32 mCountOfNextUpdatesToIgnore;

		inline StatsAccumulator()
		{
			reset();
		}

		inline void push( F32 val )
		{
			if ( mCountOfNextUpdatesToIgnore > 0 )
			{
				mCountOfNextUpdatesToIgnore--;
				return;
			}
			
			mCount++;
			mSum += val;
			mSumOfSquares += val * val;
			if (mCount == 1 || val > mMaxValue)
			{
				mMaxValue = val;
			}
			if (mCount == 1 || val < mMinValue)
			{
				mMinValue = val;
			}
		}
		
		inline F32 getMean() const
		{
			return (mCount == 0) ? 0.f : ((F32)mSum)/mCount;
		}

		inline F32 getMinValue() const
		{
			return mMinValue;
		}

		inline F32 getMaxValue() const
		{
			return mMaxValue;
		}

		inline F32 getStdDev() const
		{
			const F32 mean = getMean();
			return (mCount < 2) ? 0.f : sqrt(llmax(0.f,mSumOfSquares/mCount - (mean * mean)));
		}
		
		inline U32 getCount() const
		{
			return mCount;
		}

		inline void reset()
		{
			mCount = 0;
			mSum = mSumOfSquares = 0.f;
			mMinValue = 0.0f;
			mMaxValue = 0.0f;
			mCountOfNextUpdatesToIgnore = 0;
		}
		
		inline LLSD getData() const
		{
			LLSD data;
			data["mean"] = getMean();
			data["std_dev"] = getStdDev();
			data["count"] = (S32)mCount;
			data["min"] = getMinValue();
			data["max"] = getMaxValue();
			return data;
		}
	};

	StatsAccumulator mAgentPositionSnaps;

	// Phase tracking (originally put in for avatar rezzing), tracking
	// progress of active/completed phases for activities like outfit changing.
	typedef std::map<std::string,LLTimer>	phase_map_t;
	typedef std::map<std::string,StatsAccumulator>	phase_stats_t;
	class PhaseMap
	{
	private:
		phase_map_t mPhaseMap;
	public:
		PhaseMap();
		LLTimer&	 	getPhaseTimer(const std::string& phase_name);
		bool 			getPhaseValues(const std::string& phase_name, F32& elapsed, bool& completed);
		void			startPhase(const std::string& phase_name);
		void			stopPhase(const std::string& phase_name);
		void			clearPhases();
		LLSD			dumpPhases();
		phase_map_t::iterator begin() { return mPhaseMap.begin(); }
		phase_map_t::iterator end() { return mPhaseMap.end(); }
	};

private:
	F64	mStats[ST_COUNT];

	F64 mLastTimeDiff;  // used for time stat updates
};

static const F32 SEND_STATS_PERIOD = 300.0f;

// The following are from (older?) statistics code found in appviewer.
void reset_statistics();
void output_statistics(void*);
void update_statistics();
void send_stats();

extern LLFrameTimer gTextureTimer;
extern U32	gTotalTextureBytes;
extern U32  gTotalObjectBytes;
extern U32  gTotalTextureBytesPerBoostLevel[] ;
#endif // LL_LLVIEWERSTATS_H
