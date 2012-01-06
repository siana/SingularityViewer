/** 
 * @file llpostprocess.cpp
 * @brief LLPostProcess class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include "llpostprocess.h"
#include "llglslshader.h"
#include "llsdserialize.h"
#include "llrender.h"
#include "llvertexbuffer.h"

#include "lldir.h"
extern LLGLSLShader			gPostColorFilterProgram;
extern LLGLSLShader			gPostNightVisionProgram;
extern LLGLSLShader			gPostGaussianBlurProgram;

static const unsigned int NOISE_SIZE = 512;

/// CALCULATING LUMINANCE (Using NTSC lum weights)
/// http://en.wikipedia.org/wiki/Luma_%28video%29
static const float LUMINANCE_R = 0.299f;
static const float LUMINANCE_G = 0.587f;
static const float LUMINANCE_B = 0.114f;

static const char * const XML_FILENAME = "postprocesseffects.xml";

LLPostProcess::LLPostProcess(void) : 
					mVBO(NULL),  
					mAllEffects(LLSD::emptyMap()),
					mScreenWidth(1), mScreenHeight(1)
{
	mSceneRenderTexture = NULL ; 
	mNoiseTexture = NULL ;
					
	/*  Do nothing.  Needs to be updated to use our current shader system, and to work with the move into llrender.*/
	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", XML_FILENAME));
	LL_DEBUGS2("AppInit", "Shaders") << "Loading PostProcess Effects settings from " << pathName << LL_ENDL;

	llifstream effectsXML(pathName);

	if (effectsXML)
	{
		LLPointer<LLSDParser> parser = new LLSDXMLParser();

		parser->parse(effectsXML, mAllEffects, LLSDSerialize::SIZE_UNLIMITED);
	}

	if (!mAllEffects.has("default"))
	{
		LLSD & defaultEffect = (mAllEffects["default"] = LLSD::emptyMap());

		/*defaultEffect["enable_night_vision"] = LLSD::Boolean(false);
		defaultEffect["enable_color_filter"] = LLSD::Boolean(false);*/

		/// NVG Defaults
		defaultEffect["brightness_multiplier"] = 3.0;
		defaultEffect["noise_size"] = 25.0;
		defaultEffect["noise_strength"] = 0.4;

		// TODO BTest potentially add this to tweaks?
		mNoiseTextureScale = 1.0f;

		/// Color Filter Defaults
		defaultEffect["gamma"] = 1.0;
		defaultEffect["brightness"] = 1.0;
		defaultEffect["contrast"] = 1.0;
		defaultEffect["saturation"] = 1.0;

		LLSD& contrastBase = (defaultEffect["contrast_base"] = LLSD::emptyArray());
		contrastBase.append(1.0);
		contrastBase.append(1.0);
		contrastBase.append(1.0);
		contrastBase.append(0.5);

		defaultEffect["gauss_blur_passes"] = 2;
	}

	setSelectedEffect("default");
	//*/
}

LLPostProcess::~LLPostProcess(void)
{
	invalidate() ;
}

/*static*/void LLPostProcess::cleanupClass()
{
	if(instanceExists())
		getInstance()->invalidate() ;
}

void LLPostProcess::setSelectedEffect(std::string const & effectName)
{
	mSelectedEffectName = effectName;
	static_cast<LLSD &>(tweaks) = mAllEffects[effectName];
}

void LLPostProcess::saveEffect(std::string const & effectName)
{
	mAllEffects[effectName] = tweaks;

	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", XML_FILENAME));
	//llinfos << "Saving PostProcess Effects settings to " << pathName << llendl;

	llofstream effectsXML(pathName);

	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();

	formatter->format(mAllEffects, effectsXML);
}
void LLPostProcess::invalidate()
{
	mSceneRenderTexture = NULL ;
	mNoiseTexture = NULL ;
	mVBO = NULL ;
}

void LLPostProcess::apply(unsigned int width, unsigned int height)
{
	if(shadersEnabled())
	{
		if (mVBO.isNull() || width != mScreenWidth || height != mScreenHeight)
		{
			initialize(width, height);
		}
		doEffects();
	}
}

