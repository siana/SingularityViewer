/** 
 * @file llpolymorph.cpp
 * @brief Implementation of LLPolyMesh class
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------

#include "linden_common.h"
#include "llpolymorph.h"
#include "llavatarappearance.h"
#include "llavatarjoint.h"
//#include "llwearable.h"
#include "llxmltree.h"
#include "llendianswizzle.h"
#include "llpolymesh.h"
#include "v2math.h"

//#include "../tools/imdebug/imdebug.h"

const F32 NORMAL_SOFTEN_FACTOR = 0.65f;
const F32 SIGNIFICANT_DELTA    = 0.0001f;

//-----------------------------------------------------------------------------
// LLPolyMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData::LLPolyMorphData(const std::string& morph_name)
	: mName(morph_name)
{
	mNumIndices = 0;
	mCurrentIndex = 0;
	mTotalDistortion = 0.f;
	mAvgDistortion.clear();
	mMaxDistortion = 0.f;
	mVertexIndices = NULL;
	mCoords = NULL;
	mNormals = NULL;
	mBinormals = NULL;
	mTexCoords = NULL;

	mMesh = NULL;
}

LLPolyMorphData::LLPolyMorphData(const LLPolyMorphData &rhs) :
	mName(rhs.mName),
	mNumIndices(rhs.mNumIndices),
	mTotalDistortion(rhs.mTotalDistortion),
	mAvgDistortion(rhs.mAvgDistortion),
	mMaxDistortion(rhs.mMaxDistortion),
	mVertexIndices(NULL),
	mCoords(NULL),
	mNormals(NULL),
	mBinormals(NULL),
	mTexCoords(NULL)
{
	const S32 numVertices = mNumIndices;

	U32 size = sizeof(LLVector4a)*numVertices;

	mCoords = static_cast<LLVector4a*>( ll_aligned_malloc_16(size) );
	mNormals = static_cast<LLVector4a*>( ll_aligned_malloc_16(size) );
	mBinormals = static_cast<LLVector4a*>( ll_aligned_malloc_16(size) );
	mTexCoords = new LLVector2[numVertices];
	mVertexIndices = new U32[numVertices];
	
	for (S32 v=0; v < numVertices; v++)
	{
		mCoords[v] = rhs.mCoords[v];
		mNormals[v] = rhs.mNormals[v];
		mBinormals[v] = rhs.mBinormals[v];
		mTexCoords[v] = rhs.mTexCoords[v];
		mVertexIndices[v] = rhs.mVertexIndices[v];
	}
}

//-----------------------------------------------------------------------------
// ~LLPolyMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData::~LLPolyMorphData()
{
	freeData();
}

//-----------------------------------------------------------------------------
// loadBinary()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::loadBinary(LLFILE *fp, LLPolyMeshSharedData *mesh)
{
	S32 numVertices;
	size_t numRead; // <alchemy/>

	numRead = fread(&numVertices, sizeof(S32), 1, fp);
	llendianswizzle(&numVertices, sizeof(S32), 1);
	if (numRead != 1)
	{
		LL_WARNS() << "Can't read number of morph target vertices" << LL_ENDL;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// free any existing data
	//-------------------------------------------------------------------------
	freeData();

	//-------------------------------------------------------------------------
	// allocate vertices
	//-------------------------------------------------------------------------
	
	U32 size = sizeof(LLVector4a)*numVertices;
	
	mCoords = static_cast<LLVector4a*>(ll_aligned_malloc_16(size));
	mNormals = static_cast<LLVector4a*>(ll_aligned_malloc_16(size));
	mBinormals = static_cast<LLVector4a*>(ll_aligned_malloc_16(size));
	
	mTexCoords = new LLVector2[numVertices];
	// Actually, we are allocating more space than we need for the skiplist
	mVertexIndices = new U32[numVertices];
	mNumIndices = 0;
	mTotalDistortion = 0.f;
	mMaxDistortion = 0.f;
	mAvgDistortion.clear();
	mMesh = mesh;

	//-------------------------------------------------------------------------
	// read vertices
	//-------------------------------------------------------------------------
	for(S32 v = 0; v < numVertices; v++)
	{
		numRead = fread(&mVertexIndices[v], sizeof(U32), 1, fp);
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);
		if (numRead != 1)
		{
			LL_WARNS() << "Can't read morph target vertex number" << LL_ENDL;
			return FALSE;
		}

		if (mVertexIndices[v] > 10000)
		{
			LL_ERRS() << "Bad morph index: " << mVertexIndices[v] << LL_ENDL;
		}


		numRead = fread(&mCoords[v], sizeof(F32), 3, fp);
		llendianswizzle(&mCoords[v], sizeof(F32), 3);
		if (numRead != 3)
		{
			LL_WARNS() << "Can't read morph target vertex coordinates" << LL_ENDL;
			return FALSE;
		}

		F32 magnitude = mCoords[v].getLength3().getF32();
		
		mTotalDistortion += magnitude;
		LLVector4a t;
		t.setAbs(mCoords[v]);
		mAvgDistortion.add(t);
		
		if (magnitude > mMaxDistortion)
		{
			mMaxDistortion = magnitude;
		}

		numRead = fread(&mNormals[v], sizeof(F32), 3, fp);
		llendianswizzle(&mNormals[v], sizeof(F32), 3);
		if (numRead != 3)
		{
			LL_WARNS() << "Can't read morph target normal" << LL_ENDL;
			return FALSE;
		}

		numRead = fread(&mBinormals[v], sizeof(F32), 3, fp);
		llendianswizzle(&mBinormals[v], sizeof(F32), 3);
		if (numRead != 3)
		{
			LL_WARNS() << "Can't read morph target binormal" << LL_ENDL;
			return FALSE;
		}


		numRead = fread(&mTexCoords[v].mV, sizeof(F32), 2, fp);
		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
		if (numRead != 2)
		{
			LL_WARNS() << "Can't read morph target uv" << LL_ENDL;
			return FALSE;
		}

		mNumIndices++;
	}

	mAvgDistortion.mul(1.f/(F32)mNumIndices);
	mAvgDistortion.normalize3fast();

	return TRUE;
}

//-----------------------------------------------------------------------------
// freeData()
//-----------------------------------------------------------------------------
void LLPolyMorphData::freeData()
{
	if (mCoords != NULL)
	{
		ll_aligned_free_16(mCoords);
		mCoords = NULL;
	}

	if (mNormals != NULL)
	{
		ll_aligned_free_16(mNormals);
		mNormals = NULL;
	}

	if (mBinormals != NULL)
	{
		ll_aligned_free_16(mBinormals);
		mBinormals = NULL;
	}

	if (mTexCoords != NULL)
	{
		delete [] mTexCoords;
		mTexCoords = NULL;
	}

	if (mVertexIndices != NULL)
	{
		delete [] mVertexIndices;
		mVertexIndices = NULL;
	}
}

//-----------------------------------------------------------------------------
// LLPolyMesh::saveLLM()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::saveLLM(LLFILE *fp)
{
	if (!fp)
		return FALSE;

	S32 numVertices = mNumIndices;

	llendianswizzle(&numVertices, sizeof(S32), 1);
	if (fwrite(&numVertices, sizeof(S32), 1, fp) != 1)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}
	numVertices = mNumIndices;	// without the swizzle again

	//-------------------------------------------------------------------------
	// write vertices
	//-------------------------------------------------------------------------
	for(S32 v = 0; v < numVertices; v++)
	{
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);
		if (fwrite(&mVertexIndices[v], sizeof(U32), 1, fp) != 1)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);

		llendianswizzle(mCoords[v].getF32ptr(), sizeof(F32), 3);
		if (fwrite(mCoords[v].getF32ptr(), sizeof(F32), 3, fp) != 3)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		llendianswizzle(mCoords[v].getF32ptr(), sizeof(F32), 3);

		llendianswizzle(mNormals[v].getF32ptr(), sizeof(F32), 3);
		if (fwrite(mNormals[v].getF32ptr(), sizeof(F32), 3, fp) != 3)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		llendianswizzle(mNormals[v].getF32ptr(), sizeof(F32), 3);

		llendianswizzle(mBinormals[v].getF32ptr(), sizeof(F32), 3);
		if (fwrite(mBinormals[v].getF32ptr(), sizeof(F32), 3, fp) != 3)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		llendianswizzle(mBinormals[v].getF32ptr(), sizeof(F32), 3);

		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
		if (fwrite(&mTexCoords[v].mV, sizeof(F32), 2, fp) != 2)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// saveOBJ()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::saveOBJ(LLFILE *fp)
{
	if (!fp)
		return FALSE;

	LLPolyMesh mesh(mMesh, NULL);

	LLVector4a *coords     = mesh.getWritableCoords();
	LLVector4a *normals    = mesh.getWritableNormals();
	LLVector2 *tex_coords = mesh.getWritableTexCoords();

	for(U32 vert_index_morph = 0; vert_index_morph < mNumIndices; vert_index_morph++)
	{
		S32 vert_index_mesh = mVertexIndices[vert_index_morph];

		coords[vert_index_mesh].add(mCoords[vert_index_morph]);
		normals[vert_index_mesh].add(mNormals[vert_index_morph]);
		normals[vert_index_mesh].normalize3();
		tex_coords[vert_index_mesh] += mTexCoords[vert_index_morph];
	}

	return mesh.saveOBJ(fp);
}

//-----------------------------------------------------------------------------
// setMorphFromMesh()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::setMorphFromMesh(LLPolyMesh *morph)
{
	if (!morph)
		return FALSE;

	LLVector4a *morph_coords     = morph->getWritableCoords();
	LLVector4a *morph_normals    = morph->getWritableNormals();
	LLVector4a *morph_binormals  = morph->getWritableBinormals();
	LLVector2 *morph_tex_coords = morph->getWritableTexCoords();

	// We now have the morph loaded as a mesh.  We have to subtract the
	// base mesh to get the delta morph.

	LLPolyMesh delta(mMesh, NULL);
	U32 nverts = delta.getNumVertices();

	LLVector4a *delta_coords     = delta.getWritableCoords();
	LLVector4a *delta_normals    = delta.getWritableNormals();
	LLVector4a *delta_binormals  = delta.getWritableBinormals();
	LLVector2 *delta_tex_coords = delta.getWritableTexCoords();

	U32 num_significant = 0;
	U32 vert_index;
	for( vert_index = 0; vert_index < nverts; vert_index++)
	{
		delta_coords[vert_index].setSub(	morph_coords[vert_index], delta_coords[vert_index]);
		delta_normals[vert_index].setSub(	morph_normals[vert_index], delta_normals[vert_index]);
		delta_binormals[vert_index].setSub(	morph_binormals[vert_index], delta_binormals[vert_index]);
		delta_tex_coords[vert_index] =		morph_tex_coords[vert_index] - delta_tex_coords[vert_index];

		// For the normals and binormals, we really want the deltas
		// to be perpendicular to the mesh (bi)normals in the plane
		// that contains both the mesh and morph (bi)normals, such
		// that the morph (bi)normals form the hypotenuses of right
		// triangles. Right now, we just compute the difference vector.

		if (delta_coords[vert_index].getLength3().getF32()		> SIGNIFICANT_DELTA
		||  delta_normals[vert_index].getLength3().getF32()		> SIGNIFICANT_DELTA
		||  delta_binormals[vert_index].getLength3().getF32()	> SIGNIFICANT_DELTA
		||  delta_tex_coords[vert_index].length()				> SIGNIFICANT_DELTA)
		{
			num_significant++;
		}
	}

	//-------------------------------------------------------------------------
	// compute new morph
	//-------------------------------------------------------------------------

	// If the morph matches the base mesh, we store one vertex to prevent
	// zero length vectors.

	U32 nindices = num_significant;
	if (num_significant == 0)
		nindices = 1;

	LLVector4a* new_coords     = static_cast<LLVector4a*>(ll_aligned_malloc_16(nindices * sizeof(LLVector4a)));
	LLVector4a* new_normals    = static_cast<LLVector4a*>(ll_aligned_malloc_16(nindices * sizeof(LLVector4a)));
	LLVector4a* new_binormals  = static_cast<LLVector4a*>(ll_aligned_malloc_16(nindices * sizeof(LLVector4a)));
	LLVector2* new_tex_coords = new LLVector2[nindices];
	U32* new_vertex_indices   = new U32[nindices];

    // We'll set the distortion directly
	mTotalDistortion = 0.f;
	mMaxDistortion   = 0.f;
	mAvgDistortion.clear();

	U32 morph_index = 0;
	for( vert_index = 0; vert_index < nverts; vert_index++)
	{
		if (delta_coords[vert_index].getLength3().getF32()		> SIGNIFICANT_DELTA
		||  delta_normals[vert_index].getLength3().getF32()		> SIGNIFICANT_DELTA
		||  delta_binormals[vert_index].getLength3().getF32()	> SIGNIFICANT_DELTA
		||  delta_tex_coords[vert_index].length()				> SIGNIFICANT_DELTA
		||  num_significant == 0)
		{
			new_vertex_indices[morph_index] = vert_index;

			new_coords[morph_index]     = delta_coords[vert_index];
			new_normals[morph_index]    = delta_normals[vert_index];
			new_binormals[morph_index]  = delta_binormals[vert_index];
			new_tex_coords[morph_index] = delta_tex_coords[vert_index];

			F32 magnitude = new_coords[morph_index].getLength3().getF32();

			mTotalDistortion += magnitude;
			LLVector4a t;
			t.setAbs(new_coords[morph_index]);
			mAvgDistortion.add(t);

			if (magnitude > mMaxDistortion)
			{
				mMaxDistortion = magnitude;
			}

			morph_index++;
			num_significant = 1;
		}
	}

	mAvgDistortion.mul(1.f/(F32)nindices);
	mAvgDistortion.normalize3();

	//-------------------------------------------------------------------------
	// compute the change in the morph
	//-------------------------------------------------------------------------

	// Because meshes are set by continually updating morph weights
	// there is no easy way to reapply the morphs, so we just compute
	// the change in this morph and apply that appropriately weighted.

	for( morph_index = 0; morph_index < mNumIndices; morph_index++)
	{
		vert_index = mVertexIndices[morph_index];

		delta_coords[vert_index].sub(	mCoords[morph_index]);
		delta_normals[vert_index].sub(	mNormals[morph_index]);
		delta_binormals[vert_index].sub(mBinormals[morph_index]);
		delta_tex_coords[vert_index] -= mTexCoords[morph_index];
	}

	//-------------------------------------------------------------------------
	// Update all avatars
	//-------------------------------------------------------------------------

	std::vector< LLCharacter* >::iterator avatar_it;
	for(avatar_it = LLCharacter::sInstances.begin(); avatar_it != LLCharacter::sInstances.end(); ++avatar_it)
	{
		LLAvatarAppearance* avatarp = (LLAvatarAppearance*)*avatar_it;

		LLPolyMorphTarget* param = (LLPolyMorphTarget*) avatarp->getVisualParam(mName.c_str());
		if (!param)
		{
			continue;
		}

		F32 weight = param->getLastWeight();
		if (weight == 0.0f)
		{
			continue;
		}

		LLPolyMesh* mesh = avatarp->getMesh(mMesh);
		if (!mesh)
		{
			continue;
		}

		// If we have a vertex mask, just remove it.  It will be recreated.
		/*if (param->undoMask(TRUE))
		{
			continue;
		}*/

		LLVector4a *mesh_coords           = mesh->getWritableCoords();
		LLVector4a *mesh_normals          = mesh->getWritableNormals();
		LLVector4a *mesh_binormals        = mesh->getWritableBinormals();
		LLVector2 *mesh_tex_coords        = mesh->getWritableTexCoords();
		LLVector4a *mesh_scaled_normals   = mesh->getScaledNormals();
		LLVector4a *mesh_scaled_binormals = mesh->getScaledBinormals();

		for( vert_index = 0; vert_index < nverts; vert_index++)
		{
			delta_coords[vert_index].mul(weight);
			mesh_coords[vert_index].add(delta_coords[vert_index]);

			mesh_tex_coords[vert_index]       += delta_tex_coords[vert_index] * weight;

			delta_normals[vert_index].mul(weight * NORMAL_SOFTEN_FACTOR);
			mesh_scaled_normals[vert_index].add(delta_normals[vert_index]);
			
			LLVector4a normalized_normal       = mesh_scaled_normals[vert_index];
			normalized_normal.normalize3();
			mesh_normals[vert_index]           = normalized_normal;

			delta_binormals[vert_index].mul(weight * NORMAL_SOFTEN_FACTOR);
			mesh_scaled_binormals[vert_index].add(delta_binormals[vert_index]);

			LLVector4a tangent;
			tangent.setCross3(mesh_scaled_binormals[vert_index], normalized_normal);
			LLVector4a normalized_binormal;
			normalized_binormal.setCross3(normalized_normal, tangent);
			normalized_binormal.normalize3();
			mesh_binormals[vert_index]         = normalized_binormal;
		}

		avatarp->dirtyMesh();
	}

	//-------------------------------------------------------------------------
	// reallocate vertices
	//-------------------------------------------------------------------------
	freeData();

	mVertexIndices = new_vertex_indices;
	mCoords        = new_coords;
	mNormals       = new_normals;
	mBinormals     = new_binormals;
	mTexCoords     = new_tex_coords;
	mNumIndices    = nindices;

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMorphTargetInfo()
//-----------------------------------------------------------------------------
LLPolyMorphTargetInfo::LLPolyMorphTargetInfo()
	: mIsClothingMorph(FALSE)
{
}

