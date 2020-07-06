 /** 
 * @file llrender.cpp
 * @brief LLRender implementation
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

#include "linden_common.h"

#include "llrender.h"

#include "llvertexbuffer.h"
#include "llcubemap.h"
#include "llglslshader.h"
#include "llimagegl.h"
#include "llrendertarget.h"
#include "lltexture.h"
#include "llshadermgr.h"
#include "llmatrix4a.h"

LLRender gGL;

// Handy copies of last good GL matrices
//Would be best to migrate these to LLMatrix4a and LLVector4a, but that's too divergent right now.
LLMatrix4a gGLModelView;
LLMatrix4a gGLLastModelView;
LLMatrix4a gGLPreviousModelView;
LLMatrix4a gGLLastProjection;
LLMatrix4a gGLProjection;
LLRect gGLViewport;

U32 LLRender::sUICalls = 0;
U32 LLRender::sUIVerts = 0;
U32 LLTexUnit::sWhiteTexture = 0;
bool LLRender::sGLCoreProfile = false;

static const U32 LL_NUM_TEXTURE_LAYERS = 32; 

static const GLenum sGLTextureType[] =
{
	GL_TEXTURE_2D,
	GL_TEXTURE_CUBE_MAP_ARB
};

static const GLint sGLAddressMode[] =
{	
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE
};

static const GLenum sGLCompareFunc[] =
{
	GL_NEVER,
	GL_ALWAYS,
	GL_LESS,
	GL_LEQUAL,
	GL_EQUAL,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_GREATER
};

const U32 immediate_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXCOORD0;

static const GLenum sGLBlendFactor[] =
{
	GL_ONE,
	GL_ZERO,
	GL_DST_COLOR,
	GL_SRC_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_DST_ALPHA,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,

	GL_ZERO // 'BF_UNDEF'
};

static const GLenum sGLPolygonFaceType[] =
{
	GL_FRONT,
	GL_BACK,
	GL_FRONT_AND_BACK
};

static const GLenum sGLPolygonMode[] =
{
	GL_POINT,
	GL_LINE,
	GL_FILL
};

LLTexUnit::LLTexUnit(S32 index)
	: mCurrTexType(TT_NONE), mCurrBlendType(TB_MULT), 
	mCurrColorOp(TBO_MULT), mCurrAlphaOp(TBO_MULT),
	mCurrColorSrc1(TBS_TEX_COLOR), mCurrColorSrc2(TBS_PREV_COLOR),
	mCurrAlphaSrc1(TBS_TEX_ALPHA), mCurrAlphaSrc2(TBS_PREV_ALPHA),
	mCurrColorScale(1), mCurrAlphaScale(1), mCurrTexture(0),
	mHasMipMaps(false),
	mIndex(index)
{
	llassert_always(index < (S32)LL_NUM_TEXTURE_LAYERS);
}

//static
U32 LLTexUnit::getInternalType(eTextureType type)
{
	return sGLTextureType[type];
}

//void validate_bind_texture(U32 name);

void LLTexUnit::refreshState(void)
{
	// We set dirty to true so that the tex unit knows to ignore caching
	// and we reset the cached tex unit state

	gGL.flush();
	
	glActiveTextureARB(GL_TEXTURE0_ARB + mIndex);

	//
	// Per apple spec, don't call glEnable/glDisable when index exceeds max texture units
	// http://www.mailinglistarchive.com/html/mac-opengl@lists.apple.com/2008-07/msg00653.html
	//
	bool enableDisable = !LLGLSLShader::sNoFixedFunction && 
		(mIndex < gGLManager.mNumTextureUnits) /*&& mCurrTexType != LLTexUnit::TT_MULTISAMPLE_TEXTURE*/;
		
	if (mCurrTexType != TT_NONE)
	{
		if (enableDisable)
		{
			glEnable(sGLTextureType[mCurrTexType]);
		}
		
		//if (mCurrTexture) validate_bind_texture(mCurrTexture);
		glBindTexture(sGLTextureType[mCurrTexType], mCurrTexture);
	}
	else
	{
		if (enableDisable)
		{
			glDisable(GL_TEXTURE_2D);
		}
		
		glBindTexture(GL_TEXTURE_2D, 0);	
	}

	if (mCurrBlendType != TB_COMBINE)
	{
		setTextureBlendType(mCurrBlendType);
	}
	else
	{
		setTextureCombiner(mCurrColorOp, mCurrColorSrc1, mCurrColorSrc2, false);
		setTextureCombiner(mCurrAlphaOp, mCurrAlphaSrc1, mCurrAlphaSrc2, true);
	}
}

void LLTexUnit::activate(void)
{
	if (mIndex < 0) return;

	if ((S32)gGL.getCurrentTexUnitIndex() != mIndex || gGL.mDirty)
	{
		//gGL.flush();
		// Apply immediately.
		glActiveTextureARB(GL_TEXTURE0_ARB + mIndex);
		gGL.mContext.texUnit = gGL.mNewContext.texUnit = mIndex;
	}
}

void LLTexUnit::enable(eTextureType type)
{
	if (mIndex < 0) return;

	if ( (mCurrTexType != type || gGL.mDirty) && (type != TT_NONE) )
	{
		stop_glerror();
		activate();
		stop_glerror();
		if (mCurrTexType != TT_NONE && !gGL.mDirty)
		{
			disable(); // Force a disable of a previous texture type if it's enabled.
			stop_glerror();
		}
		mCurrTexType = type;

		gGL.flush();
		if (!LLGLSLShader::sNoFixedFunction &&
			//type != LLTexUnit::TT_MULTISAMPLE_TEXTURE &&
			mIndex < gGLManager.mNumTextureUnits)
		{
			stop_glerror();
			glEnable(sGLTextureType[type]);
			stop_glerror();
		}
	}
}

void LLTexUnit::disable(void)
{
	if (mIndex < 0) return;

	if (mCurrTexType != TT_NONE)
	{
		activate();
		unbind(mCurrTexType);
		gGL.flush();
		if (!LLGLSLShader::sNoFixedFunction &&
			//mCurrTexType != LLTexUnit::TT_MULTISAMPLE_TEXTURE &&
			mIndex < gGLManager.mNumTextureUnits)
		{
			glDisable(sGLTextureType[mCurrTexType]);
		}
		
		mCurrTexType = TT_NONE;
	}
}

bool LLTexUnit::bind(LLTexture* texture, bool for_rendering, bool forceBind)
{
	stop_glerror();
	if (mIndex >= 0)
	{
		//gGL.flush();

		LLImageGL* gl_tex = NULL ;

		if (texture != NULL && (gl_tex = texture->getGLTexture()))
		{
			if (gl_tex->getTexName()) //if texture exists
			{
				//in audit, replace the selected texture by the default one.
				if(gAuditTexture && for_rendering && LLImageGL::sCurTexPickSize > 0)
				{
					if(texture->getWidth() * texture->getHeight() == LLImageGL::sCurTexPickSize)
					{
						gl_tex->updateBindStats(gl_tex->mTextureMemory);
						return bind(LLImageGL::sHighlightTexturep.get());
					}
				}
				if ((mCurrTexture != gl_tex->getTexName()) || forceBind)
				{
					gGL.flush();
					activate();
					enable(gl_tex->getTarget());
					mCurrTexture = gl_tex->getTexName();
					//validate_bind_texture(mCurrTexture);
					glBindTexture(sGLTextureType[gl_tex->getTarget()], mCurrTexture);
					if(gl_tex->updateBindStats(gl_tex->mTextureMemory))
					{
						texture->setActive() ;
						texture->updateBindStatsForTester() ;
					}
					mHasMipMaps = gl_tex->mHasMipMaps;
					if (gl_tex->mTexOptionsDirty)
					{
						gl_tex->mTexOptionsDirty = false;
						setTextureAddressMode(gl_tex->mAddressMode);
						setTextureFilteringOption(gl_tex->mFilterOption);
					}
				}
			}
			else
			{
				//if deleted, will re-generate it immediately
				texture->forceImmediateUpdate() ;

				gl_tex->forceUpdateBindStats() ;
				return texture->bindDefaultImage(mIndex);
			}
		}
		else
		{
			LL_WARNS() << "NULL LLTexUnit::bind texture" << LL_ENDL;
			return false;
		}
	}
	else
	{ // mIndex < 0
		return false;
	}

	return true;
}

bool LLTexUnit::bind(LLImageGL* texture, bool for_rendering, bool forceBind)
{
	if (mIndex < 0) return false;

	if(!texture)
	{
		LL_WARNS() << "NULL LLTexUnit::bind texture" << LL_ENDL;
		return false;
	}

	if(!texture->getTexName())
	{
		if(LLImageGL::sDefaultGLTexture && LLImageGL::sDefaultGLTexture->getTexName())
		{
			return bind(LLImageGL::sDefaultGLTexture) ;
		}
		return false ;
	}

	if ((mCurrTexture != texture->getTexName()) || forceBind)
	{
		gGL.flush();
		activate();
		enable(texture->getTarget());
		mCurrTexture = texture->getTexName();
		//validate_bind_texture(mCurrTexture);
		glBindTexture(sGLTextureType[texture->getTarget()], mCurrTexture);
		texture->updateBindStats(texture->mTextureMemory);		
		mHasMipMaps = texture->mHasMipMaps;
		if (texture->mTexOptionsDirty)
		{
			texture->mTexOptionsDirty = false;
			setTextureAddressMode(texture->mAddressMode);
			setTextureFilteringOption(texture->mFilterOption);
		}
	}

	return true;
}

bool LLTexUnit::bind(LLCubeMap* cubeMap)
{
	if (mIndex < 0) return false;

	if (cubeMap == NULL)
	{
		LL_WARNS() << "NULL LLTexUnit::bind cubemap" << LL_ENDL;
		return false;
	}

	if (cubeMap->mImages[0].isNull())
	{
		LL_WARNS() << "NULL LLTexUnit::bind cubeMap->mImages[0]" << LL_ENDL;
		return false;
	}
	if (mCurrTexture != cubeMap->mImages[0]->getTexName())
	{
		if (gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps)
		{
			gGL.flush();
			activate();
			enable(LLTexUnit::TT_CUBE_MAP);
			mCurrTexture = cubeMap->mImages[0]->getTexName();
			//validate_bind_texture(mCurrTexture);
			glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, mCurrTexture);
			mHasMipMaps = cubeMap->mImages[0]->mHasMipMaps;
			cubeMap->mImages[0]->updateBindStats(cubeMap->mImages[0]->mTextureMemory);
			if (cubeMap->mImages[0]->mTexOptionsDirty)
			{
				cubeMap->mImages[0]->mTexOptionsDirty = false;
				setTextureAddressMode(cubeMap->mImages[0]->mAddressMode);
				setTextureFilteringOption(cubeMap->mImages[0]->mFilterOption);
			}
			return true;
		}
		else
		{
			LL_WARNS() << "Using cube map without extension!" << LL_ENDL;
			return false;
		}
	}
	return true;
}

