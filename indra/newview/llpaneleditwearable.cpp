/** 
 * @file llpaneleditwearable.cpp
 * @brief UI panel for editing of a particular wearable item.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "lfsimfeaturehandler.h"
#include "llpaneleditwearable.h"
#include "lllocaltextureobject.h"
#include "llpanel.h"
#include "llviewerwearable.h"
#include "lluictrl.h"
#include "llscrollingpanellist.h"
#include "llvisualparam.h"
#include "lltoolmorph.h"
#include "llviewerjointmesh.h"
#include "lltrans.h"
#include "llbutton.h"
#include "llsliderctrl.h"
#include "llagent.h"
#include "llvoavatarself.h"
#include "lltexteditor.h"
#include "lltextbox.h"
#include "llagentwearables.h"
#include "llscrollingpanelparam.h"
#include "llradiogroup.h"
#include "llnotificationsutil.h"
#include "llnotifications.h"

#include "llcolorswatch.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "llviewercontrol.h"    // gSavedSettings
#include "llviewertexturelist.h"
#include "llagentcamera.h"
#include "llmorphview.h"

#include "llcommandhandler.h"
#include "llappearancemgr.h"
#include "llinventoryfunctions.h"
#include "llfloatercustomize.h"

#include "llwearablelist.h"
#include "llinventoryicon.h"

#ifndef BOOST_FUNCTION_HPP_INCLUDED
#include <boost/function.hpp>
#define BOOST_FUNCTION_HPP_INCLUDED
#endif

// subparts of the UI for focus, camera position, etc.
enum ESubpart
{
	SUBPART_SHAPE_HEAD = 1, // avoid 0
	SUBPART_SHAPE_EYES,
	SUBPART_SHAPE_EARS,
	SUBPART_SHAPE_NOSE,
	SUBPART_SHAPE_MOUTH,
	SUBPART_SHAPE_CHIN,
	SUBPART_SHAPE_TORSO,
	SUBPART_SHAPE_LEGS,
	SUBPART_SHAPE_WHOLE,
	SUBPART_SHAPE_DETAIL,
	SUBPART_SKIN_COLOR,
	SUBPART_SKIN_FACEDETAIL,
	SUBPART_SKIN_MAKEUP,
	SUBPART_SKIN_BODYDETAIL,
	SUBPART_HAIR_COLOR,
	SUBPART_HAIR_STYLE,
	SUBPART_HAIR_EYEBROWS,
	SUBPART_HAIR_FACIAL,
	SUBPART_EYES,
	SUBPART_SHIRT,
	SUBPART_PANTS,
	SUBPART_SHOES,
	SUBPART_SOCKS,
	SUBPART_JACKET,
	SUBPART_GLOVES,
	SUBPART_UNDERSHIRT,
	SUBPART_UNDERPANTS,
	SUBPART_SKIRT,
	SUBPART_ALPHA,
	SUBPART_TATTOO,
	SUBPART_UNIVERSAL,
	SUBPART_PHYSICS_BREASTS_UPDOWN,
	SUBPART_PHYSICS_BREASTS_INOUT,
	SUBPART_PHYSICS_BREASTS_LEFTRIGHT,
	SUBPART_PHYSICS_BELLY_UPDOWN,
	SUBPART_PHYSICS_BUTT_UPDOWN,
	SUBPART_PHYSICS_BUTT_LEFTRIGHT,
	SUBPART_PHYSICS_ADVANCED,
};

using namespace LLAvatarAppearanceDefines;

typedef std::vector<ESubpart> subpart_vec_t;

// Locally defined classes

class LLEditWearableDictionary : public LLSingleton<LLEditWearableDictionary>
{
	//--------------------------------------------------------------------
	// Constructors and Destructors
	//--------------------------------------------------------------------
public:
	LLEditWearableDictionary();
	virtual ~LLEditWearableDictionary();

	//--------------------------------------------------------------------
	// Wearable Types
	//--------------------------------------------------------------------
public:
	struct WearableEntry : public LLDictionaryEntry
	{
		WearableEntry(LLWearableType::EType type,
				const std::string &title,
				U8 num_color_swatches,  // number of 'color_swatches'
				U8 num_texture_pickers, // number of 'texture_pickers'
				unsigned int num_subparts, ... ); // number of subparts followed by a list of ETextureIndex and ESubparts


		const LLWearableType::EType mWearableType;
		subpart_vec_t           mSubparts;
		texture_vec_t           mColorSwatchCtrls;
		texture_vec_t           mTextureCtrls;
	};

	struct Wearables : public LLDictionary<LLWearableType::EType, WearableEntry>
	{
		Wearables();
	} mWearables;

	const WearableEntry* getWearable(LLWearableType::EType type) const { return mWearables.lookup(type); }

	//--------------------------------------------------------------------
	// Subparts
	//--------------------------------------------------------------------
public:
	struct SubpartEntry : public LLDictionaryEntry
	{
		SubpartEntry(ESubpart part,
				const std::string &joint,
				const std::string &edit_group,
				const std::string &button_name,
				const LLVector3d  &target_offset,
				const LLVector3d  &camera_offset,
				const ESex        &sex);


		ESubpart			mSubpart;
		std::string			mTargetJoint;
		std::string			mEditGroup;
		std::string			mButtonName;
		LLVector3d			mTargetOffset;
		LLVector3d			mCameraOffset;
		ESex				mSex;
	};

	struct Subparts : public LLDictionary<ESubpart, SubpartEntry>
	{
		Subparts();
	} mSubparts;

	const SubpartEntry* getSubpart(ESubpart subpart) const { return mSubparts.lookup(subpart); }

	//--------------------------------------------------------------------
	// Picker Control Entries
	//--------------------------------------------------------------------
public:
	struct PickerControlEntry : public LLDictionaryEntry
	{
		PickerControlEntry(ETextureIndex tex_index,
				const std::string name,
				const LLUUID default_image_id = LLUUID::null,
				const bool allow_no_texture = false
				);
		ETextureIndex           mTextureIndex;
		const std::string       mControlName;
		const LLUUID            mDefaultImageId;
		const bool                      mAllowNoTexture;
	};

	struct ColorSwatchCtrls : public LLDictionary<ETextureIndex, PickerControlEntry>
	{
		ColorSwatchCtrls();
	} mColorSwatchCtrls;

	struct TextureCtrls : public LLDictionary<ETextureIndex, PickerControlEntry>
	{
		TextureCtrls();
	} mTextureCtrls;

	const PickerControlEntry* getTexturePicker(ETextureIndex index) const { return mTextureCtrls.lookup(index); }
	const PickerControlEntry* getColorSwatch(ETextureIndex index) const { return mColorSwatchCtrls.lookup(index); }
};

LLEditWearableDictionary::LLEditWearableDictionary()
{

}

//virtual 
LLEditWearableDictionary::~LLEditWearableDictionary()
{
}

LLEditWearableDictionary::Wearables::Wearables()
{
	// note the subpart that is listed first is treated as "default", regardless of what order is in enum.
	// Please match the order presented in XUI. -Nyx
	// this will affect what camera angle is shown when first editing a wearable
	addEntry(LLWearableType::WT_SHAPE,              new WearableEntry(LLWearableType::WT_SHAPE,"edit_shape_title",0,0,9,  SUBPART_SHAPE_WHOLE, SUBPART_SHAPE_HEAD,        SUBPART_SHAPE_EYES,     SUBPART_SHAPE_EARS,     SUBPART_SHAPE_NOSE,     SUBPART_SHAPE_MOUTH, SUBPART_SHAPE_CHIN, SUBPART_SHAPE_TORSO, SUBPART_SHAPE_LEGS));
	addEntry(LLWearableType::WT_SKIN,               new WearableEntry(LLWearableType::WT_SKIN,"edit_skin_title",0,3,4, TEX_HEAD_BODYPAINT, TEX_UPPER_BODYPAINT, TEX_LOWER_BODYPAINT, SUBPART_SKIN_COLOR, SUBPART_SKIN_FACEDETAIL, SUBPART_SKIN_MAKEUP, SUBPART_SKIN_BODYDETAIL));
	addEntry(LLWearableType::WT_HAIR,               new WearableEntry(LLWearableType::WT_HAIR,"edit_hair_title",0,1,4, TEX_HAIR, SUBPART_HAIR_COLOR,       SUBPART_HAIR_STYLE,     SUBPART_HAIR_EYEBROWS, SUBPART_HAIR_FACIAL));
	addEntry(LLWearableType::WT_EYES,               new WearableEntry(LLWearableType::WT_EYES,"edit_eyes_title",0,1,1, TEX_EYES_IRIS, SUBPART_EYES));
	addEntry(LLWearableType::WT_SHIRT,              new WearableEntry(LLWearableType::WT_SHIRT,"edit_shirt_title",1,1,1, TEX_UPPER_SHIRT, TEX_UPPER_SHIRT, SUBPART_SHIRT));
	addEntry(LLWearableType::WT_PANTS,              new WearableEntry(LLWearableType::WT_PANTS,"edit_pants_title",1,1,1, TEX_LOWER_PANTS, TEX_LOWER_PANTS, SUBPART_PANTS));
	addEntry(LLWearableType::WT_SHOES,              new WearableEntry(LLWearableType::WT_SHOES,"edit_shoes_title",1,1,1, TEX_LOWER_SHOES, TEX_LOWER_SHOES, SUBPART_SHOES));
	addEntry(LLWearableType::WT_SOCKS,              new WearableEntry(LLWearableType::WT_SOCKS,"edit_socks_title",1,1,1, TEX_LOWER_SOCKS, TEX_LOWER_SOCKS, SUBPART_SOCKS));
	addEntry(LLWearableType::WT_JACKET,     new WearableEntry(LLWearableType::WT_JACKET,"edit_jacket_title",1,2,1, TEX_UPPER_JACKET, TEX_UPPER_JACKET, TEX_LOWER_JACKET, SUBPART_JACKET));
	addEntry(LLWearableType::WT_GLOVES,     new WearableEntry(LLWearableType::WT_GLOVES,"edit_gloves_title",1,1,1, TEX_UPPER_GLOVES, TEX_UPPER_GLOVES, SUBPART_GLOVES));
	addEntry(LLWearableType::WT_UNDERSHIRT, new WearableEntry(LLWearableType::WT_UNDERSHIRT,"edit_undershirt_title",1,1,1, TEX_UPPER_UNDERSHIRT, TEX_UPPER_UNDERSHIRT, SUBPART_UNDERSHIRT));
	addEntry(LLWearableType::WT_UNDERPANTS, new WearableEntry(LLWearableType::WT_UNDERPANTS,"edit_underpants_title",1,1,1, TEX_LOWER_UNDERPANTS, TEX_LOWER_UNDERPANTS, SUBPART_UNDERPANTS));
	addEntry(LLWearableType::WT_SKIRT,              new WearableEntry(LLWearableType::WT_SKIRT,"edit_skirt_title",1,1,1, TEX_SKIRT, TEX_SKIRT, SUBPART_SKIRT));
	addEntry(LLWearableType::WT_ALPHA,              new WearableEntry(LLWearableType::WT_ALPHA,"edit_alpha_title",0,5,1, TEX_LOWER_ALPHA, TEX_UPPER_ALPHA, TEX_HEAD_ALPHA, TEX_EYES_ALPHA, TEX_HAIR_ALPHA, SUBPART_ALPHA));
	addEntry(LLWearableType::WT_TATTOO,     new WearableEntry(LLWearableType::WT_TATTOO,"edit_tattoo_title",1,3,1, TEX_HEAD_TATTOO, TEX_LOWER_TATTOO, TEX_UPPER_TATTOO, TEX_HEAD_TATTOO, SUBPART_TATTOO));
	addEntry(LLWearableType::WT_UNIVERSAL, new WearableEntry(LLWearableType::WT_UNIVERSAL, "edit_universal_title", 1, 11, 1, TEX_HEAD_UNIVERSAL_TATTOO, TEX_HEAD_UNIVERSAL_TATTOO, TEX_UPPER_UNIVERSAL_TATTOO, TEX_LOWER_UNIVERSAL_TATTOO, TEX_SKIRT_TATTOO, TEX_HAIR_TATTOO, TEX_EYES_TATTOO, TEX_LEFT_ARM_TATTOO, TEX_LEFT_LEG_TATTOO, TEX_AUX1_TATTOO, TEX_AUX2_TATTOO, TEX_AUX3_TATTOO, SUBPART_UNIVERSAL));
	addEntry(LLWearableType::WT_PHYSICS,    new WearableEntry(LLWearableType::WT_PHYSICS,"edit_physics_title",0,0,7, SUBPART_PHYSICS_BREASTS_UPDOWN, SUBPART_PHYSICS_BREASTS_INOUT, SUBPART_PHYSICS_BREASTS_LEFTRIGHT, SUBPART_PHYSICS_BELLY_UPDOWN, SUBPART_PHYSICS_BUTT_UPDOWN, SUBPART_PHYSICS_BUTT_LEFTRIGHT, SUBPART_PHYSICS_ADVANCED));
}

LLEditWearableDictionary::WearableEntry::WearableEntry(LLWearableType::EType type,
		const std::string &title,
		U8 num_color_swatches,
		U8 num_texture_pickers,
		unsigned int num_subparts, ... ) :
	LLDictionaryEntry(title),
	mWearableType(type)
{
	va_list argp;
	va_start(argp, num_subparts);

	for (U8 i = 0; i < num_color_swatches; ++i)
	{
		ETextureIndex index = (ETextureIndex)va_arg(argp,int);
		mColorSwatchCtrls.push_back(index);
	}

	for (U8 i = 0; i < num_texture_pickers; ++i)
	{
		ETextureIndex index = (ETextureIndex)va_arg(argp,int);
		mTextureCtrls.push_back(index);
	}

	for (U8 i = 0; i < num_subparts; ++i)
	{
		ESubpart part = (ESubpart)va_arg(argp,int);
		mSubparts.push_back(part);
	}
}

LLEditWearableDictionary::Subparts::Subparts()
{
	addEntry(SUBPART_SHAPE_WHOLE, new SubpartEntry(SUBPART_SHAPE_WHOLE, "mPelvis", "shape_body","Body", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f),SEX_BOTH));
	const LLVector3d head_target(0.f, 0.f, 0.05f);
	const LLVector3d head_camera(-0.5f, 0.05f, 0.07f);
	addEntry(SUBPART_SHAPE_HEAD, new SubpartEntry(SUBPART_SHAPE_HEAD, "mHead", "shape_head","Head", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SHAPE_EYES, new SubpartEntry(SUBPART_SHAPE_EYES, "mHead", "shape_eyes","Eyes", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SHAPE_EARS, new SubpartEntry(SUBPART_SHAPE_EARS, "mHead", "shape_ears","Ears", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SHAPE_NOSE, new SubpartEntry(SUBPART_SHAPE_NOSE, "mHead", "shape_nose","Nose", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SHAPE_MOUTH, new SubpartEntry(SUBPART_SHAPE_MOUTH, "mHead", "shape_mouth","Mouth", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SHAPE_CHIN, new SubpartEntry(SUBPART_SHAPE_CHIN, "mHead", "shape_chin","Chin", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SHAPE_TORSO, new SubpartEntry(SUBPART_SHAPE_TORSO, "mTorso", "shape_torso","Torso", LLVector3d(0.f, 0.f, 0.3f), LLVector3d(-1.f, 0.15f, 0.3f), SEX_BOTH));
	addEntry(SUBPART_SHAPE_LEGS, new SubpartEntry(SUBPART_SHAPE_LEGS, "mPelvis", "shape_legs","Legs", LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f), SEX_BOTH));

	addEntry(SUBPART_SKIN_COLOR, new SubpartEntry(SUBPART_SKIN_COLOR, "mHead", "skin_color","Skin Color", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SKIN_FACEDETAIL, new SubpartEntry(SUBPART_SKIN_FACEDETAIL, "mHead", "skin_facedetail","Face Detail", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SKIN_MAKEUP, new SubpartEntry(SUBPART_SKIN_MAKEUP, "mHead", "skin_makeup","Makeup", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_SKIN_BODYDETAIL, new SubpartEntry(SUBPART_SKIN_BODYDETAIL, "mPelvis", "skin_bodydetail","Body Detail", LLVector3d(0.f, 0.f, -0.2f), LLVector3d(-2.5f, 0.5f, 0.5f), SEX_BOTH));

	addEntry(SUBPART_HAIR_COLOR, new SubpartEntry(SUBPART_HAIR_COLOR, "mHead", "hair_color","Color", LLVector3d(0.f, 0.f, 0.10f), LLVector3d(-0.4f, 0.05f, 0.10f),SEX_BOTH));
	addEntry(SUBPART_HAIR_STYLE, new SubpartEntry(SUBPART_HAIR_STYLE, "mHead", "hair_style","Style", LLVector3d(0.f, 0.f, 0.10f), LLVector3d(-0.4f, 0.05f, 0.10f),SEX_BOTH));
	addEntry(SUBPART_HAIR_EYEBROWS, new SubpartEntry(SUBPART_HAIR_EYEBROWS, "mHead", "hair_eyebrows","Eyebrows", head_target, head_camera,SEX_BOTH));
	addEntry(SUBPART_HAIR_FACIAL, new SubpartEntry(SUBPART_HAIR_FACIAL, "mHead", "hair_facial","Facial", head_target, head_camera,SEX_MALE));

	addEntry(SUBPART_EYES, new SubpartEntry(SUBPART_EYES, "mHead", "eyes",LLStringUtil::null, head_target, head_camera,SEX_BOTH));

	addEntry(SUBPART_SHIRT, new SubpartEntry(SUBPART_SHIRT, "mTorso", "shirt",LLStringUtil::null, LLVector3d(0.f, 0.f, 0.3f), LLVector3d(-1.f, 0.15f, 0.3f), SEX_BOTH));
	addEntry(SUBPART_PANTS, new SubpartEntry(SUBPART_PANTS, "mPelvis", "pants",LLStringUtil::null, LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f), SEX_BOTH));
	addEntry(SUBPART_SHOES, new SubpartEntry(SUBPART_SHOES, "mPelvis", "shoes",LLStringUtil::null, LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f), SEX_BOTH));
	addEntry(SUBPART_SOCKS, new SubpartEntry(SUBPART_SOCKS, "mPelvis", "socks",LLStringUtil::null, LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f), SEX_BOTH));
	addEntry(SUBPART_JACKET, new SubpartEntry(SUBPART_JACKET, "mTorso", "jacket",LLStringUtil::null, LLVector3d(0.f, 0.f, 0.f), LLVector3d(-2.f, 0.1f, 0.3f), SEX_BOTH));
	addEntry(SUBPART_SKIRT, new SubpartEntry(SUBPART_SKIRT, "mPelvis", "skirt",LLStringUtil::null, LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f), SEX_BOTH));
	addEntry(SUBPART_GLOVES, new SubpartEntry(SUBPART_GLOVES, "mTorso", "gloves",LLStringUtil::null, LLVector3d(0.f, 0.f, 0.f), LLVector3d(-1.f, 0.15f, 0.f), SEX_BOTH));
	addEntry(SUBPART_UNDERSHIRT, new SubpartEntry(SUBPART_UNDERSHIRT, "mTorso", "undershirt",LLStringUtil::null, LLVector3d(0.f, 0.f, 0.3f), LLVector3d(-1.f, 0.15f, 0.3f), SEX_BOTH));
	addEntry(SUBPART_UNDERPANTS, new SubpartEntry(SUBPART_UNDERPANTS, "mPelvis", "underpants",LLStringUtil::null, LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f), SEX_BOTH));
	addEntry(SUBPART_ALPHA, new SubpartEntry(SUBPART_ALPHA, "mPelvis", "alpha",LLStringUtil::null, LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f), SEX_BOTH));
	addEntry(SUBPART_TATTOO, new SubpartEntry(SUBPART_TATTOO, "mPelvis", "tattoo", LLStringUtil::null, LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f),SEX_BOTH));
	addEntry(SUBPART_UNIVERSAL, new SubpartEntry(SUBPART_UNIVERSAL, "mPelvis", "universal", LLStringUtil::null, LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f), SEX_BOTH));

	addEntry(SUBPART_PHYSICS_BREASTS_UPDOWN, new SubpartEntry(SUBPART_PHYSICS_BREASTS_UPDOWN, "mTorso", "physics_breasts_updown", "Breast Bounce", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-0.8f, 0.15f, 0.38),SEX_FEMALE));
	addEntry(SUBPART_PHYSICS_BREASTS_INOUT, new SubpartEntry(SUBPART_PHYSICS_BREASTS_INOUT, "mTorso", "physics_breasts_inout", "Breast Cleavage", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-0.8f, 0.15f, 0.38f),SEX_FEMALE));
	addEntry(SUBPART_PHYSICS_BREASTS_LEFTRIGHT, new SubpartEntry(SUBPART_PHYSICS_BREASTS_LEFTRIGHT, "mTorso", "physics_breasts_leftright", "Breast Sway", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-0.8f, 0.15f, 0.38f),SEX_FEMALE));
	addEntry(SUBPART_PHYSICS_BELLY_UPDOWN, new SubpartEntry(SUBPART_PHYSICS_BELLY_UPDOWN, "mTorso", "physics_belly_updown", "Belly Bounce", LLVector3d(0.f, 0.f, -.05f), LLVector3d(-0.8f, 0.15f, 0.38f),SEX_BOTH));
	addEntry(SUBPART_PHYSICS_BUTT_UPDOWN, new SubpartEntry(SUBPART_PHYSICS_BUTT_UPDOWN, "mTorso", "physics_butt_updown", "Butt Bounce", LLVector3d(0.f, 0.f,-.1f), LLVector3d(0.3f, 0.8f, -0.1f),SEX_BOTH));
	addEntry(SUBPART_PHYSICS_BUTT_LEFTRIGHT, new SubpartEntry(SUBPART_PHYSICS_BUTT_LEFTRIGHT, "mTorso", "physics_butt_leftright", "Butt Sway", LLVector3d(0.f, 0.f,-.1f), LLVector3d(0.3f, 0.8f, -0.1f),SEX_BOTH));
	addEntry(SUBPART_PHYSICS_ADVANCED, new SubpartEntry(SUBPART_PHYSICS_ADVANCED, "mTorso", "physics_advanced", "Advanced Parameters", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f),SEX_BOTH));
}

LLEditWearableDictionary::SubpartEntry::SubpartEntry(ESubpart part,
		const std::string &joint,
		const std::string &edit_group,
		const std::string &button_name,
		const LLVector3d  &target_offset,
		const LLVector3d  &camera_offset,
		const ESex        &sex) :
	LLDictionaryEntry(edit_group),
	mSubpart(part),
	mTargetJoint(joint),
	mEditGroup(edit_group),
	mButtonName(button_name),
	mTargetOffset(target_offset),
	mCameraOffset(camera_offset),
	mSex(sex)
{
}

LLEditWearableDictionary::ColorSwatchCtrls::ColorSwatchCtrls()
{
	addEntry ( TEX_UPPER_SHIRT,  new PickerControlEntry (TEX_UPPER_SHIRT, "Color/Tint" ));
	addEntry ( TEX_LOWER_PANTS,  new PickerControlEntry (TEX_LOWER_PANTS, "Color/Tint" ));
	addEntry ( TEX_LOWER_SHOES,  new PickerControlEntry (TEX_LOWER_SHOES, "Color/Tint" ));
	addEntry ( TEX_LOWER_SOCKS,  new PickerControlEntry (TEX_LOWER_SOCKS, "Color/Tint" ));
	addEntry ( TEX_UPPER_JACKET, new PickerControlEntry (TEX_UPPER_JACKET, "Color/Tint" ));
	addEntry ( TEX_SKIRT,  new PickerControlEntry (TEX_SKIRT, "Color/Tint" ));
	addEntry ( TEX_UPPER_GLOVES, new PickerControlEntry (TEX_UPPER_GLOVES, "Color/Tint" ));
	addEntry ( TEX_UPPER_UNDERSHIRT, new PickerControlEntry (TEX_UPPER_UNDERSHIRT, "Color/Tint" ));
	addEntry ( TEX_LOWER_UNDERPANTS, new PickerControlEntry (TEX_LOWER_UNDERPANTS, "Color/Tint" ));
	addEntry ( TEX_HEAD_TATTOO, new PickerControlEntry(TEX_HEAD_TATTOO, "Color/Tint" ));
	addEntry (TEX_HEAD_UNIVERSAL_TATTOO, new PickerControlEntry(TEX_HEAD_UNIVERSAL_TATTOO, "Color/Tint"));
}

LLEditWearableDictionary::TextureCtrls::TextureCtrls()
{
	addEntry ( TEX_HEAD_BODYPAINT,  new PickerControlEntry (TEX_HEAD_BODYPAINT,  "Head Tattoos", LLUUID::null, TRUE ));
	addEntry ( TEX_UPPER_BODYPAINT, new PickerControlEntry (TEX_UPPER_BODYPAINT, "Upper Tattoos", LLUUID::null, TRUE ));
	addEntry ( TEX_LOWER_BODYPAINT, new PickerControlEntry (TEX_LOWER_BODYPAINT, "Lower Tattoos", LLUUID::null, TRUE ));
	addEntry ( TEX_HAIR, new PickerControlEntry (TEX_HAIR, "Texture", LLUUID( gSavedSettings.getString( "UIImgDefaultHairUUID" ) ), FALSE ));
	addEntry ( TEX_EYES_IRIS, new PickerControlEntry (TEX_EYES_IRIS, "Iris", LLUUID( gSavedSettings.getString( "UIImgDefaultEyesUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_SHIRT, new PickerControlEntry (TEX_UPPER_SHIRT, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultShirtUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_PANTS, new PickerControlEntry (TEX_LOWER_PANTS, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultPantsUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_SHOES, new PickerControlEntry (TEX_LOWER_SHOES, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultShoesUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_SOCKS, new PickerControlEntry (TEX_LOWER_SOCKS, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultSocksUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_JACKET, new PickerControlEntry (TEX_UPPER_JACKET, "Upper Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_JACKET, new PickerControlEntry (TEX_LOWER_JACKET, "Lower Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ), FALSE ));
	addEntry ( TEX_SKIRT, new PickerControlEntry (TEX_SKIRT, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultSkirtUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_GLOVES, new PickerControlEntry (TEX_UPPER_GLOVES, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultGlovesUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_UNDERSHIRT, new PickerControlEntry (TEX_UPPER_UNDERSHIRT, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_UNDERPANTS, new PickerControlEntry (TEX_LOWER_UNDERPANTS, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_ALPHA, new PickerControlEntry (TEX_LOWER_ALPHA, "Lower Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_UPPER_ALPHA, new PickerControlEntry (TEX_UPPER_ALPHA, "Upper Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_HEAD_ALPHA, new PickerControlEntry (TEX_HEAD_ALPHA, "Head Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_EYES_ALPHA, new PickerControlEntry (TEX_EYES_ALPHA, "Eye Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_HAIR_ALPHA, new PickerControlEntry (TEX_HAIR_ALPHA, "Hair Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_LOWER_TATTOO, new PickerControlEntry (TEX_LOWER_TATTOO, "Lower Tattoo", LLUUID::null, TRUE ));
	addEntry ( TEX_UPPER_TATTOO, new PickerControlEntry (TEX_UPPER_TATTOO, "Upper Tattoo", LLUUID::null, TRUE ));
	addEntry ( TEX_HEAD_TATTOO, new PickerControlEntry (TEX_HEAD_TATTOO, "Head Tattoo", LLUUID::null, TRUE ));
	addEntry ( TEX_LOWER_UNIVERSAL_TATTOO, new PickerControlEntry( TEX_LOWER_UNIVERSAL_TATTOO, "Lower Universal Tattoo", LLUUID::null, TRUE));
	addEntry ( TEX_UPPER_UNIVERSAL_TATTOO, new PickerControlEntry( TEX_UPPER_UNIVERSAL_TATTOO, "Upper Universal Tattoo", LLUUID::null, TRUE));
	addEntry ( TEX_HEAD_UNIVERSAL_TATTOO, new PickerControlEntry( TEX_HEAD_UNIVERSAL_TATTOO, "Head Universal Tattoo", LLUUID::null, TRUE));
	addEntry ( TEX_SKIRT_TATTOO, new PickerControlEntry(TEX_SKIRT_TATTOO, "Skirt Tattoo", LLUUID::null, TRUE));
	addEntry ( TEX_HAIR_TATTOO, new PickerControlEntry(TEX_HAIR_TATTOO, "Hair Tattoo", LLUUID::null, TRUE));
	addEntry ( TEX_EYES_TATTOO, new PickerControlEntry(TEX_EYES_TATTOO, "Eyes Tattoo", LLUUID::null, TRUE));
	addEntry (TEX_LEFT_ARM_TATTOO, new PickerControlEntry(TEX_LEFT_ARM_TATTOO, "Left Arm Tattoo", LLUUID::null, TRUE));
	addEntry (TEX_LEFT_LEG_TATTOO, new PickerControlEntry(TEX_LEFT_LEG_TATTOO, "Left Leg Tattoo", LLUUID::null, TRUE));
	addEntry (TEX_AUX1_TATTOO, new PickerControlEntry(TEX_AUX1_TATTOO, "Aux1 Tattoo", LLUUID::null, TRUE));
	addEntry (TEX_AUX2_TATTOO, new PickerControlEntry(TEX_AUX2_TATTOO, "Aux2 Tattoo", LLUUID::null, TRUE));
	addEntry (TEX_AUX3_TATTOO, new PickerControlEntry(TEX_AUX3_TATTOO, "Aux3 Tattoo", LLUUID::null, TRUE));
}

LLEditWearableDictionary::PickerControlEntry::PickerControlEntry(ETextureIndex tex_index,
		const std::string name,
		const LLUUID default_image_id,
		const bool allow_no_texture) :
	LLDictionaryEntry(name),
	mTextureIndex(tex_index),
	mControlName(name),
	mDefaultImageId(default_image_id),
	mAllowNoTexture(allow_no_texture)
{
}

// Helper functions.
static const texture_vec_t null_texture_vec;

// Specializations of this template function return a vector of texture indexes of particular control type
// (i.e. LLColorSwatchCtrl or LLTextureCtrl) which are contained in given WearableEntry.
template <typename T>
const texture_vec_t&
get_pickers_indexes(const LLEditWearableDictionary::WearableEntry *wearable_entry) { return null_texture_vec; }

// Specializations of this template function return picker control entry for particular control type.
template <typename T>
const LLEditWearableDictionary::PickerControlEntry*
get_picker_entry (const ETextureIndex index) { return nullptr; }

typedef std::function<void(LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry*)> function_t;

typedef struct PickerControlEntryNamePredicate
{
	PickerControlEntryNamePredicate(const std::string name) : mName (name) {};
	bool operator()(const LLEditWearableDictionary::PickerControlEntry* entry) const
	{
		return (entry && entry->mName == mName);
	}
	private:
	const std::string mName;
} PickerControlEntryNamePredicate;

// A full specialization of get_pickers_indexes for LLColorSwatchCtrl
template <>
const texture_vec_t&
get_pickers_indexes<LLColorSwatchCtrl> (const LLEditWearableDictionary::WearableEntry *wearable_entry)
{
	if (!wearable_entry)
	{
		LL_WARNS() << "could not get LLColorSwatchCtrl indexes for null wearable entry." << LL_ENDL;
		return null_texture_vec;
	}
	return wearable_entry->mColorSwatchCtrls;
}

// A full specialization of get_pickers_indexes for LLTextureCtrl
template <>
const texture_vec_t&
get_pickers_indexes<LLTextureCtrl> (const LLEditWearableDictionary::WearableEntry *wearable_entry)
{
	if (!wearable_entry)
	{
		LL_WARNS() << "could not get LLTextureCtrl indexes for null wearable entry." << LL_ENDL;
		return null_texture_vec;
	}
	return wearable_entry->mTextureCtrls;
}

// A full specialization of get_picker_entry for LLColorSwatchCtrl
template <>
const LLEditWearableDictionary::PickerControlEntry*
get_picker_entry<LLColorSwatchCtrl> (const ETextureIndex index)
{
	return LLEditWearableDictionary::getInstance()->getColorSwatch(index);
}

// A full specialization of get_picker_entry for LLTextureCtrl
template <>
const LLEditWearableDictionary::PickerControlEntry*
get_picker_entry<LLTextureCtrl> (const ETextureIndex index)
{
	return LLEditWearableDictionary::getInstance()->getTexturePicker(index);
}

template <typename CtrlType, class Predicate>
const LLEditWearableDictionary::PickerControlEntry*
find_picker_ctrl_entry_if(LLWearableType::EType type, const Predicate pred)
{
	const LLEditWearableDictionary::WearableEntry *wearable_entry
		= LLEditWearableDictionary::getInstance()->getWearable(type);
	if (!wearable_entry)
	{
		LL_WARNS() << "could not get wearable dictionary entry for wearable of type: " << type << LL_ENDL;
                return nullptr;
	}
	const texture_vec_t& indexes = get_pickers_indexes<CtrlType>(wearable_entry);
        for (auto te : indexes)
	{
            const LLEditWearableDictionary::PickerControlEntry*     entry
                        = get_picker_entry<CtrlType>(te);
		if (!entry)
		{
			LL_WARNS() << "could not get picker dictionary entry (" << te << ") for wearable of type: " << type << LL_ENDL;
			continue;
		}
		if (pred(entry))
		{
			return entry;
		}
	}
        return nullptr;
}

template <typename CtrlType>
void for_each_picker_ctrl_entry(LLPanel* panel, LLWearableType::EType type, function_t fun)
{
	if (!panel)
	{
		LL_WARNS() << "the panel wasn't passed for wearable of type: " << type << LL_ENDL;
		return;
	}
	const LLEditWearableDictionary::WearableEntry* wearable_entry = LLEditWearableDictionary::getInstance()->getWearable(type);
	if (!wearable_entry)
	{
		LL_WARNS() << "could not get wearable dictionary entry for wearable of type: " << type << LL_ENDL;
		return;
	}
	const texture_vec_t& indexes = get_pickers_indexes<CtrlType>(wearable_entry);
	for (const auto& te : indexes)
	{
		const LLEditWearableDictionary::PickerControlEntry* entry = get_picker_entry<CtrlType>(te);
		if (!entry)
		{
			LL_WARNS() << "could not get picker dictionary entry (" << te << ") for wearable of type: " << type << LL_ENDL;
			continue;
		}
		fun (panel, entry);
	}
}

// The helper functions for pickers management
static void init_color_swatch_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLColorSwatchCtrl* color_swatch_ctrl = panel->getChild<LLColorSwatchCtrl>(entry->mControlName);
	if (color_swatch_ctrl)
	{
		color_swatch_ctrl->setCommitCallback(boost::bind(&LLPanelEditWearable::onColorSwatchCommit, self, _1));
		// Can't get the color from the wearable here, since the wearable may not be set when this is called.
		color_swatch_ctrl->setOriginal(LLColor4::white);
	}
}

static void init_texture_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLTextureCtrl* texture_ctrl = panel->getChild<LLTextureCtrl>(entry->mControlName);
	if (texture_ctrl)
	{
		texture_ctrl->setCommitCallback(boost::bind(&LLPanelEditWearable::onTexturePickerCommit, self, _1));
		texture_ctrl->setDefaultImageAssetID(entry->mDefaultImageId);
		texture_ctrl->setAllowNoTexture(entry->mAllowNoTexture);
		// Don't allow (no copy) or (notransfer) textures to be selected.
		texture_ctrl->setImmediateFilterPermMask(PERM_NONE);//PERM_COPY | PERM_TRANSFER);
		texture_ctrl->setNonImmediateFilterPermMask(PERM_NONE);//PERM_COPY | PERM_TRANSFER);
	}
}

static void update_color_swatch_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLColorSwatchCtrl* color_swatch_ctrl = panel->getChild<LLColorSwatchCtrl>(entry->mControlName);
	if (color_swatch_ctrl)
	{
		color_swatch_ctrl->set(self->getWearable()->getClothesColor(entry->mTextureIndex));
	}
}

static void update_texture_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLTextureCtrl* texture_ctrl = panel->getChild<LLTextureCtrl>(entry->mControlName);
	if (texture_ctrl)
	{
		LLUUID new_id;
		LLLocalTextureObject *lto = self->getWearable()->getLocalTextureObject(entry->mTextureIndex);
		if (lto && (lto->getID() != IMG_DEFAULT_AVATAR))
		{
			new_id = lto->getID();
		}
		else
		{
			new_id = LLUUID::null;
		}
		LLUUID old_id = texture_ctrl->getImageAssetID();
		if (old_id != new_id)
		{
			// texture has changed, close the floater to avoid DEV-22461
			texture_ctrl->closeFloater();
		}
		texture_ctrl->setImageAssetID(new_id);
	}
}

static void set_enabled_color_swatch_ctrl(bool enabled, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLColorSwatchCtrl* color_swatch_ctrl = panel->getChild<LLColorSwatchCtrl>(entry->mControlName);
	if (color_swatch_ctrl)
	{
		color_swatch_ctrl->setEnabled(enabled);
		color_swatch_ctrl->setVisible(enabled);
	}
}

static void set_enabled_texture_ctrl(bool enabled, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLTextureCtrl* texture_ctrl = panel->getChild<LLTextureCtrl>(entry->mControlName);
	if (texture_ctrl)
	{
		texture_ctrl->setEnabled(enabled);
		texture_ctrl->setVisible(enabled);
	}
}

class LLWearableSaveAsDialog : public LLModalDialog
{
private:
	std::string	mItemName;
	boost::function<void(LLWearableSaveAsDialog*)> mSaveCallback;
	LLPanelEditWearable* mParent;

public:
	LLWearableSaveAsDialog( const std::string& desc, LLPanelEditWearable* parent, boost::function<void(LLWearableSaveAsDialog*)> cb )
		: LLModalDialog( LLStringUtil::null, 240, 100 ),
		  mSaveCallback( cb ),
		  mParent( parent )
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_wearable_save_as.xml");

		getChild<LLUICtrl>("Save")->setCommitCallback(boost::bind(&LLWearableSaveAsDialog::onSave, this) );
		getChild<LLUICtrl>("Cancel")->setCommitCallback(boost::bind(&LLWearableSaveAsDialog::onCancel, this) );

		getChild<LLUICtrl>("name ed")->setTextArg("[DESC]", desc);
	}
	~LLWearableSaveAsDialog()
	{
		if(mParent && mParent->mActiveModal == this)
			mParent->mActiveModal = NULL;
	}
	virtual void startModal()
	{
		LLModalDialog::startModal();
		LLLineEditor* edit = getChild<LLLineEditor>("name ed");
		if (!edit) return;
		edit->setFocus(TRUE);
		edit->selectAll();
	}

	const std::string& getItemName() { return mItemName; }

	void onSave()
	{
		mItemName = getChildView("name ed")->getValue().asString();
		LLStringUtil::trim(mItemName);
		if( !mItemName.empty() )
		{
			if( mSaveCallback )
			{
				mSaveCallback( this );
			}
			close(); // destroys this object
		}
	}

	void onCancel()
	{
		close(); // destroys this object
	}
};

LLPanelEditWearable::LLPanelEditWearable( LLWearableType::EType type, LLFloaterCustomize* parent )

	: LLPanel( LLWearableType::getTypeLabel( type ) ),
	  mType( type ),
	  mActiveModal( NULL ),
	  mCurrentIndex( 0 ),
	  mCurrentWearable( NULL ),
	  mPendingWearable( NULL ),
	  mPendingRefresh( false ),
	  mCustomizeFloater( parent ),
	  mSubpartBtns()
{
}
LLPanelEditWearable::~LLPanelEditWearable()
{
	if(mActiveModal)
	{
		mActiveModal->close();
	}
}

static std::string getTabName(U32 i) { return llformat("%i", i); }
void LLPanelEditWearable::addLayerTabs(U32 index, U32 last)
{
	for(U32 i = index; i <= last; ++i)
	{
		const auto number = getTabName(i);
		mTab->addTabPanel(new LLPanel(number), "Layer " + number);
	}
}

BOOL LLPanelEditWearable::postBuild()
{
	std::string icon_name = LLInventoryIcon::getIconName(LLWearableType::getAssetType( mType ),LLInventoryType::IT_WEARABLE,mType,FALSE);

	getChildView("icon")->setValue(icon_name);

	mCreateNew = getChild<LLUICtrl>("Create New");
	mCreateNew->setCommitCallback(boost::bind(&LLPanelEditWearable::onBtnCreateNew, this));

	if (mCreateNewLayer = findChild<LLUICtrl>("New Layer"))
	{
		mCreateNewLayer->setCommitCallback(boost::bind(&LLPanelEditWearable::onBtnCreateNew, this));
	}

	if (mTakeOff = findChild<LLUICtrl>("Take Off"))
	{
		// If PG, can't take off underclothing or shirt
		mCanTakeOff = !(gAgent.isTeen() && (mType == LLWearableType::WT_UNDERSHIRT || mType == LLWearableType::WT_UNDERPANTS) );
		mTakeOff->setVisible(mCanTakeOff);
		mTakeOff->setCommitCallback(boost::bind(&LLPanelEditWearable::onBtnTakeOff, this));
	}

	if (mArrowLeft = findChild<LLUICtrl>("Arrow Left"))
	{
		mArrowLeft->setCommitCallback(boost::bind(&LLPanelEditWearable::onMoveToLayer, this, true));
	}

	if (mArrowRight = findChild<LLUICtrl>("Arrow Right"))
	{
		mArrowRight->setCommitCallback(boost::bind(&LLPanelEditWearable::onMoveToLayer, this, false));
	}


	if (mSexRadio = findChild<LLUICtrl>("sex radio"))
	{
		mSexRadio->setCommitCallback(boost::bind(&LLPanelEditWearable::onCommitSexChange, this));
	}

	mSave = getChild<LLUICtrl>("Save");
	mSave->setCommitCallback(boost::bind(&LLPanelEditWearable::saveChanges, this, false, LLStringUtil::null) );

	mSaveAs = getChild<LLUICtrl>("Save As");
	mSaveAs->setCommitCallback(boost::bind(&LLPanelEditWearable::onBtnSaveAs, this) );

	mRevert = getChild<LLUICtrl>("Revert");
	mRevert->setCommitCallback(boost::bind(&LLPanelEditWearable::revertChanges, this) );

	// Cache other UI for later
	mNotWornI = getChildView("not worn instructions");
	mNoModI = getChildView("no modify instructions");
	mNotWornT = getChild<LLUICtrl>("title_not_worn");
	mNoModT = getChild<LLUICtrl>("title_no_modify");
	mTitle = getChild<LLUICtrl>("title");
	mTitleLoading = getChild<LLUICtrl>("title_loading");
	mPath = getChild<LLUICtrl>("path");
	mSquare = getChildView("square");
	mAvHeight = getChild<LLUICtrl>("avheight", false, false);

	configureAlphaCheckbox(LLAvatarAppearanceDefines::TEX_LOWER_ALPHA, "lower alpha texture invisible");
	configureAlphaCheckbox(LLAvatarAppearanceDefines::TEX_UPPER_ALPHA, "upper alpha texture invisible");
	configureAlphaCheckbox(LLAvatarAppearanceDefines::TEX_HEAD_ALPHA, "head alpha texture invisible");
	configureAlphaCheckbox(LLAvatarAppearanceDefines::TEX_EYES_ALPHA, "eye alpha texture invisible");
	configureAlphaCheckbox(LLAvatarAppearanceDefines::TEX_HAIR_ALPHA, "hair alpha texture invisible");
	
	{
		const LLEditWearableDictionary::WearableEntry *wearable_entry = LLEditWearableDictionary::getInstance()->getWearable(mType);
		if (!wearable_entry)
		{
			LL_WARNS() << "could not get wearable dictionary entry for wearable of type: " << mType << LL_ENDL;
		}
		const U8 num_subparts = (U8) wearable_entry->mSubparts.size();

		for (U8 index = 0; num_subparts > 1 && index < num_subparts; ++index)
		{
			// dive into data structures to get the panel we need
			ESubpart subpart_e = wearable_entry->mSubparts[index];
			const LLEditWearableDictionary::SubpartEntry *subpart_entry = LLEditWearableDictionary::getInstance()->getSubpart(subpart_e);
			
			if (!subpart_entry)
			{
				LL_WARNS() << "could not get wearable subpart dictionary entry for subpart: " << subpart_e << LL_ENDL;
				mSubpartBtns.push_back(NULL);
				continue;
			}
			
			if (!subpart_entry->mButtonName.empty())
			{
				//LL_INFOS() << "Finding button " << subpart_entry->mButtonName << LL_ENDL;
				LLButton* btn(findChild<LLButton>(subpart_entry->mButtonName));
				llassert_always(btn);
				mSubpartBtns.push_back(btn);
				btn->setCommitCallback(boost::bind(&LLPanelEditWearable::changeCamera, this, index));
			}
			else
			{
				mSubpartBtns.push_back(NULL);
				LL_WARNS() << "could not get wearable subpart button for subpart num: " << subpart_e << LL_ENDL;
			}
		}
		// initialize texture and color picker controls
		for_each_picker_ctrl_entry <LLColorSwatchCtrl> (this, mType, boost::bind(init_color_swatch_ctrl, this, _1, _2));
		for_each_picker_ctrl_entry <LLTextureCtrl>     (this, mType, boost::bind(init_texture_ctrl, this, _1, _2));
	}

	if (mTab = findChild<LLTabContainer>("layer_tabs"))
	{
		addLayerTabs(1, gAgentWearables.getWearableCount(mType));
		mTab->setCommitCallback(boost::bind(&LLPanelEditWearable::onTabChanged, this, _1));
		mTab->setValidateCallback(boost::bind(&LLPanelEditWearable::onTabPrecommit, this));
	}

	mNotWornT->setTextArg("[DESC]", LLWearableType::getTypeLabel(mType));
	mTitleLoading->setTextArg("[DESC]", LLWearableType::getTypeLabel(mType));
	mNoModT->setTextArg("[DESC]", LLWearableType::getTypeLabel(mType));
	mTitle->setTextArg("[DESC]", LLWearableType::getTypeLabel(mType));
	return TRUE;
}

BOOL LLPanelEditWearable::isDirty() const
{
	LLViewerWearable* wearable = getWearable();
	return wearable && wearable->isDirty();
}

void LLPanelEditWearable::draw()
{
	if (mCustomizeFloater->isMinimized() || !isAgentAvatarValid())
		return;

	refreshWearables(false);

	LLViewerWearable* wearable = getWearable();
	BOOL has_wearable = wearable != nullptr;
	BOOL is_dirty = isDirty();
	BOOL is_modifiable = FALSE;
	BOOL is_copyable = FALSE;
	BOOL is_complete = FALSE;
	LLInventoryItem* item = nullptr;
	if (wearable && (item = gInventory.getItem(wearable->getItemID())))
	{
		const LLPermissions& perm = item->getPermissions();
		is_modifiable = perm.allowModifyBy(gAgent.getID(), gAgent.getGroupID());
		is_copyable = perm.allowCopyBy(gAgent.getID(), gAgent.getGroupID());
		is_complete = ((LLViewerInventoryItem*)item)->isComplete();
	}

	mSave->setEnabled(has_wearable && is_modifiable && is_complete && is_dirty);
	mSaveAs->setEnabled(has_wearable && is_copyable && is_complete);
	mRevert->setEnabled(has_wearable && is_dirty );
	mTitle->setVisible(has_wearable && is_modifiable && is_complete);
	mTitleLoading->setVisible(has_wearable && is_modifiable && !is_complete);

	if (has_wearable && is_modifiable)
	{
		for_each_picker_ctrl_entry <LLColorSwatchCtrl>	(this, mType, boost::bind(update_color_swatch_ctrl, this, _1, _2));
		for_each_picker_ctrl_entry <LLTextureCtrl>    	(this, mType, boost::bind(update_texture_ctrl, this, _1, _2));
		for_each_picker_ctrl_entry <LLColorSwatchCtrl>	(this, mType, boost::bind(set_enabled_color_swatch_ctrl, is_complete, _1, _2));
		for_each_picker_ctrl_entry <LLTextureCtrl>      (this, mType, boost::bind(set_enabled_texture_ctrl, is_complete, _1, _2));
		for(ctrl_texture_index_map_t::iterator iter = mAlphaCheckbox2Index.begin();
				iter != mAlphaCheckbox2Index.end(); ++iter)
		{
			if (LLUICtrl* ctrl = iter->first)
			{
				ctrl->setEnabled(is_copyable && is_complete);
				ctrl->setVisible(is_copyable && is_complete);
			}
		}
	}
	else
	{
		hideTextureControls();
	}

	const LLEditWearableDictionary::WearableEntry *wearable_entry = LLEditWearableDictionary::getInstance()->getWearable(mType);
	if (wearable_entry)
	{
		const U8 num_subparts = (U8) wearable_entry->mSubparts.size();

		for (U8 index = 0; num_subparts > 1 && index < num_subparts; ++index)
		{
			// dive into data structures to get the panel we need
			ESubpart subpart_e = wearable_entry->mSubparts[index];
			const LLEditWearableDictionary::SubpartEntry *subpart_entry = LLEditWearableDictionary::getInstance()->getSubpart(subpart_e);

			if (!subpart_entry)
			{
					LL_WARNS() << "could not get wearable subpart dictionary entry for subpart: " << subpart_e << LL_ENDL;
					continue;
			}

			if (index >= mSubpartBtns.size()) continue;
			LLButton* child = mSubpartBtns[index];
			if (!child) continue;

			child->setVisible(has_wearable);
			if (has_wearable && is_complete && is_modifiable)
			{
				child->setEnabled(subpart_entry->mSex & gAgentAvatarp->getSex());
			}
			else
			{
				child->setEnabled(false);
			}
		}
	}

	if (mAvHeight) //Only display this if the element exists
	{
		// Display the shape's nominal height.
		//
		// The value for avsize is the best available estimate from
		// measuring against prims.
		float avsize = gAgentAvatarp->mBodySize.mV[VZ] + .195;
		int inches = (int)(avsize / .0254f);
		int feet = inches / 12;
		inches %= 12;

		std::ostringstream avheight(std::ostringstream::trunc);
		avheight << std::fixed << std::setprecision(2) << avsize << " m ("
			<< feet << "' " << inches << "\")";
		mAvHeight->setVisible(TRUE);
		mAvHeight->setTextArg("[AVHEIGHT]",avheight.str());
	}

	LLPanel::draw();
}

void LLPanelEditWearable::setVisible(BOOL visible)
{
	LLPanel::setVisible( visible );
	if (!visible)
	{
		for_each_picker_ctrl_entry <LLColorSwatchCtrl> (this, mType, boost::bind(set_enabled_color_swatch_ctrl, FALSE, _1, _2));
	}
}

void LLPanelEditWearable::onTabChanged(LLUICtrl* ctrl)
{
	if (mPendingWearable)
		return;
	U32 tab_index = ((LLTabContainer*)ctrl)->getCurrentPanelIndex();
	U32 wearable_index = tab_index ;
	if (wearable_index != mCurrentIndex )
	{
		setWearableIndex(wearable_index);
	}
}

bool LLPanelEditWearable::onTabPrecommit()
{
	return !mPendingWearable;
}

void LLPanelEditWearable::setWearableIndex(S32 index)
{
	if (mPendingWearable)
		return;

	mCurrentIndex = index;

	if (mTab)
	{
		if (mTab->getTabCount() && mTab->getCurrentPanelIndex() != index)
			mTab->selectTab(index);
	}


	LLViewerWearable* wearable = gAgentWearables.getViewerWearable(mType,mCurrentIndex);

	// Singu note: Set title even if the wearable didn't change: the name might have changed (when renamed).
	const auto& desc = wearable ? wearable->getName() : LLWearableType::getTypeLabel(mType);
	mTitle->setTextArg("[DESC]", desc);
	mNoModT->setTextArg("[DESC]", desc);

	if (wearable == getWearable())
		return;

	mCurrentWearable = wearable;

	if(mActiveModal)
		mActiveModal->close();

	bool editable = updatePermissions();

	if (mType == LLWearableType::WT_ALPHA)
	{
		initPreviousAlphaTextures();
		updateAlphaCheckboxes();

	}

	const LLEditWearableDictionary::SubpartEntry *subpart_entry = LLEditWearableDictionary::getInstance()->getSubpart((ESubpart)mCurrentSubpart);
	if (subpart_entry)
	{
		value_map_t sorted_params;
		getSortedParams(sorted_params, subpart_entry->mEditGroup, editable);
		buildParamList(mCustomizeFloater->getScrollingPanelList(), sorted_params);
	}

	if (wearable)
	{
		std::string path;
		const LLUUID& item_id = wearable->getItemID();
		append_path(item_id, path);
		mPath->setTextArg("[PATH]", path);
	}

	updateScrollingPanelList();
}

void LLPanelEditWearable::refreshWearables(bool force_immediate)
{
	if (!force_immediate && !mPendingRefresh)
		return;

	mPendingRefresh = false;

	U32 index;
	if (mPendingWearable)
	{
		if (!gAgentWearables.getWearableIndex(mPendingWearable, index))
			return;
		mPendingWearable = NULL;
	}
	else
	{
		if (!gAgentWearables.getWearableIndex(mPendingWearable, index))
		{
			index = gAgentWearables.getWearableCount(mType);
			if (index)
				--index;
		}
	}

	if (mTab)
	{
		S32 layer_count = gAgentWearables.getWearableCount(mType);
		S32 tab_count = mTab->getTabCount();
		if (tab_count > layer_count) // Remove some tabs
		{
			while (tab_count && tab_count > layer_count)
			{
				if (auto tab = mTab->getChild<LLPanel>(getTabName(tab_count--), false, false))
				{
					mTab->removeTabPanel(tab);
					delete tab;
				}
			}
		}
		else if (layer_count > tab_count) // Add some tabs
		{
			addLayerTabs(tab_count+1, layer_count);
		}
	}
	setWearableIndex(index);
}

void LLPanelEditWearable::wearablesChanged()
{
	mPendingRefresh = true;
}

void LLPanelEditWearable::onBtnSaveAs()
{
	if (mActiveModal)
		return;
	LLViewerWearable* wearable = getWearable();
	if (wearable)
	{
		mActiveModal = new LLWearableSaveAsDialog( wearable->getName(), this, boost::bind(&LLPanelEditWearable::onSaveAsCommit, this, _1));
		mActiveModal->startModal();
		// LLWearableSaveAsDialog deletes itself.
	}
}

void LLPanelEditWearable::onSaveAsCommit( LLWearableSaveAsDialog* save_as_dialog )
{
	saveChanges(true, save_as_dialog->getItemName());
}

void LLPanelEditWearable::onCommitSexChange()
{
	if (!isAgentAvatarValid()) return;

	LLWearableType::EType type = mType;	// TODO: MULTI-WEARABLE
	U32 index = mCurrentIndex;	// TODO: MULTI-WEARABLE

	if (!gAgentWearables.isWearableModifiable(type, index))
	{
		return;
	}
	
	LLViewerVisualParam* param = static_cast<LLViewerVisualParam*>(gAgentAvatarp->getVisualParam( "male" ));
	if (!param)
	{
		return;
	}

	bool is_new_sex_male = gSavedSettings.getU32("AvatarSex") ? true : false;
	LLViewerWearable* wearable = gAgentWearables.getViewerWearable(type, index);
	if (wearable)
	{
		wearable->setVisualParamWeight(param->getID(), is_new_sex_male, FALSE);
	}
	param->setWeight( is_new_sex_male, FALSE );

	gAgentAvatarp->updateSexDependentLayerSets( FALSE );

	gAgentAvatarp->updateVisualParams();

	updateScrollingPanelUI();
}

void LLPanelEditWearable::onBtnCreateNew()
{
	LLSD payload;
	payload["wearable_type"] = (S32)getType();
	LLNotificationsUtil::add("AutoWearNewClothing", LLSD(), payload, &onSelectAutoWearOption);
}

bool LLPanelEditWearable::onSelectAutoWearOption(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLVOAvatar* avatar = gAgentAvatarp;
	if (avatar)
	{
		// Create a new wearable in the default folder for the wearable's asset type.
		LLViewerWearable* wearable = LLWearableList::instance().createNewWearable( (LLWearableType::EType)notification["payload"]["wearable_type"].asInteger(), avatar );
		LLAssetType::EType asset_type = wearable->getAssetType();

		LLUUID folder_id;
		// regular UI, items get created in normal folder
		folder_id = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(asset_type));

		// Only auto wear the new item if the AutoWearNewClothing checkbox is selected.
		LLPointer<LLInventoryCallback> cb = option == 0 ? 
			new LLBoostFuncInventoryCallback(boost::bind(&wear_on_avatar_cb,_1,false)) : nullptr;
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
			folder_id, wearable->getTransactionID(), wearable->getName(), wearable->getDescription(),
			asset_type, LLInventoryType::IT_WEARABLE, wearable->getType(),
			wearable->getPermissions().getMaskNextOwner(), cb);
	}
	return false;
}

void LLPanelEditWearable::onMoveToLayer(bool closer)
{
	const auto wearable = getWearable();
	auto& appearance_mgr(LLAppearanceMgr::instance());
	auto links = appearance_mgr.findCOFItemLinks(wearable->getItemID());
	if (links.empty()) return;
	auto link = links.front();
	if (gAgentWearables.moveWearable(link, closer))
	{
		gAgentAvatarp->wearableUpdated(mType, true);
		/* Singu TODO: Figure out how to maintain focus on the current tab
		U32 index(0);
		gAgentWearables.getWearableIndex(wearable, index);
		setWearableIndex(index);
		*/
	}
}

