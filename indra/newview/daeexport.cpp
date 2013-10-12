/**
* @file daeexport.cpp
* @brief A system which allows saving in-world objects to Collada .DAE files for offline texturizing/shading.
* @authors Latif Khalifa
*
* $LicenseInfo:firstyear=2013&license=LGPLV2.1$
* Copyright (C) 2013 Latif Khalifa
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General
* Public License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA 02110-1301 USA
*/
#include "llviewerprecompiledheaders.h"

#include "daeexport.h"

//colladadom includes
#include "dae.h"
#include "dom/domCOLLADA.h"
#include "dom/domMatrix.h"

// library includes
#include "aifilepicker.h"
#include "llnotificationsutil.h"

// newview includes
#include "lfsimfeaturehandler.h"
#include "llface.h"
#include "llvovolume.h"
#include "llviewerinventory.h"
#include "llinventorymodel.h"
#include "llinventoryfunctions.h"

// menu includes
#include "llevent.h"
#include "llmemberlistener.h"
#include "llselectmgr.h"

// Floater and UI
#include "llfloater.h"
#include "lluictrlfactory.h"
#include "llscrollcontainer.h"
#include "lltexturectrl.h"

// Files and cache
#include "llcallbacklist.h"
#include "lldir.h"
#include "lltexturecache.h"
#include "llimage.h"
#include "llimagej2c.h"
#include "llimagepng.h"
#include "llimagetga.h"
#include "llimagebmp.h"
#include "llimagejpeg.h"

#define TEXTURE_DOWNLOAD_TIMEOUT 60.0f

extern LLUUID gAgentID;

//Typedefs used in other files, using here for consistency.
typedef LLMemberListener<LLView> view_listener_t;

namespace DAEExportUtil
{
	static LLUUID LL_TEXTURE_PLYWOOD		= LLUUID("89556747-24cb-43ed-920b-47caed15465f");
	static LLUUID LL_TEXTURE_BLANK			= LLUUID("5748decc-f629-461c-9a36-a35a221fe21f");
	static LLUUID LL_TEXTURE_INVISIBLE		= LLUUID("38b86f85-2575-52a9-a531-23108d8da837");
	static LLUUID LL_TEXTURE_TRANSPARENT	= LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903");
	static LLUUID LL_TEXTURE_MEDIA			= LLUUID("8b5fec65-8d8d-9dc5-cda8-8fdf2716e361");

	enum image_format_type
	{
		ft_tga,
		ft_png,
		ft_j2c,
		ft_bmp,
		ft_jpg
	};

	static const std::string image_format_ext[] = { "tga", "png", "j2c", "bmp", "jpg" };

	static bool canExportTexture(const LLUUID& id, std::string* name = NULL)
	{
		// Find inventory items with asset id of the sculpt map
		LLViewerInventoryCategory::cat_array_t cats;
		LLViewerInventoryItem::item_array_t items;
		LLAssetIDMatches asset_id_matches(id);
		gInventory.collectDescendentsIf(LLUUID::null,
			cats,
			items,
			LLInventoryModel::INCLUDE_TRASH,
			asset_id_matches);

		// See if any of the inventory items matching this texture id are exportable
		ExportPolicy policy = LFSimFeatureHandler::instance().exportPolicy();
		for (S32 i = 0; i < items.count(); i++)
		{
			const LLPermissions item_permissions = items[i]->getPermissions();
			if (item_permissions.allowExportBy(gAgentID, policy))
			{
				if (name != NULL)
				{
					(*name) = items[i]->getName();
				}
				return true;
			}
		}

		if (name != NULL)
		{
			(*name) = id.getString();
		}

		return (policy & ep_full_perm) == ep_full_perm;
	}

	static bool canExportNode(LLSelectNode* node)
	{
		LLPermissions* perms = node->mPermissions;	// Is perms ever NULL?
		// This tests the PERM_EXPORT bit too, which is not really necessary (just checking if it's set
		// on the root prim would suffice), but also isn't hurting.
		if (!(perms && perms->allowExportBy(gAgentID, LFSimFeatureHandler::instance().exportPolicy())))
		{
			return false;
		}

		// Additionally chack if this is a sculpt
		LLViewerObject* obj = node->getObject();
		if (obj->isSculpted() && !obj->isMesh())
		{
			LLSculptParams *sculpt_params = (LLSculptParams *)obj->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
			LLUUID sculpt_id = sculpt_params->getSculptTexture();
			return canExportTexture(sculpt_id);
		}
		else // not sculpt, we already checked generic permissions
		{
			return true;
		}
	}
}