// LLRenderTarget is unavailible on the mapserver since it uses FBOs.
#if !LL_MESA_HEADLESS
bool LLTexUnit::bind(LLRenderTarget* renderTarget, bool bindDepth)
{
	if (mIndex < 0) return false;

	//gGL.flush();

	if (bindDepth)
	{
		if (renderTarget->hasStencil())
		{
			LL_ERRS() << "Cannot bind a render buffer for sampling.  Allocate render target without a stencil buffer if sampling of depth buffer is required." << LL_ENDL;
		}

		bindManual(renderTarget->getUsage(), renderTarget->getDepth());
	}
	else
	{
		bindManual(renderTarget->getUsage(), renderTarget->getTexture());
	}

	return true;
}
#endif // LL_MESA_HEADLESS

bool LLTexUnit::bindManual(eTextureType type, U32 texture, bool hasMips)
{
	if (mIndex < 0)  
	{
		return false;
	}
	
	if(mCurrTexture != texture)
	{
		gGL.flush();

		activate();
		enable(type);
		mCurrTexture = texture;
		//validate_bind_texture(texture);
		glBindTexture(sGLTextureType[type], texture);
		mHasMipMaps = hasMips;
	}
	return true;
}

void LLTexUnit::unbind(eTextureType type)
{
	if (mIndex < 0) return;

	//always flush and activate for consistency 
	//   some code paths assume unbind always flushes and sets the active texture
	if (gGL.getCurrentTexUnitIndex() != mIndex || gGL.mDirty)
	{
		gGL.flush();
		activate();
	}

	// Disabled caching of binding state.
	if (mCurrTexType == type && mCurrTexture != 0)
	{
		gGL.flush();
		mCurrTexture = 0;
		if (/*LLGLSLShader::sNoFixedFunction && */type == LLTexUnit::TT_TEXTURE)
		{
			//if (sWhiteTexture)
			//	validate_bind_texture(sWhiteTexture);
			glBindTexture(sGLTextureType[type], sWhiteTexture);
		}
		else
		{
			glBindTexture(sGLTextureType[type], 0);
		}
	}
}

void LLTexUnit::setTextureAddressMode(eTextureAddressMode mode)
{
	if (mIndex < 0 || mCurrTexture == 0) return;

	gGL.flush();

	activate();

	glTexParameteri (sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_S, sGLAddressMode[mode]);
	glTexParameteri (sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_T, sGLAddressMode[mode]);
	if (mCurrTexType == TT_CUBE_MAP)
	{
		glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, sGLAddressMode[mode]);
	}
}

void LLTexUnit::setTextureFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
	if (mIndex < 0 || mCurrTexture == 0 /*|| mCurrTexType == LLTexUnit::TT_MULTISAMPLE_TEXTURE*/) return;

	gGL.flush();

	if (option == TFO_POINT)
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (option >= TFO_TRILINEAR && mHasMipMaps)
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	} 
	else if (option >= TFO_BILINEAR)
	{
		if (mHasMipMaps)
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		}
		else
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
	}
	else
	{
		if (mHasMipMaps)
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		}
		else
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
	}

	if (gGLManager.mHasAnisotropic)
	{
		if (LLImageGL::sGlobalUseAnisotropic && option == TFO_ANISOTROPIC)
		{
			if (gGL.mMaxAnisotropy < 1.f)
			{
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gGL.mMaxAnisotropy);

				LL_INFOS() << "gGL.mMaxAnisotropy: " << gGL.mMaxAnisotropy << LL_ENDL ;
				gGL.mMaxAnisotropy = llmax(1.f, gGL.mMaxAnisotropy) ;
			}
			glTexParameterf(sGLTextureType[mCurrTexType], GL_TEXTURE_MAX_ANISOTROPY_EXT, gGL.mMaxAnisotropy);
		}
		else
		{
			glTexParameterf(sGLTextureType[mCurrTexType], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);
		}
	}
}

void LLTexUnit::setTextureBlendType(eTextureBlendType type)
{
	if (LLGLSLShader::sNoFixedFunction)
	{ //texture blend type means nothing when using shaders
		return;
	}

	if (mIndex < 0 || mIndex >= gGLManager.mNumTextureUnits) return;

	// Do nothing if it's already correctly set.
	if (mCurrBlendType == type && !gGL.mDirty)
	{
		return;
	}

	gGL.flush();

	activate();
	mCurrBlendType = type;
	S32 scale_amount = 1;
	switch (type) 
	{
		case TB_REPLACE:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			break;
		case TB_ADD:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			break;
		case TB_MULT:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			break;
		case TB_MULT_X2:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			scale_amount = 2;
			break;
		case TB_ALPHA_BLEND:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			break;
		case TB_COMBINE:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			break;
		default:
			LL_ERRS() << "Unknown Texture Blend Type: " << type << LL_ENDL;
			break;
	}
	setColorScale(scale_amount);
	setAlphaScale(1);
}

GLint LLTexUnit::getTextureSource(eTextureBlendSrc src)
{
	switch(src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_PREV_ALPHA:
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_PREV_ALPHA:
			return GL_PREVIOUS_ARB;

		// All four cases should return the same value.
		case TBS_TEX_COLOR:
		case TBS_TEX_ALPHA:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_TEX_ALPHA:
			return GL_TEXTURE;

		// All four cases should return the same value.
		case TBS_VERT_COLOR:
		case TBS_VERT_ALPHA:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_VERT_ALPHA:
			return GL_PRIMARY_COLOR_ARB;

		// All four cases should return the same value.
		case TBS_CONST_COLOR:
		case TBS_CONST_ALPHA:
		case TBS_ONE_MINUS_CONST_COLOR:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_CONSTANT_ARB;

		default:
			LL_WARNS() << "Unknown eTextureBlendSrc: " << src << ".  Using Vertex Color instead." << LL_ENDL;
			return GL_PRIMARY_COLOR_ARB;
	}
}

GLint LLTexUnit::getTextureSourceType(eTextureBlendSrc src, bool isAlpha)
{
	switch(src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_TEX_COLOR:
		case TBS_VERT_COLOR:
		case TBS_CONST_COLOR:
			return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_PREV_ALPHA:
		case TBS_TEX_ALPHA:
		case TBS_VERT_ALPHA:
		case TBS_CONST_ALPHA:
			return GL_SRC_ALPHA;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_CONST_COLOR:
			return (isAlpha) ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE_MINUS_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_ALPHA:
		case TBS_ONE_MINUS_TEX_ALPHA:
		case TBS_ONE_MINUS_VERT_ALPHA:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_ONE_MINUS_SRC_ALPHA;

		default:
			LL_WARNS() << "Unknown eTextureBlendSrc: " << src << ".  Using Source Color or Alpha instead." << LL_ENDL;
			return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;
	}
}

void LLTexUnit::setTextureCombiner(eTextureBlendOp op, eTextureBlendSrc src1, eTextureBlendSrc src2, bool isAlpha)
{
	if (LLGLSLShader::sNoFixedFunction)
	{ //register combiners do nothing when not using fixed function
		return;
	}	

	if (mIndex < 0 || mIndex >= gGLManager.mNumTextureUnits) return;

	activate();
	if (mCurrBlendType != TB_COMBINE || gGL.mDirty)
	{
		mCurrBlendType = TB_COMBINE;
		gGL.flush();
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	}

	// We want an early out, because this function does a LOT of stuff.
	if ( ( (isAlpha && (mCurrAlphaOp == op) && (mCurrAlphaSrc1 == src1) && (mCurrAlphaSrc2 == src2))
			|| (!isAlpha && (mCurrColorOp == op) && (mCurrColorSrc1 == src1) && (mCurrColorSrc2 == src2)) ) && !gGL.mDirty)
	{
		return;
	}

	gGL.flush();

	// Get the gl source enums according to the eTextureBlendSrc sources passed in
	GLint source1 = getTextureSource(src1);
	GLint source2 = getTextureSource(src2);
	// Get the gl operand enums according to the eTextureBlendSrc sources passed in
	GLint operand1 = getTextureSourceType(src1, isAlpha);
	GLint operand2 = getTextureSourceType(src2, isAlpha);
	// Default the scale amount to 1
	S32 scale_amount = 1;
	GLenum comb_enum, src0_enum, src1_enum, src2_enum, operand0_enum, operand1_enum, operand2_enum;
	
	if (isAlpha)
	{
		// Set enums to ALPHA ones
		comb_enum = GL_COMBINE_ALPHA_ARB;
		src0_enum = GL_SOURCE0_ALPHA_ARB;
		src1_enum = GL_SOURCE1_ALPHA_ARB;
		src2_enum = GL_SOURCE2_ALPHA_ARB;
		operand0_enum = GL_OPERAND0_ALPHA_ARB;
		operand1_enum = GL_OPERAND1_ALPHA_ARB;
		operand2_enum = GL_OPERAND2_ALPHA_ARB;

		// cache current combiner
		mCurrAlphaOp = op;
		mCurrAlphaSrc1 = src1;
		mCurrAlphaSrc2 = src2;
	}
	else 
	{
		// Set enums to RGB ones
		comb_enum = GL_COMBINE_RGB_ARB;
		src0_enum = GL_SOURCE0_RGB_ARB;
		src1_enum = GL_SOURCE1_RGB_ARB;
		src2_enum = GL_SOURCE2_RGB_ARB;
		operand0_enum = GL_OPERAND0_RGB_ARB;
		operand1_enum = GL_OPERAND1_RGB_ARB;
		operand2_enum = GL_OPERAND2_RGB_ARB;

		// cache current combiner
		mCurrColorOp = op;
		mCurrColorSrc1 = src1;
		mCurrColorSrc2 = src2;
	}

	switch(op)
	{
		case TBO_REPLACE:
			// Slightly special syntax (no second sources), just set all and return.
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
			glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
			(isAlpha) ? setAlphaScale(1) : setColorScale(1);
			return;

		case TBO_MULT:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			break;

		case TBO_MULT_X2:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			scale_amount = 2;
			break;

		case TBO_MULT_X4:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			scale_amount = 4;
			break;

		case TBO_ADD:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_ADD);
			break;

		case TBO_ADD_SIGNED:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_ADD_SIGNED_ARB);
			break;

		case TBO_SUBTRACT:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_SUBTRACT_ARB);
			break;

		case TBO_LERP_VERT_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PRIMARY_COLOR_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_TEX_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_PREV_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_CONST_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_CONSTANT_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_VERT_COLOR:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PRIMARY_COLOR_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, (isAlpha) ? GL_SRC_ALPHA : GL_SRC_COLOR);
			break;

		default:
			LL_WARNS() << "Unknown eTextureBlendOp: " << op << ".  Setting op to replace." << LL_ENDL;
			// Slightly special syntax (no second sources), just set all and return.
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
			glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
			(isAlpha) ? setAlphaScale(1) : setColorScale(1);
			return;
	}

	// Set sources, operands, and scale accordingly
	glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
	glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
	glTexEnvi(GL_TEXTURE_ENV, src1_enum, source2);
	glTexEnvi(GL_TEXTURE_ENV, operand1_enum, operand2);
	(isAlpha) ? setAlphaScale(scale_amount) : setColorScale(scale_amount);
}

