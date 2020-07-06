/**
* @file floaterlocalassetbrowse.cpp
* @brief Local texture support
*
* $LicenseInfo:firstyear=2009&license=viewergpl$
*
* Copyright (c) 2010, author unknown
*
* Imprudence Viewer Source Code
* The source code in this file ("Source Code") is provided to you
* under the terms of the GNU General Public License, version 2.0
* ("GPL"). Terms of the GPL can be found in doc/GPL-license.txt in
* this distribution, or online at
* http://secondlifegrid.net/programs/open_source/licensing/gplv2
*
* There are special exceptions to the terms and conditions of the GPL as
* it is applied to this Source Code. View the full text of the exception
* in the file doc/FLOSS-exception.txt in this software distribution, or
* online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
*
* By copying, modifying or distributing this software, you acknowledge
* that you have read and understood your obligations described above,
* and agree to abide by those obligations.
*
* ALL SOURCE CODE IS PROVIDED "AS IS." THE AUTHOR MAKES NO
* WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
* COMPLETENESS OR PERFORMANCE.
* $/LicenseInfo$
*/

/*

tag: vaa emerald local_asset_browser

this feature is still a work in progress.

*/

/* basic headers */
#include "llviewerprecompiledheaders.h"

/* own class header && upload floater header */
#include "floaterlocalassetbrowse.h"
//#include "floaterlocaluploader.h" <- in development.

/* image compression headers. */
#include "llimagebmp.h"
#include "llimagetga.h"
#include "llimagejpeg.h"
#include "llimagepng.h"

/* llui headers */
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llscrolllistitem.h"
#include "lluictrlfactory.h"

/* misc headers */
#include <time.h>
#include <ctime>
#include "llviewertexturelist.h"
#include "llviewerobjectlist.h"
#include "statemachine/aifilepicker.h"
#include "llviewermenufile.h"
#include "llfloaterimagepreview.h"
#include "llfile.h"
#include "llsdparam.h"
#include "llsdserialize.h"

/* including to force rebakes when needed */
#include "llvoavatarself.h"

/* sculpt refresh */
#include "llvovolume.h"
#include "llface.h"

/* static pointer to self, wai? oh well. */
static FloaterLocalAssetBrowser* sLFInstance = NULL;

/*=======================================*/
/*     Instantiating manager class       */
/*    and formally declaring it's list   */
/*=======================================*/ 
LocalAssetBrowser* gLocalBrowser;
LocalAssetBrowserTimer* gLocalBrowserTimer;
std::vector<LocalBitmap> LocalAssetBrowser::loaded_bitmaps;
bool    LocalAssetBrowser::mLayerUpdated;
bool    LocalAssetBrowser::mSculptUpdated;

/*=======================================*/
/*  LocalBitmap: unit class              */
/*=======================================*/ 
/*
	The basic unit class responsible for
	containing one loaded local texture.
*/

LocalBitmap::Params::Params(const std::string& path)
: fullpath("path", path)
, keep_updating("update", gSavedSettings.getBOOL("LocalBitmapUpdate"))
, type("type", TYPE_TEXTURE)
, id("id", LLUUID::generateNewID())
{
}