BOOL LLPolyMorphTargetInfo::parseXml(LLXmlTreeNode* node)
{
	llassert( node->hasName( "param" ) && node->getChildByName( "param_morph" ) );

	if (!LLViewerVisualParamInfo::parseXml(node))
		return FALSE;

	// Get mixed-case name
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if( !node->getFastAttributeString( name_string, mMorphName ) )
	{
		LL_WARNS() << "Avatar file: <param> is missing name attribute" << LL_ENDL;
		return FALSE;  // Continue, ignoring this tag
	}

	static LLStdStringHandle clothing_morph_string = LLXmlTree::addAttributeString("clothing_morph");
	node->getFastAttributeBOOL(clothing_morph_string, mIsClothingMorph);

	LLXmlTreeNode *paramNode = node->getChildByName("param_morph");

        if (NULL == paramNode)
        {
                LL_WARNS() << "Failed to getChildByName(\"param_morph\")"
                        << LL_ENDL;
                return FALSE;
        }

	for (LLXmlTreeNode* child_node = paramNode->getFirstChild();
		 child_node;
		 child_node = paramNode->getNextChild())
	{
		static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
		if (child_node->hasName("volume_morph"))
		{
			std::string volume_name;
			if (child_node->getFastAttributeString(name_string, volume_name))
			{
				LLVector3 scale;
				static LLStdStringHandle scale_string = LLXmlTree::addAttributeString("scale");
				child_node->getFastAttributeVector3(scale_string, scale);
				
				LLVector3 pos;
				static LLStdStringHandle pos_string = LLXmlTree::addAttributeString("pos");
				child_node->getFastAttributeVector3(pos_string, pos);

				mVolumeInfoList.push_back(LLPolyVolumeMorphInfo(volume_name,scale,pos));
			}
		}
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMorphTarget()
//-----------------------------------------------------------------------------
LLPolyMorphTarget::LLPolyMorphTarget(LLPolyMesh *poly_mesh)
	: LLViewerVisualParam(),
	mMorphData(NULL),
	mMesh(poly_mesh),
	mVertMask(NULL),
	mLastSex(SEX_FEMALE),
	mNumMorphMasksPending(0),
	mVolumeMorphs()
{
}

//-----------------------------------------------------------------------------
// LLPolyMorphTarget()
//-----------------------------------------------------------------------------
LLPolyMorphTarget::LLPolyMorphTarget(const LLPolyMorphTarget& pOther)
	: LLViewerVisualParam(pOther),
	mMorphData(pOther.mMorphData),
	mMesh(pOther.mMesh),
	mVertMask(pOther.mVertMask == NULL ? NULL : new LLPolyVertexMask(*pOther.mVertMask)),
	mLastSex(pOther.mLastSex),
	mNumMorphMasksPending(pOther.mNumMorphMasksPending),
	mVolumeMorphs(pOther.mVolumeMorphs)
{
}

//-----------------------------------------------------------------------------
// ~LLPolyMorphTarget()
//-----------------------------------------------------------------------------
LLPolyMorphTarget::~LLPolyMorphTarget()
{
	delete mVertMask;
	mVertMask = NULL;
}

//-----------------------------------------------------------------------------
// setInfo()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphTarget::setInfo(LLPolyMorphTargetInfo* info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
		return FALSE;
	mInfo = info;
	mID = info->mID;
	setWeight(getDefaultWeight());

	LLAvatarAppearance* avatarp = mMesh->getAvatar();
	LLPolyMorphTargetInfo::volume_info_list_t::iterator iter;
	for (iter = getInfo()->mVolumeInfoList.begin(); iter != getInfo()->mVolumeInfoList.end(); iter++)
	{
		LLPolyVolumeMorphInfo *volume_info = &(*iter);
		for (S32 i = 0; i < (S32)avatarp->mCollisionVolumes.size(); i++)
		{
			if (avatarp->mCollisionVolumes[i]->getName() == volume_info->mName)
			{
				mVolumeMorphs.push_back(LLPolyVolumeMorph(avatarp->mCollisionVolumes[i],
														  volume_info->mScale,
														  volume_info->mPos));
				break;
			}
		}
	}

	std::string morph_param_name = getInfo()->mMorphName;
	
	mMorphData = mMesh->getMorphData(morph_param_name);
	if (!mMorphData)
	{
		const std::string driven_tag = "_Driven";
		size_t pos = morph_param_name.find(driven_tag);
		if (pos != std::string::npos)
		{
			morph_param_name = morph_param_name.substr(0,pos);
			mMorphData = mMesh->getMorphData(morph_param_name);
		}
	}
	if (!mMorphData)
	{
		LL_WARNS() << "No morph target named " << morph_param_name << " found in mesh." << LL_ENDL;
		return FALSE;  // Continue, ignoring this tag
	}
	return TRUE;
}

/*virtual*/ LLViewerVisualParam* LLPolyMorphTarget::cloneParam(LLWearable* wearable) const
{
	return new LLPolyMorphTarget(*this);
}

#if 0 // obsolete
//-----------------------------------------------------------------------------
// parseData()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphTarget::parseData(LLXmlTreeNode* node)
{
	LLPolyMorphTargetInfo* info = new LLPolyMorphTargetInfo;

	info->parseXml(node);
	if (!setInfo(info))
	{
		delete info;
		return FALSE;
	}
	return TRUE;
}
#endif

//-----------------------------------------------------------------------------
// getVertexDistortion()
//-----------------------------------------------------------------------------
LLVector4a LLPolyMorphTarget::getVertexDistortion(S32 requested_index, LLPolyMesh *mesh)
{
	if (!mMorphData || mMesh != mesh) return LLVector4a::getZero();

	for(U32 index = 0; index < mMorphData->mNumIndices; index++)
	{
		if (mMorphData->mVertexIndices[index] == (U32)requested_index)
		{
			return mMorphData->mCoords[index];
		}
	}

	return LLVector4a::getZero();
}

//-----------------------------------------------------------------------------
// getFirstDistortion()
//-----------------------------------------------------------------------------
const LLVector4a *LLPolyMorphTarget::getFirstDistortion(U32 *index, LLPolyMesh **poly_mesh)
{
	if (!mMorphData) return &LLVector4a::getZero();

	LLVector4a* resultVec;
	mMorphData->mCurrentIndex = 0;
	if (mMorphData->mNumIndices)
	{
		resultVec = &mMorphData->mCoords[mMorphData->mCurrentIndex];
		if (index != NULL)
		{
			*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
		}
		if (poly_mesh != NULL)
		{
			*poly_mesh = mMesh;
		}

		return resultVec;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getNextDistortion()
//-----------------------------------------------------------------------------
const LLVector4a *LLPolyMorphTarget::getNextDistortion(U32 *index, LLPolyMesh **poly_mesh)
{
	if (!mMorphData) return &LLVector4a::getZero();

	LLVector4a* resultVec;
	mMorphData->mCurrentIndex++;
	if (mMorphData->mCurrentIndex < mMorphData->mNumIndices)
	{
		resultVec = &mMorphData->mCoords[mMorphData->mCurrentIndex];
		if (index != NULL)
		{
			*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
		}
		if (poly_mesh != NULL)
		{
			*poly_mesh = mMesh;
		}
		return resultVec;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getTotalDistortion()
//-----------------------------------------------------------------------------
F32	LLPolyMorphTarget::getTotalDistortion() 
{ 
	if (mMorphData) 
	{
		return mMorphData->mTotalDistortion; 
	}
	else 
	{
		return 0.f;
	}
}

//-----------------------------------------------------------------------------
// getAvgDistortion()
//-----------------------------------------------------------------------------
const LLVector4a& LLPolyMorphTarget::getAvgDistortion()	
{
	if (mMorphData) 
	{
		return mMorphData->mAvgDistortion; 
	}
	else 
	{
		return LLVector4a::getZero();
	}
}

//-----------------------------------------------------------------------------
// getMaxDistortion()
//-----------------------------------------------------------------------------
F32	LLPolyMorphTarget::getMaxDistortion() 
{
	if (mMorphData) 
	{
		return mMorphData->mMaxDistortion; 
	}
	else
	{
		return 0.f;
	}
}

//-----------------------------------------------------------------------------
// apply()
//-----------------------------------------------------------------------------
static LLTrace::BlockTimerStatHandle FTM_APPLY_MORPH_TARGET("Apply Morph");

void LLPolyMorphTarget::apply( ESex avatar_sex )
{
	if (!mMorphData || mNumMorphMasksPending > 0)
	{
		return;
	}

	LL_RECORD_BLOCK_TIME(FTM_APPLY_MORPH_TARGET);

	mLastSex = avatar_sex;

	// Check for NaN condition (NaN is detected if a variable doesn't equal itself.
	if (mCurWeight != mCurWeight)
	{
		mCurWeight = 0.0;
	}
	if (mLastWeight != mLastWeight)
	{
		mLastWeight = mCurWeight+.001;
	}

	// perform differential update of morph
	F32 delta_weight = ( getSex() & avatar_sex ) ? (mCurWeight - mLastWeight) : (getDefaultWeight() - mLastWeight);
	// store last weight
	mLastWeight += delta_weight;

	if (delta_weight != 0.f)
	{
		llassert(!mMesh->isLOD());
		LLVector4a *coords = mMesh->getWritableCoords();

		LLVector4a *scaled_normals = mMesh->getScaledNormals();
		LLVector4a *normals = mMesh->getWritableNormals();

		LLVector4a *scaled_binormals = mMesh->getScaledBinormals();
		LLVector4a *binormals = mMesh->getWritableBinormals();

		LLVector4a *clothing_weights = mMesh->getWritableClothingWeights();
		LLVector2 *tex_coords = mMesh->getWritableTexCoords();

		F32 *maskWeightArray = (mVertMask) ? mVertMask->getMorphMaskWeights() : NULL;

		for(U32 vert_index_morph = 0; vert_index_morph < mMorphData->mNumIndices; vert_index_morph++)
		{
			S32 vert_index_mesh = mMorphData->mVertexIndices[vert_index_morph];

			F32 maskWeight = 1.f;
			if (maskWeightArray)
			{
				maskWeight = maskWeightArray[vert_index_morph];
			}


			LLVector4a pos = mMorphData->mCoords[vert_index_morph];
			pos.mul(delta_weight*maskWeight);
			coords[vert_index_mesh].add(pos);

			if (getInfo()->mIsClothingMorph && clothing_weights)
			{
				LLVector4a clothing_offset = mMorphData->mCoords[vert_index_morph];
				clothing_offset.mul(delta_weight * maskWeight);
				LLVector4a* clothing_weight = &clothing_weights[vert_index_mesh];
				clothing_weight->add(clothing_offset);
				clothing_weight->getF32ptr()[VW] = maskWeight;
			}

			// calculate new normals based on half angles
			LLVector4a norm = mMorphData->mNormals[vert_index_morph];
			norm.mul(delta_weight*maskWeight*NORMAL_SOFTEN_FACTOR);
			scaled_normals[vert_index_mesh].add(norm);
			norm = scaled_normals[vert_index_mesh];

			// guard against degenerate input data before we create NaNs below!
			//
			norm.normalize3fast();
			normals[vert_index_mesh] = norm;

			// calculate new binormals
			LLVector4a binorm = mMorphData->mBinormals[vert_index_morph];

			// guard against degenerate input data before we create NaNs below!
			//
			if (!binorm.isFinite3() || (binorm.dot3(binorm).getF32() <= F_APPROXIMATELY_ZERO))
			{
				binorm.set(1,0,0,1);
			}

			binorm.mul(delta_weight*maskWeight*NORMAL_SOFTEN_FACTOR);
			scaled_binormals[vert_index_mesh].add(binorm);
			LLVector4a tangent;
			tangent.setCross3(scaled_binormals[vert_index_mesh], norm);
			LLVector4a& normalized_binormal = binormals[vert_index_mesh];

			normalized_binormal.setCross3(norm, tangent); 			
			normalized_binormal.normalize3fast();
			
			tex_coords[vert_index_mesh] += mMorphData->mTexCoords[vert_index_morph] * delta_weight * maskWeight;
		}

		// now apply volume changes
		for( volume_list_t::iterator iter = mVolumeMorphs.begin(); iter != mVolumeMorphs.end(); ++iter )
		{
			LLPolyVolumeMorph* volume_morph = &(*iter);
			LLVector3 scale_delta = volume_morph->mScale * delta_weight;
			LLVector3 pos_delta = volume_morph->mPos * delta_weight;
			
			volume_morph->mVolume->setScale(volume_morph->mVolume->getScale() + scale_delta);
			volume_morph->mVolume->setPosition(volume_morph->mVolume->getPosition() + pos_delta);
		}
	}

	if (mNext)
	{
		mNext->apply(avatar_sex);
	}
}

//-----------------------------------------------------------------------------
// applyMask()
//-----------------------------------------------------------------------------
void	LLPolyMorphTarget::applyMask(U8 *maskTextureData, S32 width, S32 height, S32 num_components, BOOL invert)
{
	LLVector4a *clothing_weights = getInfo()->mIsClothingMorph ? mMesh->getWritableClothingWeights() : NULL;

	if (!mVertMask)
	{
		mVertMask = new LLPolyVertexMask(mMorphData);
		mNumMorphMasksPending--;
	}
	else
	{
		// remove effect of previous mask
		F32 *maskWeights = (mVertMask) ? mVertMask->getMorphMaskWeights() : NULL;

		if (maskWeights)
		{
			LLVector4a *coords = mMesh->getWritableCoords();
			LLVector4a *scaled_normals = mMesh->getScaledNormals();
			LLVector4a *scaled_binormals = mMesh->getScaledBinormals();
			LLVector2 *tex_coords = mMesh->getWritableTexCoords();

			LLVector4Logical clothing_mask;
			clothing_mask.clear();
			clothing_mask.setElement<0>();
			clothing_mask.setElement<1>();
			clothing_mask.setElement<2>();


			for(U32 vert = 0; vert < mMorphData->mNumIndices; vert++)
			{
				F32 lastMaskWeight = mLastWeight * maskWeights[vert];
				S32 out_vert = mMorphData->mVertexIndices[vert];

				// remove effect of existing masked morph
				LLVector4a t;
				t = mMorphData->mCoords[vert];
				t.mul(lastMaskWeight);
				coords[out_vert].sub(t);

				t = mMorphData->mNormals[vert];
				t.mul(lastMaskWeight*NORMAL_SOFTEN_FACTOR);
				scaled_normals[out_vert].sub(t);

				t = mMorphData->mBinormals[vert];
				t.mul(lastMaskWeight*NORMAL_SOFTEN_FACTOR);
				scaled_binormals[out_vert].sub(t);

				tex_coords[out_vert] -= mMorphData->mTexCoords[vert] * lastMaskWeight;

				if (clothing_weights)
				{
					LLVector4a clothing_offset = mMorphData->mCoords[vert];
					clothing_offset.mul(lastMaskWeight);
					LLVector4a* clothing_weight = &clothing_weights[out_vert];
					LLVector4a t;
					t.setSub(*clothing_weight, clothing_offset);
					clothing_weight->setSelectWithMask(clothing_mask, t, *clothing_weight);
				}
			}
		}
	}

	// set last weight to 0, since we've removed the effect of this morph
	mLastWeight = 0.f;

	mVertMask->generateMask(maskTextureData, width, height, num_components, invert, clothing_weights);

	apply(mLastSex);
}

void LLPolyMorphTarget::applyVolumeChanges(F32 delta_weight)
{
    // now apply volume changes
    for( volume_list_t::iterator iter = mVolumeMorphs.begin(); iter != mVolumeMorphs.end(); ++iter )
    {
        LLPolyVolumeMorph* volume_morph = &(*iter);
        LLVector3 scale_delta = volume_morph->mScale * delta_weight;
        LLVector3 pos_delta = volume_morph->mPos * delta_weight;
		
        volume_morph->mVolume->setScale(volume_morph->mVolume->getScale() + scale_delta);
        // SL-315
        volume_morph->mVolume->setPosition(volume_morph->mVolume->getPosition() + pos_delta);
    }
}

//-----------------------------------------------------------------------------
// LLPolyVertexMask()
//-----------------------------------------------------------------------------
LLPolyVertexMask::LLPolyVertexMask(LLPolyMorphData* morph_data)
	: mWeights(new F32[morph_data->mNumIndices]),
	mMorphData(morph_data),
	mWeightsGenerated(FALSE)
{
	llassert(mMorphData != NULL);
	llassert(mMorphData->mNumIndices > 0);
}

//-----------------------------------------------------------------------------
// LLPolyVertexMask()
//-----------------------------------------------------------------------------
LLPolyVertexMask::LLPolyVertexMask(const LLPolyVertexMask& pOther)
	: mWeights(new F32[pOther.mMorphData->mNumIndices]),
	mMorphData(pOther.mMorphData),
	mWeightsGenerated(pOther.mWeightsGenerated)
{
	llassert(mMorphData != NULL);
	llassert(mMorphData->mNumIndices > 0);
	memcpy(mWeights, pOther.mWeights, sizeof(F32) * mMorphData->mNumIndices);
}

//-----------------------------------------------------------------------------
// ~LLPolyVertexMask()
//-----------------------------------------------------------------------------
LLPolyVertexMask::~LLPolyVertexMask()
{
	delete [] mWeights;
	mWeights = NULL;
}

//-----------------------------------------------------------------------------
// generateMask()
//-----------------------------------------------------------------------------
void LLPolyVertexMask::generateMask(U8 *maskTextureData, S32 width, S32 height, S32 num_components, BOOL invert, LLVector4a *clothing_weights)
{
// RN debug output that uses Image Debugger (http://www.cs.unc.edu/~baxter/projects/imdebug/)
//	BOOL debugImg = FALSE; 
//	if (debugImg)
//	{
//		if (invert)
//		{
//			imdebug("lum rbga=rgba b=8 w=%d h=%d *-1 %p", width, height, maskTextureData);
//		}
//		else
//		{
//			imdebug("lum rbga=rgba b=8 w=%d h=%d %p", width, height, maskTextureData);
//		}
//	}
	for (U32 index = 0; index < mMorphData->mNumIndices; index++)
	{
		S32 vertIndex = mMorphData->mVertexIndices[index];
		const S32 *sharedVertIndex = mMorphData->mMesh->getSharedVert(vertIndex);
		LLVector2 uvCoords;

		if (sharedVertIndex)
		{
			uvCoords = mMorphData->mMesh->getUVs(*sharedVertIndex);
		}
		else
		{
			uvCoords = mMorphData->mMesh->getUVs(vertIndex);
		}
		U32 s = llclamp((U32)(uvCoords.mV[VX] * (F32)(width - 1)), (U32)0, (U32)width - 1);
		U32 t = llclamp((U32)(uvCoords.mV[VY] * (F32)(height - 1)), (U32)0, (U32)height - 1);
		
		mWeights[index] = ((F32) maskTextureData[((t * width + s) * num_components) + (num_components - 1)]) / 255.f;
		
		if (invert) 
		{
			mWeights[index] = 1.f - mWeights[index];
		}

		// now apply step function
		// mWeights[index] = mWeights[index] > 0.95f ? 1.f : 0.f;

		if (clothing_weights)
		{
			clothing_weights[vertIndex].getF32ptr()[VW] = mWeights[index];
		}
	}
	mWeightsGenerated = TRUE;
}

//-----------------------------------------------------------------------------
// getMaskForMorphIndex()
//-----------------------------------------------------------------------------
F32* LLPolyVertexMask::getMorphMaskWeights()
{
	if (!mWeightsGenerated)
	{
		return NULL;
	}
	
	return mWeights;
}
