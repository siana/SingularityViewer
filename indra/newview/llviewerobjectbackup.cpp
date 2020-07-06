/** 
 * @file llviewerobjectbackup.cpp
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
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

#include <iostream>
#include <fstream>
#include <sstream>

#include "llcallbacklist.h"
#include "lldir.h"
#include "llhttpclient.h"
#include "llinventorydefines.h"
#include "llimagej2c.h"
#include "lllfsthread.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "lltransactiontypes.h"
#include "lluictrlfactory.h"
#include "lluploaddialog.h"

#include "llagent.h"
#include "llappviewer.h" 
#include "llassetuploadresponders.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"	// for gInventory
#include "llmaterialmgr.h"
#include "llselectmgr.h"
#include "lltexturecache.h"
#include "lltoolplacer.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerobjectlist.h"
#include "llviewermenu.h"
#include "lfsimfeaturehandler.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"

#include "llviewerobjectbackup.h"

// Note: these default textures are initialized with hard coded values to
// prevent cheating. When not in SL, the user-configurable values are used
// instead (see setDefaultTextures() below).
LLUUID LL_TEXTURE_PLYWOOD        = LLUUID("89556747-24cb-43ed-920b-47caed15465f");
LLUUID LL_TEXTURE_BLANK          = LLUUID("5748decc-f629-461c-9a36-a35a221fe21f");
LLUUID LL_TEXTURE_INVISIBLE      = LLUUID("38b86f85-2575-52a9-a531-23108d8da837");
LLUUID LL_TEXTURE_TRANSPARENT    = LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903");
LLUUID LL_TEXTURE_MEDIA          = LLUUID("8b5fec65-8d8d-9dc5-cda8-8fdf2716e361");

class ImportObjectResponder: public LLNewAgentInventoryResponder
{
protected:
	LOG_CLASS(ImportObjectResponder);

public:
	ImportObjectResponder(const LLSD& post_data, const LLUUID& vfile_id,
						  LLAssetType::EType asset_type)
	: LLNewAgentInventoryResponder(post_data, vfile_id, asset_type)
	{
	}

	//virtual 
	virtual void uploadComplete(const LLSD& content)
	{
		LLObjectBackup* self = LLObjectBackup::findInstance();
		if (!self)
		{
			LL_WARNS() << "Import aborted, LLObjectBackup instance gone !"
					<< LL_ENDL;
			// remove the "Uploading..." message
			LLUploadDialog::modalUploadFinished();
			return;
		}

		LLAssetType::EType asset_type = LLAssetType::lookup(mPostData["asset_type"].asString());
		LLInventoryType::EType inv_type = LLInventoryType::lookup(mPostData["inventory_type"].asString());

		// Update L$ and ownership credit information
		// since it probably changed on the server
		if (asset_type == LLAssetType::AT_TEXTURE ||
			asset_type == LLAssetType::AT_SOUND ||
			asset_type == LLAssetType::AT_ANIMATION)
		{
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_MoneyBalanceRequest);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_MoneyData);
			msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null);
			gAgent.sendReliableMessage();
		}

		// Actually add the upload to viewer inventory
		LL_INFOS() << "Adding " << content["new_inventory_item"].asUUID() << " "
				<< content["new_asset"].asUUID() << " to inventory." << LL_ENDL;
		if (mPostData["folder_id"].asUUID().notNull())
		{
			LLPermissions perm;
			U32 next_owner_perm;
			perm.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
			if (mPostData["inventory_type"].asString() == "snapshot")
			{
				next_owner_perm = PERM_ALL;
			}
			else
			{
				next_owner_perm = PERM_MOVE | PERM_TRANSFER;
			}
			perm.initMasks(PERM_ALL, PERM_ALL, PERM_NONE, PERM_NONE, next_owner_perm);
			S32 creation_date_now = time_corrected();
			LLPointer<LLViewerInventoryItem> item;
			item = new LLViewerInventoryItem(content["new_inventory_item"].asUUID(),
										mPostData["folder_id"].asUUID(),
										perm,
										content["new_asset"].asUUID(),
										asset_type, inv_type,
										mPostData["name"].asString(),
										mPostData["description"].asString(),
										LLSaleInfo::DEFAULT,
										LLInventoryItemFlags::II_FLAGS_NONE,
										creation_date_now);
			gInventory.updateItem(item);
			gInventory.notifyObservers();
		}
		else
		{
			LL_WARNS() << "Can't find a folder to put it into" << LL_ENDL;
		}

		// remove the "Uploading..." message
		LLUploadDialog::modalUploadFinished();

		self->updateMap(content["new_asset"].asUUID());
		self->uploadNextAsset();
	}

	/*virtual*/ char const* getName(void) const { return "importResponder"; }
};

class BackupCacheReadResponder : public LLTextureCache::ReadResponder
{
protected:
	LOG_CLASS(BackupCacheReadResponder);

public:
	BackupCacheReadResponder(const LLUUID& id, LLImageFormatted* image)
		: mFormattedImage(image), mID(id)
	{
		setImage(image);
	}

	void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat,
				 BOOL imagelocal)
	{
		LLObjectBackup* self = LLObjectBackup::findInstance();
		if (!self) return;

		if (imageformat == IMG_CODEC_TGA &&
			mFormattedImage->getCodec() == IMG_CODEC_J2C)
		{
			LL_WARNS() << "FAILED: texture " << mID
					<< " is formatted as TGA. Not saving." << LL_ENDL;
			self->mNonExportedTextures |= LLObjectBackup::TEXTURE_BAD_ENCODING;
			mFormattedImage = NULL;
			mImageSize = 0;
			return;
		}

		if (mFormattedImage.notNull())
		{	
			if (mFormattedImage->getCodec() == imageformat)
			{
				mFormattedImage->appendData(data, datasize);
			}
			else
			{
				LL_WARNS() << "FAILED: texture " << mID
						<< " is formatted as " << mFormattedImage->getCodec()
						<< " while expecting " << imageformat
						<< ". Not saving." << LL_ENDL;
				mFormattedImage = NULL;
				mImageSize = 0;
				return;
			}
		}
		else
		{
			mFormattedImage = LLImageFormatted::createFromType(imageformat);
			mFormattedImage->setData(data, datasize);
		}
		mImageSize = imagesize;
		mImageLocal = imagelocal;
	}

	virtual void completed(bool success)
	{
		LLObjectBackup* self = LLObjectBackup::findInstance();
		if (!self)
		{
			LL_WARNS() << "Export aborted, LLObjectBackup instance gone !"
					<< LL_ENDL;
			return;
		}

		if (success && mFormattedImage.notNull() && mImageSize > 0)
		{
			LL_INFOS() << "SUCCESS getting texture " << mID << LL_ENDL;
			std::string name;
			mID.toString(name);
			name = self->getFolder() + "//" + name;
			LL_INFOS() << "Saving to " << name << LL_ENDL;
			if (!mFormattedImage->save(name))
			{
				LL_WARNS() << "FAILED to save texture " << mID << LL_ENDL;
				self->mNonExportedTextures |= LLObjectBackup::TEXTURE_SAVED_FAILED;
			}
		}
		else
		{
			if (!success)
			{
				LL_WARNS() << "FAILED to get texture " << mID << LL_ENDL;
				self->mNonExportedTextures |= LLObjectBackup::TEXTURE_MISSING;
			}
			if (mFormattedImage.isNull())
			{
				LL_WARNS() << "FAILED: NULL texture " << mID << LL_ENDL;
				self->mNonExportedTextures |= LLObjectBackup::TEXTURE_IS_NULL;
			}
		}	

		self->mCheckNextTexture = true;
	}

