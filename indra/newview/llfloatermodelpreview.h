/**
 * @file llfloatermodelpreview.h
 * @brief LLFloaterModelPreview class definition
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLFLOATERMODELPREVIEW_H
#define LL_LLFLOATERMODELPREVIEW_H

#include "llmodel.h"
#include "llquaternion.h"
#include "llthread.h"

#include "lldynamictexture.h"
#include "llfloatermodeluploadbase.h"
#include "llmeshrepository.h"
#include "llviewermenufile.h"

class LLComboBox;
class LLVOAvatar;
class LLVertexBuffer;
class LLModelPreview;
class LLFloaterModelPreview;
class daeElement;
class domProfile_COMMON;
class domInstance_geometry;
class domNode;
class domTranslate;
class domController;

typedef std::map<std::string, LLMatrix4> JointTransformMap;
typedef std::map<std::string, LLMatrix4>:: iterator JointTransformMapIt;

const S32 NUM_LOD = 4;

class LLModelLoader : public LLThread
{
public:
	typedef enum
	{
		STARTING = 0,
		READING_FILE,
		CREATING_FACES,
		GENERATING_VERTEX_BUFFERS,
		GENERATING_LOD,
		DONE,
		ERROR_PARSING //basically loading failed
	} eLoadState;

	U32 mState;
	std::string mFilename;
	S32 mLod;
	LLModelPreview* mPreview;
	LLMatrix4 mTransform;
	BOOL mFirstTransform;
	LLVector3 mExtents[2];
	bool mTrySLM;

	std::map<daeElement*, LLPointer<LLModel> > mModel;

	typedef std::vector<LLPointer<LLModel> > model_list;
	model_list mModelList;

	typedef std::vector<LLModelInstance> model_instance_list;

	typedef std::map<LLMatrix4, model_instance_list > scene;

	scene mScene;

	typedef std::queue<LLPointer<LLModel> > model_queue;

	//queue of models that need a physics rep
	model_queue mPhysicsQ;

	LLModelLoader(std::string filename, S32 lod, LLModelPreview* preview, JointTransformMap& jointMap, 
				   std::deque<std::string>& jointsFromNodes);
	~LLModelLoader() ;

	virtual void run();
	bool doLoadModel();
	bool loadFromSLM(const std::string& filename);
	void loadModelCallback();

	void loadTextures() ; //called in the main thread.
	void processElement(daeElement* element, bool& badElement);
	std::map<std::string, LLImportMaterial> getMaterials(LLModel* model, domInstance_geometry* instance_geo);
	LLImportMaterial profileToMaterial(domProfile_COMMON* material);
	std::string getElementLabel(daeElement *element);
	LLColor4 getDaeColor(daeElement* element);

	daeElement* getChildFromElement(daeElement* pElement, std::string const & name);

	bool isNodeAJoint(domNode* pNode);
	void processJointNode(domNode* pNode, std::map<std::string,LLMatrix4>& jointTransforms);
	void extractTranslation(domTranslate* pTranslate, LLMatrix4& transform);
	void extractTranslationViaElement(daeElement* pTranslateElement, LLMatrix4& transform);
	void extractTranslationViaSID(daeElement* pElement, LLMatrix4& transform);

	void setLoadState(U32 state);

	void buildJointToNodeMappingFromScene(daeElement* pRoot);
	void processJointToNodeMapping(domNode* pNode);
	void processChildJoints(domNode* pParentNode);

	//map of avatar joints as named in COLLADA assets to internal joint names
	std::map<std::string, std::string> mJointMap;
	JointTransformMap& mJointList;
	std::deque<std::string>& mJointsFromNode;

	S32 mNumOfFetchingTextures ; //updated in the main thread
	bool areTexturesReady() { return !mNumOfFetchingTextures; } //called in the main thread.

private:
	static std::list<LLModelLoader*> sActiveLoaderList;
	static bool isAlive(LLModelLoader* loader) ;
};

class LLFloaterModelPreview : public LLFloaterModelUploadBase
{
public:

	class DecompRequest : public LLPhysicsDecomp::Request
	{
	public:
		S32 mContinue;
		LLPointer<LLModel> mModel;

		DecompRequest(const std::string& stage, LLModel* mdl);
		virtual S32 statusCallback(const char* status, S32 p1, S32 p2);
		virtual void completed();

	};
	static LLFloaterModelPreview* sInstance;

	LLFloaterModelPreview(const std::string& name);
	virtual ~LLFloaterModelPreview();

	virtual BOOL postBuild();

	void initModelPreview();

	BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks); 

	/*virtual*/ void onOpen(const LLSD& key);

	static void onMouseCaptureLostModelPreview(LLMouseHandler*);
	static void setUploadAmount(S32 amount) { sUploadAmount = amount; }

	void setDetails(F32 x, F32 y, F32 z, F32 streaming_cost, F32 physics_cost);

	static void onBrowseLOD(void* user_data);

	static void onReset(void* data);

	static void onUpload(void* data);

	void refresh();

	void			loadModel(S32 lod);
	void 			loadModel(S32 lod, const std::string& file_name, bool force_disable_slm = false);

	bool isViewOptionChecked(const LLSD& userdata);
	bool isViewOptionEnabled(const LLSD& userdata);
	void setViewOptionEnabled(const std::string& option, bool enabled);
	void enableViewOption(const std::string& option);
	void disableViewOption(const std::string& option);

	// shows warning message if agent has no permissions to upload model
	/*virtual*/ void onPermissionsReceived(const LLSD& result);

	// called when error occurs during permissions request
	/*virtual*/ void setPermissonsErrorStatus(U32 status, const std::string& reason);

	/*virtual*/ void onModelPhysicsFeeReceived(const LLSD& result, std::string upload_url);
				void handleModelPhysicsFeeReceived();
	/*virtual*/ void setModelPhysicsFeeErrorStatus(U32 status, const std::string& reason);

	/*virtual*/ void onModelUploadSuccess();

	/*virtual*/ void onModelUploadFailure();