void LLTexUnit::setColorScale(S32 scale)
{
	if (mCurrColorScale != scale || gGL.mDirty)
	{
		gGL.flush();
		mCurrColorScale = scale;
		glTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE, scale );
	}
}

void LLTexUnit::setAlphaScale(S32 scale)
{
	if (mCurrAlphaScale != scale || gGL.mDirty)
	{
		gGL.flush();
		mCurrAlphaScale = scale;
		glTexEnvi( GL_TEXTURE_ENV, GL_ALPHA_SCALE, scale );
	}
}

// Useful for debugging that you've manually assigned a texture operation to the correct 
// texture unit based on the currently set active texture in opengl.
void LLTexUnit::debugTextureUnit(void)
{
	if (mIndex < 0) return;

	GLint activeTexture;
	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);
	if ((GL_TEXTURE0_ARB + mIndex) != activeTexture)
	{
		U32 set_unit = (activeTexture - GL_TEXTURE0_ARB);
		LL_WARNS() << "Incorrect Texture Unit!  Expected: " << set_unit << " Actual: " << mIndex << LL_ENDL;
	}
}

LLLightState::LLLightState(S32 index) :
	mState(index),
	mIndex(index)
{
	mPosMatrix.setIdentity();
	mSpotMatrix.setIdentity();
}

#define UPDATE_LIGHTSTATE(state, value) \
	if (mState.state != value) { \
		mState.state = value; \
		++gGL.mLightHash; \
	}

#define UPDATE_LIGHTSTATE_AND_TRANSFORM(state, value, matrix, transformhash) \
	if (mState.state != value || memcmp(matrix.getF32ptr(), gGL.getModelviewMatrix().getF32ptr(), sizeof(LLMatrix4a))) { \
		mState.state = value; \
		++gGL.mLightHash; \
		++gGL.transformhash[mIndex]; \
		matrix = gGL.getModelviewMatrix(); \
	}

void LLLightState::setDiffuse(const LLColor4& diffuse)
{
	UPDATE_LIGHTSTATE(mDiffuse, diffuse);
}

void LLLightState::setSpecular(const LLColor4& specular)
{
	UPDATE_LIGHTSTATE(mSpecular, specular);
}

void LLLightState::setPosition(const LLVector4& position)
{
	UPDATE_LIGHTSTATE_AND_TRANSFORM(mPosition, position, mPosMatrix, mLightPositionTransformHash);
}

void LLLightState::setConstantAttenuation(const F32& atten)
{
	UPDATE_LIGHTSTATE(mConstantAtten, atten);
}

void LLLightState::setLinearAttenuation(const F32& atten)
{
	UPDATE_LIGHTSTATE(mLinearAtten, atten);
}

void LLLightState::setQuadraticAttenuation(const F32& atten)
{
	UPDATE_LIGHTSTATE(mQuadraticAtten, atten);
}

void LLLightState::setSpotExponent(const F32& exponent)
{
	UPDATE_LIGHTSTATE(mSpotExponent, exponent);
}

void LLLightState::setSpotCutoff(const F32& cutoff)
{
	UPDATE_LIGHTSTATE(mSpotCutoff, cutoff);
}

void LLLightState::setSpotDirection(const LLVector3& direction)
{
	UPDATE_LIGHTSTATE_AND_TRANSFORM(mSpotDirection, direction, mSpotMatrix, mLightSpotTransformHash);
}

void LLLightState::setEnabled(const bool enabled)
{
	if (mEnabled != enabled)
	{
		mEnabled = enabled;
		++gGL.mLightHash;
	}
}


LLRender::LLRender()
  : mDirty(false),
    mCount(0),
    mMode(LLRender::TRIANGLES),
	mMatrixMode(LLRender::MM_MODELVIEW),
	mMatIdx{ 0 },
    mMaxAnisotropy(0.f),
	mPrimitiveReset(false)
{	
	mTexUnits.reserve(LL_NUM_TEXTURE_LAYERS);
	for (U32 i = 0; i < LL_NUM_TEXTURE_LAYERS; i++)
	{
		mTexUnits.push_back(new LLTexUnit(i));
	}
	mDummyTexUnit = new LLTexUnit(-1);

	for (U32 i = 0; i < NUM_LIGHTS; ++i)
	{
		mLightState.push_back(new LLLightState(i));
	}
	
	resetSyncHashes();
	
	//Init base matrix for each mode
	for(S32 i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		mMatrix[i][0].setIdentity();
	}

	gGLModelView.setIdentity();
	gGLLastModelView.setIdentity();
	gGLPreviousModelView.setIdentity();
	gGLLastProjection.setIdentity();
	gGLProjection.setIdentity();
}

LLRender::~LLRender()
{
	shutdown();
}

void LLRender::init()
{
	if (sGLCoreProfile && !LLVertexBuffer::sUseVAO)
	{ //bind a dummy vertex array object so we're core profile compliant
#ifdef GL_ARB_vertex_array_object
		U32 ret;
		glGenVertexArrays(1, &ret);
		glBindVertexArray(ret);
#endif
	}
	stop_glerror();
	restoreVertexBuffers();
}

void LLRender::shutdown()
{
	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		delete mTexUnits[i];
	}
	mTexUnits.clear();
	delete mDummyTexUnit;
	mDummyTexUnit = NULL;

	for (U32 i = 0; i < mLightState.size(); ++i)
	{
		delete mLightState[i];
	}
	mLightState.clear();
	mBuffer = NULL ;
}

void LLRender::destroyGL()
{
	// Reset gl state cache
	mCurShader = 0;
	mContext = Context();
	resetSyncHashes();
	LLTexUnit::sWhiteTexture = 0; // Also done in LLImageGL::destroyGL.
	for (auto unit : mTexUnits)
	{
		if (unit->getCurrTexture() > 0)
		{
			unit->unbind(unit->getCurrType());
		}
	}

	resetVertexBuffers();
}

void LLRender::refreshState(void)
{
	mDirty = true;

	U32 active_unit = getCurrentTexUnitIndex();

	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		mTexUnits[i]->refreshState();
		stop_glerror();
	}
	
	mTexUnits[active_unit]->activate();
	stop_glerror();

	/*setColorMask(mCurrColorMask[0], mCurrColorMask[1], mCurrColorMask[2], mCurrColorMask[3]);
	stop_glerror();
	
	setAlphaRejectSettings(mCurrAlphaFunc, mCurrAlphaFuncVal);
	stop_glerror();

	//Singu note: Also reset glBlendFunc
	blendFunc(mCurrBlendColorSFactor,mCurrBlendColorDFactor,mCurrBlendAlphaSFactor,mCurrBlendAlphaDFactor);
	stop_glerror();*/

	mDirty = false;
}

void LLRender::resetVertexBuffers()
{
	mBuffer = NULL;
}

void LLRender::restoreVertexBuffers()
{
	if (!mBuffer.isNull())
		return;
	stop_glerror();
	mBuffer = new LLVertexBuffer(immediate_mask, 0);
	stop_glerror();
	mBuffer->allocateBuffer(4096, 0, TRUE);
	stop_glerror();
	mBuffer->getVertexStrider(mVerticesp);
	stop_glerror();
	mBuffer->getTexCoord0Strider(mTexcoordsp);
	stop_glerror();
	mBuffer->getColorStrider(mColorsp);
	stop_glerror();
}

void LLRender::syncShaders()
{
	if (mCurShader != mNextShader)
	{
		glUseProgramObjectARB(mNextShader);
		mCurShader = mNextShader;
	}
}

