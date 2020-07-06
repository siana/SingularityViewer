/** 
 * @file llpanelface.cpp
 * @brief Panel in the tools floater for editing face textures, colors, etc.
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
#include "llpanelface.h"
 
// library includes
#include "llcalc.h"
#include "llerror.h"
#include "llfocusmgr.h"
#include "llrect.h"
#include "llstring.h"
#include "llfontgl.h"

// project includes
#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llinventorymodel.h" //Perms check for texture params
#include "lllineeditor.h"
#include "llmaterialmgr.h"
#include "llmediaentry.h"
#include "llnotificationsutil.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llvovolume.h"
#include "lluictrlfactory.h"
#include "llpluginclassmedia.h"
#include "llviewertexturelist.h"// Update sel manager as to which channel we're editing so it can reflect the correct overlay UI

//
// Constant definitions for comboboxes
// Must match the commbobox definitions in panel_tools_texture.xml
//
const S32 MATMEDIA_MATERIAL = 0;	// Material
const S32 MATMEDIA_MEDIA = 1;		// Media
const S32 MATTYPE_DIFFUSE = 0;		// Diffuse material texture
const S32 MATTYPE_NORMAL = 1;		// Normal map
const S32 MATTYPE_SPECULAR = 2;		// Specular map
const S32 ALPHAMODE_NONE = 0;		// No alpha mask applied
const S32 ALPHAMODE_BLEND = 1;		// Alpha blending mode
const S32 ALPHAMODE_MASK = 2;		// Alpha masking mode
const S32 BUMPY_TEXTURE = 18;		// use supplied normal map
const S32 SHINY_TEXTURE = 4;		// use supplied specular map

//
// "Use texture" label for normal/specular type comboboxes
// Filled in at initialization from translated strings
//
std::string USE_TEXTURE;

LLRender::eTexIndex LLPanelFace::getTextureChannelToEdit()
{
// <alchemy>
	LLRender::eTexIndex channel_to_edit = (mComboMatMedia && mComboMatMedia->getCurrentIndex() == MATMEDIA_MATERIAL) ?
													  (mComboMatType ? (LLRender::eTexIndex)mComboMatType->getCurrentIndex() : LLRender::DIFFUSE_MAP) : LLRender::DIFFUSE_MAP;

	channel_to_edit = (channel_to_edit == LLRender::NORMAL_MAP)		? (getCurrentNormalMap().isNull()		? LLRender::DIFFUSE_MAP : channel_to_edit) : channel_to_edit;
	channel_to_edit = (channel_to_edit == LLRender::SPECULAR_MAP)	? (getCurrentSpecularMap().isNull()		? LLRender::DIFFUSE_MAP : channel_to_edit) : channel_to_edit;
	return channel_to_edit;
// </alchemy>
}

// Things the UI provides...
//
// <alchemy>
LLUUID	LLPanelFace::getCurrentNormalMap()			{ return mBumpyTextureCtrl->getImageAssetID();	}
LLUUID	LLPanelFace::getCurrentSpecularMap()		{ return mShinyTextureCtrl->getImageAssetID();	}
U8			LLPanelFace::getCurrentDiffuseAlphaMode()	{ return (U8)mComboAlpha->getCurrentIndex();	}
// </alchemy>

//
// Methods
//

BOOL	LLPanelFace::postBuild()
{
	// Media button caching
	mMediaInfo = getChildView("media_info");
	mMediaAdd = getChildView("add_media");
	mMediaDelete = getChildView("delete_media");

	// Label caching
	mLabelGlossy = getChildView("label glossiness");
	mLabelEnvironment = getChildView("label environment");
	mLabelShinyColor = getChildView("label shinycolor");
	mLabelAlphaMode = getChildView("label alphamode");
	mLabelMaskCutoff = getChildView("label maskcutoff");
	mLabelBumpy = getChildView("label bumpiness");
	mLabelShiny = getChildView("label shininess");
	mLabelColor = getChildView("color label");
	mLabelGlow = getChildView("glow label");
	mLabelTexGen = getChildView("tex gen");

	// UICtrl Caching
	mComboShiny = getChild<LLComboBox>("combobox shininess");
	mComboBumpy = getChild<LLComboBox>("combobox bumpiness");
	mComboAlpha = getChild<LLComboBox>("combobox alphamode");
	mCtrlTexScaleU = getChild<LLSpinCtrl>("TexScaleU");
	mCtrlFlipTexScaleU = getChild<LLUICtrl>("flipTextureScaleU");
	mCtrlTexScaleV = getChild<LLSpinCtrl>("TexScaleV");
	mCtrlFlipTexScaleV = getChild<LLUICtrl>("flipTextureScaleV");
	mCtrlTexRot = getChild<LLSpinCtrl>("TexRot");
	mCtrlRpt = getChild<LLUICtrl>("rptctrl");
	mCtrlPlanar = getChild<LLUICtrl>("checkbox planar align");
	mCtrlTexOffsetU = getChild<LLSpinCtrl>("TexOffsetU");
	mCtrlTexOffsetV = getChild<LLSpinCtrl>("TexOffsetV");
	mBumpyScaleU = getChild<LLUICtrl>("bumpyScaleU");
	mBumpyScaleV = getChild<LLUICtrl>("bumpyScaleV");
	mBumpyRot = getChild<LLUICtrl>("bumpyRot");
	mBumpyOffsetU = getChild<LLUICtrl>("bumpyOffsetU");
	mBumpyOffsetV = getChild<LLUICtrl>("bumpyOffsetV");
	mShinyScaleU = getChild<LLUICtrl>("shinyScaleU");
	mShinyScaleV = getChild<LLUICtrl>("shinyScaleV");
	mShinyRot = getChild<LLUICtrl>("shinyRot");
	mShinyOffsetU = getChild<LLUICtrl>("shinyOffsetU");
	mShinyOffsetV = getChild<LLUICtrl>("shinyOffsetV");
	mGlossyCtrl = getChild<LLUICtrl>("glossiness");
	mEnvironmentCtrl = getChild<LLUICtrl>("environment");
	mCtrlMaskCutoff = getChild<LLUICtrl>("maskcutoff");
	mCtrlAlign = getChild<LLUICtrl>("button align");
	mCtrlMapsSync = getChild<LLUICtrl>("checkbox maps sync");
	mCtrlCopy = getChild<LLUICtrl>("copytextures");
	mCtrlPaste = getChild<LLUICtrl>("pastetextures");

	mComboShiny->setCommitCallback(boost::bind(&LLPanelFace::sendShiny, this, boost::bind(&LLComboBox::getCurrentIndex, mComboShiny)));
	mComboBumpy->setCommitCallback(boost::bind(&LLPanelFace::sendBump, this, boost::bind(&LLComboBox::getCurrentIndex, mComboBumpy)));
	mComboAlpha->setCommitCallback(boost::bind(&LLPanelFace::onCommitAlphaMode, this));
	mCtrlTexScaleU->setCommitCallback(boost::bind(&LLPanelFace::onCommitTextureInfo, this));
	mCtrlFlipTexScaleU->setCommitCallback(boost::bind(&LLPanelFace::onCommitFlip, this, true));
	mCtrlTexScaleV->setCommitCallback(boost::bind(&LLPanelFace::onCommitTextureInfo, this));
	mCtrlFlipTexScaleV->setCommitCallback(boost::bind(&LLPanelFace::onCommitFlip, this, false));
	mCtrlTexRot->setCommitCallback(boost::bind(&LLPanelFace::onCommitTextureInfo, this));
	mCtrlRpt->setCommitCallback(boost::bind(&LLPanelFace::onCommitRepeatsPerMeter, this, _1));
	mCtrlPlanar->setCommitCallback(boost::bind(&LLPanelFace::onCommitPlanarAlign, this));
	mCtrlTexOffsetU->setCommitCallback(boost::bind(&LLPanelFace::onCommitTextureInfo, this));
	mCtrlTexOffsetV->setCommitCallback(boost::bind(&LLPanelFace::onCommitTextureInfo, this));

	mBumpyScaleU->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialBumpyScaleX, this, _2));
	mBumpyScaleV->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialBumpyScaleY, this, _2));
	mBumpyRot->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialBumpyRot, this, _2));
	mBumpyOffsetU->setCommitCallback(boost::bind(LLSelectedTEMaterial::setNormalOffsetX, this, _2));
	mBumpyOffsetV->setCommitCallback(boost::bind(LLSelectedTEMaterial::setNormalOffsetY, this, _2));
	mShinyScaleU->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialShinyScaleX, this, _2));
	mShinyScaleV->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialShinyScaleY, this, _2));
	mShinyRot->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialShinyRot, this, _2));
	mShinyOffsetU->setCommitCallback(boost::bind(LLSelectedTEMaterial::setSpecularOffsetX, this, _2));
	mShinyOffsetV->setCommitCallback(boost::bind(LLSelectedTEMaterial::setSpecularOffsetY, this, _2));
	mGlossyCtrl->setCommitCallback(boost::bind(LLSelectedTEMaterial::setSpecularLightExponent, this, boost::bind(&LLSD::asInteger, _2)));
	mEnvironmentCtrl->setCommitCallback(boost::bind(LLSelectedTEMaterial::setEnvironmentIntensity, this, boost::bind(&LLSD::asInteger, _2)));
	mCtrlMaskCutoff->setCommitCallback(boost::bind(LLSelectedTEMaterial::setAlphaMaskCutoff, this, boost::bind(&LLSD::asInteger, _2)));

	mCtrlAlign->setCommitCallback(boost::bind(&LLPanelFace::onClickAutoFix,this));

	mCtrlMapsSync->setCommitCallback(boost::bind(&LLPanelFace::onClickMapsSync, this));
	mCtrlCopy->setCommitCallback(boost::bind(&LLPanelFace::onClickCopy,this));
	mCtrlPaste->setCommitCallback(boost::bind(&LLPanelFace::onClickPaste,this));

	setMouseOpaque(FALSE);

	mTextureCtrl = getChild<LLTextureCtrl>("texture control");
	if(mTextureCtrl)
	{
		mTextureCtrl->setDefaultImageAssetID(LLUUID( gSavedSettings.getString( "DefaultObjectTexture" )));
		mTextureCtrl->setCommitCallback( boost::bind(&LLPanelFace::onCommitTexture, this, _2) );
		mTextureCtrl->setOnCancelCallback( boost::bind(&LLPanelFace::onCancelTexture, this, _2) );
		mTextureCtrl->setOnSelectCallback( boost::bind(&LLPanelFace::onSelectTexture, this, _2) );
		mTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mTextureCtrl->setOnCloseCallback( boost::bind(&LLPanelFace::onCloseTexturePicker, this, _2) );

		mTextureCtrl->setFollowsTop();
		mTextureCtrl->setFollowsLeft();
		mTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}

	mShinyTextureCtrl = getChild<LLTextureCtrl>("shinytexture control");
	if(mShinyTextureCtrl)
	{
		mShinyTextureCtrl->setDefaultImageAssetID(LLUUID( gSavedSettings.getString( "DefaultObjectSpecularTexture" )));
		mShinyTextureCtrl->setCommitCallback( boost::bind(&LLPanelFace::onCommitSpecularTexture, this, _2) );
		mShinyTextureCtrl->setOnCancelCallback( boost::bind(&LLPanelFace::onCancelSpecularTexture, this, _2) );
		mShinyTextureCtrl->setOnSelectCallback( boost::bind(&LLPanelFace::onSelectSpecularTexture, this, _2) );
		mShinyTextureCtrl->setOnCloseCallback( boost::bind(&LLPanelFace::onCloseTexturePicker, this, _2) );

		mShinyTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mShinyTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mShinyTextureCtrl->setFollowsTop();
		mShinyTextureCtrl->setFollowsLeft();
		mShinyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mShinyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}

	mBumpyTextureCtrl = getChild<LLTextureCtrl>("bumpytexture control");
	if(mBumpyTextureCtrl)
	{
		mBumpyTextureCtrl->setDefaultImageAssetID(LLUUID( gSavedSettings.getString( "DefaultObjectNormalTexture" )));
		mBumpyTextureCtrl->setBlankImageAssetID(LLUUID( gSavedSettings.getString( "DefaultBlankNormalTexture" )));
		mBumpyTextureCtrl->setCommitCallback( boost::bind(&LLPanelFace::onCommitNormalTexture, this, _2) );
		mBumpyTextureCtrl->setOnCancelCallback( boost::bind(&LLPanelFace::onCancelNormalTexture, this, _2) );
		mBumpyTextureCtrl->setOnSelectCallback( boost::bind(&LLPanelFace::onSelectNormalTexture, this, _2) );
		mBumpyTextureCtrl->setOnCloseCallback( boost::bind(&LLPanelFace::onCloseTexturePicker, this, _2) );

		mBumpyTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mBumpyTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mBumpyTextureCtrl->setFollowsTop();
		mBumpyTextureCtrl->setFollowsLeft();
		mBumpyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mBumpyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}

	mColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");
	if(mColorSwatch)
	{
		mColorSwatch->setCommitCallback(boost::bind(&LLPanelFace::sendColor, this));
		mColorSwatch->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelColor, this, _2));
		mColorSwatch->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectColor, this, _2));
		mColorSwatch->setFollowsTop();
		mColorSwatch->setFollowsLeft();
		mColorSwatch->setCanApplyImmediately(TRUE);
	}

	mShinyColorSwatch = getChild<LLColorSwatchCtrl>("shinycolorswatch");
	if(mShinyColorSwatch)
	{
		mShinyColorSwatch->setCommitCallback(boost::bind(&LLPanelFace::onCommitShinyColor, this, _2));
		mShinyColorSwatch->setFollowsTop();
		mShinyColorSwatch->setFollowsLeft();
		mShinyColorSwatch->setCanApplyImmediately(TRUE);
	}

	mLabelColorTransp = getChild<LLTextBox>("color trans");
	if(mLabelColorTransp)
	{
		mLabelColorTransp->setFollowsTop();
		mLabelColorTransp->setFollowsLeft();
	}

	mCtrlColorTransp = getChild<LLSpinCtrl>("ColorTrans");
	if(mCtrlColorTransp)
	{
		mCtrlColorTransp->setCommitCallback(boost::bind(&LLPanelFace::sendAlpha, this));
		mCtrlColorTransp->setPrecision(0);
		mCtrlColorTransp->setFollowsTop();
		mCtrlColorTransp->setFollowsLeft();
	}

	mCheckFullbright = getChild<LLCheckBoxCtrl>("checkbox fullbright");
	if (mCheckFullbright)
	{
		mCheckFullbright->setCommitCallback(boost::bind(&LLPanelFace::sendFullbright, this));
	}

	mComboTexGen = getChild<LLComboBox>("combobox texgen");
	if(mComboTexGen)
	{
		mComboTexGen->setCommitCallback(boost::bind(&LLPanelFace::sendTexGen, this));
		mComboTexGen->setFollows(FOLLOWS_LEFT | FOLLOWS_TOP);	
	}

	mComboMatMedia = getChild<LLComboBox>("combobox matmedia");
	if(mComboMatMedia)
	{
		mComboMatMedia->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialsMedia,this));
		mComboMatMedia->selectNthItem(MATMEDIA_MATERIAL);
	}

	mComboMatType = getChild<LLComboBox>("combobox mattype");
	if(mComboMatType)
	{
		mComboMatType->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialType, this));
		mComboMatType->selectNthItem(MATTYPE_DIFFUSE);
	}

	mCtrlGlow = getChild<LLSpinCtrl>("glow");
	if(mCtrlGlow)
	{
		mCtrlGlow->setCommitCallback(boost::bind(&LLPanelFace::sendGlow, this));
	}


	clearCtrls();

	return TRUE;
}

LLPanelFace::LLPanelFace(const std::string& name)
:	LLPanel(name),
// <alchemy>
	mMediaInfo(NULL),
	mMediaAdd(NULL),
	mMediaDelete(NULL),
	mLabelGlossy(NULL),
	mLabelEnvironment(NULL),
	mLabelShinyColor(NULL),
	mLabelAlphaMode(NULL),
	mLabelMaskCutoff(NULL),
	mLabelBumpy(NULL),
	mLabelShiny(NULL),
	mLabelColor(NULL),
	mLabelGlow(NULL),
	mLabelTexGen(NULL),
	mComboShiny(NULL),
	mComboBumpy(NULL),
	mComboAlpha(NULL),
	mCtrlTexScaleU(NULL),
	mCtrlFlipTexScaleU(NULL),
	mCtrlTexScaleV(NULL),
	mCtrlFlipTexScaleV(NULL),
	mCtrlTexRot(NULL),
	mCtrlRpt(NULL),
	mCtrlPlanar(NULL),
	mCtrlTexOffsetU(NULL),
	mCtrlTexOffsetV(NULL),
	mBumpyScaleU(NULL),
	mBumpyScaleV(NULL),
	mBumpyRot(NULL),
	mBumpyOffsetU(NULL),
	mBumpyOffsetV(NULL),
	mShinyScaleU(NULL),
	mShinyScaleV(NULL),
	mShinyRot(NULL),
	mShinyOffsetU(NULL),
	mShinyOffsetV(NULL),
	mGlossyCtrl(NULL),
	mEnvironmentCtrl(NULL),
	mCtrlMaskCutoff(NULL),
	mCtrlAlign(NULL),
	mCtrlMapsSync(NULL),
	mCtrlCopy(NULL),
	mCtrlPaste(NULL),
	mTextureCtrl(NULL),
	mShinyTextureCtrl(NULL),
	mBumpyTextureCtrl(NULL),
	mColorSwatch(NULL),
	mShinyColorSwatch(NULL),
	mComboTexGen(NULL),
	mComboMatMedia(NULL),
	mComboMatType(NULL),
	mCheckFullbright(NULL),
	mLabelColorTransp(NULL),
	mCtrlColorTransp(NULL), // transparency = 1 - alpha
	mCtrlGlow(NULL),
// </alchemy>
	mIsAlpha(false)
{
	USE_TEXTURE = LLTrans::getString("use_texture");
}


LLPanelFace::~LLPanelFace()
{
	// Children all cleaned up by default view destructor.
}


void LLPanelFace::sendTexture()
{
	if(!mTextureCtrl) return;
	if( !mTextureCtrl->getTentative() )
	{
		// we grab the item id first, because we want to do a
		// permissions check in the selection manager. ARGH!
		LLUUID id = mTextureCtrl->getImageItemID();
		if(id.isNull())
		{
			id = mTextureCtrl->getImageAssetID();
		}
		LLSelectMgr::getInstance()->selectionSetImage(id);
	}
}

void LLPanelFace::sendBump(U32 bumpiness)
{	
	LLTextureCtrl* bumpytexture_ctrl = mBumpyTextureCtrl;
	if (bumpiness < BUMPY_TEXTURE)
	{
		LL_DEBUGS("Materials") << "clearing bumptexture control" << LL_ENDL;
		bumpytexture_ctrl->clear();
		bumpytexture_ctrl->setImageAssetID(LLUUID());
	}

	updateBumpyControls(bumpiness == BUMPY_TEXTURE, true);

	LLUUID current_normal_map = bumpytexture_ctrl->getImageAssetID();

	U8 bump = (U8) bumpiness & TEM_BUMP_MASK;

	// Clear legacy bump to None when using an actual normal map
	//
	if (!current_normal_map.isNull())
		bump = 0;

	// Set the normal map or reset it to null as appropriate
	//
	LLSelectedTEMaterial::setNormalID(this, current_normal_map);

	LLSelectMgr::getInstance()->selectionSetBumpmap( bump );
}

void LLPanelFace::sendTexGen()
{
	if(!mComboTexGen)return;
	U8 tex_gen = (U8) mComboTexGen->getCurrentIndex() << TEM_TEX_GEN_SHIFT;
	LLSelectMgr::getInstance()->selectionSetTexGen( tex_gen );
}

void LLPanelFace::sendShiny(U32 shininess)
{
	LLTextureCtrl* texture_ctrl = mShinyTextureCtrl;

	if (shininess < SHINY_TEXTURE)
	{
		texture_ctrl->clear();
		texture_ctrl->setImageAssetID(LLUUID());
	}

	LLUUID specmap = getCurrentSpecularMap();

	U8 shiny = (U8) shininess & TEM_SHINY_MASK;
	if (!specmap.isNull())
		shiny = 0;

	LLSelectedTEMaterial::setSpecularID(this, specmap);

	LLSelectMgr::getInstance()->selectionSetShiny( shiny );

	updateShinyControls(!specmap.isNull(), true);

}

void LLPanelFace::sendFullbright()
{
	if(!mCheckFullbright)return;
	U8 fullbright = mCheckFullbright->get() ? TEM_FULLBRIGHT_MASK : 0;
	LLSelectMgr::getInstance()->selectionSetFullbright( fullbright );
}

void LLPanelFace::sendColor()
{
	if(!mColorSwatch)return;
	LLColor4 color = mColorSwatch->get();

	LLSelectMgr::getInstance()->selectionSetColorOnly( color );
}

void LLPanelFace::sendAlpha()
{	
	if(!mCtrlColorTransp)return;
	F32 alpha = (100.f - mCtrlColorTransp->get()) / 100.f;

	LLSelectMgr::getInstance()->selectionSetAlphaOnly( alpha );
}


void LLPanelFace::sendGlow()
{
	llassert(mCtrlGlow);
	if (mCtrlGlow)
	{
		F32 glow = mCtrlGlow->get();
		LLSelectMgr::getInstance()->selectionSetGlow( glow );
	}
}

struct LLPanelFaceSetTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceSetTEFunctor(LLPanelFace* panel) : mPanel(panel) {}
	virtual bool apply(LLViewerObject* object, S32 te)
	{
		BOOL valid;
		F32 value;
		LLSpinCtrl*	ctrlTexScaleS = mPanel->mCtrlTexScaleU;
		LLSpinCtrl*	ctrlTexScaleT = mPanel->mCtrlTexScaleV;
		LLSpinCtrl*	ctrlTexOffsetS = mPanel->mCtrlTexOffsetU;
		LLSpinCtrl*	ctrlTexOffsetT = mPanel->mCtrlTexOffsetV;
		LLSpinCtrl*	ctrlTexRotation = mPanel->mCtrlTexRot;
		LLComboBox*		comboTexGen = mPanel->mComboTexGen;
		llassert(comboTexGen);
		llassert(object);

		if (ctrlTexScaleS)
		{
			valid = !ctrlTexScaleS->getTentative();
			if (valid)
			{
				value = ctrlTexScaleS->get();
				if (comboTexGen &&
				    comboTexGen->getCurrentIndex() == 1)
				{
					value *= 0.5f;
				}
				object->setTEScaleS( te, value );
			}
		}

		if (ctrlTexScaleT)
		{
			valid = !ctrlTexScaleT->getTentative();
			if (valid)
			{
				value = ctrlTexScaleT->get();
				if (comboTexGen &&
				    comboTexGen->getCurrentIndex() == 1)
				{
					value *= 0.5f;
				}
				object->setTEScaleT( te, value );
			}
		}

		if (ctrlTexOffsetS)
		{
			valid = !ctrlTexOffsetS->getTentative();
			if (valid)
			{
				value = ctrlTexOffsetS->get();
				object->setTEOffsetS( te, value );
			}
		}

		if (ctrlTexOffsetT)
		{
			valid = !ctrlTexOffsetT->getTentative();
			if (valid)
			{
				value = ctrlTexOffsetT->get();
				object->setTEOffsetT( te, value );
			}
		}

		if (ctrlTexRotation)
		{
			valid = !ctrlTexRotation->getTentative();
			if (valid)
			{
				value = ctrlTexRotation->get() * DEG_TO_RAD;
				object->setTERotation( te, value );
			}
		}
		return true;
	}
private:
	LLPanelFace* mPanel;
};

// Functor that aligns a face to mCenterFace
struct LLPanelFaceSetAlignedTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceSetAlignedTEFunctor(LLPanelFace* panel, LLFace* center_face) :
		mPanel(panel),
		mCenterFace(center_face) {}

	virtual bool apply(LLViewerObject* object, S32 te)
	{
		LLFace* facep = object->mDrawable->getFace(te);
		if (!facep)
		{
			return true;
		}

		if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{
			return true;
		}

		bool set_aligned = true;
		if (facep == mCenterFace)
		{
			set_aligned = false;
		}
		if (set_aligned)
		{
			LLVector2 uv_offset, uv_scale;
			F32 uv_rot;
			set_aligned = facep->calcAlignedPlanarTE(mCenterFace, &uv_offset, &uv_scale, &uv_rot);
			if (set_aligned)
			{
				object->setTEOffset(te, uv_offset.mV[VX], uv_offset.mV[VY]);
				object->setTEScale(te, uv_scale.mV[VX], uv_scale.mV[VY]);
				object->setTERotation(te, uv_rot);
			}
		}
		if (!set_aligned)
		{
			LLPanelFaceSetTEFunctor setfunc(mPanel);
			setfunc.apply(object, te);
		}
		return true;
	}
private:
	LLPanelFace* mPanel;
	LLFace* mCenterFace;
};

// Functor that tests if a face is aligned to mCenterFace
struct LLPanelFaceGetIsAlignedTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceGetIsAlignedTEFunctor(LLFace* center_face) :
		mCenterFace(center_face) {}

	virtual bool apply(LLViewerObject* object, S32 te)
	{
		LLFace* facep = object->mDrawable->getFace(te);
		if (!facep)
		{
			return false;
		}

		if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{ //volume face does not exist, can't be aligned
			return false;
		}

		if (facep == mCenterFace)
		{
			return true;
		}
		
		LLVector2 aligned_st_offset, aligned_st_scale;
		F32 aligned_st_rot;
		if ( facep->calcAlignedPlanarTE(mCenterFace, &aligned_st_offset, &aligned_st_scale, &aligned_st_rot) )
		{
			const LLTextureEntry* tep = facep->getTextureEntry();
			LLVector2 st_offset, st_scale;
			tep->getOffset(&st_offset.mV[VX], &st_offset.mV[VY]);
			tep->getScale(&st_scale.mV[VX], &st_scale.mV[VY]);
			F32 st_rot = tep->getRotation();
			// needs a fuzzy comparison, because of fp errors
			if (is_approx_equal_fraction(st_offset.mV[VX], aligned_st_offset.mV[VX], 12) &&
				is_approx_equal_fraction(st_offset.mV[VY], aligned_st_offset.mV[VY], 12) &&
				is_approx_equal_fraction(st_scale.mV[VX], aligned_st_scale.mV[VX], 12) &&
				is_approx_equal_fraction(st_scale.mV[VY], aligned_st_scale.mV[VY], 12) &&
				is_approx_equal_fraction(st_rot, aligned_st_rot, 14))
			{
				return true;
			}
		}
		return false;
	}
private:
	LLFace* mCenterFace;
};

struct LLPanelFaceSendFunctor : public LLSelectedObjectFunctor
{
	virtual bool apply(LLViewerObject* object)
	{
		object->sendTEUpdate();
		return true;
	}
};

void LLPanelFace::sendTextureInfo()
{
	if ((bool)mCtrlPlanar->getValue().asBoolean())
	{
		LLFace* last_face = NULL;
		bool identical_face = false;
		LLSelectedTE::getFace(last_face, identical_face);
		LLPanelFaceSetAlignedTEFunctor setfunc(this, last_face);
		LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
	}
	else
	{
		LLPanelFaceSetTEFunctor setfunc(this);
		LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
	}

	LLPanelFaceSendFunctor sendfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
}

void LLPanelFace::getState()
{
	updateUI();
}

void LLPanelFace::updateUI()
{ //set state of UI to match state of texture entry(ies)  (calls setEnabled, setValue, etc, but NOT setVisible)
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();

	if( objectp
		&& objectp->getPCode() == LL_PCODE_VOLUME
		&& objectp->permModify())
	{
		BOOL editable = objectp->permModify() && !objectp->isPermanentEnforced();

		// only turn on auto-adjust button if there is a media renderer and the media is loaded
		mCtrlAlign->setEnabled(editable);

		bool enable_material_controls = (!gSavedSettings.getBOOL("FSSynchronizeTextureMaps"));
		S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
		BOOL single_volume = (LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME ))
						 && (selected_count == 1);
		
		mCtrlCopy->setEnabled(single_volume && editable);
		mCtrlPaste->setEnabled(editable);

		LLComboBox* combobox_matmedia = mComboMatMedia;
		if (combobox_matmedia)
		{
			if (combobox_matmedia->getCurrentIndex() < MATMEDIA_MATERIAL)
			{
				combobox_matmedia->selectNthItem(MATMEDIA_MATERIAL);
			}
		}
		mComboMatMedia->setEnabled(editable);

		LLComboBox* combobox_mattype = mComboMatType;
		if (combobox_mattype)
		{
			if (combobox_mattype->getCurrentIndex() < MATTYPE_DIFFUSE)
			{
				combobox_mattype->selectNthItem(MATTYPE_DIFFUSE);
			}
		}
		mComboMatType->setEnabled(editable);

		updateVisibility();

		bool identical			= true;	// true because it is anded below
		bool identical_diffuse	= false;
		bool identical_norm		= false;
		bool identical_spec		= false;

		LLTextureCtrl*	texture_ctrl = mTextureCtrl;
		texture_ctrl->setFallbackImageName( "" );	//Singu Note: Don't show the 'locked' image when the texid is null.
		LLTextureCtrl*	shinytexture_ctrl = mShinyTextureCtrl;
		shinytexture_ctrl->setFallbackImageName( "" );	//Singu Note: Don't show the 'locked' image when the texid is null.
		LLTextureCtrl*	bumpytexture_ctrl = mBumpyTextureCtrl;
		bumpytexture_ctrl->setFallbackImageName( "" );	//Singu Note: Don't show the 'locked' image when the texid is null.

		LLUUID id;
		LLUUID normmap_id;
		LLUUID specmap_id;

		// Color swatch
		{
			mLabelColor->setEnabled(editable);
		}

		LLColor4 color					= LLColor4::white;
		bool		identical_color	=false;

		if(mColorSwatch)
		{
			LLSelectedTE::getColor(color, identical_color);

			mColorSwatch->setOriginal(color);
			mColorSwatch->set(color, TRUE);

			mColorSwatch->setValid(editable);
			mColorSwatch->setEnabled( editable );
			mColorSwatch->setCanApplyImmediately( editable );
		}

		// Color transparency
		mLabelColorTransp->setEnabled(editable);

		F32 transparency = (1.f - color.mV[VALPHA]) * 100.f;
		mCtrlColorTransp->setValue(editable ? transparency : 0);
		mCtrlColorTransp->setEnabled(editable);

		// Specular map
		LLSelectedTEMaterial::getSpecularID(specmap_id, identical_spec);

		U8 shiny = 0;
		bool identical_shiny = false;

		// Shiny
		LLSelectedTE::getShiny(shiny, identical_shiny);
		identical = identical && identical_shiny;

		shiny = specmap_id.isNull() ? shiny : SHINY_TEXTURE;

		if (mComboShiny)
		{
			mComboShiny->selectNthItem((S32)shiny);
		}

		mLabelShiny->setEnabled(editable);
		mComboShiny->setEnabled(editable);

		mLabelGlossy->setEnabled(editable);
		mGlossyCtrl->setEnabled(editable);

		mLabelEnvironment->setEnabled(editable);
		mEnvironmentCtrl->setEnabled(editable);
		mLabelShinyColor->setEnabled(editable);

		mComboShiny->setTentative(!identical_spec);
		mGlossyCtrl->setTentative(!identical_spec);
		mEnvironmentCtrl->setTentative(!identical_spec);
		mShinyColorSwatch->setTentative(!identical_spec);

		if(mShinyColorSwatch)
		{
			mShinyColorSwatch->setValid(editable);
			mShinyColorSwatch->setEnabled( editable );
			mShinyColorSwatch->setCanApplyImmediately( editable );
		}

		U8 bumpy = 0;
		// Bumpy
		{
			bool identical_bumpy = false;
			LLSelectedTE::getBumpmap(bumpy,identical_bumpy);

			LLUUID norm_map_id = getCurrentNormalMap();
			LLCtrlSelectionInterface* combobox_bumpiness = mComboBumpy;

			bumpy = norm_map_id.isNull() ? bumpy : BUMPY_TEXTURE;

			if (combobox_bumpiness)
			{
				combobox_bumpiness->selectNthItem((S32)bumpy);
			}

			mComboBumpy->setEnabled(editable);
			mComboBumpy->setTentative(!identical_bumpy);
			mLabelBumpy->setEnabled(editable);
		}

		// Texture
		{
			LLSelectedTE::getTexId(id,identical_diffuse);

			// Normal map
			LLSelectedTEMaterial::getNormalID(normmap_id, identical_norm);

			mIsAlpha = FALSE;
			LLGLenum image_format = GL_RGB;
			bool identical_image_format = false;
			LLSelectedTE::getImageFormat(image_format, identical_image_format);

			mIsAlpha = FALSE;
			switch (image_format)
			{
			case GL_RGBA:
			case GL_ALPHA:
				{
					mIsAlpha = TRUE;
				}
				break;

			case GL_RGB: break;
			default:
				{
					LL_WARNS("Materials") << "Unexpected tex format in LLPanelFace...resorting to no alpha" << LL_ENDL;
				}
				break;
			}

			if(LLViewerMedia::textureHasMedia(id))
			{
				mCtrlAlign->setEnabled(editable);
			}

			// Diffuse Alpha Mode

			// Init to the default that is appropriate for the alpha content of the asset
			//
			U8 alpha_mode = mIsAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

			bool identical_alpha_mode = false;

			// See if that's been overridden by a material setting for same...
			//
			LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(alpha_mode, identical_alpha_mode, mIsAlpha);

			LLCtrlSelectionInterface* combobox_alphamode = mComboAlpha;
			if (combobox_alphamode)
			{
				//it is invalid to have any alpha mode other than blend if transparency is greater than zero ...
				// Want masking? Want emissive? Tough! You get BLEND!
				alpha_mode = (transparency > 0.f) ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : alpha_mode;

				// ... unless there is no alpha channel in the texture, in which case alpha mode MUST be none
				alpha_mode = mIsAlpha ? alpha_mode : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

				combobox_alphamode->selectNthItem(alpha_mode);
			}

			updateAlphaControls();

			if (texture_ctrl)
			{
				if (identical_diffuse)
				{
					texture_ctrl->setTentative( FALSE );
					texture_ctrl->setEnabled( editable );
					texture_ctrl->setImageAssetID( id );
					mComboAlpha->setEnabled(editable && mIsAlpha && transparency <= 0.f);
					mLabelAlphaMode->setEnabled(editable && mIsAlpha);
					mCtrlMaskCutoff->setEnabled(editable && mIsAlpha);
					mLabelMaskCutoff->setEnabled(editable && mIsAlpha);
				}
				else if (id.isNull())
				{
					// None selected
					texture_ctrl->setTentative( FALSE );
					texture_ctrl->setEnabled( FALSE );
					texture_ctrl->setImageAssetID( LLUUID::null );
					mComboAlpha->setEnabled( FALSE );
					mLabelAlphaMode->setEnabled( FALSE );
					mCtrlMaskCutoff->setEnabled( FALSE);
					mLabelMaskCutoff->setEnabled( FALSE );
				}
				else
				{
					// Tentative: multiple selected with different textures
					texture_ctrl->setTentative( TRUE );
					texture_ctrl->setEnabled( editable );
					texture_ctrl->setImageAssetID( id );
					mComboAlpha->setEnabled(editable && mIsAlpha && transparency <= 0.f);
					mLabelAlphaMode->setEnabled(editable && mIsAlpha);
					mCtrlMaskCutoff->setEnabled(editable && mIsAlpha);
					mLabelMaskCutoff->setEnabled(editable && mIsAlpha);
				}
			}

			if (shinytexture_ctrl)
			{
				if (identical_spec && (shiny == SHINY_TEXTURE))
				{
					shinytexture_ctrl->setTentative( FALSE );
					shinytexture_ctrl->setEnabled( editable );
					shinytexture_ctrl->setImageAssetID( specmap_id );
				}
				else if (specmap_id.isNull())
				{
					shinytexture_ctrl->setTentative( FALSE );
					shinytexture_ctrl->setEnabled( editable );
					shinytexture_ctrl->setImageAssetID( LLUUID::null );
				}
				else
				{
					shinytexture_ctrl->setTentative( TRUE );
					shinytexture_ctrl->setEnabled( editable );
					shinytexture_ctrl->setImageAssetID( specmap_id );
				}
			}

			if (bumpytexture_ctrl)
			{
				if (identical_norm && (bumpy == BUMPY_TEXTURE))
				{
					bumpytexture_ctrl->setTentative( FALSE );
					bumpytexture_ctrl->setEnabled( editable );
					bumpytexture_ctrl->setImageAssetID( normmap_id );
				}
				else if (normmap_id.isNull())
				{
					bumpytexture_ctrl->setTentative( FALSE );
					bumpytexture_ctrl->setEnabled( editable );
					bumpytexture_ctrl->setImageAssetID( LLUUID::null );
				}
				else
				{
					bumpytexture_ctrl->setTentative( TRUE );
					bumpytexture_ctrl->setEnabled( editable );
					bumpytexture_ctrl->setImageAssetID( normmap_id );
				}
			}
		}

		// planar align
		bool align_planar = false;
		bool identical_planar_aligned = false;
		{
			LLUICtrl* cb_planar_align = mCtrlPlanar;
			align_planar = (cb_planar_align && cb_planar_align->getValue().asBoolean());

			bool enabled = (editable && isIdenticalPlanarTexgen());
			mCtrlPlanar->setValue(align_planar && enabled);
			mCtrlPlanar->setEnabled(enabled);

			if (align_planar && enabled)
			{
				LLFace* last_face = NULL;
				bool identical_face = false;
				LLSelectedTE::getFace(last_face, identical_face);

				LLPanelFaceGetIsAlignedTEFunctor get_is_aligend_func(last_face);
				// this will determine if the texture param controls are tentative:
				identical_planar_aligned = LLSelectMgr::getInstance()->getSelection()->applyToTEs(&get_is_aligend_func);
			}
		}
		
		// Needs to be public and before tex scale settings below to properly reflect
		// behavior when in planar vs default texgen modes in the
		// NORSPEC-84 et al
		//
		LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
		bool identical_texgen = true;
		bool identical_planar_texgen = false;

		{
			LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
			identical_planar_texgen = (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
		}

		// Texture scale
		{
			bool identical_diff_scale_s = false;
			bool identical_spec_scale_s = false;
			bool identical_norm_scale_s = false;

			identical = align_planar ? identical_planar_aligned : identical;

			F32 diff_scale_s = 1.f;
			F32 spec_scale_s = 1.f;
			F32 norm_scale_s = 1.f;

			LLSelectedTE::getScaleS(diff_scale_s, identical_diff_scale_s);
			LLSelectedTEMaterial::getSpecularRepeatX(spec_scale_s, identical_spec_scale_s);
			LLSelectedTEMaterial::getNormalRepeatX(norm_scale_s, identical_norm_scale_s);

			diff_scale_s = editable ? diff_scale_s : 1.0f;
			diff_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

			norm_scale_s = editable ? norm_scale_s : 1.0f;
			norm_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

			spec_scale_s = editable ? spec_scale_s : 1.0f;
			spec_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

			mCtrlTexScaleU->setValue(diff_scale_s);
			mShinyScaleU->setValue(spec_scale_s);
			mBumpyScaleU->setValue(norm_scale_s);

			mCtrlTexScaleU->setEnabled(editable);
			mShinyScaleU->setEnabled(editable && specmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment
			mBumpyScaleU->setEnabled(editable && normmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment

			BOOL diff_scale_tentative = !(identical && identical_diff_scale_s);
			BOOL norm_scale_tentative = !(identical && identical_norm_scale_s);
			BOOL spec_scale_tentative = !(identical && identical_spec_scale_s);

			mCtrlTexScaleU->setTentative(  LLSD(diff_scale_tentative));
			mShinyScaleU->setTentative(LLSD(spec_scale_tentative));
			mBumpyScaleU->setTentative(LLSD(norm_scale_tentative));

			// <FS:CR> FIRE-11407 - Materials alignment
			mCtrlMapsSync->setEnabled(editable && (specmap_id.notNull() || normmap_id.notNull()));
			// </FS:CR>
		}

		{
			bool identical_diff_scale_t = false;
			bool identical_spec_scale_t = false;
			bool identical_norm_scale_t = false;

			F32 diff_scale_t = 1.f;
			F32 spec_scale_t = 1.f;
			F32 norm_scale_t = 1.f;

			LLSelectedTE::getScaleT(diff_scale_t, identical_diff_scale_t);
			LLSelectedTEMaterial::getSpecularRepeatY(spec_scale_t, identical_spec_scale_t);
			LLSelectedTEMaterial::getNormalRepeatY(norm_scale_t, identical_norm_scale_t);

			diff_scale_t = editable ? diff_scale_t : 1.0f;
			diff_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			norm_scale_t = editable ? norm_scale_t : 1.0f;
			norm_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			spec_scale_t = editable ? spec_scale_t : 1.0f;
			spec_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			BOOL diff_scale_tentative = !identical_diff_scale_t;
			BOOL norm_scale_tentative = !identical_norm_scale_t;
			BOOL spec_scale_tentative = !identical_spec_scale_t;

			mCtrlTexScaleV->setEnabled(editable);
			mShinyScaleV->setEnabled(editable && specmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment
			mBumpyScaleV->setEnabled(editable && normmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment

			mCtrlTexScaleV->setValue(diff_scale_t);
			mShinyScaleV->setValue(norm_scale_t);
			mBumpyScaleV->setValue(spec_scale_t);

			mCtrlTexScaleV->setTentative(LLSD(diff_scale_tentative));
			mShinyScaleV->setTentative(LLSD(norm_scale_tentative));
			mBumpyScaleV->setTentative(LLSD(spec_scale_tentative));
		}

		// Texture offset
		{
			bool identical_diff_offset_s = false;
			bool identical_norm_offset_s = false;
			bool identical_spec_offset_s = false;

			F32 diff_offset_s = 0.0f;
			F32 norm_offset_s = 0.0f;
			F32 spec_offset_s = 0.0f;

			LLSelectedTE::getOffsetS(diff_offset_s, identical_diff_offset_s);
			LLSelectedTEMaterial::getNormalOffsetX(norm_offset_s, identical_norm_offset_s);
			LLSelectedTEMaterial::getSpecularOffsetX(spec_offset_s, identical_spec_offset_s);

			BOOL diff_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_s);
			BOOL norm_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_s);
			BOOL spec_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_s);

			mCtrlTexOffsetU->setValue(  editable ? diff_offset_s : 0.0f);
			mBumpyOffsetU->setValue(editable ? norm_offset_s : 0.0f);
			mShinyOffsetU->setValue(editable ? spec_offset_s : 0.0f);

			mCtrlTexOffsetU->setTentative(LLSD(diff_offset_u_tentative));
			mShinyOffsetU->setTentative(LLSD(norm_offset_u_tentative));
			mBumpyOffsetU->setTentative(LLSD(spec_offset_u_tentative));

			mCtrlTexOffsetU->setEnabled(editable);
			mShinyOffsetU->setEnabled(editable && specmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
			mBumpyOffsetU->setEnabled(editable && normmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
		}

		{
			bool identical_diff_offset_t = false;
			bool identical_norm_offset_t = false;
			bool identical_spec_offset_t = false;

			F32 diff_offset_t = 0.0f;
			F32 norm_offset_t = 0.0f;
			F32 spec_offset_t = 0.0f;

			LLSelectedTE::getOffsetT(diff_offset_t, identical_diff_offset_t);
			LLSelectedTEMaterial::getNormalOffsetY(norm_offset_t, identical_norm_offset_t);
			LLSelectedTEMaterial::getSpecularOffsetY(spec_offset_t, identical_spec_offset_t);

			BOOL diff_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_t);
			BOOL norm_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_t);
			BOOL spec_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_t);

			mCtrlTexOffsetV->setValue(  editable ? diff_offset_t : 0.0f);
			mBumpyOffsetV->setValue(editable ? norm_offset_t : 0.0f);
			mShinyOffsetV->setValue(editable ? spec_offset_t : 0.0f);

			mCtrlTexOffsetV->setTentative(LLSD(diff_offset_v_tentative));
			mShinyOffsetV->setTentative(LLSD(norm_offset_v_tentative));
			mBumpyOffsetV->setTentative(LLSD(spec_offset_v_tentative));

			mCtrlTexOffsetV->setEnabled(editable);
			mShinyOffsetV->setEnabled(editable && specmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
			mBumpyOffsetV->setEnabled(editable && normmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
		}

		// Texture rotation
		{
			bool identical_diff_rotation = false;
			bool identical_norm_rotation = false;
			bool identical_spec_rotation = false;

			F32 diff_rotation = 0.f;
			F32 norm_rotation = 0.f;
			F32 spec_rotation = 0.f;

			LLSelectedTE::getRotation(diff_rotation,identical_diff_rotation);
			LLSelectedTEMaterial::getSpecularRotation(spec_rotation,identical_spec_rotation);
			LLSelectedTEMaterial::getNormalRotation(norm_rotation,identical_norm_rotation);

			BOOL diff_rot_tentative = !(align_planar ? identical_planar_aligned : identical_diff_rotation);
			BOOL norm_rot_tentative = !(align_planar ? identical_planar_aligned : identical_norm_rotation);
			BOOL spec_rot_tentative = !(align_planar ? identical_planar_aligned : identical_spec_rotation);

			F32 diff_rot_deg = diff_rotation * RAD_TO_DEG;
			F32 norm_rot_deg = norm_rotation * RAD_TO_DEG;
			F32 spec_rot_deg = spec_rotation * RAD_TO_DEG;

			mCtrlTexRot->setEnabled(editable);
			mShinyRot->setEnabled(editable && specmap_id.notNull()
									  && enable_material_controls); // <FS:CR> Materials alignment
			mBumpyRot->setEnabled(editable && normmap_id.notNull()
									  && enable_material_controls); // <FS:CR> Materials alignment

			mCtrlTexRot->setTentative(diff_rot_tentative);
			mShinyRot->setTentative(LLSD(norm_rot_tentative));
			mBumpyRot->setTentative(LLSD(spec_rot_tentative));

			mCtrlTexRot->setValue(  editable ? diff_rot_deg : 0.0f);
			mShinyRot->setValue(editable ? spec_rot_deg : 0.0f);
			mBumpyRot->setValue(editable ? norm_rot_deg : 0.0f);
		}

		{
			F32 glow = 0.f;
			bool identical_glow = false;
			LLSelectedTE::getGlow(glow,identical_glow);
			mCtrlGlow->setValue(glow);
			mCtrlGlow->setTentative(!identical_glow);
			mCtrlGlow->setEnabled(editable);
			mLabelGlow->setEnabled(editable);
		}

		{
			LLCtrlSelectionInterface* combobox_texgen = mComboTexGen;
			if (combobox_texgen)
			{
				// Maps from enum to combobox entry index
				combobox_texgen->selectNthItem(((S32)selected_texgen) >> 1);
			}

			mComboTexGen->setEnabled(editable);
			mComboTexGen->setTentative(!identical);
			mLabelTexGen->setEnabled(editable);

			/* Singu Note: Dead code
			if (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR)
			{
				// EXP-1507 (change label based on the mapping mode)
				getChild<LLUICtrl>("rpt")->setValue(getString("string repeats per meter"));
			}
			else
			if (selected_texgen == LLTextureEntry::TEX_GEN_DEFAULT)
			{
				getChild<LLUICtrl>("rpt")->setValue(getString("string repeats per face"));
			}
			*/
		}

		{
			U8 fullbright_flag = 0;
			bool identical_fullbright = false;

			LLSelectedTE::getFullbright(fullbright_flag,identical_fullbright);

			mCheckFullbright->setValue(fullbright_flag != 0);
			mCheckFullbright->setEnabled(editable);
			mCheckFullbright->setTentative(!identical_fullbright);
		}


		// Repeats per meter
		{
			F32 repeats_diff = 1.f;
			F32 repeats_norm = 1.f;
			F32 repeats_spec = 1.f;

			bool identical_diff_repeats = false;
			bool identical_norm_repeats = false;
			bool identical_spec_repeats = false;

			LLSelectedTE::getMaxDiffuseRepeats(repeats_diff, identical_diff_repeats);
			LLSelectedTEMaterial::getMaxNormalRepeats(repeats_norm, identical_norm_repeats);
			LLSelectedTEMaterial::getMaxSpecularRepeats(repeats_spec, identical_spec_repeats);

			if (mComboTexGen)
			{
				S32 index = mComboTexGen ? mComboTexGen->getCurrentIndex() : 0;
				BOOL enabled = editable && (index != 1);
				BOOL identical_repeats = true;
				F32  repeats = 1.0f;

				U32 material_type = (combobox_matmedia->getCurrentIndex() == MATMEDIA_MATERIAL) ? combobox_mattype->getCurrentIndex() : MATTYPE_DIFFUSE;

				LLSelectMgr::getInstance()->setTextureChannel(LLRender::eTexIndex(material_type));

				switch (material_type)
				{
					default:
					case MATTYPE_DIFFUSE:
					{
						enabled = editable && !id.isNull();
						identical_repeats = identical_diff_repeats;
						repeats = repeats_diff;
					}
					break;

					case MATTYPE_SPECULAR:
					{
						enabled = (editable && ((shiny == SHINY_TEXTURE) && !specmap_id.isNull())
								   && enable_material_controls);	// <FS:CR> Materials Alignment
						identical_repeats = identical_spec_repeats;
						repeats = repeats_spec;
					}
					break;

					case MATTYPE_NORMAL:
					{
						enabled = (editable && ((bumpy == BUMPY_TEXTURE) && !normmap_id.isNull())
								   && enable_material_controls); // <FS:CR> Materials Alignment
						identical_repeats = identical_norm_repeats;
						repeats = repeats_norm;
					}
					break;
				}

				BOOL repeats_tentative = !identical_repeats;

				mCtrlRpt->setEnabled(identical_planar_texgen ? FALSE : enabled);
				mCtrlRpt->setValue(editable ? repeats : 1.0f);
				mCtrlRpt->setTentative(LLSD(repeats_tentative));

				// <FS:CR> FIRE-11407 - Flip buttons
				mCtrlFlipTexScaleU->setEnabled(enabled);
				mCtrlFlipTexScaleV->setEnabled(enabled);
				// </FS:CR>
			}
		}

		// Materials
		{
			LLMaterialPtr material;
			LLSelectedTEMaterial::getCurrent(material, identical);

			if (material && editable)
			{
				LL_DEBUGS("Materials: OnMaterialsLoaded:") << material->asLLSD() << LL_ENDL;

				// Alpha
				LLCtrlSelectionInterface* combobox_alphamode =
					mComboAlpha;
				if (combobox_alphamode)
				{
					U32 alpha_mode = material->getDiffuseAlphaMode();

					if (transparency > 0.f)
					{ //it is invalid to have any alpha mode other than blend if transparency is greater than zero ...
						alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
					}

					if (!mIsAlpha)
					{ // ... unless there is no alpha channel in the texture, in which case alpha mode MUST ebe none
						alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
					}

					combobox_alphamode->selectNthItem(alpha_mode);
				}
				mCtrlMaskCutoff->setValue(material->getAlphaMaskCutoff());
				updateAlphaControls();

				identical_planar_texgen = isIdenticalPlanarTexgen();

				// Shiny (specular)
				F32 offset_x, offset_y, repeat_x, repeat_y, rot;
				LLTextureCtrl* texture_ctrl = mShinyTextureCtrl;
				texture_ctrl->setImageAssetID(material->getSpecularID());

				if (!material->getSpecularID().isNull() && (shiny == SHINY_TEXTURE))
				{
					material->getSpecularOffset(offset_x,offset_y);
					material->getSpecularRepeat(repeat_x,repeat_y);

					if (identical_planar_texgen)
					{
						repeat_x *= 2.0f;
						repeat_y *= 2.0f;
					}

					rot = material->getSpecularRotation();
					mShinyScaleU->setValue(repeat_x);
					mShinyScaleV->setValue(repeat_y);
					mShinyRot->setValue(rot*RAD_TO_DEG);
					mShinyOffsetU->setValue(offset_x);
					mShinyOffsetV->setValue(offset_y);
					mGlossyCtrl->setValue(material->getSpecularLightExponent());
					mEnvironmentCtrl->setValue(material->getEnvironmentIntensity());

					updateShinyControls(!material->getSpecularID().isNull(), true);
				}

				// Assert desired colorswatch color to match material AFTER updateShinyControls
				// to avoid getting overwritten with the default on some UI state changes.
				//
				if (!material->getSpecularID().isNull())
				{
					mShinyColorSwatch->setOriginal(material->getSpecularLightColor());
					mShinyColorSwatch->set(material->getSpecularLightColor(),TRUE);
				}

				// Bumpy (normal)
				texture_ctrl = mBumpyTextureCtrl;
				texture_ctrl->setImageAssetID(material->getNormalID());

				if (!material->getNormalID().isNull())
				{
					material->getNormalOffset(offset_x,offset_y);
					material->getNormalRepeat(repeat_x,repeat_y);

					if (identical_planar_texgen)
					{
						repeat_x *= 2.0f;
						repeat_y *= 2.0f;
					}

					rot = material->getNormalRotation();
					mBumpyScaleU->setValue(repeat_x);
					mBumpyScaleV->setValue(repeat_y);
					mBumpyRot->setValue(rot*RAD_TO_DEG);
					mBumpyOffsetU->setValue(offset_x);
					mBumpyOffsetV->setValue(offset_y);

					updateBumpyControls(!material->getNormalID().isNull(), true);
				}
			}
		}

		// Set variable values for numeric expressions
		LLCalc* calcp = LLCalc::getInstance();
		calcp->setVar(LLCalc::TEX_U_SCALE, mCtrlTexScaleU->getValue().asReal());
		calcp->setVar(LLCalc::TEX_V_SCALE, mCtrlTexScaleV->getValue().asReal());
		calcp->setVar(LLCalc::TEX_U_OFFSET, mCtrlTexOffsetU->getValue().asReal());
		calcp->setVar(LLCalc::TEX_V_OFFSET, mCtrlTexOffsetV->getValue().asReal());
		calcp->setVar(LLCalc::TEX_ROTATION, mCtrlTexRot->getValue().asReal());
		calcp->setVar(LLCalc::TEX_TRANSPARENCY, mCtrlColorTransp->getValue().asReal());
		calcp->setVar(LLCalc::TEX_GLOW, mCtrlGlow->getValue().asReal());
	}
	else
	{
		// Disable all UICtrls
		clearCtrls();

		// Disable non-UICtrls
		LLTextureCtrl*	texture_ctrl = mTextureCtrl;
		if(texture_ctrl)
		{
			texture_ctrl->setImageAssetID( LLUUID::null );
			texture_ctrl->setEnabled( FALSE );  // this is a LLUICtrl, but we don't want it to have keyboard focus so we add it as a child, not a ctrl.
// 			texture_ctrl->setValid(FALSE);
		}
		if(mColorSwatch)
		{
			mColorSwatch->setEnabled( FALSE );			
			mColorSwatch->setFallbackImageName("locked_image.j2c" );
			mColorSwatch->setValid(FALSE);
		}
		mLabelColorTransp->setEnabled(FALSE);
		/* Singu Note: This is missing in xml
		getChildView("rpt")->setEnabled(FALSE);
		getChildView("tex offset")->setEnabled(FALSE);
		*/
		mLabelTexGen->setEnabled(FALSE);
		mLabelShiny->setEnabled(FALSE);
		mLabelBumpy->setEnabled(FALSE);
		mCtrlAlign->setEnabled(FALSE);
		//getChildView("has media")->setEnabled(FALSE);
		//getChildView("media info set")->setEnabled(FALSE);

		updateVisibility();

		// Set variable values for numeric expressions
		LLCalc* calcp = LLCalc::getInstance();
		calcp->clearVar(LLCalc::TEX_U_SCALE);
		calcp->clearVar(LLCalc::TEX_V_SCALE);
		calcp->clearVar(LLCalc::TEX_U_OFFSET);
		calcp->clearVar(LLCalc::TEX_V_OFFSET);
		calcp->clearVar(LLCalc::TEX_ROTATION);
		calcp->clearVar(LLCalc::TEX_TRANSPARENCY);
		calcp->clearVar(LLCalc::TEX_GLOW);
	}
}