protected:
	friend class LLModelPreview;
	friend class LLMeshFilePicker;
	friend class LLPhysicsDecomp;

	static void	onImportScaleCommit(LLUICtrl* ctrl, void*);
	static void	onPelvisOffsetCommit(LLUICtrl* ctrl, void*);
	static void	onUploadJointsCommit(LLUICtrl* ctrl, void*);
	static void	onUploadSkinCommit(LLUICtrl* ctrl, void*);

	static void	onPreviewLODCommit(LLUICtrl* ctrl, void*);

	static void	onGenerateNormalsCommit(LLUICtrl* ctrl, void*);

	static void toggleGenerateNormals(LLUICtrl* ctrl, void*);

	static void	onAutoFillCommit(LLUICtrl* ctrl, void*);

	static void toggleCalculateButtonCallBack(LLUICtrl* ctrl, void* userdata);

	static void onClickCalculateBtn(void* userdata);

	static void onLoDSourceCommit(LLUICtrl* ctrl, void* userdata);

	static void onViewOptionChecked(LLUICtrl* ctrl, void* userdata);

	static void onLODParamCommit(LLUICtrl* ctrl, void* userdata);
	static void onLODParamCommitEnforceTriLimit(LLUICtrl* ctrl, void* userdata);

	static void	onExplodeCommit(LLUICtrl* ctrl, void*);

	static void onPhysicsParamCommit(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsStageExecute(LLUICtrl* ctrl, void* userdata);
	static void onCancel(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsStageCancel(LLUICtrl* ctrl, void* userdata);

	static void onPhysicsBrowse(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsUseLOD(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsOptimize(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsDecomposeBack(LLUICtrl* ctrl, void* userdata);
	static void onPhysicsSimplifyBack(LLUICtrl* ctrl, void* userdata);

	void		draw();

	void		initDecompControls();

	void		setStatusMessage(const std::string& msg);

	LLModelPreview*	mModelPreview;

	LLPhysicsDecomp::decomp_params mDecompParams;

	S32			mLastMouseX;
	S32			mLastMouseY;
	LLRect		mPreviewRect;
	U32			mGLName;
	static S32	sUploadAmount;

	std::set<LLPointer<DecompRequest> > mCurRequest;
	std::string mStatusMessage;

	//use "disabled" as false by default
	std::map<std::string, bool> mViewOptionDisabled;

	//store which lod mode each LOD is using
	// 0 - load from file
	// 1 - auto generate
	// 2 - use LoD above
	S32 mLODMode[4];

	LLMutex* mStatusLock;

	LLSD mModelPhysicsFee;

private:
	// Toggles between "Calculate weights & fee" and "Upload" buttons.
	void toggleCalculateButton(bool visible);

	// resets display options of model preview to their defaults.
	void resetDisplayOptions();

	void createSmoothComboBox(LLComboBox* combo_box, float min, float max);

	LLButton* mUploadBtn;
	LLButton* mCalculateBtn;
};

class LLMeshFilePicker : public LLFilePickerThread
{
public:
	LLMeshFilePicker(LLModelPreview* mp, S32 lod);
	virtual void notify(const std::string& filename);

private:
	LLModelPreview* mMP;
	S32 mLOD;
};

class LLModelPreview : public LLViewerDynamicTexture, public LLMutex
{
	typedef boost::signals2::signal<void (F32 x, F32 y, F32 z, F32 streaming_cost, F32 physics_cost)> details_signal_t;
	typedef boost::signals2::signal<void (void)> model_loaded_signal_t;
	typedef boost::signals2::signal<void (bool)> model_updated_signal_t;

public:
	LLModelPreview(S32 width, S32 height, LLFloater* fmp);
	virtual ~LLModelPreview();

	void resetPreviewTarget();
	void setPreviewTarget(F32 distance);
	void setTexture(U32 name) { mTextureName = name; }

	void setPhysicsFromLOD(S32 lod);
	BOOL render();
	void update();
	void genBuffers(S32 lod, bool skinned);
	void clearBuffers();
	void refresh();
	void rotate(F32 yaw_radians, F32 pitch_radians);
	void zoom(F32 zoom_amt);
	void pan(F32 right, F32 up);
	virtual BOOL needsRender() { return mNeedsUpdate; }
	void setPreviewLOD(S32 lod);
	void clearModel(S32 lod);
	void loadModel(std::string filename, S32 lod, bool force_disable_slm = false);
	void loadModelCallback(S32 lod);
	void genLODs(S32 which_lod = -1, U32 decimation = 3, bool enforce_tri_limit = false);
	void generateNormals();
	U32 calcResourceCost();
	void rebuildUploadData();
	void saveUploadData(bool save_skinweights, bool save_joint_poisitions);
	void saveUploadData(const std::string& filename, bool save_skinweights, bool save_joint_poisitions);
	void clearIncompatible(S32 lod);
	void updateStatusMessages();
	void updateLodControls(S32 lod);
	void clearGLODGroup();
	void onLODParamCommit(S32 lod, bool enforce_tri_limit);
	void addEmptyFace(LLModel* pTarget);

	const bool getModelPivot(void) const { return mHasPivot; }
	void setHasPivot(bool val) { mHasPivot = val; }
	void setModelPivot(const LLVector3& pivot) { mModelPivot = pivot; }

	//Determines the viability of an asset to be used as an avatar rig (w or w/o joint upload caps)
	void critiqueRigForUploadApplicability(const std::vector<std::string> &jointListFromAsset);
	void critiqueJointToNodeMappingFromScene(void);
	//Is a rig valid so that it can be used as a criteria for allowing for uploading of joint positions
	//Accessors for joint position upload friendly rigs
	const bool isRigValidForJointPositionUpload(void) const { return mRigValidJointUpload; }
	void setRigValidForJointPositionUpload(bool rigValid) { mRigValidJointUpload = rigValid; }
	bool isRigSuitableForJointPositionUpload(const std::vector<std::string> &jointListFromAsset);
	//Determines if a rig is a legacy from the joint list
	bool isRigLegacy(const std::vector<std::string> &jointListFromAsset);
	//Accessors for the legacy rigs
	const bool isLegacyRigValid(void) const { return mLegacyRigValid; }
	void setLegacyRigValid(bool rigValid) { mLegacyRigValid = rigValid; }
	//Verify that a controller matches vertex counts
	bool verifyController(domController* pController);

	static void	textureLoadedCallback(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* src_aux, S32 discard_level, BOOL final, void* userdata);

	boost::signals2::connection setDetailsCallback(const details_signal_t::slot_type& cb){  return mDetailsSignal.connect(cb);  }
	boost::signals2::connection setModelLoadedCallback(const model_loaded_signal_t::slot_type& cb){  return mModelLoadedSignal.connect(cb);  }
	boost::signals2::connection setModelUpdatedCallback(const model_updated_signal_t::slot_type& cb){  return mModelUpdatedSignal.connect(cb);  }

	void setLoadState(U32 state) { mLoadState = state; }
	U32 getLoadState() { return mLoadState; }
	void setRigWithSceneParity(bool state) { mRigParityWithScene = state; }
	const bool getRigWithSceneParity(void) const { return mRigParityWithScene; }

	LLVector3 getTranslationForJointOffset(std::string joint);

private:
	//Utility function for controller vertex compare
	bool verifyCount(int expected, int result);
	//Creates the dummy avatar for the preview window
	void		createPreviewAvatar(void);
	//Accessor for the dummy avatar
	LLVOAvatar* getPreviewAvatar(void) { return mPreviewAvatar; }

 protected:
	friend class LLModelLoader;
	friend class LLFloaterModelPreview;
	friend class LLFloaterModelPreview::DecompRequest;
	friend class LLPhysicsDecomp;

	LLFloater*  mFMP;

	BOOL        mNeedsUpdate;
	bool		mDirty;
	bool		mGenLOD;
	U32         mTextureName;
	F32			mCameraDistance;
	F32			mCameraYaw;
	F32			mCameraPitch;
	F32			mCameraZoom;
	LLVector3	mCameraOffset;
	LLVector3	mPreviewTarget;
	LLVector3	mPreviewScale;
	S32			mPreviewLOD;
	U32			mResourceCost;
	std::string mLODFile[LLModel::NUM_LODS];
	bool		mLoading;
	U32			mLoadState;
	bool		mResetJoints;
	bool		mRigParityWithScene;

	std::map<std::string, bool> mViewOption;

	//GLOD object parameters (must rebuild object if these change)
	bool mLODFrozen;
	F32 mBuildShareTolerance;
	U32 mBuildQueueMode;
	U32 mBuildOperator;
	U32 mBuildBorderMode;
	U32 mRequestedLoDMode[LLModel::NUM_LODS];
	S32 mRequestedTriangleCount[LLModel::NUM_LODS];
	F32 mRequestedErrorThreshold[LLModel::NUM_LODS];
	U32 mRequestedBuildOperator[LLModel::NUM_LODS];
	U32 mRequestedQueueMode[LLModel::NUM_LODS];
	U32 mRequestedBorderMode[LLModel::NUM_LODS];
	F32 mRequestedShareTolerance[LLModel::NUM_LODS];
	F32 mRequestedCreaseAngle[LLModel::NUM_LODS];

	LLModelLoader* mModelLoader;

	LLModelLoader::scene mScene[LLModel::NUM_LODS];
	LLModelLoader::scene mBaseScene;

	LLModelLoader::model_list mModel[LLModel::NUM_LODS];
	LLModelLoader::model_list mBaseModel;

	U32 mGroup;
	std::map<LLPointer<LLModel>, U32> mObject;
	U32 mMaxTriangleLimit;

	LLMeshUploadThread::instance_list mUploadData;
	std::set<LLViewerFetchedTexture* > mTextureSet;

	//map of vertex buffers to models (one vertex buffer in vector per face in model
	std::map<LLModel*, std::vector<LLPointer<LLVertexBuffer> > > mVertexBuffer[LLModel::NUM_LODS+1];

	details_signal_t mDetailsSignal;
	model_loaded_signal_t mModelLoadedSignal;
	model_updated_signal_t mModelUpdatedSignal;

	LLVector3	mModelPivot;
	bool		mHasPivot;

	float		mPelvisZOffset;

	bool		mRigValidJointUpload;
	bool		mLegacyRigValid;

	bool		mLastJointUpdate;

	std::deque<std::string> mMasterJointList;
	std::deque<std::string> mMasterLegacyJointList;
	std::deque<std::string> mJointsFromNode;
	JointTransformMap		mJointTransformMap;
	LLPointer<LLVOAvatar>	mPreviewAvatar;
};

#endif  // LL_LLFLOATERMODELPREVIEW_H
