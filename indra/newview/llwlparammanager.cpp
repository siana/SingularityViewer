/**
 * @file llwlparammanager.cpp
 * @brief Implementation for the LLWLParamManager class.
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

#include "llviewerprecompiledheaders.h"

#include "llwlparammanager.h"

#include "pipeline.h"
#include "llsky.h"

#include "lldiriterator.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"
#include "llviewercamera.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llsdserialize.h"

#include "v4math.h"
#include "llviewerdisplay.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "lldrawpoolwater.h"
#include "llagent.h"
#include "llviewerregion.h"

#include "lldaycyclemanager.h"
#include "llenvmanager.h"
#include "llwlparamset.h"
#include "llpostprocess.h"
#include "llfloaterwindlight.h"
#include "llfloaterdaycycle.h"
#include "llfloaterenvsettings.h"

#include "llviewershadermgr.h"
#include "llglslshader.h"

// For notecard loading
#include "llvfile.h"
#include "llnotecard.h"
#include "llmemorystream.h"
#include "llnotify.h"
#include "llagent.h"
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llviewerregion.h"
#include "llassetuploadresponders.h"

#include "llstreamtools.h"

// [RLVa:KB] - Checked: 2011-09-04 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
#include <boost/algorithm/string.hpp>
// [/RLVa:KB]

LLWLParamManager::LLWLParamManager() :

	//set the defaults for the controls

	/// Sun Delta Terrain tweak variables.
	mSunDeltaYaw(180.0f),
	mSceneLightStrength(2.0f),
	mWLGamma(1.0f, "gamma"),

	mBlueHorizon(0.25f, 0.25f, 1.0f, 1.0f, "blue_horizon", "WLBlueHorizon"),
	mHazeDensity(1.0f, "haze_density"),
	mBlueDensity(0.25f, 0.25f, 0.25f, 1.0f, "blue_density", "WLBlueDensity"),
	mDensityMult(1.0f, "density_multiplier", 1000),
	mHazeHorizon(1.0f, "haze_horizon"),
	mMaxAlt(4000.0f, "max_y"),

	// Lighting
	mLightnorm(0.f, 0.707f, -0.707f, 1.f, "lightnorm"),
	mSunlight(0.5f, 0.5f, 0.5f, 1.0f, "sunlight_color", "WLSunlight"),
	mAmbient(0.5f, 0.75f, 1.0f, 1.19f, "ambient", "WLAmbient"),
	mGlow(18.0f, 0.0f, -0.01f, 1.0f, "glow"),

	// Clouds
	mCloudColor(0.5f, 0.5f, 0.5f, 1.0f, "cloud_color", "WLCloudColor"),
	mCloudMain(0.5f, 0.5f, 0.125f, 1.0f, "cloud_pos_density1"),
	mCloudCoverage(0.0f, "cloud_shadow"),
	mCloudDetail(0.0f, 0.0f, 0.0f, 1.0f, "cloud_pos_density2"),
	mDistanceMult(1.0f, "distance_multiplier"),
	mCloudScale(0.42f, "cloud_scale"),

	// sky dome
	mDomeOffset(0.96f),
	mDomeRadius(15000.f)
{
}

LLWLParamManager::~LLWLParamManager()
{
}

void LLWLParamManager::clearParamSetsOfScope(LLWLParamKey::EScope scope)
{
	if (LLWLParamKey::SCOPE_LOCAL == scope)
	{
		LL_WARNS("Windlight") << "Tried to clear windlight sky presets from local system!  This shouldn't be called..." << LL_ENDL;
		return;
	}

	std::set<LLWLParamKey> to_remove;
	for(std::map<LLWLParamKey, LLWLParamSet>::iterator iter = mParamList.begin(); iter != mParamList.end(); ++iter)
	{
		if(iter->first.scope == scope)
		{
			to_remove.insert(iter->first);
		}
	}

	for(std::set<LLWLParamKey>::iterator iter = to_remove.begin(); iter != to_remove.end(); ++iter)
	{
		mParamList.erase(*iter);
	}
}

// returns all skies referenced by the day cycle, with their final names
// side effect: applies changes to all internal structures!
std::map<LLWLParamKey, LLWLParamSet> LLWLParamManager::finalizeFromDayCycle(LLWLParamKey::EScope scope)
{
	LL_DEBUGS() << "mDay before finalizing:" << LL_ENDL;
	{
		for (std::map<F32, LLWLParamKey>::iterator iter = mDay.mTimeMap.begin(); iter != mDay.mTimeMap.end(); ++iter)
		{
			LLWLParamKey& key = iter->second;
			LL_DEBUGS() << iter->first << "->" << key.name << LL_ENDL;
		}
	}

	std::map<LLWLParamKey, LLWLParamSet> final_references;

	// Move all referenced to desired scope, renaming if necessary
	// First, save skies referenced
	std::map<LLWLParamKey, LLWLParamSet> current_references; // all skies referenced by the day cycle, with their current names
	// guard against skies with same name and different scopes
	std::set<std::string> inserted_names;
	std::map<std::string, unsigned int> conflicted_names; // integer later used as a count, for uniquely renaming conflicts

	LLWLDayCycle& cycle = mDay;
	for(std::map<F32, LLWLParamKey>::iterator iter = cycle.mTimeMap.begin();
		iter != cycle.mTimeMap.end();
		++iter)
	{
		LLWLParamKey& key = iter->second;
		std::string desired_name = key.name;
		replace_newlines_with_whitespace(desired_name); // already shouldn't have newlines, but just in case
		if(inserted_names.find(desired_name) == inserted_names.end())
		{
			inserted_names.insert(desired_name);
		}
		else
		{
			// make exist in map
			conflicted_names[desired_name] = 0;
		}
		current_references[key] = mParamList[key];
	}

	// forget all old skies in target scope, and rebuild, renaming as needed
	clearParamSetsOfScope(scope);
	for(std::map<LLWLParamKey, LLWLParamSet>::iterator iter = current_references.begin(); iter != current_references.end(); ++iter)
	{
		const LLWLParamKey& old_key = iter->first;

		std::string desired_name(old_key.name);
		replace_newlines_with_whitespace(desired_name);

		LLWLParamKey new_key(desired_name, scope); // name will be replaced later if necessary

		// if this sky is one with a non-unique name, rename via appending a number
		// an existing preset of the target scope gets to keep its name
		if (scope != old_key.scope && conflicted_names.find(desired_name) != conflicted_names.end())
		{
			std::string& new_name = new_key.name;

			do
			{
				// if this executes more than once, this is an absurdly pathological case
				// (e.g. "x" repeated twice, but "x 1" already exists, so need to use "x 2")
				std::stringstream temp;
				temp << desired_name << " " << (++conflicted_names[desired_name]);
				new_name = temp.str();
			} while (inserted_names.find(new_name) != inserted_names.end());

			// yay, found one that works
			inserted_names.insert(new_name); // track names we consume here; shouldn't be necessary due to ++int? but just in case

			// *TODO factor out below into a rename()?

			LL_INFOS("Windlight") << "Renamed " << old_key.name << " (scope" << old_key.scope << ") to "
				<< new_key.name << " (scope " << new_key.scope << ")" << LL_ENDL;

			// update name in sky
			iter->second.mName = new_name;

			// update keys in day cycle
			for(std::map<F32, LLWLParamKey>::iterator frame = cycle.mTimeMap.begin(); frame != cycle.mTimeMap.end(); ++frame)
			{
				if (frame->second == old_key)
				{
					frame->second = new_key;
				}
			}

			// add to master sky map
			mParamList[new_key] = iter->second;
		}

		final_references[new_key] = iter->second;
	}

	LL_DEBUGS() << "mDay after finalizing:" << LL_ENDL;
	{
		for (std::map<F32, LLWLParamKey>::iterator iter = mDay.mTimeMap.begin(); iter != mDay.mTimeMap.end(); ++iter)
		{
			LLWLParamKey& key = iter->second;
			LL_DEBUGS() << iter->first << "->" << key.name << LL_ENDL;
		}
	}

	return final_references;
}

// static
LLSD LLWLParamManager::createSkyMap(std::map<LLWLParamKey, LLWLParamSet> refs)
{
	LLSD skies = LLSD::emptyMap();
	for(std::map<LLWLParamKey, LLWLParamSet>::iterator iter = refs.begin(); iter != refs.end(); ++iter)
	{
		skies.insert(iter->first.name, iter->second.getAll());
	}
	return skies;
}

void LLWLParamManager::addAllSkies(const LLWLParamKey::EScope scope, const LLSD& sky_presets)
{
	for(LLSD::map_const_iterator iter = sky_presets.beginMap(); iter != sky_presets.endMap(); ++iter)
	{
		setParamSet(LLWLParamKey(iter->first, scope), iter->second);
	}
}

void LLWLParamManager::refreshRegionPresets()
{
	// Remove all region sky presets because they may belong to a previously visited region.
	clearParamSetsOfScope(LLEnvKey::SCOPE_REGION);

	// Add all sky presets belonging to the current region.
	addAllSkies(LLEnvKey::SCOPE_REGION, LLEnvManagerNew::instance().getRegionSettings().getSkyMap());
}

void LLWLParamManager::loadAllPresets()
{
	// First, load system (coming out of the box) sky presets.
	loadPresetsFromDir(getSysDir());

	// Then load user presets. Note that user day presets will modify any system ones already loaded.
	loadPresetsFromDir(getUserDir());
}

void LLWLParamManager::loadPresetsFromDir(const std::string& dir)
{
	LL_INFOS("AppInit", "Shaders") << "Loading sky presets from " << dir << LL_ENDL;

	LLDirIterator dir_iter(dir, "*.xml");
	while (1)
	{
		std::string file;
		if (!dir_iter.next(file))
		{
			break; // no more files
		}

		std::string path = dir + file;
		if (!loadPreset(path))
		{
			LL_WARNS() << "Error loading sky preset from " << path << LL_ENDL;
		}
	}
}

bool LLWLParamManager::loadPreset(const std::string& path)
{
	llifstream xml_file;
	std::string name(LLURI::unescape(gDirUtilp->getBaseFileName(path, true)));

	xml_file.open(path.c_str());
	if (!xml_file)
	{
		return false;
	}

	LL_DEBUGS("AppInit", "Shaders") << "Loading sky " << name << LL_ENDL;

	LLSD params_data;
	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	parser->parse(xml_file, params_data, LLSDSerialize::SIZE_UNLIMITED);
	xml_file.close();

	LLWLParamKey key(name, LLEnvKey::SCOPE_LOCAL);
	if (hasParamSet(key))
	{
		setParamSet(key, params_data);
	}
	else
	{
		addParamSet(key, params_data);
	}

	return true;
}

void LLWLParamManager::savePreset(LLWLParamKey key)
{
	llassert(key.scope == LLEnvKey::SCOPE_LOCAL && !key.name.empty());

	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());
	std::string pathName(getUserDir() + LLWeb::curlEscape(key.name) + ".xml");

	// fill it with LLSD windlight params
	paramsData = mParamList[key].getAll();

	// write to file
	llofstream presetsXML(pathName);
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
	presetsXML.close();

	propagateParameters();
}

void LLWLParamManager::updateShaderUniforms(LLGLSLShader * shader)
{
	if (gPipeline.canUseWindLightShaders())
	{
		mCurParams.update(shader);
	}

	if (shader->mShaderGroup == LLGLSLShader::SG_DEFAULT)
	{
		shader->uniform4fv(LLShaderMgr::LIGHTNORM, 1, mRotatedLightDir.mV);
		shader->uniform3fv(LLShaderMgr::WL_CAMPOSLOCAL, 1, LLViewerCamera::getInstance()->getOrigin().mV);
	} 

	else if (shader->mShaderGroup == LLGLSLShader::SG_SKY)
	{
		shader->uniform4fv(LLViewerShaderMgr::LIGHTNORM, 1, mClampedLightDir.mV);
	}

	shader->uniform1f(LLShaderMgr::SCENE_LIGHT_STRENGTH, mSceneLightStrength);
	
}

void LLWLParamManager::updateShaderLinks()
{
	mShaderList.clear();
	LLViewerShaderMgr::shader_iter shaders_iter, end_shaders;
	end_shaders = LLViewerShaderMgr::instance()->endShaders();
	for(shaders_iter = LLViewerShaderMgr::instance()->beginShaders(); shaders_iter != end_shaders; ++shaders_iter)
	{
		if (shaders_iter->mProgramObject != 0
			&& (gPipeline.canUseWindLightShaders() || shaders_iter->mShaderGroup == LLGLSLShader::SG_WATER))
		{
			if(	glGetUniformLocationARB(shaders_iter->mProgramObject,"lightnorm")>=0			||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"camPosLocal")>=0			||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"scene_light_strength")>=0	||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"cloud_pos_density1")>=0)
			mShaderList.push_back(&(*shaders_iter));
		}
	}
}

static LLTrace::BlockTimerStatHandle FTM_UPDATE_WLPARAM("Update Windlight Params");

void LLWLParamManager::propagateParameters(void)
{
	LL_RECORD_BLOCK_TIME(FTM_UPDATE_WLPARAM);
	
	LLVector4 sunDir;
	LLVector4 moonDir;

	// set the sun direction from SunAngle and EastAngle
	F32 sinTheta = sin(mCurParams.getEastAngle());
	F32 cosTheta = cos(mCurParams.getEastAngle());

	F32 sinPhi = sin(mCurParams.getSunAngle());
	F32 cosPhi = cos(mCurParams.getSunAngle());

	sunDir.mV[0] = -sinTheta * cosPhi;
	sunDir.mV[1] = sinPhi;
	sunDir.mV[2] = cosTheta * cosPhi;
	sunDir.mV[3] = 0;

	moonDir = -sunDir;

	// is the normal from the sun or the moon
	if(sunDir.mV[1] >= 0)
	{
		mLightDir = sunDir;
	}
	else if(sunDir.mV[1] < 0 && sunDir.mV[1] > LLSky::NIGHTTIME_ELEVATION_COS)
	{
		// clamp v1 to 0 so sun never points up and causes weirdness on some machines
		LLVector3 vec(sunDir.mV[0], sunDir.mV[1], sunDir.mV[2]);
		vec.mV[1] = 0;
		vec.normVec();
		mLightDir = LLVector4(vec, 0.f);
	}
	else
	{
		mLightDir = moonDir;
	}

	// calculate the clamp lightnorm for sky (to prevent ugly banding in sky
	// when haze goes below the horizon
	mClampedLightDir = sunDir;

	if (mClampedLightDir.mV[1] < -0.1f)
	{
		mClampedLightDir.mV[1] = -0.1f;
	}

	mCurParams.set("lightnorm", mLightDir);

	// bind the variables for all shaders only if we're using WindLight
	std::vector<LLGLSLShader*>::iterator shaders_iter=mShaderList.begin();
	for(; shaders_iter != mShaderList.end(); ++shaders_iter)
	{
		(*shaders_iter)->mUniformsDirty = TRUE;
	}

	// get the cfr version of the sun's direction
	LLVector3 cfrSunDir(sunDir.mV[2], sunDir.mV[0], sunDir.mV[1]);

	// set direction and don't allow overriding
	gSky.setSunDirection(cfrSunDir, LLVector3(0,0,0));
	gSky.setOverrideSun(TRUE);
}

void LLWLParamManager::update(LLViewerCamera * cam)
{
	LL_RECORD_BLOCK_TIME(FTM_UPDATE_WLPARAM);

	// update clouds, sun, and general
	mCurParams.updateCloudScrolling();
	
	// update only if running
	if(mAnimator.getIsRunning()) 
	{
		mAnimator.update(mCurParams);
	}

	// update the shaders and the menu
	propagateParameters();
	
	// sync menus if they exist
	if(LLFloaterWindLight::isOpen()) 
	{
		LLFloaterWindLight::instance()->syncMenu();
	}
	if(LLFloaterDayCycle::isOpen()) 
	{
		LLFloaterDayCycle::instance()->syncMenu();
	}
	if(LLFloaterEnvSettings::isOpen()) 
	{
		LLFloaterEnvSettings::instance()->syncMenu();
	}

	F32 camYaw = cam->getYaw();

	stop_glerror();

	// *TODO: potential optimization - this block may only need to be
	// executed some of the time.  For example for water shaders only.
	{
		F32 camYawDelta = mSunDeltaYaw * DEG_TO_RAD;
		
		LLVector3 lightNorm3(mLightDir);
		lightNorm3 *= LLQuaternion(-(camYaw + camYawDelta), LLVector3(0.f, 1.f, 0.f));
		mRotatedLightDir = LLVector4(lightNorm3, 0.f);

		std::vector<LLGLSLShader*>::iterator shaders_iter=mShaderList.begin();
		for(; shaders_iter != mShaderList.end(); ++shaders_iter)
		{
			(*shaders_iter)->mUniformsDirty = TRUE;
		}
	}
}

bool LLWLParamManager::applyDayCycleParams(const LLSD& params, LLEnvKey::EScope scope, F32 time)
{
	mDay.loadDayCycle(params, scope);
	resetAnimator(time, true); // set to specified time and start animator
	return true;
}

bool LLWLParamManager::applySkyParams(const LLSD& params, bool interpolate /*= false*/)
{
	if (params.size() == 0)
	{
		LL_WARNS() << "Undefined sky params" << LL_ENDL;
		return false;
	}

	if (interpolate)
	{
		if (!mAnimator.getIsRunning())
			resetAnimator(0.f, true); 
		
		if (!params.has("mName") || mCurParams.mName != params["mName"])
			LLWLParamManager::getInstance()->mAnimator.startInterpolationSky(params);
	}
	else
	{
		mAnimator.deactivate();
		mCurParams.setAll(params);
	}

	return true;
}

