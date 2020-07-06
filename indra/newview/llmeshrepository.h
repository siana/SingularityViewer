/** 
 * @file llmeshrepository.h
 * @brief Client-side repository of mesh assets.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_MESH_REPOSITORY_H
#define LL_MESH_REPOSITORY_H

#include "llassettype.h"
#include "llmodel.h"
#include "lluuid.h"
#include "llviewertexture.h"
#include "llvolume.h"

#define LLCONVEXDECOMPINTER_STATIC 1

#include "llconvexdecomposition.h"
#include "lluploadfloaterobservers.h"
#include "aistatemachinethread.h"

#include <absl/container/node_hash_map.h>

#ifndef BOOST_FUNCTION_HPP_INCLUDED
#include <boost/function.hpp>
#define BOOST_FUNCTION_HPP_INCLUDED
#endif

class LLVOVolume;
class LLMeshResponder;
class LLMutex;
class LLCondition;
class LLVFS;
class LLMeshRepository;
class AIMeshUpload;

class LLMeshUploadData
{
public:
	LLPointer<LLModel> mBaseModel;
	LLPointer<LLModel> mModel[5];
	LLUUID mUUID;
	U32 mRetries;
	std::string mRSVP;
	std::string mAssetData;
	LLSD mPostData;

	LLMeshUploadData()
	{
		mRetries = 0;
	}
};

class LLTextureUploadData
{
public:
	LLViewerFetchedTexture* mTexture;
	LLUUID mUUID;
	std::string mRSVP;
	std::string mLabel;
	U32 mRetries;
	std::string mAssetData;
	LLSD mPostData;

	LLTextureUploadData()
	{
		mRetries = 0;
	}

	LLTextureUploadData(LLViewerFetchedTexture* texture, std::string& label)
		: mTexture(texture), mLabel(label)
	{
		mRetries = 0;
	}
};

class LLPhysicsDecomp : public LLThread
{
public:

	typedef std::map<std::string, LLSD> decomp_params;

	class Request : public LLRefCount
	{
	public:
		//input params
		S32* mDecompID;
		std::string mStage;
		std::vector<LLVector3> mPositions;
		std::vector<U16> mIndices;
		decomp_params mParams;
				
		//output state
		std::string mStatusMessage;
		std::vector<LLModel::PhysicsMesh> mHullMesh;
		LLModel::convex_hull_decomposition mHull;
		
		//status message callback, called from decomposition thread
		virtual S32 statusCallback(const char* status, S32 p1, S32 p2) = 0;

		//completed callback, called from the main thread
		virtual void completed() = 0;

		virtual void setStatusMessage(const std::string& msg);

		bool isValid() const {return mPositions.size() > 2 && mIndices.size() > 2 ;}

	protected:
		//internal use
		LLVector3 mBBox[2] ;
		F32 mTriangleAreaThreshold ;

		void assignData(LLModel* mdl) ;
		void updateTriangleAreaThreshold() ;
		bool isValidTriangle(U16 idx1, U16 idx2, U16 idx3) ;
	};

	LLCondition* mSignal;
	LLMutex* mMutex;
	
	bool mInited;
	bool mQuitting;
	bool mDone;
	
	LLPhysicsDecomp();
	~LLPhysicsDecomp();

	void shutdown();
		
	void submitRequest(Request* request);
	static S32 llcdCallback(const char*, S32, S32);
	void cancel();

	void setMeshData(LLCDMeshData& mesh, bool vertex_based);
	void doDecomposition();
	void doDecompositionSingleHull();

	virtual void run();
	
	void completeCurrent();
	void notifyCompleted();

	std::map<std::string, S32> mStageID;

	typedef std::queue<LLPointer<Request> > request_queue;
	request_queue mRequestQ;

	LLPointer<Request> mCurRequest;

	std::queue<LLPointer<Request> > mCompletedQ;

};

class LLMeshRepoThread : public LLThread
{
public:

	static S32 sActiveHeaderRequests;
	static S32 sActiveLODRequests;
	static U32 sMaxConcurrentRequests;

	LLMutex*	mMutex;
	LLMutex*	mHeaderMutex;
	LLCondition*	mSignal;

	//map of known mesh headers
	typedef std::map<LLUUID, LLSD> mesh_header_map;
	mesh_header_map mMeshHeader;
	
	std::map<LLUUID, U32> mMeshHeaderSize;

	struct MeshRequest
	{
		LLTimer mTimer;
		LLVolumeParams mMeshParams;
		MeshRequest(const LLVolumeParams&  mesh_params) : mMeshParams(mesh_params)
		{
			mTimer.start();
		}
		virtual ~MeshRequest() {}
		virtual void preFetch() {}
		virtual bool fetch(U32& count) = 0;
	};
	class HeaderRequest : public MeshRequest
	{ 
	public:
		HeaderRequest(const LLVolumeParams&  mesh_params)
			: MeshRequest(mesh_params)
		{}
		bool fetch(U32& count);
		bool operator<(const HeaderRequest& rhs) const
		{
			return mMeshParams < rhs.mMeshParams;
		}
	};

	class LODRequest : public MeshRequest
	{
	public:
		S32 mLOD;
		F32 mScore;

		LODRequest(const LLVolumeParams&  mesh_params, S32 lod)
			: MeshRequest(mesh_params), mLOD(lod), mScore(0.f)
		{}
		void preFetch();
		bool fetch(U32& count);
	};

	struct CompareScoreGreater
	{
		bool operator()(const LODRequest& lhs, const LODRequest& rhs)
		{
			return lhs.mScore > rhs.mScore; // greatest = first
		}
	};
	

	class LoadedMesh
	{
	public:
		LLPointer<LLVolume> mVolume;
		LLVolumeParams mMeshParams;
		S32 mLOD;

		LoadedMesh(LLVolume* volume, const LLVolumeParams&  mesh_params, S32 lod)
			: mVolume(volume), mMeshParams(mesh_params), mLOD(lod)
		{
		}

	};

	struct MeshHeaderInfo
	{
		MeshHeaderInfo()
			: mHeaderSize(0), mVersion(0), mOffset(-1), mSize(0) {}
		U32 mHeaderSize;
		U32 mVersion;
		S32 mOffset;
		S32 mSize;
	};

	//set of requested skin info
	uuid_set_t mSkinRequests;
	
	//queue of completed skin info requests
	std::queue<LLMeshSkinInfo> mSkinInfoQ;
	LLMutex* mSkinInfoQMutex;

	//set of requested decompositions
	uuid_set_t mDecompositionRequests;

	//set of requested physics shapes
	uuid_set_t mPhysicsShapeRequests;

	//queue of completed Decomposition info requests
	std::queue<LLModel::Decomposition*> mDecompositionQ;
	LLMutex* mDecompositionQMutex;

	//queue of requested headers
	std::deque<std::pair<std::shared_ptr<MeshRequest>, F32> > mHeaderReqQ;

	//queue of requested LODs
	std::deque<std::pair<std::shared_ptr<MeshRequest>, F32> > mLODReqQ;

	//queue of unavailable LODs (either asset doesn't exist or asset doesn't have desired LOD)
	std::queue<LODRequest> mUnavailableQ;

	//queue of successfully loaded meshes
	std::queue<LoadedMesh> mLoadedQ;

	//map of pending header requests and currently desired LODs
	typedef std::map<LLVolumeParams, std::vector<S32> > pending_lod_map;
	pending_lod_map mPendingLOD;

	static std::string constructUrl(LLUUID mesh_id);

	LLMeshRepoThread();
	~LLMeshRepoThread();

	void runQueue(std::deque<std::pair<std::shared_ptr<MeshRequest>, F32> >& queue, U32& count, S32& active_requests);
	void runSet(uuid_set_t& set, std::function<bool (const LLUUID& mesh_id)> fn);
	void pushHeaderRequest(const LLVolumeParams& mesh_params, F32 delay = 0)
	{
		std::shared_ptr<LLMeshRepoThread::MeshRequest> req;
		req.reset(new LLMeshRepoThread::HeaderRequest(mesh_params));
		mHeaderReqQ.push_back(std::make_pair(req, delay));
	}
	void pushLODRequest(const LLVolumeParams& mesh_params, S32 lod, F32 delay = 0)
	{
		std::shared_ptr<LLMeshRepoThread::MeshRequest> req;
		req.reset(new LLMeshRepoThread::LODRequest(mesh_params, lod));
		mLODReqQ.push_back(std::make_pair(req, delay));
	}
	virtual void run();

	void lockAndLoadMeshLOD(const LLVolumeParams& mesh_params, S32 lod);
	void loadMeshLOD(const LLVolumeParams& mesh_params, S32 lod);
	bool fetchMeshHeader(const LLVolumeParams& mesh_params, U32& count);
	bool fetchMeshLOD(const LLVolumeParams& mesh_params, S32 lod, U32& count);
	bool headerReceived(const LLVolumeParams& mesh_params, U8* data, S32 data_size);
	bool lodReceived(const LLVolumeParams& mesh_params, S32 lod, U8* data, S32 data_size);
	bool skinInfoReceived(const LLUUID& mesh_id, U8* data, S32 data_size);
	bool decompositionReceived(const LLUUID& mesh_id, U8* data, S32 data_size);
	bool physicsShapeReceived(const LLUUID& mesh_id, U8* data, S32 data_size);
	LLSD& getMeshHeader(const LLUUID& mesh_id);

	bool getMeshHeaderInfo(const LLUUID& mesh_id, const char* block_name, MeshHeaderInfo& info);
	bool loadInfoFromVFS(const LLUUID& mesh_id, MeshHeaderInfo& info, boost::function<bool(const LLUUID&, U8*, S32)> fn);

	void notifyLoadedMeshes();
	S32 getActualMeshLOD(const LLVolumeParams& mesh_params, S32 lod);
	
	void loadMeshSkinInfo(const LLUUID& mesh_id);
	void loadMeshDecomposition(const LLUUID& mesh_id);
	void loadMeshPhysicsShape(const LLUUID& mesh_id);

	//send request for skin info, returns true if header info exists 
	//  (should hold onto mesh_id and try again later if header info does not exist)
	bool fetchMeshSkinInfo(const LLUUID& mesh_id);

	//send request for decomposition, returns true if header info exists 
	//  (should hold onto mesh_id and try again later if header info does not exist)
	bool fetchMeshDecomposition(const LLUUID& mesh_id);

	//send request for PhysicsShape, returns true if header info exists 
	//  (should hold onto mesh_id and try again later if header info does not exist)
	bool fetchMeshPhysicsShape(const LLUUID& mesh_id);

	static void incActiveLODRequests();
	static void decActiveLODRequests();
	static void incActiveHeaderRequests();
	static void decActiveHeaderRequests();

};

class LLMeshUploadThread : public AIThreadImpl
{
private:
	S32 mMeshUploadTimeOut ; //maximum time in seconds to execute an uploading request.

public:
	class DecompRequest : public LLPhysicsDecomp::Request
	{
	public:
		LLPointer<LLModel> mModel;
		LLPointer<LLModel> mBaseModel;

		LLMeshUploadThread* mThread;

		DecompRequest(LLModel* mdl, LLModel* base_model, LLMeshUploadThread* thread);

		S32 statusCallback(const char* status, S32 p1, S32 p2) { return 1; }
		void completed();
	};

	LLPointer<DecompRequest> mFinalDecomp;
	bool mPhysicsComplete;
	LLSD mModelData;
	LLSD mBody;

	typedef std::map<LLPointer<LLModel>, std::vector<LLVector3> > hull_map;
	hull_map mHullMap;

	typedef std::vector<LLModelInstance> instance_list;
	instance_list mInstanceList;

	typedef std::map<LLPointer<LLModel>, instance_list> instance_map;
	instance_map mInstance;

	LLVector3		mOrigin;
	bool			mUploadTextures;
	bool			mUploadSkin;
	bool			mUploadJoints;

	LLHost			mHost;
	std::string		mWholeModelFeeCapability;

#ifdef LL_DEBUG
	LLMeshUploadThread(void) : AIThreadImpl("mesh upload") { }
#endif
	void init(instance_list& data, LLVector3& scale, bool upload_textures, bool upload_skin, bool upload_joints, bool do_upload,
		LLHandle<LLWholeModelFeeObserver> const& fee_observer, LLHandle<LLWholeModelUploadObserver> const& upload_observer);
	~LLMeshUploadThread();

	void postRequest(std::string& url, AIMeshUpload* state_machine);

	virtual bool run();
	void preStart();

	void generateHulls();

	void wholeModelToLLSD(LLSD& dest, bool include_textures);

	void decomposeMeshMatrix(LLMatrix4& transformation,
							 LLVector3& result_pos,
							 LLQuaternion& result_rot,
							 LLVector3& result_scale);

	void setFeeObserverHandle(LLHandle<LLWholeModelFeeObserver> observer_handle) { mFeeObserverHandle = observer_handle; }
	void setUploadObserverHandle(LLHandle<LLWholeModelUploadObserver> observer_handle) { mUploadObserverHandle = observer_handle; }

	LLViewerFetchedTexture* FindViewerTexture(const LLImportMaterial& material);
private:
	LLHandle<LLWholeModelFeeObserver> mFeeObserverHandle;
	LLHandle<LLWholeModelUploadObserver> mUploadObserverHandle;

	bool mDoUpload; // if FALSE only model data will be requested, otherwise the model will be uploaded
};

enum AIMeshUpload_state_type {
	AIMeshUpload_start = AIStateMachine::max_state,
	AIMeshUpload_threadFinished,
	AIMeshUpload_responderFinished
};

class AIMeshUpload : public AIStateMachine
{
private:
	LLPointer<AIStateMachineThread<LLMeshUploadThread> > mMeshUpload;
	std::string mWholeModelUploadURL;

public:
	AIMeshUpload(LLMeshUploadThread::instance_list& data, LLVector3& scale,
		bool upload_textures, bool upload_skin, bool upload_joints, std::string const& upload_url, bool do_upload,
		LLHandle<LLWholeModelFeeObserver> const& fee_observer, LLHandle<LLWholeModelUploadObserver> const& upload_observer);

	void setWholeModelUploadURL(std::string const& whole_model_upload_url) { mWholeModelUploadURL = whole_model_upload_url; }

	/*virtual*/ const char* getName() const { return "AIMeshUpload"; }