class ColladaExportFloater
	: public LLFloater
{
private:
	typedef std::map<LLUUID, std::string> texture_list_t;
	LLView* mExportBtn;
	LLView* mFileName;
	LLView* mTextureTypeCombo;

	DAESaver mSaver;
	texture_list_t mTexturesToSave;
	S32 mTotal;
	S32 mNumTextures;
	S32 mNumExportableTextures;
	std::string mObjectName;
	LLTimer mTimer;
	LLUIString mTitleProgress;

public:
	ColladaExportFloater()
		: LLFloater(std::string("Collada Export"), std::string("ColladaExportFloaterRect"), LLStringUtil::null)
	{
		mCommitCallbackRegistrar.add("ColladaExport.FilePicker",		boost::bind(&ColladaExportFloater::onClickBrowse, this));
		mCommitCallbackRegistrar.add("ColladaExport.Export",			boost::bind(&ColladaExportFloater::onClickExport, this));
		mCommitCallbackRegistrar.add("ColladaExport.TextureTypeCombo",	boost::bind(&ColladaExportFloater::onTextureTypeCombo, this, boost::bind(&LLUICtrl::getControlName, _1), _2));
		mCommitCallbackRegistrar.add("ColladaExport.TextureExport",		boost::bind(&ColladaExportFloater::onTextureExportCheck, this, _2));
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_dae_export.xml");

		addSelectedObjects();
		if (LLUICtrl* ctrl = findChild<LLUICtrl>("Object Name"))
		{
			ctrl->setTextArg("[NAME]", mObjectName);
		}
		if (LLUICtrl* ctrl = findChild<LLUICtrl>("Exportable Prims"))
		{
			ctrl->setTextArg("[COUNT]", llformat("%d", mSaver.mObjects.size()));
			ctrl->setTextArg("[TOTAL]", llformat("%d", mTotal));
		}
		if (LLUICtrl* ctrl = findChild<LLUICtrl>("Exportable Textures"))
		{
			ctrl->setTextArg("[COUNT]", llformat("%d", mNumExportableTextures));
			ctrl->setTextArg("[TOTAL]", llformat("%d", mNumTextures));
		}
		addTexturePreview();
	}

	virtual ~ColladaExportFloater()
	{
		if (gIdleCallbacks.containsFunction(saveTexturesWorker, this))
		{
			gIdleCallbacks.deleteFunction(saveTexturesWorker, this);
		}
	}

	BOOL postBuild()
	{
		mFileName				= getChildView("file name editor");
		mExportBtn				= getChildView("export button");
		mTextureTypeCombo       = getChildView("texture type combo");
		mTitleProgress			= getString("texture_progress");

		mTextureTypeCombo->setValue(gSavedSettings.getS32(mTextureTypeCombo->getControlName()));
		onTextureExportCheck(getChildView("texture export check")->getValue());
		return TRUE;
	}

	void updateTitleProgress()
	{
		mTitleProgress.setArg("COUNT", llformat("%d", mTexturesToSave.size()));
		setTitle(mTitleProgress);
	}

	void onTextureExportCheck(const LLSD& value)
	{
		mTextureTypeCombo->setEnabled(value);
	}

	void onTextureTypeCombo(const std::string& control_name, const LLSD& value)
	{
		gSavedSettings.setS32(control_name, value);
	}

	void onClickBrowse()
	{
		static const std::string file_ext = ".dae";
		AIFilePicker* filepicker = AIFilePicker::create();
		filepicker->open(mObjectName + file_ext);
		filepicker->run(boost::bind(&ColladaExportFloater::onFilePicker, this, filepicker));
	}

	void onFilePicker(AIFilePicker* filepicker)
	{
		if (filepicker->hasFilename())
		{
			mFileName->setValue(filepicker->getFilename());
			mExportBtn->setEnabled(TRUE);
		}
	}

	void onClickExport()
	{
		if (gSavedSettings.getBOOL("DAEExportTextures"))
		{
			saveTextures();
		}
		else
		{
			onTexturesSaved();
		}
	}

	void onTexturesSaved()
	{
		std::string selected_filename = mFileName->getValue();
		mSaver.saveDAE(selected_filename);
		LLNotificationsUtil::add("WavefrontExportSuccess", LLSD().with("FILENAME", selected_filename));
		close();
	}

	void addSelectedObjects()
	{
		if (LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection())
		{
			mSaver.mOffset = -selection->getFirstRootObject()->getRenderPosition();
			mObjectName = selection->getFirstRootNode()->mName;
			mTotal = 0;

			for (LLObjectSelection::iterator iter = selection->begin(); iter != selection->end(); ++iter)
			{
				mTotal++;
				LLSelectNode* node = *iter;
				if (!node->getObject()->getVolume() || !DAEExportUtil::canExportNode(node)) continue;
				mSaver.add(node->getObject(), node->mName);
			}

			if (mSaver.mObjects.empty())
			{
				LLNotificationsUtil::add("ExportFailed");
				close();
				return;
			}

			mSaver.updateTextureInfo();
			mNumTextures = mSaver.mTextures.size();
			mNumExportableTextures = getNumExportableTextures();
		}
	}

	S32 getNumExportableTextures()
	{
		S32 res = 0;

		for (DAESaver::string_list_t::const_iterator t = mSaver.mTextureNames.begin(); t != mSaver.mTextureNames.end(); ++t)
		{
			std::string name = *t;
			if (!name.empty())
			{
				++res;
			}
		}

		return res;
	}

	void addTexturePreview()
	{
		S32 num_text = mNumExportableTextures;
		if (num_text == 0) return;

		S32 img_width = 100;
		S32 img_height = img_width + 15;
		S32 panel_height = (num_text / 3 + 1) * (img_height) + 10;
		LLScrollContainer* scroll_container = getChild<LLScrollContainer>("textures container");
		LLPanel* panel = new LLPanel(std::string(), LLRect(0, panel_height, 350, 0), false);
		scroll_container->setScrolledView(panel);
		scroll_container->addChild(panel);
		panel->setEnabled(FALSE);
		S32 img_nr = 0;
		for (U32 i=0; i < mSaver.mTextures.size(); i++)
		{
			if (mSaver.mTextureNames[i].empty()) continue;

			S32 left = 16 + (img_nr % 3) * (img_width + 13);
			S32 bottom = panel_height - (10 + (img_nr / 3 + 1) * (img_height));

			LLRect rect(left, bottom + img_height, left + img_width, bottom);
			LLTextureCtrl* img = new LLTextureCtrl(std::string(), rect, std::string(), mSaver.mTextures[i], LLUUID(), std::string());
			panel->addChild(img);
			img_nr++;
		}
	}

	void saveTextures()
	{
		mTexturesToSave.clear();
		for (U32 i=0; i < mSaver.mTextures.size(); i++)
		{
			if (mSaver.mTextureNames[i].empty()) continue;
			mTexturesToSave[mSaver.mTextures[i]] = mSaver.mTextureNames[i];
		}

		mSaver.mImageFormat = DAEExportUtil::image_format_ext[(S32)mTextureTypeCombo->getValue()];

		LL_INFOS("ColladaExport") << "Starting to save textures" << LL_ENDL;
		mTimer.setTimerExpirySec(TEXTURE_DOWNLOAD_TIMEOUT);
		mTimer.start();
		updateTitleProgress();
		gIdleCallbacks.addFunction(saveTexturesWorker, this);
	}

	class CacheReadResponder : public LLTextureCache::ReadResponder
	{
	private:
		LLPointer<LLImageFormatted> mFormattedImage;
		LLUUID mID;
		std::string mName;
		S32 mImageType;

	public:
		CacheReadResponder(const LLUUID& id, LLImageFormatted* image, std::string name, S32 img_type)
			: mFormattedImage(image), mID(id), mName(name), mImageType(img_type)
		{
			setImage(image);
		}

		void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat, BOOL imagelocal)
		{
			if (imageformat == IMG_CODEC_TGA && mFormattedImage->getCodec() == IMG_CODEC_J2C)
			{
				LL_WARNS("ColladaExport") << "FAILED: texture " << mID << " is formatted as TGA. Not saving." << LL_ENDL;
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
					LL_WARNS("ColladaExport") << "FAILED: texture " << mID << " in wrong format." << LL_ENDL;
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
			if (success && mFormattedImage.notNull() && mImageSize > 0)
			{
				bool ok = false;

				// If we are saving jpeg2000, no need to do anything, just write to disk
				if (mImageType == DAEExportUtil::ft_j2c)
				{
					mName += "." + mFormattedImage->getExtension();
					ok = mFormattedImage->save(mName);
				}
				else
				{
					// For other formats we need to decode first
					if (mFormattedImage->updateData() && (mFormattedImage->getWidth() * mFormattedImage->getHeight() * mFormattedImage->getComponents()))
					{
						LLPointer<LLImageRaw> raw = new LLImageRaw;
						raw->resize(mFormattedImage->getWidth(), mFormattedImage->getHeight(),	mFormattedImage->getComponents());

						if (mFormattedImage->decode(raw, 0))
						{
							LLPointer<LLImageFormatted> img = NULL;
							switch (mImageType)
							{
							case DAEExportUtil::ft_tga:
								img = new LLImageTGA;
								break;
							case DAEExportUtil::ft_png:
								img = new LLImagePNG;
								break;
							case DAEExportUtil::ft_bmp:
								img = new LLImageBMP;
								break;
							case DAEExportUtil::ft_jpg:
								img = new LLImageJPEG;
								break;
							}

							if (!img.isNull())
							{
								if (img->encode(raw, 0))
								{
									mName += "." + img->getExtension();
									ok = img->save(mName);
								}
							}
						}
					}
				}

				if (ok)
				{
					LL_INFOS("ColladaExport") << "Saved texture to " << mName << LL_ENDL;
				}
				else
				{
					LL_WARNS("ColladaExport") << "FAILED to save texture " << mID << LL_ENDL;
				}
			}
			else
			{
				LL_WARNS("ColladaExport") << "FAILED to save texture " << mID << LL_ENDL;
			}
		}
	};

	static void saveTexturesWorker(void* data)
	{
		ColladaExportFloater* me = (ColladaExportFloater *)data;
		if (me->mTexturesToSave.size() == 0)
		{
			LL_INFOS("ColladaExport") << "Done saving textures" << LL_ENDL;
			me->updateTitleProgress();
			gIdleCallbacks.deleteFunction(saveTexturesWorker, me);
			me->mTimer.stop();
			me->onTexturesSaved();
			return;
		}

		LLUUID id = me->mTexturesToSave.begin()->first;
		LLViewerTexture* imagep = LLViewerTextureManager::findTexture(id);
		if (!imagep)
		{
			me->mTexturesToSave.erase(id);
			me->updateTitleProgress();
			me->mTimer.reset();
		}
		else
		{
			if (imagep->getDiscardLevel() == 0) // image download is complete
			{
				LL_INFOS("ColladaExport") << "Saving texture " << id << LL_ENDL;
				LLImageFormatted* img = new LLImageJ2C;
				S32 img_type = me->mTextureTypeCombo->getValue();
				std::string name = gDirUtilp->getDirName(me->mFileName->getValue());
				name += gDirUtilp->getDirDelimiter() + me->mTexturesToSave[id];
				CacheReadResponder* responder = new CacheReadResponder(id, img, name, img_type);
				LLAppViewer::getTextureCache()->readFromCache(id, LLWorkerThread::PRIORITY_HIGH, 0, 999999, responder);
				me->mTexturesToSave.erase(id);
				me->updateTitleProgress();
				me->mTimer.reset();
			}
			else if (me->mTimer.hasExpired())
			{
				LL_WARNS("ColladaExport") << "Timed out downloading texture " << id << LL_ENDL;
				me->mTexturesToSave.erase(id);
				me->updateTitleProgress();
				me->mTimer.reset();
			}
		}
	}

};