private:
	LLPointer<LLImageFormatted> mFormattedImage;
	LLUUID mID;
};

LLObjectBackup::LLObjectBackup(const LLSD&)
:	mRunning(false),
	mRetexture(false),
	mTexturesList(),
	mAssetMap(),
	mCurrentAsset()
{
	//buildFromFile("floater_object.xml");
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_object_backup.xml",
												 NULL, false);	// Don't open
}

//virtual
LLObjectBackup::~LLObjectBackup()
{
	// Just in case we got closed unexpectedly...
	if (gIdleCallbacks.containsFunction(exportWorker))
	{
		gIdleCallbacks.deleteFunction(exportWorker);
	}
}

//static
bool LLObjectBackup::confirmCloseCallback(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLObjectBackup* self = findInstance();
		if (self)
		{
			self->destroy();
		}
	}
	return false;
}

//virtual
void LLObjectBackup::onClose(bool app_quitting)
{
	// Do not destroy the floater on user close action to avoid getting things
	// messed up during import/export.
	if (app_quitting)
	{
		destroy();
	}
	else
	{
		LLNotificationsUtil::add("ConfirmAbortBackup", LLSD(), LLSD(),
										confirmCloseCallback);
	}
}

void LLObjectBackup::showFloater(bool exporting)
{
	// Set the title
	setTitle(getString(exporting ? "export" : "import"));

	mCurObject = 1;
	mCurPrim = mObjects = mPrims = mRezCount = 0;

	// Make the floater pop up
	setVisibleAndFrontmost();
}

void LLObjectBackup::updateExportNumbers()
{
	std::stringstream sstr;	
	LLUICtrl* ctrl = getChild<LLUICtrl>("name_label");	

	sstr << "Export Progress \n";

	sstr << "Remaining Textures " << mTexturesList.size() << "\n";
	ctrl->setValue(LLSD("Text") = sstr.str());
}

void LLObjectBackup::updateImportNumbers()
{
	std::stringstream sstr;	
	LLUICtrl* ctrl = getChild<LLUICtrl>("name_label");	

	if (mRetexture)
	{
		sstr << " Textures uploads remaining : " << mTexturesList.size()
			 << "\n";
		ctrl->setValue(LLSD("Text") = sstr.str());
	}
	else
	{
		sstr << " Textures uploads N/A \n";
		ctrl->setValue(LLSD("Text") = sstr.str());
	}

	sstr << " Objects " << mCurObject << "/" << mObjects << "\n";
	ctrl->setValue(LLSD("Text") = sstr.str());

	sstr << " Rez "<< mRezCount << "/" << mPrims;
	ctrl->setValue(LLSD("Text") = sstr.str());

	sstr << " Build " << mCurPrim << "/" << mPrims;
	ctrl->setValue(LLSD("Text") = sstr.str());
}

void LLObjectBackup::exportObject()
{
	mTexturesList.clear();
	mBadPermsTexturesList.clear();
	mLLSD.clear();
	mThisGroup.clear();
	setDefaultTextures();
	LLSelectMgr::getInstance()->getSelection()->ref();

	// Open the file save dialog
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open("", FFSAVE_XML);
	filepicker->run(boost::bind(&LLObjectBackup::exportObject_continued, this, filepicker));
}

void LLObjectBackup::exportObject_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
	{
		// User canceled save.
		LLSelectMgr::getInstance()->getSelection()->unref();
		return;
	}

	mFileName = filepicker->getFilename();
	mFolder = gDirUtilp->getDirName(mFileName);

	mNonExportedTextures = TEXTURE_OK;
	mExportState = EXPORT_INIT;
	mGotExtraPhysics = !gAgent.getRegion()->getCapability("GetObjectPhysicsData").empty();
	gIdleCallbacks.addFunction(exportWorker, NULL);
}

//---------------------------------------------------------------------------//
// Permissions checking functions
//---------------------------------------------------------------------------//

//static
void LLObjectBackup::setDefaultTextures()
{
	if (LFSimFeatureHandler::instance().exportPolicy() == ep_full_perm)
	{
		// When not in SL and not in an OpenSIM grid with export permission
		// support (i.e. when no texture permission check is needed), we can
		// get these defaults from the user settings...
		LL_TEXTURE_PLYWOOD = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
		LL_TEXTURE_BLANK = LLUUID(gSavedSettings.getString("UIImgWhiteUUID"));
		LL_TEXTURE_INVISIBLE = LLUUID(gSavedSettings.getString("UIImgInvisibleUUID"));
	}
}

//static
bool LLObjectBackup::validatePerms(const LLPermissions* item_permissions)
{
	return item_permissions->allowExportBy(gAgentID, LFSimFeatureHandler::instance().exportPolicy());
}

// So far, only Second Life forces TPVs to verify the creator for textures...
// which sucks, because there is no other way to check for the texture
// permissions or creator than to try and find the asset(s) corresponding to
// the texture in the inventory and check the permissions/creator on the said
// asset(s), meaning that if you created the texture and subsequently deleted
// it from your inventory, you will not be able to export it any more !!!
// The "must be creator" stuff also goes against the usage in Linden Lab's own
// official viewers, since those allow you to save full perm textures (such as
// the textures in the Library), whoever is the actual creator... Go figure !