LocalBitmap::LocalBitmap(const Params& p)
{
	llassert(!p.fullpath().empty());
	valid = false;
	if ( gDirUtilp->fileExists(p.fullpath) )
	{
		/* taking care of basic properties */
		id            = p.id;
		filename	  = p.fullpath;
		keep_updating = p.keep_updating;
		linkstatus    = keep_updating ? LINK_ON : LINK_OFF;
		shortname     = gDirUtilp->getBaseFileName(filename, true);
		bitmap_type   = p.type;
		sculpt_dirty  = false;
		volume_dirty  = false;

		/* taking care of extension type now to avoid switch madness */
		std::string temp_exten = gDirUtilp->getExtension(filename);

		if (temp_exten == "bmp") extension = IMG_EXTEN_BMP;
		else if (temp_exten == "tga") extension = IMG_EXTEN_TGA;
		else if (temp_exten == "jpg" || temp_exten == "jpeg") extension = IMG_EXTEN_JPG;
		else if (temp_exten == "png") extension = IMG_EXTEN_PNG;
		else return; // no valid extension.

		/* getting file's last modified */

		llstat temp_stat;
		LLFile::stat(filename, &temp_stat);
		std::time_t time = temp_stat.st_mtime;

		last_modified = asctime( localtime(&time) );

		/* checking if the bitmap is valid && decoding if it is */
		LLPointer<LLImageRaw> raw_image = new LLImageRaw;
		if (decodeSelf(raw_image))
		{
			/* creating a shell LLViewerTexture and fusing raw image into it */
			LLViewerFetchedTexture* viewer_image = new LLViewerFetchedTexture( "file://"+filename, FTT_LOCAL_FILE, id, LOCAL_USE_MIPMAPS );
			viewer_image->createGLTexture( LOCAL_DISCARD_LEVEL, raw_image );
			viewer_image->setCachedRawImage(-1,raw_image);

			/* making damn sure gTextureList will not delete it prematurely */
			viewer_image->ref();

			/* finalizing by adding LLViewerTexture instance into gTextureList */
			gTextureList.addImage(viewer_image, TEX_LIST_STANDARD);

			/* filename is valid, bitmap is decoded and valid, i can haz liftoff! */
			valid = true;
			LocalAssetBrowser::add(*this);
		}
	}
}

LocalBitmap::~LocalBitmap()
{
}

/* [maintenence functions] */
void LocalBitmap::updateSelf()
{
	if (linkstatus == LINK_ON || linkstatus == LINK_UPDATING)
	{
		/* making sure file still exists */
		if (!gDirUtilp->fileExists(filename))
		{
			linkstatus = LINK_BROKEN;
			return;
		}

		/* exists, let's check if it's lastmod has changed */
		llstat temp_stat;
		LLFile::stat(filename, &temp_stat);
		std::time_t temp_time = temp_stat.st_mtime;

		LLSD new_last_modified = asctime( localtime(&temp_time) );
		if (last_modified.asString() == new_last_modified.asString()) return;

		/* here we update the image */
		LLPointer<LLImageRaw> new_imgraw = new LLImageRaw;
		if (!decodeSelf(new_imgraw))
		{
			linkstatus = LINK_UPDATING;
			return;
		}
		else
		{
			linkstatus = LINK_ON;
		}

		LLViewerFetchedTexture* image = gTextureList.findImage(id, TEX_LIST_STANDARD);
		if (!image->forSculpt())
			image->createGLTexture(LOCAL_DISCARD_LEVEL, new_imgraw);
		else
			image->setCachedRawImage(-1,new_imgraw);

		/* finalizing by updating lastmod to current */
		last_modified = new_last_modified;

		/* setting unit property to reflect that it has been changed */
		switch (bitmap_type)
		{
			case TYPE_SCULPT:
			{
				 /* sets a bool to run through all visible sculpts in one go, and update the ones necessary. */
				sculpt_dirty = true;
				volume_dirty = true;
				gLocalBrowser->setSculptUpdated( true );
				break;
			}
			case TYPE_LAYER:
			{
				/* sets a bool to rebake layers after the iteration is done with */
				gLocalBrowser->setLayerUpdated( true );
				break;
			}
			case TYPE_TEXTURE:
			default:
				break;
		}
	}

}

bool LocalBitmap::decodeSelf(LLImageRaw* rawimg)
{
	switch (extension)
	{
		case IMG_EXTEN_BMP:
		{
			LLPointer<LLImageBMP> bmp_image = new LLImageBMP;
			if (!bmp_image->load(filename)) break;
			if (!bmp_image->decode(rawimg, 0.0f)) break;

			rawimg->biasedScaleToPowerOfTwo(LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT);
			return true;
		}
		case IMG_EXTEN_TGA:
		{
			LLPointer<LLImageTGA> tga_image = new LLImageTGA;
			if (!tga_image->load(filename)) break;
			if (!tga_image->decode(rawimg)) break;
			if (( tga_image->getComponents() != 3) &&
				( tga_image->getComponents() != 4) )
				break;

			rawimg->biasedScaleToPowerOfTwo(LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT);
			return true;
		}
		case IMG_EXTEN_JPG:
		{
			LLPointer<LLImageJPEG> jpeg_image = new LLImageJPEG;
			if (!jpeg_image->load(filename)) break;
			if (!jpeg_image->decode(rawimg, 0.0f)) break;

			rawimg->biasedScaleToPowerOfTwo(LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT);
			return true;
		}
		case IMG_EXTEN_PNG:
		{
			LLPointer<LLImagePNG> png_image = new LLImagePNG;
			if ( !png_image->load(filename) ) break;
			if ( !png_image->decode(rawimg, 0.0f) ) break;

			rawimg->biasedScaleToPowerOfTwo(LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT);
			return true;
		}
		default:
			break;
	}
	return false;
}