void DAESaver::add(const LLViewerObject* prim, const std::string name)
{
	mObjects.push_back(std::pair<LLViewerObject*,std::string>((LLViewerObject*)prim, name));
}

void DAESaver::updateTextureInfo()
{
	mTextures.clear();
	mTextureNames.clear();

	for (obj_info_t::iterator obj_iter = mObjects.begin(); obj_iter != mObjects.end(); ++obj_iter)
	{
		LLViewerObject* obj = obj_iter->first;
		S32 num_faces = obj->getVolume()->getNumVolumeFaces();
		for (S32 face_num = 0; face_num < num_faces; ++face_num)
		{
			LLTextureEntry* te = obj->getTE(face_num);
			const LLUUID id = te->getID();
			if (std::find(mTextures.begin(), mTextures.end(), id) != mTextures.end())
			{
				continue;
			}
			mTextures.push_back(id);
			std::string name;
			if (id != DAEExportUtil::LL_TEXTURE_BLANK && DAEExportUtil::canExportTexture(id, &name))
			{
				std::string safe_name = gDirUtilp->getScrubbedFileName(name);
				std::replace(safe_name.begin(), safe_name.end(), ' ', '_');
				mTextureNames.push_back(safe_name);
			}
			else
			{
				mTextureNames.push_back(std::string());
			}
		}
	}
}