void LLRender::syncContextState()
{
	if (mContext.color != mNewContext.color)
	{
		mContext.color = mNewContext.color;
		glColor4fv(mContext.color.mV);
	}
	if (mContext.colorMask != mNewContext.colorMask)
	{
		mContext.colorMask = mNewContext.colorMask;
		glColorMask(
			mContext.colorMask & (1 << 0),
			mContext.colorMask & (1 << 1),
			mContext.colorMask & (1 << 2),
			mContext.colorMask & (1 << 3));
	}
	if (mContext.alphaFunc != mNewContext.alphaFunc ||
		mContext.alphaVal != mNewContext.alphaVal)
	{
		mContext.alphaFunc = mNewContext.alphaFunc;
		mContext.alphaVal = mNewContext.alphaVal;
		if (mContext.alphaFunc == CF_DEFAULT)
		{
			glAlphaFunc(GL_GREATER, 0.01f);
		}
		else
		{
			glAlphaFunc(sGLCompareFunc[mContext.alphaFunc], mContext.alphaVal);
		}
	}
	if (LLGLState<GL_BLEND>::isEnabled() && (
		mContext.blendColorSFactor != mNewContext.blendColorSFactor ||
		mContext.blendAlphaSFactor != mNewContext.blendAlphaSFactor ||
		mContext.blendColorDFactor != mNewContext.blendColorDFactor ||
		mContext.blendAlphaDFactor != mNewContext.blendAlphaDFactor))
	{
		mContext.blendColorSFactor = mNewContext.blendColorSFactor;
		mContext.blendAlphaSFactor = mNewContext.blendAlphaSFactor;
		mContext.blendColorDFactor = mNewContext.blendColorDFactor;
		mContext.blendAlphaDFactor = mNewContext.blendAlphaDFactor;
		if (mContext.blendColorSFactor == mContext.blendAlphaSFactor &&
			mContext.blendColorDFactor == mContext.blendAlphaDFactor)
		{
			glBlendFunc(sGLBlendFactor[mContext.blendColorSFactor], sGLBlendFactor[mContext.blendColorDFactor]);
		}
		else
		{
			glBlendFuncSeparateEXT(sGLBlendFactor[mContext.blendColorSFactor], sGLBlendFactor[mContext.blendColorDFactor],
				sGLBlendFactor[mContext.blendAlphaSFactor], sGLBlendFactor[mContext.blendAlphaDFactor]);
		}
	}
	if (mContext.lineWidth != mNewContext.lineWidth)
	{
		mContext.lineWidth = mNewContext.lineWidth;
		glLineWidth(mContext.lineWidth);
	}
	if (mContext.pointSize != mNewContext.pointSize)
	{
		mContext.pointSize = mNewContext.pointSize;
		glPointSize(mContext.pointSize);
	}
	if (mContext.polygonMode[0] != mNewContext.polygonMode[0] || mContext.polygonMode[1] != mNewContext.polygonMode[1])
	{
		if (mNewContext.polygonMode[0] == mNewContext.polygonMode[1])
		{
			glPolygonMode(GL_FRONT_AND_BACK, sGLPolygonMode[mNewContext.polygonMode[0]]);
		}
		else
		{
			if (mContext.polygonMode[0] != mNewContext.polygonMode[0])
			{
				glPolygonMode(GL_FRONT, sGLPolygonMode[mNewContext.polygonMode[0]]);
			}
			if (mContext.polygonMode[1] != mNewContext.polygonMode[1])
			{
				glPolygonMode(GL_BACK, sGLPolygonMode[mNewContext.polygonMode[1]]);
			}
		}

		mContext.polygonMode[0] = mNewContext.polygonMode[0];
		mContext.polygonMode[1] = mNewContext.polygonMode[1];
	}
	if (mContext.polygonOffset[0] != mNewContext.polygonOffset[0] || mContext.polygonOffset[1] != mNewContext.polygonOffset[1])
	{
		mContext.polygonOffset[0] = mNewContext.polygonOffset[0];
		mContext.polygonOffset[1] = mNewContext.polygonOffset[1];
		glPolygonOffset(mContext.polygonOffset[0], mContext.polygonOffset[1]);
	}
	if (mContext.viewPort != mNewContext.viewPort)
	{
		mContext.viewPort = mNewContext.viewPort;
		glViewport(mContext.viewPort.mLeft, mContext.viewPort.mBottom, mContext.viewPort.getWidth(), mContext.viewPort.getHeight());
	}
	if (LLGLState<GL_SCISSOR_TEST>::isEnabled() && mContext.scissor != mNewContext.scissor)
	{
		mContext.scissor = mNewContext.scissor;
		glScissor(mContext.scissor.mLeft, mContext.scissor.mBottom, mContext.scissor.getWidth(), mContext.scissor.getHeight());
	}
}

U32 sLightMask = 0xFFFFFFFF;
void LLRender::syncLightState()
{
	if (!LLGLSLShader::sNoFixedFunction)
	{
		// Legacy
		if (mCurLegacyLightHash != mLightHash)
		{
			mCurLegacyLightHash = mLightHash;
			for (U32 i = 0; i < NUM_LIGHTS; i++)
			{
				const LLLightState* light = mLightState[i];
				const U32 idx = GL_LIGHT0 + i;
				const LLLightStateData& state = light->mState;

				if (light->mEnabled && (1 << i) & sLightMask) {
					glEnable(idx);
					if (mLightSpotTransformHash[i] != mCurLightSpotTransformHash[i] ||
						mLightPositionTransformHash[i] != mCurLightPositionTransformHash[i])
					{

						glPushAttrib(GL_TRANSFORM_BIT);
							glMatrixMode(GL_MODELVIEW);
							glPushMatrix();
							if (mLightPositionTransformHash[i] != mCurLightPositionTransformHash[i])
							{
								glLoadMatrixf(light->mPosMatrix.getF32ptr());
								glLightfv(idx, GL_POSITION, state.mPosition.mV);
							}
							if (mLightSpotTransformHash[i] != mCurLightSpotTransformHash[i])
							{
								glLoadMatrixf(light->mSpotMatrix.getF32ptr());
								glLightfv(idx, GL_SPOT_DIRECTION, state.mSpotDirection.mV);
							}
							mCurLightPositionTransformHash[i] = mLightPositionTransformHash[i];
							mCurLightSpotTransformHash[i] = mLightSpotTransformHash[i];

							glPopMatrix();
						glPopAttrib();
					}
					glLightfv(idx, GL_DIFFUSE, state.mDiffuse.mV);
					glLightfv(idx, GL_SPECULAR, state.mSpecular.mV);
					glLightf(idx, GL_CONSTANT_ATTENUATION, state.mConstantAtten);
					glLightf(idx, GL_LINEAR_ATTENUATION, state.mLinearAtten);
					glLightf(idx, GL_QUADRATIC_ATTENUATION, state.mQuadraticAtten);
					glLightf(idx, GL_SPOT_EXPONENT, state.mSpotExponent);
					glLightf(idx, GL_SPOT_CUTOFF, state.mSpotCutoff);
				}
				else
				{
					glDisable(idx);
				}
			}

			glLightModelfv(GL_LIGHT_MODEL_AMBIENT, mAmbientLightColor.mV);
		}
		return;
	}

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	if (!shader || (!shader->mFeatures.hasLighting && !shader->mFeatures.calculatesLighting))
	{
		return;
	}
	if (shader->mLightHash != mLightHash)
	{
		shader->mLightHash = mLightHash;

		LLVector3 attenuation[8];
		LLVector3 diffuse[8];

		for (U32 i = 0; i < NUM_LIGHTS; i++)
		{
			const LLLightState* light = mLightState[i];
			const LLLightStateData& state = light->mState;

			attenuation[i].set(state.mLinearAtten, state.mQuadraticAtten, state.mSpecular.mV[3]);
			diffuse[i].set((light->mEnabled && (1 << i) & sLightMask) ? state.mDiffuse.mV : LLVector3::zero.mV);

			if (mLightPositionTransformHash[i] != mCurLightPositionTransformHash[i])
			{
				LLVector4a pos;
				pos.loadua(state.mPosition.mV);
				light->mPosMatrix.rotate4(pos, pos);
				mCurLightPosition[i].set(pos.getF32ptr());
				mCurLightPositionTransformHash[i] = mLightPositionTransformHash[i];
			}
			// If state.mSpecular.mV[3] == 0.f then this light is a spotlight, thus update the direction...
			// Otherwise don't bother and leave the hash stale in case it turns into a spotlight later.
			if (state.mSpecular.mV[3] == 0.f && mLightSpotTransformHash[i] != mCurLightSpotTransformHash[i])
			{
				LLVector4a dir;
				dir.load3(state.mSpotDirection.mV);
				light->mSpotMatrix.rotate(dir, dir);
				mCurSpotDirection[i].set(dir.getF32ptr());
				mCurLightSpotTransformHash[i] = mLightSpotTransformHash[i];
			}
		}

		shader->uniform4fv(LLShaderMgr::LIGHT_POSITION, NUM_LIGHTS, mCurLightPosition[0].mV);
		shader->uniform3fv(LLShaderMgr::LIGHT_DIRECTION, NUM_LIGHTS, mCurSpotDirection[0].mV);
		shader->uniform3fv(LLShaderMgr::LIGHT_ATTENUATION, NUM_LIGHTS, attenuation[0].mV);
		shader->uniform3fv(LLShaderMgr::LIGHT_DIFFUSE, NUM_LIGHTS, diffuse[0].mV);
		shader->uniform4fv(LLShaderMgr::LIGHT_AMBIENT, 1, mAmbientLightColor.mV);
		//HACK -- duplicate sunlight color for compatibility with drivers that can't deal with multiple shader objects referencing the same uniform
		shader->uniform3fv(LLShaderMgr::SUNLIGHT_COLOR, 1, diffuse[0].mV);
	}
}