void LLPanelFace::refresh()
{
	LL_DEBUGS("Materials") << LL_ENDL;
	getState();
}

//
// Static functions
//

// static
F32 LLPanelFace::valueGlow(LLViewerObject* object, S32 face)
{
	return (F32)(object->getTE(face)->getGlow());
}


void LLPanelFace::onCommitShinyColor(const LLSD& data)
{
	LLSelectedTEMaterial::setSpecularLightColor(this, mShinyColorSwatch->get());
}

void LLPanelFace::onCancelColor(const LLSD& data)
{
	LLSelectMgr::getInstance()->selectionRevertColors();
}

void LLPanelFace::onSelectColor(const LLSD& data)
{
	LLSelectMgr::getInstance()->saveSelectedObjectColors();
	sendColor();
}

void LLPanelFace::onCommitMaterialsMedia()
{
	// Force to default states to side-step problems with menu contents
	// and generally reflecting old state when switching tabs or objects
	//
	updateShinyControls(false,true);
	updateBumpyControls(false,true);
	updateUI();
}

// static
void LLPanelFace::updateVisibility()
{
	LLComboBox* combo_matmedia = mComboMatMedia;
	LLComboBox* combo_mattype = mComboMatType;
	LLComboBox* combo_shininess = mComboShiny;
	LLComboBox* combo_bumpiness = mComboBumpy;
	if (!combo_mattype || !combo_matmedia || !combo_shininess || !combo_bumpiness)
	{
		LL_WARNS("Materials") << "Combo box not found...exiting." << LL_ENDL;
		return;
	}
	U32 materials_media = combo_matmedia->getCurrentIndex();
	U32 material_type = combo_mattype->getCurrentIndex();
	bool show_media = (materials_media == MATMEDIA_MEDIA) && combo_matmedia->getEnabled();
	bool show_texture = (show_media || ((material_type == MATTYPE_DIFFUSE) && combo_matmedia->getEnabled()));
	bool show_bumpiness = (!show_media) && (material_type == MATTYPE_NORMAL) && combo_matmedia->getEnabled();
	bool show_shininess = (!show_media) && (material_type == MATTYPE_SPECULAR) && combo_matmedia->getEnabled();
	mComboMatType->setVisible(!show_media);
	// <FS:CR> FIRE-11407 - Be consistant and hide this with the other controls
	//mCtrlRpt->setVisible(true);
	mCtrlRpt->setVisible(combo_matmedia->getEnabled());
	// and other additions...
	mCtrlFlipTexScaleU->setVisible(combo_matmedia->getEnabled());
	mCtrlFlipTexScaleV->setVisible(combo_matmedia->getEnabled());
	// </FS:CR>

	// Media controls
	mMediaInfo->setVisible(show_media);
	mMediaAdd->setVisible(show_media);
	mMediaDelete->setVisible(show_media);
	mCtrlAlign->setVisible(show_media);

	// Diffuse texture controls
	mTextureCtrl->setVisible(show_texture && !show_media);
	mLabelAlphaMode->setVisible(show_texture && !show_media);
	mComboAlpha->setVisible(show_texture && !show_media);
	mLabelMaskCutoff->setVisible(false);
	mCtrlMaskCutoff->setVisible(false);
	if (show_texture && !show_media)
	{
		updateAlphaControls();
	}
	mCtrlTexScaleU->setVisible(show_texture);
	mCtrlTexScaleV->setVisible(show_texture);
	mCtrlTexRot->setVisible(show_texture);
	mCtrlTexOffsetU->setVisible(show_texture);
	mCtrlTexOffsetV->setVisible(show_texture);

	// Specular map controls
	mShinyTextureCtrl->setVisible(show_shininess);
	mComboShiny->setVisible(show_shininess);
	mLabelShiny->setVisible(show_shininess);
	mLabelGlossy->setVisible(false);
	mGlossyCtrl->setVisible(false);
	mLabelEnvironment->setVisible(false);
	mEnvironmentCtrl->setVisible(false);
	mLabelShinyColor->setVisible(false);
	mShinyColorSwatch->setVisible(false);
	if (show_shininess)
	{
		updateShinyControls();
	}
	mShinyScaleU->setVisible(show_shininess);
	mShinyScaleV->setVisible(show_shininess);
	mShinyRot->setVisible(show_shininess);
	mShinyOffsetU->setVisible(show_shininess);
	mShinyOffsetV->setVisible(show_shininess);

	// Normal map controls
	if (show_bumpiness)
	{
		updateBumpyControls();
	}
	mBumpyTextureCtrl->setVisible(show_bumpiness);
	mComboBumpy->setVisible(show_bumpiness);
	mLabelBumpy->setVisible(show_bumpiness);
	mBumpyScaleU->setVisible(show_bumpiness);
	mBumpyScaleV->setVisible(show_bumpiness);
	mBumpyRot->setVisible(show_bumpiness);
	mBumpyOffsetU->setVisible(show_bumpiness);
	mBumpyOffsetV->setVisible(show_bumpiness);
}