//static
bool LLObjectBackup::validateTexturePerms(const LLUUID& asset_id)
{
	if (LFSimFeatureHandler::instance().exportPolicy() == ep_full_perm)
	{
		// If we are not in Second Life and we don't have export-permission
		// support, don't bother and unconditionally accept the texture export
		// (legacy behaviour).
		return true;
	}

	if (asset_id == LL_TEXTURE_PLYWOOD ||
		asset_id == LL_TEXTURE_BLANK ||
		asset_id == LL_TEXTURE_INVISIBLE ||
		asset_id == LL_TEXTURE_TRANSPARENT ||
		asset_id == LL_TEXTURE_MEDIA)
	{
		// Allow to export a few default SL textures.
		return true;
	}

	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null, cats, items,
							LLInventoryModel::INCLUDE_TRASH,
							asset_id_matches);
	S32 count = items.size();
	if (count > 0)
	{
		for (S32 i = 0; i < count; ++i)
		{
			const LLPermissions item_permissions = items[i]->getPermissions();
			if (validatePerms(&item_permissions))
			{
				return true;
			}
		}
	}

	return false;
}

LLUUID LLObjectBackup::validateTextureID(const LLUUID& asset_id)
{
	if (mBadPermsTexturesList.count(asset_id))
	{
		// We already checked it and know it's bad...
		return LL_TEXTURE_PLYWOOD;
	}
	else if (asset_id.isNull() || validateTexturePerms(asset_id))
	{
		return asset_id;
	}
	else
	{
		mBadPermsTexturesList.insert(asset_id);	// Cache bad texture ID
		mNonExportedTextures |= TEXTURE_BAD_PERM;
		LL_WARNS() << "Bad permissions for texture ID: " << asset_id
				<< " - Texture will not be exported." << LL_ENDL;
		return LL_TEXTURE_PLYWOOD;
	}
}

//static
bool LLObjectBackup::validateNode(LLSelectNode* node)
{
	LLPermissions* perms = node->mPermissions;
	if (!perms || !validatePerms(perms))
	{
		return false;
	}

	// Additionally check if this is a sculpt or a mesh object and if yes, if
	// we have export permission on the sclupt texture or the mesh object.
	LLViewerObject* obj = node->getObject();
	if (!obj)	// Paranoia
	{
		return false;
	}
	else if (obj->isSculpted())
	{
		if (obj->isMesh())
		{
			return false; // We can't export mesh from here, anyway.
		}
		else
		{
			const LLSculptParams* params = obj->getSculptParams();
			LLUUID sculpt_id = params->getSculptTexture();
			return validateTexturePerms(sculpt_id);
		}
	}
	else
	{
		return true;
	}
}

//---------------------------------------------------------------------------//

//static
void LLObjectBackup::exportWorker(void *userdata)
{	
	LLObjectBackup* self = findInstance();
	if (!self)
	{
		gIdleCallbacks.deleteFunction(exportWorker);
		LL_WARNS() << "Export process aborted. LLObjectBackup instance gone !"
				<< LL_ENDL;
		LLNotifications::instance().add("ExportAborted");
		return;
	}

	self->updateExportNumbers();

	switch (self->mExportState)
	{
		case EXPORT_INIT:
		{
			self->showFloater(true);
			//LLSelectMgr::getInstance()->getSelection()->ref(); // Singu Note: Don't do this.
			// Fall through to EXPORT_CHECK_PERMS
		}
		case EXPORT_CHECK_PERMS:
		{
			struct ff : public LLSelectedNodeFunctor
			{
				virtual bool apply(LLSelectNode* node)
				{
					return LLObjectBackup::validateNode(node);
				}
			} func;

			LLViewerObject* object;
			object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
			if (object)
			{
				if (LLSelectMgr::getInstance()->getSelection()->applyToNodes(&func, false))
				{
					self->mExportState = EXPORT_FETCH_PHYSICS;
				}
				else
				{
					struct vv : public LLSelectedNodeFunctor
					{
						virtual bool apply(LLSelectNode* node)
						{
							return node->mValid;
						}
					} func2;

					if (LLSelectMgr::getInstance()->getSelection()->applyToNodes(&func2, false))
					{
						LL_WARNS() << "Incorrect permission to export" << LL_ENDL;
						self->mExportState = EXPORT_FAILED;
						LLSelectMgr::getInstance()->getSelection()->unref();
					}
					else
					{
						LL_DEBUGS("ObjectBackup") << "Nodes permissions not yet received, delaying..."
												  << LL_ENDL;
						self->mExportState = EXPORT_CHECK_PERMS;
					}
				}
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
				LLSelectMgr::getInstance()->getSelection()->unref();
			}
			break;
		}

		case EXPORT_FETCH_PHYSICS:
		{
			// Don't bother to try and fetch the extra physics flags if we
			// don't have sim support for them...
			if (!self->mGotExtraPhysics)
			{
				self->mExportState = EXPORT_STRUCTURE;
				break;
			}

			struct ff : public LLSelectedNodeFunctor
			{
				virtual bool apply(LLSelectNode* node)
				{
					LLViewerObject* object = node->getObject();
					return gObjectList.gotObjectPhysicsFlags(object);
				}
			} func;

			LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
			if (object)
			{
				if (LLSelectMgr::getInstance()->getSelection()->applyToNodes(&func, false))
				{
					self->mExportState = EXPORT_STRUCTURE;
				}
				else
				{
					LL_DEBUGS("ObjectBackup") << "Nodes physics not yet received, delaying..."
											  << LL_ENDL;
				}
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
				LLSelectMgr::getInstance()->getSelection()->unref();
			}
			break;
		}

		case EXPORT_STRUCTURE:
		{
			struct ff : public LLSelectedObjectFunctor
			{
				virtual bool apply(LLViewerObject* object)
				{
					bool is_attachment = object->isAttachment();
					object->boostTexturePriority(true);
					LLViewerObject::child_list_t children = object->getChildren();
					children.push_front(object); //push root onto list
					LLObjectBackup* self = findInstance();
					LLSD prim_llsd = self->primsToLLSD(children,
													   is_attachment);
					LLSD stuff;
					if (is_attachment)
					{
						stuff["root_position"] = object->getPositionEdit().getValue();
						stuff["root_rotation"] = ll_sd_from_quaternion(object->getRotationEdit());
					}
					else
					{
						stuff["root_position"] = object->getPosition().getValue();
						stuff["root_rotation"] = ll_sd_from_quaternion(object->getRotation());
					}
					stuff["group_body"] = prim_llsd;
					self->mLLSD["data"].append(stuff);
					return true;
				}
			} func;

			LLViewerObject* object;
			object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
			if (object)
			{
				self->mExportState = EXPORT_LLSD;
				LLSelectMgr::getInstance()->getSelection()->applyToRootObjects(&func, false);
			}
			else
			{
				self->mExportState = EXPORT_ABORTED;
			}
			LLSelectMgr::getInstance()->getSelection()->unref();
			break;
		}

		case EXPORT_TEXTURES:
		{
			if (!self->mCheckNextTexture)
			{
				// The texture is being fetched. Wait till next idle callback.
				return;
			}

			if (self->mTexturesList.empty())
			{
				self->mExportState = EXPORT_DONE;
				return;
			}

			// Ok, we got work to do...
			self->mCheckNextTexture = false;
			self->exportNextTexture();
			break;
		}

		case EXPORT_LLSD:
		{
			// Create a file stream and write to it
			llofstream export_file(self->mFileName);
			LLSDSerialize::toPrettyXML(self->mLLSD, export_file);
			export_file.close();
			self->mCheckNextTexture = true;
			self->mExportState = EXPORT_TEXTURES;
			break;
		}

		case EXPORT_DONE:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			if (self->mNonExportedTextures == LLObjectBackup::TEXTURE_OK)
			{
				LL_INFOS() << "Export successful and complete." << LL_ENDL;
				LLNotificationsUtil::add("ExportSuccessful");
			}
			else
			{
				LL_INFOS() << "Export successful but incomplete: some texture(s) not saved."
						<< LL_ENDL;
				std::string reason;
				U32 error_bits_map = self->mNonExportedTextures;
				if (error_bits_map & LLObjectBackup::TEXTURE_BAD_PERM)
				{
					reason += "\nBad permissions/creator.";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_MISSING)
				{
					reason += "\nMissing texture (retrying after full rezzing might work).";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_BAD_ENCODING)
				{
					reason += "\nBad texture encoding.";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_IS_NULL)
				{
					reason += "\nNull texture.";
				}
				if (error_bits_map & LLObjectBackup::TEXTURE_SAVED_FAILED)
				{
					reason += "\nCould not write to disk.";
				}
				LLSD args;
				args["REASON"] = reason;
				LLNotificationsUtil::add("ExportPartial", args);
			}
			self->destroy();
			break;
		}

		case EXPORT_FAILED:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			LL_WARNS() << "Export process failed." << LL_ENDL;
			LLNotificationsUtil::add("ExportFailed");
			self->destroy();
			break;
		}

		case EXPORT_ABORTED:
		{
			gIdleCallbacks.deleteFunction(exportWorker);
			LL_WARNS() << "Export process aborted." << LL_ENDL;
			LLNotificationsUtil::add("ExportAborted");
			self->destroy();
			break;
		}
	}
}