void LLWLParamManager::resetAnimator(F32 curTime, bool run)
{
	mAnimator.setTrack(mDay.mTimeMap, mDay.mDayRate, 
		curTime, run);

	return;
}

bool LLWLParamManager::addParamSet(const LLWLParamKey& key, LLWLParamSet& param)
{
	// add a new one if not one there already
	std::map<LLWLParamKey, LLWLParamSet>::iterator mIt = mParamList.find(key);
	if(mIt == mParamList.end()) 
	{	
		llassert(!key.name.empty());
		// *TODO: validate params
		mParamList[key] = param;
		mPresetListChangeSignal();
		return true;
	}

	return false;
}

BOOL LLWLParamManager::addParamSet(const LLWLParamKey& key, LLSD const & param)
{
	LLWLParamSet param_set;
	param_set.setAll(param);
	return addParamSet(key, param_set);
}

bool LLWLParamManager::getParamSet(const LLWLParamKey& key, LLWLParamSet& param)
{
	// find it and set it
	std::map<LLWLParamKey, LLWLParamSet>::iterator mIt = mParamList.find(key);
	if(mIt != mParamList.end()) 
	{
		param = mParamList[key];
		param.mName = key.name;
		return true;
	}

	return false;
}

bool LLWLParamManager::hasParamSet(const LLWLParamKey& key)
{
	LLWLParamSet dummy;
	return getParamSet(key, dummy);
}