LLViewerWearable* LLPanelEditWearable::getWearable() const
{
	return mCurrentWearable;
}

U32 LLPanelEditWearable::getIndex() const
{
	return mCurrentIndex;
}

void LLPanelEditWearable::onTexturePickerCommit(const LLUICtrl* ctrl)
{
	const LLTextureCtrl* texture_ctrl = dynamic_cast<const LLTextureCtrl*>(ctrl);
	if (!texture_ctrl)
	{
		LL_WARNS() << "got commit signal from not LLTextureCtrl." << LL_ENDL;
		return;
	}
	
	if (getWearable())
	{
		LLWearableType::EType type = getWearable()->getType();
		const PickerControlEntryNamePredicate name_pred(texture_ctrl->getName());
		const LLEditWearableDictionary::PickerControlEntry* entry
				= find_picker_ctrl_entry_if<LLTextureCtrl, PickerControlEntryNamePredicate>(type, name_pred);
		if (entry)
		{
			// Set the new version
			setNewImageID(entry->mTextureIndex, texture_ctrl->getImageAssetID());
		}
		else
		{
			LL_WARNS() << "could not get texture picker dictionary entry for wearable of type: " << type << LL_ENDL;
		}
	}
}

void LLPanelEditWearable::setNewImageID(ETextureIndex te_index, LLUUID const& uuid)
{
	LLViewerFetchedTexture* image = LLViewerTextureManager::getFetchedTexture(uuid);
	if (image->getID() == IMG_DEFAULT)
	{
		image = LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
	}
	if (getWearable())
	{
		U32 index;
		if (gAgentWearables.getWearableIndex(getWearable(), index))
		{
			gAgentAvatarp->setLocalTexture(te_index, image, FALSE, index);
			LLVisualParamHint::requestHintUpdates();
			gAgentAvatarp->wearableUpdated(mType, FALSE);
		}
	}
	if (mType == LLWearableType::WT_ALPHA && image->getID() != IMG_INVISIBLE)
	{
		mPreviousAlphaTexture[te_index] = image->getID();
	}
}