LLSD LLObjectBackup::primsToLLSD(LLViewerObject::child_list_t child_list,
								 bool is_attachment)
{
	LLViewerObject* object;
	LLSD llsd;
	char localid[16];
	LLUUID t_id;

	for (LLViewerObject::child_list_t::iterator i = child_list.begin();
		 i != child_list.end(); ++i)
	{
		object = (*i);
		LLUUID id = object->getID();

		LL_INFOS() << "Exporting prim " << object->getID().asString() << LL_ENDL;

		// Create an LLSD object that represents this prim. It will be injected
		// in to the overall LLSD tree structure
		LLSD prim_llsd;

		if (!object->isRoot())
		{
			// Parent id
			snprintf(localid, sizeof(localid), "%u",
					 object->getSubParent()->getLocalID());
			prim_llsd["parent"] = localid;
		}

		// Name and description
		LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(object);
		if (node)
		{
			prim_llsd["name"] = node->mName;
			prim_llsd["description"] = node->mDescription;
		}

		// Transforms
		if (is_attachment)
		{
			prim_llsd["position"] = object->getPositionEdit().getValue();
			prim_llsd["rotation"] = ll_sd_from_quaternion(object->getRotationEdit());
		}
		else
		{
			prim_llsd["position"] = object->getPosition().getValue();
			prim_llsd["rotation"] = ll_sd_from_quaternion(object->getRotation());
		}
		prim_llsd["scale"] = object->getScale().getValue();

		// Flags
		prim_llsd["phantom"] = object->flagPhantom();		// legacy
		prim_llsd["physical"] = object->flagUsePhysics();	// legacy
		prim_llsd["flags"] = (S32)object->getFlags();		// new way

		// Extra physics flags
		if (mGotExtraPhysics)
		{
			LLSD& physics = prim_llsd["ExtraPhysics"];
			physics["PhysicsShapeType"] = object->getPhysicsShapeType();
			physics["Gravity"] = object->getPhysicsGravity();
			physics["Friction"] = object->getPhysicsFriction();
			physics["Density"] = object->getPhysicsDensity();
			physics["Restitution"] = object->getPhysicsRestitution();
		}

		// Click action
		if (S32 action = object->getClickAction()) // Non-zero
			prim_llsd["clickaction"] = action;

		// Prim material
		prim_llsd["material"] = object->getMaterial();

		// Volume params
		LLVolumeParams params = object->getVolume()->getParams();
		prim_llsd["volume"] = params.asLLSD();

		// Extra paramsb6fab961-af18-77f8-cf08-f021377a7244

		if (object->isFlexible())
		{
			// Flexible
			const LLFlexibleObjectData* flex = object->getFlexibleObjectData();
			if (flex)
			{
				prim_llsd["flexible"] = flex->asLLSD();
			}
		}

		const LLLightParams* light = object->getLightParams();
		if (light)
		{
			// Light
			prim_llsd["light"] = light->asLLSD();
		}

		const LLLightImageParams* light_param = object->getLightImageParams();
		if (light_param)
		{
			// Light image
			t_id = validateTextureID(light_param->getLightTexture());
			if (mTexturesList.count(t_id) == 0)
			{
				LL_INFOS() << "Found a light texture, adding to list " << t_id
						<< LL_ENDL;
				mTexturesList.insert(t_id);
			}
			prim_llsd["light_texture"] = light_param->asLLSD();
		}

		const LLSculptParams* sculpt = object->getSculptParams();
		if (sculpt)
		{
			// Sculpt
			prim_llsd["sculpt"] = sculpt->asLLSD();
			if ((sculpt->getSculptType() & LL_SCULPT_TYPE_MASK) != LL_SCULPT_TYPE_MESH)
			{
				LLUUID sculpt_texture = sculpt->getSculptTexture();
				if (sculpt_texture == validateTextureID(sculpt_texture))
				{
					if (mTexturesList.count(sculpt_texture) == 0)
					{
						LL_INFOS() << "Found a sculpt texture, adding to list "
								<< sculpt_texture << LL_ENDL;
						mTexturesList.insert(sculpt_texture);
					}
				}
				else
				{
					LL_WARNS() << "Incorrect permission to export a sculpt texture."
							<< LL_ENDL;
					mExportState = EXPORT_FAILED;
				}
			}
		}

		// Textures and materials
		LLSD te_llsd;
		LLSD this_te_llsd;
		LLSD te_mat_llsd;
		LLSD this_te_mat_llsd;
		bool has_materials = false;
		for (U8 i = 0, count = object->getNumTEs(); i < count; ++i)
		{
			LLTextureEntry* te = object->getTE(i);
			if (!te) continue;	// Paranoia

			// Normal texture/diffuse map
			t_id = validateTextureID(te->getID());
			this_te_llsd = te->asLLSD();
			this_te_llsd["imageid"] = t_id;
			te_llsd.append(this_te_llsd);
			// Do not export non-existent default textures
			if (t_id != LL_TEXTURE_BLANK && t_id != LL_TEXTURE_INVISIBLE)
			{
				if (mTexturesList.count(t_id) == 0)
				{
					mTexturesList.insert(t_id);
				}
			}

			// Materials
			LLMaterial* mat = te->getMaterialParams().get();
			if (mat)
			{
				has_materials = true;
				this_te_mat_llsd = mat->asLLSD();

				t_id = validateTextureID(mat->getNormalID());
				this_te_mat_llsd["NormMap"] = t_id;
				if (mTexturesList.count(t_id) == 0)
				{
					mTexturesList.insert(t_id);
				}

				t_id = validateTextureID(mat->getSpecularID());
				this_te_mat_llsd["SpecMap"] = t_id;
				if (mTexturesList.count(t_id) == 0)
				{
					mTexturesList.insert(t_id);
				}

				te_mat_llsd.append(this_te_mat_llsd);
			}
		}
		prim_llsd["textures"] = te_llsd;
		if (has_materials)
		{
			prim_llsd["materials"] = te_mat_llsd;
		}

		// The keys in the primitive maps do not have to be localids, they can
		// be any string. We simply use localids because they are a unique
		// identifier
		snprintf(localid, sizeof(localid), "%u", object->getLocalID());
		llsd[(const char*)localid] = prim_llsd;
	}

	updateExportNumbers();

	return llsd;
}