void LLPanelFace::onCommitMaterialType()
{
	// Force to default states to side-step problems with menu contents
	// and generally reflecting old state when switching tabs or objects
	//
	updateShinyControls(false,true);
	updateBumpyControls(false,true);
	updateUI();
}

void LLPanelFace::updateShinyControls(bool is_setting_texture, bool mess_with_shiny_combobox)
{
	LLTextureCtrl* texture_ctrl = mShinyTextureCtrl;
	LLUUID shiny_texture_ID = texture_ctrl->getImageAssetID();
	LL_DEBUGS("Materials") << "Shiny texture selected: " << shiny_texture_ID << LL_ENDL;
	LLComboBox* comboShiny = mComboShiny;

	if(mess_with_shiny_combobox)
	{
		if (!comboShiny)
		{
			return;
		}
		if (!shiny_texture_ID.isNull() && is_setting_texture)
		{
			if (!comboShiny->itemExists(USE_TEXTURE))
			{
				comboShiny->add(USE_TEXTURE);
			}
			comboShiny->setSimple(USE_TEXTURE);
		}
		else
		{
			if (comboShiny->itemExists(USE_TEXTURE))
			{
				comboShiny->remove(SHINY_TEXTURE);
				comboShiny->selectFirstItem();
			}
		}
	}

	LLComboBox* combo_matmedia = mComboMatMedia;
	LLComboBox* combo_mattype =mComboMatType;
	U32 materials_media = combo_matmedia->getCurrentIndex();
	U32 material_type = combo_mattype->getCurrentIndex();
	bool show_media = (materials_media == MATMEDIA_MEDIA) && combo_matmedia->getEnabled();
	bool show_shininess = (!show_media) && (material_type == MATTYPE_SPECULAR) && combo_matmedia->getEnabled();
	U32 shiny_value = comboShiny->getCurrentIndex();
	bool show_shinyctrls = (shiny_value == SHINY_TEXTURE) && show_shininess; // Use texture
	mLabelGlossy->setVisible(show_shinyctrls);
	mGlossyCtrl->setVisible(show_shinyctrls);
	mLabelEnvironment->setVisible(show_shinyctrls);
	mEnvironmentCtrl->setVisible(show_shinyctrls);
	mLabelShinyColor->setVisible(show_shinyctrls);
	mShinyColorSwatch->setVisible(show_shinyctrls);
}