void LocalBitmap::setUpdateBool()
{
	if (linkstatus != LINK_BROKEN)
	{
		if (!keep_updating)
		{
			linkstatus = LINK_ON;
			keep_updating = true;
		}
		else
		{
			linkstatus = LINK_OFF;
			keep_updating = false;
		}
	}
	else
	{
		keep_updating = false;
	}
	gSavedSettings.setBOOL("LocalBitmapUpdate", keep_updating);
}

void LocalBitmap::setType( S32 type )
{
	bitmap_type = type;
}

void LocalBitmap::setID(const LLUUID& uuid)
{
	LLViewerFetchedTexture* image = gTextureList.findImage(id, TEX_LIST_STANDARD);
	gTextureList.deleteImage(image);
	id = uuid;
	image->setID(id);
	gTextureList.addImage(image, TEX_LIST_STANDARD);
}

/* [information query functions] */
std::string LocalBitmap::getShortName() const
{
	return shortname;
}

std::string LocalBitmap::getFileName() const
{
	return filename;
}

LLUUID LocalBitmap::getID() const
{
	return id;
}

LLSD LocalBitmap::getLastModified() const
{
	return last_modified;
}

std::string LocalBitmap::getLinkStatus() const
{
	switch(linkstatus)
	{
		case LINK_ON: return "On";
		case LINK_OFF: return "Off";
		case LINK_BROKEN: return "Broken";
		case LINK_UPDATING: return "Updating";
		default: return "Unknown";
	}
}

bool LocalBitmap::getUpdateBool() const
{
	return keep_updating;
}

bool LocalBitmap::getIfValidBool() const
{
	return valid;
}

S32 LocalBitmap::getType() const
{
	return bitmap_type;
}

std::vector<LLFace*> LocalBitmap::getFaceUsesThis(LLDrawable* drawable) const
{
	std::vector<LLFace*> matching_faces;

	for ( S32 face_iter = 0; face_iter <= drawable->getNumFaces(); face_iter++ )
	{
		LLFace* newface = drawable->getFace(face_iter);

		if (id == newface->getTexture()->getID())
			matching_faces.push_back(newface);
	}

	return matching_faces;
}

std::vector<affected_object> LocalBitmap::getUsingObjects(bool seek_by_type, bool seek_textures, bool seek_sculptmaps) const
{
	std::vector<affected_object> affected_vector;
	
	for (std::vector< LLPointer<LLViewerObject> >::iterator  iter = gObjectList.mObjects.begin();
		 iter != gObjectList.mObjects.end(); iter++ )
	{
		LLViewerObject* obj = *iter;

		if (obj->isDead())
		{
			continue;
		}

		affected_object shell;
		shell.object = obj;
		shell.local_sculptmap = false;
		bool obj_relevant = false;

		if ( obj && obj->mDrawable )
		{
			/* looking for textures */
			if (seek_textures || (seek_by_type && bitmap_type == TYPE_TEXTURE))
			{
				std::vector<LLFace*> affected_faces = getFaceUsesThis(obj->mDrawable);
				if (!affected_faces.empty())
				{
					shell.face_list = affected_faces;
					obj_relevant = true;
				}
			}

			/* looking for sculptmaps */
			if ((seek_sculptmaps || (seek_by_type && bitmap_type == TYPE_SCULPT))
				   && obj->isSculpted() && obj->getVolume()
				   && id == obj->getVolume()->getParams().getSculptID())
			{ 
				shell.local_sculptmap = true;
				obj_relevant = true;
			}
		}

		if (obj_relevant) affected_vector.push_back(shell);
	}

	return affected_vector;
}