void LLObjectBackup::exportNextTexture()
{
	LLUUID id;
	textures_set_t::iterator iter = mTexturesList.begin();
	while (true)
	{
		if (mTexturesList.empty())
		{
			mCheckNextTexture = true;
			LL_INFOS() << "Finished exporting textures." << LL_ENDL;
			return;
		}
		if (iter == mTexturesList.end())
		{
			// Not yet ready, wait and re-check at next idle callback...
			mCheckNextTexture = true;
			return;
		}

		id = *iter++;
		if (id.isNull())
		{
			// NULL texture id: just remove and ignore.
			mTexturesList.erase(id);
			LL_DEBUGS("ObjectBackup") << "Null texture UUID found, ignoring."
									  << LL_ENDL;
			continue;
		}

		LLViewerTexture* imagep = LLViewerTextureManager::findFetchedTexture(id, TEX_LIST_STANDARD);
		if (imagep)
		{
			if (imagep->getDiscardLevel() > 0)
			{
				// Boost texture loading
				imagep->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
				LL_DEBUGS("ObjectBackup") << "Boosting texture: " << id
										  << LL_ENDL;
				LLViewerFetchedTexture* tex;
				tex = LLViewerTextureManager::staticCastToFetchedTexture(imagep);
				if (tex && tex->getDesiredDiscardLevel() > 0)
				{
					// Set min discard level to 0
					tex->setMinDiscardLevel(0);
					LL_DEBUGS("ObjectBackup") << "Min discard level set to 0 for texture: "
											  << id << LL_ENDL;
				}
			}
			else
			{
				// Texture is ready !
				break;
			}
		}
		else
		{
			LL_WARNS() << "We *DON'T* have the texture " << id << LL_ENDL;
			mNonExportedTextures |= TEXTURE_MISSING;
			mTexturesList.erase(id);
		}
	}

	mTexturesList.erase(id);

	LL_INFOS() << "Requesting texture " << id << " from cache." << LL_ENDL;
	LLImageJ2C* mFormattedImage = new LLImageJ2C;
	BackupCacheReadResponder* responder;
	responder = new BackupCacheReadResponder(id, mFormattedImage);
	LLAppViewer::getTextureCache()->readFromCache(id,
												  LLWorkerThread::PRIORITY_HIGH,
												  0, 999999, responder);
}


bool is_default_texture(const LLUUID& id)
{
	return id.isNull() || id == LL_TEXTURE_PLYWOOD || id == LL_TEXTURE_BLANK ||
		   id == LL_TEXTURE_INVISIBLE;
}

void LLObjectBackup::importObject(bool upload)
{
	mRetexture = upload;
	mTexturesList.clear();
	mAssetMap.clear();
	mCurrentAsset.setNull();

	mGotExtraPhysics = !gAgent.getRegion()->getCapability("GetObjectPhysicsData").empty();

	setDefaultTextures();

	// Open the file open dialog
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(FFLOAD_XML, "", "import");
	filepicker->run(boost::bind(&LLObjectBackup::importObject_continued, this, filepicker));
}