class v4adapt
{
private:
	LLStrider<LLVector4a> mV4aStrider;
public:
	v4adapt(LLVector4a* vp){ mV4aStrider = vp; }
	inline LLVector3 operator[] (const unsigned int i)
	{
		return LLVector3((F32*)&mV4aStrider[i]);
	}
};

void DAESaver::addSource(daeElement* mesh, const char* src_id, std::string params, const std::vector<F32> &vals)
{
	daeElement* source = mesh->add("source");
	source->setAttribute("id", src_id);
	daeElement* src_array = source->add("float_array");

	src_array->setAttribute("id", llformat("%s-%s", src_id, "array").c_str());
	src_array->setAttribute("count", llformat("%d", vals.size()).c_str());

	for (U32 i = 0; i < vals.size(); i++)
	{
		((domFloat_array*)src_array)->getValue().append(vals[i]);
	}

	domAccessor* acc = daeSafeCast<domAccessor>(source->add("technique_common accessor"));
	acc->setSource(llformat("#%s-%s", src_id, "array").c_str());
	acc->setCount(vals.size() / params.size());
	acc->setStride(params.size());

	for (std::string::iterator p_iter = params.begin(); p_iter != params.end(); ++p_iter)
	{
		domElement* pX = acc->add("param");
		pX->setAttribute("name", llformat("%c", *p_iter).c_str());
		pX->setAttribute("type", "float");
	}
}