void LLPanelEditWearable::onColorSwatchCommit(const LLUICtrl* base_ctrl )
{
	LLColorSwatchCtrl* ctrl = (LLColorSwatchCtrl*) base_ctrl;
	
	if (getWearable())
	{
		LLWearableType::EType type = getWearable()->getType();
		const PickerControlEntryNamePredicate name_pred(ctrl->getName());
		const LLEditWearableDictionary::PickerControlEntry* entry = find_picker_ctrl_entry_if<LLColorSwatchCtrl, PickerControlEntryNamePredicate>(type, name_pred);
		if (entry)
		{
			const LLColor4& old_color = getWearable()->getClothesColor(entry->mTextureIndex);
			const LLColor4& new_color = LLColor4(ctrl->getValue());
			if (old_color != new_color)
			{
				getWearable()->setClothesColor(entry->mTextureIndex, new_color, TRUE);
				LLVisualParamHint::requestHintUpdates();
				gAgentAvatarp->wearableUpdated(getWearable()->getType(), FALSE);
			}
		}
		else
		{
			LL_WARNS() << "could not get color swatch dictionary entry for wearable of type: " << type << LL_ENDL;
		}
	}
}


void LLPanelEditWearable::hideTextureControls()
{
	for_each_picker_ctrl_entry <LLTextureCtrl>     (this, mType, boost::bind(set_enabled_texture_ctrl, FALSE, _1, _2));
	for_each_picker_ctrl_entry <LLColorSwatchCtrl> (this, mType, boost::bind(set_enabled_color_swatch_ctrl, FALSE, _1, _2));
	for(ctrl_texture_index_map_t::iterator iter = mAlphaCheckbox2Index.begin();
	        iter != mAlphaCheckbox2Index.end(); ++iter )
	{
		if (LLUICtrl* ctrl = iter->first)
		{
			ctrl->setEnabled(FALSE);
			ctrl->setVisible(FALSE);
		}
	}
}