void LLObjectBackup::importObject_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
	{
		// User canceled save.
		return;
	}

	std::string file_name = filepicker->getFilename();
	mFolder = gDirUtilp->getDirName(file_name);
	llifstream import_file(file_name);
	LLSDSerialize::fromXML(mLLSD, import_file);
	import_file.close();
	if (!mLLSD.has("data"))
	{
		LLNotificationsUtil::add("ImportFailed");
		destroy();
		return;
	}

	showFloater(false);

	mAgentPos = gAgent.getPositionAgent();
	mAgentRot = LLQuaternion(gAgent.getAtAxis(), gAgent.getLeftAxis(),
							 gAgent.getUpAxis());

	// Get the texture map

	mCurObject = 1;
	mCurPrim = 1;
	mObjects = mLLSD["data"].size();
	mPrims = 0;
	mRezCount = 0;
	updateImportNumbers();

	for (auto const& entry : mLLSD["data"].array())
	{
		LLSD llsd2 = entry["group_body"];

		for (LLSD::map_const_iterator prim_it = llsd2.beginMap(),
									  prim_end = llsd2.endMap();
			 prim_it != prim_end; ++prim_it)
		{
			LLSD prim_llsd = llsd2[prim_it->first];
			if (prim_llsd.has("sculpt"))
			{
				LLSculptParams sculpt;
				sculpt.fromLLSD(prim_llsd["sculpt"]);
				if ((sculpt.getSculptType() & LL_SCULPT_TYPE_MASK) != LL_SCULPT_TYPE_MESH)
				{
					LLUUID orig = sculpt.getSculptTexture();
					if (mTexturesList.count(orig) == 0)
					{
						LL_INFOS() << "Found a new SCULPT texture to upload "
								<< orig << LL_ENDL;
						mTexturesList.insert(orig);
					}
				}
			}

			if (prim_llsd.has("light_texture"))
			{
				LLLightImageParams lightimg;
				lightimg.fromLLSD(prim_llsd["light_texture"]);
				LLUUID t_id = lightimg.getLightTexture();
				if (!is_default_texture(t_id) &&
					mTexturesList.count(t_id) == 0)
				{
					LL_INFOS() << "Found a new light texture to upload: " << t_id
							<< LL_ENDL;
					mTexturesList.insert(t_id);
				}
			}

			// Check both for "textures" and "texture" since the second (buggy)
			// case has already been seen in some exported prims XML files...
			LLSD& te_llsd = prim_llsd.has("textures") ? prim_llsd["textures"]
													  : prim_llsd["texture"];
			for (LLSD::array_iterator it = te_llsd.beginArray();
				 it != te_llsd.endArray(); ++it)
			{
				LLSD the_te = *it;
				LLTextureEntry te;
				te.fromLLSD(the_te);

				LLUUID t_id = te.getID();
				if (!is_default_texture(t_id) &&
					mTexturesList.count(t_id) == 0)
				{
					LL_INFOS() << "Found a new texture to upload: " << t_id
							<< LL_ENDL;
						mTexturesList.insert(t_id);
				}
			}

			if (prim_llsd.has("materials"))
			{
				LLSD mat_llsd = prim_llsd["materials"];
				for (LLSD::array_iterator it = mat_llsd.beginArray();
					 it != mat_llsd.endArray(); ++it)
				{
					LLSD the_mat = *it;
					LLMaterial mat;
					mat.fromLLSD(the_mat);

					LLUUID t_id = mat.getNormalID();
					if (!is_default_texture(t_id) &&
						mTexturesList.count(t_id) == 0)
					{
						LL_INFOS() << "Found a new normal map to upload: "
								<< t_id << LL_ENDL;
						mTexturesList.insert(t_id);
					}

					t_id = mat.getSpecularID();
					if (!is_default_texture(t_id) &&
						mTexturesList.count(t_id) == 0)
					{
						LL_INFOS() << "Found a new specular map to upload: "
								<< t_id << LL_ENDL;
						mTexturesList.insert(t_id);
					}
				}
			}
		}
	}

	if (mRetexture)
	{
		uploadNextAsset();
	}
	else
	{
		importFirstObject();
	}
}

LLVector3 LLObjectBackup::offsetAgent(LLVector3 offset)
{
	return offset * mAgentRot + mAgentPos;
}

void LLObjectBackup::rezAgentOffset(LLVector3 offset)
{
	// This will break for a sitting agent
	LLToolPlacer mPlacer;
	mPlacer.setObjectType(LL_PCODE_CUBE);
	mPlacer.placeObject((S32)offset.mV[0], (S32)offset.mV[1], MASK_NONE);
}

void LLObjectBackup::importFirstObject()
{
	mRunning = true;
	showFloater(false);
	mGroupPrimImportIter = mLLSD["data"].beginArray();	
	mRootRootPos = LLVector3((*mGroupPrimImportIter)["root_position"]);
	mObjects = mLLSD["data"].size();
	mCurObject = 1;
	importNextObject();
}

void LLObjectBackup::importNextObject()
{
	mToSelect.clear();
	mRezCount = 0;

	mThisGroup = (*mGroupPrimImportIter)["group_body"];
	mPrimImportIter = mThisGroup.beginMap();

	mCurPrim = 0;
	mPrims = mThisGroup.size();
	updateImportNumbers();

	LLVector3 lgpos = LLVector3((*mGroupPrimImportIter)["root_position"]);
	mGroupOffset = lgpos - mRootRootPos;
	mRootPos = offsetAgent(LLVector3(2.0, 0.0, 0.0));
	mRootRot = ll_quaternion_from_sd((*mGroupPrimImportIter)["root_rotation"]);

	rezAgentOffset(LLVector3(0.0, 2.0, 0.0));
	// Now we must wait for the callback when ViewerObjectList gets the new
	// objects and we have the correct number selected
}