void LocalBitmap::getDebugInfo() const
{
	/* debug function: dumps everything human readable into llinfos */
	LL_INFOS() << "===[local bitmap debug]==="               << '\n'
			<< "path: "          << filename        << '\n'
			<< "name: "          << shortname       << '\n'
			<< "extension: "     << extension       << '\n'
			<< "uuid: "          << id              << '\n'
			<< "last modified: " << last_modified   << '\n'
			<< "link status: "   << getLinkStatus() << '\n'
			<< "keep updated: "  << keep_updating   << '\n'
			<< "type: "          << bitmap_type     << '\n'
			<< "is valid: "      << valid           << '\n'
			<< "=========================="               << LL_ENDL;
}

LLSD LocalBitmap::asLLSD() const
{
	return LLSD()
		.with("path", filename)
		.with("id", id)
		.with("update", keep_updating)
		.with("type", bitmap_type);
}

/*=======================================*/
/*  LocalAssetBrowser: internal class  */
/*=======================================*/ 
/*
	Responsible for internal workings.
	Instantiated at the top of the source file.
	Sits in memory until the viewer is closed.
*/

const std::string LocalAssetBrowser::getFileName() const
{
	return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "local_assets.xml");
}

LocalAssetBrowser::LocalAssetBrowser()
{
	gLocalBrowser = this;
	mLayerUpdated = false;
	mSculptUpdated = false;

	// Load bitmaps
	llifstream file(getFileName());
	if (!file) return;
	LLSD saved_assets;
	LLSDSerialize::fromXML(saved_assets, file);
	file.close();
	for (auto it = saved_assets.beginArray(), end = saved_assets.endArray(); it < end; ++it)
	{
		const auto&& p = LLSDParamAdapter<LocalBitmap::Params>(*it);
		LocalBitmap bm(p); // Creating one adds it to the list
	}

	if (!loaded_bitmaps.empty()) PingTimer();
}

LocalAssetBrowser::~LocalAssetBrowser()
{
	// Save bitmaps
	llofstream file(getFileName());
	if (!file)
	{
		LL_WARNS() << "Could not open file " << getFileName() << " for saving." << LL_ENDL;
		return;
	}
	LLSD saved_assets(LLSD::emptyArray());
	for (const auto& bitmap : loaded_bitmaps)
		saved_assets.append(bitmap.asLLSD());
	LLSDSerialize::toPrettyXML(saved_assets, file);
	file.close();
	gLocalBrowser = nullptr;
}

void LocalAssetBrowser::AddBitmap()
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(FFLOAD_IMAGE, "", "image", true);
	filepicker->run(boost::bind(&LocalAssetBrowser::AddBitmap_continued, filepicker));
}

void LocalAssetBrowser::AddBitmap_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
		return;

	bool change_happened = false;

	for(const auto& filename : filepicker->getFilenames())
		if (LocalBitmap(filename).getIfValidBool())
			change_happened = true;

	if (change_happened) onChangeHappened();
}
void LocalAssetBrowser::DelBitmap( std::vector<LLScrollListItem*> delete_vector, S32 column )
{
	bool change_happened = false;
	for( std::vector<LLScrollListItem*>::iterator list_iter = delete_vector.begin();
		 list_iter != delete_vector.end(); list_iter++ )
	{
		LLScrollListItem* list_item = *list_iter; 
		if ( list_item )
		{
			LLUUID id = list_item->getColumn(column)->getValue().asUUID();
			for (local_list_iter iter = loaded_bitmaps.begin();
				 iter != loaded_bitmaps.end();)
			{
				if ((*iter).getID() == id)
				{	
					LLViewerFetchedTexture* image = gTextureList.findImage(id, TEX_LIST_STANDARD);
					gTextureList.deleteImage( image );
					image->unref();
					iter = loaded_bitmaps.erase(iter);
					change_happened = true;
				}
				else
					++iter;
			}
		}
	}

	if (change_happened) onChangeHappened();
}

void LocalAssetBrowser::onUpdateBool(LLUUID id)
{
	if (LocalBitmap* unit = GetBitmapUnit(id))
	{ 
		unit->setUpdateBool(); 
		PingTimer();
	}
}