void LLPanelFace::updateBumpyControls(bool is_setting_texture, bool mess_with_combobox)
{
	LLTextureCtrl* texture_ctrl = mBumpyTextureCtrl;
	LLUUID bumpy_texture_ID = texture_ctrl->getImageAssetID();
	LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;
	LLComboBox* comboBumpy = mComboBumpy;
	if (!comboBumpy)
	{
		return;
	}

	if (mess_with_combobox)
	{
		LLTextureCtrl* texture_ctrl = mBumpyTextureCtrl;
		LLUUID bumpy_texture_ID = texture_ctrl->getImageAssetID();
		LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;

		if (!bumpy_texture_ID.isNull() && is_setting_texture)
		{
			if (!comboBumpy->itemExists(USE_TEXTURE))
			{
				comboBumpy->add(USE_TEXTURE);
			}
			comboBumpy->setSimple(USE_TEXTURE);
		}
		else
		{
			if (comboBumpy->itemExists(USE_TEXTURE))
			{
				comboBumpy->remove(BUMPY_TEXTURE);
				comboBumpy->selectFirstItem();
			}
		}
	}
}

void LLPanelFace::updateAlphaControls()
{
	LLComboBox* comboAlphaMode = mComboAlpha;
	if (!comboAlphaMode)
	{
		return;
	}
	U32 alpha_value = comboAlphaMode->getCurrentIndex();
	bool show_alphactrls = (alpha_value == ALPHAMODE_MASK); // Alpha masking

	LLComboBox* combobox_matmedia = mComboMatMedia;
	U32 mat_media = MATMEDIA_MATERIAL;
	if (combobox_matmedia)
	{
		mat_media = combobox_matmedia->getCurrentIndex();
	}

	LLComboBox* combobox_mattype = mComboMatType;
	U32 mat_type = MATTYPE_DIFFUSE;
	if (combobox_mattype)
	{
		mat_type = combobox_mattype->getCurrentIndex();
	}

	show_alphactrls = show_alphactrls && (mat_media == MATMEDIA_MATERIAL);
	show_alphactrls = show_alphactrls && (mat_type == MATTYPE_DIFFUSE);

	mLabelMaskCutoff->setVisible(show_alphactrls);
	mCtrlMaskCutoff->setVisible(show_alphactrls);
}

