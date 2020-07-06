/** 
 * @file llpaneldisplay.cpp
 * @brief Display preferences for the preferences floater
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

#include "llviewerprecompiledheaders.h"

// file include
#include "llpaneldisplay.h"

// linden library includes
#include "llerror.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llstring.h"

// project includes
#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llflexibleobject.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewermessage.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvovolume.h"
#include "llvotree.h"
#include "llvosky.h"
#include "llwindow.h"
#include "llworld.h"
#include "pipeline.h"
#include "lluictrlfactory.h"
#include "llfeaturemanager.h"
#include "llviewershadermgr.h"
#include "llboost.h"

//RN temporary includes for resolution switching
#include "llglheaders.h"
#include "llviewercontrol.h"
#include "llsky.h"

// parent
#include "llfloaterpreference.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

#if LL_MSVC
#pragma warning( disable       : 4265 )	// "class has virtual functions, but destructor is not virtual"
#endif


#include <boost/regex.hpp>

const F32 MAX_USER_FAR_CLIP = 512.f;
const F32 MIN_USER_FAR_CLIP = 64.f;

const S32 ASPECT_RATIO_STR_LEN = 100;

void reset_to_default(const std::string& control);
void reset_all_to_default(const LLView* panel)
{
	LLView::child_list_const_iter_t	end(panel->endChild());
	for (LLView::child_list_const_iter_t i = panel->beginChild(); i != end; ++i)
	{
		const std::string& control_name((*i)->getControlName());
		if (control_name.empty()) continue;
		if (control_name == "RenderDepthOfField") continue; // Don't touch render settings *sigh* hack
		reset_to_default(control_name);
	}
}

LLPanelDisplay::LLPanelDisplay()
{
	mCommitCallbackRegistrar.add("Graphics.ResetTab", boost::bind(reset_all_to_default, boost::bind(&LLView::getChildView, this, _2, true, false)));
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_graphics1.xml");
}

void updateSliderText(LLSliderCtrl* slider, LLTextBox* text_box);

BOOL LLPanelDisplay::postBuild()
{
	// return to default values
	getChild<LLButton>("Defaults")->setClickedCallback(boost::bind(&LLPanelDisplay::setHardwareDefaults, this));
	
	//============================================================================
	// Resolution
	
	// radio set for fullscreen size
	
	mCtrlWindowed = getChild<LLCheckBoxCtrl>( "windowed mode");
	mCtrlWindowed->setCommitCallback(boost::bind(&LLPanelDisplay::onCommitWindowedMode,this));

	mAspectRatioLabel1 = getChild<LLTextBox>("AspectRatioLabel1");
	mDisplayResLabel = getChild<LLTextBox>("DisplayResLabel");

	S32 num_resolutions = 0;
	LLWindow::LLWindowResolution* supported_resolutions = gViewerWindow->getWindow()->getSupportedResolutions(num_resolutions);

	S32 fullscreen_mode = num_resolutions - 1;

	mCtrlFullScreen = getChild<LLComboBox>( "fullscreen combo");
	
	LLUIString resolution_label = getString("resolution_format");

	for (S32 i = 0; i < num_resolutions; i++)
	{
		resolution_label.setArg("[RES_X]", llformat("%d", supported_resolutions[i].mWidth));
		resolution_label.setArg("[RES_Y]", llformat("%d", supported_resolutions[i].mHeight));
		mCtrlFullScreen->add( resolution_label, ADD_BOTTOM );
	}

	{
		BOOL targetFullscreen;
		S32 targetWidth;
		S32 targetHeight;
		
		gViewerWindow->getTargetWindow(targetFullscreen, targetWidth, targetHeight);
		
		if (targetFullscreen)
		{
			fullscreen_mode = 0; // default to 800x600
			for (S32 i = 0; i < num_resolutions; i++)
			{
				if (targetWidth == supported_resolutions[i].mWidth
				&&  targetHeight == supported_resolutions[i].mHeight)
				{
					fullscreen_mode = i;
				}
			}
			mCtrlFullScreen->setCurrentByIndex(fullscreen_mode);
			mCtrlWindowed->set(FALSE);
			mCtrlFullScreen->setVisible(TRUE);
		}
		else
		{
			// set to windowed mode
			//fullscreen_mode = mCtrlFullScreen->getItemCount() - 1;
			mCtrlWindowed->set(TRUE);
			mCtrlFullScreen->setCurrentByIndex(0);
			mCtrlFullScreen->setVisible(FALSE);
		}
	}

	initWindowSizeControls();

	if (gSavedSettings.getBOOL("FullScreenAutoDetectAspectRatio"))
	{
		mAspectRatio = gViewerWindow->getDisplayAspectRatio();
	}
	else
	{
		mAspectRatio = gSavedSettings.getF32("FullScreenAspectRatio");
	}

	S32 numerator = 0;
	S32 denominator = 0;
	fractionFromDecimal(mAspectRatio, numerator, denominator);

	LLUIString aspect_ratio_text = getString("aspect_ratio_text");
	if (numerator != 0)
	{
		aspect_ratio_text.setArg("[NUM]", llformat("%d",  numerator));
		aspect_ratio_text.setArg("[DEN]", llformat("%d",  denominator));
	}
	else
	{
		aspect_ratio_text = llformat("%.3f", mAspectRatio);
	}

	mCtrlAutoDetectAspect = getChild<LLCheckBoxCtrl>( "aspect_auto_detect");
	mCtrlAutoDetectAspect->setCommitCallback(boost::bind(&LLPanelDisplay::onCommitAutoDetectAspect,this,_2));

	mCtrlAspectRatio = getChild<LLComboBox>( "aspect_ratio");
	mCtrlAspectRatio->setTextEntryCallback(boost::bind(&LLUICtrl::setValue, mCtrlAutoDetectAspect, false));
	mCtrlAspectRatio->setCommitCallback(boost::bind(&LLUICtrl::setValue, mCtrlAutoDetectAspect, false));
	// add default aspect ratios
	mCtrlAspectRatio->add(aspect_ratio_text, &mAspectRatio, ADD_TOP);
	mCtrlAspectRatio->setCurrentByIndex(0);

	// radio performance box
	mCtrlSliderQuality = getChild<LLSliderCtrl>("QualityPerformanceSelection");
	mCtrlSliderQuality->setSliderMouseUpCallback(boost::bind(&LLPanelDisplay::onChangeQuality,this,_1));

	mCtrlCustomSettings = getChild<LLCheckBoxCtrl>("CustomSettings");
	mCtrlCustomSettings->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));

	//----------------------------------------------------------------------------
	// Enable Bump/Shiny
	mCtrlBumpShiny = getChild<LLCheckBoxCtrl>("BumpShiny");
	
	//----------------------------------------------------------------------------
	// Enable Reflections
	mCtrlReflectionDetail = getChild<LLComboBox>("ReflectionDetailCombo");
	mCtrlReflectionDetail->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));

	// WindLight
	mCtrlWindLight = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	mCtrlWindLight->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));

	// Deferred
	mCtrlDeferred = getChild<LLCheckBoxCtrl>("RenderDeferred");
	mCtrlDeferred->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));
	mCtrlDeferredDoF = getChild<LLCheckBoxCtrl>("RenderDepthOfField");
	mCtrlDeferredDoF->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));
	mCtrlShadowDetail = getChild<LLComboBox>("ShadowDetailCombo");
	mCtrlShadowDetail->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));

	//----------------------------------------------------------------------------
	// Terrain Scale
	mCtrlTerrainScale = getChild<LLComboBox>("TerrainScaleCombo");

	//----------------------------------------------------------------------------
	// Enable Avatar Shaders
	mCtrlAvatarVP = getChild<LLCheckBoxCtrl>("AvatarVertexProgram");
	mCtrlAvatarVP->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));

	//----------------------------------------------------------------------------
	// Avatar Render Mode
	mCtrlAvatarCloth = getChild<LLCheckBoxCtrl>("AvatarCloth");
	mCtrlAvatarImpostors = getChild<LLCheckBoxCtrl>("AvatarImpostors");
	mCtrlAvatarImpostors->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));

	//----------------------------------------------------------------------------
	// Checkbox for ambient occlusion
	mCtrlAmbientOcc = getChild<LLCheckBoxCtrl>("UseSSAO");

	//----------------------------------------------------------------------------
	// radio set for terrain detail mode
	mRadioTerrainDetail = getChild<LLRadioGroup>("TerrainDetailRadio");

	//----------------------------------------------------------------------------
	// Global Shader Enable
	mCtrlShaderEnable = getChild<LLCheckBoxCtrl>("BasicShaders");
	mCtrlShaderEnable->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));
	
	//============================================================================

	// Object detail slider
	LLSliderCtrl* ctrl_slider = getChild<LLSliderCtrl>("ObjectMeshDetail");
	LLTextBox* slider_text = getChild<LLTextBox>("ObjectMeshDetailText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Flex object detail slider
	ctrl_slider = getChild<LLSliderCtrl>("FlexibleMeshDetail");
	slider_text = getChild<LLTextBox>("FlexibleMeshDetailText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Tree detail slider
	ctrl_slider = getChild<LLSliderCtrl>("TreeMeshDetail");
	slider_text = getChild<LLTextBox>("TreeMeshDetailText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Avatar detail slider
	ctrl_slider = getChild<LLSliderCtrl>("AvatarMeshDetail");
	slider_text = getChild<LLTextBox>("AvatarMeshDetailText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Avatar physics detail slider
	ctrl_slider = getChild<LLSliderCtrl>("AvatarPhysicsDetail");
	slider_text = getChild<LLTextBox>("AvatarPhysicsDetailText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Terrain detail slider
	ctrl_slider = getChild<LLSliderCtrl>("TerrainMeshDetail");
	slider_text = getChild<LLTextBox>("TerrainMeshDetailText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Terrain detail slider
	ctrl_slider = getChild<LLSliderCtrl>("SkyMeshDetail");
	slider_text = getChild<LLTextBox>("SkyMeshDetailText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Glow detail slider
	ctrl_slider = getChild<LLSliderCtrl>("RenderPostProcess");
	slider_text = getChild<LLTextBox>("PostProcessText");
	ctrl_slider->setCommitCallback(boost::bind(updateSliderText, ctrl_slider, slider_text));
	updateSliderText(ctrl_slider, slider_text);

	// Text boxes (for enabling/disabling)

	// Hardware tab
	mVBO = getChild<LLCheckBoxCtrl>("vbo");
	mVBO->setCommitCallback(boost::bind(&LLPanelDisplay::refreshEnabledState, this));

	if(gGLManager.mIsATI)	//AMD gpus don't go beyond 8x fsaa.
	{
		LLComboBox* fsaa = getChild<LLComboBox>("fsaa");
		fsaa->remove("16x");
	}
	if(!gGLManager.mHasAdaptiveVsync)
	{
		LLComboBox* vsync = getChild<LLComboBox>("vsync");
		vsync->remove("VSyncAdaptive");
	}

	refresh();

	return TRUE;
}

void LLPanelDisplay::initWindowSizeControls()
{
	// Window size
	mWindowSizeLabel = getChild<LLTextBox>("WindowSizeLabel");
	mCtrlWindowSize = getChild<LLComboBox>("windowsize combo");

	// Look to see if current window size matches existing window sizes, if so then
	// just set the selection value...
	const U32 height = gViewerWindow->getWindowDisplayHeight();
	const U32 width = gViewerWindow->getWindowDisplayWidth();
	for (S32 i=0; i < mCtrlWindowSize->getItemCount(); i++)
	{
		U32 height_test = 0;
		U32 width_test = 0;
		mCtrlWindowSize->setCurrentByIndex(i);
		if (extractWindowSizeFromString(mCtrlWindowSize->getValue().asString(), width_test, height_test))
		{
			if ((height_test == height) && (width_test == width))
			{
				return;
			}
		}
	}
	// ...otherwise, add a new entry with the current window height/width.
	LLUIString resolution_label = getString("resolution_format");
	resolution_label.setArg("[RES_X]", llformat("%d", width));
	resolution_label.setArg("[RES_Y]", llformat("%d", height));
	mCtrlWindowSize->add(resolution_label, ADD_TOP);
	mCtrlWindowSize->setCurrentByIndex(0);
}

LLPanelDisplay::~LLPanelDisplay()
{
	// clean up user data
	for (S32 i = 0; i < mCtrlAspectRatio->getItemCount(); i++)
	{
		mCtrlAspectRatio->setCurrentByIndex(i);
	}
	for (S32 i = 0; i < mCtrlWindowSize->getItemCount(); i++)
	{
		mCtrlWindowSize->setCurrentByIndex(i);
	}
}

void LLPanelDisplay::refresh()
{
	LLPanel::refresh();
	
	mFSAutoDetectAspect = gSavedSettings.getBOOL("FullScreenAutoDetectAspectRatio");

	mQualityPerformance = gSavedSettings.getU32("RenderQualityPerformance");
	mCustomSettings = gSavedSettings.getBOOL("RenderCustomSettings");

	mTransparentWater = gSavedSettings.getBOOL("RenderTransparentWater");
	// shader settings
	mBumpShiny = gSavedSettings.getBOOL("RenderObjectBump");
	mShaderEnable = gSavedSettings.getBOOL("VertexShaderEnable");
	mWindLight = gSavedSettings.getBOOL("WindLightUseAtmosShaders");
	mAvatarVP = gSavedSettings.getBOOL("RenderAvatarVP");
	mDeferred = gSavedSettings.getBOOL("RenderDeferred");
	mDeferredDoF = gSavedSettings.getBOOL("RenderDepthOfField");
	mDeferredSSAO = gSavedSettings.getBOOL("RenderDeferredSSAO");

	// combo boxes
	mReflectionDetail = gSavedSettings.getS32("RenderReflectionDetail");
	mShadowDetail = gSavedSettings.getS32("RenderShadowDetail");
	mTerrainScale = gSavedSettings.getF32("RenderTerrainScale");

	// avatar settings
	mAvatarImpostors = gSavedSettings.getBOOL("RenderUseImpostors");
	mNonImpostors = gSavedSettings.getS32("RenderAvatarMaxVisible");
	mAvatarCloth = gSavedSettings.getBOOL("RenderAvatarCloth");

	// Draw distance
	mRenderFarClip = gSavedSettings.getF32("RenderFarClip");

	// sliders and their text boxes
	mPrimLOD = gSavedSettings.getF32("RenderVolumeLODFactor");
	mFlexLOD = gSavedSettings.getF32("RenderFlexTimeFactor");
	mTreeLOD = gSavedSettings.getF32("RenderTreeLODFactor");
	mAvatarLOD = gSavedSettings.getF32("RenderAvatarLODFactor");
	mTerrainLOD = gSavedSettings.getF32("RenderTerrainLODFactor");
	mSkyLOD = gSavedSettings.getU32("WLSkyDetail");
	mParticleCount = gSavedSettings.getS32("RenderMaxPartCount");
	mPostProcess = gSavedSettings.getS32("RenderGlowResolutionPow");
	
	// lighting and terrain radios
	mLocalLights = gSavedSettings.getBOOL("RenderLocalLights");
	mTerrainDetail =  gSavedSettings.getS32("RenderTerrainDetail");

	// Hardware tab
	mUseVBO = gSavedSettings.getBOOL("RenderVBOEnable");
	mUseFBO = gSavedSettings.getBOOL("RenderUseFBO");
	mUseAniso = gSavedSettings.getBOOL("RenderAnisotropic");
	mFSAASamples = gSavedSettings.getU32("RenderFSAASamples");
	mGamma = gSavedSettings.getF32("RenderGamma");
	mVideoCardMem = gSavedSettings.getS32("TextureMemory");
	mFogRatio = gSavedSettings.getF32("RenderFogRatio");
	mVsyncMode = gSavedSettings.getS32("SHRenderVsyncMode");

	childSetValue("fsaa", (LLSD::Integer) mFSAASamples);
	childSetValue("vsync", (LLSD::Integer) mVsyncMode);

	// Depth of Field tab
	mFNumber = gSavedSettings.getF32("CameraFNumber");
	mFocalLength = gSavedSettings.getF32("CameraFocalLength");
	mMaxCoF = gSavedSettings.getF32("CameraMaxCoF");
	mFocusTrans = gSavedSettings.getF32("CameraFocusTransitionTime");
	mDoFRes = gSavedSettings.getF32("CameraDoFResScale");

	refreshEnabledState();
}

void LLPanelDisplay::refreshEnabledState()
{
	// if in windowed mode, disable full screen options
	bool isFullScreen = !mCtrlWindowed->get();

	mDisplayResLabel->setVisible(isFullScreen);
	mCtrlFullScreen->setVisible(isFullScreen);
	mCtrlAspectRatio->setVisible(isFullScreen);
	mAspectRatioLabel1->setVisible(isFullScreen);
	mCtrlAutoDetectAspect->setVisible(isFullScreen);
	mWindowSizeLabel->setVisible(!isFullScreen);
	mCtrlWindowSize->setVisible(!isFullScreen);

	F32 mem_multiplier = gSavedSettings.getF32("RenderTextureMemoryMultiple");
	S32Megabytes min_tex_mem = LLViewerTextureList::getMinVideoRamSetting();
	S32Megabytes max_tex_mem = LLViewerTextureList::getMaxVideoRamSetting(false, mem_multiplier);

	// Hardware tab
	getChild<LLUICtrl>("GrapicsCardTextureMemory")->setMinValue(min_tex_mem.value());
	getChild<LLUICtrl>("GrapicsCardTextureMemory")->setMaxValue(max_tex_mem.value());

	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderVBOEnable") ||
		!gGLManager.mHasVertexBufferObject)
	{
		mVBO->setEnabled(false);
		mVBO->setValue(false);
	}

	static LLCachedControl<bool> wlatmos("WindLightUseAtmosShaders",false);
	// if no windlight shaders, enable gamma, and fog distance
	getChildView("gamma")->setEnabled(!wlatmos);
	getChildView("fog")->setEnabled(!wlatmos);
	getChildView("note")->setVisible(wlatmos);

	// disable graphics settings and exit if it's not set to custom
	if (!gSavedSettings.getBOOL("RenderCustomSettings")) return;

	// Reflections
	BOOL reflections = gSavedSettings.getBOOL("VertexShaderEnable") 
		&& gGLManager.mHasCubeMap 
		&& LLCubeMap::sUseCubeMaps;
	mCtrlReflectionDetail->setEnabled(reflections);
	
	// Bump & Shiny
	bool bumpshiny = gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps && LLFeatureManager::getInstance()->isFeatureAvailable("RenderObjectBump");
	mCtrlBumpShiny->setEnabled(bumpshiny);

	mCtrlAvatarCloth->setEnabled(gSavedSettings.getBOOL("VertexShaderEnable") && gSavedSettings.getBOOL("RenderAvatarVP"));

	//I actually recommend RenderUseFBO:FALSE for ati users when not using deferred, so RenderUseFBO shouldn't control visibility of the element.
	// Instead, gGLManager.mHasFramebufferObject seems better as it is determined by hardware and not current user settings. -Shyotl
	//Enabling deferred will force RenderUseFBO to TRUE.
	BOOL can_defer = LLPipeline::isRenderDeferredCapable() &&
		gSavedSettings.getBOOL("VertexShaderEnable") && //Basic Shaders
		wlatmos; //Atmospheric Shaders


	mCtrlDeferred->setEnabled(can_defer);
	static LLCachedControl<bool> render_deferred("RenderDeferred",false);
	mCtrlShadowDetail->setEnabled(can_defer && render_deferred);
	mCtrlAmbientOcc->setEnabled(can_defer && render_deferred);
	mCtrlDeferredDoF->setEnabled(can_defer && render_deferred);

	// Vertex Shaders
//	mCtrlShaderEnable->setEnabled(LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"));
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Modified: RLVa-0.2.0a
	// "Basic Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlShaderEnable = LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable");
	mCtrlShaderEnable->setEnabled(fCtrlShaderEnable && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV) || !mShaderEnable));
// [/RLVa:KB]

	bool shaders = mCtrlShaderEnable->get();
	if (shaders)
	{
		mRadioTerrainDetail->setValue(1);
		mRadioTerrainDetail->setEnabled(false);
	}
	else
	{
		mRadioTerrainDetail->setEnabled(true);
	}

	// *HACK just checks to see if we can use shaders... 
	// maybe some cards that use shaders, but don't support windlight
//	mCtrlWindLight->setEnabled(mCtrlShaderEnable->getEnabled() && shaders);
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Modified: RLVa-0.2.0a
	// "Atmospheric Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlWindLightEnable = fCtrlShaderEnable && shaders;
	mCtrlWindLight->setEnabled(fCtrlWindLightEnable && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV) || !mWindLight));
// [/RLVa:KB]

	// Avatar Mode and FBO
	if (render_deferred && wlatmos && shaders)
	{
		getChildView("fbo")->setEnabled(false);
		//getChildView("fbo")->setValue(true);
		mCtrlAvatarVP->setEnabled(false);
		//gSavedSettings.setBOOL("RenderAvatarVP", true);
	}
	else
	{
		getChildView("fbo")->setEnabled(gGLManager.mHasFramebufferObject);
		//getChildView("fbo")->setValue(gSavedSettings.getBOOL("RenderUseFBO"));
		mCtrlAvatarVP->setEnabled(shaders);
		//if (!shaders) gSavedSettings.setBOOL("RenderAvatarVP", false);
	}

	// now turn off any features that are unavailable
	disableUnavailableSettings();
}

void LLPanelDisplay::disableUnavailableSettings()
{	
	// disabled terrain scale
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderTerrainScale"))
	{
		mCtrlTerrainScale->setEnabled(false);
		mCtrlTerrainScale->setValue(false);
	}
	// disabled impostors
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderUseImpostors"))
	{
		mCtrlAvatarImpostors->setEnabled(false);
		mCtrlAvatarImpostors->setValue(false);
	}

	// if vertex shaders off, disable all shader related products
	// Singu Note: Returns early this, place all unrelated checks above now.
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"))
	{
		mCtrlShaderEnable->setEnabled(FALSE);
		mCtrlShaderEnable->setValue(FALSE);

		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);

		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(FALSE);

		mCtrlAvatarVP->setEnabled(FALSE);
		mCtrlAvatarVP->setValue(FALSE);

		mCtrlAvatarCloth->setEnabled(FALSE);
		mCtrlAvatarCloth->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
		mCtrlAmbientOcc->setEnabled(FALSE);
		mCtrlAmbientOcc->setValue(FALSE);
		mCtrlDeferredDoF->setEnabled(FALSE);
		mCtrlDeferredDoF->setValue(FALSE);
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(FALSE);
		return;
	}

	// disabled windlight
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders"))
	{
		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);
	}

	// disabled reflections
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderReflectionDetail"))
	{
		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(FALSE);
	}

	// disabled av
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP"))
	{
		mCtrlAvatarVP->setEnabled(FALSE);
		mCtrlAvatarVP->setValue(FALSE);

		mCtrlAvatarCloth->setEnabled(FALSE);
		mCtrlAvatarCloth->setValue(FALSE);
	}
	// disabled cloth
	else if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarCloth"))
	{
		mCtrlAvatarCloth->setEnabled(FALSE);
		mCtrlAvatarCloth->setValue(FALSE);
	}
	// disabled deferred
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred"))
	{
		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
		mCtrlAmbientOcc->setEnabled(FALSE);
		mCtrlAmbientOcc->setValue(FALSE);
		mCtrlDeferredDoF->setEnabled(FALSE);
		mCtrlDeferredDoF->setValue(FALSE);
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(FALSE);
	}

}

void LLPanelDisplay::cancel()
{
	gSavedSettings.setBOOL("FullScreenAutoDetectAspectRatio", mFSAutoDetectAspect);
	gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);

	gSavedSettings.setU32("RenderQualityPerformance", mQualityPerformance);

	gSavedSettings.setBOOL("RenderCustomSettings", mCustomSettings);

	gSavedSettings.setBOOL("RenderTransparentWater", mTransparentWater);
	gSavedSettings.setBOOL("RenderObjectBump", mBumpShiny);
	gSavedSettings.setBOOL("VertexShaderEnable", mShaderEnable);
	gSavedSettings.setBOOL("WindLightUseAtmosShaders", mWindLight);

	gSavedSettings.setBOOL("RenderAvatarVP", mAvatarVP);
	gSavedSettings.setBOOL("RenderDeferred", mDeferred);
	gSavedSettings.setBOOL("RenderDepthOfField", mDeferredDoF);
	gSavedSettings.setBOOL("RenderDeferredSSAO", mDeferredSSAO);

	gSavedSettings.setS32("RenderReflectionDetail", mReflectionDetail);
	gSavedSettings.setS32("RenderShadowDetail", mShadowDetail);
	gSavedSettings.setF32("RenderTerrainScale", mTerrainScale);

	gSavedSettings.setBOOL("RenderUseImpostors", mAvatarImpostors);
	gSavedSettings.setS32("RenderAvatarMaxVisible", mNonImpostors);
	gSavedSettings.setBOOL("RenderAvatarCloth", mAvatarCloth);

	gSavedSettings.setBOOL("RenderLocalLights", mLocalLights);
	gSavedSettings.setS32("RenderTerrainDetail", mTerrainDetail);

	gSavedSettings.setF32("RenderFarClip", mRenderFarClip);
	gSavedSettings.setF32("RenderVolumeLODFactor", mPrimLOD);
	gSavedSettings.setF32("RenderFlexTimeFactor", mFlexLOD);
	gSavedSettings.setF32("RenderTreeLODFactor", mTreeLOD);
	gSavedSettings.setF32("RenderAvatarLODFactor", mAvatarLOD);
	gSavedSettings.setF32("RenderTerrainLODFactor", mTerrainLOD);
	gSavedSettings.setU32("WLSkyDetail", mSkyLOD);
	gSavedSettings.setS32("RenderMaxPartCount", mParticleCount);
	gSavedSettings.setS32("RenderGlowResolutionPow", mPostProcess);

	// Hardware tab
	gSavedSettings.setBOOL("RenderVBOEnable", mUseVBO);
	gSavedSettings.setBOOL("RenderUseFBO", mUseFBO);
	gSavedSettings.setBOOL("RenderAnisotropic", mUseAniso);
	gSavedSettings.setU32("RenderFSAASamples", mFSAASamples);
	gSavedSettings.setF32("RenderGamma", mGamma);
	gSavedSettings.setS32("TextureMemory", mVideoCardMem);
	gSavedSettings.setF32("RenderFogRatio", mFogRatio);
	gSavedSettings.setS32("SHRenderVsyncMode", mVsyncMode);

	// Depth of Field tab
	gSavedSettings.setF32("CameraFNumber", mFNumber);
	gSavedSettings.setF32("CameraFocalLength", mFocalLength);
	gSavedSettings.setF32("CameraMaxCoF", mMaxCoF);
	gSavedSettings.setF32("CameraFocusTransitionTime", mFocusTrans);
	gSavedSettings.setF32("CameraDoFResScale", mDoFRes);
}

void LLPanelDisplay::apply()
{
	bool can_defer = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred");

	S32 vsync_value = childGetValue("vsync").asInteger();
	bool fbo_value = childGetValue("fbo").asBoolean() || (can_defer && mCtrlDeferred->getValue().asBoolean());
	bool fbo_prior_value = mUseFBO || (can_defer && mDeferred );
	U8 fsaa_value = fbo_value ? 0 : childGetValue("fsaa").asInteger();
	U8 fsaa_prior_value = fbo_prior_value ? 0 : mFSAASamples;
	LLWindow* window = gViewerWindow->getWindow();

	if(vsync_value == -1 && !gGLManager.mHasAdaptiveVsync)
		vsync_value = 0;

	bool apply_fsaa_change = fsaa_value != fsaa_prior_value;

	bool apply_vsync_change = vsync_value != mVsyncMode;

	gSavedSettings.setU32("RenderFSAASamples", childGetValue("fsaa").asInteger());
	gSavedSettings.setS32("SHRenderVsyncMode", vsync_value);

	applyResolution();

	// Only set window size if we're not in fullscreen mode
	if (mCtrlWindowed->get())
	{
		applyWindowSize();
	}

	// Hardware tab
	//Still do a bit of voodoo here. V2 forces restart to change FSAA with FBOs off.
	//Let's not do that, and instead do pre-V2 FSAA change handling for that particular case
	if(apply_fsaa_change || apply_vsync_change)
	{
		bool logged_in = (LLStartUp::getStartupState() >= STATE_STARTED);
		LLCoordScreen size;
		window->getSize(&size);
		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkTextureChannels();
		gViewerWindow->changeDisplaySettings(window->getFullscreen(),
												size,
												vsync_value,
												logged_in);
		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkTextureChannels();
	}
}

void LLPanelDisplay::onChangeQuality(LLUICtrl* ctrl)
{
	LLFeatureManager::getInstance()->setGraphicsLevel(ctrl->getValue(), true);
	refreshEnabledState();
	refresh();
}

void LLPanelDisplay::applyResolution()
{
	gGL.flush();
	char aspect_ratio_text[ASPECT_RATIO_STR_LEN];		/*Flawfinder: ignore*/
	if (mCtrlAspectRatio->getCurrentIndex() == -1)
	{
		// *Can't pass const char* from c_str() into strtok
		strncpy(aspect_ratio_text, mCtrlAspectRatio->getSimple().c_str(), sizeof(aspect_ratio_text) -1);	/*Flawfinder: ignore*/
		aspect_ratio_text[sizeof(aspect_ratio_text) -1] = '\0';
		char *element = strtok(aspect_ratio_text, ":/\\");
		if (!element)
		{
			mAspectRatio = 0.f; // will be clamped later
		}
		else
		{
			LLLocale locale(LLLocale::USER_LOCALE);
			mAspectRatio = (F32)atof(element);
		}

		// look for denominator
		element = strtok(NULL, ":/\\");
		if (element)
		{
			LLLocale locale(LLLocale::USER_LOCALE);

			F32 denominator = (F32)atof(element);
			if (denominator != 0.f)
			{
				mAspectRatio /= denominator;
			}
		}
	}
	else
	{
		mAspectRatio = (F32)mCtrlAspectRatio->getValue().asReal();
	}
	
	// presumably, user entered a non-numeric value if aspect_ratio == 0.f
	if (mAspectRatio != 0.f)
	{
		mAspectRatio = llclamp(mAspectRatio, 0.2f, 5.f);
		gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);
	}
	
	// Screen resolution
	S32 num_resolutions;
	LLWindow::LLWindowResolution* supported_resolutions = 
		gViewerWindow->getWindow()->getSupportedResolutions(num_resolutions);
	U32 resIndex = mCtrlFullScreen->getCurrentIndex();
	gSavedSettings.setS32("FullScreenWidth", supported_resolutions[resIndex].mWidth);
	gSavedSettings.setS32("FullScreenHeight", supported_resolutions[resIndex].mHeight);

	gViewerWindow->requestResolutionUpdate(!mCtrlWindowed->get());

	send_agent_update(TRUE);

	// Update enable/disable
	refresh();
}