void LocalAssetBrowser::onSetType(LLUUID id, S32 type)
{
	if (LocalBitmap* unit = GetBitmapUnit(id)) unit->setType(type);
}

LocalBitmap* LocalAssetBrowser::GetBitmapUnit(LLUUID id)
{
	for (local_list_iter iter = loaded_bitmaps.begin(); iter != loaded_bitmaps.end(); ++iter)
		if ((*iter).getID() == id)
			return &(*iter);
	return NULL;
}

bool LocalAssetBrowser::IsDoingUpdates()
{
	for (local_list_iter iter = loaded_bitmaps.begin(); iter != loaded_bitmaps.end(); iter++)
	{ 
		if ((*iter).getUpdateBool()) return true; /* if at least one unit in the list needs updates - we need a timer. */
	}

	return false;
}


/* Reaction to a change in bitmaplist, this function finds a texture picker floater's appropriate scrolllist
   and passes this scrolllist's pointer to UpdateTextureCtrlList for processing.
   it also processes timer start/stops as needed */
void LocalAssetBrowser::onChangeHappened()
{
	/* own floater update */
	if (sLFInstance) sLFInstance->UpdateBitmapScrollList();

	/* texturepicker related */
	const LLView::child_list_t* child_list = gFloaterView->getChildList();
	LLView::child_list_const_iter_t child_list_iter = child_list->begin();

	for (; child_list_iter != child_list->end(); child_list_iter++)
	{
		LLView* view = *child_list_iter;
		if ( view->getName() == LOCAL_TEXTURE_PICKER_NAME )
		{
			LLScrollListCtrl* ctrl = view->getChild<LLScrollListCtrl>
									( LOCAL_TEXTURE_PICKER_LIST_NAME, 
									  LOCAL_TEXTURE_PICKER_RECURSE, 
									  LOCAL_TEXTURE_PICKER_CREATEIFMISSING );

			if ( ctrl ) { UpdateTextureCtrlList(ctrl); }
		}
	}

	/* poking timer to see if it's still needed/still not needed */
	PingTimer();

}

void LocalAssetBrowser::PingTimer()
{
	if (!loaded_bitmaps.empty() && IsDoingUpdates())
	{
		if (!gLocalBrowserTimer)
			gLocalBrowserTimer = new LocalAssetBrowserTimer();
		if (!gLocalBrowserTimer->isRunning())
			gLocalBrowserTimer->start();
	}
	else if (gLocalBrowserTimer && gLocalBrowserTimer->isRunning())
	{
		gLocalBrowserTimer->stop();
	}
}

/* This function refills the texture picker floater's scrolllist with the updated contents of bitmaplist */
void LocalAssetBrowser::UpdateTextureCtrlList(LLScrollListCtrl* ctrl)
{
	if (ctrl) // checking again in case called externally for some silly reason.
	{
		ctrl->clearRows(); 
		for (const auto& bitmap : loaded_bitmaps)
		{
			auto row = LLScrollListItem::Params();
			row.value(bitmap.id);
			row.columns.add(LLScrollListCell::Params()
				.column("unit_name")
				.type("text")
				.value(bitmap.shortname));

			row.columns.add(LLScrollListCell::Params()
				.column("unit_id_HIDDEN")
				.type("text")
				.value(bitmap.id));

			ctrl->addRow(row);
		}
	}
}

void LocalAssetBrowser::PerformTimedActions()
{
	// perform checking if updates are needed && update if so.
	for (local_list_iter iter = loaded_bitmaps.begin(); iter != loaded_bitmaps.end(); ++iter)
	{
		(*iter).updateSelf();
		// one or more sculpts have been updated, refreshing them.
		if (mSculptUpdated && (*iter).sculpt_dirty)
		{
			PerformSculptUpdates(*iter);
			(*iter).sculpt_dirty = false;
		}
	}
	mSculptUpdated = false;

	// one of the layer bitmaps has been updated, we need to rebake.
	if (mLayerUpdated)
	{
		if (isAgentAvatarValid())
			gAgentAvatarp->forceBakeAllTextures(SLAM_FOR_DEBUG);
		mLayerUpdated = false;
	}
}