void LLRender::syncMatrices()
{
	stop_glerror();

	syncShaders();

	static const U32 name[] = 
	{
		LLShaderMgr::MODELVIEW_MATRIX,
		LLShaderMgr::PROJECTION_MATRIX,
		LLShaderMgr::TEXTURE_MATRIX0,
		LLShaderMgr::TEXTURE_MATRIX1,
		LLShaderMgr::TEXTURE_MATRIX2,
		LLShaderMgr::TEXTURE_MATRIX3,
	};

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	static LLMatrix4a cached_mvp;
	static U32 cached_mvp_mdv_hash = 0xFFFFFFFF;
	static U32 cached_mvp_proj_hash = 0xFFFFFFFF;
	
	static LLMatrix4a cached_normal;
	static U32 cached_normal_hash = 0xFFFFFFFF;

	if (shader)
	{
		llassert(shader);

		bool mvp_done = false;

		U32 i = MM_MODELVIEW;
		if (mMatHash[i] != shader->mMatHash[i])
		{ //update modelview, normal, and MVP
			const LLMatrix4a& mat = mMatrix[i][mMatIdx[i]];
			
			shader->uniformMatrix4fv(name[i], 1, GL_FALSE, mat.getF32ptr());
			shader->mMatHash[i] = mMatHash[i];

			//update normal matrix
			S32 loc = shader->getUniformLocation(LLShaderMgr::NORMAL_MATRIX);
			if (loc > -1)
			{
				if (cached_normal_hash != mMatHash[i])
				{
					cached_normal = mat;
					cached_normal.invert();
					cached_normal.transpose();
					cached_normal_hash = mMatHash[i];
				}
				
				const LLMatrix4a& norm = cached_normal;

				LLVector3 norms[3];
				norms[0].set(norm.getRow<0>().getF32ptr());
				norms[1].set(norm.getRow<1>().getF32ptr());
				norms[2].set(norm.getRow<2>().getF32ptr());

				shader->uniformMatrix3fv(LLShaderMgr::NORMAL_MATRIX, 1, GL_FALSE, norms[0].mV);
			}

			//update MVP matrix
			mvp_done = true;
			loc = shader->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
			if (loc > -1)
			{
				U32 proj = MM_PROJECTION;

				if (cached_mvp_mdv_hash != mMatHash[i] || cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
				{
					cached_mvp.setMul(mMatrix[proj][mMatIdx[proj]], mat);
					cached_mvp_mdv_hash = mMatHash[i];
					cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
				}

				shader->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX, 1, GL_FALSE, cached_mvp.getF32ptr());
			}
		}


		i = MM_PROJECTION;
		if (mMatHash[i] != shader->mMatHash[i])
		{ //update projection matrix, normal, and MVP
			const LLMatrix4a& mat = mMatrix[i][mMatIdx[i]];
			
			shader->uniformMatrix4fv(name[i], 1, GL_FALSE, mat.getF32ptr());
			shader->mMatHash[i] = mMatHash[i];

			if (!mvp_done)
			{
				//update MVP matrix
				S32 loc = shader->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
				if (loc > -1)
				{
					if (cached_mvp_mdv_hash != mMatHash[i] || cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
					{
						U32 mdv = MM_MODELVIEW;
						cached_mvp.setMul(mat,mMatrix[mdv][mMatIdx[mdv]]);
						cached_mvp_mdv_hash = mMatHash[MM_MODELVIEW];
						cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
					}

					shader->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX, 1, GL_FALSE, cached_mvp.getF32ptr());
				}
			}
		}

		for (i = MM_TEXTURE0; i < NUM_MATRIX_MODES; ++i)
		{
			if (mMatHash[i] != shader->mMatHash[i])
			{
				shader->uniformMatrix4fv(name[i], 1, GL_FALSE, mMatrix[i][mMatIdx[i]].getF32ptr());
				shader->mMatHash[i] = mMatHash[i];
			}
		}
	}
	else if (!LLGLSLShader::sNoFixedFunction)
	{
		GLenum mode[] = 
		{
			GL_MODELVIEW,
			GL_PROJECTION,
			GL_TEXTURE,
			GL_TEXTURE,
			GL_TEXTURE,
			GL_TEXTURE,
		};

		for (U32 i = 0; i < 2; ++i)
		{
			if (mMatHash[i] != mCurLegacyMatHash[i])
			{
				glMatrixMode(mode[i]);
				glLoadMatrixf(mMatrix[i][mMatIdx[i]].getF32ptr());
				mCurLegacyMatHash[i] = mMatHash[i];
			}
		}

		for (U32 i = 2; i < NUM_MATRIX_MODES; ++i)
		{
			if (mMatHash[i] != mCurLegacyMatHash[i])
			{
				gGL.getTexUnit(i-2)->activate();
				glMatrixMode(mode[i]);
				glLoadMatrixf(mMatrix[i][mMatIdx[i]].getF32ptr());
				mCurLegacyMatHash[i] = mMatHash[i];
			}
		}
	}

	//also sync light state
	syncLightState();
	//sync context.
	syncContextState();

	stop_glerror();
}

LLMatrix4a LLRender::genRot(const GLfloat& a, const LLVector4a& axis) const
{
	F32 r = a * DEG_TO_RAD;

	F32 c = cosf(r);
	F32 s = sinf(r);

	F32 ic = 1.f-c;

	const LLVector4a add1(c,axis[VZ]*s,-axis[VY]*s);	//1,z,-y
	const LLVector4a add2(-axis[VZ]*s,c,axis[VX]*s);	//-z,1,x
	const LLVector4a add3(axis[VY]*s,-axis[VX]*s,c);	//y,-x,1

	LLVector4a axis_x;
	axis_x.splat<0>(axis);
	LLVector4a axis_y;
	axis_y.splat<1>(axis);
	LLVector4a axis_z;
	axis_z.splat<2>(axis);

	LLVector4a c_axis;
	c_axis.setMul(axis,ic);

	LLMatrix4a rot_mat;
	rot_mat.getRow<0>().setMul(c_axis,axis_x);
	rot_mat.getRow<0>().add(add1);
	rot_mat.getRow<1>().setMul(c_axis,axis_y);
	rot_mat.getRow<1>().add(add2);
	rot_mat.getRow<2>().setMul(c_axis,axis_z);
	rot_mat.getRow<2>().add(add3);
	rot_mat.setRow<3>(LLVector4a(0,0,0,1));

	return rot_mat;
}
LLMatrix4a LLRender::genOrtho(const GLfloat& left, const GLfloat& right, const GLfloat& bottom, const GLfloat&  top, const GLfloat& zNear, const GLfloat& zFar) const
{
	LLMatrix4a ortho_mat;
	ortho_mat.setRow<0>(LLVector4a(2.f/(right-left),0,0));
	ortho_mat.setRow<1>(LLVector4a(0,2.f/(top-bottom),0));
	ortho_mat.setRow<2>(LLVector4a(0,0,-2.f/(zFar-zNear)));
	ortho_mat.setRow<3>(LLVector4a(-(right+left)/(right-left),-(top+bottom)/(top-bottom),-(zFar+zNear)/(zFar-zNear),1));

	return ortho_mat;
}

LLMatrix4a LLRender::genPersp(const GLfloat& fovy, const GLfloat& aspect, const GLfloat& zNear, const GLfloat& zFar) const
{
	GLfloat f = 1.f/tanf(DEG_TO_RAD*fovy/2.f);

	LLMatrix4a persp_mat;
	persp_mat.setRow<0>(LLVector4a(f/aspect,0,0));
	persp_mat.setRow<1>(LLVector4a(0,f,0));
	persp_mat.setRow<2>(LLVector4a(0,0,(zFar+zNear)/(zNear-zFar),-1.f));
	persp_mat.setRow<3>(LLVector4a(0,0,(2.f*zFar*zNear)/(zNear-zFar),0));

	return persp_mat;
}

LLMatrix4a LLRender::genLook(const LLVector3& pos_in, const LLVector3& dir_in, const LLVector3& up_in) const
{
	const LLVector4a pos(pos_in.mV[VX],pos_in.mV[VY],pos_in.mV[VZ],1.f);
	LLVector4a dir(dir_in.mV[VX],dir_in.mV[VY],dir_in.mV[VZ]);
	const LLVector4a up(up_in.mV[VX],up_in.mV[VY],up_in.mV[VZ]);

	LLVector4a left_norm;
	left_norm.setCross3(dir,up);
	left_norm.normalize3fast();
	LLVector4a up_norm;
	up_norm.setCross3(left_norm,dir);
	up_norm.normalize3fast();
	LLVector4a& dir_norm = dir;
	dir.normalize3fast();
	
	LLVector4a left_dot;
	left_dot.setAllDot3(left_norm,pos);
	left_dot.negate();
	LLVector4a up_dot;
	up_dot.setAllDot3(up_norm,pos);
	up_dot.negate();
	LLVector4a dir_dot;
	dir_dot.setAllDot3(dir_norm,pos);

	dir_norm.negate();

	LLMatrix4a lookat_mat;
	lookat_mat.setRow<0>(left_norm);
	lookat_mat.setRow<1>(up_norm);
	lookat_mat.setRow<2>(dir_norm);
	lookat_mat.setRow<3>(LLVector4a(0,0,0,1));

	lookat_mat.getRow<0>().copyComponent<3>(left_dot);
	lookat_mat.getRow<1>().copyComponent<3>(up_dot);
	lookat_mat.getRow<2>().copyComponent<3>(dir_dot);

	lookat_mat.transpose();

	return lookat_mat;
}

const LLMatrix4a& LLRender::genNDCtoWC() const
{
	static LLMatrix4a mat(
		LLVector4a(.5f,0,0,0),
		LLVector4a(0,.5f,0,0),
		LLVector4a(0,0,.5f,0),
		LLVector4a(.5f,.5f,.5f,1.f));
	return mat;
}

void LLRender::translatef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	if(	llabs(x) < F_APPROXIMATELY_ZERO &&
		llabs(y) < F_APPROXIMATELY_ZERO &&
		llabs(z) < F_APPROXIMATELY_ZERO)
	{
		return;
	}

	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].applyTranslation_affine(x,y,z);
	mMatHash[mMatrixMode]++;

}

void LLRender::scalef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	if(	(llabs(x-1.f)) < F_APPROXIMATELY_ZERO &&
		(llabs(y-1.f)) < F_APPROXIMATELY_ZERO &&
		(llabs(z-1.f)) < F_APPROXIMATELY_ZERO)
	{
		return;
	}
	flush();
	
	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].applyScale_affine(x,y,z);
		mMatHash[mMatrixMode]++;
	}
}

void LLRender::ortho(F32 left, F32 right, F32 bottom, F32 top, F32 zNear, F32 zFar)
{
	flush();

	LLMatrix4a ortho_mat;
	ortho_mat.setRow<0>(LLVector4a(2.f/(right-left),0,0));
	ortho_mat.setRow<1>(LLVector4a(0,2.f/(top-bottom),0));
	ortho_mat.setRow<2>(LLVector4a(0,0,-2.f/(zFar-zNear)));
	ortho_mat.setRow<3>(LLVector4a(-(right+left)/(right-left),-(top+bottom)/(top-bottom),-(zFar+zNear)/(zFar-zNear),1));	

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].mul_affine(ortho_mat);
	mMatHash[mMatrixMode]++;
}

void LLRender::rotatef(const LLMatrix4a& rot)
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].mul_affine(rot);
	mMatHash[mMatrixMode]++;
}

void LLRender::rotatef(const GLfloat& a, const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	if(	llabs(a) < F_APPROXIMATELY_ZERO ||
		llabs(a-360.f) < F_APPROXIMATELY_ZERO)
	{
		return;
	}
	
	flush();

	rotatef(genRot(a,x,y,z));
}

//LLRender::projectf & LLRender::unprojectf adapted from gluProject & gluUnproject in Mesa's GLU 9.0 library.
//  License/Copyright Statement:
/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */

bool LLRender::projectf(const LLVector3& object, const LLMatrix4a& modelview, const LLMatrix4a& projection, const LLRect& viewport, LLVector3& windowCoordinate)
{
	//Begin SSE intrinsics

	// Declare locals
	const LLVector4a obj_vector(object.mV[VX],object.mV[VY],object.mV[VZ]);
	const LLVector4a one(1.f);
	LLVector4a temp_vec;								//Scratch vector
	LLVector4a w;										//Splatted W-component.
	
	modelview.affineTransform(obj_vector, temp_vec);	//temp_vec = modelview * obj_vector;

	//Passing temp_matrix as v and res is safe. res not altered until after all other calculations
	projection.rotate4(temp_vec, temp_vec);				//temp_vec = projection * temp_vec

	w.splat<3>(temp_vec);								//w = temp_vec.wwww

	//If w == 0.f, use 1.f instead.
	LLVector4a div;
	div.setSelectWithMask( w.equal( _mm_setzero_ps() ), one, w );	//float div = (w[N] == 0.f ? 1.f : w[N]);
	temp_vec.div(div);									//temp_vec /= div;

	//Map x, y to range 0-1
	temp_vec.mul(.5f);
	temp_vec.add(.5f);

	LLVector4Logical mask = temp_vec.equal(_mm_setzero_ps());
	if(mask.areAllSet(LLVector4Logical::MASK_W))
		return false;

	//End SSE intrinsics

	//Window coordinates
	windowCoordinate[0]=temp_vec[VX]*viewport.getWidth()+viewport.mLeft;
	windowCoordinate[1]=temp_vec[VY]*viewport.getHeight()+viewport.mBottom;
	//This is only correct when glDepthRange(0.0, 1.0)
	windowCoordinate[2]=temp_vec[VZ];	
	
	return true;
}

bool LLRender::unprojectf(const LLVector3& windowCoordinate, const LLMatrix4a& modelview, const LLMatrix4a& projection, const LLRect& viewport, LLVector3& object)
{
	//Begin SSE intrinsics

	// Declare locals
	static const LLVector4a one(1.f);
	static const LLVector4a two(2.f);
	LLVector4a norm_view(
		((windowCoordinate.mV[VX] - (F32)viewport.mLeft) / (F32)viewport.getWidth()),
		((windowCoordinate.mV[VY] - (F32)viewport.mBottom) / (F32)viewport.getHeight()),
		windowCoordinate.mV[VZ],
		1.f);
	
	LLMatrix4a inv_mat;								//Inverse transformation matrix
	LLVector4a temp_vec;							//Scratch vector
	LLVector4a w;									//Splatted W-component.

	inv_mat.setMul(projection,modelview);			//inv_mat = projection*modelview

	float det = inv_mat.invert();

	//Normalize. -1.0 : +1.0
	norm_view.mul(two);								// norm_view *= vec4(.2f)
	norm_view.sub(one);								// norm_view -= vec4(1.f)

	inv_mat.rotate4(norm_view,temp_vec);			//inv_mat * norm_view

	w.splat<3>(temp_vec);							//w = temp_vec.wwww

	//If w == 0.f, use 1.f instead. Defer return if temp_vec.w == 0.f until after all SSE intrinsics.
	LLVector4a div;
	div.setSelectWithMask( w.equal( _mm_setzero_ps() ), one, w );	//float div = (w[N] == 0.f ? 1.f : w[N]);
	temp_vec.div(div);								//temp_vec /= div;

	LLVector4Logical mask = temp_vec.equal(_mm_setzero_ps());
	if(mask.areAllSet(LLVector4Logical::MASK_W))
		return false;

	//End SSE intrinsics

	if(det == 0.f)	
       return false;

	object.set(temp_vec.getF32ptr());

	return true;
}

void LLRender::pushMatrix()
{
	{
		if (mMatIdx[mMatrixMode] < LL_MATRIX_STACK_DEPTH-1)
		{
			mMatrix[mMatrixMode][mMatIdx[mMatrixMode]+1] = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
			++mMatIdx[mMatrixMode];
		}
		else
		{
			LL_WARNS() << "Matrix stack overflow." << LL_ENDL;
		}
	}
}

void LLRender::popMatrix()
{
	{
		if (mMatIdx[mMatrixMode] > 0)
		{
			if ( memcmp(mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].getF32ptr(), mMatrix[mMatrixMode][mMatIdx[mMatrixMode] - 1].getF32ptr(), sizeof(LLMatrix4a)) )
			{
				flush();
			}
			--mMatIdx[mMatrixMode];
			mMatHash[mMatrixMode]++;
		}
		else
		{
			flush();
			LL_WARNS() << "Matrix stack underflow." << LL_ENDL;
		}
	}
}

void LLRender::loadMatrix(const LLMatrix4a& mat)
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = mat;
	mMatHash[mMatrixMode]++;
}

void LLRender::multMatrix(const LLMatrix4a& mat)
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].mul_affine(mat); 
	mMatHash[mMatrixMode]++;
	
}

void LLRender::matrixMode(U32 mode)
{
	if (mode == MM_TEXTURE)
	{
		mode = MM_TEXTURE0 + gGL.getCurrentTexUnitIndex();
	}

	llassert(mode < NUM_MATRIX_MODES);
	mMatrixMode = mode;
}

U32 LLRender::getMatrixMode()
{
	if (mMatrixMode >= MM_TEXTURE0 && mMatrixMode <= MM_TEXTURE3)
	{ //always return MM_TEXTURE if current matrix mode points at any texture matrix
		return MM_TEXTURE;
	}

	return mMatrixMode;
}


void LLRender::loadIdentity()
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].setIdentity();
	mMatHash[mMatrixMode]++;
}

const LLMatrix4a& LLRender::getModelviewMatrix()
{
	return mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];
}

const LLMatrix4a& LLRender::getProjectionMatrix()
{
	return mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];
}

void LLRender::translateUI(F32 x, F32 y, F32 z)
{
	if (mUIOffset.empty())
	{
		LL_ERRS() << "Need to push a UI translation frame before offsetting" << LL_ENDL;
	}

	LLVector4a add(x,y,z);
	mUIOffset.back().add(add);
}

void LLRender::scaleUI(F32 x, F32 y, F32 z)
{
	if (mUIScale.empty())
	{
		LL_ERRS() << "Need to push a UI transformation frame before scaling." << LL_ENDL;
	}

	LLVector4a scale(x,y,z);
	mUIScale.back().mul(scale);
}

void LLRender::rotateUI(LLQuaternion& rot)
{
	if (mUIRotation.empty())
	{
		mUIRotation.push_back(rot);
	}
	else
	{
		mUIRotation.push_back(mUIRotation.back()*rot);
	}
}

void LLRender::pushUIMatrix()
{
	if (mUIOffset.empty())
	{
		mUIOffset.emplace_back(LLVector4a(0.f));
	}
	else
	{
		mUIOffset.push_back(mUIOffset.back());
	}
	
	if (mUIScale.empty())
	{
		mUIScale.emplace_back(LLVector4a(1.f));
	}
	else
	{
		mUIScale.push_back(mUIScale.back());
	}
	if (!mUIRotation.empty())
	{
		mUIRotation.push_back(mUIRotation.back());
	}
}

void LLRender::popUIMatrix()
{
	if (mUIOffset.empty() || mUIScale.empty())
	{
		LL_ERRS() << "UI offset or scale stack blown." << LL_ENDL;
	}
	mUIOffset.pop_back();
	mUIScale.pop_back();
	if (!mUIRotation.empty())
	{
		mUIRotation.pop_back();
	}
}

LLVector3 LLRender::getUITranslation()
{
	if (mUIOffset.empty())
	{
		return LLVector3(0,0,0);
	}
	return LLVector3(mUIOffset.back().getF32ptr());
}

LLVector3 LLRender::getUIScale()
{
	if (mUIScale.empty())
	{
		return LLVector3(1,1,1);
	}
	return LLVector3(mUIScale.back().getF32ptr());
}


void LLRender::loadUIIdentity()
{
	if (mUIOffset.empty() || mUIScale.empty())
	{
		LL_ERRS() << "Need to push UI translation frame before clearing offset." << LL_ENDL;
	}
	mUIOffset.back().splat(0.f);
	mUIScale.back().splat(1.f);
	if (!mUIRotation.empty())
		mUIRotation.push_back(LLQuaternion());
}

void LLRender::setColorMask(bool writeColor, bool writeAlpha)
{
	setColorMask(writeColor, writeColor, writeColor, writeAlpha);
}

void LLRender::setColorMask(bool writeColorR, bool writeColorG, bool writeColorB, bool writeAlpha)
{
	const U8 mask = (U8)writeColorR | ((U8)writeColorG << 1) | ((U8)writeColorB << 2) | ((U8)writeAlpha << 3);
	if (mNewContext.colorMask != mask || mDirty)
	{
		flush();
		mNewContext.colorMask = mask;
	}
}

void LLRender::setSceneBlendType(eBlendType type)
{
	switch (type) 
	{
		case BT_ALPHA:
			blendFunc(BF_SOURCE_ALPHA, BF_ONE_MINUS_SOURCE_ALPHA);
			break;
		case BT_ADD:
			blendFunc(BF_ONE, BF_ONE);
			break;
		case BT_ADD_WITH_ALPHA:
			blendFunc(BF_SOURCE_ALPHA, BF_ONE);
			break;
		case BT_MULT:
			blendFunc(BF_DEST_COLOR, BF_ZERO);
			break;
		case BT_MULT_ALPHA:
			blendFunc(BF_DEST_ALPHA, BF_ZERO);
			break;
		case BT_MULT_X2:
			blendFunc(BF_DEST_COLOR, BF_SOURCE_COLOR);
			break;
		case BT_REPLACE:
			blendFunc(BF_ONE, BF_ZERO);
			break;
		default:
			LL_ERRS() << "Unknown Scene Blend Type: " << type << LL_ENDL;
			break;
	}
}

void LLRender::setAlphaRejectSettings(eCompareFunc func, F32 value)
{
	if (LLGLSLShader::sNoFixedFunction)
	{ //glAlphaFunc is deprecated in OpenGL 3.3
		return;
	}

	if (mNewContext.alphaFunc != func ||
		mNewContext.alphaVal != value || mDirty)
	{
		flush();
		mNewContext.alphaFunc = func;
		mNewContext.alphaVal = value;
	}

	/*if (gDebugGL)
	{ //make sure cached state is correct
		GLint cur_func = 0;
		glGetIntegerv(GL_ALPHA_TEST_FUNC, &cur_func);

		if (func == CF_DEFAULT)
		{
			func = CF_GREATER;
		}

		if (cur_func != sGLCompareFunc[func])
		{
			LL_ERRS() << "Alpha test function corrupted!" << LL_ENDL;
		}

		F32 ref = 0.f;
		glGetFloatv(GL_ALPHA_TEST_REF, &ref);

		if (ref != value)
		{
			LL_ERRS() << "Alpha test value corrupted!" << LL_ENDL;
		}
	}*/
}

