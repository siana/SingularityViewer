/** 
 * @file lldrawpooltree.cpp
 * @brief LLDrawPoolTree class implementation
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

#include "lldrawpooltree.h"

#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "llvotree.h"
#include "pipeline.h"
#include "llviewercamera.h"
#include "llviewershadermgr.h"
#include "llrender.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

S32 LLDrawPoolTree::sDiffTex = 0;
static LLGLSLShader* shader = NULL;
static LLTrace::BlockTimerStatHandle FTM_SHADOW_TREE("Tree Shadow");

LLDrawPoolTree::LLDrawPoolTree(LLViewerTexture *texturep) :
	LLFacePool(POOL_TREE),
	mTexturep(texturep)
{
	mTexturep->setAddressMode(LLTexUnit::TAM_WRAP);
}

LLDrawPool *LLDrawPoolTree::instancePool()
{
	return new LLDrawPoolTree(mTexturep);
}

void LLDrawPoolTree::prerender()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolTree::beginRenderPass(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_TREES);
		
	if (LLPipeline::sUnderWaterRender)
	{
		shader = &gTreeWaterProgram;
	}
	else
	{
		shader = &gTreeProgram;
	}

	if (LLGLSLShader::sNoFixedFunction)
	{
		shader->bind();
		shader->setMinimumAlpha(0.5f);
		gGL.diffuseColor4f(1,1,1,1);
	}
	else
	{
		gGL.setAlphaRejectSettings(LLRender::CF_GREATER, 0.5f);
	}
}

void LLDrawPoolTree::render(S32 pass)
{
	LL_RECORD_BLOCK_TIME(LLPipeline::sShadowRender ? FTM_SHADOW_TREE : FTM_RENDER_TREES);

	if (mDrawFace.empty())
	{
		return;
	}

	LLGLState<GL_ALPHA_TEST> test(LLGLSLShader::sNoFixedFunction ? 0 : 1);
	LLGLState<GL_LIGHTING> light_state;
	if (!LLGLSLShader::sNoFixedFunction)
	{
		gPipeline.enableLightsDynamic(light_state);
	}
	LLOverrideFaceColor color(this, 1.f, 1.f, 1.f, 1.f);

	gGL.getTexUnit(sDiffTex)->bind(mTexturep);
	
	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			 iter != mDrawFace.end(); iter++)
	{
		LLFace *face = *iter;
		if(face->getViewerObject())
		{
			LLVOTree* pTree = dynamic_cast<LLVOTree*>(face->getViewerObject());
			if(pTree && !pTree->mDrawList.empty() )
			{
				LLMatrix4a* model_matrix = &(face->getDrawable()->getRegion()->mRenderMatrix);

				gGL.loadMatrix(gGLModelView);
				gGL.multMatrix(*model_matrix);
				gPipeline.mMatrixOpCount++;

				for(std::vector<LLPointer<LLDrawInfo> >::iterator iter2 = pTree->mDrawList.begin();
					iter2 != pTree->mDrawList.end(); iter2++)
				{
					LLDrawInfo& params = *iter2->get();
					gGL.pushMatrix();
					gGL.multMatrix(*params.mModelMatrix);
					gPipeline.mMatrixOpCount++;
					params.mVertexBuffer->setBuffer(LLDrawPoolTree::VERTEX_DATA_MASK);
					params.mVertexBuffer->drawRange(params.mDrawMode, params.mStart, params.mEnd, params.mCount, params.mOffset);
					gGL.popMatrix();
				}
				continue;
			}
		}
		LLVertexBuffer* buff = face->getVertexBuffer();

		if(buff)
		{
			LLMatrix4a* model_matrix = &(face->getDrawable()->getRegion()->mRenderMatrix);
			if(model_matrix && model_matrix->isIdentity())
			{
				model_matrix = NULL;
			}
			if (model_matrix != gGLLastMatrix)
			{
				gGLLastMatrix = model_matrix;
				gGL.loadMatrix(gGLModelView);
				if (model_matrix)
				{
					llassert(gGL.getMatrixMode() == LLRender::MM_MODELVIEW);
					gGL.multMatrix(*model_matrix);
				}
				gPipeline.mMatrixOpCount++;
			}

			buff->setBuffer(LLDrawPoolTree::VERTEX_DATA_MASK);
			buff->drawRange(LLRender::TRIANGLES, 0, buff->getNumVerts()-1, buff->getNumIndices(), 0); 
			gPipeline.addTrianglesDrawn(buff->getNumIndices());
		}
	}
}

void LLDrawPoolTree::endRenderPass(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_TREES);
		
	if (gPipeline.canUseWindLightShadersOnObjects())
	{
		shader->unbind();
	}
	
	if (mVertexShaderLevel <= 0)
	{
		gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
	}
}

//============================================
// deferred implementation
//============================================
void LLDrawPoolTree::beginDeferredPass(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_TREES);
		
	shader = &gDeferredTreeProgram;
	shader->bind();
	shader->setMinimumAlpha(0.5f);
}

void LLDrawPoolTree::renderDeferred(S32 pass)
{
	render(pass);
}

void LLDrawPoolTree::endDeferredPass(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_TREES);
	
	shader->unbind();
}

//============================================
// shadow implementation
//============================================
void LLDrawPoolTree::beginShadowPass(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_SHADOW_TREE);

	static const LLCachedControl<F32> render_deferred_offset("RenderDeferredTreeShadowOffset",1.f);
	static const LLCachedControl<F32> render_deferred_bias("RenderDeferredTreeShadowBias",1.f);
	gGL.setPolygonOffset(render_deferred_offset,render_deferred_bias);
	gDeferredTreeShadowProgram.bind();
	gDeferredTreeShadowProgram.setMinimumAlpha(0.5f);
}

void LLDrawPoolTree::renderShadow(S32 pass)
{
	render(pass);
}

void LLDrawPoolTree::endShadowPass(S32 pass)
{
	LL_RECORD_BLOCK_TIME(FTM_SHADOW_TREE);

	static const LLCachedControl<F32> render_deferred_offset("RenderDeferredSpotShadowOffset",1.f);
	static const LLCachedControl<F32> render_deferred_bias("RenderDeferredSpotShadowBias",1.f);
	gGL.setPolygonOffset(render_deferred_offset,render_deferred_bias);
	gDeferredTreeShadowProgram.unbind();
}

BOOL LLDrawPoolTree::verify() const
{
/*	BOOL ok = TRUE;

	if (!ok)
	{
		printDebugInfo();
	}
	return ok;*/

	return TRUE;
}

LLViewerTexture *LLDrawPoolTree::getTexture()
{
	return mTexturep;
}

LLViewerTexture *LLDrawPoolTree::getDebugTexture()
{
	return mTexturep;
}


LLColor3 LLDrawPoolTree::getDebugColor() const
{
	return LLColor3(1.f, 0.f, 1.f);
}