bool LLWLParamManager::setParamSet(const std::string& name, LLWLParamSet& param, LLEnvKey::EScope scope)
{
	const LLWLParamKey key(name, scope);
	return setParamSet(key, param);
}

bool LLWLParamManager::setParamSet(const LLWLParamKey& key, LLWLParamSet& param)
{
	llassert(!key.name.empty());
	// *TODO: validate params
	mParamList[key] = param;

	return true;
}

bool LLWLParamManager::setParamSet(const std::string& name, const LLSD & param, LLEnvKey::EScope scope)
{
	const LLWLParamKey key(name, scope);
	return setParamSet(key, param);
}

bool LLWLParamManager::setParamSet(const LLWLParamKey& key, const LLSD & param)
{
	llassert(!key.name.empty());
	// *TODO: validate params

	// quick, non robust (we won't be working with files, but assets) check
	// this might not actually be true anymore....
	if(!param.isMap()) 
	{
		return false;
	}
	
	LLWLParamSet param_set;
	param_set.setAll(param);
	return setParamSet(key, param_set);
}

bool LLWLParamManager::removeParamSet(const std::string& name, bool delete_from_disk)
{
	const LLWLParamKey key(name, LLEnvKey::SCOPE_LOCAL);
	return removeParamSet(key, delete_from_disk);
}

