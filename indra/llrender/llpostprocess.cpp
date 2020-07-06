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

#include "lldir.h"
#include "llcontrol.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llmatrix4a.h"
#include "llperlin.h"
#include "llrender.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llvertexbuffer.h"

extern LLGLSLShader			gPostColorFilterProgram;
extern LLGLSLShader			gPostNightVisionProgram;
extern LLGLSLShader			gPostGaussianBlurProgram;
extern LLGLSLShader			gPostPosterizeProgram;
extern LLGLSLShader			gPostMotionBlurProgram;
extern LLGLSLShader			gPostVignetteProgram;

static LLStaticHashedString sGamma("gamma");
static LLStaticHashedString sBrightness("brightness");
static LLStaticHashedString sContrast("contrast");
static LLStaticHashedString sContrastBase("contrastBase");
static LLStaticHashedString sSaturation("saturation");
static LLStaticHashedString sBrightMult("brightMult");
static LLStaticHashedString sNoiseStrength("noiseStrength");
static LLStaticHashedString sLayerCount("layerCount");

static LLStaticHashedString sVignetteStrength("vignette_strength");
static LLStaticHashedString sVignettRadius("vignette_radius");
static LLStaticHashedString sVignetteDarkness("vignette_darkness");
static LLStaticHashedString sVignetteDesaturation("vignette_desaturation");
static LLStaticHashedString sVignetteChromaticAberration("vignette_chromatic_aberration");

static LLStaticHashedString sHorizontalPass("horizontalPass");
static LLStaticHashedString sKern("kern");

static LLStaticHashedString sPrevProj("prev_proj");
static LLStaticHashedString sInvProj("inv_proj");
static LLStaticHashedString sBlurStrength("blur_strength");

static const unsigned int NOISE_SIZE = 512;

static const char * const XML_FILENAME = "postprocesseffects.xml";

class LLPostProcessShader : public IPostProcessShader, public LLRefCount
{
public:
	LLPostProcessShader(const std::string& enable_name, LLGLSLShader& shader, bool enabled = false) :
		mShader(shader), mEnabled(enable_name,enabled)
	{
		addSetting(mEnabled);
	}
	/*virtual*/	bool isEnabled()					const	{return mShader.mProgramObject && mEnabled;}
	/*virtual*/ void bindShader()							{mShader.bind();}
	/*virtual*/ void unbindShader()							{mShader.unbind();}
	/*virtual*/ LLGLSLShader& getShader()					{return mShader;}

	/*virtual*/ LLSD getDefaults();							//See IPostProcessShader::getDefaults
	/*virtual*/ void loadSettings(const LLSD& settings);	//See IPostProcessShader::loadSettings
	/*virtual*/ void addSetting(IShaderSettingBase& setting) { mSettings.push_back(&setting); }
protected:
	template<typename T>
	struct LLShaderSetting : public IShaderSettingBase
	{
		LLShaderSetting(const std::string& name, T def) : 
			mValue(def), mDefault(def), mSettingName(name) {}
		operator T()								const	{ return mValue; }
		T get()										const	{ return mValue; }
		/*virtual*/ const std::string& getName()	const	{ return mSettingName; }	//See LLShaderSettingBase::getName
		/*virtual*/ LLSD getDefaultValue()			const	{ return mDefault; }		//See LLShaderSettingBase::getDefaultValue
		/*virtual*/ void setValue(const LLSD& value)		{ mValue = value; }			//See LLShaderSettingBase::setValue	
	private:
		const std::string mSettingName;		//LLSD key names as found in postprocesseffects.xml. eg 'contrast_base'
		T mValue;							//The member variable mentioned above.
		T mDefault;							//Set via ctor. Value is inserted into the defaults LLSD list if absent from postprocesseffects.xml
	};
private:
	std::vector<IShaderSettingBase*> mSettings;	//Contains a list of all the 'settings' this shader uses. Manually add via push_back in ctor.
	LLGLSLShader& mShader;
	LLShaderSetting<bool> mEnabled;
};

