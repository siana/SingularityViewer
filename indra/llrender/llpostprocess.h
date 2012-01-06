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
#include <fstream>
#include "llgl.h"
#include "llglheaders.h"

class LLPostProcess : public LLSingleton<LLPostProcess>
{
public:

	typedef enum _QuadType {
		QUAD_NORMAL,
		QUAD_NOISE
	} QuadType;

	/// GLSL Shader Encapsulation Struct
	//typedef std::map<const char *, GLuint> glslUniforms;

	struct PostProcessTweaks : public LLSD {
		inline PostProcessTweaks() : LLSD(LLSD::emptyMap())
		{
		}

		inline LLSD & brightMult() {
			return (*this)["brightness_multiplier"];
		}

		inline LLSD & noiseStrength() {
			return (*this)["noise_strength"];
		}

		inline LLSD & noiseSize() {
			return (*this)["noise_size"];
		}

		inline LLSD & brightness() {
			return (*this)["brightness"];
		}

		inline LLSD & contrast() {
			return (*this)["contrast"];
		}

		inline LLSD & contrastBaseR() {
			return (*this)["contrast_base"][0];
		}

		inline LLSD & contrastBaseG() {
			return (*this)["contrast_base"][1];
		}

		inline LLSD & contrastBaseB() {
			return (*this)["contrast_base"][2];
		}

		inline LLSD & contrastBaseIntensity() {
			return (*this)["contrast_base"][3];
		}

		inline LLSD & saturation() {
			return (*this)["saturation"];
		}

		inline LLSD & useNightVisionShader() {
			return (*this)["enable_night_vision"];
		}

		inline LLSD & useColorFilter() {
			return (*this)["enable_color_filter"];
		}
	
		inline LLSD & useGaussBlurFilter() {
			return (*this)["enable_gauss_blur"];
		}

		inline F32 getBrightMult() const {
			return F32((*this)["brightness_multiplier"].asReal());
		}

		inline F32 getNoiseStrength() const {
			return F32((*this)["noise_strength"].asReal());
		}

		inline F32 getNoiseSize() const {
			return F32((*this)["noise_size"].asReal());
		}

		inline F32 getGamma() const {
			return F32((*this)["gamma"].asReal());
		}

		inline F32 getBrightness() const {
			return F32((*this)["brightness"].asReal());
		}

		inline F32 getContrast() const {
			return F32((*this)["contrast"].asReal());
		}

		inline F32 getContrastBaseR() const {
			return F32((*this)["contrast_base"][0].asReal());
		}

		inline F32 getContrastBaseG() const {
			return F32((*this)["contrast_base"][1].asReal());
		}

		inline F32 getContrastBaseB() const {
			return F32((*this)["contrast_base"][2].asReal());
		}

		inline F32 getContrastBaseIntensity() const {
			return F32((*this)["contrast_base"][3].asReal());
		}

		inline F32 getSaturation() const {
			return F32((*this)["saturation"].asReal());
		}

		inline LLSD & getGaussBlurPasses() {
			return (*this)["gauss_blur_passes"];
		}
	};

	PostProcessTweaks tweaks;

	// the map of all availible effects
	LLSD mAllEffects;

private:
	LLPointer<LLVertexBuffer> mVBO;
	LLPointer<LLImageGL> mSceneRenderTexture ;
	LLPointer<LLImageGL> mNoiseTexture ;

public:
	LLPostProcess(void);

	~LLPostProcess(void);

	void apply(unsigned int width, unsigned int height);
	void invalidate() ;

	// Cleanup of global data that's only inited once per class.
	static void cleanupClass();

	void setSelectedEffect(std::string const & effectName);

	inline std::string const & getSelectedEffect(void) const {
		return mSelectedEffectName;
	}

	void saveEffect(std::string const & effectName);

private:
		/// read in from file
	std::string mShaderErrorString;
	unsigned int mScreenWidth;
	unsigned int mScreenHeight;

	float mNoiseTextureScale;
	
	// the name of currently selected effect in mAllEffects
	// invariant: tweaks == mAllEffects[mSelectedEffectName]
	std::string mSelectedEffectName;

	/// General functions
	void initialize(unsigned int width, unsigned int height);
	void doEffects(void);
	void applyShaders(void);
	bool shadersEnabled(void);

	/// Night Vision Functions
	void applyNightVisionShader(void);

	/// Color Filter Functions
	void applyColorFilterShader(void);

	/// Gaussian blur Filter Functions
	void applyGaussBlurShader(void);

	/// OpenGL Helper Functions
	void copyFrameBuffer();
	void createScreenTexture();
	void createNoiseTexture();
	bool checkError(void);
	void drawOrthoQuad(QuadType type);
};
#endif // LL_POSTPROCESS_H