protected:
	// Implement AIStateMachine.
	/*virtual*/ const char* state_str_impl(state_type run_state) const;
	/*virtual*/ void initialize_impl();
	/*virtual*/ void multiplex_impl(state_type run_state);
};

// Params related to streaming cost, render cost, and scene complexity tracking.
class LLMeshCostData
{
public:
    LLMeshCostData();

    bool init(const LLSD& header);
    
    // Size for given LOD
    S32 getSizeByLOD(S32 lod);

    // Sum of all LOD sizes.
    S32 getSizeTotal();

    // Estimated triangle counts for the given LOD.
    F32 getEstTrisByLOD(S32 lod);
    
    // Estimated triangle counts for the largest LOD. Typically this
    // is also the "high" LOD, but not necessarily.
    F32 getEstTrisMax();

    // Triangle count as computed by original streaming cost
    // formula. Triangles in each LOD are weighted based on how
    // frequently they will be seen.
    // This was called "unscaled_value" in the original getStreamingCost() functions.
    F32 getRadiusWeightedTris(F32 radius);

    // Triangle count used by triangle-based cost formula. Based on
    // triangles in highest LOD plus potentially partial charges for
    // lower LODs depending on complexity.
    F32 getEstTrisForStreamingCost();

    // Streaming cost. This should match the server-side calculation
    // for the corresponding volume.
    F32 getRadiusBasedStreamingCost(F32 radius);

