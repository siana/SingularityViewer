/** 
 * @file pipeline.h
 * @brief Rendering pipeline definitions
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

#ifndef LL_PIPELINE_H
#define LL_PIPELINE_H

#include "llcamera.h"
#include "llerror.h"
#include "lldrawpool.h"
#include "llspatialpartition.h"
#include "m4math.h"
#include "llpointer.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolmaterials.h"
#include "llgl.h"
#include "lldrawable.h"
#include "llrendertarget.h"
#include "llfasttimer.h"

#include <stack>

#include <stack>

#include <stack>

class LLViewerTexture;
class LLEdge;
class LLFace;
class LLViewerObject;
class LLAgent;
class LLDisplayPrimitive;
class LLTextureEntry;
class LLRenderFunc;
class LLCubeMap;
class LLCullResult;
class LLVOAvatar;
class LLVOPartGroup;
class LLGLSLShader;
class LLDrawPoolAlpha;

class LLMeshResponder;

typedef enum e_avatar_skinning_method
{
	SKIN_METHOD_SOFTWARE,
	SKIN_METHOD_VERTEX_PROGRAM
} EAvatarSkinningMethod;

BOOL compute_min_max(LLMatrix4& box, LLVector2& min, LLVector2& max); // Shouldn't be defined here!
bool LLRayAABB(const LLVector3 &center, const LLVector3 &size, const LLVector3& origin, const LLVector3& dir, LLVector3 &coord, F32 epsilon = 0);
BOOL setup_hud_matrices(); // use whole screen to render hud
BOOL setup_hud_matrices(const LLRect& screen_region); // specify portion of screen (in pixels) to render hud attachments from (for picking)
const LLMatrix4a& glh_get_current_modelview();
void glh_set_current_modelview(const LLMatrix4a& mat);
const LLMatrix4a& glh_get_current_projection();
void glh_set_current_projection(const LLMatrix4a& mat);

extern LLTrace::BlockTimerStatHandle FTM_RENDER_GEOMETRY;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_GRASS;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_OCCLUSION;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_SHINY;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_SIMPLE;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_TERRAIN;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_TREES;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_UI;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_WATER;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_WL_SKY;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_ALPHA;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_CHARACTERS;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_BUMP;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_MATERIALS;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_FULLBRIGHT;
extern LLTrace::BlockTimerStatHandle FTM_RENDER_GLOW;
extern LLTrace::BlockTimerStatHandle FTM_STATESORT;
extern LLTrace::BlockTimerStatHandle FTM_PIPELINE;
extern LLTrace::BlockTimerStatHandle FTM_CLIENT_COPY;


LL_ALIGN_PREFIX(16)
class LLPipeline
{
public:
	LLPipeline();
	~LLPipeline();

	void destroyGL();
	void restoreGL();
	void resetVertexBuffers();
	void doResetVertexBuffers(bool forced = false);
	void resizeScreenTexture();
	void releaseOcclusionBuffers();
	void releaseVertexBuffers();
	void releaseGLBuffers();
	void releaseLUTBuffers();
	void releaseScreenBuffers();
	void createGLBuffers();
	void createLUTBuffers();

	//allocate the largest screen buffer possible up to resX, resY
	//returns true if full size buffer allocated, false if some other size is allocated
	bool allocateScreenBuffer(U32 resX, U32 resY);

	typedef enum {
		FBO_SUCCESS_FULLRES = 0,
		FBO_SUCCESS_LOWRES,
		FBO_FAILURE
	} eFBOStatus;

private:
	//implementation of above, wrapped for easy error handling
	eFBOStatus doAllocateScreenBuffer(U32 resX, U32 resY);
public:

	//attempt to allocate screen buffers at resX, resY
	//returns true if allocation successful, false otherwise
	bool allocateScreenBuffer(U32 resX, U32 resY, U32 samples);

	void allocatePhysicsBuffer();
	
	void resetVertexBuffers(LLDrawable* drawable);
	void generateImpostor(LLVOAvatar* avatar);
	void bindScreenToTexture();
	void renderBloom(BOOL for_snapshot, F32 zoom_factor = 1.f, int subfield = 0, bool tiling = false);

	void init();
	void cleanup();
	BOOL isInit() { return mInitialized; };

	/// @brief Get a draw pool from pool type (POOL_SIMPLE, POOL_MEDIA) and texture.
	/// @return Draw pool, or NULL if not found.
	LLDrawPool *findPool(const U32 pool_type, LLViewerTexture *tex0 = NULL);

	/// @brief Get a draw pool for faces of the appropriate type and texture.  Create if necessary.
	/// @return Always returns a draw pool.
	LLDrawPool *getPool(const U32 pool_type, LLViewerTexture *tex0 = NULL);

	/// @brief Figures out draw pool type from texture entry. Creates pool if necessary.
	static LLDrawPool* getPoolFromTE(const LLTextureEntry* te, LLViewerTexture* te_image);
	static U32 getPoolTypeFromTE(const LLTextureEntry* te, LLViewerTexture* imagep);

	void		 addPool(LLDrawPool *poolp);	// Only to be used by LLDrawPool classes for splitting pools!
	void		 removePool( LLDrawPool* poolp );

	void		 allocDrawable(LLViewerObject *obj);

	void		 unlinkDrawable(LLDrawable*);

	static void removeMutedAVsLights(LLVOAvatar*);

	// Object related methods
	void        markVisible(LLDrawable *drawablep, LLCamera& camera);
	void		markOccluder(LLSpatialGroup* group);

	//downsample source to dest, taking the maximum depth value per pixel in source and writing to dest
	// if source's depth buffer cannot be bound for reading, a scratch space depth buffer must be provided
	void		downsampleDepthBuffer(LLRenderTarget& source, LLRenderTarget& dest, LLRenderTarget* scratch_space = NULL);

	void		doOcclusion(LLCamera& camera, LLRenderTarget& source, LLRenderTarget& dest, LLRenderTarget* scratch_space = NULL);
	void		doOcclusion(LLCamera& camera);
	void		markNotCulled(LLSpatialGroup* group, LLCamera &camera);
	void        markMoved(LLDrawable *drawablep, BOOL damped_motion = FALSE);
	void        markShift(LLDrawable *drawablep);
	void        markTextured(LLDrawable *drawablep);
	void		markGLRebuild(LLGLUpdate* glu);
	void		markRebuild(LLSpatialGroup* group, BOOL priority = FALSE);
	void        markRebuild(LLDrawable *drawablep, LLDrawable::EDrawableFlags flag = LLDrawable::REBUILD_ALL, BOOL priority = FALSE);
	void		markPartitionMove(LLDrawable* drawablep);
	void		markMeshDirty(LLSpatialGroup* group);

	//get the object between start and end that's closest to start.
	LLViewerObject* lineSegmentIntersectInWorld(const LLVector4a& start, const LLVector4a& end,
												BOOL pick_transparent,
												bool pick_rigged,
												S32* face_hit,                          // return the face hit
												LLVector4a* intersection = NULL,         // return the intersection point
												LLVector2* tex_coord = NULL,            // return the texture coordinates of the intersection point
												LLVector4a* normal = NULL,               // return the surface normal at the intersection point
												LLVector4a* tangent = NULL             // return the surface tangent at the intersection point  
		);

	//get the closest particle to start between start and end, returns the LLVOPartGroup and particle index
	LLVOPartGroup* lineSegmentIntersectParticle(const LLVector4a& start, const LLVector4a& end, LLVector4a* intersection,
														S32* face_hit);


	LLViewerObject* lineSegmentIntersectInHUD(const LLVector4a& start, const LLVector4a& end,
											  BOOL pick_transparent,
											  S32* face_hit,                          // return the face hit
											  LLVector4a* intersection = NULL,         // return the intersection point
											  LLVector2* tex_coord = NULL,            // return the texture coordinates of the intersection point
											  LLVector4a* normal = NULL,               // return the surface normal at the intersection point
											  LLVector4a* tangent = NULL             // return the surface tangent at the intersection point
		);

	// Something about these textures has changed.  Dirty them.
	void        dirtyPoolObjectTextures(const std::set<LLViewerFetchedTexture*>& textures);

	void        resetDrawOrders();

	U32         addObject(LLViewerObject *obj);

	void		enableShadows(const BOOL enable_shadows);

	void		updateLocalLightingEnabled();
	bool		isLocalLightingEnabled() const { return mLightingEnabled; }
		
	BOOL		canUseWindLightShaders() const;
	BOOL		canUseWindLightShadersOnObjects() const;
	BOOL		canUseAntiAliasing() const;

	// phases
	void resetFrameStats();

	void updateMoveDampedAsync(LLDrawable* drawablep);
	void updateMoveNormalAsync(LLDrawable* drawablep);
	void updateMovedList(LLDrawable::drawable_vector_t& move_list);
	void updateMove();
	BOOL visibleObjectsInFrustum(LLCamera& camera);
	BOOL getVisibleExtents(LLCamera& camera, LLVector3 &min, LLVector3& max);
	BOOL getVisiblePointCloud(LLCamera& camera, LLVector3 &min, LLVector3& max, std::vector<LLVector3>& fp, LLVector3 light_dir = LLVector3(0,0,0));
	void updateCull(LLCamera& camera, LLCullResult& result, S32 water_clip = 0, LLPlane* plane = NULL);  //if water_clip is 0, ignore water plane, 1, cull to above plane, -1, cull to below plane
	void createObjects(F32 max_dtime);
	void createObject(LLViewerObject* vobj);
	void processPartitionQ();
	void updateGeom(F32 max_dtime, LLCamera& camera);
	void updateGL();
	void rebuildPriorityGroups();
	void rebuildGroups();
	void clearRebuildGroups();
	void clearRebuildDrawables();

	//calculate pixel area of given box from vantage point of given camera
	static F32 calcPixelArea(LLVector3 center, LLVector3 size, LLCamera& camera);
	static F32 calcPixelArea(const LLVector4a& center, const LLVector4a& size, LLCamera &camera);

	void stateSort(LLCamera& camera, LLCullResult& result);
	void stateSort(LLSpatialGroup* group, LLCamera& camera);
	void stateSort(LLSpatialBridge* bridge, LLCamera& camera);
	void stateSort(LLDrawable* drawablep, LLCamera& camera);
	void postSort(LLCamera& camera);
	void forAllVisibleDrawables(void (*func)(LLDrawable*));

	void renderObjects(U32 type, U32 mask, BOOL texture = TRUE, BOOL batch_texture = FALSE);
	void renderMaskedObjects(U32 type, U32 mask, BOOL texture = TRUE, BOOL batch_texture = FALSE);
	void renderGroups(LLRenderPass* pass, U32 type, U32 mask, BOOL texture);

	void grabReferences(LLCullResult& result);
	void clearReferences();

	//check references will assert that there are no references in sCullResult to the provided data
	void checkReferences(LLFace* face);
	void checkReferences(LLDrawable* drawable);
	void checkReferences(LLDrawInfo* draw_info);
	void checkReferences(LLSpatialGroup* group);


	void renderGeom(LLCamera& camera, BOOL forceVBOUpdate = FALSE);
	void renderGeomDeferred(LLCamera& camera);
	void renderGeomPostDeferred(LLCamera& camera, bool do_occlusion=true);
	void renderGeomShadow(LLCamera& camera);
	void bindDeferredShader(LLGLSLShader& shader, LLRenderTarget* diffuse_source = NULL, LLRenderTarget* light_source = NULL);
	void setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep);

	void unbindDeferredShader(LLGLSLShader& shader, LLRenderTarget* diffuse_source = NULL, LLRenderTarget* light_source = NULL);
	void renderDeferredLighting();
	void renderDeferredLightingToRT(LLRenderTarget* target);
	
	void generateWaterReflection(LLCamera& camera);
	void generateSunShadow(LLCamera& camera);


	void renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj, LLCamera& camera, LLCullResult& result, BOOL use_shader, BOOL use_occlusion, U32 target_width);
	void renderHighlights();
	void renderDebug();
	void renderPhysicsDisplay();

	void rebuildPools(); // Rebuild pools

	void findReferences(LLDrawable *drawablep);	// Find the lists which have references to this object
	BOOL verify();						// Verify that all data in the pipeline is "correct"

	S32  getLightCount() const { return mLights.size(); }

	void resetLocalLights();		//Default all gl light parameters. Used upon restoreGL. Fixes light brightness problems on fullscren toggle
	void calcNearbyLights(LLCamera& camera);
	void gatherLocalLights();
	void setupHWLights();
	void updateHWLightMode(U8 mode);
	U8 setupFeatureLights(U8 cur_count);
	void enableLights(U32 mask, LLGLState<GL_LIGHTING>& light_state, const LLColor4* color = nullptr);
	void enableLightsStatic(LLGLState<GL_LIGHTING>& light_state);
	void enableLightsDynamic(LLGLState<GL_LIGHTING>& light_state);
	void enableLightsAvatar(LLGLState<GL_LIGHTING>& light_state);
	void enableLightsPreview(LLGLState<GL_LIGHTING>& light_state);
	void enableLightsAvatarEdit(LLGLState<GL_LIGHTING>& light_state, const LLColor4& color);
	void enableLightsFullbright(LLGLState<GL_LIGHTING>& light_state);
	void disableLights(LLGLState<GL_LIGHTING>& light_state);

	void shiftObjects(const LLVector3 &offset);

	void setLight(LLDrawable *drawablep, BOOL is_light);
	
	BOOL hasRenderBatches(const U32 type) const;
	LLCullResult::drawinfo_iterator beginRenderMap(U32 type) const;
	LLCullResult::drawinfo_iterator endRenderMap(U32 type) const;
	LLCullResult::sg_iterator beginAlphaGroups() const;
	LLCullResult::sg_iterator endAlphaGroups() const;
	

	void addTrianglesDrawn(S32 index_count, U32 render_type = LLRender::TRIANGLES);

	BOOL hasRenderDebugFeatureMask(const U32 mask) const	{ return (mRenderDebugFeatureMask & mask) ? TRUE : FALSE; }
	BOOL hasRenderDebugMask(const U32 mask) const			{ return (mRenderDebugMask & mask) ? TRUE : FALSE; }
	void setAllRenderDebugFeatures() { mRenderDebugFeatureMask = 0xffffffff; }
	void clearAllRenderDebugFeatures() { mRenderDebugFeatureMask = 0x0; }
	void setAllRenderDebugDisplays() { mRenderDebugMask = 0xffffffff; }
	void clearAllRenderDebugDisplays() { mRenderDebugMask = 0x0; }

	BOOL hasRenderType(const U32 type) const;
	BOOL hasAnyRenderType(const U32 type, ...) const;

	void setRenderTypeMask(U32 type, ...);
	// This is equivalent to 'setRenderTypeMask'
	//void orRenderTypeMask(U32 type, ...);
	void andRenderTypeMask(U32 type, ...);
	void clearRenderTypeMask(U32 type, ...);
	void setAllRenderTypes();
	void clearAllRenderTypes();
	
	void pushRenderTypeMask();
	void popRenderTypeMask();

	void pushRenderDebugFeatureMask();
	void popRenderDebugFeatureMask();

	template <LLGLenum T>
	LLGLStateIface* pushRenderPassState(U8 newState = LLGLStateIface::CURRENT_STATE) {
		llassert_always(mInRenderPass);
		LLGLStateIface* stateObject = new LLGLState<T>(newState);
		mRenderPassStates.emplace_back(stateObject);
		return stateObject;
	}

	static void toggleRenderType(U32 type);

	// For UI control of render features
	static BOOL hasRenderTypeControl(void* data);
	static void toggleRenderDebug(void* data);
	static void toggleRenderDebugFeature(void* data);
	static void toggleRenderTypeControl(void* data);
	static BOOL toggleRenderTypeControlNegated(void* data);
	static BOOL hasRenderPairedTypeControl(void* data);
	static void toggleRenderPairedTypeControl(void* data);
	static BOOL toggleRenderDebugControl(void* data);
	static BOOL toggleRenderDebugFeatureControl(void* data);
	static void setRenderDebugFeatureControl(U32 bit, bool value);

	static void setRenderParticleBeacons(BOOL val);
	static void toggleRenderParticleBeacons(void* data);
	static BOOL getRenderParticleBeacons(void* data);

	static void setRenderSoundBeacons(BOOL val);
	static void toggleRenderSoundBeacons(void* data);
	static BOOL getRenderSoundBeacons(void* data);

	static void setRenderMOAPBeacons(BOOL val);
	static void toggleRenderMOAPBeacons(void * data);
	static BOOL getRenderMOAPBeacons(void * data);

	static void setRenderPhysicalBeacons(BOOL val);
	static void toggleRenderPhysicalBeacons(void* data);
	static BOOL getRenderPhysicalBeacons(void* data);

	static void setRenderScriptedBeacons(BOOL val);
	static void toggleRenderScriptedBeacons(void* data);
	static BOOL getRenderScriptedBeacons(void* data);

	static void setRenderScriptedTouchBeacons(BOOL val);
	static void toggleRenderScriptedTouchBeacons(void* data);
	static BOOL getRenderScriptedTouchBeacons(void* data);

	static void setRenderBeacons(BOOL val);
	static void toggleRenderBeacons(void* data);
	static BOOL getRenderBeacons(void* data);

	static void setRenderHighlights(BOOL val);
	static void toggleRenderHighlights(void* data);
	static BOOL getRenderHighlights(void* data);
	static void setRenderHighlightTextureChannel(LLRender::eTexIndex channel); // sets which UV setup to display in highlight overlay

	static bool isRenderDeferredCapable();
	static bool isRenderDeferredDesired();
	static void updateRenderDeferred();
	static void refreshCachedSettings();

	static void throttleNewMemoryAllocation(BOOL disable);

	void addDebugBlip(const LLVector3& position, const LLColor4& color);

	void hideObject( const LLUUID& id );
	void restoreHiddenObject( const LLUUID& id );

private:
	void unloadShaders();
	void addToQuickLookup( LLDrawPool* new_poolp );
	void removeFromQuickLookup( LLDrawPool* poolp );
	BOOL updateDrawableGeom(LLDrawable* drawable, BOOL priority);
	void assertInitializedDoError();
	bool assertInitialized() { const bool is_init = isInit(); if (!is_init) assertInitializedDoError(); return is_init; };
	void hideDrawable( LLDrawable *pDrawable );
	void unhideDrawable( LLDrawable *pDrawable );

	void drawFullScreenRect();

	void clearRenderPassStates() {
		while (!mRenderPassStates.empty()) {
			mRenderPassStates.pop_back();
		}
		mInRenderPass = false;
	}
public:
	enum {GPU_CLASS_MAX = 3 };

	enum LLRenderTypeMask
	{
		// Following are pool types (some are also object types)
		RENDER_TYPE_SKY			= LLDrawPool::POOL_SKY,
		RENDER_TYPE_WL_SKY		= LLDrawPool::POOL_WL_SKY,
		RENDER_TYPE_GROUND		= LLDrawPool::POOL_GROUND,	
		RENDER_TYPE_TERRAIN		= LLDrawPool::POOL_TERRAIN,
		RENDER_TYPE_SIMPLE						= LLDrawPool::POOL_SIMPLE,
		RENDER_TYPE_GRASS						= LLDrawPool::POOL_GRASS,
		RENDER_TYPE_ALPHA_MASK					= LLDrawPool::POOL_ALPHA_MASK,
		RENDER_TYPE_FULLBRIGHT_ALPHA_MASK		= LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK,
		RENDER_TYPE_FULLBRIGHT					= LLDrawPool::POOL_FULLBRIGHT,
		RENDER_TYPE_BUMP						= LLDrawPool::POOL_BUMP,
		RENDER_TYPE_MATERIALS					= LLDrawPool::POOL_MATERIALS,
		RENDER_TYPE_AVATAR						= LLDrawPool::POOL_AVATAR,
		RENDER_TYPE_TREE		= LLDrawPool::POOL_TREE,
		RENDER_TYPE_VOIDWATER	= LLDrawPool::POOL_VOIDWATER,
		RENDER_TYPE_WATER						= LLDrawPool::POOL_WATER,
 		RENDER_TYPE_ALPHA						= LLDrawPool::POOL_ALPHA,
		RENDER_TYPE_GLOW						= LLDrawPool::POOL_GLOW,
		RENDER_TYPE_PASS_SIMPLE 				= LLRenderPass::PASS_SIMPLE,
		RENDER_TYPE_PASS_GRASS					= LLRenderPass::PASS_GRASS,
		RENDER_TYPE_PASS_FULLBRIGHT				= LLRenderPass::PASS_FULLBRIGHT,
		RENDER_TYPE_PASS_FULLBRIGHT_SHINY		= LLRenderPass::PASS_FULLBRIGHT_SHINY,
		RENDER_TYPE_PASS_SHINY					= LLRenderPass::PASS_SHINY,
		RENDER_TYPE_PASS_BUMP					= LLRenderPass::PASS_BUMP,
		RENDER_TYPE_PASS_POST_BUMP				= LLRenderPass::PASS_POST_BUMP,
		RENDER_TYPE_PASS_GLOW					= LLRenderPass::PASS_GLOW,
		RENDER_TYPE_PASS_ALPHA					= LLRenderPass::PASS_ALPHA,
		RENDER_TYPE_PASS_ALPHA_MASK				= LLRenderPass::PASS_ALPHA_MASK,
		RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK	= LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
		RENDER_TYPE_PASS_MATERIAL				= LLRenderPass::PASS_MATERIAL,
		RENDER_TYPE_PASS_MATERIAL_ALPHA			= LLRenderPass::PASS_MATERIAL_ALPHA,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK	= LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE= LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		RENDER_TYPE_PASS_SPECMAP				= LLRenderPass::PASS_SPECMAP,
		RENDER_TYPE_PASS_SPECMAP_BLEND			= LLRenderPass::PASS_SPECMAP_BLEND,
		RENDER_TYPE_PASS_SPECMAP_MASK			= LLRenderPass::PASS_SPECMAP_MASK,
		RENDER_TYPE_PASS_SPECMAP_EMISSIVE		= LLRenderPass::PASS_SPECMAP_EMISSIVE,
		RENDER_TYPE_PASS_NORMMAP				= LLRenderPass::PASS_NORMMAP,
		RENDER_TYPE_PASS_NORMMAP_BLEND			= LLRenderPass::PASS_NORMMAP_BLEND,
		RENDER_TYPE_PASS_NORMMAP_MASK			= LLRenderPass::PASS_NORMMAP_MASK,
		RENDER_TYPE_PASS_NORMMAP_EMISSIVE		= LLRenderPass::PASS_NORMMAP_EMISSIVE,
		RENDER_TYPE_PASS_NORMSPEC				= LLRenderPass::PASS_NORMSPEC,
		RENDER_TYPE_PASS_NORMSPEC_BLEND			= LLRenderPass::PASS_NORMSPEC_BLEND,
		RENDER_TYPE_PASS_NORMSPEC_MASK			= LLRenderPass::PASS_NORMSPEC_MASK,
		RENDER_TYPE_PASS_NORMSPEC_EMISSIVE		= LLRenderPass::PASS_NORMSPEC_EMISSIVE,
		// Following are object types (only used in drawable mRenderType)
		RENDER_TYPE_HUD = LLRenderPass::NUM_RENDER_TYPES,
		RENDER_TYPE_VOLUME,
		RENDER_TYPE_PARTICLES,
		RENDER_TYPE_WL_CLOUDS,
#if ENABLE_CLASSIC_CLOUDS
		RENDER_TYPE_CLASSIC_CLOUDS,
#endif
		RENDER_TYPE_HUD_PARTICLES,
		NUM_RENDER_TYPES,
		END_RENDER_TYPES = NUM_RENDER_TYPES
	};

	enum LLRenderDebugFeatureMask
	{
		RENDER_DEBUG_FEATURE_UI					= 0x0001,
		RENDER_DEBUG_FEATURE_SELECTED			= 0x0002,
		RENDER_DEBUG_FEATURE_HIGHLIGHTED		= 0x0004,
		RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES	= 0x0008,
// 		RENDER_DEBUG_FEATURE_HW_LIGHTING		= 0x0010,
		RENDER_DEBUG_FEATURE_FLEXIBLE			= 0x0010,
		RENDER_DEBUG_FEATURE_FOG				= 0x0020,
		RENDER_DEBUG_FEATURE_FR_INFO			= 0x0080,
		RENDER_DEBUG_FEATURE_FOOT_SHADOWS		= 0x0100,
	};

	enum LLRenderDebugMask
	{
		RENDER_DEBUG_COMPOSITION		= 0x00000001,
		RENDER_DEBUG_VERIFY				= 0x00000002,
		RENDER_DEBUG_BBOXES				= 0x00000004,
		RENDER_DEBUG_OCTREE				= 0x00000008,
		RENDER_DEBUG_WIND_VECTORS		= 0x00000010,
		RENDER_DEBUG_OCCLUSION			= 0x00000020,
		RENDER_DEBUG_POINTS				= 0x00000040,
		RENDER_DEBUG_TEXTURE_PRIORITY	= 0x00000080,
		RENDER_DEBUG_TEXTURE_AREA		= 0x00000100,
		RENDER_DEBUG_FACE_AREA			= 0x00000200,
		RENDER_DEBUG_PARTICLES			= 0x00000400,
		RENDER_DEBUG_GLOW				= 0x00000800,
		RENDER_DEBUG_TEXTURE_ANIM		= 0x00001000,
		RENDER_DEBUG_LIGHTS				= 0x00002000,
		RENDER_DEBUG_BATCH_SIZE			= 0x00004000,
		RENDER_DEBUG_ALPHA_BINS			= 0x00008000,
		RENDER_DEBUG_RAYCAST            = 0x00010000,
		RENDER_DEBUG_SHAME				= 0x00020000,
		RENDER_DEBUG_SHADOW_FRUSTA		= 0x00040000,
		RENDER_DEBUG_SCULPTED           = 0x00080000,
		RENDER_DEBUG_AVATAR_VOLUME      = 0x00100000,
		RENDER_DEBUG_AVATAR_JOINTS      = 0x00200000,
		RENDER_DEBUG_BUILD_QUEUE		= 0x00400000,
		RENDER_DEBUG_AGENT_TARGET       = 0x00800000,
		RENDER_DEBUG_UPDATE_TYPE		= 0x01000000,
		RENDER_DEBUG_PHYSICS_SHAPES     = 0x02000000,
		RENDER_DEBUG_NORMALS	        = 0x04000000,
		RENDER_DEBUG_LOD_INFO	        = 0x08000000,
		RENDER_DEBUG_RENDER_COMPLEXITY  = 0x10000000,
		RENDER_DEBUG_ATTACHMENT_BYTES	= 0x20000000,
		RENDER_DEBUG_TEXEL_DENSITY		= 0x40000000
	};

public:
	
	LLSpatialPartition* getSpatialPartition(LLViewerObject* vobj);

	BOOL					 mBackfaceCull;
	S32						 mBatchCount;
	S32						 mMatrixOpCount;
	S32						 mTextureMatrixOps;
	S32						 mMaxBatchSize;
	S32						 mMinBatchSize;
	S32						 mMeanBatchSize;
	S32						 mTrianglesDrawn;
	S32						 mNumVisibleNodes;

	static S32				sCompiles;

	static BOOL				sShowHUDAttachments;
	static BOOL				sForceOldBakedUpload; // If true will not use capabilities to upload baked textures.
	static S32				sUseOcclusion;  // 0 = no occlusion, 1 = read only, 2 = read/write
	static BOOL				sDelayVBUpdate;
	static BOOL				sAutoMaskAlphaDeferred;
	static BOOL				sAutoMaskAlphaNonDeferred;
	static BOOL				sRenderBump;
	static BOOL				sNoAlpha;
	static BOOL				sUseFarClip;
	static BOOL				sShadowRender;
	static BOOL				sSkipUpdate; //skip lod updates
	static BOOL				sWaterReflections;
	static BOOL				sDynamicLOD;
	static BOOL				sPickAvatar;
	static BOOL				sReflectionRender;
	static BOOL				sImpostorRender;
	static BOOL				sImpostorRenderAlphaDepthPass;
	static BOOL				sUnderWaterRender;
	static BOOL				sRenderGlow;
	static BOOL				sTextureBindTest;
	static BOOL				sRenderFrameTest;
	static BOOL				sRenderAttachedLights;
	static BOOL				sRenderAttachedParticles;
	static BOOL				sRenderDeferred;
	static BOOL             sMemAllocationThrottled;
	static S32				sVisibleLightCount;
	static F32				sMinRenderSize;
	static BOOL				sRenderingHUDs;


public:
	//screen texture
	LLRenderTarget			mScreen;
	LLRenderTarget			mFinalScreen;
	LLRenderTarget			mDeferredScreen;
private:
	LLRenderTarget			mFXAABuffer;
public:
	LLRenderTarget			mDeferredDepth;
private:
	LLRenderTarget			mDeferredDownsampledDepth;
	LLRenderTarget			mDeferredLight;
public:
	LLMultisampleBuffer		mSampleBuffer;
private:
	LLRenderTarget			mPhysicsDisplay;

	//utility buffer for rendering post effects, gets abused by renderDeferredLighting
	LLPointer<LLVertexBuffer> mAuxScreenRectVB;

public:
	//utility buffer for rendering cubes, 8 vertices are corners of a cube [-1, 1]
	LLPointer<LLVertexBuffer> mCubeVB;

private:
	//sun shadow map
	LLRenderTarget			mShadow[6];
	std::vector<LLVector3>	mShadowFrustPoints[4];
public:
	LLCamera				mShadowCamera[8];
private:
	LLVector3				mShadowExtents[4][2];
	LLMatrix4a				mSunShadowMatrix[6];
	LLMatrix4a				mShadowModelview[6];
	LLMatrix4a				mShadowProjection[6];
	
	LLPointer<LLDrawable>				mShadowSpotLight[2];
	F32									mSpotLightFade[2];
	LLPointer<LLDrawable>				mTargetShadowSpotLight[2];

	LLVector4				mSunClipPlanes;
public:
	//water reflection texture
	LLRenderTarget				mWaterRef;

	//water distortion texture (refraction)
	LLRenderTarget				mWaterDis;

private:
	//texture for making the glow
	LLRenderTarget				mGlow[2];

	//noise map
	LLImageGL::GLTextureName	mNoiseMap;
	LLImageGL::GLTextureName	mLightFunc;

	LLColor4				mSunDiffuse;
	LLVector3				mSunDir;
	LL_ALIGN_16(LLVector4a	mTransformedSunDir);

public:
	BOOL					mInitialized;
	BOOL					mVertexShadersEnabled;

	U32						mTransformFeedbackPrimitives; //number of primitives expected to be generated by transform feedback
private:
	BOOL					mRenderTypeEnabled[NUM_RENDER_TYPES];
	std::stack<std::string> mRenderTypeEnableStack;

	U32						mRenderDebugFeatureMask;
	U32						mRenderDebugMask;
	std::stack<U32>			mRenderDebugFeatureStack;

	U32						mOldRenderDebugMask;
	
	/////////////////////////////////////////////
	//
	//
	LLDrawable::drawable_vector_t	mMovedList;
	LLDrawable::drawable_vector_t mMovedBridge;
	LLDrawable::drawable_vector_t	mShiftList;

	/////////////////////////////////////////////
	//
	//
	struct Light
	{
		Light(LLDrawable* ptr, F32 d, F32 f = 0.0f)
			: drawable(ptr),
			  dist(d),
			  fade(f)
		{}
		LLPointer<LLDrawable> drawable;
		F32 dist;
		F32 fade;
		struct compare
		{
			bool operator()(const Light& a, const Light& b) const
			{
				if ( a.dist < b.dist )
					return true;
				else if ( a.dist > b.dist )
					return false;
				else
					return a.drawable < b.drawable;
			}
		};
	};
	typedef std::set< Light, Light::compare > light_set_t;
	
	LLDrawable::drawable_set_t		mLights;
	light_set_t						mNearbyLights; // lights near camera
	
	/////////////////////////////////////////////
	//
	// Different queues of drawables being processed.
	//
	LLDrawable::drawable_list_t 	mBuildQ1; // priority
	LLDrawable::drawable_list_t 	mBuildQ2; // non-priority
	LLSpatialGroup::sg_vector_t		mGroupQ1; //priority
	LLSpatialGroup::sg_vector_t		mGroupQ2; // non-priority

	LLSpatialGroup::sg_vector_t		mGroupSaveQ1; // a place to save mGroupQ1 until it is safe to unref

	LLSpatialGroup::sg_vector_t		mMeshDirtyGroup; //groups that need rebuildMesh called
	U32 mMeshDirtyQueryObject;

	LLDrawable::drawable_list_t		mPartitionQ; //drawables that need to update their spatial partition radius 

	bool mGroupQ2Locked;
	bool mGroupQ1Locked;

	bool mResetVertexBuffers; //if true, clear vertex buffers on next update

	LLViewerObject::vobj_list_t		mCreateQ;
		
	LLDrawable::drawable_set_t		mRetexturedList;

	bool mInRenderPass;
	std::vector< std::unique_ptr<LLGLStateIface> > mRenderPassStates;

	//////////////////////////////////////////////////
	//
	// Draw pools are responsible for storing all rendered data,
	// and performing the actual rendering of objects.
	//
	struct compare_pools
	{
		bool operator()(const LLDrawPool* a, const LLDrawPool* b) const
		{
			if (!a)
				return true;
			else if (!b)
				return false;
			else
			{
				S32 atype = a->getType();
				S32 btype = b->getType();
				if (atype < btype)
					return true;
				else if (atype > btype)
					return false;
				else
					return a->getId() < b->getId();
			}
		}
	};
 	typedef std::set<LLDrawPool*, compare_pools > pool_set_t;
	pool_set_t mPools;
	LLDrawPool*	mLastRebuildPool;
	
	// For quick-lookups into mPools (mapped by texture pointer)
	std::map<uintptr_t, LLDrawPool*>	mTerrainPools;
	std::map<uintptr_t, LLDrawPool*>	mTreePools;
	LLDrawPoolAlpha*			mAlphaPool;
	LLDrawPool*					mSkyPool;
	LLDrawPool*					mTerrainPool;
	LLDrawPool*					mWaterPool;
	LLDrawPool*					mGroundPool;
	LLRenderPass*				mSimplePool;
	LLRenderPass*				mGrassPool;
	LLRenderPass*				mAlphaMaskPool;
	LLRenderPass*				mFullbrightAlphaMaskPool;
	LLRenderPass*				mFullbrightPool;
	LLDrawPool*					mGlowPool;
	LLDrawPool*					mBumpPool;
	LLDrawPool*					mMaterialsPool;
	LLDrawPool*					mWLSkyPool;
	// Note: no need to keep an quick-lookup to avatar pools, since there's only one per avatar
	
public:
	std::vector<LLFace*>		mHighlightFaces;	// highlight faces on physical objects
protected:
	std::vector<LLFace*>		mSelectedFaces;

	class DebugBlip
	{
	public:
		LLColor4 mColor;
		LLVector3 mPosition;
		F32 mAge;

		DebugBlip(const LLVector3& position, const LLColor4& color)
			: mColor(color), mPosition(position), mAge(0.f)
		{ }
	};

	std::list<DebugBlip> mDebugBlips;

	LLPointer<LLViewerFetchedTexture>	mFaceSelectImagep;
	
	std::vector<LLLightStateData>		mLocalLights;
	U8						mHWLightCount;
	U8						mLightMode;
	U32						mLightMask;
	bool					mLightingEnabled;
		
	static BOOL				sRenderPhysicalBeacons;
	static BOOL				sRenderMOAPBeacons;
	static BOOL				sRenderScriptedTouchBeacons;
	static BOOL				sRenderScriptedBeacons;
	static BOOL				sRenderParticleBeacons;
	static BOOL				sRenderSoundBeacons;
public:
	static BOOL				sRenderBeacons;
	static BOOL				sRenderHighlight;

	// Determines which set of UVs to use in highlight display
	//
	static LLRender::eTexIndex sRenderHighlightTextureChannel;

	//debug use
	static U32              sCurRenderPoolType ;
} LL_ALIGN_POSTFIX(16);

void render_bbox(const LLVector3 &min, const LLVector3 &max);
void render_hud_elements();

extern LLPipeline gPipeline;
extern BOOL gDebugPipeline;
extern const LLMatrix4a* gGLLastMatrix;

#endif