// Extract from strings of the form "<width> x <height>", e.g. "640 x 480".
bool LLPanelDisplay::extractWindowSizeFromString(const std::string& instr, U32 &width, U32 &height)
{
	using namespace boost;
	cmatch what;
	const regex expression("([0-9]+) x ([0-9]+)");
	if (regex_match(instr.c_str(), what, expression))
	{
		width = atoi(what[1].first);
		height = atoi(what[2].first);
		return true;
	}
	width = height = 0;
	return false;
}

void LLPanelDisplay::applyWindowSize()
{
	if (mCtrlWindowSize->getVisible() && (mCtrlWindowSize->getCurrentIndex() != -1))
	{
		U32 width = 0;
		U32 height = 0;
		if (extractWindowSizeFromString(mCtrlWindowSize->getValue().asString().c_str(), width,height))
		{
			LLViewerWindow::movieSize(width, height);
		}
	}
}

void LLPanelDisplay::onCommitWindowedMode()
{
	refresh();
}

void LLPanelDisplay::onCommitAutoDetectAspect(const LLSD& value)
{
	BOOL auto_detect = value.asBoolean();
	F32 ratio;

	if (auto_detect)
	{
		S32 numerator = 0;
		S32 denominator = 0;
		
		// clear any aspect ratio override
		gViewerWindow->getWindow()->setNativeAspectRatio(0.f);
		fractionFromDecimal(gViewerWindow->getWindow()->getNativeAspectRatio(), numerator, denominator);

		std::string aspect;
		if (numerator != 0)
		{
			aspect = llformat("%d:%d", numerator, denominator);
		}
		else
		{
			aspect = llformat("%.3f", gViewerWindow->getWindow()->getNativeAspectRatio());
		}

		mCtrlAspectRatio->setLabel(aspect);

		ratio = gViewerWindow->getWindow()->getNativeAspectRatio();
		gSavedSettings.setF32("FullScreenAspectRatio", ratio);
	}
}