void DAESaver::addPolygons(daeElement* mesh, const char* geomID, const char* materialID, LLViewerObject* obj, int_list_t* faces_to_include)
{
	domPolylist* polylist = daeSafeCast<domPolylist>(mesh->add("polylist"));
	polylist->setMaterial(materialID);

	// Vertices semantic
	{
		domInputLocalOffset* input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
		input->setSemantic("VERTEX");
		input->setOffset(0);
		input->setSource(llformat("#%s-%s", geomID, "vertices").c_str());
	}

	// Normals semantic
	{
		domInputLocalOffset* input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
		input->setSemantic("NORMAL");
		input->setOffset(0);
		input->setSource(llformat("#%s-%s", geomID, "normals").c_str());
	}

	// UV semantic
	{
		domInputLocalOffset* input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
		input->setSemantic("TEXCOORD");
		input->setOffset(0);
		input->setSource(llformat("#%s-%s", geomID, "map0").c_str());
	}

	// Save indices
	domP* p = daeSafeCast<domP>(polylist->add("p"));
	domPolylist::domVcount *vcount = daeSafeCast<domPolylist::domVcount>(polylist->add("vcount"));
	S32 index_offset = 0;
	S32 num_tris = 0;
	for (S32 face_num = 0; face_num < obj->getVolume()->getNumVolumeFaces(); face_num++)
	{
		if (skipFace(obj->getTE(face_num))) continue;

		const LLVolumeFace* face = (LLVolumeFace*)&obj->getVolume()->getVolumeFace(face_num);

		if (faces_to_include == NULL || (std::find(faces_to_include->begin(), faces_to_include->end(), face_num) != faces_to_include->end()))
		{
			for (S32 i = 0; i < face->mNumIndices; i++)
			{
				U16 index = index_offset + face->mIndices[i];
				(p->getValue()).append(index);
				if (i % 3 == 0)
				{
					(vcount->getValue()).append(3);
					num_tris++;
				}
			}
		}
		index_offset += face->mNumVertices;
	}
	polylist->setCount(num_tris);
}

void DAESaver::transformTexCoord(S32 num_vert, LLVector2* coord, LLVector3* positions, LLVector3* normals, LLTextureEntry* te, LLVector3 scale)
{
	F32 cosineAngle = cos(te->getRotation());
	F32 sinAngle = sin(te->getRotation());

	for (S32 ii=0; ii<num_vert; ii++)
	{
		if (LLTextureEntry::TEX_GEN_PLANAR == te->getTexGen())
		{
			LLVector3 normal = normals[ii];
			LLVector3 pos = positions[ii];
			LLVector3 binormal;
			F32 d = normal * LLVector3::x_axis;
			if (d >= 0.5f || d <= -0.5f)
			{
				binormal = LLVector3::y_axis;
				if (normal.mV[0] < 0) binormal *= -1.0f;
			}
			else
			{
				binormal = LLVector3::x_axis;
				if (normal.mV[1] > 0) binormal *= -1.0f;
			}
			LLVector3 tangent = binormal % normal;
			LLVector3 scaledPos = pos.scaledVec(scale);
			coord[ii].mV[0] = 1.f + ((binormal * scaledPos) * 2.f - 0.5f);
			coord[ii].mV[1] = -((tangent * scaledPos) * 2.f - 0.5f);
		}

		F32 repeatU;
		F32 repeatV;
		te->getScale(&repeatU, &repeatV);
		F32 tX = coord[ii].mV[0] - 0.5f;
		F32 tY = coord[ii].mV[1] - 0.5f;

		F32 offsetU;
		F32 offsetV;
		te->getOffset(&offsetU, &offsetV);

		coord[ii].mV[0] = (tX * cosineAngle + tY * sinAngle) * repeatU + offsetU + 0.5f;
		coord[ii].mV[1] = (-tX * sinAngle + tY * cosineAngle) * repeatV + offsetV + 0.5f;
	}
}