    // New streaming cost formula, currently only used for animated objects.
    F32 getTriangleBasedStreamingCost();

private:
    // From the "size" field of the mesh header. LOD 0=lowest, 3=highest.
    std::vector<S32> mSizeByLOD;

    // Estimated triangle counts derived from the LOD sizes. LOD 0=lowest, 3=highest.
    std::vector<F32> mEstTrisByLOD;
};

class LLMeshRepository
{
public:

	//metrics
	static U32 sBytesReceived;
	static U32 sHTTPRequestCount;
	static U32 sHTTPRetryCount;
	static U32 sLODPending;
	static U32 sLODProcessing;
	static U32 sCacheBytesRead;
	static U32 sCacheBytesWritten;
	static U32 sPeakKbps;
	
	// Estimated triangle count of the largest LOD
	F32 getEstTrianglesMax(LLUUID mesh_id);
	F32 getEstTrianglesStreamingCost(LLUUID mesh_id);
	F32 getStreamingCostLegacy(LLUUID mesh_id, F32 radius, S32* bytes = NULL, S32* visible_bytes = NULL, S32 detail = -1, F32 *unscaled_value = NULL);
	static F32 getStreamingCostLegacy(LLSD& header, F32 radius, S32* bytes = NULL, S32* visible_bytes = NULL, S32 detail = -1, F32 *unscaled_value = NULL);
	bool getCostData(LLUUID mesh_id, LLMeshCostData& data);
	static bool getCostData(LLSD& header, LLMeshCostData& data);

