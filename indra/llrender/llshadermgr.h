/** 
 * @file llshadermgr.h
 * @brief Shader Manager
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

#ifndef LL_SHADERMGR_H
#define LL_SHADERMGR_H

#include "llgl.h"
#include "llglslshader.h"

class LLShaderMgr
{
public:
	LLShaderMgr();
	virtual ~LLShaderMgr();

	typedef enum
	{
		MODELVIEW_MATRIX = 0,
		PROJECTION_MATRIX,
		INVERSE_PROJECTION_MATRIX,
		MODELVIEW_PROJECTION_MATRIX,
		NORMAL_MATRIX,
		TEXTURE_MATRIX0,
		TEXTURE_MATRIX1,
		TEXTURE_MATRIX2,
		TEXTURE_MATRIX3,
		VIEWPORT,
		LIGHT_POSITION,
		LIGHT_DIRECTION,
		LIGHT_ATTENUATION,
		LIGHT_DIFFUSE,
		LIGHT_AMBIENT,
		MULTI_LIGHT_COUNT,
		MULTI_LIGHT,
		MULTI_LIGHT_COL,
		MULTI_LIGHT_FAR_Z,
		PROJECTOR_MATRIX,
		PROJECTOR_NEAR,
		PROJECTOR_P,
		PROJECTOR_N,
		PROJECTOR_ORIGIN,
		PROJECTOR_RANGE,
		PROJECTOR_AMBIANCE,
		PROJECTOR_SHADOW_INDEX,
		PROJECTOR_SHADOW_FADE,
		PROJECTOR_FOCUS,
		PROJECTOR_LOD,
		PROJECTOR_AMBIENT_LOD,
		DIFFUSE_COLOR,
		DIFFUSE_MAP,
		SPECULAR_MAP,
		BUMP_MAP,
		ENVIRONMENT_MAP,
		CLOUD_NOISE_MAP,
		FULLBRIGHT,
		LIGHTNORM,
		SUNLIGHT_COLOR,
		AMBIENT,
		BLUE_HORIZON,
		BLUE_DENSITY,
		HAZE_HORIZON,
		HAZE_DENSITY,
		CLOUD_SHADOW,
		DENSITY_MULTIPLIER,
		DISTANCE_MULTIPLIER,
		MAX_Y,
		GLOW,
		CLOUD_COLOR,
		CLOUD_POS_DENSITY1,
		CLOUD_POS_DENSITY2,
		CLOUD_SCALE,
		GAMMA,
		SCENE_LIGHT_STRENGTH,
		LIGHT_CENTER,
		LIGHT_SIZE,
		LIGHT_FALLOFF,

		GLOW_MIN_LUMINANCE,
		GLOW_MAX_EXTRACT_ALPHA,
		GLOW_LUM_WEIGHTS,
		GLOW_WARMTH_WEIGHTS,
		GLOW_WARMTH_AMOUNT,
		GLOW_STRENGTH,
		GLOW_DELTA,

		MINIMUM_ALPHA,

		DEFERRED_SHADOW_MATRIX,
		DEFERRED_ENV_MAT,
		DEFERRED_SHADOW_CLIP,
		DEFERRED_SUN_WASH,
		DEFERRED_SHADOW_NOISE,
		DEFERRED_BLUR_SIZE,
		DEFERRED_SSAO_RADIUS,
		DEFERRED_SSAO_MAX_RADIUS,
		DEFERRED_SSAO_FACTOR,
		DEFERRED_SSAO_FACTOR_INV,
		DEFERRED_SSAO_EFFECT_MAT,
		DEFERRED_SCREEN_RES,
		DEFERRED_NEAR_CLIP,
		DEFERRED_SHADOW_OFFSET,
		DEFERRED_SHADOW_BIAS,
		DEFERRED_SPOT_SHADOW_BIAS,
		DEFERRED_SPOT_SHADOW_OFFSET,
		DEFERRED_SUN_DIR,
		DEFERRED_SHADOW_RES,
		DEFERRED_PROJ_SHADOW_RES,
		DEFERRED_DEPTH_CUTOFF,
		DEFERRED_NORM_CUTOFF,

		FXAA_TC_SCALE,
		FXAA_RCP_SCREEN_RES,
		FXAA_RCP_FRAME_OPT,
		FXAA_RCP_FRAME_OPT2,

		DOF_FOCAL_DISTANCE,
		DOF_BLUR_CONSTANT,
		DOF_TAN_PIXEL_ANGLE,
		DOF_MAGNIFICATION,
		DOF_MAX_COF,
		DOF_RES_SCALE,

		DEFERRED_DEPTH,
		DEFERRED_SHADOW0,
		DEFERRED_SHADOW1,
		DEFERRED_SHADOW2,
		DEFERRED_SHADOW3,
		DEFERRED_SHADOW4,
		DEFERRED_SHADOW5,
		DEFERRED_NORMAL,
		DEFERRED_POSITION,
		DEFERRED_DIFFUSE,
		DEFERRED_SPECULAR,
		DEFERRED_NOISE,
		DEFERRED_LIGHTFUNC,
		DEFERRED_LIGHT,
		DEFERRED_BLOOM,
		DEFERRED_PROJECTION,
		END_RESERVED_UNIFORMS
	} eGLSLReservedUniforms;

	// singleton pattern implementation
	static LLShaderMgr * instance();

	virtual void initAttribsAndUniforms(void);

	BOOL attachShaderFeatures(LLGLSLShader * shader);
	void dumpObjectLog(GLhandleARB ret, BOOL warns = TRUE);
	BOOL	linkProgramObject(GLhandleARB obj, BOOL suppress_errors = FALSE);
	BOOL	validateProgramObject(GLhandleARB obj);
	GLhandleARB loadShaderFile(const std::string& filename, S32 & shader_level, GLenum type, S32 texture_index_channels = -1);

	// Implemented in the application to actually point to the shader directory.
	virtual std::string getShaderDirPrefix(void) = 0; // Pure Virtual

	// Implemented in the application to actually update out of date uniforms for a particular shader
	virtual void updateShaderUniforms(LLGLSLShader * shader) = 0; // Pure Virtual

public:
	// Map of shader names to compiled
	std::map<std::string, GLhandleARB> mShaderObjects;

	//global (reserved slot) shader parameters
	std::vector<std::string> mReservedAttribs;

	std::vector<std::string> mReservedUniforms;

	//preprocessor definitions (name/value)
	std::map<std::string, std::string> mDefinitions;

protected:

	// our parameter manager singleton instance
	static LLShaderMgr * sInstance;

public:
	static void unloadShaderClass(int shader_class);
	static std::vector<LLGLSLShader *> &getGlobalShaderList(); //Holds a list of ALL LLGLSLShader objects.
}; //LLShaderMgr

#endif