// This function takes a pointer to a viewerobject and applies the prim
// definition that prim_llsd has
void LLObjectBackup::xmlToPrim(LLSD prim_llsd, LLViewerObject* object)
{
	LLUUID id = object->getID();
	mExpectingUpdate = object->getID();
	LLSelectMgr::getInstance()->selectObjectAndFamily(object);

	if (prim_llsd.has("name"))
	{
		LLSelectMgr::getInstance()->selectionSetObjectName(prim_llsd["name"]);
	}

	if (prim_llsd.has("description"))
	{
		LLSelectMgr::getInstance()->selectionSetObjectDescription(prim_llsd["description"]);
	}

	if (prim_llsd.has("material"))
	{
		LLSelectMgr::getInstance()->selectionSetMaterial(prim_llsd["material"].asInteger());
	}

	if (prim_llsd.has("clickaction"))
	{
		LLSelectMgr::getInstance()->selectionSetClickAction(prim_llsd["clickaction"].asInteger());
	}

	if (prim_llsd.has("parent"))
	{
		//we are not the root node.
		LLVector3 pos = LLVector3(prim_llsd["position"]);
		LLQuaternion rot = ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setPositionRegion(pos * mRootRot + mRootPos + mGroupOffset);
		object->setRotation(rot * mRootRot);
	}
	else
	{
		object->setPositionRegion(mRootPos + mGroupOffset);
		LLQuaternion rot=ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setRotation(rot);
	}

	object->setScale(LLVector3(prim_llsd["scale"]));

	if (prim_llsd.has("flags"))
	{
		U32 flags = (U32)prim_llsd["flags"].asInteger();
		object->setFlags(flags, true);
	}
	else	// Kept for backward compatibility
	{
		/*if (prim_llsd.has("shadows"))
			if (prim_llsd["shadows"].asInteger() == 1)
				object->setFlags(FLAGS_CAST_SHADOWS, true);*/

		if (prim_llsd.has("phantom") && prim_llsd["phantom"].asInteger() == 1)
		{
				object->setFlags(FLAGS_PHANTOM, true);
		}

		if (prim_llsd.has("physical") &&
			prim_llsd["physical"].asInteger() == 1)
		{
			object->setFlags(FLAGS_USE_PHYSICS, true);
		}
	}

	if (mGotExtraPhysics && prim_llsd.has("ExtraPhysics"))
	{
		const LLSD& physics = prim_llsd["ExtraPhysics"];
		object->setPhysicsShapeType(physics["PhysicsShapeType"].asInteger());
		F32 gravity = physics.has("Gravity") ? physics["Gravity"].asReal()
											 : physics["GravityMultiplier"].asReal();
		object->setPhysicsGravity(gravity);
		object->setPhysicsFriction(physics["Friction"].asReal());
		object->setPhysicsDensity(physics["Density"].asReal());
		object->setPhysicsRestitution(physics["Restitution"].asReal());
		object->updateFlags(true);
	}

	// Volume params
	LLVolumeParams volume_params = object->getVolume()->getParams();
	volume_params.fromLLSD(prim_llsd["volume"]);
	object->updateVolume(volume_params);

	if (prim_llsd.has("sculpt"))
	{
		LLSculptParams sculpt;
		sculpt.fromLLSD(prim_llsd["sculpt"]);

		// TODO: check if map is valid and only set texture if map is valid and
		// changes
		LLUUID t_id = sculpt.getSculptTexture();
		if (mAssetMap.count(t_id))
		{
			sculpt.setSculptTexture(mAssetMap[t_id], sculpt.getSculptType());
		}

		object->setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt, true);
	}
		
	if (prim_llsd.has("light"))
	{
		LLLightParams light;
		light.fromLLSD(prim_llsd["light"]);
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT, light, true);
	}
	if (prim_llsd.has("light_texture"))
	{
		// Light image
		LLLightImageParams lightimg;
		lightimg.fromLLSD(prim_llsd["light_texture"]);
		LLUUID t_id = lightimg.getLightTexture();
		if (mAssetMap.count(t_id))
		{
			lightimg.setLightTexture(mAssetMap[t_id]);
		}
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT_IMAGE, lightimg,
								  true);
	}

	if (prim_llsd.has("flexible"))
	{
		LLFlexibleObjectData flex;
		flex.fromLLSD(prim_llsd["flexible"]);
		object->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, flex, true);
	}

	// Textures
	// Check both for "textures" and "texture" since the second (buggy) case
	// has already been seen in some exported prims XML files...
	LL_INFOS() << "Processing textures for prim" << id << LL_ENDL;
	LLSD& te_llsd = prim_llsd.has("textures") ? prim_llsd["textures"]
											  : prim_llsd["texture"];
	U8 i = 0;
	for (LLSD::array_iterator it =  te_llsd.beginArray();
		 it != te_llsd.endArray(); ++it)
	{
	    LLSD the_te = *it;
	    LLTextureEntry te;
	    te.fromLLSD(the_te);
		LLUUID t_id = te.getID();
		if (mAssetMap.count(t_id))
		{
			te.setID(mAssetMap[t_id]);
		}

	    object->setTE(i++, te);
	}
	LL_INFOS() << "Textures done !" << LL_ENDL;

	// Materials
	if (prim_llsd.has("materials"))
	{
		LL_INFOS() << "Processing materials for prim " << id << LL_ENDL;
		te_llsd = prim_llsd["materials"];
		i = 0;
		for (LLSD::array_iterator it = te_llsd.beginArray();
			 it != te_llsd.endArray(); ++it)
		{
		    LLSD the_mat = *it;
			LLMaterialPtr mat = new LLMaterial(the_mat);

			LLUUID t_id = mat->getNormalID();
			if (id.notNull() && mAssetMap.count(t_id))
			{
				mat->setNormalID(mAssetMap[t_id]);
			}

			t_id = mat->getSpecularID();
			if (id.notNull() && mAssetMap.count(t_id))
			{
				mat->setSpecularID(mAssetMap[t_id]);
			}

			LLMaterialMgr::getInstance()->put(id, i++, *mat);
		}
		LL_INFOS() << "Materials done !" << LL_ENDL;
	}

	object->sendRotationUpdate();
	object->sendTEUpdate();	
	object->sendShapeUpdate();
	LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_SCALE | UPD_POSITION);

	LLSelectMgr::getInstance()->deselectAll();
}