	LLMeshRepository();

	void init();
	void shutdown();

	void unregisterMesh(LLVOVolume* volume);
	//mesh management functions
	S32 loadMesh(LLVOVolume* volume, const LLVolumeParams& mesh_params, S32 detail = 0, S32 last_lod = -1);
	
	void notifyLoadedMeshes();
	void notifyMeshLoaded(const LLVolumeParams& mesh_params, LLVolume* volume);
	void notifyMeshUnavailable(const LLVolumeParams& mesh_params, S32 lod);
	void notifySkinInfoReceived(LLMeshSkinInfo& info);
	void notifyDecompositionReceived(LLModel::Decomposition* info);

	S32 getActualMeshLOD(const LLVolumeParams& mesh_params, S32 lod);
	static S32 getActualMeshLOD(LLSD& header, S32 lod);
	const LLMeshSkinInfo* getSkinInfo(const LLUUID& mesh_id, const LLVOVolume* requesting_obj);
	LLModel::Decomposition* getDecomposition(const LLUUID& mesh_id);
	void fetchPhysicsShape(const LLUUID& mesh_id);
	bool hasPhysicsShape(const LLUUID& mesh_id);
	
	void buildHull(const LLVolumeParams& params, S32 detail);
	void buildPhysicsMesh(LLModel::Decomposition& decomp);
	