//helpers
class LLPostProcessSinglePassColorShader : public LLPostProcessShader
{
public:
	LLPostProcessSinglePassColorShader(const std::string& enable_name, LLGLSLShader& shader, bool enabled = false) :
		LLPostProcessShader(enable_name, shader, enabled) {}
	/*virtual*/ S32 getColorChannel()	const	{return 0;}
	/*virtual*/ S32 getDepthChannel()	const	{return -1;}
	/*virtual*/ bool draw(U32 pass)				{return pass == 1;}
	/*virtual*/ void postDraw()					{}
};

template<> LLSD LLPostProcessShader::LLShaderSetting<LLVector4>::getDefaultValue() const
{
	return mDefault.getValue();
}
template<> void LLPostProcessShader::LLShaderSetting<LLVector4>::setValue(const LLSD& value)
{
	mValue = ll_vector4_from_sd(value); 
}

LLSD LLPostProcessShader::getDefaults()
{
	LLSD defaults;
	for(std::vector<IShaderSettingBase*>::iterator it=mSettings.begin();it!=mSettings.end();++it)
	{
		defaults[(*it)->getName()]=(*it)->getDefaultValue();
	}
	return defaults;
}
void LLPostProcessShader::loadSettings(const LLSD& settings)
{
	for(std::vector<IShaderSettingBase*>::iterator it=mSettings.begin();it!=mSettings.end();++it)
	{
		LLSD value = settings[(*it)->getName()];
		(*it)->setValue(value);
	}
}

class LLColorFilterShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<F32> mGamma, mBrightness, mContrast, mSaturation;
	LLShaderSetting<LLVector4> mContrastBase;
public:
	LLColorFilterShader() : 
		LLPostProcessSinglePassColorShader("enable_color_filter",gPostColorFilterProgram),
		mGamma("gamma",1.f),
		mBrightness("brightness",1.f),
		mContrast("contrast",1.f),
		mSaturation("saturation",1.f),
		mContrastBase("contrast_base",LLVector4(1.f,1.f,1.f,0.5f))
	{
		addSetting(mGamma);
		addSetting(mBrightness);
		addSetting(mContrast);
		addSetting(mSaturation);
		addSetting(mContrastBase);
	}

	/*virtual*/ QuadType preDraw()
	{
		getShader().uniform1f(sGamma, mGamma);
		getShader().uniform1f(sBrightness, mBrightness);
		getShader().uniform1f(sContrast, mContrast);
		float baseI = (mContrastBase.get()[VX] + mContrastBase.get()[VY] + mContrastBase.get()[VZ]) / 3.0f;
		baseI = mContrastBase.get()[VW] / llmax(baseI,0.001f);
		float baseR = mContrastBase.get()[VX] * baseI;
		float baseG = mContrastBase.get()[VY] * baseI;
		float baseB = mContrastBase.get()[VZ] * baseI;
		getShader().uniform3fv(sContrastBase, 1, LLVector3(baseR, baseG, baseB).mV);
		getShader().uniform1f(sSaturation, mSaturation);

		return QUAD_NORMAL;
	}
};

class LLNightVisionShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<F32> mBrightnessMult, mNoiseStrength;
public:
	LLNightVisionShader() : 
		LLPostProcessSinglePassColorShader("enable_night_vision",gPostNightVisionProgram),
		mBrightnessMult("brightness_multiplier",3.f),
		mNoiseStrength("noise_strength",.4f)
	{
		addSetting(mBrightnessMult);
		addSetting(mNoiseStrength);
	}
	/*virtual*/ QuadType preDraw()
	{
		LLPostProcess::getInstance()->bindNoise(1);

		getShader().uniform1f(sBrightMult, mBrightnessMult);
		getShader().uniform1f(sNoiseStrength, mNoiseStrength);

		return QUAD_NOISE;
	}
};

class LLPosterizeShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<S32> mNumLayers;
public:
	LLPosterizeShader() : LLPostProcessSinglePassColorShader("enable_posterize",gPostPosterizeProgram),
		mNumLayers("posterize_layers",2)
	{
		addSetting(mNumLayers);
	}
	/*virtual*/ QuadType preDraw()
	{
		getShader().uniform1i(sLayerCount, mNumLayers);
		return QUAD_NORMAL;
	}
};

class LLVignetteShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<F32> mStrength, mRadius, mDarkness, mDesaturation, mChromaticAberration;
public:
	LLVignetteShader() : LLPostProcessSinglePassColorShader("enable_vignette",gPostVignetteProgram),
	mStrength("vignette_strength",.85f),
	mRadius("vignette_radius",.7f),
	mDarkness("vignette_darkness",1.f),
	mDesaturation("vignette_desaturation",1.5f),
	mChromaticAberration("vignette_chromatic_aberration",.05f)
	{
		addSetting(mStrength);
		addSetting(mRadius);
		addSetting(mDarkness);
		addSetting(mDesaturation);
		addSetting(mChromaticAberration);
	}
	/*virtual*/ QuadType preDraw()
	{
		LLVector2 screen_rect = LLPostProcess::getInstance()->getDimensions();

		getShader().uniform1f(sVignetteStrength, mStrength);
		getShader().uniform1f(sVignettRadius, mRadius);
		getShader().uniform1f(sVignetteDarkness, mDarkness);
		getShader().uniform1f(sVignetteDesaturation, mDesaturation);
		getShader().uniform1f(sVignetteChromaticAberration, mChromaticAberration);
		return QUAD_NORMAL;
	}
};

class LLGaussBlurShader : public LLPostProcessShader
{
private:
	LLShaderSetting<S32> mNumPasses;
	GLint mPassLoc;
public:
	LLGaussBlurShader() : LLPostProcessShader("enable_gauss_blur",gPostGaussianBlurProgram),
		mNumPasses("gauss_blur_passes",2),
		mPassLoc(0)
	{
		addSetting(mNumPasses);
	}
	/*virtual*/ bool isEnabled()		const	{ return LLPostProcessShader::isEnabled() && mNumPasses.get(); }
	/*virtual*/ S32 getColorChannel()	const	{ return 0; }
	/*virtual*/ S32 getDepthChannel()	const	{ return -1; }
	/*virtual*/ QuadType preDraw()
	{
		LLVector2 screen_rect = LLPostProcess::getInstance()->getDimensions();

		mPassLoc = getShader().getUniformLocation(sHorizontalPass);
		LLVector4 vec[] = { LLVector4(1.3846153846f, 3.2307692308f, 0.f, 0.f) / screen_rect.mV[VX], LLVector4( 0.f,0.f, 1.3846153846f, 3.2307692308f ) / screen_rect.mV[VY] };
		getShader().uniform4fv(sKern, LL_ARRAY_SIZE(vec), (GLfloat*)vec);
		return QUAD_NORMAL;
	}
	/*virtual*/ bool draw(U32 pass) 
	{
		if((S32)pass > mNumPasses*2)
			return false;
		gGL.syncShaders();
		glUniform1iARB(mPassLoc, (pass-1) % 2);
		return true;
	}
	/*virtual*/ void postDraw()					{}
};

class LLMotionShader : public LLPostProcessShader
{
private:
	LLShaderSetting<S32> mStrength;
public:
	LLMotionShader() : LLPostProcessShader("enable_motionblur",gPostMotionBlurProgram),
		mStrength("blur_strength",1)
	{
		addSetting(mStrength);
	}
	/*virtual*/ bool isEnabled()		const	{ return LLPostProcessShader::isEnabled() && llabs(gGLModelView.getF32ptr()[0] - gGLPreviousModelView.getF32ptr()[0]) > .0000001; }
	/*virtual*/ S32 getColorChannel()	const	{ return 0; }
	/*virtual*/ S32 getDepthChannel()	const	{ return 1; }
	/*virtual*/ QuadType preDraw()
	{
		LLMatrix4a inv_proj;
		inv_proj.setMul(gGLProjection,gGLModelView);
		inv_proj.invert();
		LLMatrix4a prev_proj;
		prev_proj.setMul(gGLProjection,gGLPreviousModelView);

		LLVector2 screen_rect = LLPostProcess::getInstance()->getDimensions();

		getShader().uniformMatrix4fv(sPrevProj, 1, GL_FALSE, prev_proj.getF32ptr());
		getShader().uniformMatrix4fv(sInvProj, 1, GL_FALSE, inv_proj.getF32ptr());
		getShader().uniform1i(sBlurStrength, mStrength);
		
		return QUAD_NORMAL;
	}
	/*virtual*/ bool draw(U32 pass) 
	{
		return pass == 1;
	}
	/*virtual*/ void postDraw()					{}
};

