/** 
 * @file lldrawpoolwater.cpp
 * @brief LLDrawPoolWater class implementation
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

#include "llviewerprecompiledheaders.h"
#include "llfeaturemanager.h"
#include "lldrawpoolwater.h"

#include "llviewercontrol.h"
#include "lldir.h"
#include "llerror.h"
#include "m3math.h"
#include "llrender.h"

#include "llagent.h"		// for gAgent for getRegion for getWaterHeight
#include "llcubemap.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "llviewertexturelist.h"
#include "llviewerregion.h"
#include "llvosky.h"
#include "llvowater.h"
#include "llworld.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llwaterparammanager.h"
#include "llappviewer.h"
#include "lltexturecache.h"

const LLUUID TRANSPARENT_WATER_TEXTURE("2bfd3884-7e27-69b9-ba3a-3e673f680004");
const LLUUID OPAQUE_WATER_TEXTURE("43c32285-d658-1793-c123-bf86315de055");

static float sTime;

BOOL deferred_render = FALSE;

BOOL LLDrawPoolWater::sNeedsReflectionUpdate = TRUE;
BOOL LLDrawPoolWater::sNeedsDistortionUpdate = TRUE;
LLColor4 LLDrawPoolWater::sWaterFogColor = LLColor4(0.2f, 0.5f, 0.5f, 0.f);
F32 LLDrawPoolWater::sWaterFogEnd = 0.f;

LLVector3 LLDrawPoolWater::sLightDir;

LLDrawPoolWater::LLDrawPoolWater() :
	LLFacePool(POOL_WATER)
{
	mHBTex[0] = LLViewerTextureManager::getFetchedTexture(gSunTextureID, FTT_DEFAULT, TRUE, LLGLTexture::BOOST_UI);
	gGL.getTexUnit(0)->bind(mHBTex[0]) ;
	mHBTex[0]->setAddressMode(LLTexUnit::TAM_CLAMP);

	mHBTex[1] = LLViewerTextureManager::getFetchedTexture(gMoonTextureID, FTT_DEFAULT, TRUE, LLGLTexture::BOOST_UI);
	gGL.getTexUnit(0)->bind(mHBTex[1]);
	mHBTex[1]->setAddressMode(LLTexUnit::TAM_CLAMP);


	mWaterImagep = LLViewerTextureManager::getFetchedTexture(TRANSPARENT_WATER_TEXTURE);
	llassert(mWaterImagep);
	mWaterImagep->setNoDelete();
	mOpaqueWaterImagep = LLViewerTextureManager::getFetchedTexture(OPAQUE_WATER_TEXTURE);
	llassert(mOpaqueWaterImagep);
	mWaterNormp = LLViewerTextureManager::getFetchedTexture(DEFAULT_WATER_NORMAL);
	mWaterNormp->setNoDelete();

	restoreGL();
}

LLDrawPoolWater::~LLDrawPoolWater()
{
}

//static
void LLDrawPoolWater::restoreGL()
{
	
}

LLDrawPool *LLDrawPoolWater::instancePool()
{
	LL_ERRS() << "Should never be calling instancePool on a water pool!" << LL_ENDL;
	return NULL;
}


void LLDrawPoolWater::prerender()
{
	mVertexShaderLevel = (gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps) ?
		LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_WATER) : 0;

	// got rid of modulation by light color since it got a little too
	// green at sunset and sl-57047 (underwater turns black at 8:00)
	sWaterFogColor = LLWaterParamManager::getInstance()->getFogColor();
	sWaterFogColor.mV[3] = 0;

}

S32 LLDrawPoolWater::getNumPasses()
{
	if (LLViewerCamera::getInstance()->getOrigin().mV[2] < 1024.f)
	{
		return 1;
	}

	return 0;
}

void LLDrawPoolWater::beginPostDeferredPass(S32 pass)
{
	beginRenderPass(pass);
	deferred_render = TRUE;
}

void LLDrawPoolWater::endPostDeferredPass(S32 pass)
{
	endRenderPass(pass);
	deferred_render = FALSE;
}

//===============================
//DEFERRED IMPLEMENTATION
//===============================
void LLDrawPoolWater::renderDeferred(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_WATER);
	deferred_render = TRUE;
	shade();
	deferred_render = FALSE;
}

//=========================================

void LLDrawPoolWater::render(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_WATER);
	if (mDrawFace.empty() || LLDrawable::getCurrentFrame() <= 1)
	{
		return;
	}

	//do a quick 'n dirty depth sort
	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			 iter != mDrawFace.end(); iter++)
	{
		LLFace* facep = *iter;
		facep->mDistance = -facep->mCenterLocal.mV[2];
	}

	std::sort(mDrawFace.begin(), mDrawFace.end(), LLFace::CompareDistanceGreater());
	static const LLCachedControl<bool> render_transparent_water("RenderTransparentWater",false);
	if(!render_transparent_water)
	{
		// render water for low end hardware
		renderOpaqueLegacyWater();
		return;
	}

	LLGLEnable<GL_BLEND> blend;

	if (mVertexShaderLevel > 0)
	{
		shade();
		return;
	}

	LLVOSky *voskyp = gSky.mVOSkyp;

	stop_glerror();

	if (!gGLManager.mHasMultitexture)
	{
		// Ack!  No multitexture!  Bail!
		return;
	}

	LLFace* refl_face = voskyp->getReflFace();

	LLGLState<GL_LIGHTING> light_state;
	gPipeline.disableLights(light_state);
	
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

	LLGLDisable<GL_CULL_FACE> cullFace;
	
	// Set up second pass first
	mWaterImagep->addTextureStats(1024.f*1024.f);
	gGL.getTexUnit(1)->activate();
	gGL.getTexUnit(1)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->bind(mWaterImagep) ;

	LLVector3 camera_up = LLViewerCamera::getInstance()->getUpAxis();
	F32 up_dot = camera_up * LLVector3::z_axis;

	LLColor4 water_color;
	if (LLViewerCamera::getInstance()->cameraUnderWater())
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.4f);
	}
	else
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.5f*(1.f + up_dot));
	}

	gGL.diffuseColor4fv(water_color.mV);

	// Automatically generate texture coords for detail map
	glEnable(GL_TEXTURE_GEN_S); //texture unit 1
	glEnable(GL_TEXTURE_GEN_T); //texture unit 1
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	// Slowly move over time.
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	static F32 frame_time;
	if (!freeze_time) frame_time = gFrameTimeSeconds;
	F32 offset = fmod(frame_time*2.f, 100.f);
	F32 tp0[4] = {16.f/256.f, 0.0f, 0.0f, offset*0.01f};
	F32 tp1[4] = {0.0f, 16.f/256.f, 0.0f, offset*0.01f};
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1);

	gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_PREV_COLOR);
	gGL.getTexUnit(1)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_ALPHA);

	gGL.getTexUnit(0)->activate();
	
	glClearStencil(1);
	glClear(GL_STENCIL_BUFFER_BIT);
	glClearStencil(0);
	LLGLEnable<GL_STENCIL_TEST> gls_stencil;
	glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
	glStencilFunc(GL_ALWAYS, 0, 0xFFFFFFFF);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
		 iter != mDrawFace.end(); iter++)
	{
		LLFace *face = *iter;
		if (voskyp->isReflFace(face))
		{
			continue;
		}
		if(face->getTexture() && face->getTexture()->hasGLTexture())
		{
			gGL.getTexUnit(0)->bind(face->getTexture());
			face->renderIndexed();
		}
	}

	// Now, disable texture coord generation on texture state 1
	gGL.getTexUnit(1)->activate();
	gGL.getTexUnit(1)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->disable();
	glDisable(GL_TEXTURE_GEN_S); //texture unit 1
	glDisable(GL_TEXTURE_GEN_T); //texture unit 1

	// Disable texture coordinate and color arrays
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	stop_glerror();
	
	if (gSky.mVOSkyp->getCubeMap())
	{
		gSky.mVOSkyp->getCubeMap()->enable(0);
		gSky.mVOSkyp->getCubeMap()->bind();

		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.loadIdentity();
		LLMatrix4a camera_rot = LLViewerCamera::getInstance()->getModelview();
		camera_rot.extractRotation_affine();
		camera_rot.invert();
		gGL.loadMatrix(camera_rot);

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		LLOverrideFaceColor overrid(this, 1.f, 1.f, 1.f,  0.5f*up_dot);

		gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			 iter != mDrawFace.end(); iter++)
		{
			LLFace *face = *iter;
			if (voskyp->isReflFace(face))
			{
				//refl_face = face;
				continue;
			}

			if (face->getGeomCount() > 0)
			{					
				face->renderIndexed();
			}
		}

		gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

		gSky.mVOSkyp->getCubeMap()->disable();
		
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		
	}

	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    if (refl_face)
	{
		glStencilFunc(GL_NOTEQUAL, 0, 0xFFFFFFFF);
		renderReflection(refl_face);
	}

	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
}

// for low end hardware
void LLDrawPoolWater::renderOpaqueLegacyWater()
{
	LLVOSky *voskyp = gSky.mVOSkyp;

	LLGLSLShader* shader = NULL;
	if (LLGLSLShader::sNoFixedFunction)
	{
		if (LLPipeline::sUnderWaterRender)
		{
			shader = &gObjectSimpleNonIndexedTexGenWaterProgram;
		}
		else
		{
			shader = &gObjectSimpleNonIndexedTexGenProgram;
		}

		shader->bind();
	}

	stop_glerror();

	// Depth sorting and write to depth buffer
	// since this is opaque, we should see nothing
	// behind the water.  No blending because
	// of no transparency.  And no face culling so
	// that the underside of the water is also opaque.
	LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE);
	LLGLDisable<GL_CULL_FACE> no_cull;
	LLGLDisable<GL_BLEND> no_blend;
	LLGLState<GL_LIGHTING> light_state;
	gPipeline.disableLights(light_state);

	//Singu note: This is a hack around bizarre opensim behavior. The opaque water texture we get is pure white and only has one channel.
	// This behavior is clearly incorrect, so we try to detect that case, purge it from the cache, and try to re-fetch the texture.
	// If the re-fetched texture is still invalid, or doesn't exist, we use transparent water, which is fine since alphablend is unset.
	// The current logic for refetching is crude here, and probably wont work if, say, a prim were to also have the texture for some reason,
	// however it works well enough otherwise, and is much cleaner than diving into LLTextureList, LLViewerFetchedTexture, and LLViewerTexture.
	// Perhaps a proper reload mechanism could be done if we ever add user-level texture reloading, but until then it's not a huge priority.
	// Failing to fully refetch will just give us the same invalid texture we started with, which will result in the fallback texture being used.
	if(mOpaqueWaterImagep != mWaterImagep)
	{
		if(mOpaqueWaterImagep->isMissingAsset())
		{
			mOpaqueWaterImagep = mWaterImagep;
		}
		else if(mOpaqueWaterImagep->hasGLTexture() && mOpaqueWaterImagep->getComponents() < 3)
		{
			LLAppViewer::getTextureCache()->removeFromCache(mOpaqueWaterImagep->getID());
			static bool sRefetch = true;
			if(sRefetch)
			{
				sRefetch = false;
				((LLViewerFetchedTexture*)mOpaqueWaterImagep.get())->forceRefetch();
			}
			else
				mOpaqueWaterImagep = mWaterImagep;
		}
	}
	mOpaqueWaterImagep->addTextureStats(1024.f*1024.f);

	// Activate the texture binding and bind one
	// texture since all images will have the same texture
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->bind(mOpaqueWaterImagep);

	// Automatically generate texture coords for water texture
	if (!shader)
	{
		glEnable(GL_TEXTURE_GEN_S); //texture unit 0
		glEnable(GL_TEXTURE_GEN_T); //texture unit 0
		glTexGenf(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
		glTexGenf(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	}

	// Use the fact that we know all water faces are the same size
	// to save some computation

	// Slowly move texture coordinates over time so the water appears
	// to be moving.
	F32 movement_period_secs = 50.f;

	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	static F32 frame_time;
	if (!freeze_time) frame_time = gFrameTimeSeconds;
	F32 offset = fmod(frame_time, movement_period_secs);

	if (movement_period_secs != 0)
	{
		offset /= movement_period_secs;
	}
	else
	{
		offset = 0;
	}

	F32 tp0[4] = { 16.f / 256.f, 0.0f, 0.0f, offset };
	F32 tp1[4] = { 0.0f, 16.f / 256.f, 0.0f, offset };

	if (!shader)
	{
		glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0);
		glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1);
	}
	else
	{
		shader->uniform4fv(LLShaderMgr::OBJECT_PLANE_S, 1, tp0);
		shader->uniform4fv(LLShaderMgr::OBJECT_PLANE_T, 1, tp1);
	}

	gGL.diffuseColor3f(1.f, 1.f, 1.f);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
		 iter != mDrawFace.end(); iter++)
	{
		LLFace *face = *iter;
		if (voskyp->isReflFace(face))
		{
			continue;
		}

		face->renderIndexed();
	}

	stop_glerror();

	if (!shader)
	{
		// Reset the settings back to expected values
		glDisable(GL_TEXTURE_GEN_S); //texture unit 0
		glDisable(GL_TEXTURE_GEN_T); //texture unit 0
	}

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
}


void LLDrawPoolWater::renderReflection(LLFace* face)
{
	LLVOSky *voskyp = gSky.mVOSkyp;

	if (!voskyp)
	{
		return;
	}

	if (!face->getGeomCount())
	{
		return;
	}
	
	S8 dr = voskyp->getDrawRefl();
	if (dr < 0)
	{
		return;
	}

	LLGLSNoFog noFog;

	gGL.getTexUnit(0)->bind(mHBTex[dr]);

	LLOverrideFaceColor override(this, face->getFaceColor().mV);
	face->renderIndexed();
}

void LLDrawPoolWater::shade()
{
	if (!deferred_render)
	{
		gGL.setColorMask(true, true);
	}

	LLVOSky *voskyp = gSky.mVOSkyp;

	if(voskyp == NULL) 
	{
		return;
	}

	LLGLDisable<GL_BLEND> blend;

	LLColor3 light_diffuse(0,0,0);
	F32 light_exp = 0.0f;
	LLVector3 light_dir;
	LLColor3 light_color;

	if (gSky.getSunDirection().mV[2] > LLSky::NIGHTTIME_ELEVATION_COS) 	 
    { 	 
        light_dir  = gSky.getSunDirection(); 	 
        light_dir.normVec(); 	
		light_color = gSky.getSunDiffuseColor();
		if(gSky.mVOSkyp) {
	        light_diffuse = gSky.mVOSkyp->getSun().getColorCached(); 	 
			light_diffuse.normVec(); 	 
		}
        light_exp = light_dir * LLVector3(light_dir.mV[0], light_dir.mV[1], 0); 	 
        light_diffuse *= light_exp + 0.25f; 	 
    } 	 
    else  	 
    { 	 
        light_dir       = gSky.getMoonDirection(); 	 
        light_dir.normVec(); 	 
		light_color = gSky.getMoonDiffuseColor();
        light_diffuse   = gSky.mVOSkyp->getMoon().getColorCached(); 	 
        light_diffuse.normVec(); 	 
        light_diffuse *= 0.5f; 	 
        light_exp = light_dir * LLVector3(light_dir.mV[0], light_dir.mV[1], 0); 	 
    }

	light_exp *= light_exp;
	light_exp *= light_exp;
	light_exp *= light_exp;
	light_exp *= light_exp;
	light_exp *= 256.f;
	light_exp = light_exp > 32.f ? light_exp : 32.f;

	LLGLSLShader* shader;

	F32 eyedepth = LLViewerCamera::getInstance()->getOrigin().mV[2] - gAgent.getRegion()->getWaterHeight();
	
	if (eyedepth < 0.f && LLPipeline::sWaterReflections)
	{
	if (deferred_render)
	{
			shader = &gDeferredUnderWaterProgram;
	}
		else
	{
		shader = &gUnderWaterProgram;
	}
	}
	else if (deferred_render)
	{
		shader = &gDeferredWaterProgram;
	}
	else
	{
		shader = &gWaterProgram;
	}

	if (deferred_render)
	{
		gPipeline.bindDeferredShader(*shader);
	}
	else
	{
		shader->bind();
	}

	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (!freeze_time)
	{
		sTime = (F32)LLFrameTimer::getElapsedSeconds()*0.5f;
	}
	
	S32 reftex = shader->enableTexture(LLShaderMgr::WATER_REFTEX);
		
	if (reftex > -1)
	{
		gGL.getTexUnit(reftex)->activate();
		gGL.getTexUnit(reftex)->bind(&gPipeline.mWaterRef);
		gGL.getTexUnit(0)->activate();
	}	

	//bind normal map
	S32 bumpTex = shader->enableTexture(LLViewerShaderMgr::BUMP_MAP);

	LLWaterParamManager * param_mgr = LLWaterParamManager::getInstance();

	// change mWaterNormp if needed
	if (mWaterNormp->getID() != param_mgr->getNormalMapID())
	{
		mWaterNormp = LLViewerTextureManager::getFetchedTexture(param_mgr->getNormalMapID());
	}

	mWaterNormp->addTextureStats(1024.f*1024.f);
	gGL.getTexUnit(bumpTex)->bind(mWaterNormp) ;
	static const LLCachedControl<bool> render_water_mip_normal("RenderWaterMipNormal");
	if (render_water_mip_normal)
	{
		mWaterNormp->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
	}
	else 
	{
		mWaterNormp->setFilteringOption(LLTexUnit::TFO_POINT);
	}
	
	S32 screentex = shader->enableTexture(LLShaderMgr::WATER_SCREENTEX);	
		
	if (screentex > -1)
	{
		shader->uniform4fv(LLShaderMgr::WATER_FOGCOLOR, 1, sWaterFogColor.mV);
		shader->uniform1f(LLShaderMgr::WATER_FOGDENSITY, 
			param_mgr->getFogDensity());
		gPipeline.mWaterDis.bindTexture(0, screentex);
	}
	
	stop_glerror();
	
	gGL.getTexUnit(screentex)->bind(&gPipeline.mWaterDis);	

	if (mVertexShaderLevel == 1)
	{
		sWaterFogColor.mV[3] = param_mgr->mDensitySliderValue;
		shader->uniform4fv(LLShaderMgr::WATER_FOGCOLOR, 1, sWaterFogColor.mV);
	}
	
	S32 diffTex = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP);
	stop_glerror();
	
	light_dir.normVec();
	sLightDir = light_dir;
	
	light_diffuse *= 6.f;

	//shader->uniformMatrix4fv("inverse_ref", 1, GL_FALSE, (GLfloat*) gGLObliqueProjectionInverse.mMatrix);
	shader->uniform1f(LLShaderMgr::WATER_WATERHEIGHT, eyedepth);
	shader->uniform1f(LLShaderMgr::WATER_TIME, sTime);
	shader->uniform3fv(LLShaderMgr::WATER_EYEVEC, 1, LLViewerCamera::getInstance()->getOrigin().mV);
	shader->uniform3fv(LLShaderMgr::WATER_SPECULAR, 1, light_diffuse.mV);
	shader->uniform1f(LLShaderMgr::WATER_SPECULAR_EXP, light_exp);
	shader->uniform2fv(LLShaderMgr::WATER_WAVE_DIR1, 1, param_mgr->getWave1Dir().mV);
	shader->uniform2fv(LLShaderMgr::WATER_WAVE_DIR2, 1, param_mgr->getWave2Dir().mV);
	shader->uniform3fv(LLShaderMgr::WATER_LIGHT_DIR, 1, light_dir.mV);

	shader->uniform3fv(LLShaderMgr::WATER_NORM_SCALE, 1, param_mgr->getNormalScale().mV);
	shader->uniform1f(LLShaderMgr::WATER_FRESNEL_SCALE, param_mgr->getFresnelScale());
	shader->uniform1f(LLShaderMgr::WATER_FRESNEL_OFFSET, param_mgr->getFresnelOffset());
	shader->uniform1f(LLShaderMgr::WATER_BLUR_MULTIPLIER, param_mgr->getBlurMultiplier());

	F32 sunAngle = llmax(0.f, light_dir.mV[2]);
	F32 scaledAngle = 1.f - sunAngle;

	shader->uniform1f(LLShaderMgr::WATER_SUN_ANGLE, sunAngle);
	shader->uniform1f(LLShaderMgr::WATER_SCALED_ANGLE, scaledAngle);
	shader->uniform1f(LLShaderMgr::WATER_SUN_ANGLE2, 0.1f + 0.2f*sunAngle);

	LLColor4 water_color;
	LLVector3 camera_up = LLViewerCamera::getInstance()->getUpAxis();
	F32 up_dot = camera_up * LLVector3::z_axis;
	if (LLViewerCamera::getInstance()->cameraUnderWater())
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.4f);
		shader->uniform1f(LLShaderMgr::WATER_REFSCALE, param_mgr->getScaleBelow());
	}
	else
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.5f*(1.f + up_dot));
		shader->uniform1f(LLShaderMgr::WATER_REFSCALE, param_mgr->getScaleAbove());
	}

	if (water_color.mV[3] > 0.9f)
	{
		water_color.mV[3] = 0.9f;
	}

	{
		LLGLEnable<GL_DEPTH_CLAMP> depth_clamp(gGLManager.mHasDepthClamp);
		LLGLDisable<GL_CULL_FACE> cullface;
		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			iter != mDrawFace.end(); iter++)
		{
			LLFace *face = *iter;

			if (voskyp->isReflFace(face))
			{
				continue;
			}

			LLVOWater* water = (LLVOWater*) face->getViewerObject();
			if(diffTex > -1 && face->getTexture() && face->getTexture()->hasGLTexture())
				gGL.getTexUnit(diffTex)->bind(face->getTexture());

			sNeedsReflectionUpdate = TRUE;

			if (water->getUseTexture() || !water->getIsEdgePatch())
			{
				sNeedsDistortionUpdate = TRUE;
				face->renderIndexed();
			}
			else if (gGLManager.mHasDepthClamp || deferred_render)
			{
				face->renderIndexed();
			}
			else
			{
				LLGLSquashToFarClip far_clip(glh_get_current_projection());
				face->renderIndexed();
			}
			//if(diffTex > -1 && face->getTexture()->hasGLTexture())
			//	gGL.getTexUnit(diffTex)->unbind(LLTexUnit::TT_TEXTURE);
		}
	}
	
	shader->disableTexture(LLShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
	shader->disableTexture(LLShaderMgr::WATER_SCREENTEX);	
	shader->disableTexture(LLShaderMgr::BUMP_MAP);
	shader->disableTexture(LLShaderMgr::DIFFUSE_MAP);
	shader->disableTexture(LLShaderMgr::WATER_REFTEX);
	shader->disableTexture(LLShaderMgr::WATER_SCREENDEPTH);

	if (deferred_render)
	{
		gPipeline.unbindDeferredShader(*shader);
	}
	else
	{
		shader->unbind();
	}

	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
	if (!deferred_render)
	{
		gGL.setColorMask(true, false);
	}

}

LLViewerTexture *LLDrawPoolWater::getDebugTexture()
{
	return LLViewerFetchedTexture::sSmokeImagep;
}

LLColor3 LLDrawPoolWater::getDebugColor() const
{
	return LLColor3(0.f, 1.f, 1.f);
}