bool LLWLParamManager::removeParamSet(const LLWLParamKey& key, bool delete_from_disk)
{
	// *NOTE: Removing a sky preset invalidates day cycles that refer to it.

	if (key.scope == LLEnvKey::SCOPE_REGION)
	{
		LL_WARNS() << "Removing region skies not supported" << LL_ENDL;
		llassert(key.scope == LLEnvKey::SCOPE_LOCAL);
		return false;
	}

	// remove from param list
	std::map<LLWLParamKey, LLWLParamSet>::iterator it = mParamList.find(key);
	if (it == mParamList.end())
	{
		LL_WARNS("WindLight") << "No sky preset named " << key.name << LL_ENDL;
		return false;
	}

	mParamList.erase(it);
	mDay.removeReferencesTo(key);

	// remove from file system if requested
	if (delete_from_disk)
	{
		std::string path_name(getUserDir());
		std::string escaped_name = LLWeb::curlEscape(key.name);

		if(gDirUtilp->deleteFilesInDir(path_name, escaped_name + ".xml") < 1)
		{
			LL_WARNS("WindLight") << "Error removing sky preset " << key.name << " from disk" << LL_ENDL;
		}
	}

	// signal interested parties
	mPresetListChangeSignal();

	return true;
}

bool LLWLParamManager::isSystemPreset(const std::string& preset_name) const
{
	// *TODO: file system access is excessive here.
	return gDirUtilp->fileExists(getSysDir() + LLWeb::curlEscape(preset_name) + ".xml");
}