void LLPanelEditWearable::saveChanges(bool force_save_as, std::string new_name)
{
	if (!getWearable() || (!force_save_as && !isDirty()))
	{
		// do nothing if no unsaved changes
		return;
	}
	
	U32 index;
	gAgentWearables.getWearableIndex(getWearable(), index);

	// Find an existing link to this wearable's inventory item, if any, and its description field.
	LLInventoryItem *link_item = NULL;

	std::string description;
	if (gAgentAvatarp->isUsingServerBakes())
	{
		LLInventoryModel::item_array_t links =
			LLAppearanceMgr::instance().findCOFItemLinks(getWearable()->getItemID());
		if (links.size()>0)
		{
			link_item = links.at(0).get();
			if (link_item && link_item->getIsLinkType())
			{
				description = link_item->getActualDescription();
			}
		}
	}

	if (force_save_as)
	{
		// the name of the wearable has changed, re-save wearable with new name
		LLAppearanceMgr::instance().removeCOFItemLinks(getWearable()->getItemID());
		LLViewerWearable* new_wearable = gAgentWearables.saveWearableAs(mType, index, new_name, description, FALSE);
		if(new_wearable)
		{
			mPendingWearable = new_wearable;
			mCurrentWearable = new_wearable;
			mTitle->setTextArg("[DESC]", new_wearable->getName());
			mNoModT->setTextArg("[DESC]", new_wearable->getName());
		}
	}
	else
	{
		// Make another copy of this link, with the same
		// description.  This is needed to bump the COF
		// version so texture baking service knows appearance has changed.
		if (link_item)
		{
			// Create new link
			LL_DEBUGS("Avatar") << "link refresh, creating new link to " << link_item->getLinkedUUID()
								<< " removing old link at " << link_item->getUUID()
								<< /*" wearable item id " << mWearablePtr->getItemID() <<*/ LL_ENDL;

			link_inventory_object( LLAppearanceMgr::instance().getCOF(),
								 link_item,
								 NULL);

			// Remove old link
			remove_inventory_item(link_item, NULL);
		}
		gAgentWearables.saveWearable(mType, index, new_name);
	}
}