void LLRender::setViewport(const LLRect& rect)
{
	if (mNewContext.viewPort != rect || mDirty)
	{
		flush();
		mNewContext.viewPort = rect;
	}
}

void LLRender::setScissor(const LLRect& rect)
{
	if (mNewContext.scissor != rect || mDirty)
	{
		if (LLGLState<GL_SCISSOR_TEST>::isEnabled())
		{
			flush();
		}
		mNewContext.scissor = rect;
	}
}

void check_blend_funcs()
{
	llassert_always(gGL.mNewContext.blendColorSFactor == LLRender::BF_SOURCE_ALPHA );
	llassert_always(gGL.mNewContext.blendAlphaSFactor == LLRender::BF_SOURCE_ALPHA );
	llassert_always(gGL.mNewContext.blendColorDFactor == LLRender::BF_ONE_MINUS_SOURCE_ALPHA);
	llassert_always(gGL.mNewContext.blendAlphaDFactor == LLRender::BF_ONE_MINUS_SOURCE_ALPHA );
}

void LLRender::blendFunc(eBlendFactor sfactor, eBlendFactor dfactor)
{
	llassert(sfactor < BF_UNDEF);
	llassert(dfactor < BF_UNDEF);
	if (mNewContext.blendColorSFactor != sfactor || mNewContext.blendColorDFactor != dfactor ||
		mNewContext.blendAlphaSFactor != sfactor || mNewContext.blendAlphaDFactor != dfactor || mDirty)
	{
		if (LLGLState<GL_BLEND>::isEnabled())
		{
			flush();
		}
		mNewContext.blendColorSFactor = sfactor;
		mNewContext.blendAlphaSFactor = sfactor;
		mNewContext.blendColorDFactor = dfactor;
		mNewContext.blendAlphaDFactor = dfactor;
	}
}

void LLRender::blendFunc(eBlendFactor color_sfactor, eBlendFactor color_dfactor,
			 eBlendFactor alpha_sfactor, eBlendFactor alpha_dfactor)
{
	llassert(color_sfactor < BF_UNDEF);
	llassert(color_dfactor < BF_UNDEF);
	llassert(alpha_sfactor < BF_UNDEF);
	llassert(alpha_dfactor < BF_UNDEF);
	if (!gGLManager.mHasBlendFuncSeparate)
	{
		LL_WARNS_ONCE("render") << "no glBlendFuncSeparateEXT(), using color-only blend func" << LL_ENDL;
		blendFunc(color_sfactor, color_dfactor);
		return;
	}
	if (mNewContext.blendColorSFactor != color_sfactor || mNewContext.blendColorDFactor != color_dfactor ||
		mNewContext.blendAlphaSFactor != alpha_sfactor || mNewContext.blendAlphaDFactor != alpha_dfactor || mDirty)
	{
		if (LLGLState<GL_BLEND>::isEnabled())
		{
			flush();
		}
		mNewContext.blendColorSFactor = color_sfactor;
		mNewContext.blendAlphaSFactor = alpha_sfactor;
		mNewContext.blendColorDFactor = color_dfactor;
		mNewContext.blendAlphaDFactor = alpha_dfactor;
	}
}

LLTexUnit* LLRender::getTexUnit(U32 index)
{
	if (index < mTexUnits.size())
	{
		return mTexUnits[index];
	}
	else 
	{
		LL_DEBUGS() << "Non-existing texture unit layer requested: " << index << LL_ENDL;
		return mDummyTexUnit;
	}
}

LLLightState* LLRender::getLight(U32 index)
{
	if (index < mLightState.size())
	{
		return mLightState[index];
	}
	
	return NULL;
}

void LLRender::setAmbientLightColor(const LLColor4& color)
{
	if (color != mAmbientLightColor || mDirty)
	{
		++mLightHash;
		mAmbientLightColor = color;
	}
}

void LLRender::setLineWidth(F32 line_width)
{
	if (LLRender::sGLCoreProfile)
	{
		mNewContext.lineWidth = 1.f;
		return;
	}
	if (mNewContext.lineWidth != line_width || mDirty)
	{
		if (mMode == LLRender::LINES || mMode == LLRender::LINE_STRIP)
		{
			flush();
		}
		mNewContext.lineWidth = line_width;
	}
}

void LLRender::setPointSize(F32 point_size)
{
	if (mNewContext.pointSize != point_size || mDirty)
	{
		if (mMode == LLRender::POINTS)
		{
			flush();
		}
		mNewContext.pointSize = point_size;
	}
}

void LLRender::setPolygonMode(ePolygonFaceType type, ePolygonMode mode)
{
	ePolygonMode newMode[] = {
		(type == PF_FRONT_AND_BACK || type == PF_FRONT) ? mode : mNewContext.polygonMode[0],
		(type == PF_FRONT_AND_BACK || type == PF_BACK) ? mode : mNewContext.polygonMode[1]
	};

	if (newMode[0] != mNewContext.polygonMode[0] || newMode[1] != mNewContext.polygonMode[1] || mDirty)
	{
		flush();
		mNewContext.polygonMode[0] = newMode[0];
		mNewContext.polygonMode[1] = newMode[1];
	}
}

void LLRender::setPolygonOffset(F32 factor, F32 bias)
{
	if (factor != mNewContext.polygonOffset[0] ||
		bias != mNewContext.polygonOffset[1] || mDirty)
	{
		if (LLGLState<GL_POLYGON_OFFSET_FILL>::isEnabled() ||
			LLGLState<GL_POLYGON_OFFSET_LINE>::isEnabled() /*||
			Unused: LLGLState<GL_POLYGON_OFFSET_POINT>::isEnabled()*/ )
		{
			flush();
		}
		mNewContext.polygonOffset[0] = factor;
		mNewContext.polygonOffset[1] = bias;
	}
}

bool LLRender::verifyTexUnitActive(U32 unitToVerify)
{
	if (getCurrentTexUnitIndex() == unitToVerify)
	{
		return true;
	}
	else 
	{
		LL_WARNS() << "TexUnit currently active: " << getCurrentTexUnitIndex() << " (expecting " << unitToVerify << ")" << LL_ENDL;
		return false;
	}
}

void LLRender::clearErrors()
{
	while (glGetError())
	{
		//loop until no more error flags left
	}
}

void LLRender::resetSyncHashes() {
	memset(&mLightHash, 0, sizeof(mLightHash));
	memset(&mCurLegacyLightHash, 0xFF, sizeof(mCurLegacyLightHash));
	memset(mMatHash, 0, sizeof(mMatHash));
	memset(mCurLegacyMatHash, 0xFF, sizeof(mCurLegacyMatHash));
	memset(mLightPositionTransformHash, 0, sizeof(mLightPositionTransformHash));
	memset(mCurLightPositionTransformHash, 0xFF, sizeof(mCurLightPositionTransformHash));
	memset(mLightSpotTransformHash, 0, sizeof(mLightSpotTransformHash));
	memset(mCurLightSpotTransformHash, 0xFF, sizeof(mLightSpotTransformHash));
}

void LLRender::begin(const GLuint& mode)
{
	if (mode != mMode)
	{
		if (mMode == LLRender::LINES ||
			mMode == LLRender::TRIANGLES ||
			mMode == LLRender::POINTS ||
			mMode == LLRender::TRIANGLE_STRIP )
		{
			flush();
		}
		else if (mCount != 0)
		{
			LL_ERRS() << "gGL.begin() called redundantly." << LL_ENDL;
		}
		
		mMode = mode;
	}
}

void LLRender::end()
{ 
	if (mCount == 0)
	{
		return;
		//IMM_ERRS << "GL begin and end called with no vertices specified." << LL_ENDL;
	}

	if ((mMode != LLRender::LINES &&
		mMode != LLRender::TRIANGLES &&
		mMode != LLRender::POINTS &&
		mMode != LLRender::TRIANGLE_STRIP) ||
		mCount > 2048)
	{
		flush();
	}
	else if (mMode == LLRender::TRIANGLE_STRIP)
	{
		mPrimitiveReset = true;
	}
}

void LLRender::flush()
{
	if (mCount > 0)
	{
#if 0
		if (!glIsEnabled(GL_VERTEX_ARRAY))
		{
			LL_ERRS() << "foo 1" << LL_ENDL;
		}

		if (!glIsEnabled(GL_COLOR_ARRAY))
		{
			LL_ERRS() << "foo 2" << LL_ENDL;
		}

		if (!glIsEnabled(GL_TEXTURE_COORD_ARRAY))
		{
			LL_ERRS() << "foo 3" << LL_ENDL;
		}

		if (glIsEnabled(GL_NORMAL_ARRAY))
		{
			LL_ERRS() << "foo 7" << LL_ENDL;
		}

		GLvoid* pointer;

		glGetPointerv(GL_VERTEX_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].v))
		{
			LL_ERRS() << "foo 4" << LL_ENDL;
		}

		glGetPointerv(GL_COLOR_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].c))
		{
			LL_ERRS() << "foo 5" << LL_ENDL;
		}

		glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].uv))
		{
			LL_ERRS() << "foo 6" << LL_ENDL;
		}
#endif
				
		if (!mUIOffset.empty())
		{
			sUICalls++;
			sUIVerts += mCount;
		}
		
		if (gDebugGL)
		{
			if (mMode == LLRender::TRIANGLES)
			{
				if (mCount%3 != 0)
				{
					LL_ERRS() << "Incomplete triangle rendered." << LL_ENDL;
				}
			}
			
			if (mMode == LLRender::LINES)
			{
				if (mCount%2 != 0)
				{
					LL_ERRS() << "Incomplete line rendered." << LL_ENDL;
				}
			}
		}

		//store mCount in a local variable to avoid re-entrance (drawArrays may call flush)
		U32 count = mCount;
		mCount = 0;

		if (mBuffer->useVBOs() && !mBuffer->isLocked())
		{ //hack to only flush the part of the buffer that was updated (relies on stream draw using buffersubdata)
			mBuffer->getVertexStrider(mVerticesp, 0, count);
			mBuffer->getTexCoord0Strider(mTexcoordsp, 0, count);
			mBuffer->getColorStrider(mColorsp, 0, count);
		}
		
		mBuffer->flush();
		mBuffer->setBuffer(immediate_mask);

		mBuffer->drawArrays(mMode, 0, count);
		
		mVerticesp[0] = mVerticesp[count];
		mTexcoordsp[0] = mTexcoordsp[count];
		mColorsp[0] = mColorsp[count];
		
		mCount = 0;
		mPrimitiveReset = false;
	}
}