bool DAESaver::saveDAE(std::string filename)
{
	mAllMaterials.clear();
	mTotalNumMaterials = 0;
	DAE dae;
	// First set the filename to save
	daeElement* root = dae.add(filename);

	// Obligatory elements in header
	daeElement* asset = root->add("asset");
	// Get ISO format time
	time_t rawtime;
	time(&rawtime);
	struct tm* utc_time = gmtime(&rawtime);
	std::string date = llformat("%04d-%02d-%02dT%02d:%02d:%02d", utc_time->tm_year + 1900, utc_time->tm_mon + 1, utc_time->tm_mday, utc_time->tm_hour, utc_time->tm_min, utc_time->tm_sec);
	daeElement* created = asset->add("created");
	created->setCharData(date);
	daeElement* modified = asset->add("modified");
	modified->setCharData(date);
	daeElement* unit = asset->add("unit");
	unit->setAttribute("name", "meter");
	unit->setAttribute("value", "1");
	daeElement* up_axis = asset->add("up_axis");
	up_axis->setCharData("Z_UP");

	// File creator
	daeElement* contributor = asset->add("contributor");
	contributor->add("author")->setCharData(LLAppViewer::instance()->getSecondLifeTitle() + " User");
	contributor->add("authoring_tool")->setCharData(LLAppViewer::instance()->getSecondLifeTitle() + " Collada Export");

	daeElement* images = root->add("library_images");
	daeElement* geomLib = root->add("library_geometries");
	daeElement* effects = root->add("library_effects");
	daeElement* materials = root->add("library_materials");
	daeElement* scene = root->add("library_visual_scenes visual_scene");
	scene->setAttribute("id", "Scene");
	scene->setAttribute("name", "Scene");

	if (gSavedSettings.getBOOL("DAEExportTextures"))
	{
		generateImagesSection(images);
	}

	S32 prim_nr = 0;

	for (obj_info_t::iterator obj_iter = mObjects.begin(); obj_iter != mObjects.end(); ++obj_iter)
	{
		LLViewerObject* obj = obj_iter->first;
		S32 total_num_vertices = 0;

		std::string name = "";
		if (name.empty()) name = llformat("prim%d", prim_nr++);

		const char* geomID = name.c_str();

		daeElement* geom = geomLib->add("geometry");
		geom->setAttribute("id", llformat("%s-%s", geomID, "mesh").c_str());
		daeElement* mesh = geom->add("mesh");

		std::vector<F32> position_data;
		std::vector<F32> normal_data;
		std::vector<F32> uv_data;
		bool applyTexCoord = gSavedSettings.getBOOL("DAEExportTextureParams");

		S32 num_faces = obj->getVolume()->getNumVolumeFaces();
		for (S32 face_num = 0; face_num < num_faces; face_num++)
		{
			if (skipFace(obj->getTE(face_num))) continue;

			const LLVolumeFace* face = (LLVolumeFace*)&obj->getVolume()->getVolumeFace(face_num);
			total_num_vertices += face->mNumVertices;

			v4adapt verts(face->mPositions);
			v4adapt norms(face->mNormals);

			LLVector2* newCoord = NULL;

			if (applyTexCoord)
			{
				newCoord = new LLVector2[face->mNumVertices];
				LLVector3* newPos = new LLVector3[face->mNumVertices];
				LLVector3* newNormal = new LLVector3[face->mNumVertices];
				for (S32 i = 0; i < face->mNumVertices; i++)
				{
					newPos[i] = verts[i];
					newNormal[i] = norms[i];
					newCoord[i] = face->mTexCoords[i];
				}
				transformTexCoord(face->mNumVertices, newCoord, newPos, newNormal, obj->getTE(face_num), obj->getScale());
				delete[] newPos;
				delete[] newNormal;
			}

			for (S32 i=0; i < face->mNumVertices; i++)
			{
				const LLVector3 v = verts[i];
				position_data.push_back(v.mV[VX]);
				position_data.push_back(v.mV[VY]);
				position_data.push_back(v.mV[VZ]);

				const LLVector3 n = norms[i];
				normal_data.push_back(n.mV[VX]);
				normal_data.push_back(n.mV[VY]);
				normal_data.push_back(n.mV[VZ]);

				const LLVector2 uv = applyTexCoord ? newCoord[i] : face->mTexCoords[i];

				uv_data.push_back(uv.mV[VX]);
				uv_data.push_back(uv.mV[VY]);
			}

			if (applyTexCoord)
			{
				delete[] newCoord;
			}
		}


		addSource(mesh, llformat("%s-%s", geomID, "positions").c_str(), "XYZ", position_data);
		addSource(mesh, llformat("%s-%s", geomID, "normals").c_str(), "XYZ", normal_data);
		addSource(mesh, llformat("%s-%s", geomID, "map0").c_str(), "ST", uv_data);

		// Add the <vertices> element
		{
			daeElement*	verticesNode = mesh->add("vertices");
			verticesNode->setAttribute("id", llformat("%s-%s", geomID, "vertices").c_str());
			daeElement* verticesInput = verticesNode->add("input");
			verticesInput->setAttribute("semantic", "POSITION");
			verticesInput->setAttribute("source", llformat("#%s-%s", geomID, "positions").c_str());
		}

		material_list_t objMaterials;
		getMaterials(obj, &objMaterials);

		// Add triangles
		if (gSavedSettings.getBOOL("DAEExportConsolidateMaterials"))
		{
			for (U32 objMaterial = 0; objMaterial < objMaterials.size(); objMaterial++)
			{
				int_list_t faces;
				getFacesWithMaterial(obj, objMaterials[objMaterial], &faces);
				std::string matName = objMaterials[objMaterial].name;
				addPolygons(mesh, geomID, (matName + "-material").c_str(), obj, &faces);
			}
		}
		else
		{
			S32 mat_nr = 0;
			for (S32 face_num = 0; face_num < num_faces; face_num++)
			{
				if (skipFace(obj->getTE(face_num))) continue;
				int_list_t faces;
				faces.push_back(face_num);
				std::string matName = objMaterials[mat_nr++].name;
				addPolygons(mesh, geomID, (matName + "-material").c_str(), obj, &faces);
			}
		}

		daeElement* node = scene->add("node");
		node->setAttribute("type", "NODE");
		node->setAttribute("id", geomID);
		node->setAttribute("name", geomID);

		// Set tranform matrix (node position, rotation and scale)
		domMatrix* matrix = (domMatrix*)node->add("matrix");
		LLXform srt;
		srt.setScale(obj->getScale());
		srt.setPosition(obj->getRenderPosition() + mOffset);
		srt.setRotation(obj->getRenderRotation());
		LLMatrix4 m4;
		srt.getLocalMat4(m4);
		for (int i=0; i<4; i++)
			for (int j=0; j<4; j++)
				(matrix->getValue()).append(m4.mMatrix[j][i]);

		// Geometry of the node
		daeElement* nodeGeometry = node->add("instance_geometry");

		// Bind materials
		daeElement* tq = nodeGeometry->add("bind_material technique_common");
		for (U32 objMaterial = 0; objMaterial < objMaterials.size(); objMaterial++)
		{
			std::string matName = objMaterials[objMaterial].name;
			daeElement* instanceMaterial = tq->add("instance_material");
			instanceMaterial->setAttribute("symbol", (matName + "-material").c_str());
			instanceMaterial->setAttribute("target", ("#" + matName + "-material").c_str());
		}

		nodeGeometry->setAttribute("url", llformat("#%s-%s", geomID, "mesh").c_str());

	}

	// Effects (face texture, color, alpha)
	generateEffects(effects);

	// Materials
	for (U32 objMaterial = 0; objMaterial < mAllMaterials.size(); objMaterial++)
	{
		daeElement* mat = materials->add("material");
		mat->setAttribute("id", (mAllMaterials[objMaterial].name + "-material").c_str());
		daeElement* matEffect = mat->add("instance_effect");
		matEffect->setAttribute("url", ("#" + mAllMaterials[objMaterial].name + "-fx").c_str());
	}

	root->add("scene instance_visual_scene")->setAttribute("url", "#Scene");

	return dae.writeAll();
}