void LLPanelEditWearable::revertChanges()
{
	LLViewerWearable* wearable = getWearable();
	if (!wearable || !isDirty())
	{
		// no unsaved changes to revert
		return;
	}

	wearable->revertValues();
	mTitle->setTextArg("[DESC]", wearable->getName());
	gAgentAvatarp->wearableUpdated(mType, FALSE);

	if (mType == LLWearableType::WT_ALPHA)
	{
		updateAlphaCheckboxes();
	}
}

void LLPanelEditWearable::showDefaultSubpart()
{
	refreshWearables(true);
	changeCamera(0);
}

void LLPanelEditWearable::setUIPermissions(U32 perm_mask, BOOL is_complete)
{
	BOOL is_copyable = (perm_mask & PERM_COPY) ? TRUE : FALSE;
	BOOL is_modifiable = (perm_mask & PERM_MODIFY) ? TRUE : FALSE;

	mSave->setEnabled(is_modifiable && is_complete);
	mSaveAs->setEnabled(is_copyable && is_complete);
	if (mSexRadio)
		mSexRadio->setEnabled(is_modifiable && is_complete);
	for_each_picker_ctrl_entry <LLTextureCtrl>		(this, mType, boost::bind(set_enabled_texture_ctrl, is_copyable && is_modifiable && is_complete, _1, _2));
	for_each_picker_ctrl_entry <LLColorSwatchCtrl>	(this, mType, boost::bind(set_enabled_color_swatch_ctrl, is_modifiable && is_complete, _1, _2));
	for(ctrl_texture_index_map_t::iterator iter = mAlphaCheckbox2Index.begin();
            iter != mAlphaCheckbox2Index.end(); ++iter )
	{
		if (LLUICtrl* ctrl = iter->first)
		{
			ctrl->setEnabled(is_copyable && is_modifiable && is_complete);
			ctrl->setVisible(is_copyable && is_modifiable && is_complete);
		}
	}
}