LLPostProcess::LLPostProcess(void) : 
					mVBO(NULL),
					mDepthTexture(0),
					mNoiseTexture(0),
					mScreenWidth(0),
					mScreenHeight(0),
					mNoiseTextureScale(0.f),
					mSelectedEffectInfo(LLSD::emptyMap()),
					mAllEffectInfo(LLSD::emptyMap())
{
	mShaders.push_back(new LLMotionShader());
	mShaders.push_back(new LLVignetteShader());
	mShaders.push_back(new LLColorFilterShader());
	mShaders.push_back(new LLNightVisionShader());
	mShaders.push_back(new LLGaussBlurShader());
	mShaders.push_back(new LLPosterizeShader());

	/*  Do nothing.  Needs to be updated to use our current shader system, and to work with the move into llrender.*/
	auto load_effects = [&](const std::string& pathName)
	{
		LL_DEBUGS("AppInit", "Shaders") << "Loading PostProcess Effects settings from " << pathName << LL_ENDL;
		llifstream effectsXML(pathName);

		if (effectsXML)
		{
			LLPointer<LLSDParser> parser = new LLSDXMLParser();
			parser->parse(effectsXML, mAllEffectInfo, LLSDSerialize::SIZE_UNLIMITED);
		}
	};
	load_effects(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", XML_FILENAME));
	load_effects(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "windlight", XML_FILENAME));

	auto setting = gSavedSettings.getControl("SinguPostProcessDefault");
	setting->getSignal()->connect(std::bind(&LLPostProcess::setSelectedEffect, this, std::bind(&LLSD::asStringRef, std::placeholders::_2)));
	const auto& str = setting->get().asStringRef();
	if (!mAllEffectInfo.has(str))
		mAllEffectInfo[str] = LLSD::emptyMap();

	LLSD& defaults = mAllEffectInfo[str];

	// Add defaults for all missing effects
	for(auto& shader : mShaders)
	{
		const LLSD shader_defaults = shader->getDefaults();
		for (LLSD::map_const_iterator it2 = defaults.beginMap();it2 != defaults.endMap();++it2)
		{
			if(!defaults.has(it2->first))
				defaults[it2->first]=it2->second;
		}
	}
	setSelectedEffect(str);
}

LLPostProcess::~LLPostProcess(void)
{
	gSavedSettings.setString("SinguPostProcessDefault", mSelectedEffectName);
	destroyGL() ;
}

void LLPostProcess::initialize(unsigned int width, unsigned int height)
{
	destroyGL();
	mScreenWidth = width;
	mScreenHeight = height;

	createScreenTextures();
	createNoiseTexture();

	//Setup our VBO.
	{
		mVBO = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1,GL_STREAM_DRAW_ARB);
		mVBO->allocateBuffer(4,0,TRUE);

		LLStrider<LLVector3> v;
		LLStrider<LLVector2> uv1;
		LLStrider<LLVector2> uv2;

		mVBO->getVertexStrider(v);
		mVBO->getTexCoord0Strider(uv1);
		mVBO->getTexCoord1Strider(uv2);

		uv2[0] = uv1[0] = LLVector2(0, 0);
		uv2[1] = uv1[1] = LLVector2(0, 1);
		uv2[2] = uv1[2] = LLVector2(1, 0);
		uv2[3] = uv1[3] = LLVector2(1, 1);

		LLVector3 vec1[4] = {
			 LLVector3(0, 0, 0),
			 LLVector3(0, mScreenHeight, 0),
			 LLVector3(mScreenWidth, 0, 0),
			 LLVector3(mScreenWidth, mScreenHeight, 0) };

		v[0] = vec1[0];
		v[1] = vec1[1];
		v[2] = vec1[2];
		v[3] = vec1[3];

		mVBO->flush();
	}
	stop_glerror();
}

