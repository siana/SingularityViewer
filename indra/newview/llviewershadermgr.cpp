/** 
 * @file llviewershadermgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include <boost/lexical_cast.hpp>

#include "llfeaturemanager.h"
#include "llviewershadermgr.h"

#include "llfile.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "pipeline.h"
#include "llworld.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llsky.h"
#include "llvosky.h"
#include "llrender.h"
#include "lljoint.h"
#include "llskinningutil.h"

#if LL_DARWIN
#include "OpenGL/OpenGL.h"
#endif

#ifdef LL_RELEASE_FOR_DOWNLOAD
#define UNIFORM_ERRS LL_WARNS_ONCE("Shader")
#else
#define UNIFORM_ERRS LL_ERRS("Shader")
#endif

static LLStaticHashedString sTexture0("texture0");
static LLStaticHashedString sTexture1("texture1");
static LLStaticHashedString sTex0("tex0");
static LLStaticHashedString sTex1("tex1");
static LLStaticHashedString sGlowMap("glowMap");
static LLStaticHashedString sScreenMap("screenMap");

// Lots of STL stuff in here, using namespace std to keep things more readable
using std::vector;
using std::pair;
using std::make_pair;
using std::string;

BOOL				LLViewerShaderMgr::sInitialized = FALSE;
bool				LLViewerShaderMgr::sSkipReload = false;

LLVector4			gShinyOrigin;

 // WindLight shader handles
 // Make sure WL Sky is the first program
LLGLSLShader		gWLSkyProgram(LLViewerShaderMgr::SHADER_WINDLIGHT);
LLGLSLShader		gWLCloudProgram(LLViewerShaderMgr::SHADER_WINDLIGHT);
//transform shaders
LLGLSLShader			gTransformPositionProgram(LLViewerShaderMgr::SHADER_TRANSFORM);
LLGLSLShader			gTransformTexCoordProgram(LLViewerShaderMgr::SHADER_TRANSFORM);
LLGLSLShader			gTransformNormalProgram(LLViewerShaderMgr::SHADER_TRANSFORM);
LLGLSLShader			gTransformColorProgram(LLViewerShaderMgr::SHADER_TRANSFORM);
LLGLSLShader			gTransformTangentProgram(LLViewerShaderMgr::SHADER_TRANSFORM);

//utility shaders
LLGLSLShader	gOcclusionProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gOcclusionCubeProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gCustomAlphaProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gGlowCombineProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gSplatTextureRectProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gGlowCombineFXAAProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gTwoTextureAddProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gOneTextureNoColorProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gDebugProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gClipProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gDownsampleDepthProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gAlphaMaskProgram(LLViewerShaderMgr::SHADER_INTERFACE);

LLGLSLShader	gUIProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader	gSolidColorProgram(LLViewerShaderMgr::SHADER_INTERFACE);

 //object shaders
LLGLSLShaderArray<LLViewerShaderMgr::SHADER_OBJECT>	gObjectSimpleProgram[1<<SHD_COUNT];
LLGLSLShaderArray<LLViewerShaderMgr::SHADER_OBJECT>	gObjectFullbrightProgram[1<<SHD_COUNT];
LLGLSLShaderArray<LLViewerShaderMgr::SHADER_OBJECT>	gObjectEmissiveProgram[1<<SHD_SHINY_BIT];
LLGLSLShader		gObjectPreviewProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectBumpProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gTreeProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gTreeWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectFullbrightNoColorProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectFullbrightNoColorWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);

LLGLSLShader		gObjectSimpleNonIndexedTexGenProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectSimpleNonIndexedTexGenWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectAlphaMaskNoColorProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectAlphaMaskNoColorWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);

 //environment shaders
LLGLSLShader		gTerrainProgram(LLViewerShaderMgr::SHADER_ENVIRONMENT);
LLGLSLShader		gTerrainWaterProgram(LLViewerShaderMgr::SHADER_WATER); //note, water.
LLGLSLShader		gWaterProgram(LLViewerShaderMgr::SHADER_WATER);
LLGLSLShader		gUnderWaterProgram(LLViewerShaderMgr::SHADER_WATER);
 
 //interface shaders
LLGLSLShader		gHighlightProgram(LLViewerShaderMgr::SHADER_INTERFACE);			//Not in mShaderList
LLGLSLShader		gHighlightNormalProgram(LLViewerShaderMgr::SHADER_INTERFACE);
LLGLSLShader		gHighlightSpecularProgram(LLViewerShaderMgr::SHADER_INTERFACE);
 //avatar shader handles
LLGLSLShader		gAvatarProgram(LLViewerShaderMgr::SHADER_AVATAR);
LLGLSLShader		gAvatarWaterProgram(LLViewerShaderMgr::SHADER_AVATAR);
LLGLSLShader		gImpostorProgram(LLViewerShaderMgr::SHADER_OBJECT);

 // Effects Shaders
LLGLSLShader			gGlowProgram(LLViewerShaderMgr::SHADER_EFFECT);				//Not in mShaderList
LLGLSLShader			gGlowExtractProgram(LLViewerShaderMgr::SHADER_EFFECT);		//Not in mShaderList
LLGLSLShader			gPostColorFilterProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
LLGLSLShader			gPostNightVisionProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
LLGLSLShader			gPostGaussianBlurProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
LLGLSLShader			gPostPosterizeProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
LLGLSLShader			gPostMotionBlurProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
LLGLSLShader			gPostVignetteProgram(LLViewerShaderMgr::SHADER_EFFECT);		//Not in mShaderList

 // Deferred rendering shaders
LLGLSLShader			gDeferredImpostorProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredWaterProgram(LLViewerShaderMgr::SHADER_DEFERRED); //calculatesAtmospherics
LLGLSLShader			gDeferredUnderWaterProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredDiffuseProgram(LLViewerShaderMgr::SHADER_DEFERRED);//Not in mShaderList
LLGLSLShader			gDeferredDiffuseAlphaMaskProgram(LLViewerShaderMgr::SHADER_DEFERRED);
//LLGLSLShader			gDeferredNonIndexedDiffuseProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredNonIndexedDiffuseAlphaMaskProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSkinnedDiffuseProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSkinnedBumpProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSkinnedAlphaProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredBumpProgram(LLViewerShaderMgr::SHADER_DEFERRED);	//Not in mShaderList
LLGLSLShader			gDeferredTerrainProgram(LLViewerShaderMgr::SHADER_DEFERRED);//Not in mShaderList
LLGLSLShader			gDeferredTreeProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredTreeShadowProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredAvatarProgram(LLViewerShaderMgr::SHADER_DEFERRED);	//Not in mShaderList
LLGLSLShader			gDeferredAvatarAlphaProgram(LLViewerShaderMgr::SHADER_DEFERRED); //calculatesAtmospherics
LLGLSLShader			gDeferredLightProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShaderArray<LLViewerShaderMgr::SHADER_DEFERRED>	gDeferredMultiLightProgram[16];
LLGLSLShader			gDeferredSpotLightProgram(LLViewerShaderMgr::SHADER_DEFERRED); //Not in mShaderList
LLGLSLShader			gDeferredMultiSpotLightProgram(LLViewerShaderMgr::SHADER_DEFERRED); //Not in mShaderList
LLGLSLShader			gDeferredSSAOProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredDownsampleDepthNearestProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSunProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredBlurLightProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSoftenProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSoftenWaterProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredShadowProgram(LLViewerShaderMgr::SHADER_DEFERRED);		//Not in mShaderList
LLGLSLShader			gDeferredShadowCubeProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredShadowAlphaMaskProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredAvatarShadowProgram(LLViewerShaderMgr::SHADER_DEFERRED);//Not in mShaderList
LLGLSLShader			gDeferredAttachmentShadowProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredAlphaProgram(LLViewerShaderMgr::SHADER_DEFERRED);		//calculatesAtmospherics
LLGLSLShader			gDeferredAlphaImpostorProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredAlphaWaterProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredFullbrightProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredFullbrightAlphaMaskProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredFullbrightWaterProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredFullbrightAlphaMaskWaterProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredEmissiveProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredPostProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredCoFProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredDoFCombineProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredPostGammaCorrectProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gFXAAProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredPostNoDoFProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredWLSkyProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredWLCloudProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredStarProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredFullbrightShinyProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSkinnedFullbrightShinyProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSkinnedFullbrightProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gNormalMapGenProgram(LLViewerShaderMgr::SHADER_DEFERRED);

// Deferred materials shaders
LLGLSLShaderArray<LLViewerShaderMgr::SHADER_DEFERRED>	gDeferredMaterialProgram[LLMaterial::SHADER_COUNT*2];
LLGLSLShaderArray<LLViewerShaderMgr::SHADER_DEFERRED>	gDeferredMaterialWaterProgram[LLMaterial::SHADER_COUNT*2];

LLViewerShaderMgr::LLViewerShaderMgr() :
	mVertexShaderLevel(SHADER_COUNT, 0)
{}

LLViewerShaderMgr::~LLViewerShaderMgr()
{
	mVertexShaderLevel.clear();
}

// static
LLViewerShaderMgr * LLViewerShaderMgr::instance()
{
	if(NULL == sInstance)
	{
		sInstance = new LLViewerShaderMgr();
	}

	return static_cast<LLViewerShaderMgr*>(sInstance);
}

void LLViewerShaderMgr::initAttribsAndUniforms(void)
{
	if (mReservedAttribs.empty())
	{
		LLShaderMgr::initAttribsAndUniforms();
	}	
}
	

//============================================================================
// Set Levels

S32 LLViewerShaderMgr::getVertexShaderLevel(S32 type)
{
	return mVertexShaderLevel[type];
}

//============================================================================
// Shader Management

void LLViewerShaderMgr::setShaders()
{
	//setShaders might be called redundantly by gSavedSettings, so return on reentrance
	static bool reentrance = false;
	
	if (!gPipeline.mInitialized || !sInitialized || reentrance || sSkipReload)
	{
		return;
	}

	//Since setShaders can be reentrant, be sure to clear out stale shader objects that may be left over from parent call.
	unloadShaderObjects();
	unloadShaders();

	LLGLSLShader::sIndexedTextureChannels = llmax(llmin(gGLManager.mNumTextureImageUnits, (S32) gSavedSettings.getU32("RenderMaxTextureIndex")), 1);
	static const LLCachedControl<bool> no_texture_indexing("ShyotlUseLegacyTextureBatching",false);
	if(no_texture_indexing)
		LLGLSLShader::sIndexedTextureChannels = 1;

	//NEVER use more than 16 texture channels (work around for prevalent driver bug)
	LLGLSLShader::sIndexedTextureChannels = llmin(LLGLSLShader::sIndexedTextureChannels, 16);

	if (gGLManager.mGLSLVersionMajor < 1 ||
		(gGLManager.mGLSLVersionMajor == 1 && gGLManager.mGLSLVersionMinor <= 20))
	{ //NEVER use indexed texture rendering when GLSL version is 1.20 or earlier
		LLGLSLShader::sIndexedTextureChannels = 1;
	}

	reentrance = true;

	if (LLRender::sGLCoreProfile)
	{  
		if (!gSavedSettings.getBOOL("VertexShaderEnable"))
		{ //vertex shaders MUST be enabled to use core profile
			gSavedSettings.setBOOL("VertexShaderEnable", TRUE);
		}
	}
	
	initAttribsAndUniforms();
	gPipeline.releaseGLBuffers();

	bool want_shaders = LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable") &&
						gSavedSettings.getBOOL("VertexShaderEnable") && 
						(gGLManager.mGLSLVersionMajor > 1 || gGLManager.mGLSLVersionMinor >= 10);

	bool want_deferred = want_shaders && LLPipeline::isRenderDeferredDesired();

	if (want_shaders)
	{
		//sRenderGlow needs set as it's referenced in the shader load process.
		LLPipeline::sRenderGlow = want_deferred || gSavedSettings.getBOOL("RenderGlow");
	}
	
	//hack to reset buffers that change behavior with shaders
	gPipeline.resetVertexBuffers();

	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_WAIT);
	}

	// Lighting
	gPipeline.updateLocalLightingEnabled();

	// Shaders
	LL_INFOS("ShaderLoading") << "\n~~~~~~~~~~~~~~~~~~\n Loading Shaders:\n~~~~~~~~~~~~~~~~~~" << LL_ENDL;
	LL_INFOS("ShaderLoading") << llformat("Using GLSL %d.%d", gGLManager.mGLSLVersionMajor, gGLManager.mGLSLVersionMinor) << LL_ENDL;

	LLVertexBuffer::unbind();
	
	if (want_shaders)
	{
		S32 light_class = 2;
		S32 env_class = 2;
		S32 obj_class = 2;
		S32 effect_class = 2;
		S32 wl_class = 2;
		S32 water_class = 2;
		S32 deferred_class = 0;
		S32 transform_class = gGLManager.mHasTransformFeedback ? 1 : 0;

		static LLCachedControl<bool> use_transform_feedback(gSavedSettings, "RenderUseTransformFeedback");
		if (!use_transform_feedback)
		{
			transform_class = 0;
		}

		if (want_deferred)
		{
			if (gSavedSettings.getS32("RenderShadowDetail") > 0)
			{ //shadows
				deferred_class = 2;
			}
			else
			{ //no shadows
				deferred_class = 1;
			}

			//make sure hardware skinning is enabled
			//gSavedSettings.setBOOL("RenderAvatarVP", TRUE);
			
			//make sure atmospheric shaders are enabled
			//gSavedSettings.setBOOL("WindLightUseAtmosShaders", TRUE);
		}


		if (!(LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders")
			  && gSavedSettings.getBOOL("WindLightUseAtmosShaders")))
		{
			// user has disabled WindLight in their settings, downgrade
			// windlight shaders to stub versions.
			wl_class = 1;
		}


		// Trigger a full rebuild of the fallback skybox / cubemap if we've toggled windlight shaders
		if (mVertexShaderLevel[SHADER_WINDLIGHT] != wl_class && gSky.mVOSkyp.notNull())
		{
			gSky.mVOSkyp->forceSkyUpdate();
		}

		// Load lighting shaders
		mVertexShaderLevel[SHADER_LIGHTING] = light_class;
		mVertexShaderLevel[SHADER_INTERFACE] = light_class;
		mVertexShaderLevel[SHADER_ENVIRONMENT] = env_class;
		mVertexShaderLevel[SHADER_WATER] = water_class;
		mVertexShaderLevel[SHADER_OBJECT] = obj_class;
		mVertexShaderLevel[SHADER_EFFECT] = effect_class;
		mVertexShaderLevel[SHADER_WINDLIGHT] = wl_class;
		mVertexShaderLevel[SHADER_DEFERRED] = deferred_class;
		mVertexShaderLevel[SHADER_TRANSFORM] = transform_class;

		BOOL loaded = loadBasicShaders();

		if (loaded)
		{
			// Load all shaders to set max levels
			loaded = loadShadersEnvironment();

			if (loaded)
			{
				loaded = loadShadersWater();
			}

			if (loaded)
			{
				loaded = loadShadersWindLight();
			}

			if (loaded)
			{
				loaded = loadShadersEffects();
			}

			if (loaded)
			{
				loaded = loadShadersInterface();
			}
			
			if (loaded)
			{
				loadTransformShaders();
			}

			if (loaded)
			{
				// Load max avatar shaders to set the max level
				bool can_skin = want_deferred || (LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP") && gSavedSettings.getBOOL("RenderAvatarVP"));
				
				if (can_skin)
				{ //hardware skinning is enabled and rigged attachment shaders loaded correctly

					// Set the actual level
					mVertexShaderLevel[SHADER_AVATAR] = gSavedSettings.getBOOL("RenderAvatarCloth") ? 3 : 1;

					if(!loadShadersAvatar() || !loadShadersObject())
					{
						can_skin = false;
					}
					else if (mVertexShaderLevel[SHADER_AVATAR] < 3)
					{
						gSavedSettings.setBOOL("RenderAvatarCloth", false);
					}
				}

				if (!can_skin)
				{ //hardware skinning not possible, neither is deferred rendering
					mVertexShaderLevel[SHADER_DEFERRED] = 0;
					mVertexShaderLevel[SHADER_AVATAR] = 0;
					unloadShaderClass(SHADER_AVATAR);
					unloadShaderClass(SHADER_OBJECT);

					want_deferred = false;	//Deferred requires skinned shaders.

					gSavedSettings.setBOOL("RenderDeferred", FALSE);
					gSavedSettings.setBOOL("RenderAvatarCloth", FALSE);
					gSavedSettings.setBOOL("RenderAvatarVP", FALSE);

					loaded = loadShadersObject();
				}
			}

			if (loaded)
			{
				loaded = loadShadersInterface();
			}

			if (!loaded)
			{ //some shader absolutely could not load, try to fall back to a simpler setting
				if (gSavedSettings.getBOOL("WindLightUseAtmosShaders"))
				{ //disable windlight and try again
					gSavedSettings.setBOOL("WindLightUseAtmosShaders", FALSE);
					gSavedSettings.setBOOL("RenderDeferred", FALSE);
					reentrance = false;
					setShaders();
					return;
				}

				if (gSavedSettings.getBOOL("VertexShaderEnable"))
				{ //disable shaders outright and try again
					gSavedSettings.setBOOL("VertexShaderEnable", FALSE);
					gSavedSettings.setBOOL("RenderDeferred", FALSE);
					reentrance = false;
					setShaders();
					return;
				}
			}
			else if (!loadShadersDeferred())
			{
				gSavedSettings.setBOOL("RenderDeferred", FALSE);
				reentrance = false;
				setShaders();
				return;
			}
		}
		if (!loaded)
		{
			llassert_always(!LLRender::sGLCoreProfile);	//This is bad...
			unloadShaders();
		}
		else //Shaders loaded
		{
			//using shaders, disable fixed function
			LLGLSLShader::sNoFixedFunction = true;
			LLPipeline::sWaterReflections = gGLManager.mHasCubeMap;
			LLPipeline::sRenderDeferred = want_deferred;
		}

		//Flag base shader objects for deletion
		//Don't worry-- they won't be deleted until no programs refrence them.
		unloadShaderObjects();
		cleanupShaderSources();
	}

	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_ARROW);
	}

	LLWaterParamManager::getInstance()->updateShaderLinks();
	LLWLParamManager::getInstance()->updateShaderLinks();

	gPipeline.refreshCachedSettings();
	gPipeline.createGLBuffers();

	reentrance = false;
}

void LLViewerShaderMgr::unloadShaders()
{
	LLShaderMgr::unloadShaders();

	for (S32 i = 0; i < SHADER_COUNT; i++)
		mVertexShaderLevel[i] = 0;

	//Unset all shader-dependent static variables.
	LLGLSLShader::sNoFixedFunction = LLRender::sGLCoreProfile;
	LLGLSLShader::sIndexedTextureChannels = 1;
	LLPipeline::sRenderDeferred = false;
	LLPipeline::sWaterReflections = FALSE;
	LLPipeline::sRenderGlow = FALSE;

	// Clear sync hashes. If shaders were just turned off then glLight state needs to be resynced.
	gGL.resetSyncHashes();
}

BOOL LLViewerShaderMgr::loadBasicShaders()
{
	// Load basic dependency shaders first
	// All of these have to load for any shaders to function
	
	S32 sum_lights_class = 3;

	// class one cards will get the lower sum lights
	// class zero we're not going to think about
	// since a class zero card COULD be a ridiculous new card
	// and old cards should have the features masked
	if(LLFeatureManager::getInstance()->getGPUClass() == GPU_CLASS_1)
	{
		sum_lights_class = 2;
	}

	// If we have sun and moon only checked, then only sum those lights.
	if (!gPipeline.isLocalLightingEnabled())
	{
		sum_lights_class = 1;
	}

#if LL_DARWIN
	// Work around driver crashes on older Macs when using deferred rendering
	// NORSPEC-59
	//
	if (gGLManager.mIsMobileGF)
		sum_lights_class = 3;
#endif
	
	// Use the feature table to mask out the max light level to use.  Also make sure it's at least 1.
	S32 max_light_class = gSavedSettings.getS32("RenderShaderLightingMaxLevel");
	sum_lights_class = llclamp(sum_lights_class, 1, max_light_class);

	// Load the Basic Vertex Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	vector< pair<string, S32> > shaders;
	shaders.push_back( make_pair( "windlight/atmosphericsVarsV.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/atmosphericsVarsWaterV.glsl",	mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/atmosphericsHelpersV.glsl",	mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "lighting/lightFuncV.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/sumLightsV.glsl",				sum_lights_class ) );
	shaders.push_back( make_pair( "lighting/lightV.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFuncSpecularV.glsl",		mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/sumLightsSpecularV.glsl",		sum_lights_class ) );
	shaders.push_back( make_pair( "lighting/lightSpecularV.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "windlight/atmosphericsV.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "avatar/avatarSkinV.glsl",				1 ) );
	shaders.push_back( make_pair( "avatar/objectSkinV.glsl",				1 ) );
	if (gGLManager.mGLSLVersionMajor >= 2 || gGLManager.mGLSLVersionMinor >= 30)
	{
		shaders.push_back( make_pair( "objects/indexedTextureV.glsl",			1 ) );
	}
	shaders.push_back( make_pair( "objects/nonindexedTextureV.glsl",		1 ) );

	std::map<std::string, std::string> attribs;
	attribs["MAX_JOINTS_PER_MESH_OBJECT"] = 
		boost::lexical_cast<std::string>(LLSkinningUtil::getMaxJointCount());
	
	// We no longer have to bind the shaders to global glhandles, they are automatically added to a map now.
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_VERTEX_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_VERTEX_SHADER_ARB, &attribs) == 0)
		{
			return FALSE;
		}
	}

	// Load the Basic Fragment Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	shaders.clear();
	S32 ch = llmax(LLGLSLShader::sIndexedTextureChannels-1, 1);

	std::vector<S32> index_channels;
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "windlight/atmosphericsVarsF.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "windlight/atmosphericsVarsWaterF.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "windlight/gammaF.glsl",					mVertexShaderLevel[SHADER_WINDLIGHT]) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "windlight/atmosphericsF.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "windlight/transportF.glsl",				mVertexShaderLevel[SHADER_WINDLIGHT] ) );	
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "environment/waterFogF.glsl",				mVertexShaderLevel[SHADER_WATER] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightNonIndexedF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightAlphaMaskNonIndexedF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightFullbrightNonIndexedF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightFullbrightNonIndexedAlphaMaskF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightWaterNonIndexedF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightWaterAlphaMaskNonIndexedF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightFullbrightWaterNonIndexedF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightFullbrightWaterNonIndexedAlphaMaskF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightShinyNonIndexedF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightFullbrightShinyNonIndexedF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightShinyWaterNonIndexedF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( make_pair( "lighting/lightFullbrightShinyWaterNonIndexedF.glsl", mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightAlphaMaskF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightFullbrightF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightFullbrightAlphaMaskF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightWaterF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightWaterAlphaMaskF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightFullbrightWaterF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightFullbrightWaterAlphaMaskF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightShinyF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( make_pair( "lighting/lightFullbrightShinyF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	shaders.push_back( make_pair( "lighting/lightShinyWaterF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	shaders.push_back( make_pair( "lighting/lightFullbrightShinyWaterF.glsl", mVertexShaderLevel[SHADER_LIGHTING] ) );
	
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_FRAGMENT_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_FRAGMENT_SHADER_ARB, &attribs, index_channels[i]) == 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersEnvironment()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_ENVIRONMENT] == 0)
	{
		unloadShaderClass(SHADER_ENVIRONMENT);
		return TRUE;
	}

	if (success)
	{
		gTerrainProgram.mName = "Terrain Shader";
		gTerrainProgram.mFeatures.calculatesLighting = true;
		gTerrainProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainProgram.mFeatures.hasAtmospherics = true;
		gTerrainProgram.mFeatures.mIndexedTextureChannels = 0;
		gTerrainProgram.mFeatures.disableTextureIndex = true;
		gTerrainProgram.mFeatures.hasGamma = true;
		gTerrainProgram.mShaderFiles.clear();
		gTerrainProgram.mShaderFiles.push_back(make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gTerrainProgram.mShaderFiles.push_back(make_pair("environment/terrainF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTerrainProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		success = gTerrainProgram.createShader(NULL, NULL);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
		unloadShaderClass(SHADER_ENVIRONMENT);
	}
	else
	{
		LLWorld::getInstance()->updateWaterObjects();
	}
	
	return success;
}

BOOL LLViewerShaderMgr::loadShadersWater()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_WATER] == 0)
	{
		unloadShaderClass(SHADER_WATER);
		return TRUE;
	}

	if (success)
	{
		// load water shader
		gWaterProgram.mName = "Water Shader";
		gWaterProgram.mFeatures.calculatesAtmospherics = true;
		gWaterProgram.mFeatures.hasGamma = true;
		gWaterProgram.mFeatures.hasTransport = true;
		gWaterProgram.mShaderFiles.clear();
		gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		success = gWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		//load under water vertex shader
		gUnderWaterProgram.mName = "Underwater Shader";
		gUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		gUnderWaterProgram.mShaderFiles.clear();
		gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/underWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gUnderWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		gUnderWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		
		success = gUnderWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		//load terrain water shader
		gTerrainWaterProgram.mName = "Terrain Water Shader";
		gTerrainWaterProgram.mFeatures.calculatesLighting = true;
		gTerrainWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasWaterFog = true;
		gTerrainWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gTerrainWaterProgram.mFeatures.disableTextureIndex = true;
		gTerrainWaterProgram.mShaderFiles.clear();
		gTerrainWaterProgram.mShaderFiles.push_back(make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gTerrainWaterProgram.mShaderFiles.push_back(make_pair("environment/terrainWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTerrainWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		gTerrainWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gTerrainWaterProgram.createShader(NULL, NULL);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_WATER] = 0;
		unloadShaderClass(SHADER_WATER);
	}
	else
	{
		LLWorld::getInstance()->updateWaterObjects();
	}

	return success;
}

BOOL LLViewerShaderMgr::loadShadersEffects()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_EFFECT] == 0)
	{
		unloadShaderClass(SHADER_EFFECT);
		return TRUE;
	}

	if(LLPipeline::sRenderGlow)
	{
		if (success)
		{
			gGlowProgram.mName = "Glow Shader (Post)";
			gGlowProgram.mShaderFiles.clear();
			gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowV.glsl", GL_VERTEX_SHADER_ARB));
			gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowF.glsl", GL_FRAGMENT_SHADER_ARB));
			gGlowProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
			success = gGlowProgram.createShader(NULL, NULL);
			LLPipeline::sRenderGlow = success;
		}
		
		if (success)
		{
			gGlowExtractProgram.mName = "Glow Extract Shader (Post)";
			gGlowExtractProgram.mShaderFiles.clear();
			gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER_ARB));
			gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractF.glsl", GL_FRAGMENT_SHADER_ARB));
			gGlowExtractProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
			success = gGlowExtractProgram.createShader(NULL, NULL);
			LLPipeline::sRenderGlow = success;
		}
	}
	
#if 1
	// disabling loading of postprocess shaders until we fix
	// ATI sampler2DRect compatibility.
	
	//load Color Filter Shader
	//if (success)
	{
		static std::vector<LLStaticHashedString> shaderUniforms;
		if(shaderUniforms.empty())
		{
			shaderUniforms.reserve(6);
			shaderUniforms.push_back(LLStaticHashedString("gamma"));
			shaderUniforms.push_back(LLStaticHashedString("brightness"));
			shaderUniforms.push_back(LLStaticHashedString("contrast"));
			shaderUniforms.push_back(LLStaticHashedString("contrastBase"));
			shaderUniforms.push_back(LLStaticHashedString("saturation"));
		}

		gPostColorFilterProgram.mName = "Color Filter Shader (Post)";
		gPostColorFilterProgram.mShaderFiles.clear();
		gPostColorFilterProgram.mShaderFiles.push_back(make_pair("effects/colorFilterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostColorFilterProgram.mShaderFiles.push_back(make_pair("interface/onetexturenocolorV.glsl", GL_VERTEX_SHADER_ARB));
		gPostColorFilterProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		if(gPostColorFilterProgram.createShader(NULL, &shaderUniforms))
		{
			gPostColorFilterProgram.bind();
			gPostColorFilterProgram.uniform1i(sTex0, 0);
		}
	}

	//load Night Vision Shader
	//if (success)
	{
		static std::vector<LLStaticHashedString> shaderUniforms;
		if(shaderUniforms.empty())
		{
			shaderUniforms.reserve(3);
			shaderUniforms.push_back(LLStaticHashedString("brightMult"));
			shaderUniforms.push_back(LLStaticHashedString("noiseStrength"));
		}
		gPostNightVisionProgram.mName = "Night Vision Shader (Post)";
		gPostNightVisionProgram.mShaderFiles.clear();
		gPostNightVisionProgram.mShaderFiles.push_back(make_pair("effects/nightVisionF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostNightVisionProgram.mShaderFiles.push_back(make_pair("interface/twotextureaddV.glsl", GL_VERTEX_SHADER_ARB));
		gPostNightVisionProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		if(gPostNightVisionProgram.createShader(NULL, &shaderUniforms))
		{
			gPostNightVisionProgram.bind();
			gPostNightVisionProgram.uniform1i(sTex0, 0);
			gPostNightVisionProgram.uniform1i(sTex1, 1);
		}
	}

	//if (success)
	{
		static std::vector<LLStaticHashedString> shaderUniforms;
		if(shaderUniforms.empty())
		{
			shaderUniforms.reserve(1);
			shaderUniforms.push_back(LLStaticHashedString("horizontalPass"));
		}

		gPostGaussianBlurProgram.mName = "Gaussian Blur Shader (Post)";
		gPostGaussianBlurProgram.mShaderFiles.clear();
		gPostGaussianBlurProgram.mShaderFiles.push_back(make_pair("effects/gaussBlurF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostGaussianBlurProgram.mShaderFiles.push_back(make_pair("interface/onetexturenocolorV.glsl", GL_VERTEX_SHADER_ARB));
		gPostGaussianBlurProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		if(gPostGaussianBlurProgram.createShader(NULL, &shaderUniforms))
		{
			gPostGaussianBlurProgram.bind();
			gPostGaussianBlurProgram.uniform1i(sTex0, 0);
		}
	}

	{
		static std::vector<LLStaticHashedString> shaderUniforms;
		if(shaderUniforms.empty())
		{
			shaderUniforms.reserve(1);
			shaderUniforms.push_back(LLStaticHashedString("layerCount"));
		}

		gPostPosterizeProgram.mName = "Posterize Shader (Post)";
		gPostPosterizeProgram.mShaderFiles.clear();
		gPostPosterizeProgram.mShaderFiles.push_back(make_pair("effects/PosterizeF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostPosterizeProgram.mShaderFiles.push_back(make_pair("interface/onetexturenocolorV.glsl", GL_VERTEX_SHADER_ARB));
		gPostPosterizeProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		if(gPostPosterizeProgram.createShader(NULL, &shaderUniforms))
		{
			gPostPosterizeProgram.bind();
			gPostPosterizeProgram.uniform1i(sTex0, 0);
		}
	}

	{
		static std::vector<LLStaticHashedString> shaderUniforms;
		if(shaderUniforms.empty())
		{
			shaderUniforms.reserve(3);
			shaderUniforms.push_back(LLStaticHashedString("inv_proj"));
			shaderUniforms.push_back(LLStaticHashedString("prev_proj"));
		}

		gPostMotionBlurProgram.mName = "Motion Blur Shader (Post)";
		gPostMotionBlurProgram.mShaderFiles.clear();
		gPostMotionBlurProgram.mShaderFiles.push_back(make_pair("effects/MotionBlurF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostMotionBlurProgram.mShaderFiles.push_back(make_pair("interface/onetexturenocolorV.glsl", GL_VERTEX_SHADER_ARB));
		gPostMotionBlurProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		if(gPostMotionBlurProgram.createShader(NULL, &shaderUniforms))
		{
			gPostMotionBlurProgram.bind();
			gPostMotionBlurProgram.uniform1i(sTex0, 0);
			gPostMotionBlurProgram.uniform1i(sTex1, 1);
		}
	}

	{
		static std::vector<LLStaticHashedString> shaderUniforms;
		if(shaderUniforms.empty())
		{
			shaderUniforms.reserve(3);
			shaderUniforms.push_back(LLStaticHashedString("vignette_darkness"));
			shaderUniforms.push_back(LLStaticHashedString("vignette_radius"));
		}

		gPostVignetteProgram.mName = "Vignette Shader (Post)";
		gPostVignetteProgram.mShaderFiles.clear();
		gPostVignetteProgram.mShaderFiles.push_back(make_pair("effects/VignetteF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostVignetteProgram.mShaderFiles.push_back(make_pair("interface/onetexturenocolorV.glsl", GL_VERTEX_SHADER_ARB));
		gPostVignetteProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		if(gPostVignetteProgram.createShader(NULL, &shaderUniforms))
		{
			gPostVignetteProgram.bind();
			gPostVignetteProgram.uniform1i(sTex0, 0);
		}
	}

	#endif

	if (!success)
	{
		mVertexShaderLevel[SHADER_EFFECT] = 0;
		unloadShaderClass(SHADER_EFFECT);
	}

	return success;

}

BOOL LLViewerShaderMgr::loadShadersDeferred()
{
	if (mVertexShaderLevel[SHADER_DEFERRED] == 0)
	{
		unloadShaderClass(SHADER_DEFERRED);
		return TRUE;
	}

	BOOL success = TRUE;

	success = loadShaderFile("deferred/components/utilityFuncF.glsl", mVertexShaderLevel[SHADER_DEFERRED], GL_FRAGMENT_SHADER_ARB);

	if (success)
	{
		gDeferredDiffuseProgram.mName = "Deferred Diffuse Shader";
		gDeferredDiffuseProgram.mShaderFiles.clear();
		gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseIndexedF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredDiffuseProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDiffuseProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredDiffuseAlphaMaskProgram.mName = "Deferred Diffuse Alpha Mask Shader";
		gDeferredDiffuseAlphaMaskProgram.mShaderFiles.clear();
		gDeferredDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseAlphaMaskIndexedF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredDiffuseAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredDiffuseAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDiffuseAlphaMaskProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mName = "Deferred Diffuse Non-Indexed Alpha Mask Colored Shader";
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.clear();
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseAlphaMaskF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredNonIndexedDiffuseAlphaMaskProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mName = "Deferred Diffuse Non-Indexed Alpha Mask Shader";
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.clear();
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("deferred/diffuseNoColorV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("deferred/diffuseAlphaMaskNoColorF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.createShader(NULL, NULL);
	}

	/*if (success)
	{
		gDeferredNonIndexedDiffuseProgram.mName = "Non Indexed Deferred Diffuse Shader";
		gDeferredNonIndexedDiffuseProgram.mShaderFiles.clear();
		gDeferredNonIndexedDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredNonIndexedDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredNonIndexedDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredNonIndexedDiffuseProgram.createShader(NULL, NULL);
	}*/
		

	if (success)
	{
		gDeferredSkinnedDiffuseProgram.mName = "Deferred Skinned Diffuse Shader";
		gDeferredSkinnedDiffuseProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedDiffuseProgram.mShaderFiles.clear();
		gDeferredSkinnedDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseSkinnedV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSkinnedDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredSkinnedDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSkinnedDiffuseProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredSkinnedBumpProgram.mName = "Deferred Skinned Bump Shader";
		gDeferredSkinnedBumpProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedBumpProgram.mShaderFiles.clear();
		gDeferredSkinnedBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpSkinnedV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSkinnedBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredSkinnedBumpProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSkinnedBumpProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredSkinnedAlphaProgram.mName = "Deferred Skinned Alpha Shader";
		gDeferredSkinnedAlphaProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedAlphaProgram.mFeatures.calculatesLighting = false;
		gDeferredSkinnedAlphaProgram.mFeatures.hasLighting = false;
		gDeferredSkinnedAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredSkinnedAlphaProgram.mFeatures.disableTextureIndex = true;
		gDeferredSkinnedAlphaProgram.mShaderFiles.clear();
		gDeferredSkinnedAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSkinnedAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredSkinnedAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredSkinnedAlphaProgram.addPermutation("USE_DIFFUSE_TEX", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("HAS_SKIN", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		success = gDeferredSkinnedAlphaProgram.createShader(NULL, NULL);
		
		// Hack to include uniforms for lighting without linking in lighting file
		gDeferredSkinnedAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredSkinnedAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredBumpProgram.mName = "Deferred Bump Shader";
		gDeferredBumpProgram.mShaderFiles.clear();
		gDeferredBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredBumpProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredBumpProgram.createShader(NULL, NULL);
	}
	
	gDeferredMaterialProgram[1].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[5].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[9].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[13].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;

	gDeferredMaterialWaterProgram[1].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[5].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[9].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[13].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;

	for (U32 i = 0; i < LLMaterial::SHADER_COUNT*2; ++i)
	{
		if (success)
		{
			gDeferredMaterialProgram[i].mName = llformat("Deferred Material Shader %d", i);
			
			U32 alpha_mode = i & 0x3;

			gDeferredMaterialProgram[i].mShaderFiles.clear();
			gDeferredMaterialProgram[i].mShaderFiles.push_back(make_pair("deferred/materialV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredMaterialProgram[i].mShaderFiles.push_back(make_pair("deferred/materialF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredMaterialProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			gDeferredMaterialProgram[i].addPermutation("HAS_NORMAL_MAP", i & 0x8? "1" : "0");
			gDeferredMaterialProgram[i].addPermutation("HAS_SPECULAR_MAP", i & 0x4 ? "1" : "0");
			gDeferredMaterialProgram[i].addPermutation("DIFFUSE_ALPHA_MODE", llformat("%d", alpha_mode));
			gDeferredMaterialProgram[i].addPermutation("HAS_SUN_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
			bool has_skin = i & 0x10;
			gDeferredMaterialProgram[i].addPermutation("HAS_SKIN",has_skin ? "1" : "0");

			if (has_skin)
			{
				gDeferredMaterialProgram[i].mFeatures.hasObjectSkinning = true;
			}

			success = gDeferredMaterialProgram[i].createShader(NULL, NULL);
		}

		if (success)
		{
			gDeferredMaterialWaterProgram[i].mName = llformat("Deferred Underwater Material Shader %d", i);

			U32 alpha_mode = i & 0x3;

			gDeferredMaterialWaterProgram[i].mShaderFiles.clear();
			gDeferredMaterialWaterProgram[i].mShaderFiles.push_back(make_pair("deferred/materialV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredMaterialWaterProgram[i].mShaderFiles.push_back(make_pair("deferred/materialF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredMaterialWaterProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			gDeferredMaterialWaterProgram[i].mShaderGroup = LLGLSLShader::SG_WATER;

			gDeferredMaterialWaterProgram[i].addPermutation("HAS_NORMAL_MAP", i & 0x8? "1" : "0");
			gDeferredMaterialWaterProgram[i].addPermutation("HAS_SPECULAR_MAP", i & 0x4 ? "1" : "0");
			gDeferredMaterialWaterProgram[i].addPermutation("DIFFUSE_ALPHA_MODE", llformat("%d", alpha_mode));
			gDeferredMaterialWaterProgram[i].addPermutation("HAS_SUN_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
			bool has_skin = i & 0x10;
			gDeferredMaterialWaterProgram[i].addPermutation("HAS_SKIN",has_skin ? "1" : "0");
			gDeferredMaterialWaterProgram[i].addPermutation("WATER_FOG","1");

			if (has_skin)
			{
				gDeferredMaterialWaterProgram[i].mFeatures.hasObjectSkinning = true;
			}

			success = gDeferredMaterialWaterProgram[i].createShader(NULL, NULL);//&mWLUniforms);
		}
	}

	gDeferredMaterialProgram[1].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[5].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[9].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[13].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;

	gDeferredMaterialWaterProgram[1].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[5].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[9].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[13].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;

	
	if (success)
	{
		gDeferredTreeProgram.mName = "Deferred Tree Shader";
		gDeferredTreeProgram.mShaderFiles.clear();
		gDeferredTreeProgram.mShaderFiles.push_back(make_pair("deferred/treeV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredTreeProgram.mShaderFiles.push_back(make_pair("deferred/treeF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredTreeProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredTreeProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredTreeShadowProgram.mName = "Deferred Tree Shadow Shader";
		gDeferredTreeShadowProgram.mShaderFiles.clear();
		gDeferredTreeShadowProgram.mShaderFiles.push_back(make_pair("deferred/treeShadowV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredTreeShadowProgram.mShaderFiles.push_back(make_pair("deferred/treeShadowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredTreeShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredTreeShadowProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredImpostorProgram.mName = "Deferred Impostor Shader";
		gDeferredImpostorProgram.mShaderFiles.clear();
		gDeferredImpostorProgram.mShaderFiles.push_back(make_pair("deferred/impostorV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredImpostorProgram.mShaderFiles.push_back(make_pair("deferred/impostorF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredImpostorProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredLightProgram.mName = "Deferred Light Shader";
		gDeferredLightProgram.mShaderFiles.clear();
		gDeferredLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredLightProgram.createShader(NULL, NULL);
	}

	for (U32 i = 0; i < LL_DEFERRED_MULTI_LIGHT_COUNT; i++)
	{
	if (success)
	{
			gDeferredMultiLightProgram[i].mName = llformat("Deferred MultiLight Shader %d", i);
			gDeferredMultiLightProgram[i].mShaderFiles.clear();
			gDeferredMultiLightProgram[i].mShaderFiles.push_back(make_pair("deferred/multiPointLightV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredMultiLightProgram[i].mShaderFiles.push_back(make_pair("deferred/multiPointLightF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredMultiLightProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			gDeferredMultiLightProgram[i].addPermutation("LIGHT_COUNT", llformat("%d", i+1));
			success = gDeferredMultiLightProgram[i].createShader(NULL, NULL);
		}
	}

	if (success)
	{
		gDeferredSpotLightProgram.mName = "Deferred SpotLight Shader";
		gDeferredSpotLightProgram.mShaderFiles.clear();
		gDeferredSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/spotLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredSpotLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSpotLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredMultiSpotLightProgram.mName = "Deferred MultiSpotLight Shader";
		gDeferredMultiSpotLightProgram.mShaderFiles.clear();
		gDeferredMultiSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/multiPointLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredMultiSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/multiSpotLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredMultiSpotLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredMultiSpotLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		std::string fragment;
		std::string vertex = "deferred/postDeferredNoTCV.glsl";

		if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{
			fragment = "deferred/sunLightSSAOF.glsl";
		}
		else
		{
			fragment = "deferred/sunLightF.glsl";
			if (mVertexShaderLevel[SHADER_DEFERRED] == 1)
			{ //no shadows, no SSAO, no frag coord
				vertex = "deferred/sunLightNoFragCoordV.glsl";
			}
		}

		gDeferredSunProgram.mName = "Deferred Sun Shader";
		gDeferredSunProgram.mShaderFiles.clear();
		gDeferredSunProgram.mShaderFiles.push_back(make_pair(vertex, GL_VERTEX_SHADER_ARB));
		gDeferredSunProgram.mShaderFiles.push_back(make_pair(fragment, GL_FRAGMENT_SHADER_ARB));
		gDeferredSunProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSunProgram.createShader(NULL, NULL);
	}

	if(gSavedSettings.getBOOL("RenderDeferredSSAO"))
	{
		if (success)
		{
			gDeferredSSAOProgram.mName = "Deferred Ambient Occlusion Shader";
			gDeferredSSAOProgram.mShaderFiles.clear();
			gDeferredSSAOProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredSSAOProgram.mShaderFiles.push_back(make_pair("deferred/SSAOF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredSSAOProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredSSAOProgram.createShader(NULL, NULL);
		}

		if (success)
		{
			gDeferredDownsampleDepthNearestProgram.mName = "Deferred Nearest Downsample Depth Shader";
			gDeferredDownsampleDepthNearestProgram.mShaderFiles.clear();
			gDeferredDownsampleDepthNearestProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredDownsampleDepthNearestProgram.mShaderFiles.push_back(make_pair("deferred/downsampleDepthNearestF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredDownsampleDepthNearestProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredDownsampleDepthNearestProgram.createShader(NULL, NULL);
		}
	}

	if (success)
	{
		gDeferredBlurLightProgram.mName = "Deferred Blur Light Shader";
		gDeferredBlurLightProgram.mShaderFiles.clear();
		gDeferredBlurLightProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredBlurLightProgram.mShaderFiles.push_back(make_pair("deferred/blurLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredBlurLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredBlurLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredAlphaProgram.mName = "Deferred Alpha Shader";

		gDeferredAlphaProgram.mFeatures.calculatesLighting = false;
		gDeferredAlphaProgram.mFeatures.hasLighting = false;
		gDeferredAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredAlphaProgram.mFeatures.disableTextureIndex = true; //hack to disable auto-setup of texture channels
		if (mVertexShaderLevel[SHADER_DEFERRED] < 1)
		{
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{ //shave off some texture units for shadow maps
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels = llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}
			
		gDeferredAlphaProgram.mShaderFiles.clear();
		gDeferredAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAlphaProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAlphaProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAlphaProgram.createShader(NULL, NULL);

		// Hack
		gDeferredAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAlphaImpostorProgram.mName = "Deferred Alpha Shader";

		gDeferredAlphaImpostorProgram.mFeatures.calculatesLighting = false;
		gDeferredAlphaImpostorProgram.mFeatures.hasLighting = false;
		gDeferredAlphaImpostorProgram.mFeatures.isAlphaLighting = true;
		gDeferredAlphaImpostorProgram.mFeatures.disableTextureIndex = true; //hack to disable auto-setup of texture channels
		if (mVertexShaderLevel[SHADER_DEFERRED] < 1)
		{
			gDeferredAlphaImpostorProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{ //shave off some texture units for shadow maps
			gDeferredAlphaImpostorProgram.mFeatures.mIndexedTextureChannels = llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}

		gDeferredAlphaImpostorProgram.mShaderFiles.clear();
		gDeferredAlphaImpostorProgram.mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAlphaImpostorProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAlphaImpostorProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaImpostorProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAlphaImpostorProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredAlphaImpostorProgram.addPermutation("FOR_IMPOSTOR", "1");

		gDeferredAlphaImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAlphaImpostorProgram.createShader(NULL, NULL);

		// Hack
		gDeferredAlphaImpostorProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaImpostorProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAlphaWaterProgram.mName = "Deferred Alpha Underwater Shader";
		gDeferredAlphaWaterProgram.mFeatures.calculatesLighting = false;
		gDeferredAlphaWaterProgram.mFeatures.hasLighting = false;
		gDeferredAlphaWaterProgram.mFeatures.isAlphaLighting = true;
		gDeferredAlphaWaterProgram.mFeatures.disableTextureIndex = true; //hack to disable auto-setup of texture channels
		if (mVertexShaderLevel[SHADER_DEFERRED] < 1)
		{
			gDeferredAlphaWaterProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{ //shave off some texture units for shadow maps
			gDeferredAlphaWaterProgram.mFeatures.mIndexedTextureChannels = llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}
		gDeferredAlphaWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredAlphaWaterProgram.mShaderFiles.clear();
		gDeferredAlphaWaterProgram.mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAlphaWaterProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAlphaWaterProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaWaterProgram.addPermutation("WATER_FOG", "1");
		gDeferredAlphaWaterProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredAlphaWaterProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAlphaWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAlphaWaterProgram.createShader(NULL, NULL);

		// Hack
		gDeferredAlphaWaterProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaWaterProgram.mFeatures.hasLighting = true;
	}
	
	if (success)
	{
		gDeferredFullbrightProgram.mName = "Deferred Fullbright Shader";
		gDeferredFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightProgram.mShaderFiles.clear();
		gDeferredFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredFullbrightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredFullbrightAlphaMaskProgram.mName = "Deferred Fullbright Alpha Masking Shader";
		gDeferredFullbrightAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightAlphaMaskProgram.mShaderFiles.clear();
		gDeferredFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredFullbrightAlphaMaskProgram.addPermutation("HAS_ALPHA_MASK","1");
		gDeferredFullbrightAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredFullbrightAlphaMaskProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredFullbrightWaterProgram.mName = "Deferred Fullbright Underwater Shader";
		gDeferredFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightWaterProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightWaterProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightWaterProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightWaterProgram.mShaderFiles.clear();
		gDeferredFullbrightWaterProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredFullbrightWaterProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredFullbrightWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredFullbrightWaterProgram.addPermutation("WATER_FOG","1");
		success = gDeferredFullbrightWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredFullbrightAlphaMaskWaterProgram.mName = "Deferred Fullbright Underwater Alpha Masking Shader";
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderFiles.clear();
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredFullbrightAlphaMaskWaterProgram.addPermutation("HAS_ALPHA_MASK","1");
		gDeferredFullbrightAlphaMaskWaterProgram.addPermutation("WATER_FOG","1");
		success = gDeferredFullbrightAlphaMaskWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredFullbrightShinyProgram.mName = "Deferred FullbrightShiny Shader";
		gDeferredFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightShinyProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightShinyProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightShinyProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels-1;
		gDeferredFullbrightShinyProgram.mShaderFiles.clear();
		gDeferredFullbrightShinyProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightShinyV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredFullbrightShinyProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredFullbrightShinyProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredSkinnedFullbrightProgram.mName = "Skinned Fullbright Shader";
		gDeferredSkinnedFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasGamma = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasTransport = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.disableTextureIndex = true;
		gDeferredSkinnedFullbrightProgram.mShaderFiles.clear();
		gDeferredSkinnedFullbrightProgram.mShaderFiles.push_back(make_pair("objects/fullbrightSkinnedV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSkinnedFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredSkinnedFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gDeferredSkinnedFullbrightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredSkinnedFullbrightShinyProgram.mName = "Skinned Fullbright Shiny Shader";
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasGamma = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasTransport = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.disableTextureIndex = true;
		gDeferredSkinnedFullbrightShinyProgram.mShaderFiles.clear();
		gDeferredSkinnedFullbrightShinyProgram.mShaderFiles.push_back(make_pair("objects/fullbrightShinySkinnedV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSkinnedFullbrightShinyProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredSkinnedFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gDeferredSkinnedFullbrightShinyProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredEmissiveProgram.mName = "Deferred Emissive Shader";
		gDeferredEmissiveProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredEmissiveProgram.mFeatures.hasGamma = true;
		gDeferredEmissiveProgram.mFeatures.hasTransport = true;
		gDeferredEmissiveProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredEmissiveProgram.mShaderFiles.clear();
		gDeferredEmissiveProgram.mShaderFiles.push_back(make_pair("deferred/emissiveV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredEmissiveProgram.mShaderFiles.push_back(make_pair("deferred/emissiveF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredEmissiveProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredEmissiveProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		// load water shader
		gDeferredWaterProgram.mName = "Deferred Water Shader";
		gDeferredWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredWaterProgram.mFeatures.hasGamma = true;
		gDeferredWaterProgram.mFeatures.hasTransport = true;
		gDeferredWaterProgram.mShaderFiles.clear();
		gDeferredWaterProgram.mShaderFiles.push_back(make_pair("deferred/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredWaterProgram.mShaderFiles.push_back(make_pair("deferred/waterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		// load water shader
		gDeferredUnderWaterProgram.mName = "Deferred Under Water Shader";
		gDeferredUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredUnderWaterProgram.mFeatures.hasGamma = true;
		gDeferredUnderWaterProgram.mFeatures.hasTransport = true;
		gDeferredUnderWaterProgram.mShaderFiles.clear();
		gDeferredUnderWaterProgram.mShaderFiles.push_back(make_pair("deferred/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredUnderWaterProgram.mShaderFiles.push_back(make_pair("deferred/underWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredUnderWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredUnderWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredSoftenProgram.mName = "Deferred Soften Shader";
		gDeferredSoftenProgram.mShaderFiles.clear();
		gDeferredSoftenProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSoftenProgram.mShaderFiles.push_back(make_pair("deferred/softenLightF.glsl", GL_FRAGMENT_SHADER_ARB));

		gDeferredSoftenProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{ //if using SSAO, take screen space light map into account as if shadows are enabled
			gDeferredSoftenProgram.mShaderLevel = llmax(gDeferredSoftenProgram.mShaderLevel, 2);
		}
				
		success = gDeferredSoftenProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredSoftenWaterProgram.mName = "Deferred Soften Underwater Shader";
		gDeferredSoftenWaterProgram.mShaderFiles.clear();
		gDeferredSoftenWaterProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSoftenWaterProgram.mShaderFiles.push_back(make_pair("deferred/softenLightF.glsl", GL_FRAGMENT_SHADER_ARB));

		gDeferredSoftenWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredSoftenWaterProgram.addPermutation("WATER_FOG", "1");
		gDeferredSoftenWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;

		if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{ //if using SSAO, take screen space light map into account as if shadows are enabled
			gDeferredSoftenWaterProgram.mShaderLevel = llmax(gDeferredSoftenWaterProgram.mShaderLevel, 2);
		}

		success = gDeferredSoftenWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredShadowProgram.mName = "Deferred Shadow Shader";
		gDeferredShadowProgram.mShaderFiles.clear();
		gDeferredShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredShadowProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredShadowCubeProgram.mName = "Deferred Shadow Cube Shader";
		gDeferredShadowCubeProgram.mShaderFiles.clear();
		gDeferredShadowCubeProgram.mShaderFiles.push_back(make_pair("deferred/shadowCubeV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredShadowCubeProgram.mShaderFiles.push_back(make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredShadowCubeProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredShadowCubeProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredShadowAlphaMaskProgram.mName = "Deferred Shadow Alpha Mask Shader";
		gDeferredShadowAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredShadowAlphaMaskProgram.mShaderFiles.clear();
		gDeferredShadowAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/shadowAlphaMaskV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredShadowAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/shadowAlphaMaskF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredShadowAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredShadowAlphaMaskProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredAvatarShadowProgram.mName = "Deferred Avatar Shadow Shader";
		gDeferredAvatarShadowProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarShadowProgram.mShaderFiles.clear();
		gDeferredAvatarShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarShadowV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAvatarShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarShadowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAvatarShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarShadowProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredAttachmentShadowProgram.mName = "Deferred Attachment Shadow Shader";
		gDeferredAttachmentShadowProgram.mFeatures.hasObjectSkinning = true;
		gDeferredAttachmentShadowProgram.mShaderFiles.clear();
		gDeferredAttachmentShadowProgram.mShaderFiles.push_back(make_pair("deferred/attachmentShadowV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAttachmentShadowProgram.mShaderFiles.push_back(make_pair("deferred/attachmentShadowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAttachmentShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAttachmentShadowProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gTerrainProgram.mName = "Deferred Terrain Shader";
		gDeferredTerrainProgram.mShaderFiles.clear();
		gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredTerrainProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredTerrainProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredAvatarProgram.mName = "Avatar Shader";
		gDeferredAvatarProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarProgram.mShaderFiles.clear();
		gDeferredAvatarProgram.mShaderFiles.push_back(make_pair("deferred/avatarV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAvatarProgram.mShaderFiles.push_back(make_pair("deferred/avatarF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAvatarProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredAvatarAlphaProgram.mName = "Avatar Alpha Shader";
		gDeferredAvatarAlphaProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = false;
		gDeferredAvatarAlphaProgram.mFeatures.hasLighting = false;
		gDeferredAvatarAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredAvatarAlphaProgram.mFeatures.disableTextureIndex = true;
		gDeferredAvatarAlphaProgram.mShaderFiles.clear();
		gDeferredAvatarAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAvatarAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAvatarAlphaProgram.addPermutation("USE_DIFFUSE_TEX", "1");
		gDeferredAvatarAlphaProgram.addPermutation("IS_AVATAR_SKIN", "1");
		gDeferredAvatarAlphaProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAvatarAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAvatarAlphaProgram.createShader(NULL, NULL);

		gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAvatarAlphaProgram.mFeatures.hasLighting = true;
	}
	
	if (success)
	{
		gDeferredPostGammaCorrectProgram.mName = "Deferred Gamma Correction Post Process";
		gDeferredPostGammaCorrectProgram.mShaderFiles.clear();
		gDeferredPostGammaCorrectProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredPostGammaCorrectProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredGammaCorrectF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredPostGammaCorrectProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostGammaCorrectProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gFXAAProgram.mName = "FXAA Shader";
		gFXAAProgram.mShaderFiles.clear();
		gFXAAProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gFXAAProgram.mShaderFiles.push_back(make_pair("deferred/fxaaF.glsl", GL_FRAGMENT_SHADER_ARB));
		static LLCachedControl<uint32_t> fxaaQuality("FXAAQuality", 12);
		gFXAAProgram.addPermutation("FXAA_QUALITY_M_PRESET", std::to_string(fxaaQuality.get()));
		gFXAAProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gFXAAProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredPostProgram.mName = "Deferred Post Shader";
		gDeferredPostProgram.mShaderFiles.clear();
		gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredPostProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredCoFProgram.mName = "Deferred CoF Shader";
		gDeferredCoFProgram.mShaderFiles.clear();
		gDeferredCoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredCoFProgram.mShaderFiles.push_back(make_pair("deferred/cofF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredCoFProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredCoFProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredDoFCombineProgram.mName = "Deferred DoFCombine Shader";
		gDeferredDoFCombineProgram.mShaderFiles.clear();
		gDeferredDoFCombineProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredDoFCombineProgram.mShaderFiles.push_back(make_pair("deferred/dofCombineF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredDoFCombineProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDoFCombineProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredPostNoDoFProgram.mName = "Deferred Post Shader";
		gDeferredPostNoDoFProgram.mShaderFiles.clear();
		gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoDoFF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredPostNoDoFProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostNoDoFProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredWLSkyProgram.mName = "Deferred Windlight Sky Shader";
		//gWLSkyProgram.mFeatures.hasGamma = true;
		gDeferredWLSkyProgram.mShaderFiles.clear();
		gDeferredWLSkyProgram.mShaderFiles.push_back(make_pair("deferred/skyV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredWLSkyProgram.mShaderFiles.push_back(make_pair("deferred/skyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredWLSkyProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredWLSkyProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredWLCloudProgram.mName = "Deferred Windlight Cloud Program";
		gDeferredWLCloudProgram.mShaderFiles.clear();
		gDeferredWLCloudProgram.mShaderFiles.push_back(make_pair("deferred/cloudsV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredWLCloudProgram.mShaderFiles.push_back(make_pair("deferred/cloudsF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredWLCloudProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredWLCloudProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredStarProgram.mName = "Deferred Star Program";
		static std::vector<LLStaticHashedString> shaderUniforms;
		if(shaderUniforms.empty())
		{
			shaderUniforms.push_back(LLStaticHashedString("custom_alpha"));
		}
		gDeferredStarProgram.mShaderFiles.clear();
		gDeferredStarProgram.mShaderFiles.push_back(make_pair("deferred/starsV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredStarProgram.mShaderFiles.push_back(make_pair("deferred/starsF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredStarProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredStarProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredStarProgram.createShader(NULL, &shaderUniforms);
	}

	if (success)
	{
		gNormalMapGenProgram.mName = "Normal Map Generation Program";
		gNormalMapGenProgram.mShaderFiles.clear();
		gNormalMapGenProgram.mShaderFiles.push_back(make_pair("deferred/normgenV.glsl", GL_VERTEX_SHADER_ARB));
		gNormalMapGenProgram.mShaderFiles.push_back(make_pair("deferred/normgenF.glsl", GL_FRAGMENT_SHADER_ARB));
		gNormalMapGenProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gNormalMapGenProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gNormalMapGenProgram.createShader(NULL, NULL);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_DEFERRED] = 0;
		unloadShaderClass(SHADER_DEFERRED);
	}

	return success;
}

BOOL LLViewerShaderMgr::loadShadersObject()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_OBJECT] == 0)
	{
		unloadShaderClass(SHADER_OBJECT);
		return TRUE;
	}
	
	if (success)
	{
		gObjectSimpleNonIndexedTexGenProgram.mName = "Non indexed tex-gen Shader";
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasGamma = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedTexGenProgram.mShaderFiles.clear();
		gObjectSimpleNonIndexedTexGenProgram.mShaderFiles.push_back(make_pair("objects/simpleTexGenV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectSimpleNonIndexedTexGenProgram.mShaderFiles.push_back(make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectSimpleNonIndexedTexGenProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectSimpleNonIndexedTexGenProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gObjectSimpleNonIndexedTexGenWaterProgram.mName = "Non indexed tex-gen Water Shader";
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderFiles.clear();
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleTexGenV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleNonIndexedTexGenWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectAlphaMaskNoColorProgram.mName = "No color alpha mask Shader";
		gObjectAlphaMaskNoColorProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasGamma = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNoColorProgram.mShaderFiles.clear();
		gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("objects/simpleNoColorV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectAlphaMaskNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectAlphaMaskNoColorProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gObjectAlphaMaskNoColorWaterProgram.mName = "No color alpha mask Water Shader";
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasWaterFog = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNoColorWaterProgram.mShaderFiles.clear();
		gObjectAlphaMaskNoColorWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleNoColorV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectAlphaMaskNoColorWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectAlphaMaskNoColorWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectAlphaMaskNoColorWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectAlphaMaskNoColorWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gTreeProgram.mName = "Tree Shader";
		gTreeProgram.mFeatures.calculatesLighting = true;
		gTreeProgram.mFeatures.calculatesAtmospherics = true;
		gTreeProgram.mFeatures.hasGamma = true;
		gTreeProgram.mFeatures.hasAtmospherics = true;
		gTreeProgram.mFeatures.hasLighting = true;
		gTreeProgram.mFeatures.disableTextureIndex = true;
		gTreeProgram.mFeatures.hasAlphaMask = true;
		gTreeProgram.mShaderFiles.clear();
		gTreeProgram.mShaderFiles.push_back(make_pair("objects/treeV.glsl", GL_VERTEX_SHADER_ARB));
		gTreeProgram.mShaderFiles.push_back(make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTreeProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gTreeProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gTreeWaterProgram.mName = "Tree Water Shader";
		gTreeWaterProgram.mFeatures.calculatesLighting = true;
		gTreeWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTreeWaterProgram.mFeatures.hasWaterFog = true;
		gTreeWaterProgram.mFeatures.hasAtmospherics = true;
		gTreeWaterProgram.mFeatures.hasLighting = true;
		gTreeWaterProgram.mFeatures.disableTextureIndex = true;
		gTreeWaterProgram.mFeatures.hasAlphaMask = true;
		gTreeWaterProgram.mShaderFiles.clear();
		gTreeWaterProgram.mShaderFiles.push_back(make_pair("objects/treeV.glsl", GL_VERTEX_SHADER_ARB));
		gTreeWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTreeWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gTreeWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gTreeWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectFullbrightNoColorProgram.mName = "Non Indexed no color Fullbright Shader";
		gObjectFullbrightNoColorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNoColorProgram.mFeatures.hasGamma = true;
		gObjectFullbrightNoColorProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNoColorProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNoColorProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightNoColorProgram.mShaderFiles.clear();
		gObjectFullbrightNoColorProgram.mShaderFiles.push_back(make_pair("objects/fullbrightNoColorV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightNoColorProgram.mShaderFiles.push_back(make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightNoColorProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectFullbrightNoColorWaterProgram.mName = "Non Indexed no color Fullbright Water Shader";
		gObjectFullbrightNoColorWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectFullbrightNoColorWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightNoColorWaterProgram.mShaderFiles.clear();
		gObjectFullbrightNoColorWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightNoColorV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightNoColorWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightNoColorWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightNoColorWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightNoColorWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gImpostorProgram.mName = "Impostor Shader";
		gImpostorProgram.mFeatures.disableTextureIndex = true;
		gImpostorProgram.mShaderFiles.clear();
		gImpostorProgram.mShaderFiles.push_back(make_pair("objects/impostorV.glsl", GL_VERTEX_SHADER_ARB));
		gImpostorProgram.mShaderFiles.push_back(make_pair("objects/impostorF.glsl", GL_FRAGMENT_SHADER_ARB));
		gImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gImpostorProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectPreviewProgram.mName = "Simple Shader";
		gObjectPreviewProgram.mFeatures.calculatesLighting = false;
		gObjectPreviewProgram.mFeatures.calculatesAtmospherics = false;
		gObjectPreviewProgram.mFeatures.hasGamma = false;
		gObjectPreviewProgram.mFeatures.hasAtmospherics = false;
		gObjectPreviewProgram.mFeatures.hasLighting = false;
		gObjectPreviewProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectPreviewProgram.mFeatures.disableTextureIndex = true;
		gObjectPreviewProgram.mShaderFiles.clear();
		gObjectPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectPreviewProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectPreviewProgram.createShader(NULL, NULL);
		gObjectPreviewProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		for(U32 i = 0; i < (U32)1<<SHD_COUNT; ++i)
		{
			bool mask = i & 1<<SHD_ALPHA_MASK_BIT;
			bool fog = i & 1<<SHD_WATER_BIT;
			bool no_index = i & 1<<SHD_NO_INDEX_BIT;
			bool skin = i & 1<<SHD_SKIN_BIT;
			bool shiny = i & 1<<SHD_SHINY_BIT;

			std::string name;
			if(shiny) name += " Shiny";
			if(skin) name += " Skinned";
			if(no_index) name += " Non-Indexed";
			if(mask) name += " Alpha Mask";	
			if(fog) name += " Water";

			if(no_index && !mask)
			{
				continue;	//all non-indexed shaders here have alphamasks;
			}

			if(skin && (!no_index || mVertexShaderLevel[SHADER_AVATAR] < 1))
			{
				continue;	//Skins are always non-indexed.
			}

			if(shiny && mask && !skin && !no_index)
			{
				continue;	//non-skinned indexed shiny does not support alphamasking
			}



			LLShaderFeatures features;
			features.calculatesLighting = true;
			features.calculatesAtmospherics = true;
			features.hasAtmospherics = true;
			features.hasLighting = !shiny;
			features.mIndexedTextureChannels = 0;
			features.hasGamma = !fog;
			features.hasWaterFog = fog;
			features.hasAlphaMask = mask;
			features.disableTextureIndex = no_index;
			features.hasObjectSkinning = skin;
			features.isShiny = shiny;

			gObjectSimpleProgram[i].mName = std::string("Simple") + name + " Shader";
			gObjectSimpleProgram[i].mFeatures = features;
			gObjectSimpleProgram[i].mShaderFiles.clear();
			if(!shiny)
			{
				gObjectSimpleProgram[i].mShaderFiles.push_back(make_pair(skin ? "objects/simpleSkinnedV.glsl" : "objects/simpleV.glsl", GL_VERTEX_SHADER_ARB));
				gObjectSimpleProgram[i].mShaderFiles.push_back(make_pair(fog ? "objects/simpleWaterF.glsl" : "objects/simpleF.glsl", GL_FRAGMENT_SHADER_ARB));
			}
			else
			{
				gObjectSimpleProgram[i].mShaderFiles.push_back(make_pair(skin ? "objects/shinySimpleSkinnedV.glsl" : "objects/shinyV.glsl", GL_VERTEX_SHADER_ARB));
				gObjectSimpleProgram[i].mShaderFiles.push_back(make_pair(fog ? "objects/shinyWaterF.glsl" : "objects/shinyF.glsl", GL_FRAGMENT_SHADER_ARB));
			}
			gObjectSimpleProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			if(fog)
				gObjectSimpleProgram[i].mShaderGroup = LLGLSLShader::SG_WATER;
			if(!(success = gObjectSimpleProgram[i].createShader(NULL, NULL)))
				break;

			features.calculatesLighting = false;
			features.hasLighting = false;
			features.hasTransport = true;
			features.hasAtmospherics = false;
			features.isFullbright = true;
			features.hasGamma |= (shiny && fog);
			
			gObjectFullbrightProgram[i].mName = std::string("Fullbright") + name + " Shader";
			gObjectFullbrightProgram[i].mFeatures = features;
			gObjectFullbrightProgram[i].mShaderFiles.clear();
			if(!shiny)
			{
				gObjectFullbrightProgram[i].mShaderFiles.push_back(make_pair(skin ? "objects/fullbrightSkinnedV.glsl" : "objects/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
				gObjectFullbrightProgram[i].mShaderFiles.push_back(make_pair(fog ? "objects/fullbrightWaterF.glsl" : "objects/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
			}
			else
			{
				gObjectFullbrightProgram[i].mShaderFiles.push_back(make_pair(skin ? "objects/fullbrightShinySkinnedV.glsl" : "objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER_ARB));
				gObjectFullbrightProgram[i].mShaderFiles.push_back(make_pair(fog ? "objects/fullbrightShinyWaterF.glsl" : "objects/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER_ARB));
			}
			gObjectFullbrightProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			if(fog)
				gObjectFullbrightProgram[i].mShaderGroup = LLGLSLShader::SG_WATER;
			if(!(success = gObjectFullbrightProgram[i].createShader(NULL, NULL)))
				break;

			if(!shiny)
			{
				gObjectEmissiveProgram[i].mName = std::string("Emissive") + name + " Shader";
				gObjectEmissiveProgram[i].mFeatures = features;
				gObjectEmissiveProgram[i].mShaderFiles.clear();
				gObjectEmissiveProgram[i].mShaderFiles.push_back(make_pair(skin ? "objects/emissiveSkinnedV.glsl" : "objects/emissiveV.glsl", GL_VERTEX_SHADER_ARB));
				gObjectEmissiveProgram[i].mShaderFiles.push_back(make_pair(fog ? "objects/fullbrightWaterF.glsl" : "objects/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
				gObjectEmissiveProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
				if(fog)
					gObjectEmissiveProgram[i].mShaderGroup = LLGLSLShader::SG_WATER;
				if(!(success = gObjectEmissiveProgram[i].createShader(NULL, NULL)))
					break;
			}
		}
	}

	if (success)
	{
		gObjectBumpProgram.mName = "Bump Shader";
		/*gObjectBumpProgram.mFeatures.calculatesLighting = true;
		gObjectBumpProgram.mFeatures.calculatesAtmospherics = true;
		gObjectBumpProgram.mFeatures.hasGamma = true;
		gObjectBumpProgram.mFeatures.hasAtmospherics = true;
		gObjectBumpProgram.mFeatures.hasLighting = true;
		gObjectBumpProgram.mFeatures.mIndexedTextureChannels = 0;*/
		gObjectBumpProgram.mShaderFiles.clear();
		gObjectBumpProgram.mShaderFiles.push_back(make_pair("objects/bumpV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectBumpProgram.mShaderFiles.push_back(make_pair("objects/bumpF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectBumpProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectBumpProgram.createShader(NULL, NULL);
		if (success)
		{ //lldrawpoolbump assumes "texture0" has channel 0 and "texture1" has channel 1
			gObjectBumpProgram.bind();
			gObjectBumpProgram.uniform1i(sTexture0, 0);
			gObjectBumpProgram.uniform1i(sTexture1, 1);
			gObjectBumpProgram.unbind();
		}
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_OBJECT] = 0;
		unloadShaderClass(SHADER_OBJECT);
	}
	
	return success;
}

BOOL LLViewerShaderMgr::loadShadersAvatar()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_AVATAR] == 0)
	{
		unloadShaderClass(SHADER_AVATAR);
		return TRUE;
	}

	if (success)
	{
		gAvatarProgram.mName = "Avatar Shader";
		gAvatarProgram.mFeatures.hasSkinning = true;
		gAvatarProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarProgram.mFeatures.calculatesLighting = true;
		gAvatarProgram.mFeatures.hasGamma = true;
		gAvatarProgram.mFeatures.hasAtmospherics = true;
		gAvatarProgram.mFeatures.hasLighting = true;
		gAvatarProgram.mFeatures.hasAlphaMask = true;
		gAvatarProgram.mFeatures.disableTextureIndex = true;
		gAvatarProgram.mShaderFiles.clear();
		gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER_ARB));
		gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAvatarProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarProgram.createShader(NULL, NULL);
			
		if (success)
		{
			gAvatarWaterProgram.mName = "Avatar Water Shader";
			gAvatarWaterProgram.mFeatures.hasSkinning = true;
			gAvatarWaterProgram.mFeatures.calculatesAtmospherics = true;
			gAvatarWaterProgram.mFeatures.calculatesLighting = true;
			gAvatarWaterProgram.mFeatures.hasWaterFog = true;
			gAvatarWaterProgram.mFeatures.hasAtmospherics = true;
			gAvatarWaterProgram.mFeatures.hasLighting = true;
			gAvatarWaterProgram.mFeatures.hasAlphaMask = true;
			gAvatarWaterProgram.mFeatures.disableTextureIndex = true;
			gAvatarWaterProgram.mShaderFiles.clear();
			gAvatarWaterProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER_ARB));
			gAvatarWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
			// Note: no cloth under water:
			gAvatarWaterProgram.mShaderLevel = llmin(mVertexShaderLevel[SHADER_AVATAR], 1);	
			gAvatarWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;				
			success = gAvatarWaterProgram.createShader(NULL, NULL);
		}
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_AVATAR] = 0;
		unloadShaderClass(SHADER_AVATAR);
	}
	
	return success;
}

BOOL LLViewerShaderMgr::loadShadersInterface()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_INTERFACE] == 0)
	{
		unloadShaderClass(SHADER_INTERFACE);
		return TRUE;
	}
	
	if (success)
	{
		gHighlightProgram.mName = "Highlight Shader";
		gHighlightProgram.mShaderFiles.clear();
		gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightV.glsl", GL_VERTEX_SHADER_ARB));
		gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gHighlightProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gHighlightNormalProgram.mName = "Highlight Normals Shader";
		gHighlightNormalProgram.mShaderFiles.clear();
		gHighlightNormalProgram.mShaderFiles.push_back(make_pair("interface/highlightNormV.glsl", GL_VERTEX_SHADER_ARB));
		gHighlightNormalProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gHighlightNormalProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightNormalProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gHighlightSpecularProgram.mName = "Highlight Spec Shader";
		gHighlightSpecularProgram.mShaderFiles.clear();
		gHighlightSpecularProgram.mShaderFiles.push_back(make_pair("interface/highlightSpecV.glsl", GL_VERTEX_SHADER_ARB));
		gHighlightSpecularProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gHighlightSpecularProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightSpecularProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gUIProgram.mName = "UI Shader";
		gUIProgram.mShaderFiles.clear();
		gUIProgram.mShaderFiles.push_back(make_pair("interface/uiV.glsl", GL_VERTEX_SHADER_ARB));
		gUIProgram.mShaderFiles.push_back(make_pair("interface/uiF.glsl", GL_FRAGMENT_SHADER_ARB));
		gUIProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gUIProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gCustomAlphaProgram.mName = "Custom Alpha Shader";
		gCustomAlphaProgram.mShaderFiles.clear();
		gCustomAlphaProgram.mShaderFiles.push_back(make_pair("interface/customalphaV.glsl", GL_VERTEX_SHADER_ARB));
		gCustomAlphaProgram.mShaderFiles.push_back(make_pair("interface/customalphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gCustomAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gCustomAlphaProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gSplatTextureRectProgram.mName = "Splat Texture Rect Shader";
		gSplatTextureRectProgram.mShaderFiles.clear();
		gSplatTextureRectProgram.mShaderFiles.push_back(make_pair("interface/splattexturerectV.glsl", GL_VERTEX_SHADER_ARB));
		gSplatTextureRectProgram.mShaderFiles.push_back(make_pair("interface/splattexturerectF.glsl", GL_FRAGMENT_SHADER_ARB));
		gSplatTextureRectProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gSplatTextureRectProgram.createShader(NULL, NULL);
		if (success)
		{
			gSplatTextureRectProgram.bind();
			gSplatTextureRectProgram.uniform1i(sScreenMap, 0);
			gSplatTextureRectProgram.unbind();
		}
	}

	if (success)
	{
		gGlowCombineProgram.mName = "Glow Combine Shader";
		gGlowCombineProgram.mShaderFiles.clear();
		gGlowCombineProgram.mShaderFiles.push_back(make_pair("interface/glowcombineV.glsl", GL_VERTEX_SHADER_ARB));
		gGlowCombineProgram.mShaderFiles.push_back(make_pair("interface/glowcombineF.glsl", GL_FRAGMENT_SHADER_ARB));
		gGlowCombineProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gGlowCombineProgram.createShader(NULL, NULL);
		if (success)
		{
			gGlowCombineProgram.bind();
			gGlowCombineProgram.uniform1i(sGlowMap, 0);
			gGlowCombineProgram.uniform1i(sScreenMap, 1);
			gGlowCombineProgram.unbind();
		}
	}

	if (success)
	{
		gGlowCombineFXAAProgram.mName = "Glow CombineFXAA Shader";
		gGlowCombineFXAAProgram.mShaderFiles.clear();
		gGlowCombineFXAAProgram.mShaderFiles.push_back(make_pair("interface/glowcombineFXAAV.glsl", GL_VERTEX_SHADER_ARB));
		gGlowCombineFXAAProgram.mShaderFiles.push_back(make_pair("interface/glowcombineFXAAF.glsl", GL_FRAGMENT_SHADER_ARB));
		gGlowCombineFXAAProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gGlowCombineFXAAProgram.createShader(NULL, NULL);
		if (success)
		{
			gGlowCombineFXAAProgram.bind();
			gGlowCombineFXAAProgram.uniform1i(sGlowMap, 0);
			gGlowCombineFXAAProgram.uniform1i(sScreenMap, 1);
			gGlowCombineFXAAProgram.unbind();
		}
	}


	if (success)
	{
		gTwoTextureAddProgram.mName = "Two Texture Add Shader";
		gTwoTextureAddProgram.mShaderFiles.clear();
		gTwoTextureAddProgram.mShaderFiles.push_back(make_pair("interface/twotextureaddV.glsl", GL_VERTEX_SHADER_ARB));
		gTwoTextureAddProgram.mShaderFiles.push_back(make_pair("interface/twotextureaddF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTwoTextureAddProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gTwoTextureAddProgram.createShader(NULL, NULL);
		if (success)
		{
			gTwoTextureAddProgram.bind();
			gTwoTextureAddProgram.uniform1i(sTex0, 0);
			gTwoTextureAddProgram.uniform1i(sTex1, 1);
		}
	}

	if (success)
	{
		gOneTextureNoColorProgram.mName = "One Texture No Color Shader";
		gOneTextureNoColorProgram.mShaderFiles.clear();
		gOneTextureNoColorProgram.mShaderFiles.push_back(make_pair("interface/onetexturenocolorV.glsl", GL_VERTEX_SHADER_ARB));
		gOneTextureNoColorProgram.mShaderFiles.push_back(make_pair("interface/onetexturenocolorF.glsl", GL_FRAGMENT_SHADER_ARB));
		gOneTextureNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gOneTextureNoColorProgram.createShader(NULL, NULL);
		if (success)
		{
			gOneTextureNoColorProgram.bind();
			gOneTextureNoColorProgram.uniform1i(sTex0, 0);
		}
	}

	if (success)
	{
		gSolidColorProgram.mName = "Solid Color Shader";
		gSolidColorProgram.mShaderFiles.clear();
#if LL_WINDOWS
		if(gGLManager.mIsIntel && gGLManager.mGLVersion >= 4.f)
			gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorIntelV.glsl", GL_VERTEX_SHADER_ARB));
		else
#endif
		gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorV.glsl", GL_VERTEX_SHADER_ARB));
		gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorF.glsl", GL_FRAGMENT_SHADER_ARB));
		gSolidColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gSolidColorProgram.createShader(NULL, NULL);
		if (success)
		{
			gSolidColorProgram.bind();
			gSolidColorProgram.uniform1i(sTex0, 0);
			gSolidColorProgram.unbind();
		}
	}

	if (success)
	{
		gOcclusionProgram.mName = "Occlusion Shader";
		gOcclusionProgram.mShaderFiles.clear();
		gOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionV.glsl", GL_VERTEX_SHADER_ARB));
		gOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER_ARB));
		gOcclusionProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gOcclusionProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gOcclusionCubeProgram.mName = "Occlusion Cube Shader";
		gOcclusionCubeProgram.mShaderFiles.clear();
		gOcclusionCubeProgram.mShaderFiles.push_back(make_pair("interface/occlusionCubeV.glsl", GL_VERTEX_SHADER_ARB));
		gOcclusionCubeProgram.mShaderFiles.push_back(make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER_ARB));
		gOcclusionCubeProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gOcclusionCubeProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDebugProgram.mName = "Debug Shader";
		gDebugProgram.mShaderFiles.clear();
		gDebugProgram.mShaderFiles.push_back(make_pair("interface/debugV.glsl", GL_VERTEX_SHADER_ARB));
		gDebugProgram.mShaderFiles.push_back(make_pair("interface/debugF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDebugProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gDebugProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gClipProgram.mName = "Clip Shader";
		gClipProgram.mShaderFiles.clear();
		gClipProgram.mShaderFiles.push_back(make_pair("interface/clipV.glsl", GL_VERTEX_SHADER_ARB));
		gClipProgram.mShaderFiles.push_back(make_pair("interface/clipF.glsl", GL_FRAGMENT_SHADER_ARB));
		gClipProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gClipProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDownsampleDepthProgram.mName = "DownsampleDepth Shader";
		gDownsampleDepthProgram.mShaderFiles.clear();
		gDownsampleDepthProgram.mShaderFiles.push_back(make_pair("interface/downsampleDepthV.glsl", GL_VERTEX_SHADER_ARB));
		gDownsampleDepthProgram.mShaderFiles.push_back(make_pair("interface/downsampleDepthF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDownsampleDepthProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gDownsampleDepthProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gAlphaMaskProgram.mName = "Alpha Mask Shader";
		gAlphaMaskProgram.mShaderFiles.clear();
		gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskV.glsl", GL_VERTEX_SHADER_ARB));
		gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gAlphaMaskProgram.createShader(NULL, NULL);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_INTERFACE] = 0;
		unloadShaderClass(SHADER_INTERFACE);
	}

	return success;
}

BOOL LLViewerShaderMgr::loadShadersWindLight()
{	
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_WINDLIGHT] < 2)
	{
		unloadShaderClass(SHADER_WINDLIGHT);
		return TRUE;
	}

	if (success)
	{
		gWLSkyProgram.mName = "Windlight Sky Shader";
		//gWLSkyProgram.mFeatures.hasGamma = true;
		gWLSkyProgram.mShaderFiles.clear();
		gWLSkyProgram.mShaderFiles.push_back(make_pair("windlight/skyV.glsl", GL_VERTEX_SHADER_ARB));
		gWLSkyProgram.mShaderFiles.push_back(make_pair("windlight/skyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWLSkyProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLSkyProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gWLCloudProgram.mName = "Windlight Cloud Program";
		//gWLCloudProgram.mFeatures.hasGamma = true;
		gWLCloudProgram.mShaderFiles.clear();
		gWLCloudProgram.mShaderFiles.push_back(make_pair("windlight/cloudsV.glsl", GL_VERTEX_SHADER_ARB));
		gWLCloudProgram.mShaderFiles.push_back(make_pair("windlight/cloudsF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWLCloudProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLCloudProgram.createShader(NULL, NULL);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_WINDLIGHT] = 0;
		unloadShaderClass(SHADER_WINDLIGHT);
	}

	return success;
}

BOOL LLViewerShaderMgr::loadTransformShaders()
{
	BOOL success = TRUE;
	
	if (mVertexShaderLevel[SHADER_TRANSFORM] < 1)
	{
		unloadShaderClass(SHADER_TRANSFORM);
		return TRUE;
	}

	if (success)
	{
		gTransformPositionProgram.mName = "Position Transform Shader";
		gTransformPositionProgram.mShaderFiles.clear();
		gTransformPositionProgram.mShaderFiles.push_back(make_pair("transform/positionV.glsl", GL_VERTEX_SHADER_ARB));
		gTransformPositionProgram.mShaderLevel = mVertexShaderLevel[SHADER_TRANSFORM];

		const char* varyings[] = {
			"position_out",
			"texture_index_out",
		};
	
		success = gTransformPositionProgram.createShader(NULL, NULL, 2, varyings);
	}

	if (success)
	{
		gTransformTexCoordProgram.mName = "TexCoord Transform Shader";
		gTransformTexCoordProgram.mShaderFiles.clear();
		gTransformTexCoordProgram.mShaderFiles.push_back(make_pair("transform/texcoordV.glsl", GL_VERTEX_SHADER_ARB));
		gTransformTexCoordProgram.mShaderLevel = mVertexShaderLevel[SHADER_TRANSFORM];

		const char* varyings[] = {
			"texcoord_out",
		};
	
		success = gTransformTexCoordProgram.createShader(NULL, NULL, 1, varyings);
	}

	if (success)
	{
		gTransformNormalProgram.mName = "Normal Transform Shader";
		gTransformNormalProgram.mShaderFiles.clear();
		gTransformNormalProgram.mShaderFiles.push_back(make_pair("transform/normalV.glsl", GL_VERTEX_SHADER_ARB));
		gTransformNormalProgram.mShaderLevel = mVertexShaderLevel[SHADER_TRANSFORM];

		const char* varyings[] = {
			"normal_out",
		};
	
		success = gTransformNormalProgram.createShader(NULL, NULL, 1, varyings);
	}

	if (success)
	{
		gTransformColorProgram.mName = "Color Transform Shader";
		gTransformColorProgram.mShaderFiles.clear();
		gTransformColorProgram.mShaderFiles.push_back(make_pair("transform/colorV.glsl", GL_VERTEX_SHADER_ARB));
		gTransformColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_TRANSFORM];

		const char* varyings[] = {
			"color_out",
		};
	
		success = gTransformColorProgram.createShader(NULL, NULL, 1, varyings);
	}

	if (success)
	{
		gTransformTangentProgram.mName = "Binormal Transform Shader";
		gTransformTangentProgram.mShaderFiles.clear();
		gTransformTangentProgram.mShaderFiles.push_back(make_pair("transform/binormalV.glsl", GL_VERTEX_SHADER_ARB));
		gTransformTangentProgram.mShaderLevel = mVertexShaderLevel[SHADER_TRANSFORM];

		const char* varyings[] = {
			"tangent_out",
		};
	
		success = gTransformTangentProgram.createShader(NULL, NULL, 1, varyings);
	}


	if (!success)
	{
		mVertexShaderLevel[SHADER_TRANSFORM] = 0;
		unloadShaderClass(SHADER_TRANSFORM);
	}
	
	return success;
}

std::string LLViewerShaderMgr::getShaderDirPrefix(void)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders/class");
}

void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
	LLWLParamManager::getInstance()->updateShaderUniforms(shader);
	LLWaterParamManager::getInstance()->updateShaderUniforms(shader);
}

/* virtual */ bool LLViewerShaderMgr::attachClassSharedShaders(LLGLSLShader& shader, S32 shader_class)
{
	switch (shader_class)
	{
	case LLViewerShaderMgr::SHADER_DEFERRED:
		LL_INFOS() << "deferred/components/utilityFuncF.glsl" << LL_ENDL;
		return shader.attachObject("deferred/components/utilityFuncF.glsl");
	}
	return true;
}

/*static*/ void LLShaderMgr::unloadShaderClass(int shader_class)
{
	std::vector<LLGLSLShader *> &shader_list = getGlobalShaderList();
	for(std::vector<LLGLSLShader *>::iterator it=shader_list.begin();it!=shader_list.end();++it)
	{
		if((*it)->mShaderClass == shader_class)
			(*it)->unload();
	}
}
/*static*/ std::vector<LLGLSLShader *> &LLShaderMgr::getGlobalShaderList()
{
	static std::vector<LLGLSLShader *> sGlbShaderLst;
	return sGlbShaderLst;
}