void LLPanelFace::onCommitAlphaMode()
{
	updateAlphaControls();
	LLSelectedTEMaterial::setDiffuseAlphaMode(this, getCurrentDiffuseAlphaMode());
}

// static
BOOL LLPanelFace::onDragTexture(LLUICtrl*, LLInventoryItem* item)
{
	BOOL accept = TRUE;
	for (LLObjectSelection::root_iterator iter = LLSelectMgr::getInstance()->getSelection()->root_begin();
		 iter != LLSelectMgr::getInstance()->getSelection()->root_end(); iter++)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* obj = node->getObject();
		if(!LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
		{
			accept = FALSE;
			break;
		}
	}
	return accept;
}

void LLPanelFace::onCommitTexture( const LLSD& data )
{
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_EDIT_TEXTURE_COUNT );
	sendTexture();
}

void LLPanelFace::onCancelTexture(const LLSD& data)
{
	LLSelectMgr::getInstance()->selectionRevertTextures();
}

void LLPanelFace::onSelectTexture(const LLSD& data)
{
	LLSelectMgr::getInstance()->saveSelectedObjectTextures();
	sendTexture();

	LLGLenum image_format(0);
	bool identical_image_format = false;
	LLSelectedTE::getImageFormat(image_format, identical_image_format);

	LLCtrlSelectionInterface* combobox_alphamode =
		mComboAlpha;

	U32 alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
	if (combobox_alphamode)
	{
		switch (image_format)
		{
		case GL_RGBA:
		case GL_ALPHA:
			{
				alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
			}
			break;

		case GL_RGB: break;
		default:
			{
				LL_WARNS() << "Unexpected tex format in LLPanelFace...resorting to no alpha" << LL_ENDL;
			}
			break;
		}

		combobox_alphamode->selectNthItem(alpha_mode);
	}
	LLSelectedTEMaterial::setDiffuseAlphaMode(this, getCurrentDiffuseAlphaMode());
}