void LLPostProcess::createScreenTextures()
{
	const LLTexUnit::eTextureType type = LLTexUnit::TT_TEXTURE;

	mRenderTarget[0].allocate(mScreenWidth,mScreenHeight,GL_RGBA,FALSE,FALSE,type,FALSE);
	if(mRenderTarget[0].getFBO())//Only need pingpong between two rendertargets if FBOs are supported.
		mRenderTarget[1].allocate(mScreenWidth,mScreenHeight,GL_RGBA,FALSE,FALSE,type,FALSE);
	stop_glerror();

	mDepthTexture.reset();

	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		if((*it)->getDepthChannel()>=0)
		{
			mDepthTexture = LLImageGL::createTextureName();
			gGL.getTexUnit(0)->bindManual(type, mDepthTexture->getTexName());
			LLImageGL::setManualImage(LLTexUnit::getInternalType(type), 0, GL_DEPTH_COMPONENT24, mScreenWidth, mScreenHeight, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
			stop_glerror();
			gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
			stop_glerror();
			break;
		}
	}
}

void LLPostProcess::createNoiseTexture()
{	
	std::vector<GLubyte> buffer(NOISE_SIZE * NOISE_SIZE);
	for (unsigned int i = 0; i < NOISE_SIZE; i++){
		for (unsigned int k = 0; k < NOISE_SIZE; k++){
			F32 pnoise = LLPerlinNoise::noise(LLVector2(i, k) * .5f) * (1.f/.7f);
			buffer[i * NOISE_SIZE + k] = (GLubyte)(llclamp(.5f + pnoise * .5f, 0.f, 1.f) * 255.f);
		}
	}

	mNoiseTexture = LLImageGL::createTextureName();
	stop_glerror();
	gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mNoiseTexture->getTexName());
	stop_glerror();

	LLImageGL::setManualImage(GL_TEXTURE_2D, 0, GL_LUMINANCE8, NOISE_SIZE, NOISE_SIZE, GL_LUMINANCE, GL_UNSIGNED_BYTE, &buffer[0]);

	stop_glerror();
	gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
	gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	stop_glerror();
}

void LLPostProcess::destroyGL()
{
	mRenderTarget[0].release();
	mRenderTarget[1].release();
	mDepthTexture.reset();
	mNoiseTexture.reset();
	mNoiseTexture=0 ;
	mVBO = NULL ;
}

/*static*/void LLPostProcess::cleanupClass()
{
	if (instanceExists())
		deleteSingleton();
}

void LLPostProcess::copyFrameBuffer()
{
	mRenderTarget[!!mRenderTarget[0].getFBO()].bindTexture(0,0);
	glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,mScreenWidth, mScreenHeight);
	stop_glerror();

	if(mDepthTexture)
	{
		for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
		{
			if((*it)->isEnabled() && (*it)->getDepthChannel()>=0)
			{
				gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mDepthTexture->getTexName());
				glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,mScreenWidth, mScreenHeight);
				stop_glerror();
				break;
			}
		}
	}

}

void LLPostProcess::bindNoise(U32 channel)
{
	gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE,mNoiseTexture->getTexName());
}

void LLPostProcess::renderEffects(unsigned int width, unsigned int height)
{
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		if((*it)->isEnabled())
		{
			if (mVBO.isNull() || width != mScreenWidth || height != mScreenHeight)
			{
				initialize(width, height);
			}
			doEffects();
			return;
		}
	}
}

void LLPostProcess::doEffects(void)
{
	LLVertexBuffer::unbind();

	mNoiseTextureScale = (1.f - (mSelectedEffectInfo["noise_size"].asFloat() - 1.f) *(9.f/990.f)) / (float)NOISE_SIZE;

	/// Copy the screen buffer to the render texture
	copyFrameBuffer();
	stop_glerror();

	//Disable depth. Set blendmode to replace.
	LLGLDepthTest depth(GL_FALSE,GL_FALSE);
	LLGLEnable<GL_BLEND> blend;
	gGL.setSceneBlendType(LLRender::BT_REPLACE);

	/// Change to an orthogonal view
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho( 0.f, (GLdouble) mScreenWidth, 0.f, (GLdouble) mScreenHeight, -1.f, 1.f );
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();
	
	applyShaders();

	LLGLSLShader::bindNoShader();

	/// Change to a perspective view
	gGL.flush();
	gGL.matrixMode( LLRender::MM_PROJECTION );
	gGL.popMatrix();
	gGL.matrixMode( LLRender::MM_MODELVIEW );
	gGL.popMatrix();

	gGL.setSceneBlendType(LLRender::BT_ALPHA);	//Restore blendstate. Alpha is ASSUMED for hud/ui render, etc.

	gGL.getTexUnit(1)->disable();
}