void LLPanelEditWearable::changeCamera(U8 subpart)
{
	const LLEditWearableDictionary::WearableEntry *wearable_entry = LLEditWearableDictionary::getInstance()->getWearable(mType);
	if (!wearable_entry)
	{
		LL_INFOS() << "could not get wearable dictionary entry for wearable type: " << mType << LL_ENDL;
		return;
	}

	if (subpart >= wearable_entry->mSubparts.size())
	{
		LL_INFOS() << "accordion tab expanded for invalid subpart. Wearable type: " << mType << " subpart num: " << subpart << LL_ENDL;
		return;
	}

	ESubpart subpart_e = wearable_entry->mSubparts[subpart];
	const LLEditWearableDictionary::SubpartEntry *subpart_entry = LLEditWearableDictionary::getInstance()->getSubpart(subpart_e);

	if (!subpart_entry)
	{
		LL_WARNS() << "could not get wearable subpart dictionary entry for subpart: " << subpart_e << LL_ENDL;
		return;
	}

	mCurrentSubpart = subpart_e;
	//Update the buttons to reflect the current selected subpart.
	for (U8 index = 0; index < wearable_entry->mSubparts.size(); ++index)
	{
		// dive into data structures to get the panel we need
		ESubpart subpart_e = wearable_entry->mSubparts[index];
		const LLEditWearableDictionary::SubpartEntry *subpart_entry = LLEditWearableDictionary::getInstance()->getSubpart(subpart_e);

		if (subpart_entry)
		{
			if (index < mSubpartBtns.size())
			if (LLButton* btn = mSubpartBtns[index])
			{
				btn->setToggleState(subpart == subpart_e);
			}
		}
	}
	
	// Update the thumbnails we display
	bool editable = updatePermissions();
	value_map_t sorted_params;
	getSortedParams(sorted_params, subpart_entry->mEditGroup, editable);
	buildParamList(mCustomizeFloater->getScrollingPanelList(), sorted_params);
	updateScrollingPanelUI();
	
	// Update the camera
	if (gMorphView)
	{
		gMorphView->setCameraTargetJoint( gAgentAvatarp->getJoint( subpart_entry->mTargetJoint ) );
		gMorphView->setCameraTargetOffset( subpart_entry->mTargetOffset );
		gMorphView->setCameraOffset( subpart_entry->mCameraOffset );
		gMorphView->setCameraDistToDefault();
		if (gAgentCamera.cameraCustomizeAvatar())
		{
			gMorphView->updateCamera();
		}
	}
}