void LLPanelFace::onCloseTexturePicker(const LLSD& data)
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	updateUI();
}

void LLPanelFace::onCommitSpecularTexture( const LLSD& data )
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	sendShiny(SHINY_TEXTURE);
}

void LLPanelFace::onCommitNormalTexture( const LLSD& data )
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	LLUUID nmap_id = getCurrentNormalMap();
	sendBump(nmap_id.isNull() ? 0 : BUMPY_TEXTURE);
}

void LLPanelFace::onCancelSpecularTexture(const LLSD& data)
{
	U8 shiny = 0;
	bool identical_shiny = false;
	LLSelectedTE::getShiny(shiny, identical_shiny);
	LLUUID spec_map_id = mShinyTextureCtrl->getImageAssetID();
	shiny = spec_map_id.isNull() ? shiny : SHINY_TEXTURE;
	sendShiny(shiny);
}

void LLPanelFace::onCancelNormalTexture(const LLSD& data)
{
	U8 bumpy = 0;
	bool identical_bumpy = false;
	LLSelectedTE::getBumpmap(bumpy, identical_bumpy);
	LLUUID norm_map_id = mBumpyTextureCtrl->getImageAssetID();
	bumpy = norm_map_id.isNull() ? bumpy : BUMPY_TEXTURE;
	sendBump(bumpy);
}