void LLRender::vertex4a(const LLVector4a& vertex)
{ 
	//the range of mVerticesp, mColorsp and mTexcoordsp is [0, 4095]
	if (mCount > 2048)
	{ //break when buffer gets reasonably full to keep GL command buffers happy and avoid overflow below
		switch (mMode)
		{
			case LLRender::POINTS: flush(); break;
			case LLRender::TRIANGLES: if (mCount%3==0) flush(); break;
			case LLRender::LINES: if (mCount%2 == 0) flush(); break;
			case LLRender::TRIANGLE_STRIP:
			{
				LLVector4a vert[] = { mVerticesp[mCount - 2], mVerticesp[mCount - 1], mVerticesp[mCount] };
				LLColor4U col[] = { mColorsp[mCount - 2], mColorsp[mCount - 1], mColorsp[mCount] };
				LLVector2 tc[] = { mTexcoordsp[mCount - 2], mTexcoordsp[mCount - 1], mTexcoordsp[mCount] };
				flush();
				for (int i = 0; i < LL_ARRAY_SIZE(vert); ++i)
				{
					mVerticesp[i] = vert[i];
					mColorsp[i] = col[i];
					mTexcoordsp[i] = tc[i];
				}
				mCount = 2;
				break;
			}
		}
	}
			
	if (mCount > 4094)
	{
	//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
	}

	if (mUIOffset.empty())
	{
		if (!mUIRotation.empty() && mUIRotation.back().isNotIdentity())
		{
			LLVector4 vert(vertex.getF32ptr());
			mVerticesp[mCount].loadua((vert*mUIRotation.back()).mV);
		}
		else
		{
			mVerticesp[mCount] = vertex;
		}
	}
	else
	{
		if (!mUIRotation.empty() && mUIRotation.back().isNotIdentity())
		{
			LLVector4 vert(vertex.getF32ptr());
			vert = vert * mUIRotation.back();
			LLVector4a postrot_vert;
			postrot_vert.loadua(vert.mV);
			mVerticesp[mCount].setAdd(postrot_vert, mUIOffset.back());
			mVerticesp[mCount].mul(mUIScale.back());
		}
		else
		{
			//LLVector3 vert = (LLVector3(x,y,z)+mUIOffset.back()).scaledVec(mUIScale.back());
			mVerticesp[mCount].setAdd(vertex, mUIOffset.back());
			mVerticesp[mCount].mul(mUIScale.back());
		}
	}

	mCount++;
	mVerticesp[mCount] = mVerticesp[mCount-1];
	mColorsp[mCount] = mColorsp[mCount-1];
	mTexcoordsp[mCount] = mTexcoordsp[mCount-1];

	if (mPrimitiveReset && mCount)
	{
		mCount++;
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	mPrimitiveReset = false;
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, S32 vert_count)
{
	if (mCount + vert_count > 4094)
	{
		//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = verts[0];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
		++mCount;
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	mVerticesp.copyArray(mCount, verts, vert_count);

	for (S32 i = 0; i < vert_count; i++)
	{
		mCount++;
		mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
		mColorsp[mCount] = mColorsp[mCount-1];
	}

	if (mCount > 0) // ND: Guard against crashes if mCount is zero, yes it can happen
		mVerticesp[mCount] = mVerticesp[mCount-1];

	mPrimitiveReset = false;
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, S32 vert_count)
{
	if (mCount + vert_count > 4094)
	{
		//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = verts[0];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = uvs[0];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
		++mCount;
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	mVerticesp.copyArray(mCount, verts, vert_count);
	mTexcoordsp.copyArray(mCount, uvs, vert_count);

	for (S32 i = 0; i < vert_count; i++)
	{
		mCount++;
		mColorsp[mCount] = mColorsp[mCount-1];
	}

	if (mCount > 0)
	{
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	mPrimitiveReset = false;
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, LLColor4U* colors, S32 vert_count)
{
	if (mCount + vert_count > 4094)
	{
		//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = verts[0];
		mColorsp[mCount] = colors[mCount - 1];
		mTexcoordsp[mCount] = uvs[0];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
		++mCount;
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	// Singu Note: Batch copies instead of iterating.
	mVerticesp.copyArray(mCount, verts, vert_count);
	mTexcoordsp.copyArray(mCount, uvs, vert_count);
	mColorsp.copyArray(mCount, colors, vert_count);
	mCount += vert_count;

	if (mCount > 0)
	{
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
		mColorsp[mCount] = mColorsp[mCount - 1];
	}

	mPrimitiveReset = false;
}

void LLRender::texCoord2f(const GLfloat& x, const GLfloat& y)
{ 
	mTexcoordsp[mCount] = LLVector2(x,y);
}

void LLRender::texCoord2i(const GLint& x, const GLint& y)
{ 
	texCoord2f((GLfloat) x, (GLfloat) y);
}

void LLRender::texCoord2fv(const GLfloat* tc)
{ 
	texCoord2f(tc[0], tc[1]);
}

void LLRender::color4ub(const GLubyte& r, const GLubyte& g, const GLubyte& b, const GLubyte& a)
{
	if (!LLGLSLShader::sCurBoundShaderPtr ||
		LLGLSLShader::sCurBoundShaderPtr->mAttributeMask & LLVertexBuffer::MAP_COLOR)
	{
		mColorsp[mCount] = LLColor4U(r,g,b,a);
	}
	else
	{ //not using shaders or shader reads color from a uniform
		diffuseColor4ub(r,g,b,a);
	}
}
void LLRender::color4ubv(const GLubyte* c)
{
	color4ub(c[0], c[1], c[2], c[3]);
}

void LLRender::color4f(const GLfloat& r, const GLfloat& g, const GLfloat& b, const GLfloat& a)
{
	color4ub((GLubyte) (llclamp(r, 0.f, 1.f)*255),
		(GLubyte) (llclamp(g, 0.f, 1.f)*255),
		(GLubyte) (llclamp(b, 0.f, 1.f)*255),
		(GLubyte) (llclamp(a, 0.f, 1.f)*255));
}

void LLRender::color4fv(const GLfloat* c)
{ 
	color4f(c[0],c[1],c[2],c[3]);
}

void LLRender::color3f(const GLfloat& r, const GLfloat& g, const GLfloat& b)
{ 
	color4f(r,g,b,1);
}

void LLRender::color3fv(const GLfloat* c)
{ 
	color4f(c[0],c[1],c[2],1);
}

void LLRender::diffuseColor3f(F32 r, F32 g, F32 b)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,1.f);
	}
	else if (r != mNewContext.color.mV[0] || g != mNewContext.color.mV[1] || b != mNewContext.color.mV[2] || mNewContext.color.mV[3] != 1.f || mDirty)
	{
		flush();
		mNewContext.color.set(r, g, b, 1.f);
	}
}

void LLRender::diffuseColor3fv(const F32* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0], c[1], c[2], 1.f);
	}
	else if (c[0] != mNewContext.color.mV[0] || c[1] != mNewContext.color.mV[1] || c[2] != mNewContext.color.mV[2] || mNewContext.color.mV[3] != 1.f || mDirty)
	{
		flush();
		mNewContext.color.set(c[0], c[1], c[2], 1.f);
	}
}

void LLRender::diffuseColor4f(F32 r, F32 g, F32 b, F32 a)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,a);
	}
	else if (r != mNewContext.color.mV[0] || g != mNewContext.color.mV[1] || b != mNewContext.color.mV[2] || a != mNewContext.color.mV[3] || mDirty)
	{
		flush();
		mNewContext.color = { r, g, b, a };
	}
}

void LLRender::diffuseColor4fv(const F32* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, c);
	}
	else if (c[0] != mNewContext.color.mV[0] || c[1] != mNewContext.color.mV[1] || c[2] != mNewContext.color.mV[2] || c[3] != mNewContext.color.mV[3] || mDirty)
	{
		flush();
		mNewContext.color.set(c);
	}
}

void LLRender::diffuseColor4ubv(const U8* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f);
	}
	else if (c[0] / 255.f != mNewContext.color.mV[0] || c[1] / 255.f != mNewContext.color.mV[1] || c[2] / 255.f != mNewContext.color.mV[2] || c[3] / 255.f != mNewContext.color.mV[3] || mDirty)
	{
		flush();
		mNewContext.color.mV[0] = c[0] / 255.f;
		mNewContext.color.mV[1] = c[1] / 255.f;
		mNewContext.color.mV[2] = c[2] / 255.f;
		mNewContext.color.mV[3] = c[3] / 255.f;
	}
}

void LLRender::diffuseColor4ub(U8 r, U8 g, U8 b, U8 a)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r/255.f, g/255.f, b/255.f, a/255.f);
	}
	else if (r / 255.f != mNewContext.color.mV[0] || g / 255.f != mNewContext.color.mV[1] || b / 255.f != mNewContext.color.mV[2] || a / 255.f != mNewContext.color.mV[3] || mDirty)
	{
		flush();
		mNewContext.color.mV[0] = r / 255.f;
		mNewContext.color.mV[1] = g / 255.f;
		mNewContext.color.mV[2] = b / 255.f;
		mNewContext.color.mV[3] = a / 255.f;
	}
}

void LLRender::debugTexUnits(void)
{
	LL_INFOS("TextureUnit") << "Active TexUnit: " << getCurrentTexUnitIndex() << LL_ENDL;
	std::string active_enabled = "false";
	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		if (getTexUnit(i)->mCurrTexType != LLTexUnit::TT_NONE)
		{
			if (i == getCurrentTexUnitIndex()) active_enabled = "true";
			LL_INFOS("TextureUnit") << "TexUnit: " << i << " Enabled" << LL_ENDL;
			LL_INFOS("TextureUnit") << "Enabled As: " ;
			switch (getTexUnit(i)->mCurrTexType)
			{
				case LLTexUnit::TT_TEXTURE:
					LL_CONT << "Texture 2D";
					break;
				case LLTexUnit::TT_CUBE_MAP:
					LL_CONT << "Cube Map";
					break;
				default:
					LL_CONT << "ARGH!!! NONE!";
					break;
			}
			LL_CONT << ", Texture Bound: " << getTexUnit(i)->mCurrTexture << LL_ENDL;
		}
	}
	LL_INFOS("TextureUnit") << "Active TexUnit Enabled : " << active_enabled << LL_ENDL;
}