void LLPostProcess::applyShaders(void)
{
	bool primary_rendertarget = 1;
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		if((*it)->isEnabled())
		{
			S32 color_channel = (*it)->getColorChannel();
			S32 depth_channel = (*it)->getDepthChannel();
			if(depth_channel >= 0)
				gGL.getTexUnit(depth_channel)->bindManual(LLTexUnit::TT_TEXTURE, mDepthTexture->getTexName());
			U32 pass = 1;

			(*it)->bindShader();
			QuadType quad = (*it)->preDraw();
			while((*it)->draw(pass++))
			{
				LLRenderTarget& write_target = mRenderTarget[!primary_rendertarget];
				LLRenderTarget& read_target = mRenderTarget[mRenderTarget[0].getFBO() ? primary_rendertarget : !primary_rendertarget];
				write_target.bindTarget();

				if(color_channel >= 0)
					read_target.bindTexture(0,color_channel);

				drawOrthoQuad(quad);

				if(color_channel >= 0 && !mRenderTarget[0].getFBO())
					gGL.getTexUnit(color_channel)->unbind(read_target.getUsage());

				write_target.flush();
				if(mRenderTarget[0].getFBO())
					primary_rendertarget = !primary_rendertarget;
			}
			(*it)->postDraw();
			(*it)->unbindShader();
		}
	}
	//Only need to copy to framebuffer if FBOs are supported, else we've already been drawing to the framebuffer to begin with.
	if(mRenderTarget[0].getFBO())
	{
		//copyContentsToFramebuffer also binds the main framebuffer.
		LLRenderTarget::copyContentsToFramebuffer(mRenderTarget[primary_rendertarget],0,0,mScreenWidth,mScreenHeight,0,0,mScreenWidth,mScreenHeight,GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	stop_glerror();
}

void LLPostProcess::drawOrthoQuad(QuadType type)
{
	if(type == QUAD_NOISE)
	{
		//This could also be done through uniforms.
		LLStrider<LLVector2> uv2;
		mVBO->getTexCoord1Strider(uv2);

		float offs[2] = {
			/*ll_round*/((float) rand() / (float) RAND_MAX),
			/*ll_round*/((float) rand() / (float) RAND_MAX) };
		float scale[2] = {
			((float)mScreenWidth * mNoiseTextureScale),
			((float)mScreenHeight * mNoiseTextureScale) };

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

void LLPostProcess::setSelectedEffect(std::string const & effectName)
{
	mSelectedEffectName = effectName;
	mSelectedEffectInfo = mAllEffectInfo[effectName];
	for(auto shader : mShaders)
	{
		shader->loadSettings(mSelectedEffectInfo);
	}
	mSelectedEffectChanged(mSelectedEffectName);
}

void LLPostProcess::setSelectedEffectValue(std::string const & setting, LLSD value)
{
	char buf[256];
	S32 elem=0;
	if(sscanf(setting.c_str(),"%255[^[][%d]", buf, &elem) == 2)
	{
		mSelectedEffectInfo[static_cast<const char*>(buf)][elem] = value;
	}
	else
	{
		mSelectedEffectInfo[setting] = value;
	}
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		(*it)->loadSettings(mSelectedEffectInfo);
	}
}

void LLPostProcess::resetSelectedEffect()
{
	if(!llsd_equals(mAllEffectInfo[mSelectedEffectName], mSelectedEffectInfo))
	{
		mSelectedEffectInfo = mAllEffectInfo[mSelectedEffectName];
		for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
		{
			(*it)->loadSettings(mSelectedEffectInfo);
		}
	}
}

void LLPostProcess::saveEffectAs(std::string const & effectName)
{
	mAllEffectInfo[effectName] = mSelectedEffectInfo;
	mSelectedEffectChanged(mSelectedEffectName); // Might've changed, either way update the lists

	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "windlight", XML_FILENAME));
	//LL_INFOS() << "Saving PostProcess Effects settings to " << pathName << LL_ENDL;

	llofstream effectsXML(pathName);

	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();

	formatter->format(mAllEffectInfo, effectsXML);
}