DAESaver::DAESaver()
{}

bool DAESaver::skipFace(LLTextureEntry *te)
{
	return (gSavedSettings.getBOOL("DAEExportSkipTransparent")
		&& (te->getColor().mV[3] < 0.01f || te->getID() == DAEExportUtil::LL_TEXTURE_TRANSPARENT));
}

DAESaver::MaterialInfo DAESaver::getMaterial(LLTextureEntry* te)
{
	if (gSavedSettings.getBOOL("DAEExportConsolidateMaterials"))
	{
		for (U32 i=0; i < mAllMaterials.size(); i++)
		{
			if (mAllMaterials[i].matches(te))
			{
				return mAllMaterials[i];
			}
		}
	}

	MaterialInfo ret;
	ret.textureID = te->getID();
	ret.color = te->getColor();
	ret.name = llformat("Material%d", mAllMaterials.size());
	mAllMaterials.push_back(ret);
	return mAllMaterials[mAllMaterials.size() - 1];
}

void DAESaver::getMaterials(LLViewerObject* obj, material_list_t* ret)
{
	S32 num_faces = obj->getVolume()->getNumVolumeFaces();
	for (S32 face_num = 0; face_num < num_faces; ++face_num)
	{
		LLTextureEntry* te = obj->getTE(face_num);

		if (skipFace(te))
		{
			continue;
		}

		MaterialInfo mat = getMaterial(te);

		if (!gSavedSettings.getBOOL("DAEExportConsolidateMaterials") || std::find(ret->begin(), ret->end(), mat) == ret->end())
		{
			ret->push_back(mat);
		}
	}
}