void LocalAssetBrowser::PerformSculptUpdates(LocalBitmap& unit)
{
	/* looking for sculptmap using objects only */
	std::vector<affected_object> object_list = unit.getUsingObjects(false, false, true);
	if (object_list.empty()) { return; }

	for( std::vector<affected_object>::iterator iter = object_list.begin();
		 iter != object_list.end(); iter++ )
	{
		affected_object aobj = *iter;
		if (aobj.object)
		{
			if (!aobj.local_sculptmap) continue; // should never get here. only in case of misuse.
			// update code [begin]
			if (unit.volume_dirty)
			{
				LLImageRaw* rawimage = gTextureList.findImage(unit.getID(), TEX_LIST_STANDARD)->getCachedRawImage();

				aobj.object->getVolume()->sculpt(rawimage->getWidth(), rawimage->getHeight(), rawimage->getComponents(), rawimage->getData(), 0, true);
				unit.volume_dirty = false;
			}

			// tell affected drawable it's got updated
			aobj.object->mDrawable->getVOVolume()->setSculptChanged(true);
			aobj.object->mDrawable->getVOVolume()->markForUpdate(true);
			// update code [end]
		}
	}
}

/*==================================================*/
/*  FloaterLocalAssetBrowser : floater class      */
/*==================================================*/ 
/*
	Responsible for talking to the user.
	Instantiated by user request.
	Destroyed when the floater is closed.

*/
FloaterLocalAssetBrowser::FloaterLocalAssetBrowser()
:   LLFloater(std::string("local_bitmap_browser_floater"))
{
	// xui creation:
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_local_asset_browse.xml");
	
	// setting element/xui children:
	mAddBtn         = getChild<LLButton>("add_btn");
	mDelBtn         = getChild<LLButton>("del_btn");
	mMoreBtn        = getChild<LLButton>("more_btn");
	mLessBtn        = getChild<LLButton>("less_btn");
	mUploadBtn      = getChild<LLButton>("upload_btn");

	mBitmapList     = getChild<LLScrollListCtrl>("bitmap_list");
	mTextureView    = getChild<LLTextureCtrl>("texture_view");
	mUpdateChkBox   = getChild<LLCheckBoxCtrl>("keep_updating_checkbox");

	mPathTxt            = getChild<LLLineEditor>("path_text");
	mUUIDTxt            = getChild<LLLineEditor>("uuid_text");
	mNameTxt            = getChild<LLLineEditor>("name_text");

	mLinkTxt		    = getChild<LLTextBox>("link_text");
	mTimeTxt		    = getChild<LLTextBox>("time_text");
	mTypeComboBox       = getChild<LLComboBox>("type_combobox");

	mCaptionPathTxt     = getChild<LLTextBox>("path_caption_text");
	mCaptionUUIDTxt     = getChild<LLTextBox>("uuid_caption_text");
	mCaptionLinkTxt     = getChild<LLTextBox>("link_caption_text");
	mCaptionNameTxt     = getChild<LLTextBox>("name_caption_text");
	mCaptionTimeTxt	    = getChild<LLTextBox>("time_caption_text");

	// pre-disabling line editors, they're for view only and buttons that shouldn't be on on-spawn.
	mPathTxt->setEnabled( false );
	mUUIDTxt->setEnabled( false );
	mNameTxt->setEnabled( false );

	mDelBtn->setEnabled( false );
	mUploadBtn->setEnabled( false );

	// Initialize visibility
	FloaterResize(true);

	// setting button callbacks:
	mAddBtn->setClickedCallback(boost::bind(&FloaterLocalAssetBrowser::onClickAdd, this));
	mDelBtn->setClickedCallback(boost::bind(&FloaterLocalAssetBrowser::onClickDel, this));
	mMoreBtn->setClickedCallback(boost::bind(&FloaterLocalAssetBrowser::FloaterResize, this, true));
	mLessBtn->setClickedCallback(boost::bind(&FloaterLocalAssetBrowser::FloaterResize, this, false));
	mUploadBtn->setClickedCallback(boost::bind(&FloaterLocalAssetBrowser::onClickUpload, this));
	
	// combo callback
	mTypeComboBox->setCommitCallback(boost::bind(&FloaterLocalAssetBrowser::onCommitTypeCombo,this));

	// scrolllist callbacks
	mBitmapList->setCommitCallback(boost::bind(&FloaterLocalAssetBrowser::onChooseBitmapList,this));

	// checkbox callbacks
	mUpdateChkBox->setCommitCallback(boost::bind(&FloaterLocalAssetBrowser::onClickUpdateChkbox,this));

	// Allow changing the ID!
	mUUIDTxt->setCommitCallback(boost::bind(&FloaterLocalAssetBrowser::onUpdateID, this, _2));
	mUUIDTxt->setCommitOnReturn(true);
	mUUIDTxt->setCommitOnFocusLost(true);
}

