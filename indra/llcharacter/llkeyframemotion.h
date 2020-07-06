/** 
 * @file llkeyframemotion.h
 * @brief Implementation of LLKeframeMotion class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * AICachedPointer and AICachedPointPtr copyright (c) 2013, Aleric Inglewood.
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

#ifndef LL_LLKEYFRAMEMOTION_H
#define LL_LLKEYFRAMEMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------

#include <string>

#include "llassetstorage.h"
#include "llbboxlocal.h"
#include "llhandmotion.h"
#include "lljointstate.h"
#include "llmotion.h"
#include "llquaternion.h"
#include "v3dmath.h"
#include "v3math.h"
#include "llbvhconsts.h"
#include <boost/intrusive_ptr.hpp>

class LLKeyframeDataCache;
class LLVFS;
class LLDataPacker;
class LLMotionController;

#define MIN_REQUIRED_PIXEL_AREA_KEYFRAME (40.f)
#define MAX_CHAIN_LENGTH (4)

const S32 KEYFRAME_MOTION_VERSION = 1;
const S32 KEYFRAME_MOTION_SUBVERSION = 0;

//-----------------------------------------------------------------------------
// <singu>

template<typename KEY, typename T>
class AICachedPointer;

template<typename KEY, typename T>
void intrusive_ptr_add_ref(AICachedPointer<KEY, T> const* p);

template<typename KEY, typename T>
void intrusive_ptr_release(AICachedPointer<KEY, T> const* p);

template<typename KEY, typename T>
class AICachedPointer
{
  public:
	typedef std::set<AICachedPointer<KEY, T> > container_type;

  private:
	KEY mKey;							// The unique key.
	LLPointer<T> mData;					// The actual data pointer.
	container_type* mCache;				// Pointer to the underlaying cache.
	mutable int mRefCount;				// Number of AICachedPointerPtr's pointing to this object.

  public:
	// Construct a NULL pointer. This is needed when adding a new entry to a std::set, it is always first default constructed.
	AICachedPointer(void) : mCache(NULL) { }

	// Copy constructor. This is needed to replace the std::set inserted instance with its actual value.
	AICachedPointer(AICachedPointer const& cptr) : mKey(cptr.mKey), mData(cptr.mData), mCache(cptr.mCache), mRefCount(0) { }

	// Construct a AICachedPointer that points to 'ptr' with key 'key'.
	AICachedPointer(KEY const& key, T* ptr, container_type* cache) : mKey(key), mData(ptr), mCache(cache), mRefCount(-1) { }

	// Construct a temporary NULL pointer that can be used in a search for a key.
	AICachedPointer(KEY const& key) : mKey(key), mCache(NULL) { }

	// Accessors for key and data.
	KEY const& key(void) const { return mKey; }
	T const* get(void) const { return mData.get(); }
	T* get(void) { return mData.get(); }

	// Order only by key.
	friend bool operator<(AICachedPointer const& cp1, AICachedPointer const& cp2) { return cp1.mKey < cp2.mKey; }

  private:
	friend void intrusive_ptr_add_ref<>(AICachedPointer<KEY, T> const* p);
	friend void intrusive_ptr_release<>(AICachedPointer<KEY, T> const* p);

  private:
	AICachedPointer& operator=(AICachedPointer const&);
};

template<typename KEY, typename T>
void intrusive_ptr_add_ref(AICachedPointer<KEY, T> const* p)
{
  llassert(p->mCache);
  if (p->mCache)
  {
	p->mRefCount++;
  }
}

template<typename KEY, typename T>
void intrusive_ptr_release(AICachedPointer<KEY, T> const* p)
{
  llassert(p->mCache);
  if (p->mCache)
  {
	if (--p->mRefCount == 0)
	{
	  p->mCache->erase(p->mKey);
	}
  }
}

template<typename KEY, typename T>
class AICachedPointerPtr
{
  private:
	boost::intrusive_ptr<AICachedPointer<KEY, T> const> mPtr;
	static int sCnt;

  public:
	AICachedPointerPtr(void) { ++sCnt; }
	AICachedPointerPtr(AICachedPointerPtr const& cpp) : mPtr(cpp.mPtr) { ++sCnt; }
	AICachedPointerPtr(AICachedPointer<KEY, T> const* cp) : mPtr(cp) { ++sCnt; }
	~AICachedPointerPtr() { --sCnt; }

	typedef boost::intrusive_ptr<AICachedPointer<KEY, T> const> const AICachedPointerPtr<KEY, T>::* const bool_type;
	operator bool_type() const { return mPtr ? &AICachedPointerPtr<KEY, T>::mPtr : NULL; }

	T const* operator->() const { return mPtr->get(); }
	T* operator->() { return const_cast<AICachedPointer<KEY, T>&>(*mPtr).get(); }
	T const& operator*() const { return *mPtr->get(); }
	T& operator*() { return *const_cast<AICachedPointer<KEY, T>&>(*mPtr).get(); }

	AICachedPointerPtr& operator=(AICachedPointerPtr const& cpp) { mPtr = cpp.mPtr; return *this; }
};

template<typename KEY, typename T>
int AICachedPointerPtr<KEY, T>::sCnt;

// </singu>
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// class LLKeyframeMotion
//-----------------------------------------------------------------------------

namespace LLKeyframeMotionLerp
{
	template<typename T>
	inline T lerp(F32 t, const T& before, const T& after)
	{
		return ::lerp(before, after, t);
	}

	template<>
	inline LLQuaternion lerp(F32 t, const LLQuaternion& before, const LLQuaternion& after)
	{
		return nlerp(t, before, after);
	}
}

class LLKeyframeMotion :
	public LLMotion
{
	friend class LLKeyframeDataCache;
public:
	// Constructor
	LLKeyframeMotion(const LLUUID &id, LLMotionController* controller);

	// Destructor
	virtual ~LLKeyframeMotion();

private:
	// private helper functions to wrap some asserts
	LLPointer<LLJointState>& getJointState(U32 index);
	LLJoint* getJoint(U32 index);
	
public:
	//-------------------------------------------------------------------------
	// functions to support MotionController and MotionRegistry
	//-------------------------------------------------------------------------

	// static constructor
	// all subclasses must implement such a function and register it
	static LLMotion* create(LLUUID const& id, LLMotionController* controller);

public:
	//-------------------------------------------------------------------------
	// animation callbacks to be implemented by subclasses
	//-------------------------------------------------------------------------

	// motions must specify whether or not they loop
	virtual BOOL getLoop() { 
		if (mJointMotionList) return mJointMotionList->mLoop; 
		else return FALSE;
	}

	// motions must report their total duration
	virtual F32 getDuration() { 
		if (mJointMotionList) return mJointMotionList->mDuration; 
		else return 0.f;
	}

	// motions must report their "ease in" duration
	virtual F32 getEaseInDuration() { 
		if (mJointMotionList) return mJointMotionList->mEaseInDuration; 
		else return 0.f;
	}

	// motions must report their "ease out" duration.
	virtual F32 getEaseOutDuration() { 
		if (mJointMotionList) return mJointMotionList->mEaseOutDuration; 
		else return 0.f;
	}

	// motions must report their priority
	virtual LLJoint::JointPriority getPriority() { 
		if (mJointMotionList) return mJointMotionList->mBasePriority; 
		else return LLJoint::LOW_PRIORITY;
	}

	virtual LLMotionBlendType getBlendType() { return NORMAL_BLEND; }

	// called to determine when a motion should be activated/deactivated based on avatar pixel coverage
	virtual F32 getMinPixelArea() { return MIN_REQUIRED_PIXEL_AREA_KEYFRAME; }

	// run-time (post constructor) initialization,
	// called after parameters have been set
	// must return true to indicate success and be available for activation
	virtual LLMotionInitStatus onInitialize(LLCharacter *character);

	// called when a motion is activated
	// must return TRUE to indicate success, or else
	// it will be deactivated
	virtual BOOL onActivate();

	// called per time step
	// must return TRUE while it is active, and
	// must return FALSE when the motion is completed.
	virtual BOOL onUpdate(F32 time, U8* joint_mask);

	// called when a motion is deactivated
	virtual void onDeactivate();

	virtual void setStopTime(F32 time);

	static void setVFS(LLVFS* vfs) { sVFS = vfs; }

	static void onLoadComplete(LLVFS *vfs,
							   const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);

public:
	U32		getFileSize();
	BOOL	serialize(LLDataPacker& dp) const;
	BOOL	deserialize(LLDataPacker& dp, const LLUUID& asset_id);
	BOOL	isLoaded() { return !!mJointMotionList; }


	// setters for modifying a keyframe animation
	void setLoop(BOOL loop);

	F32 getLoopIn() {
		return (mJointMotionList) ? mJointMotionList->mLoopInPoint : 0.f;
	}

	F32 getLoopOut() {
		return (mJointMotionList) ? mJointMotionList->mLoopOutPoint : 0.f;
	}
	
	void setLoopIn(F32 in_point);

	void setLoopOut(F32 out_point);

	void setHandPose(LLHandMotion::eHandPose pose) {
		if (mJointMotionList) mJointMotionList->mHandPose = pose;
	}

	LLHandMotion::eHandPose getHandPose() { 
		return (mJointMotionList) ? mJointMotionList->mHandPose : LLHandMotion::HAND_POSE_RELAXED;
	}

	void setPriority(S32 priority);

	void setEmote(const LLUUID& emote_id);

	void setEaseIn(F32 ease_in);

	void setEaseOut(F32 ease_in);

	F32 getLastUpdateTime() { return mLastLoopedTime; }

	const LLBBoxLocal& getPelvisBBox();

	static void flushKeyframeCache();

protected:
	//-------------------------------------------------------------------------
	// JointConstraintSharedData
	//-------------------------------------------------------------------------
	class JointConstraintSharedData
	{
	public:
		JointConstraintSharedData() :
			mChainLength(0),
			mEaseInStartTime(0.f), 
			mEaseInStopTime(0.f),
			mEaseOutStartTime(0.f),
			mEaseOutStopTime(0.f), 
			mUseTargetOffset(FALSE),
			mConstraintType(CONSTRAINT_TYPE_POINT),
			mConstraintTargetType(CONSTRAINT_TARGET_TYPE_BODY),
			mSourceConstraintVolume(0),
			mTargetConstraintVolume(0),
			mJointStateIndices(NULL)
		{ };
		~JointConstraintSharedData() { delete [] mJointStateIndices; }

		S32						mSourceConstraintVolume;
		LLVector3				mSourceConstraintOffset;
		S32						mTargetConstraintVolume;
		LLVector3				mTargetConstraintOffset;
		LLVector3				mTargetConstraintDir;
		S32						mChainLength;
		S32*					mJointStateIndices;
		F32						mEaseInStartTime;
		F32						mEaseInStopTime;
		F32						mEaseOutStartTime;
		F32						mEaseOutStopTime;
		BOOL					mUseTargetOffset;
		EConstraintType			mConstraintType;
		EConstraintTargetType	mConstraintTargetType;
	};

	//-----------------------------------------------------------------------------
	// JointConstraint()
	//-----------------------------------------------------------------------------
	class JointConstraint
	{
	public:
		JointConstraint(JointConstraintSharedData* shared_data);
		~JointConstraint();

		JointConstraintSharedData*	mSharedData;
		F32							mWeight;
		F32							mTotalLength;
		LLVector3					mPositions[MAX_CHAIN_LENGTH];
		F32							mJointLengths[MAX_CHAIN_LENGTH];
		F32							mJointLengthFractions[MAX_CHAIN_LENGTH];
		BOOL						mActive;
		LLVector3d					mGroundPos;
		LLVector3					mGroundNorm;
		LLJoint*					mSourceVolume;
		LLJoint*					mTargetVolume;
		F32							mFixupDistanceRMS;
	};

	void applyKeyframes(F32 time);

	void applyConstraints(F32 time, U8* joint_mask);

	void activateConstraint(JointConstraint* constraintp);

	void initializeConstraint(JointConstraint* constraint);

	void deactivateConstraint(JointConstraint *constraintp);

	void applyConstraint(JointConstraint* constraintp, F32 time, U8* joint_mask);

	BOOL	setupPose();

public:
	enum AssetStatus { ASSET_LOADED, ASSET_FETCHED, ASSET_NEEDS_FETCH, ASSET_FETCH_FAILED, ASSET_UNDEFINED };

	enum InterpolationType { IT_STEP, IT_LINEAR, IT_SPLINE };

	template<typename T>
	struct Curve
	{
		struct Key
		{
			Key() = default;
			Key(F32 time, const T& value) { mTime = time; mValue = value; }
			F32			mTime = 0;
			T			mValue;
		};

		T interp(F32 u, Key& before, Key& after)
		{
			switch (mInterpolationType)
			{
			case IT_STEP:
				return before.mValue;
			default:
			case IT_LINEAR:
			case IT_SPLINE:
				return LLKeyframeMotionLerp::lerp(u, before.mValue, after.mValue);
			}
		}

		T getValue(F32 time, F32 duration)
		{
			if (mKeys.empty())
			{
				return T();
			}

			T value;
			typename key_map_t::iterator right = std::lower_bound(mKeys.begin(), mKeys.end(), time, [](const auto& a, const auto& b) { return a.first < b; });
			if (right == mKeys.end())
			{
				// Past last key
				--right;
				value = right->second.mValue;
			}
			else if (right == mKeys.begin() || right->first == time)
			{
				// Before first key or exactly on a key
				value = right->second.mValue;
			}
			else
			{
				// Between two keys
				typename key_map_t::iterator left = right; --left;
				F32 index_before = left->first;
				F32 index_after = right->first;
				Key& pos_before = left->second;
				Key& pos_after = right->second;
				if (right == mKeys.end())
				{
					pos_after = mLoopInKey;
					index_after = duration;
				}

				F32 u = (time - index_before) / (index_after - index_before);
				value = interp(u, pos_before, pos_after);
			}
			return value;
		}

		InterpolationType	mInterpolationType = LLKeyframeMotion::IT_LINEAR;
		S32					mNumKeys = 0;
		typedef std::vector< std::pair<F32, Key> > key_map_t;
		key_map_t 			mKeys;
		Key					mLoopInKey;
		Key					mLoopOutKey;
	};

	typedef Curve<LLVector3> ScaleCurve;
	typedef ScaleCurve::Key ScaleKey;
	typedef Curve<LLQuaternion> RotationCurve;
	typedef RotationCurve::Key RotationKey;
	typedef Curve<LLVector3> PositionCurve;
	typedef PositionCurve::Key PositionKey;

	//-------------------------------------------------------------------------
	// JointMotion
	//-------------------------------------------------------------------------
	class JointMotion
	{
	public:
		PositionCurve	mPositionCurve;
		RotationCurve	mRotationCurve;
		ScaleCurve		mScaleCurve;
		std::string		mJointName;
		U32				mUsage;
		LLJoint::JointPriority	mPriority;

		void update(LLJointState* joint_state, F32 time, F32 duration);
	};
	
	//-------------------------------------------------------------------------
	// JointMotionList
	//-------------------------------------------------------------------------
	class JointMotionList : public LLRefCount
	{
	public:
		std::vector<JointMotion*> mJointMotionArray;
		F32						mDuration;
		BOOL					mLoop;
		F32						mLoopInPoint;
		F32						mLoopOutPoint;
		F32						mEaseInDuration;
		F32						mEaseOutDuration;
		LLJoint::JointPriority	mBasePriority;
		LLHandMotion::eHandPose mHandPose;
		LLJoint::JointPriority  mMaxPriority;
		typedef std::list<JointConstraintSharedData*> constraint_list_t;
		constraint_list_t		mConstraints;
		LLBBoxLocal				mPelvisBBox;
		// mEmoteName is a facial motion, but it's necessary to appear here so that it's cached.
		// TODO: LLKeyframeDataCache::getKeyframeData should probably return a class containing 
		// JointMotionList and mEmoteName, see LLKeyframeMotion::onInitialize.
		std::string				mEmoteName; 
	public:
		JointMotionList();
		~JointMotionList();
		U32 dumpDiagInfo(bool silent = false) const;
		JointMotion* getJointMotion(U32 index) const { llassert(index < mJointMotionArray.size()); return mJointMotionArray[index]; }
		U32 getNumJointMotions() const { return mJointMotionArray.size(); }
	};


	// Singu: Type of a pointer to the cached pointer (in LLKeyframeDataCache::sKeyframeDataMap) to a JointMotionList object.
	typedef AICachedPointerPtr<LLUUID, JointMotionList> JointMotionListPtr;

protected:
	static LLVFS*				sVFS;

	//-------------------------------------------------------------------------
	// Member Data
	//-------------------------------------------------------------------------
	JointMotionListPtr				mJointMotionList;			// singu: automatically clean up cache entry when destructed.
	std::vector<LLPointer<LLJointState> > mJointStates;
	LLJoint*						mPelvisp;
	LLCharacter*					mCharacter;
	typedef std::list<JointConstraint*>	constraint_list_t;
	constraint_list_t				mConstraints;
	U32								mLastSkeletonSerialNum;
	F32								mLastUpdateTime;
	F32								mLastLoopedTime;
	AssetStatus						mAssetStatus;
};

class LLKeyframeDataCache
{
private:
	friend class LLKeyframeMotion;
	LLKeyframeDataCache(){};
	~LLKeyframeDataCache();

public:
	typedef AICachedPointer<LLUUID, LLKeyframeMotion::JointMotionList>::container_type keyframe_data_map_t;	// singu: add automatic cache cleanup.
	static keyframe_data_map_t sKeyframeDataMap;

	//<singu>
	static LLKeyframeMotion::JointMotionListPtr createKeyframeData(LLUUID const& id);	// id may not exist.
	static LLKeyframeMotion::JointMotionListPtr    getKeyframeData(LLUUID const& id);	// id may or may not exists. Returns a NULL pointer when it doesn't exist.
	//</singu>

	static void removeKeyframeData(const LLUUID& id);

	//print out diagnostic info
	static void dumpDiagInfo(int quiet = 0);	// singu: added param 'quiet'.
	static void clear();
};

#endif // LL_LLKEYFRAMEMOTION_H