//static
void LLPanelDisplay::fractionFromDecimal(F32 decimal_val, S32& numerator, S32& denominator)
{
	numerator = 0;
	denominator = 0;
	for (F32 test_denominator = 1.f; test_denominator < 30.f; test_denominator += 1.f)
	{
		if (fmodf((decimal_val * test_denominator) + 0.01f, 1.f) < 0.02f)
		{
			numerator = ll_round(decimal_val * test_denominator);
			denominator = ll_round(test_denominator);
			break;
		}
	}
}

void LLPanelDisplay::setHardwareDefaults()
{
	LLFeatureManager::getInstance()->applyRecommendedSettings();
	if (LLControlVariable* controlp = gSavedSettings.getControl("RenderAvatarMaxVisible"))
	{
		controlp->resetToDefault(true);
	}
	refreshEnabledState();
}

void updateSliderText(LLSliderCtrl* slider, LLTextBox* text_box)
{
	// get range and points when text should change
	const F32 value = slider->getValue().asFloat();
	const F32 min_val = slider->getMinValue();
	const F32 max_val = slider->getMaxValue();
	const F32 range = (max_val - min_val)/3.f;

	// choose the right text
	if (value < min_val + range)
	{
		//Hack to display 'Off' for avatar physics slider.
		if (!value && slider->getName() == "AvatarPhysicsDetail")
			text_box->setText(std::string("Off"));
		else
			text_box->setText(std::string("Low"));
	} 
	else if (value < min_val + 2.0f * range)
	{
		text_box->setText(std::string("Mid"));
	}
	else if (value < max_val)
	{
		text_box->setText(std::string("High"));
	}
	else
	{
		text_box->setText(std::string("Max"));
	}
}