//Singu note: this function was split off from LLPanelEditWearable::changeCamera
// Return true if the current wearable is editable and update state accordingly.
bool LLPanelEditWearable::updatePermissions()
{
	LLViewerWearable* wearable = getWearable();
	LLViewerInventoryItem* item = wearable ? gInventory.getItem(wearable->getItemID()) : NULL;
	U32 perm_mask = wearable ? PERM_NONE : PERM_ALL;
	BOOL is_complete = wearable ? FALSE : TRUE;
	bool can_export = false;
	bool can_import = false;
	if (item)
	{
		perm_mask = item->getPermissions().getMaskOwner();
		is_complete = item->isComplete();
		
		//Singu note: allow to import values over any modifiable wearable (why not?).
		{
			can_import = true;
	
			// Exporting (of slider values) is allowed when the wearable is full perm, and owned by and created by the user.
			// Of course, only modifiable is enough for the user to write down the values and enter them else where... but why make it easy for them to break the ToS.
			if (is_complete &&
				(item->getPermissions().allowExportBy(gAgent.getID(), LFSimFeatureHandler::instance().exportPolicy())))
			{
				can_export = true;
			}
		}
	}
	setUIPermissions(perm_mask, is_complete);
	mCustomizeFloater->childSetEnabled("Export", can_export);
	mCustomizeFloater->childSetEnabled("Import", can_import);
	return (perm_mask & PERM_MODIFY) && is_complete;
}