// This is fired when the update packet is processed so we know the prim
// settings have stuck
//static
void LLObjectBackup::primUpdate(LLViewerObject* object)
{
	LLObjectBackup* self = findInstance();
	if (!object || !self || !self->mRunning ||
		object->mID != self->mExpectingUpdate)
	{
			return;
	}

	++self->mCurPrim;
	self->updateImportNumbers();
	++self->mPrimImportIter;

	self->mExpectingUpdate.setNull();

	if (self->mPrimImportIter == self->mThisGroup.endMap())
	{
		LL_INFOS() << "Trying to link..." << LL_ENDL;

		if (self->mToSelect.size() > 1)
		{
			std::reverse(self->mToSelect.begin(), self->mToSelect.end());
			// Now link
			LLSelectMgr::getInstance()->deselectAll();
			LLSelectMgr::getInstance()->selectObjectAndFamily(self->mToSelect, true);
			LLSelectMgr::getInstance()->sendLink();
			LLViewerObject* root = self->mToSelect.back();
			root->setRotation(self->mRootRot);
		}

		++self->mCurObject;
		++self->mGroupPrimImportIter;
		if (self->mGroupPrimImportIter != self->mLLSD["data"].endArray())
		{
			self->importNextObject();
			return;
		}

		self->mRunning = false;
		self->destroy();
		return;
	}

	LLSD prim_llsd = self->mThisGroup[self->mPrimImportIter->first];

	if (self->mToSelect.empty())
	{
		LL_WARNS() << "error: ran out of objects to mod." << LL_ENDL;
		self->mRunning = false;
		self->destroy();
		return;
	}

	if (self->mPrimImportIter != self->mThisGroup.endMap())
	{
		//rezAgentOffset(LLVector3(1.0, 0.0, 0.0));
		LLSD prim_llsd =self-> mThisGroup[self->mPrimImportIter->first];
		++self->mProcessIter;
		self->xmlToPrim(prim_llsd, *(self->mProcessIter));
	}
}

// Callback when we rez a new object when the importer is running.
//static
void LLObjectBackup::newPrim(LLViewerObject* object)
{
	LLObjectBackup* self = findInstance();
	if (!object || !self || !self->mRunning) return;

	++self->mRezCount;
	self->mToSelect.push_back(object);
	self->updateImportNumbers();
	++self->mPrimImportIter;

	object->setPosition(self->offsetAgent(LLVector3(0.0, 1.0, 0.0)));
	LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_POSITION);

	if (self->mPrimImportIter != self->mThisGroup.endMap())
	{
		self->rezAgentOffset(LLVector3(1.0, 0.0 ,0.0));
	}
	else
	{
		LL_INFOS() << "All prims rezzed, moving to build stage" << LL_ENDL;
		// Deselecting is required to ensure that the first child prim in
		// the link set (which is also the last rezzed prim and thus
		// currently selected) will be properly renamed and desced.
		LLSelectMgr::getInstance()->deselectAll();
		self->mPrimImportIter = self->mThisGroup.beginMap();
		LLSD prim_llsd = self->mThisGroup[self->mPrimImportIter->first];
		self->mProcessIter = self->mToSelect.begin();
		self->xmlToPrim(prim_llsd, *(self->mProcessIter));
	}
}

void LLObjectBackup::updateMap(LLUUID uploaded_asset)
{
	if (mCurrentAsset.notNull())
	{
		LL_INFOS() << "Mapping " << mCurrentAsset << " to " << uploaded_asset
				<< LL_ENDL;
		mAssetMap[mCurrentAsset] = uploaded_asset;
	}
}

void myupload_new_resource(const LLTransactionID &tid,
						 LLAssetType::EType asset_type,
						 std::string name,
						 std::string desc,
						 S32 compression_info,
						 LLFolderType::EType destination_folder_type,
						 LLInventoryType::EType inv_type,
						 U32 next_owner_perm,
						 const std::string& display_name,
						 LLAssetStorage::LLStoreAssetCallback callback,
						 void *userdata)
{
	if (gDisconnected)
	{
		return;
	}

	LLAssetID uuid = tid.makeAssetID(gAgent.getSecureSessionID());	

	// At this point, we're ready for the upload.
	std::string upload_message = "Uploading texture:\n\n" + display_name;
	LLUploadDialog::modalUploadDialog(upload_message);

	std::string url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
	if (!url.empty())
	{
		LLSD body;
		body["folder_id"] = gInventory.findCategoryUUIDForType(destination_folder_type == LLFolderType::FT_NONE ?
															   LLFolderType::assetTypeToFolderType(asset_type) :
															   destination_folder_type);
		body["asset_type"] = LLAssetType::lookup(asset_type);
		body["inventory_type"] = LLInventoryType::lookup(inv_type);
		body["name"] = name;
		body["description"] = desc;

		std::ostringstream llsdxml;
		LLSDSerialize::toXML(body, llsdxml);
		LL_DEBUGS("ObjectBackup") << "posting body to capability: "
								  << llsdxml.str() << LL_ENDL;
		LLHTTPClient::post(url, body,
						   new ImportObjectResponder(body, uuid, asset_type));
	}
	else
	{
		LL_INFOS() << "NewAgentInventory capability not found. Can't upload !"
				<< LL_ENDL;
	}
}

void LLObjectBackup::uploadNextAsset()
{
	if (mTexturesList.empty())
	{
		LL_INFOS() << "Texture list is empty, moving to rez stage." << LL_ENDL;
		mCurrentAsset.setNull();
		importFirstObject();
		return;
	}

	updateImportNumbers();

	textures_set_t::iterator iter = mTexturesList.begin();
	LLUUID id = *iter;
	mTexturesList.erase(iter);

	LL_INFOS() << "Got texture ID " << id << ": trying to upload..." << LL_ENDL;

	mCurrentAsset = id;
	std::string struid;
	id.toString(struid);
	std::string filename = mFolder + "//" + struid;
	LLAssetID uuid;
	LLTransactionID tid;

	// generate a new transaction ID for this asset
	tid.generate();
	uuid = tid.makeAssetID(gAgent.getSecureSessionID());

	S32 file_size;
	LLAPRFile outfile(filename, LL_APR_RB, &file_size);
	if (outfile.getFileHandle())
	{
		const S32 buf_size = 65536;	
		U8 copy_buf[buf_size];
		LLVFile file(gVFS, uuid,  LLAssetType::AT_TEXTURE, LLVFile::WRITE);
		file.setMaxSize(file_size);
		while ((file_size = outfile.read(copy_buf, buf_size)))
		{
			file.write(copy_buf, file_size);
		}
		outfile.close();
	}
	else
	{
		LL_WARNS() << "Unable to access output file " << filename << LL_ENDL;
		uploadNextAsset();
		return;
	}

	 myupload_new_resource(tid, LLAssetType::AT_TEXTURE, struid, struid, 0,
		LLFolderType::FT_TEXTURE,
		LLInventoryType::defaultForAssetType(LLAssetType::AT_TEXTURE),
						   0x0, struid, NULL, NULL);
}