void LLPostProcess::initialize(unsigned int width, unsigned int height)
{
	invalidate();
	mScreenWidth = width;
	mScreenHeight = height;

	createScreenTexture();

	//Setup our VBO.
	{
		mVBO = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1,3);
		mVBO->allocateBuffer(4,0,TRUE);

		LLStrider<LLVector3> v;
		LLStrider<LLVector2> uv1;
		LLStrider<LLVector2> uv2;

		mVBO->getVertexStrider(v);
		mVBO->getTexCoord0Strider(uv1);
		mVBO->getTexCoord1Strider(uv2);
	
		v[0] = LLVector3( uv2[0] = uv1[0] = LLVector2(0, 0) );
		v[1] = LLVector3( uv2[1] = uv1[1] = LLVector2(0, mScreenHeight) );
		v[2] = LLVector3( uv2[2] = uv1[2] = LLVector2(mScreenWidth, 0) );
		v[3] = LLVector3( uv2[3] = uv1[3] = LLVector2(mScreenWidth, mScreenHeight) );

		mVBO->flush();
	}

	checkError();
	createNoiseTexture();
	checkError();
}

inline bool LLPostProcess::shadersEnabled(void)
{
	return (tweaks.useColorFilter().asBoolean() ||
			tweaks.useNightVisionShader().asBoolean() ||
			tweaks.useGaussBlurFilter().asBoolean() );
}

void LLPostProcess::applyShaders(void)
{
	bool copy_buffer = false;
	if (tweaks.useColorFilter())
	{
		applyColorFilterShader();
		checkError();
		copy_buffer = true;
	}
	if (tweaks.useGaussBlurFilter())
	{
		/// If any of the above shaders have been called update the frame buffer;
		if (copy_buffer)
			copyFrameBuffer();
		applyGaussBlurShader();
		checkError();
		copy_buffer = true;
	}
	if (tweaks.useNightVisionShader())
	{
		/// If any of the above shaders have been called update the frame buffer;
		if (copy_buffer)
			copyFrameBuffer();
		applyNightVisionShader();
		checkError();
		copy_buffer = true;
	}
}

void LLPostProcess::applyColorFilterShader(void)
{	
	if(gPostColorFilterProgram.mProgramObject == 0)
		return;

	gPostColorFilterProgram.bind();

	gGL.getTexUnit(0)->bind(mSceneRenderTexture);

	gPostColorFilterProgram.uniform1f("gamma", tweaks.getGamma());
	gPostColorFilterProgram.uniform1f("brightness", tweaks.getBrightness());
	gPostColorFilterProgram.uniform1f("contrast", tweaks.getContrast());
	float baseI = (tweaks.getContrastBaseR() + tweaks.getContrastBaseG() + tweaks.getContrastBaseB()) / 3.0f;
	baseI = tweaks.getContrastBaseIntensity() / ((baseI < 0.001f) ? 0.001f : baseI);
	float baseR = tweaks.getContrastBaseR() * baseI;
	float baseG = tweaks.getContrastBaseG() * baseI;
	float baseB = tweaks.getContrastBaseB() * baseI;
	gPostColorFilterProgram.uniform3fv("contrastBase", 1, LLVector3(baseR, baseG, baseB).mV);
	gPostColorFilterProgram.uniform1f("saturation", tweaks.getSaturation());
	gPostColorFilterProgram.uniform3fv("lumWeights", 1, LLVector3(LUMINANCE_R, LUMINANCE_G, LUMINANCE_B).mV);
	
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_REPLACE);
	LLGLDepthTest depth(GL_FALSE);
		
	/// Draw a screen space quad
	drawOrthoQuad(QUAD_NORMAL);
	gPostColorFilterProgram.unbind();
}

void LLPostProcess::applyNightVisionShader(void)
{	
	if(gPostNightVisionProgram.mProgramObject == 0)
		return;

	gPostNightVisionProgram.bind();

	gGL.getTexUnit(0)->bind(mSceneRenderTexture);
	gGL.getTexUnit(1)->bind(mNoiseTexture);
	
	gPostNightVisionProgram.uniform1f("brightMult", tweaks.getBrightMult());
	gPostNightVisionProgram.uniform1f("noiseStrength", tweaks.getNoiseStrength());
	mNoiseTextureScale = 0.001f + ((100.f - tweaks.getNoiseSize()) / 100.f);
	mNoiseTextureScale *= (mScreenHeight / NOISE_SIZE);

	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_REPLACE);
	LLGLDepthTest depth(GL_FALSE);
		
	/// Draw a screen space quad
	drawOrthoQuad(QUAD_NOISE);
	gPostNightVisionProgram.unbind();
}

void LLPostProcess::applyGaussBlurShader(void)
{
	int pass_count = tweaks.getGaussBlurPasses();
	if(!pass_count || gPostGaussianBlurProgram.mProgramObject == 0)
		return;

	gPostGaussianBlurProgram.bind();

	gGL.getTexUnit(0)->bind(mSceneRenderTexture);

	LLGLEnable blend(GL_BLEND);
	LLGLDepthTest depth(GL_FALSE);
	gGL.setSceneBlendType(LLRender::BT_REPLACE);
	GLint horiz_pass = gPostGaussianBlurProgram.getUniformLocation("horizontalPass");
	for(int i = 0;i<pass_count;++i)
	{
		for(int j = 0;j<2;++j)
		{
			if(i || j)
				copyFrameBuffer();		
			glUniform1iARB(horiz_pass, j);
			drawOrthoQuad(QUAD_NORMAL);
		}
	}
	gPostGaussianBlurProgram.unbind();
}