void DAESaver::getFacesWithMaterial(LLViewerObject* obj, MaterialInfo& mat, int_list_t* ret)
{
	S32 num_faces = obj->getVolume()->getNumVolumeFaces();
	for (S32 face_num = 0; face_num < num_faces; ++face_num)
	{
		if (mat == getMaterial(obj->getTE(face_num)))
		{
			ret->push_back(face_num);
		}
	}
}

void DAESaver::generateEffects(daeElement *effects)
{
	// Effects (face color, alpha)
	bool export_textures = gSavedSettings.getBOOL("DAEExportTextures");

	for (U32 mat = 0; mat < mAllMaterials.size(); mat++)
	{
		LLColor4 color = mAllMaterials[mat].color;
		domEffect* effect = (domEffect*)effects->add("effect");
		effect->setId((mAllMaterials[mat].name + "-fx").c_str());
		daeElement* profile = effect->add("profile_COMMON");
		std::string colladaName;

		if (export_textures)
		{
			LLUUID textID;
			U32 i = 0;
			for (; i < mTextures.size(); i++)
			{
				if (mAllMaterials[mat].textureID == mTextures[i])
				{
					textID = mTextures[i];
					break;
				}
			}

			if (!textID.isNull() && !mTextureNames[i].empty())
			{
				colladaName = mTextureNames[i] + "_" + mImageFormat;
				daeElement* newparam = profile->add("newparam");
				newparam->setAttribute("sid", (colladaName + "-surface").c_str());
				daeElement* surface = newparam->add("surface");
				surface->setAttribute("type", "2D");
				surface->add("init_from")->setCharData(colladaName.c_str());
				newparam = profile->add("newparam");
				newparam->setAttribute("sid", (colladaName + "-sampler").c_str());
				newparam->add("sampler2D source")->setCharData((colladaName + "-surface").c_str());
			}
		}

		daeElement* t = profile->add("technique");
		t->setAttribute("sid", "common");
		domElement* phong = t->add("phong");
		domElement* diffuse = phong->add("diffuse");
		// Only one <color> or <texture> can appear inside diffuse element
		if (!colladaName.empty())
		{
			daeElement* txtr = diffuse->add("texture");
			txtr->setAttribute("texture", (colladaName + "-sampler").c_str());
			txtr->setAttribute("texcoord", colladaName.c_str());
		}
		else
		{
			daeElement* diffuseColor = diffuse->add("color");
			diffuseColor->setAttribute("sid", "diffuse");
			diffuseColor->setCharData(llformat("%f %f %f %f", color.mV[0], color.mV[1], color.mV[2], color.mV[3]).c_str());
			phong->add("transparency float")->setCharData(llformat("%f", color.mV[3]).c_str());
		}
	}
}

void DAESaver::generateImagesSection(daeElement* images)
{
	for (U32 i=0; i < mTextureNames.size(); i++)
	{
		std::string name = mTextureNames[i];
		if (name.empty()) continue;
		std::string colladaName = name + "_" + mImageFormat;
		daeElement* image = images->add("image");
		image->setAttribute("id", colladaName.c_str());
		image->setAttribute("name", colladaName.c_str());
		image->add("init_from")->setCharData(LLURI::escape(name + "." + mImageFormat));
	}
}

class DAESaveSelectedObjects : public view_listener_t
{
	bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
	{
		(new ColladaExportFloater())->open();
		return true;
	}
};

void addMenu(view_listener_t* menu, const std::string& name);
void add_dae_listeners() // Called in llviewermenu with other addMenu calls, function linked against
{
	addMenu(new DAESaveSelectedObjects(), "Object.SaveAsDAE");
}