void FloaterLocalAssetBrowser::show(void*)
{
	if (!sLFInstance)
		sLFInstance = new FloaterLocalAssetBrowser();
	sLFInstance->open();
	sLFInstance->UpdateBitmapScrollList();
}

FloaterLocalAssetBrowser::~FloaterLocalAssetBrowser()
{
	sLFInstance = NULL;
}

void FloaterLocalAssetBrowser::onClickAdd()
{	
	gLocalBrowser->AddBitmap();
}

void FloaterLocalAssetBrowser::onClickDel()
{
	gLocalBrowser->DelBitmap(mBitmapList->getAllSelected());
}

void FloaterLocalAssetBrowser::onClickUpload()
{
	std::string filename = gLocalBrowser->GetBitmapUnit( 
		(LLUUID)mBitmapList->getSelectedItemLabel(BITMAPLIST_COL_ID) )->getFileName();

	if ( !filename.empty() )
	{
		LLFloaterImagePreview* floaterp = new LLFloaterImagePreview(filename);
		LLUICtrlFactory::getInstance()->buildFloater(floaterp, "floater_image_preview.xml");
	}
}

void FloaterLocalAssetBrowser::onChooseBitmapList()
{
	bool button_status = !mBitmapList->isEmpty() && mBitmapList->getFirstSelected();
	mDelBtn->setEnabled(button_status);
	mUploadBtn->setEnabled(button_status);

	UpdateRightSide();
}

void FloaterLocalAssetBrowser::onClickUpdateChkbox()
{
	std::string temp_str = mBitmapList->getSelectedItemLabel(BITMAPLIST_COL_ID);
	if ( !temp_str.empty() )
	{
		gLocalBrowser->onUpdateBool( (LLUUID)temp_str );
		UpdateRightSide();
	}
}

void FloaterLocalAssetBrowser::onCommitTypeCombo()
{
	std::string temp_str = mBitmapList->getSelectedItemLabel(BITMAPLIST_COL_ID);

	if (!temp_str.empty())
	{
		S32 selection = mTypeComboBox->getCurrentIndex();
		gLocalBrowser->onSetType((LLUUID)temp_str, selection);
	}
}

void FloaterLocalAssetBrowser::onUpdateID(const LLSD& val)
{
	const auto& id = val.asUUID();
	const auto& col = mBitmapList->getFirstSelected()->getColumn(BITMAPLIST_COL_ID);
	const auto& old_id = col->getValue().asString();
	if (id.isNull() && id.asString() != old_id) // Just reset if invalid or unchanged
		mUUIDTxt->setValue(old_id);
	else
	{
		gLocalBrowser->GetBitmapUnit(LLUUID(old_id))->setID(id);
		mTextureView->setImageAssetID(id);
		col->setValue(id);
	}
}

void FloaterLocalAssetBrowser::FloaterResize(bool expand)
{
	mMoreBtn->setVisible(!expand);
	mLessBtn->setVisible(expand);
	mTextureView->setVisible(expand);
	mUpdateChkBox->setVisible(expand);
	mCaptionPathTxt->setVisible(expand);
	mCaptionUUIDTxt->setVisible(expand);
	mCaptionLinkTxt->setVisible(expand);
	mCaptionNameTxt->setVisible(expand);
	mCaptionTimeTxt->setVisible(expand);
	mTypeComboBox->setVisible(expand);

	mTimeTxt->setVisible(expand);
	mPathTxt->setVisible(expand);
	mUUIDTxt->setVisible(expand);
	mLinkTxt->setVisible(expand);
	mNameTxt->setVisible(expand);

	reshape(expand ? LF_FLOATER_EXPAND_WIDTH : LF_FLOATER_CONTRACT_WIDTH, LF_FLOATER_HEIGHT);
	setResizeLimits(expand ? LF_FLOATER_EXPAND_WIDTH : LF_FLOATER_CONTRACT_WIDTH, LF_FLOATER_HEIGHT);
	if (expand) UpdateRightSide();
}