void LLPostProcess::doEffects(void)
{
	LLVertexBuffer::unbind();

	/// Copy the screen buffer to the render texture
	copyFrameBuffer();

	/// Clear the frame buffer.
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	LLGLDisable(GL_DEPTH_TEST);

	/// Change to an orthogonal view
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho( 0.f, (GLdouble) mScreenWidth, 0.f, (GLdouble) mScreenHeight, -1.f, 1.f );
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();
	
	applyShaders();
	checkError();

	LLGLSLShader::bindNoShader();

	/// Change to a perspective view
	gGL.flush();
	gGL.matrixMode( LLRender::MM_PROJECTION );
	gGL.popMatrix();
	gGL.matrixMode( LLRender::MM_MODELVIEW );
	gGL.popMatrix();

	gGL.getTexUnit(1)->disable();
	checkError();
}

void LLPostProcess::copyFrameBuffer()
{
	gGL.getTexUnit(0)->bindManual(mSceneRenderTexture->getTarget(), mSceneRenderTexture->getTexName());
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 0, 0, mScreenWidth, mScreenHeight, 0);
}

void LLPostProcess::drawOrthoQuad(QuadType type)
{
	if(type == QUAD_NOISE)
	{
		//This could also be done through uniforms.
		LLStrider<LLVector2> uv2;
		mVBO->getTexCoord1Strider(uv2);

		float offs[2] = {(float) rand() / (float) RAND_MAX, (float) rand() / (float) RAND_MAX};
		float scale[2] = {mScreenWidth * mNoiseTextureScale / mScreenHeight, mNoiseTextureScale};
		uv2[0] = LLVector2(offs[0],offs[1]);
		uv2[1] = LLVector2(offs[0],offs[1]+scale[1]);
		uv2[2] = LLVector2(offs[0]+scale[0],offs[1]);
		uv2[3] = LLVector2(uv2[2].mV[0],uv2[1].mV[1]);
		mVBO->flush();
	}

	U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | (type == QUAD_NOISE ? LLVertexBuffer::MAP_TEXCOORD1 : 0);
	mVBO->setBuffer(mask);
	mVBO->drawArrays(LLRender::TRIANGLE_STRIP, 0, 4);
}

void LLPostProcess::createScreenTexture()
{
	std::vector<GLubyte> data(mScreenWidth * mScreenHeight * 3, 0) ;

	mSceneRenderTexture = new LLImageGL(FALSE) ;	
	if(mSceneRenderTexture->createGLTexture())
	{
		gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_RECT_TEXTURE, mSceneRenderTexture->getTexName());
		LLImageGL::setManualImage(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGB, mScreenWidth, mScreenHeight, GL_RGB, GL_UNSIGNED_BYTE, &data[0]);
		gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	}
}

void LLPostProcess::createNoiseTexture()
{	
	std::vector<GLubyte> buffer(NOISE_SIZE * NOISE_SIZE);
	for (unsigned int i = 0; i < NOISE_SIZE; i++){
		for (unsigned int k = 0; k < NOISE_SIZE; k++){
			buffer[(i * NOISE_SIZE) + k] = (GLubyte)((double) rand() / ((double) RAND_MAX + 1.f) * 255.f);
		}
	}

	mNoiseTexture = new LLImageGL(FALSE) ;
	if(mNoiseTexture->createGLTexture())
	{
		gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mNoiseTexture->getTexName());
		LLImageGL::setManualImage(GL_TEXTURE_2D, 0, GL_LUMINANCE, NOISE_SIZE, NOISE_SIZE, GL_LUMINANCE, GL_UNSIGNED_BYTE, &buffer[0]);
		gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	}
}

bool LLPostProcess::checkError(void)
{
	GLenum glErr;
    bool    retCode = false;

    glErr = glGetError();
    while (glErr != GL_NO_ERROR)
    {
		// shaderErrorLog << (const char *) gluErrorString(glErr) << std::endl;
		char const * err_str_raw = (const char *) gluErrorString(glErr);

		if(err_str_raw == NULL)
		{
			std::ostringstream err_builder;
			err_builder << "unknown error number " << glErr;
			mShaderErrorString = err_builder.str();
		}
		else
		{
			mShaderErrorString = err_str_raw;
		}

        retCode = true;
        glErr = glGetError();
    }
    return retCode;
}

