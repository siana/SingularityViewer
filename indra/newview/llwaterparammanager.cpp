/**
 * @file llwaterparammanager.cpp
 * @brief Implementation for the LLWaterParamManager class.
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

#include "llwaterparammanager.h"

#include "llrender.h"

#include "pipeline.h"
#include "llsky.h"

#include "lldiriterator.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llsdserialize.h"

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

#include "v4math.h"
#include "llviewerdisplay.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "lldrawpoolwater.h"
#include "llagent.h"
#include "llviewerregion.h"

#include "llwlparammanager.h"
#include "llwaterparamset.h"
#include "llpostprocess.h"
#include "llfloaterwater.h"

#include "llagentcamera.h"

LLWaterParamManager::LLWaterParamManager() :
	mFogColor(22.f/255.f, 43.f/255.f, 54.f/255.f, 0.0f, 0.0f, "waterFogColor", "WaterFogColor"),
	mFogDensity(4, "waterFogDensity", 2),
	mUnderWaterFogMod(0.25, "underWaterFogMod"),
	mNormalScale(2.f, 2.f, 2.f, "normScale"),
	mFresnelScale(0.5f, "fresnelScale"),
	mFresnelOffset(0.4f, "fresnelOffset"),
	mScaleAbove(0.025f, "scaleAbove"),
	mScaleBelow(0.2f, "scaleBelow"),
	mBlurMultiplier(0.1f, "blurMultiplier"),
	mWave1Dir(.5f, .5f, "wave1Dir"),
	mWave2Dir(.5f, .5f, "wave2Dir"),
	mDensitySliderValue(1.0f),
	mWaterFogKS(1.0f)
{
}

LLWaterParamManager::~LLWaterParamManager()
{
}

void LLWaterParamManager::loadAllPresets()
{
	// First, load system (coming out of the box) water presets.
	loadPresetsFromDir(getSysDir());

	// Then load user presets. Note that user day presets will modify any system ones already loaded.
	loadPresetsFromDir(getUserDir());
}

void LLWaterParamManager::loadPresetsFromDir(const std::string& dir)
{
	LL_INFOS2("AppInit", "Shaders") << "Loading water presets from " << dir << LL_ENDL;

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
			llwarns << "Error loading water preset from " << path << llendl;
		}
	}
}

bool LLWaterParamManager::loadPreset(const std::string& path)
{
	llifstream xml_file;
	std::string name(LLURI::unescape(gDirUtilp->getBaseFileName(path, true)));

	xml_file.open(path.c_str());
	if (!xml_file)
	{
		return false;
	}

	LL_DEBUGS2("AppInit", "Shaders") << "Loading water " << name << LL_ENDL;

	LLSD params_data;
	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	parser->parse(xml_file, params_data, LLSDSerialize::SIZE_UNLIMITED);
	xml_file.close();

	if (hasParamSet(name))
	{
		setParamSet(name, params_data);
	}
	else
	{
		addParamSet(name, params_data);
	}

	return true;
}

bool LLWaterParamManager::loadPresetXML(const std::string& name, std::istream& preset_stream)
{
	LLSD paramsData(LLSD::emptyMap());
	
	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	
	if(parser->parse(preset_stream, paramsData, LLSDSerialize::SIZE_UNLIMITED) == LLSDParser::PARSE_FAILURE)
	{
		return false;
	}

	static const char* expected_windlight_settings[] = {
		"blurMultiplier",
		"fresnelOffset",
		"fresnelScale",
		"normScale",
		"normalMap",
		"scaleAbove",
		"scaleBelow",
		"waterFogColor",
		"waterFogDensity",
		"wave1Dir",
		"wave2Dir"
	};
	static S32 expected_count = LL_ARRAY_SIZE(expected_windlight_settings);
	for(S32 i = 0; i < expected_count; ++i)
	{
		if(!paramsData.has(expected_windlight_settings[i]))
		{
			LL_WARNS("WindLight") << "Attempted to load WindLight water param set without " << expected_windlight_settings[i] << LL_ENDL;
			return false;
		}
	}

	if(!hasParamSet(name))
	{
		addParamSet(name, paramsData);
	}
	else 
	{
		setParamSet(name, paramsData);
	}

	return true;
}

void LLWaterParamManager::loadPresetNotecard(const std::string& name, const LLUUID& asset_id, const LLUUID& inv_id)
{
	gAssetStorage->getInvItemAsset(LLHost::invalid,
								   gAgent.getID(),
								   gAgent.getSessionID(),
								   gAgent.getID(),
								   LLUUID::null,
								   inv_id,
								   asset_id,
								   LLAssetType::AT_NOTECARD,
								   &loadWaterNotecard,
								   (void*)&inv_id);
}

void LLWaterParamManager::savePreset(const std::string & name)
{
	llassert(!name.empty());

	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());
	std::string pathName(getUserDir() + LLWeb::curlEscape(name) + ".xml");

	// fill it with LLSD windlight params
	paramsData = mParamList[name].getAll();

	// write to file
	llofstream presetsXML(pathName);
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
	presetsXML.close();

	propagateParameters();
}

// Yes, this function is completely identical to LLWLParamManager::savePresetToNotecard.
// I feel some refactoring of this whole WindLight thing would be generally beneficial.
// Damned if I'm going to be the one to do it, though.
bool LLWaterParamManager::savePresetToNotecard(const std::string & name)
{
	if(!hasParamSet(name)) return false;

	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());
	
	// fill it with LLSD windlight params
	paramsData = mParamList[name].getAll();
	
	// get some XML
	std::ostringstream presetsXML;
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
	
	// Write it to a notecard
	LLNotecard notecard;
	notecard.setText(presetsXML.str());
	
	LLInventoryItem *item = gInventory.getItem(mParamList[name].mInventoryID);
	if(!item)
	{
		mParamList[name].mInventoryID = LLUUID::null;
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
		LL_WARNS("WindLight") << "Stuff the legacy system." << LL_ENDL;
		return false;
	}

	return true;
}

void LLWaterParamManager::propagateParameters(void)
{
	// bind the variables only if we're using shaders
	if(gPipeline.canUseVertexShaders())
	{
		std::vector<LLGLSLShader*>::iterator shaders_iter=mShaderList.begin();
		for(; shaders_iter != mShaderList.end(); ++shaders_iter)
		{
			(*shaders_iter)->mUniformsDirty = TRUE;
		}
	}

	bool err;
	F32 fog_density_slider =
			log(mCurParams.getFloat(mFogDensity.mName, err)) /
			log(mFogDensity.mBase);

	setDensitySliderValue(fog_density_slider);
}

void LLWaterParamManager::updateShaderUniforms(LLGLSLShader * shader)
{
	if (shader->mShaderGroup == LLGLSLShader::SG_WATER)
	{
		shader->uniform4fv(LLViewerShaderMgr::LIGHTNORM, 1, LLWLParamManager::getInstance()->getRotatedLightDir().mV);
		shader->uniform3fv("camPosLocal", 1, LLViewerCamera::getInstance()->getOrigin().mV);
		shader->uniform4fv("waterFogColor", 1, LLDrawPoolWater::sWaterFogColor.mV);
		shader->uniform4fv("waterPlane", 1, mWaterPlane.mV);
		shader->uniform1f("waterFogDensity", getFogDensity());
		shader->uniform1f("waterFogKS", mWaterFogKS);
		shader->uniform1f("distance_multiplier", 0);
	}
}

void LLWaterParamManager::applyParams(const LLSD& params, bool interpolate)
{
	if (params.size() == 0)
	{
		llwarns << "Undefined water params" << llendl;
		return;
	}

	if (interpolate)
	{
		LLWLParamManager::getInstance()->mAnimator.startInterpolation(params);
	}
	else
	{
		mCurParams.setAll(params);
	}
}

static LLFastTimer::DeclareTimer FTM_UPDATE_WATERPARAM("Update Water Params");

void LLWaterParamManager::updateShaderLinks()
{
	mShaderList.clear();
	LLViewerShaderMgr::shader_iter shaders_iter, end_shaders;
	end_shaders = LLViewerShaderMgr::instance()->endShaders();
	for(shaders_iter = LLViewerShaderMgr::instance()->beginShaders(); shaders_iter != end_shaders; ++shaders_iter)
	{
		if (shaders_iter->mProgramObject != 0
			&& shaders_iter->mShaderGroup == LLGLSLShader::SG_WATER)
		{
			if(	glGetUniformLocationARB(shaders_iter->mProgramObject,"lightnorm")>=0		||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"camPosLocal")>=0		||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"waterFogColor")>=0	||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"waterPlane")>=0		||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"waterFogDensity")>=0	||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"waterFogKS")>=0		||
				glGetUniformLocationARB(shaders_iter->mProgramObject,"distance_multiplier")>=0)
			mShaderList.push_back(&(*shaders_iter));
		}
	}
}

void LLWaterParamManager::update(LLViewerCamera * cam)
{
	LLFastTimer ftm(FTM_UPDATE_WATERPARAM);

	// update the shaders and the menu
	propagateParameters();
	
	// sync menus if they exist
	if(LLFloaterWater::isOpen()) 
	{
		LLFloaterWater::instance()->syncMenu();
	}

	stop_glerror();

	// only do this if we're dealing with shaders
	if(gPipeline.canUseVertexShaders()) 
	{
		//transform water plane to eye space
		glh::vec3f norm(0.f, 0.f, 1.f);
		glh::vec3f p(0.f, 0.f, gAgent.getRegion()->getWaterHeight()+0.1f);
		
		F32 modelView[16];
		for (U32 i = 0; i < 16; i++)
		{
			modelView[i] = (F32) gGLModelView[i];
		}

		glh::matrix4f mat(modelView);
		glh::matrix4f invtrans = mat.inverse().transpose();
		glh::vec3f enorm;
		glh::vec3f ep;
		invtrans.mult_matrix_vec(norm, enorm);
		enorm.normalize();
		mat.mult_matrix_vec(p, ep);

		mWaterPlane = LLVector4(enorm.v[0], enorm.v[1], enorm.v[2], -ep.dot(enorm));

		LLVector3 sunMoonDir;
		if (gSky.getSunDirection().mV[2] > LLSky::NIGHTTIME_ELEVATION_COS) 	 
		{ 	 
			sunMoonDir = gSky.getSunDirection(); 	 
		} 	 
		else  	 
		{
			sunMoonDir = gSky.getMoonDirection();
		}
		sunMoonDir.normVec();
		mWaterFogKS = 1.f/llmax(sunMoonDir.mV[2], WATER_FOG_LIGHT_CLAMP);

		std::vector<LLGLSLShader*>::iterator shaders_iter=mShaderList.begin();
		for(; shaders_iter != mShaderList.end(); ++shaders_iter)
		{
			(*shaders_iter)->mUniformsDirty = TRUE;
		}

	}
}


bool LLWaterParamManager::addParamSet(const std::string& name, LLWaterParamSet& param)
{
	// add a new one if not one there already
	preset_map_t::iterator mIt = mParamList.find(name);
	if(mIt == mParamList.end()) 
	{	
		mParamList[name] = param;
		mPresetListChangeSignal();
		return true;
	}

	return false;
}

BOOL LLWaterParamManager::addParamSet(const std::string& name, LLSD const & param)
{
	LLWaterParamSet param_set;
	param_set.setAll(param);
	return addParamSet(name, param_set);
}

bool LLWaterParamManager::getParamSet(const std::string& name, LLWaterParamSet& param)
{
	// find it and set it
	preset_map_t::iterator mIt = mParamList.find(name);
	if(mIt != mParamList.end()) 
	{
		param = mParamList[name];
		param.mName = name;
		return true;
	}

	return false;
}

bool LLWaterParamManager::hasParamSet(const std::string& name)
{
	LLWaterParamSet dummy;
	return getParamSet(name, dummy);
}

bool LLWaterParamManager::setParamSet(const std::string& name, LLWaterParamSet& param)
{
	mParamList[name] = param;

	return true;
}

bool LLWaterParamManager::setParamSet(const std::string& name, const LLSD & param)
{
	// quick, non robust (we won't be working with files, but assets) check
	if(!param.isMap()) 
	{
		return false;
	}
	
	mParamList[name].setAll(param);

	return true;
}

bool LLWaterParamManager::removeParamSet(const std::string& name, bool delete_from_disk)
{
	// remove from param list
	preset_map_t::iterator it = mParamList.find(name);
	if (it == mParamList.end())
	{
		LL_WARNS("WindLight") << "No water preset named " << name << LL_ENDL;
		return false;
	}

	mParamList.erase(it);

	// remove from file system if requested
	if (delete_from_disk)
	{
		if (gDirUtilp->deleteFilesInDir(getUserDir(), LLWeb::curlEscape(name) + ".xml") < 1)
		{
			LL_WARNS("WindLight") << "Error removing water preset " << name << " from disk" << LL_ENDL;
		}
	}

	// signal interested parties
	mPresetListChangeSignal();
	return true;
}

bool LLWaterParamManager::isSystemPreset(const std::string& preset_name) const
{
	// *TODO: file system access is excessive here.
	return gDirUtilp->fileExists(getSysDir() + LLURI::escape(preset_name) + ".xml");
}

void LLWaterParamManager::getPresetNames(preset_name_list_t& presets) const
{
	presets.clear();

	for (preset_map_t::const_iterator it = mParamList.begin(); it != mParamList.end(); ++it)
	{
		presets.push_back(it->first);
	}
}

void LLWaterParamManager::getPresetNames(preset_name_list_t& user_presets, preset_name_list_t& system_presets) const
{
	user_presets.clear();
	system_presets.clear();

	for (preset_map_t::const_iterator it = mParamList.begin(); it != mParamList.end(); ++it)
	{
		if (isSystemPreset(it->first))
		{
			system_presets.push_back(it->first);
		}
		else
		{
			user_presets.push_back(it->first);
		}
	}
}

void LLWaterParamManager::getUserPresetNames(preset_name_list_t& user_presets) const
{
	preset_name_list_t dummy;
	getPresetNames(user_presets, dummy);
}

boost::signals2::connection LLWaterParamManager::setPresetListChangeCallback(const preset_list_signal_t::slot_type& cb)
{
	return mPresetListChangeSignal.connect(cb);
}

F32 LLWaterParamManager::getFogDensity(void)
{
	bool err;

	F32 fogDensity = mCurParams.getFloat("waterFogDensity", err);
	
	// modify if we're underwater
	const F32 water_height = gAgent.getRegion() ? gAgent.getRegion()->getWaterHeight() : 0.f;
	F32 camera_height = gAgentCamera.getCameraPositionAgent().mV[2];
	if(camera_height <= water_height)
	{
		// raise it to the underwater fog density modifier
		fogDensity = pow(fogDensity, mCurParams.getFloat("underWaterFogMod", err));
	}

	return fogDensity;
}

// static
void LLWaterParamManager::initSingleton()
{
	LL_DEBUGS("Windlight") << "Initializing water" << LL_ENDL;

	loadAllPresets();

	// This shouldn't be called here. It has nothing to do with the initialization of this singleton.
	// Instead, call it one-time when the viewer starts. Calling it here causes a recursive entry
	// of LLWaterParamManager::initSingleton().
	//LLEnvManagerNew::instance().usePrefs();
}

// static
std::string LLWaterParamManager::getSysDir()
{
	return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/water", "");
}

// static
std::string LLWaterParamManager::getUserDir()
{
	return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "windlight/water", "");
}

// static
void LLWaterParamManager::loadWaterNotecard(LLVFS *vfs, const LLUUID& asset_id, LLAssetType::EType asset_type, void *user_data, S32 status, LLExtStat ext_status)
{
	LLUUID inventory_id(*((LLUUID*)user_data));
	std::string name = "WindLight Setting.ww";
	LLViewerInventoryItem *item = gInventory.getItem(inventory_id);
	if(item)
	{
		inventory_id = item->getUUID();
		name = item->getName();
	}
	if(LL_ERR_NOERR == status)
	{
		std::string key(" Notecard: " + name);

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

		bool is_real_setting = getInstance()->loadPresetXML(key, settings_str);
		if(!is_real_setting)
		{
			LLSD subs;
			subs["NAME"] = name;
			LLNotifications::instance().add("KittyInvalidWaterlightNotecard", subs);
		}
		else
		{
			// We can do this because we know mCurParams 
			getInstance()->mParamList[key].mInventoryID = inventory_id;
			LLEnvManagerNew::instance().setUseWaterPreset(key);
		}
	}
}
