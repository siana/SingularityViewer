/** 
 * @file llfloaternamedesc.cpp
 * @brief LLFloaterNameDesc class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
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

#include "llfloaternamedesc.h"

// project includes
#include "lllineeditor.h"
#include "llresmgr.h"
#include "lltextbox.h"
#include "llbutton.h"
#include "llviewerwindow.h"
#include "llfocusmgr.h"
#include "llradiogroup.h"
#include "lldbstrings.h"
#include "lldir.h"
#include "llfloaterperms.h"
#include "llviewercontrol.h"
#include "llviewermenufile.h"	// upload_new_resource()
#include "lluictrlfactory.h"
#include "llstring.h"

// linden includes
#include "llassetstorage.h"
#include "llinventorytype.h"
#include "llagentbenefits.h"

#include "hippogridmanager.h"

const S32 PREVIEW_LINE_HEIGHT = 19;
const S32 PREVIEW_CLOSE_BOX_SIZE = 16;
const S32 PREVIEW_BORDER_WIDTH = 2;
const S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) + PREVIEW_BORDER_WIDTH;
const S32 PREVIEW_VPAD = 2;
const S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
const S32 PREVIEW_HEADER_SIZE = 3 * PREVIEW_LINE_HEIGHT + PREVIEW_VPAD;
const S32 PREF_BUTTON_WIDTH = 64;
const S32 PREF_BUTTON_HEIGHT = 16;

//-----------------------------------------------------------------------------
// LLFloaterNameDesc()
//-----------------------------------------------------------------------------
LLFloaterNameDesc::LLFloaterNameDesc(const LLSD& filename, void* item )
	: LLFloater(std::string("Name/Description Floater")),
	// <edit>
	  mItem(item),
	// </edit>
	  mIsAudio(FALSE)
{
	mFilenameAndPath = filename.asString();
	mFilename = gDirUtilp->getBaseFileName(mFilenameAndPath, false);
}

//-----------------------------------------------------------------------------
// postBuild()
//-----------------------------------------------------------------------------
BOOL LLFloaterNameDesc::postBuild()
{
	LLRect r;

	std::string asset_name = mFilename;
	LLStringUtil::replaceNonstandardASCII( asset_name, '?' );
	LLStringUtil::replaceChar(asset_name, '|', '?');
	LLStringUtil::stripNonprintable(asset_name);
	LLStringUtil::trim(asset_name);

	asset_name = gDirUtilp->getBaseFileName(asset_name, true); // no extsntion

	setTitle(mFilename);

	centerWithin(gViewerWindow->getRootView()->getRect());

	S32 line_width = getRect().getWidth() - 2 * PREVIEW_HPAD;
	S32 y = getRect().getHeight() - PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize( PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT );
	y -= PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize( PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT );    

	getChild<LLUICtrl>("name_form")->setCommitCallback(boost::bind(&LLFloaterNameDesc::doCommit, this));
	getChild<LLUICtrl>("name_form")->setValue(LLSD(asset_name));

	LLLineEditor *NameEditor = getChild<LLLineEditor>("name_form");
	if (NameEditor)
	{
		NameEditor->setMaxTextLength(DB_INV_ITEM_NAME_STR_LEN);
		NameEditor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	y -= llfloor(PREVIEW_LINE_HEIGHT * 1.2f);
	y -= PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize( PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT );  
	getChild<LLUICtrl>("description_form")->setCommitCallback(boost::bind(&LLFloaterNameDesc::doCommit, this));
	LLLineEditor *DescEditor = getChild<LLLineEditor>("description_form");
	if (DescEditor)
	{
		DescEditor->setMaxTextLength(DB_INV_ITEM_DESC_STR_LEN);
		DescEditor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	y -= llfloor(PREVIEW_LINE_HEIGHT * 1.2f);

	// Cancel button
	getChild<LLUICtrl>("cancel_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnCancel, this));

	S32 expected_upload_cost = getExpectedUploadCost();
	getChild<LLUICtrl>("ok_btn")->setLabelArg("[UPLOADFEE]", llformat("%s%d", gHippoGridManager->getConnectedGrid()->getCurrencySymbol().c_str(), expected_upload_cost));

	setDefaultBtn("ok_btn");

	return TRUE;
}

S32 LLFloaterNameDesc::getExpectedUploadCost() const
{
    std::string exten = gDirUtilp->getExtension(mFilename);
	LLAssetType::EType asset_type = exten == "wav" ? LLAssetType::AT_SOUND
		: (exten == "anim" || exten == "bvh") ? LLAssetType::AT_ANIMATION
		: exten != "lsl" ? LLAssetType::AT_TEXTURE
		: LLAssetType::AT_NONE;
	S32 upload_cost = -1;

	if (asset_type != LLAssetType::AT_NONE)
	{
		if (!LLAgentBenefitsMgr::current().findUploadCost(asset_type, upload_cost))
		{
			LL_WARNS() << "Unable to find upload cost for asset type " << asset_type << LL_ENDL;
		}
	}
	/*else
	{
		LL_WARNS() << "Unable to find upload cost for " << mFilename << LL_ENDL;
	}*/
	return upload_cost;
}