void LLPanelFace::onSelectSpecularTexture(const LLSD& data)
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	sendShiny(SHINY_TEXTURE);
}

void LLPanelFace::onSelectNormalTexture(const LLSD& data)
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	LLUUID nmap_id = getCurrentNormalMap();
	sendBump(nmap_id.isNull() ? 0 : BUMPY_TEXTURE);
}

void LLPanelFace::onCommitMaterialBumpyScaleX(const LLSD& value)
{
	F32 bumpy_scale_u = value.asReal();
	if (isIdenticalPlanarTexgen())
	{
		bumpy_scale_u *= 0.5f;
	}
	LLSelectedTEMaterial::setNormalRepeatX(this, bumpy_scale_u);
}

void LLPanelFace::onCommitMaterialBumpyScaleY(const LLSD& value)
{
	F32 bumpy_scale_v = value.asReal();
	if (isIdenticalPlanarTexgen())
	{
		bumpy_scale_v *= 0.5f;
	}
	LLSelectedTEMaterial::setNormalRepeatY(this, bumpy_scale_v);
}

void LLPanelFace::onCommitMaterialShinyScaleX(const LLSD& value)
{
	F32 shiny_scale_u = value.asReal();
	if (isIdenticalPlanarTexgen())
	{
		shiny_scale_u *= 0.5f;
	}
	LLSelectedTEMaterial::setSpecularRepeatX(this, shiny_scale_u);
}

void LLPanelFace::onCommitMaterialShinyScaleY(const LLSD& value)
{
	F32 shiny_scale_v = value.asReal();
	if (isIdenticalPlanarTexgen())
	{
		shiny_scale_v *= 0.5f;
	}
	LLSelectedTEMaterial::setSpecularRepeatY(this, shiny_scale_v);
}

void LLPanelFace::onCommitMaterialBumpyRot(const LLSD& value)
{
	LLSelectedTEMaterial::setNormalRotation(this, value.asReal() * DEG_TO_RAD);
}

void LLPanelFace::onCommitMaterialShinyRot(const LLSD& value)
{
	LLSelectedTEMaterial::setSpecularRotation(this, value.asReal() * DEG_TO_RAD);
}

void LLPanelFace::onCommitTextureInfo()
{
	sendTextureInfo();
	// <FS:CR> Materials alignment
	if (gSavedSettings.getBOOL("FSSynchronizeTextureMaps"))
	{
		alignMaterialsProperties();
	}
	// </FS:CR>
}

// Commit the number of repeats per meter
void LLPanelFace::onCommitRepeatsPerMeter(LLUICtrl* repeats_ctrl)
{
	LLComboBox* combo_matmedia = mComboMatMedia;
	LLComboBox* combo_mattype = mComboMatType;

	U32 materials_media = combo_matmedia->getCurrentIndex();


	U32 material_type	= (materials_media == MATMEDIA_MATERIAL) ? combo_mattype->getCurrentIndex() : 0;
	F32 repeats_per_meter	= repeats_ctrl->getValue().asReal();

	F32 obj_scale_s = 1.0f;
	F32 obj_scale_t = 1.0f;

	bool identical_scale_s = false;
	bool identical_scale_t = false;

	LLSelectedTE::getObjectScaleS(obj_scale_s, identical_scale_s);
	LLSelectedTE::getObjectScaleS(obj_scale_t, identical_scale_t);

	switch (material_type)
	{
		case MATTYPE_DIFFUSE:
		{
			LLSelectMgr::getInstance()->selectionTexScaleAutofit( repeats_per_meter );
		}
		break;

		case MATTYPE_NORMAL:
		{
			LLUICtrl* bumpy_scale_u = mBumpyScaleU;
			LLUICtrl* bumpy_scale_v = mBumpyScaleV;

			bumpy_scale_u->setValue(obj_scale_s * repeats_per_meter);
			bumpy_scale_v->setValue(obj_scale_t * repeats_per_meter);

			LLSelectedTEMaterial::setNormalRepeatX(this,obj_scale_s * repeats_per_meter);
			LLSelectedTEMaterial::setNormalRepeatY(this,obj_scale_t * repeats_per_meter);
		}
		break;

		case MATTYPE_SPECULAR:
		{
			LLUICtrl* shiny_scale_u = mShinyScaleU;
			LLUICtrl* shiny_scale_v = mShinyScaleV;

			shiny_scale_u->setValue(obj_scale_s * repeats_per_meter);
			shiny_scale_v->setValue(obj_scale_t * repeats_per_meter);

			LLSelectedTEMaterial::setSpecularRepeatX(this,obj_scale_s * repeats_per_meter);
			LLSelectedTEMaterial::setSpecularRepeatY(this,obj_scale_t * repeats_per_meter);
		}
		break;

		default:
			llassert(false);
		break;
	}
}

struct LLPanelFaceSetMediaFunctor : public LLSelectedTEFunctor
{
	virtual bool apply(LLViewerObject* object, S32 te)
	{
		viewer_media_t pMediaImpl;
				
		const LLTextureEntry* tep = object->getTE(te);
		const LLMediaEntry* mep = tep->hasMedia() ? tep->getMediaData() : NULL;
		if ( mep )
		{
			pMediaImpl = LLViewerMedia::getMediaImplFromTextureID(mep->getMediaID());
		}
		
		if ( pMediaImpl.isNull())
		{
			// If we didn't find face media for this face, check whether this face is showing parcel media.
			pMediaImpl = LLViewerMedia::getMediaImplFromTextureID(tep->getID());
		}
		
		if ( pMediaImpl.notNull())
		{
			LLPluginClassMedia *media = pMediaImpl->getMediaPlugin();
			if(media)
			{
				S32 media_width = media->getWidth();
				S32 media_height = media->getHeight();
				S32 texture_width = media->getTextureWidth();
				S32 texture_height = media->getTextureHeight();
				F32 scale_s = (F32)media_width / (F32)texture_width;
				F32 scale_t = (F32)media_height / (F32)texture_height;

				// set scale and adjust offset
				object->setTEScaleS( te, scale_s );
				object->setTEScaleT( te, scale_t );	// don't need to flip Y anymore since QT does this for us now.
				object->setTEOffsetS( te, -( 1.0f - scale_s ) / 2.0f );
				object->setTEOffsetT( te, -( 1.0f - scale_t ) / 2.0f );
			}
		}
		return true;
	};
};

void LLPanelFace::onClickAutoFix()
{
	LLPanelFaceSetMediaFunctor setfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);

	LLPanelFaceSendFunctor sendfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
}



// TODO: I don't know who put these in or what these are for???
void LLPanelFace::setMediaURL(const std::string& url)
{
}
void LLPanelFace::setMediaType(const std::string& mime_type)
{
}

void LLPanelFace::onCommitPlanarAlign()
{
	getState();
	sendTextureInfo();
}

void LLPanelFace::onTextureSelectionChanged(LLInventoryItem* itemp)
{
	LL_DEBUGS("Materials") << "item asset " << itemp->getAssetUUID() << LL_ENDL;
	LLComboBox* combo_mattype = mComboMatType;
	if (!combo_mattype)
	{
		return;
	}
	U32 mattype = combo_mattype->getCurrentIndex();
	LLTextureCtrl* texture_ctrl = mTextureCtrl;
	switch (mattype)
	{
		case MATTYPE_SPECULAR:
			texture_ctrl = mShinyTextureCtrl;
			break;
		case MATTYPE_NORMAL:
			texture_ctrl = mBumpyTextureCtrl;
			break;
		// no default needed
	}
	if (texture_ctrl)
	{
		LLUUID obj_owner_id;
		std::string obj_owner_name;
		LLSelectMgr::instance().selectGetOwner(obj_owner_id, obj_owner_name);

		LLSaleInfo sale_info;
		LLSelectMgr::instance().selectGetSaleInfo(sale_info);

		bool can_copy = itemp->getPermissions().allowCopyBy(gAgentID); // do we have perm to copy this texture?
		bool can_transfer = itemp->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID); // do we have perm to transfer this texture?
		bool is_object_owner = gAgentID == obj_owner_id; // does object for which we are going to apply texture belong to the agent?
		bool not_for_sale = !sale_info.isForSale(); // is object for which we are going to apply texture not for sale?

		if (can_copy && can_transfer)
		{
			texture_ctrl->setCanApply(true, true);
			return;
		}

		// if texture has (no-transfer) attribute it can be applied only for object which we own and is not for sale
		texture_ctrl->setCanApply(false, can_transfer ? true : is_object_owner && not_for_sale);

		if (gSavedSettings.getBOOL("ApplyTextureImmediately"))
		{
			LLNotificationsUtil::add("LivePreviewUnavailable");
		}
	}
}

bool LLPanelFace::isIdenticalPlanarTexgen()
{
	LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
	bool identical_texgen = false;
	LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
	return (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
}

void LLPanelFace::LLSelectedTE::getFace(LLFace*& face_to_return, bool& identical_face)
{
	struct LLSelectedTEGetFace : public LLSelectedTEGetFunctor<LLFace *>
	{
		LLFace* get(LLViewerObject* object, S32 te)
		{
			return (object->mDrawable) ? object->mDrawable->getFace(te): NULL;
		}
	} get_te_face_func;
	identical_face = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&get_te_face_func, face_to_return);
}

void LLPanelFace::LLSelectedTE::getImageFormat(LLGLenum& image_format_to_return, bool& identical_face)
{
	LLGLenum image_format(0);
	struct LLSelectedTEGetImageFormat : public LLSelectedTEGetFunctor<LLGLenum>
	{
		LLGLenum get(LLViewerObject* object, S32 te_index)
		{
			LLViewerTexture* image = object->getTEImage(te_index);
			return image ? image->getPrimaryFormat() : GL_RGB;
		}
	} get_glenum;
	identical_face = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&get_glenum, image_format);
	image_format_to_return = image_format;
}