void LLPanelEditWearable::updateScrollingPanelList()
{
	updateScrollingPanelUI();
}

void LLPanelEditWearable::updateScrollingPanelUI()
{
	LLViewerWearable* wearable = getWearable();

	BOOL is_modifiable = FALSE;
	BOOL is_copyable = FALSE;
	LLViewerInventoryItem* item = wearable ? gInventory.getItem(wearable->getItemID()) : nullptr;
	if (item)
	{
		BOOL is_complete = item->isComplete();
		LLScrollingPanelParam::sUpdateDelayFrames = 0;
		const LLPermissions& perm = item->getPermissions();
		const auto& group_id(gAgent.getGroupID());
		is_modifiable = perm.allowModifyBy(gAgentID, group_id);
		is_copyable = perm.allowCopyBy(gAgentID, group_id);
		mCustomizeFloater->getScrollingPanelList()->updatePanels(is_modifiable && is_complete);
	}

	// Update some UI here instead of the draw call
	bool has_wearable = wearable != nullptr;
	bool max_layers = gAgentWearables.getClothingLayerCount() == LLAgentWearables::MAX_CLOTHING_LAYERS;
	bool show_create_new = !has_wearable && !max_layers;

	if (mTakeOff)
	{
		mTakeOff->setEnabled(has_wearable);
		if (mCanTakeOff) mTakeOff->setVisible(has_wearable);
	}
	if (mCreateNewLayer)
	{
		mCreateNewLayer->setVisible(has_wearable && !max_layers);
	}
	if (mArrowLeft)
	{
		mArrowLeft->setEnabled(has_wearable && gAgentWearables.getBottomWearable(mType) != wearable);
		mArrowLeft->setVisible(has_wearable);
	}
	if (mArrowRight)
	{
		mArrowRight->setEnabled(has_wearable && gAgentWearables.getTopWearable(mType) != wearable);
		mArrowRight->setVisible(has_wearable);
	}
	mCreateNew->setVisible(show_create_new);
	mNotWornI->setVisible(show_create_new);
	mNotWornT->setVisible(show_create_new);
	mNoModI->setVisible(has_wearable && !is_modifiable);
	mNoModT->setVisible(has_wearable && !is_modifiable);
	mPath->setVisible(has_wearable);
	mSquare->setVisible(has_wearable && !is_modifiable); //lock icon

	 // do nothing else if we don't have a valid wearable we're editing
	if (!wearable) return;
	//LL_INFOS() << llformat("%#.8lX", wearable) << LL_ENDL;
	//LL_INFOS() << "cur_wearable->isDirty()=" << wearable->isDirty() << LL_ENDL;
	refreshWearables(false);
}

void LLPanelEditWearable::onBtnTakeOff()
{
	LLViewerWearable* wearable = getWearable();
	if (!wearable)
	{
		return;
	}

	LLAppearanceMgr::instance().removeItemFromAvatar(wearable->getItemID());
	refreshWearables(true);
}

// static
void LLPanelEditWearable::getSortedParams(value_map_t &sorted_params, const std::string &edit_group, bool editable)
{
	if (!getWearable()) return;

	LLViewerWearable::visual_param_vec_t param_list;
	ESex avatar_sex = gAgentAvatarp->getSex();
	
	getWearable()->getVisualParams(param_list);
	
	for (LLViewerWearable::visual_param_vec_t::iterator iter = param_list.begin();
	        iter != param_list.end();
	        ++iter)
	{
		LLViewerVisualParam *param = (LLViewerVisualParam*) *iter;
		
		if (param->getID() == -1 
		        || !param->isTweakable()
		        || param->getEditGroup() != edit_group 
		        || !(param->getSex() & avatar_sex))
		{
			continue;
		}
		
		// negative getDisplayOrder() to make lowest order the highest priority
		value_map_t::value_type vt(-param->getDisplayOrder(), editable_param(editable, param));
		llassert( sorted_params.find(-param->getDisplayOrder()) == sorted_params.end() );  // Check for duplicates
		sorted_params.insert(vt);
	}
}

void LLPanelEditWearable::buildParamList(LLScrollingPanelList *panel_list, value_map_t &sorted_params)
{
	// sorted_params is sorted according to magnitude of effect from
	// least to greatest.  Adding to the front of the child list
	// reverses that order.
	if (panel_list)
	{
		panel_list->clearPanels();
		value_map_t::iterator end = sorted_params.end();
		for(value_map_t::iterator it = sorted_params.begin(); it != end; ++it)
		{
			LLScrollingPanelParam *panel_param = NULL;
			panel_param = new LLScrollingPanelParam( "LLScrollingPanelParam", NULL, (*it).second.second, (*it).second.first, getWearable(), (mType != LLWearableType::WT_PHYSICS));
			panel_list->addPanel( panel_param );
		}
	}
}
	
void LLPanelEditWearable::configureAlphaCheckbox(LLAvatarAppearanceDefines::ETextureIndex te, const std::string& name)
{
	if (LLUICtrl* checkbox = findChild<LLUICtrl>(name))
	{
		checkbox->setCommitCallback(boost::bind(&LLPanelEditWearable::onInvisibilityCommit, this, checkbox, te));
		initPreviousAlphaTextureEntry(te);
		mAlphaCheckbox2Index[checkbox] = te;
	}
}

void LLPanelEditWearable::onInvisibilityCommit(LLUICtrl* ctrl, LLAvatarAppearanceDefines::ETextureIndex te)
{
	if (!ctrl)
		return;
	if (!getWearable())
		return;
	
	LL_INFOS() << "onInvisibilityCommit, self " << this << " ctrl " << ctrl << LL_ENDL;
	
	bool new_invis_state = ctrl->getValue();
	if (new_invis_state)
	{
		LLLocalTextureObject *lto = getWearable()->getLocalTextureObject(te);
		mPreviousAlphaTexture[te] = lto->getID();
		
		LLViewerFetchedTexture* image = LLViewerTextureManager::getFetchedTexture( IMG_INVISIBLE );
		U32 index;
		if (gAgentWearables.getWearableIndex(getWearable(), index))
		{
			gAgentAvatarp->setLocalTexture(te, image, FALSE, index);
			gAgentAvatarp->wearableUpdated(getWearable()->getType(), FALSE);
		}
	}
	else
	{
		// Try to restore previous texture, if any.
		LLUUID prev_id = mPreviousAlphaTexture[te];
		if (prev_id.isNull() || (prev_id == IMG_INVISIBLE))
		{
			prev_id = LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) );
		}
		if (prev_id.isNull())
			return;
		
		LLViewerFetchedTexture* image = LLViewerTextureManager::getFetchedTexture(prev_id);

		if (!image)
			return;
		
		U32 index;
		if (gAgentWearables.getWearableIndex(getWearable(), index))
		{
			gAgentAvatarp->setLocalTexture(te, image, FALSE, index);
			gAgentAvatarp->wearableUpdated(getWearable()->getType(), FALSE);
		}
	}
}

void LLPanelEditWearable::updateAlphaCheckboxes()
{
	if (!getWearable())
		return;
	for(ctrl_texture_index_map_t::iterator iter = mAlphaCheckbox2Index.begin();
			iter != mAlphaCheckbox2Index.end(); ++iter )
	{
		LLAvatarAppearanceDefines::ETextureIndex te = (LLAvatarAppearanceDefines::ETextureIndex)iter->second;
		if (LLUICtrl* ctrl = iter->first)
		{
			ctrl->setValue(!gAgentAvatarp->isTextureVisible(te, getWearable()));
		}
	}
}

void LLPanelEditWearable::initPreviousAlphaTextures()
{
	initPreviousAlphaTextureEntry(TEX_LOWER_ALPHA);
	initPreviousAlphaTextureEntry(TEX_UPPER_ALPHA);
	initPreviousAlphaTextureEntry(TEX_HEAD_ALPHA);
	initPreviousAlphaTextureEntry(TEX_EYES_ALPHA);
	initPreviousAlphaTextureEntry(TEX_HAIR_ALPHA);
}

void LLPanelEditWearable::initPreviousAlphaTextureEntry(LLAvatarAppearanceDefines::ETextureIndex te)
{
	if (!getWearable())
		return;
	LLLocalTextureObject *lto = getWearable()->getLocalTextureObject(te);
	if (lto)
	{
		mPreviousAlphaTexture[te] = lto->getID();
	}
}
