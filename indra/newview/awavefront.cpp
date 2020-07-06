/**
 * @file awavefront.cpp
 * @brief A system which allows saving in-world objects to Wavefront .OBJ files for offline texturizing/shading.
 * @authors Apelsin, Lirusaito
 *
 * $LicenseInfo:firstyear=2011&license=LGPLV3$
 * Copyright (C) 2011-2013 Apelsin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#include "llviewerprecompiledheaders.h"

#include "awavefront.h"

// library includes
#include "aifilepicker.h"
#include "llnotificationsutil.h"

// newview includes
#include "lfsimfeaturehandler.h"
#include "llavatarappearancedefines.h"
#include "llface.h"
#include "llvoavatar.h"
#include "llvovolume.h"
#include "llviewerinventory.h"
#include "llinventorymodel.h"
#include "llinventoryfunctions.h"

// menu includes
#include "llevent.h"
#include "llmemberlistener.h"
#include "llview.h"
#include "llselectmgr.h"

LLVOAvatar* find_avatar_from_object(LLViewerObject* object);
extern LLUUID gAgentID;

//Typedefs used in other files, using here for consistency.
typedef std::vector<LLAvatarJoint*> avatar_joint_list_t;
typedef LLMemberListener<LLView> view_listener_t;

namespace
{
	const std::string OBJ(".obj");

	void save_wavefront_continued(WavefrontSaver* wfsaver, AIFilePicker* filepicker)
	{
		if (filepicker->hasFilename())
		{
			const std::string selected_filename = filepicker->getFilename();
			if (LLFILE* fp = LLFile::fopen(selected_filename, "wb"))
			{
				wfsaver->saveFile(fp);
				LL_INFOS() << "OBJ file saved to " << selected_filename << LL_ENDL;
				if (gSavedSettings.getBOOL("OBJExportNotifySuccess"))
					LLNotificationsUtil::add("WavefrontExportSuccess", LLSD().with("FILENAME", selected_filename));
				fclose(fp);
			}
			else LL_ERRS() << "can't open: " << selected_filename << LL_ENDL;
		}
		else LL_WARNS() << "No file; bailing" << LL_ENDL;

		delete wfsaver;
	}

	void save_wavefront_picker(WavefrontSaver* wfsaver, std::string name)
	{
		AIFilePicker* filepicker = AIFilePicker::create();
		filepicker->open(name);
		filepicker->run(boost::bind(&save_wavefront_continued, wfsaver, filepicker));
	}

	void save_wavefront_on_confirm(const LLSD& notification, const LLSD& response, WavefrontSaver* wfsaver, std::string name)
	{
		if (LLNotificationsUtil::getSelectedOption(notification, response) == 0) // 0 - Proceed, first choice
		{
			save_wavefront_picker(wfsaver, name);
		}
		else
		{
			delete wfsaver;
		}
	}
}

Wavefront::Wavefront(vert_t v, tri_t t)
:	name("")
,	vertices(v)
,	triangles(t)
{
}

Wavefront::Wavefront(const LLVolumeFace* face, const LLXform* transform, const LLXform* transform_normals)
:	name("")
{
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
	v4adapt verts(face->mPositions);
	for (S32 i = 0; i < face->mNumVertices; ++i)
	{
		LLVector3 v = verts[i];
		vertices.push_back(std::pair<LLVector3, LLVector2>(v, face->mTexCoords[i]));
	}

	if (transform) Transform(vertices, transform);

	v4adapt norms(face->mNormals);
	for (S32 i = 0; i < face->mNumVertices; ++i)
		normals.push_back(norms[i]);

	if (transform_normals) Transform(normals, transform_normals);

	for (S32 i = 0; i < face->mNumIndices/3; ++i)
	{
		triangles.push_back(tri(face->mIndices[i*3+0], face->mIndices[i*3+1], face->mIndices[i*3+2]));
	}
}

Wavefront::Wavefront(LLFace* face, LLPolyMesh* mesh, const LLXform* transform, const LLXform* transform_normals)
:	name("")
{
	LLVertexBuffer* vb = face->getVertexBuffer();
	if (!vb) return;

	LLStrider<LLVector3> getVerts;
	LLStrider<LLVector3> getNorms;
	LLStrider<LLVector2> getCoord;
	LLStrider<U16> getIndices;
	face->getGeometry(getVerts, getNorms, getCoord, getIndices);

	const U16 start = face->getGeomStart();
	const U32 end = start + (mesh ? mesh->getNumVertices() : vb->getNumVerts()) - 1; //vertices
	for (U32 i = start; i <= end; ++i)
		vertices.push_back(std::make_pair(getVerts[i], getCoord[i]));

	if (transform) Transform(vertices, transform);

	for (U32 i = start; i <= end; ++i)
		normals.push_back(getNorms[i]);

	if (transform_normals) Transform(normals, transform_normals);

	const U32 pcount = mesh ? mesh->getNumFaces() : (vb->getNumIndices()/3); //indices
	const U32 offset = face->getIndicesStart(); //indices
	for (U32 i = 0; i < pcount; ++i)
	{
		const auto off = i * 3 + offset;
		triangles.push_back(tri(getIndices[off] + start, getIndices[off + 1] + start, getIndices[off + 2] + start));
	}
}

void Wavefront::Transform(vert_t& v, const LLXform* x) //recursive
{
	LLMatrix4 m;
	x->getLocalMat4(m);
	for (auto& i : v)
	{
		i.first = i.first * m;
	}

	if (const LLXform* xp = x->getParent()) Transform(v, xp);
}

void Wavefront::Transform(vec3_t& v, const LLXform* x) //recursive
{
	LLMatrix4 m;
	x->getLocalMat4(m);
	for (auto& i : v)
	{
		i = i * m;
	}

	if (const LLXform* xp = x->getParent()) Transform(v, xp);
}

WavefrontSaver::WavefrontSaver()
{}

void WavefrontSaver::Add(const Wavefront& obj)
{
	obj_v.push_back(obj);
}

void WavefrontSaver::Add(const LLVolume* vol, const LLXform* transform, const LLXform* transform_normals)
{
	const int faces = vol->getNumVolumeFaces();
	for(int i = 0; i < faces; ++i) //each face will be treated as a separate Wavefront object
	{
		Add(Wavefront(&vol->getVolumeFace(i), transform, transform_normals));
	}
}
void WavefrontSaver::Add(const LLViewerObject* some_vo)
{
	LLXform v_form;
	v_form.setScale(some_vo->getScale());
	v_form.setPosition(some_vo->getRenderPosition());
	v_form.setRotation(some_vo->getRenderRotation());

	LLXform normfix;
	normfix.setRotation(v_form.getRotation()); //Should work...
	Add(some_vo->getVolume(), &v_form, &normfix);
}
namespace
{
	// Identical to the one in daeexport.cpp.
	bool can_export_node(LLSelectNode* node)
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
			const LLSculptParams *sculpt_params = obj->getSculptParams();
			LLUUID sculpt_id = sculpt_params->getSculptTexture();

			// Find inventory items with asset id of the sculpt map
			LLViewerInventoryCategory::cat_array_t cats;
			LLViewerInventoryItem::item_array_t items;
			LLAssetIDMatches asset_id_matches(sculpt_id);
			gInventory.collectDescendentsIf(LLUUID::null,
							cats,
							items,
							LLInventoryModel::INCLUDE_TRASH,
							asset_id_matches);

			// See if any of the inventory items matching this sculpt id are exportable
			for (const auto& item : items)
			{
				const LLPermissions item_permissions = item->getPermissions();
				if (item_permissions.allowExportBy(gAgentID, LFSimFeatureHandler::instance().exportPolicy()))
				{
					return true;
				}
			}

			return false;
		}
		else // not sculpt, we already checked generic permissions
		{
			return true;
		}
	}

	class LFSaveSelectedObjects final : public view_listener_t
	{
		bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata) override
		{
			if (LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection())
			{
				if (!selection->getFirstRootObject())
				{
					if (gSavedSettings.getBOOL("OBJExportNotifyFailed"))
						LLNotificationsUtil::add("ExportFailed");
					return true;
				}

				WavefrontSaver* wfsaver = new WavefrontSaver; // deleted in callback
				wfsaver->offset = -selection->getFirstRootObject()->getRenderPosition();
				S32 total = 0;
				S32 included = 0;
				for (LLObjectSelection::iterator iter = selection->begin(); iter != selection->end(); ++iter)
				{
					++total;
					LLSelectNode* node = *iter;
					if (!can_export_node(node)) continue;
					++included;
					wfsaver->Add(node->getObject());
				}
				if (wfsaver->obj_v.empty())
				{
					if (gSavedSettings.getBOOL("OBJExportNotifyFailed"))
						LLNotificationsUtil::add("ExportFailed");
					delete wfsaver;
					return true;
				}

				if (total != included)
				{
					LLSD args;
					args["TOTAL"] = total;
					args["FAILED"] = total - included;
					LLNotificationsUtil::add("WavefrontExportPartial", args, LLSD(), boost::bind(&save_wavefront_on_confirm, _1, _2, wfsaver, selection->getFirstNode()->mName.c_str() + OBJ));
					return true;
				}

				save_wavefront_picker(wfsaver, selection->getFirstNode()->mName.c_str() + OBJ);
			}
			return true;
		}
	};
}

void WavefrontSaver::Add(const LLVOAvatar* av_vo) //adds attachments, too!
{
	offset = -av_vo->getRenderPosition();
	avatar_joint_list_t vjv = av_vo->mMeshLOD;
	for (const auto& itervj : vjv)
	{
		const auto* vj = dynamic_cast<const LLViewerJoint*>(itervj);
		if (!vj || vj->mMeshParts.empty()) continue;

		auto* vjm = dynamic_cast<LLViewerJointMesh*>(vj->mMeshParts[0]); //highest LOD
		if (!vjm) continue;

		vjm->updateJointGeometry();
		LLFace* face = vjm->mFace;
		if (!face) continue;

		//Beware: this is a hack because LLFace has multiple LODs
		//'pm' supplies the right number of vertices and triangles!
		LLPolyMesh* pm = vjm->getMesh();
		if (!pm) continue;
		LLXform normfix;
		normfix.setRotation(pm->getRotation());

		//Special case for balls...I mean eyeballs!
		static const std::string eyeLname = LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::getInstance()->getMeshEntry(LLAvatarAppearanceDefines::MESH_ID_EYEBALL_LEFT)->mName;
		static const std::string eyeRname = LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::getInstance()->getMeshEntry(LLAvatarAppearanceDefines::MESH_ID_EYEBALL_RIGHT)->mName;
		const std::string name = vj->getName();
		if (name == eyeLname || name == eyeRname)
		{
			LLXform lol;
			lol.setPosition(-offset);
			Add(Wavefront(face, pm, &lol, &normfix));
		}
		else
			Add(Wavefront(face, pm, NULL, &normfix));
	}

	for (const auto& ap : av_vo->mAttachmentPoints)
	{
		LLViewerJointAttachment* ja = ap.second;
		if (!ja) continue;

		for (const auto& o : ja->mAttachedObjects)
		{
			if (!o) continue;

			std::vector<LLViewerObject*> prims;
			o->addThisAndAllChildren(prims);
			for (const auto& c : prims)
			{
				if (!c) continue;
				if (LLSelectNode* n = LLSelectMgr::getInstance()->getSelection()->findNode(const_cast<LLViewerObject*>(c)))
				{
					if (!can_export_node(n)) continue;
				}
				else continue;
				const LLVolume* vol = c->getVolume();
				if (!vol) continue;

				LLXform v_form;
				v_form.setScale(c->getScale());
				v_form.setPosition(c->getRenderPosition());
				v_form.setRotation(c->getRenderRotation());

				LLXform normfix;
				normfix.setRotation(v_form.getRotation());

				if (c->isHUDAttachment())
				{
					v_form.addPosition(-offset);
					//Normals of HUD elements are funky
					//TO-DO: fix 'em!
				}
				Add(vol, &v_form, &normfix);
			}
		}
	}
}
namespace
{
	class LFSaveSelectedAvatar final : public view_listener_t
	{
		bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata) override
		{
			if (const LLVOAvatar* avatar = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject()))
			{
				if (!avatar->isSelf())
				{
					if (gSavedSettings.getBOOL("OBJExportNotifyFailed")) LLNotificationsUtil::add("ExportFailed");
					return true;
				}
				WavefrontSaver* wfsaver = new WavefrontSaver; // deleted in callback
				wfsaver->Add(avatar);
				if (wfsaver->obj_v.empty())
				{
					if (gSavedSettings.getBOOL("OBJExportNotifyFailed"))
						LLNotificationsUtil::add("ExportFailed");
					delete wfsaver;
					return true;
				}

				AIFilePicker* filepicker = AIFilePicker::create();
				filepicker->open(avatar->getFullname()+OBJ);
				filepicker->run(boost::bind(save_wavefront_continued, wfsaver, filepicker));
			}
			return true;
		}
	};
}

namespace
{
	void write_or_bust(LLFILE* fp, const std::string outstring)
	{
		const size_t size = outstring.length();
		if (fwrite(outstring.c_str(), 1, size, fp) != size)
			LL_WARNS() << "Short write" << LL_ENDL;
	}
}

bool WavefrontSaver::saveFile(LLFILE* fp)
{
	if (!fp) return false;

	int num = 0;
	int index = 0;
	for (const auto& obj : obj_v)
	{
		int count = 0;

		std::string name = obj.name;
		if (name.empty()) name = llformat("%d", num++);

		auto& vertices = obj.vertices;
		auto& normals = obj.normals;
		auto& triangles = obj.triangles;

		//Write Object
		write_or_bust(fp, "o " + name + '\n');

		//Write vertices; swap axes if necessary
		static const LLCachedControl<bool> swapYZ("OBJExportSwapYZ", false);
		const double xm = swapYZ ? -1.0 : 1.0;
		const int y = swapYZ ? 2 : 1;
		const int z = swapYZ ? 1 : 2;
		for (const auto& vert : vertices)
		{
			++count;
			const LLVector3 v = vert.first + offset;
			write_or_bust(fp, llformat("v %f %f %f\n",v[0] * xm, v[y], v[z]));
		}

		for (const auto& n : normals)
		{
			write_or_bust(fp, llformat("vn %f %f %f\n",n[0] * xm, n[y], n[z]));
		}

		for (const auto& vert : vertices)
		{
			write_or_bust(fp, llformat("vt %f %f\n", vert.second[0], vert.second[1]));
		}

		//Write triangles
		for (const auto& triangle : triangles)
		{
			const int f1 = triangle.v0 + index + 1;
			const int f2 = triangle.v1 + index + 1;
			const int f3 = triangle.v2 + index + 1;
			write_or_bust(fp, llformat("f %d/%d/%d %d/%d/%d %d/%d/%d\n",
										  f1,f1,f1,f2,f2,f2,f3,f3,f3));
		}
		index += count;
	}

	return true;
}

void addMenu(view_listener_t* menu, const std::string& name);
void add_wave_listeners() // Called in llviewermenu with other addMenu calls, function linked against
{
	addMenu(new LFSaveSelectedObjects(), "Object.SaveAsOBJ");
	addMenu(new LFSaveSelectedAvatar(), "Avatar.SaveAsOBJ");
}