void LLPanelFace::LLSelectedTE::getTexId(LLUUID& id, bool& identical)
{
	struct LLSelectedTEGetTexId : public LLSelectedTEGetFunctor<LLUUID>
	{
		LLUUID get(LLViewerObject* object, S32 te_index)
		{
			LLTextureEntry *te = object->getTE(te_index);
			if (te)
			{
				if ((te->getID() == IMG_USE_BAKED_EYES) || (te->getID() == IMG_USE_BAKED_HAIR) || (te->getID() == IMG_USE_BAKED_HEAD) || (te->getID() == IMG_USE_BAKED_LOWER) || (te->getID() == IMG_USE_BAKED_SKIRT) || (te->getID() == IMG_USE_BAKED_UPPER)
					|| (te->getID() == IMG_USE_BAKED_LEFTARM) || (te->getID() == IMG_USE_BAKED_LEFTLEG) || (te->getID() == IMG_USE_BAKED_AUX1) || (te->getID() == IMG_USE_BAKED_AUX2) || (te->getID() == IMG_USE_BAKED_AUX3))
				{
					return te->getID();
				}
			}
			LLUUID id;
			LLViewerTexture* image = object->getTEImage(te_index);
			if (image)
			{
				id = image->getID();
			}

			if (!id.isNull() && LLViewerMedia::textureHasMedia(id))
			{
				if (te)
				{
					LLViewerTexture* tex = te->getID().notNull() ? gTextureList.findImage(te->getID(), TEX_LIST_STANDARD) : NULL;
					if(!tex)
					{
						tex = LLViewerFetchedTexture::sDefaultImagep;
					}
					if (tex)
					{
						id = tex->getID();
					}
				}
			}
			return id;
		}
	} func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, id );
}

void LLPanelFace::LLSelectedTEMaterial::getCurrent(LLMaterialPtr& material_ptr, bool& identical_material)
{
	struct MaterialFunctor : public LLSelectedTEGetFunctor<LLMaterialPtr>
	{
		LLMaterialPtr get(LLViewerObject* object, S32 te_index)
		{
			return object->getTE(te_index)->getMaterialParams();
		}
	} func;
	identical_material = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, material_ptr);
}

void LLPanelFace::LLSelectedTEMaterial::getMaxSpecularRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxSpecRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			LLMaterial* mat = object->getTE(face)->getMaterialParams().get();
			U32 s_axis = VX;
			U32 t_axis = VY;
			F32 repeats_s = 1.0f;
			F32 repeats_t = 1.0f;
			if (mat)
			{
				mat->getSpecularRepeat(repeats_s, repeats_t);
				repeats_s /= object->getScale().mV[s_axis];
				repeats_t /= object->getScale().mV[t_axis];
			}
			return llmax(repeats_s, repeats_t);
		}

	} max_spec_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_spec_repeats_func, repeats);
}

void LLPanelFace::LLSelectedTEMaterial::getMaxNormalRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxNormRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			LLMaterial* mat = object->getTE(face)->getMaterialParams().get();
			U32 s_axis = VX;
			U32 t_axis = VY;
			F32 repeats_s = 1.0f;
			F32 repeats_t = 1.0f;
			if (mat)
			{
				mat->getNormalRepeat(repeats_s, repeats_t);
				repeats_s /= object->getScale().mV[s_axis];
				repeats_t /= object->getScale().mV[t_axis];
			}
			return llmax(repeats_s, repeats_t);
		}

	} max_norm_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_norm_repeats_func, repeats);
}

void LLPanelFace::LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(U8& diffuse_alpha_mode, bool& identical, bool diffuse_texture_has_alpha)
{
	struct LLSelectedTEGetDiffuseAlphaMode : public LLSelectedTEGetFunctor<U8>
	{
		LLSelectedTEGetDiffuseAlphaMode() : _isAlpha(false) {}
		LLSelectedTEGetDiffuseAlphaMode(bool diffuse_texture_has_alpha) : _isAlpha(diffuse_texture_has_alpha) {}
		virtual ~LLSelectedTEGetDiffuseAlphaMode() {}

		U8 get(LLViewerObject* object, S32 face)
		{
			U8 diffuse_mode = _isAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

			LLTextureEntry* tep = object->getTE(face);
			if (tep)
			{
				LLMaterial* mat = tep->getMaterialParams().get();
				if (mat)
				{
					diffuse_mode = mat->getDiffuseAlphaMode();
				}
			}

			return diffuse_mode;
		}
		bool _isAlpha; // whether or not the diffuse texture selected contains alpha information
	} get_diff_mode(diffuse_texture_has_alpha);
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &get_diff_mode, diffuse_alpha_mode);
}

void LLPanelFace::LLSelectedTE::getObjectScaleS(F32& scale_s, bool& identical)
{
	struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[s_axis];
		}

	} scale_s_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_s_func, scale_s );
}

void LLPanelFace::LLSelectedTE::getObjectScaleT(F32& scale_t, bool& identical)
{
	struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[t_axis];
		}

	} scale_t_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_t_func, scale_t );
}

void LLPanelFace::LLSelectedTE::getMaxDiffuseRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxDiffuseRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			F32 repeats_s = object->getTE(face)->mScaleS / object->getScale().mV[s_axis];
			F32 repeats_t = object->getTE(face)->mScaleT / object->getScale().mV[t_axis];
			return llmax(repeats_s, repeats_t);
		}

	} max_diff_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_diff_repeats_func, repeats );
}

static LLSD textures;

void LLPanelFace::onClickCopy()
{
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
	if(!objectp)
	{
		objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
		if (!objectp)
		{
			return;
		}
	}
	S32 te_count = objectp->getNumFaces();
	textures.clear();
	for (S32 i = 0; i < te_count; i++)
	{
		LLSD tex_params = objectp->getTE(i)->asLLSD();
		LLInventoryItem* itemp = gInventory.getItem(objectp->getTE(i)->getID());
		LLUUID tex = tex_params["imageid"];
		tex_params["imageid"] = LLUUID::null;
		//gSavedPerAccountSettings.setLLSD("Image.Settings", tex_params);
		if (itemp)
		{
			LLPermissions perms = itemp->getPermissions();
			//full perms
			if (perms.getMaskOwner() & PERM_ITEM_UNRESTRICTED)
			{
				tex_params["imageid"] = tex;
			}
		}
		LL_INFOS() << "Copying params on face " << i << "." << LL_ENDL;
		textures.append(tex_params);
	}
}

void LLPanelFace::onClickPaste()
{
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
	if(!objectp)
	{
		objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
		if (!objectp)
		{
			return;
		}
	}
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ObjectImage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
	msg->addStringFast(_PREHASH_MediaURL, NULL);
	
	LLPrimitive obj;
	obj.setNumTEs(U8(textures.size()));
	
	for (int i = 0; i < textures.size() && i < objectp->getNumTEs(); i++)
	{
		LL_INFOS() << "Pasting params on face " << i << "." << LL_ENDL;
		LLSD cur_tex = objectp->getTE(i)->asLLSD();
		if (textures[i]["imageid"].asUUID() == LLUUID::null)
			textures[i]["imageid"] = cur_tex["imageid"];
		LLTextureEntry tex;
		tex.fromLLSD(textures[i]);
		obj.setTE(U8(i), tex);
	}
	
	obj.packTEMessage(gMessageSystem);
	
	msg->sendReliable(gAgent.getRegion()->getHost());
}

// <FS:CR> Materials alignment
void LLPanelFace::onClickMapsSync()
{
	getState();
	if (gSavedSettings.getBOOL("FSSynchronizeTextureMaps"))
	{
		alignMaterialsProperties();
	}
}

void LLPanelFace::alignMaterialsProperties()
{
	F32 tex_scale_u =	mCtrlTexScaleU->getValue().asReal();
	F32 tex_scale_v =	mCtrlTexScaleV->getValue().asReal();
	F32 tex_offset_u =	mCtrlTexOffsetU->getValue().asReal();
	F32 tex_offset_v =	mCtrlTexOffsetV->getValue().asReal();
	F32 tex_rot =		mCtrlTexRot->getValue().asReal();

	mShinyScaleU->setValue(tex_scale_u);
	mShinyScaleV->setValue(tex_scale_v);
	mShinyOffsetU->setValue(tex_offset_u);
	mShinyOffsetV->setValue(tex_offset_v);
	mShinyRot->setValue(tex_rot);

	LLSelectedTEMaterial::setSpecularRepeatX(this, tex_scale_u);
	LLSelectedTEMaterial::setSpecularRepeatY(this, tex_scale_v);
	LLSelectedTEMaterial::setSpecularOffsetX(this, tex_offset_u);
	LLSelectedTEMaterial::setSpecularOffsetY(this, tex_offset_v);
	LLSelectedTEMaterial::setSpecularRotation(this, tex_rot * DEG_TO_RAD);

	mBumpyScaleU->setValue(tex_scale_u);
	mBumpyScaleV->setValue(tex_scale_v);
	mBumpyOffsetU->setValue(tex_offset_u);
	mBumpyOffsetV->setValue(tex_offset_v);
	mBumpyRot->setValue(tex_rot);

	LLSelectedTEMaterial::setNormalRepeatX(this, tex_scale_u);
	LLSelectedTEMaterial::setNormalRepeatY(this, tex_scale_v);
	LLSelectedTEMaterial::setNormalOffsetX(this, tex_offset_u);
	LLSelectedTEMaterial::setNormalOffsetY(this, tex_offset_v);
	LLSelectedTEMaterial::setNormalRotation(this, tex_rot * DEG_TO_RAD);
}

// <FS:CR> FIRE-11407 - Flip buttons
void LLPanelFace::onCommitFlip(bool flip_x)
{
	S32 mattype(mComboMatType->getCurrentIndex());
	LLUICtrl* spinner = NULL;
	switch (mattype)
	{
		case MATTYPE_DIFFUSE:
			spinner = flip_x ? mCtrlTexScaleU : mCtrlTexScaleV;
			break;
		case MATTYPE_NORMAL:
			spinner = flip_x ? mBumpyScaleU : mBumpyScaleV;
			break;
		case MATTYPE_SPECULAR:
			spinner = flip_x ? mShinyScaleU : mShinyScaleV;
			break;
		default:
			//llassert(mattype);
			return;
	}

	if (spinner)
	{
		F32 value = -(spinner->getValue().asReal());
		spinner->setValue(value);

		switch (mattype)
		{
			case MATTYPE_DIFFUSE:
				sendTextureInfo();
				if (gSavedSettings.getBOOL("FSSyncronizeTextureMaps"))
				{
					alignMaterialsProperties();
				}
				break;
			case MATTYPE_NORMAL:
				if (flip_x)
					LLSelectedTEMaterial::setNormalRepeatX(this, value);
				else
					LLSelectedTEMaterial::setNormalRepeatY(this, value);
				break;
			case MATTYPE_SPECULAR:
				if (flip_x)
					LLSelectedTEMaterial::setSpecularRepeatX(this, value);
				else
					LLSelectedTEMaterial::setSpecularRepeatY(this, value);
				break;
			default:
				//llassert(mattype);
				return;
		}
	}
}
// </FS:CR>
