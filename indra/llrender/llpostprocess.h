/** 
 * @file llpostprocess.h
 * @brief LLPostProcess class definition
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

#ifndef LL_POSTPROCESS_H
#define LL_POSTPROCESS_H

#include <map>
#include <boost/signals2.hpp>
#include "llsd.h"
#include "llrendertarget.h"

class LLSD;
class LLGLSLShader;

typedef enum _QuadType {
	QUAD_NORMAL,
	QUAD_NOISE
} QuadType;

//LLPostProcessShader is an attempt to encapsulate the shaders a little better.
class IPostProcessShader //Abstract. PostProcess shaders derive off of this common base.
{
protected:
	//LLShaderSetting is used to associate key names to member variables to avoid LLSD lookups when drawing.
	//It also facilitates automating the assigning of defaults to, as well as parsing from, the effects LLSD list.
	//This replaces the entire old PostProcessTweaks structure. More will be done in the future to move into a more
	//xml-driven configuration.
	struct IShaderSettingBase
	{
		virtual ~IShaderSettingBase() {}					//virtual dtor.
		virtual const std::string& getName() const = 0;		//Returns the name of this setting
		virtual LLSD getDefaultValue() const = 0;			//Converts the member variable as an LLSD. Used to set defaults absent in postprocesseffects.xml
		virtual void setValue(const LLSD& value) = 0;		//Connects the LLSD element to the member variable. Used when loading effects (such as default)
	};
public:
	virtual ~IPostProcessShader() {}			//virtual dtor.
	virtual bool isEnabled()		const = 0;	//Returning false avoids bind/draw/unbind calls. If no shaders are enabled, framebuffer copying is skipped.
	virtual S32 getColorChannel()	const = 0;	//If color buffer is used in this shader returns > -1 to cue LLPostProcess on copying it from the framebuffer. 
	virtual S32 getDepthChannel()	const = 0;	//If depth buffer is used in this shader returns > -1 to cue LLPostProcess on copying it from the framebuffer.
	virtual void bindShader() = 0;
	virtual void unbindShader() = 0;
	virtual LLGLSLShader& getShader() = 0;

	virtual QuadType preDraw() = 0;		//Bind shader and textures, set up attribs. Returns the 'type' of quad to be drawn.
	virtual bool draw(U32 pass) = 0;	//returning false means finished. Used to update per-pass attributes and such. LLPostProcess will call
										//drawOrthoQuad when this returns true, increment pass, then call this again, and keep repeating this until false is returned.
	virtual void postDraw() = 0;		//Done drawing..

	virtual LLSD getDefaults() = 0;							//Returns a full LLSD kvp list filled with default values.
	virtual void loadSettings(const LLSD& settings) = 0;	//Parses the effects LLSD list and sets the member variables linked to them (via LLShaderSetting::setValue())
	virtual void addSetting(IShaderSettingBase& setting) = 0;
};

class LLPostProcessShader;
class LLPostProcess : public LLSingleton<LLPostProcess>
{
private:
	std::list<LLPointer<LLPostProcessShader> > mShaders;	//List of all registered LLPostProcessShader instances.

	LLPointer<LLVertexBuffer> mVBO;
	U32 mNextDrawTarget;	//Need to pingpong between two rendertargets. Cannot sample target texture of currently bound FBO.
							// However this is ONLY the case if fbos are actually supported, else swapping isn't needed.
	LLRenderTarget mRenderTarget[2];
	LLImageGL::GLTextureName mDepthTexture;
	LLImageGL::GLTextureName mNoiseTexture ;
	
	U32 mScreenWidth;
	U32 mScreenHeight;
	F32 mNoiseTextureScale;

	//  The name of currently selected effect in mAllEffectInfo
	std::string mSelectedEffectName;
	//  The map of settings for currently selected effect.
	LLSD mSelectedEffectInfo;
	//  The map of all availible effects
	LLSD mAllEffectInfo;

	typedef boost::signals2::signal<void(const std::string&)> selected_effect_changed_signal;
	selected_effect_changed_signal mSelectedEffectChanged;

public:
	LLPostProcess(void);
	~LLPostProcess(void);
private:
	// OpenGL initialization
	void initialize(unsigned int width, unsigned int height);	//Sets mScreenWidth and mScreenHeight
																// calls createScreenTextures and createNoiseTexture
																// creates VBO
	void createScreenTextures();								//Creates color texture and depth texture(if needed).
	void createNoiseTexture();									//Creates 'random' noise texture.

public:
	// Teardown
	//  Called on destroyGL or cleanupClass. Releases VBOs, rendertargets and textures.
	void destroyGL();
	//  Cleanup of global data that's only inited once per class.
	static void cleanupClass();

	// Setup for draw.
	void copyFrameBuffer();
	void bindNoise(U32 channel);
	
	// Draw
	void renderEffects(unsigned int width, unsigned int height);	//Entry point for newview.
private:
	void doEffects(void);				//Sets up viewmatrix, blits the framebuffer, then calls applyShaders.
	void applyShaders(void);			//Iterates over all active post shaders, manages binding, calls drawOrthoQuad for render.
	void drawOrthoQuad(QuadType type);	//Finally draws fullscreen quad with the shader currently bound.

public:
	LLVector2 getDimensions() { return LLVector2(mScreenWidth,mScreenHeight); }


	// UI interaction
	//  Getters
	inline LLSD const & getAllEffectInfo(void) const				{ return mAllEffectInfo; }
	inline std::string const & getSelectedEffectName(void) const	{ return mSelectedEffectName; }
	inline LLSD const & getSelectedEffectInfo(void) const			{ return mSelectedEffectInfo; }
	//  Setters
	void setSelectedEffect(std::string const & effectName);
	void setSelectedEffectValue(std::string const & setting, LLSD value);
	auto setSelectedEffectChangeCallback(const selected_effect_changed_signal::slot_type& func) { return mSelectedEffectChanged.connect(func); }
	void resetSelectedEffect();
	void saveEffectAs(std::string const & effectName);
};
#endif // LL_POSTPROCESS_H