	bool meshUploadEnabled();
	bool meshRezEnabled();
	

	LLSD& getMeshHeader(const LLUUID& mesh_id);

	void uploadModel(std::vector<LLModelInstance>& data, LLVector3& scale, bool upload_textures,
					 bool upload_skin, bool upload_joints, std::string upload_url, bool do_upload = true,
					 LLHandle<LLWholeModelFeeObserver> fee_observer= (LLHandle<LLWholeModelFeeObserver>()), LLHandle<LLWholeModelUploadObserver> upload_observer = (LLHandle<LLWholeModelUploadObserver>()));

	S32 getMeshSize(const LLUUID& mesh_id, S32 lod);

	typedef std::map<LLVolumeParams, std::vector<LLVOVolume*> > mesh_load_map;
	mesh_load_map mLoadingMeshes[4];

	typedef absl::node_hash_map<LLUUID, LLMeshSkinInfo> skin_map;
	skin_map mSkinMap;

	typedef std::map<LLUUID, LLModel::Decomposition*> decomposition_map;
	decomposition_map mDecompositionMap;

	LLMutex*					mMeshMutex;
	
	std::vector<LLMeshRepoThread::LODRequest> mPendingRequests;
	
	//list of mesh ids awaiting skin info
	typedef std::map<LLUUID, uuid_set_t > skin_load_map;
	skin_load_map mLoadingSkins;

	//list of mesh ids that need to send skin info fetch requests
	std::queue<LLUUID> mPendingSkinRequests;

	//list of mesh ids awaiting decompositions
	uuid_set_t mLoadingDecompositions;

	//list of mesh ids that need to send decomposition fetch requests
	std::queue<LLUUID> mPendingDecompositionRequests;
	
	//list of mesh ids awaiting physics shapes
	uuid_set_t mLoadingPhysicsShapes;

	//list of mesh ids that need to send physics shape fetch requests
	std::queue<LLUUID> mPendingPhysicsShapeRequests;
	
	U32 mMeshThreadCount;

	void cacheOutgoingMesh(LLMeshUploadData& data, LLSD& header);

	LLMeshRepoThread* mThread;

	LLPhysicsDecomp* mDecompThread;
	
	class inventory_data
	{
	public:
		LLSD mPostData;
		LLSD mResponse;

		inventory_data(const LLSD& data, const LLSD& content)
			: mPostData(data), mResponse(content)
		{
		}
	};

	std::queue<inventory_data> mInventoryQ;

	std::queue<LLSD> mUploadErrorQ;

	void uploadError(LLSD& args);
	void updateInventory(inventory_data data);

	std::string mGetMeshCapability;
};

extern LLMeshRepository gMeshRepo;

const F32 ANIMATED_OBJECT_BASE_COST = 15.0f;
const F32 ANIMATED_OBJECT_COST_PER_KTRI = 1.5f;

#endif