void LLWLParamManager::getPresetNames(preset_name_list_t& region, preset_name_list_t& user, preset_name_list_t& sys) const
{
	region.clear();
	user.clear();
	sys.clear();

	for (std::map<LLWLParamKey, LLWLParamSet>::const_iterator it = mParamList.begin(); it != mParamList.end(); it++)
	{
		const LLWLParamKey& key = it->first;
		const std::string& name = key.name;

		if (key.scope == LLEnvKey::SCOPE_REGION)
		{
			region.push_back(name);
		}
		else
		{
			if (isSystemPreset(name))
			{
				sys.push_back(name);
			}
			else
			{
				user.push_back(name);
			}
		}
	}
}

// [RLVa:KB] - Checked: 2011-09-04 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
const std::string& LLWLParamManager::findPreset(const std::string& strPresetName, LLEnvKey::EScope eScope)
{
	for (std::map<LLWLParamKey, LLWLParamSet>::const_iterator itList = mParamList.begin(); itList != mParamList.end(); itList++)
	{
		const LLWLParamKey& wlKey = itList->first;
		if ( (wlKey.scope == eScope) && (boost::iequals(wlKey.name, strPresetName)) )
			return wlKey.name;
	}
	return LLStringUtil::null;
}
// [/RLVa:KB]
void LLWLParamManager::getUserPresetNames(preset_name_list_t& user) const
{
	preset_name_list_t region, sys; // unused
	getPresetNames(region, user, sys);
}