void FloaterLocalAssetBrowser::UpdateBitmapScrollList()
{
	mBitmapList->clearRows();
	if (!gLocalBrowser->loaded_bitmaps.empty())
	{
		LocalAssetBrowser::local_list_iter iter;
		for(iter = gLocalBrowser->loaded_bitmaps.begin(); iter != gLocalBrowser->loaded_bitmaps.end(); iter++)
		{
			LLSD element;
			element["columns"][BITMAPLIST_COL_NAME]["column"] = "bitmap_name";
			element["columns"][BITMAPLIST_COL_NAME]["type"]   = "text";
			element["columns"][BITMAPLIST_COL_NAME]["value"]  = (*iter).getShortName();

			element["columns"][BITMAPLIST_COL_ID]["column"] = "bitmap_uuid";
			element["columns"][BITMAPLIST_COL_ID]["type"]   = "text";
			element["columns"][BITMAPLIST_COL_ID]["value"]  = (*iter).getID();

			mBitmapList->addElement(element);
		}

	}
	onChooseBitmapList();
}

void FloaterLocalAssetBrowser::UpdateRightSide()
{
	/*
	Since i'm not keeping a bool on if the floater is expanded or not, i'll
	just check if one of the widgets that shows when the floater is expanded is visible.
	Also obviously before updating - checking if something IS actually selected :o
	*/
	if (!mTextureView->getVisible()) return;

	if (const auto& selected = mBitmapList->getFirstSelected())
	{
		LocalBitmap* unit = gLocalBrowser->GetBitmapUnit(LLUUID(selected->getColumn(BITMAPLIST_COL_ID)->getValue().asString()));
		if ( unit )
		{
			mTextureView->setImageAssetID(unit->getID());
			mUpdateChkBox->set(unit->getUpdateBool());
			mPathTxt->setText(unit->getFileName());
			mUUIDTxt->setText(unit->getID().asString());
			mNameTxt->setText(unit->getShortName());
			mTimeTxt->setText(unit->getLastModified().asString());
			mLinkTxt->setText(unit->getLinkStatus());
			mTypeComboBox->selectNthItem(unit->getType());

			mTextureView->setEnabled(true);
			mUpdateChkBox->setEnabled(true);
			mTypeComboBox->setEnabled(true);
			mUUIDTxt->setEnabled(true);
		}
	}
	else
	{
		mTextureView->setImageAssetID(NO_IMAGE);
		mTextureView->setEnabled(false);
		mUpdateChkBox->set(false);
		mUpdateChkBox->setEnabled(false);

		mTypeComboBox->selectFirstItem();
		mTypeComboBox->setEnabled(false);
		mUUIDTxt->setEnabled(false);

		const auto none = LLStringExplicit("None");
		mPathTxt->setText(none);
		mUUIDTxt->setText(none);
		mNameTxt->setText(none);
		mLinkTxt->setText(none);
		mTimeTxt->setText(none);
	}
}


/*==================================================*/
/*     LocalAssetBrowserTimer: timer class          */
/*==================================================*/ 
/*
	A small, simple timer class inheriting from
	LLEventTimer, responsible for pinging the
	LocalAssetBrowser class to perform it's
	updates / checks / etc.

*/

LocalAssetBrowserTimer::LocalAssetBrowserTimer() : LLEventTimer( (F32)TIMER_HEARTBEAT )
{

}

LocalAssetBrowserTimer::~LocalAssetBrowserTimer()
{

}

BOOL LocalAssetBrowserTimer::tick()
{
	gLocalBrowser->PerformTimedActions();
	return FALSE;
}

void LocalAssetBrowserTimer::start()
{
	mEventTimer.start();
}

void LocalAssetBrowserTimer::stop()
{
	mEventTimer.stop();
}

bool LocalAssetBrowserTimer::isRunning() const
{
	return mEventTimer.getStarted();
}