//-----------------------------------------------------------------------------
// LLFloaterNameDesc()
//-----------------------------------------------------------------------------
LLFloaterNameDesc::~LLFloaterNameDesc()
{
	gFocusMgr.releaseFocusIfNeeded( this ); // calls onCommit()
}

// Sub-classes should override this function if they allow editing
//-----------------------------------------------------------------------------
// onCommit()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::onCommit()
{
}

//-----------------------------------------------------------------------------
// onCommit()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::doCommit()
{
	onCommit();
}

//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::onBtnOK()
{
	getChildView("ok_btn")->setEnabled(FALSE); // don't allow inadvertent extra uploads
	
	LLAssetStorage::LLStoreAssetCallback callback = NULL;
	S32 expected_upload_cost = getExpectedUploadCost();

	void *nruserdata = NULL;
	std::string display_name = LLStringUtil::null;

	upload_new_resource(mFilenameAndPath, // file
			    getChild<LLUICtrl>("name_form")->getValue().asString(),
			    getChild<LLUICtrl>("description_form")->getValue().asString(),
			    0, LLFolderType::FT_NONE, LLInventoryType::IT_NONE,
			    LLFloaterPerms::getNextOwnerPerms("Uploads"),
			    LLFloaterPerms::getGroupPerms("Uploads"),
			    LLFloaterPerms::getEveryonePerms("Uploads"),
			    display_name, callback, expected_upload_cost, nruserdata);
	close(false);
}

//-----------------------------------------------------------------------------
// onBtnCancel()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::onBtnCancel()
{
	close(false);
}


//-----------------------------------------------------------------------------
// LLFloaterSoundPreview()
//-----------------------------------------------------------------------------

LLFloaterSoundPreview::LLFloaterSoundPreview(const LLSD& filename, void* item )
	: LLFloaterNameDesc(filename, item)
{
	mIsAudio = TRUE;
}

BOOL LLFloaterSoundPreview::postBuild()
{
	if (!LLFloaterNameDesc::postBuild())
	{
		return FALSE;
	}
	getChild<LLUICtrl>("ok_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnOK, this));
	return TRUE;
}


//-----------------------------------------------------------------------------
// LLFloaterAnimPreview()
//-----------------------------------------------------------------------------

LLFloaterAnimPreview::LLFloaterAnimPreview(const LLSD& filename, void* item )
	: LLFloaterNameDesc(filename, item)
{
}

BOOL LLFloaterAnimPreview::postBuild()
{
	if (!LLFloaterNameDesc::postBuild())
	{
		return FALSE;
	}
	getChild<LLUICtrl>("ok_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnOK, this));
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLFloaterScriptPreview()
//-----------------------------------------------------------------------------

LLFloaterScriptPreview::LLFloaterScriptPreview(const LLSD& filename, void* item )
	: LLFloaterNameDesc(filename, item)
{
	mIsText = TRUE;
}

BOOL LLFloaterScriptPreview::postBuild()
{
	if (!LLFloaterNameDesc::postBuild())
	{
		return FALSE;
	}
	getChild<LLUICtrl>("ok_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnOK, this));
	return TRUE;
}