void LLWLParamManager::getLocalPresetNames(preset_name_list_t& local) const
{
	local.clear();

	for (std::map<LLWLParamKey, LLWLParamSet>::const_iterator it = mParamList.begin(); it != mParamList.end(); it++)
	{
		const LLWLParamKey& key = it->first;
		const std::string& name = key.name;

		if (key.scope != LLEnvKey::SCOPE_REGION)
		{
			local.push_back(name);
		}
	}
}

void LLWLParamManager::getPresetKeys(preset_key_list_t& keys) const
{
	keys.clear();

	for (std::map<LLWLParamKey, LLWLParamSet>::const_iterator it = mParamList.begin(); it != mParamList.end(); it++)
	{
		keys.push_back(it->first);
	}
}

boost::signals2::connection LLWLParamManager::setPresetListChangeCallback(const preset_list_signal_t::slot_type& cb)
{
	return mPresetListChangeSignal.connect(cb);
}


void LLWLParamManager::initSingleton()
{
	LL_DEBUGS("Windlight") << "Initializing sky" << LL_ENDL;

	loadAllPresets();

	// Here it used to call LLWLParamManager::initHack(), but we can't do that since it calls
	// LLWLParamManager::initSingleton() recursively. Instead, call it from LLAppViewer::init().
}

// This is really really horrible, but can't be fixed without a rewrite.
void LLWLParamManager::initHack()
{
	// load the day
	std::string preferred_day = LLEnvManagerNew::instance().getDayCycleName();
	if (!LLDayCycleManager::instance().getPreset(preferred_day, mDay))
	{
		// Fall back to default.
		LL_WARNS() << "No day cycle named " << preferred_day << ", falling back to defaults" << LL_ENDL;
		mDay.loadDayCycleFromFile("Default.xml");

		// *TODO: Fix user preferences accordingly.
	}

	// *HACK - sets cloud scrolling to what we want... fix this better in the future
	std::string sky = LLEnvManagerNew::instance().getSkyPresetName();
	if (!getParamSet(LLWLParamKey(sky, LLWLParamKey::SCOPE_LOCAL), mCurParams))
	{
		LL_WARNS() << "No sky preset named " << sky << ", falling back to defaults" << LL_ENDL;
		getParamSet(LLWLParamKey("Default", LLWLParamKey::SCOPE_LOCAL), mCurParams);

		// *TODO: Fix user preferences accordingly.
	}

	// set it to noon
	resetAnimator(0.5, LLEnvManagerNew::instance().getUseDayCycle());

	// but use linden time sets it to what the estate is
	mAnimator.setTimeType(LLWLAnimator::TIME_LINDEN);

	// This shouldn't be called here. It has nothing to do with the initialization of this singleton.
	// Instead, call it one-time when the viewer starts. Calling it here causes a recursive entry
	// of LLWLParamManager::initSingleton().
	//LLEnvManagerNew::instance().usePrefs();
}

// static
std::string LLWLParamManager::getSysDir()
{
	return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/skies", "");
}

// static
std::string LLWLParamManager::getUserDir()
{
	return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "windlight/skies", "");
}


bool LLWLParamManager::loadPresetXML(const LLWLParamKey& key, std::istream& preset_stream)
{
	LLSD params_data(LLSD::emptyMap());
	
	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	
	if(parser->parse(preset_stream, params_data, LLSDSerialize::SIZE_UNLIMITED) == LLSDParser::PARSE_FAILURE)
	{
		return false;
	}
	
	static const char* expected_windlight_settings[] = {
		"ambient",
		"blue_density",
		"blue_horizon",
		"cloud_color",
		"cloud_pos_density1",
		"cloud_pos_density2",
		"cloud_scale",
		"cloud_scroll_rate",
		"cloud_shadow",
		"density_multiplier",
		"distance_multiplier",
		"east_angle",
		"enable_cloud_scroll",
		"gamma",
		"glow",
		"haze_density",
		"haze_horizon",
		"lightnorm",
		"max_y",
		"star_brightness",
		"sun_angle",
		"sunlight_color"
	};
	static S32 expected_count = LL_ARRAY_SIZE(expected_windlight_settings);
	for(S32 i = 0; i < expected_count; ++i)
	{
		if(!params_data.has(expected_windlight_settings[i]))
		{
			LL_WARNS("WindLight") << "Attempted to load WindLight param set without " << expected_windlight_settings[i] << LL_ENDL;
			return false;
		}
	}

	if (hasParamSet(key))
	{
		setParamSet(key, params_data);
	}
	else
	{
		addParamSet(key, params_data);
	}

	return true;
}

void LLWLParamManager::loadPresetNotecard(const std::string& name, const LLUUID& asset_id, const LLUUID& inv_id)
{
	gAssetStorage->getInvItemAsset(LLHost::invalid,
								   gAgent.getID(),
								   gAgent.getSessionID(),
								   gAgent.getID(),
								   LLUUID::null,
								   inv_id,
								   asset_id,
								   LLAssetType::AT_NOTECARD,
								   &loadWindlightNotecard,
								   (void*)&inv_id);
}


bool LLWLParamManager::savePresetToNotecard(const std::string & name)
{
	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());

	LLWLParamKey key(name, LLEnvKey::SCOPE_LOCAL);
	if(!hasParamSet(key)) return false;

	// fill it with LLSD windlight params
	paramsData = mParamList[key].getAll();

	// get some XML
	std::ostringstream presetsXML;
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);

	// Write it to a notecard
	LLNotecard notecard;
	notecard.setText(presetsXML.str());
 
	LLInventoryItem *item = gInventory.getItem(mParamList[key].mInventoryID);
	if(!item)
	{
		mParamList[key].mInventoryID = LLUUID::null;
		return false;
	}
	std::string agent_url = gAgent.getRegion()->getCapability("UpdateNotecardAgentInventory");
	if(!agent_url.empty())
	{
		LLTransactionID tid;
		LLAssetID asset_id;
		tid.generate();
		asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
		
		LLVFile file(gVFS, asset_id, LLAssetType::AT_NOTECARD, LLVFile::APPEND);
		
		std::ostringstream stream;
		notecard.exportStream(stream);
		std::string buffer = stream.str();
		
		S32 size = buffer.length() + 1;
		file.setMaxSize(size);
		file.write((U8*)buffer.c_str(), size);
		LLSD body;
		body["item_id"] = item->getUUID();
		LLHTTPClient::post(agent_url, body, new LLUpdateAgentInventoryResponder(body, asset_id, LLAssetType::AT_NOTECARD));
	}
	else
	{
		LL_WARNS("WindLight") << "Failed to save notecard." << LL_ENDL;
		return false;
	}
	
	return true;
}

// static
void LLWLParamManager::loadWindlightNotecard(LLVFS *vfs, const LLUUID& asset_id, LLAssetType::EType asset_type, void *user_data, S32 status, LLExtStat ext_status)
{
	LLUUID inventory_id(*((LLUUID*)user_data));
	std::string name = "WindLight Setting.wl";
	LLViewerInventoryItem *item = gInventory.getItem(inventory_id);
	if(item)
	{
		inventory_id = item->getUUID();
		name = item->getName();
	}
	if(LL_ERR_NOERR == status)
	{
		LLVFile file(vfs, asset_id, asset_type, LLVFile::READ);
		S32 file_length = file.getSize();
		std::vector<char> buffer(file_length + 1);
		file.read((U8*)&buffer[0], file_length);
		buffer[file_length] = 0;
		LLNotecard notecard(LLNotecard::MAX_SIZE);
		LLMemoryStream str((U8*)&buffer[0], file_length + 1);
		notecard.importStream(str);
		std::string settings = notecard.getText();
		LLMemoryStream settings_str((U8*)settings.c_str(), settings.length());
		
		LLWLParamKey key((" Notecard: " + name), LLEnvKey::SCOPE_LOCAL);
		bool is_real_setting = getInstance()->loadPresetXML(key, settings_str);
		if(!is_real_setting)
		{
			LLSD subs;
			subs["NAME"] = name;
			LLNotifications::getInstance()->add("KittyInvalidWindlightNotecard", subs);
		}
		else
		{
			// We can do this because we know mCurParams
			getInstance()->mParamList[key].mInventoryID = inventory_id;
			LLEnvManagerNew::instance().setUseSkyPreset(key.name);
		}
	}
}
