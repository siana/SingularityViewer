/** 
 * @file llpolymesh.cpp
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
#include "llpolymesh.h"
#include "llfasttimer.h"
#include "llmemory.h"

//#include "llviewercontrol.h"
#include "llxmltree.h"
#include "llavatarappearance.h"
//#include "llwearable.h"
#include "lldir.h"
#include "llvolume.h"
#include "llendianswizzle.h"


#define HEADER_ASCII "Linden Mesh 1.0"
#define HEADER_BINARY "Linden Binary Mesh 1.0"

//extern LLControlGroup gSavedSettings;                           // read only

LLPolyMorphData *clone_morph_param_duplicate(const LLPolyMorphData *src_data,
					     const std::string &name);
LLPolyMorphData *clone_morph_param_direction(const LLPolyMorphData *src_data,
					     const LLVector3 &direction,
					     const std::string &name);
LLPolyMorphData *clone_morph_param_cleavage(const LLPolyMorphData *src_data,
                                            F32 scale,
                                            const std::string &name);

//-----------------------------------------------------------------------------
// Global table of loaded LLPolyMeshes
//-----------------------------------------------------------------------------
LLPolyMesh::LLPolyMeshSharedDataTable LLPolyMesh::sGlobalSharedMeshList;

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData()
//-----------------------------------------------------------------------------
LLPolyMeshSharedData::LLPolyMeshSharedData()
{
	mNumVertices = 0;
	mBaseCoords = NULL;
	mBaseNormals = NULL;
	mBaseBinormals = NULL;
	mTexCoords = NULL;
	mDetailTexCoords = NULL;
	mWeights = NULL;
	mHasWeights = FALSE;
	mHasDetailTexCoords = FALSE;

	mNumFaces = 0;
	mFaces = NULL;

	mNumJointNames = 0;
	mJointNames = NULL;

	mTriangleIndices = NULL;
	mNumTriangleIndices = 0;

	mReferenceData = NULL;

	mLastIndexOffset = -1;
}

//-----------------------------------------------------------------------------
// ~LLPolyMeshSharedData()
//-----------------------------------------------------------------------------
LLPolyMeshSharedData::~LLPolyMeshSharedData()
{
	freeMeshData();
	for_each(mMorphData.begin(), mMorphData.end(), DeletePointer());
	mMorphData.clear();
}

//-----------------------------------------------------------------------------
// setupLOD()
//-----------------------------------------------------------------------------
void LLPolyMeshSharedData::setupLOD(LLPolyMeshSharedData* reference_data)
{
	mReferenceData = reference_data;

	if (reference_data)
	{
		mBaseCoords = reference_data->mBaseCoords;
		mBaseNormals = reference_data->mBaseNormals;
		mBaseBinormals = reference_data->mBaseBinormals;
		mTexCoords = reference_data->mTexCoords;
		mDetailTexCoords = reference_data->mDetailTexCoords;
		mWeights = reference_data->mWeights;
		mHasWeights = reference_data->mHasWeights;
		mHasDetailTexCoords = reference_data->mHasDetailTexCoords;
	}
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::freeMeshData()
//-----------------------------------------------------------------------------
void LLPolyMeshSharedData::freeMeshData()
{
        if (!mReferenceData)
        {
                mNumVertices = 0;

                ll_aligned_free_16(mBaseCoords);
                mBaseCoords = NULL;

                ll_aligned_free_16(mBaseNormals);
                mBaseNormals = NULL;

                ll_aligned_free_16(mBaseBinormals);
                mBaseBinormals = NULL;

                ll_aligned_free_16(mTexCoords);
                mTexCoords = NULL;

                ll_aligned_free_16(mDetailTexCoords);
                mDetailTexCoords = NULL;

                ll_aligned_free_16(mWeights);
                mWeights = NULL;
        }

	mNumFaces = 0;
	delete [] mFaces;
	mFaces = NULL;

	mNumJointNames = 0;
	delete [] mJointNames;
	mJointNames = NULL;

	delete [] mTriangleIndices;
	mTriangleIndices = NULL;

//	mVertFaceMap.deleteAllData();
}

// compare_int is used by the qsort function to sort the index array
S32 compare_int(const void *a, const void *b);

//-----------------------------------------------------------------------------
// genIndices()
//-----------------------------------------------------------------------------
void LLPolyMeshSharedData::genIndices(S32 index_offset)
{
	if (index_offset == mLastIndexOffset)
	{
		return;
	}

	delete []mTriangleIndices;
	mTriangleIndices = new U32[mNumTriangleIndices];

	S32 cur_index = 0;
	for (S32 i = 0; i < mNumFaces; i++)
	{
		mTriangleIndices[cur_index] = mFaces[i][0] + index_offset;
		cur_index++;
		mTriangleIndices[cur_index] = mFaces[i][1] + index_offset;
		cur_index++;
		mTriangleIndices[cur_index] = mFaces[i][2] + index_offset;
		cur_index++;
	}

	mLastIndexOffset = index_offset;
}

//--------------------------------------------------------------------
// LLPolyMeshSharedData::getNumKB()
//--------------------------------------------------------------------
U32 LLPolyMeshSharedData::getNumKB()
{
	U32 num_kb = sizeof(LLPolyMesh);

	if (!isLOD())
	{
		num_kb += mNumVertices *
					( sizeof(LLVector3) +	// coords
					sizeof(LLVector3) +		// normals
					sizeof(LLVector2) );	// texCoords
	}

	if (mHasDetailTexCoords && !isLOD())
	{
		num_kb += mNumVertices * sizeof(LLVector2);	// detailTexCoords
	}

	if (mHasWeights && !isLOD())
	{
		num_kb += mNumVertices * sizeof(float);		// weights
	}

	num_kb += mNumFaces * sizeof(LLPolyFace);	// faces

	num_kb /= 1024;
	return num_kb;
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::allocateVertexData()
//-----------------------------------------------------------------------------
BOOL LLPolyMeshSharedData::allocateVertexData( U32 numVertices )
{
        U32 i;
        mBaseCoords = (LLVector4a*) ll_aligned_malloc_16(numVertices*sizeof(LLVector4a));
        mBaseNormals = (LLVector4a*) ll_aligned_malloc_16(numVertices*sizeof(LLVector4a));
        mBaseBinormals = (LLVector4a*) ll_aligned_malloc_16(numVertices*sizeof(LLVector4a));
        mTexCoords = (LLVector2*) ll_aligned_malloc_16((numVertices+numVertices%2)*sizeof(LLVector2));
        mDetailTexCoords = (LLVector2*) ll_aligned_malloc_16((numVertices+numVertices%2)*sizeof(LLVector2));
        mWeights = (F32*) ll_aligned_malloc_16(((numVertices)*sizeof(F32)+0xF) & ~0xF);
        for (i = 0; i < numVertices; i++)
	{
		mBaseCoords[i].clear();
		mBaseNormals[i].clear();
		mBaseBinormals[i].clear();
		mTexCoords[i].clear();
		mWeights[i] = 0.f;
    }
	mNumVertices = numVertices;
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::allocateFaceData()
//-----------------------------------------------------------------------------
BOOL LLPolyMeshSharedData::allocateFaceData( U32 numFaces )
{
	mFaces = new LLPolyFace[ numFaces ];
	mNumFaces = numFaces;
	mNumTriangleIndices = mNumFaces * 3;
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::allocateJointNames()
//-----------------------------------------------------------------------------
BOOL LLPolyMeshSharedData::allocateJointNames( U32 numJointNames )
{
	mJointNames = new std::string[ numJointNames ];
	mNumJointNames = numJointNames;
	return TRUE;
}

//--------------------------------------------------------------------
// LLPolyMeshSharedData::loadMesh()
//--------------------------------------------------------------------
BOOL LLPolyMeshSharedData::loadMesh( const std::string& fileName )
{
	//-------------------------------------------------------------------------
	// Open the file
	//-------------------------------------------------------------------------
	if(fileName.empty())
	{
                LL_ERRS() << "Filename is Empty!" << LL_ENDL;
		return FALSE;
	}
	LLFILE* fp = LLFile::fopen(fileName, "rb");			/*Flawfinder: ignore*/
	if (!fp)
	{
                LL_ERRS() << "can't open: " << fileName << LL_ENDL;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// Read a chunk
	//-------------------------------------------------------------------------
	char header[128];		/*Flawfinder: ignore*/
	if (fread(header, sizeof(char), 128, fp) != 128)
	{
                LL_WARNS() << "Short read" << LL_ENDL;
	}

	//-------------------------------------------------------------------------
	// Check for proper binary header
	//-------------------------------------------------------------------------
	BOOL status = FALSE;
	if ( strncmp(header, HEADER_BINARY, strlen(HEADER_BINARY)) == 0 )	/*Flawfinder: ignore*/
	{
                LL_DEBUGS() << "Loading " << fileName << LL_ENDL;

		//----------------------------------------------------------------
		// File Header (seek past it)
		//----------------------------------------------------------------
                if (fseek(fp, 24, SEEK_SET) != 0)
                {
                        LL_ERRS() << "can't seek past header from " << fileName << LL_ENDL;
                        return FALSE;
                }

		//----------------------------------------------------------------
		// HasWeights
		//----------------------------------------------------------------
		U8 hasWeights;
		size_t numRead = fread(&hasWeights, sizeof(U8), 1, fp);
		if (numRead != 1)
		{
                        LL_ERRS() << "can't read HasWeights flag from " << fileName << LL_ENDL;
			return FALSE;
		}
		if (!isLOD())
		{
			mHasWeights = (hasWeights==0) ? FALSE : TRUE;
		}

		//----------------------------------------------------------------
		// HasDetailTexCoords
		//----------------------------------------------------------------
		U8 hasDetailTexCoords;
		numRead = fread(&hasDetailTexCoords, sizeof(U8), 1, fp);
		if (numRead != 1)
		{
                        LL_ERRS() << "can't read HasDetailTexCoords flag from " << fileName << LL_ENDL;
			return FALSE;
		}

		//----------------------------------------------------------------
		// Position
		//----------------------------------------------------------------
		LLVector3 position;
		numRead = fread(position.mV, sizeof(float), 3, fp);
		llendianswizzle(position.mV, sizeof(float), 3);
		if (numRead != 3)
		{
                        LL_ERRS() << "can't read Position from " << fileName << LL_ENDL;
			return FALSE;
		}
		setPosition( position );

		//----------------------------------------------------------------
		// Rotation
		//----------------------------------------------------------------
		LLVector3 rotationAngles;
		numRead = fread(rotationAngles.mV, sizeof(float), 3, fp);
		llendianswizzle(rotationAngles.mV, sizeof(float), 3);
		if (numRead != 3)
		{
                        LL_ERRS() << "can't read RotationAngles from " << fileName << LL_ENDL;
			return FALSE;
		}

		U8 rotationOrder;
		numRead = fread(&rotationOrder, sizeof(U8), 1, fp);

		if (numRead != 1)
		{
                        LL_ERRS() << "can't read RotationOrder from " << fileName << LL_ENDL;
			return FALSE;
		}

		rotationOrder = 0;

		setRotation( mayaQ(	rotationAngles.mV[0],
							rotationAngles.mV[1],
							rotationAngles.mV[2],
							(LLQuaternion::Order)rotationOrder ) );

		//----------------------------------------------------------------
		// Scale
		//----------------------------------------------------------------
		LLVector3 scale;
		numRead = fread(scale.mV, sizeof(float), 3, fp);
		llendianswizzle(scale.mV, sizeof(float), 3);
		if (numRead != 3)
		{
                        LL_ERRS() << "can't read Scale from " << fileName << LL_ENDL;
			return FALSE;
		}
		setScale( scale );

		//-------------------------------------------------------------------------
		// Release any existing mesh geometry
		//-------------------------------------------------------------------------
		freeMeshData();

		U16 numVertices = 0;

		//----------------------------------------------------------------
		// NumVertices
		//----------------------------------------------------------------
		if (!isLOD())
		{
			numRead = fread(&numVertices, sizeof(U16), 1, fp);
			llendianswizzle(&numVertices, sizeof(U16), 1);
			if (numRead != 1)
			{
                                LL_ERRS() << "can't read NumVertices from " << fileName << LL_ENDL;
				return FALSE;
			}

			allocateVertexData( numVertices );	

			for (U16 i = 0; i < numVertices; ++i)
			{
				//----------------------------------------------------------------
				// Coords
				//----------------------------------------------------------------
				numRead = fread(&mBaseCoords[i], sizeof(float), 3, fp);
				llendianswizzle(&mBaseCoords[i], sizeof(float), 3);
				if (numRead != 3)
					{
									LL_ERRS() << "can't read Coordinates from " << fileName << LL_ENDL;
						return FALSE;
					}
				}

				for (U16 i = 0; i < numVertices; ++i)
				{
					//----------------------------------------------------------------
					// Normals
					//----------------------------------------------------------------
					numRead = fread(&mBaseNormals[i], sizeof(float), 3, fp);
					llendianswizzle(&mBaseNormals[i], sizeof(float), 3);
					if (numRead != 3)
					{
									LL_ERRS() << " can't read Normals from " << fileName << LL_ENDL;
						return FALSE;
					}
				}

				for (U16 i = 0; i < numVertices; ++i)
				{
				//----------------------------------------------------------------
				// Binormals
				//----------------------------------------------------------------
				numRead = fread(&mBaseBinormals[i], sizeof(float), 3, fp);
				llendianswizzle(&mBaseBinormals[i], sizeof(float), 3);
				if (numRead != 3)
				{
									LL_ERRS() << " can't read Binormals from " << fileName << LL_ENDL;
					return FALSE;
				}
			}

			//----------------------------------------------------------------
			// TexCoords
			//----------------------------------------------------------------
			numRead = fread(mTexCoords, 2*sizeof(float), numVertices, fp);
			llendianswizzle(mTexCoords, sizeof(float), 2*numVertices);
			if (numRead != numVertices)
			{
                                LL_ERRS() << "can't read TexCoords from " << fileName << LL_ENDL;
				return FALSE;
			}

			//----------------------------------------------------------------
			// DetailTexCoords
			//----------------------------------------------------------------
			if (mHasDetailTexCoords)
			{
				numRead = fread(mDetailTexCoords, 2*sizeof(float), numVertices, fp);
				llendianswizzle(mDetailTexCoords, sizeof(float), 2*numVertices);
				if (numRead != numVertices)
				{
                                        LL_ERRS() << "can't read DetailTexCoords from " << fileName << LL_ENDL;
					return FALSE;
				}
			}

			//----------------------------------------------------------------
			// Weights
			//----------------------------------------------------------------
			if (mHasWeights)
			{
				numRead = fread(mWeights, sizeof(float), numVertices, fp);
				llendianswizzle(mWeights, sizeof(float), numVertices);
				if (numRead != numVertices)
				{
                                        LL_ERRS() << "can't read Weights from " << fileName << LL_ENDL;
					return FALSE;
				}
			}
		}

		//----------------------------------------------------------------
		// NumFaces
		//----------------------------------------------------------------
		U16 numFaces;
		numRead = fread(&numFaces, sizeof(U16), 1, fp);
		llendianswizzle(&numFaces, sizeof(U16), 1);
		if (numRead != 1)
		{
                        LL_ERRS() << "can't read NumFaces from " << fileName << LL_ENDL;
			return FALSE;
		}
		allocateFaceData( numFaces );


		//----------------------------------------------------------------
		// Faces
		//----------------------------------------------------------------
		U32 i;
		U32 numTris = 0;
		for (i = 0; i < numFaces; i++)
		{
			S16 face[3];
			numRead = fread(face, sizeof(U16), 3, fp);
			llendianswizzle(face, sizeof(U16), 3);
			if (numRead != 3)
			{
                                LL_ERRS() << "can't read Face[" << i << "] from " << fileName << LL_ENDL;
				return FALSE;
			}
			if (mReferenceData)
			{
				llassert(face[0] < mReferenceData->mNumVertices);
				llassert(face[1] < mReferenceData->mNumVertices);
				llassert(face[2] < mReferenceData->mNumVertices);
			}
			
			if (isLOD())
			{
				// store largest index in case of LODs
				for (S32 j = 0; j < 3; j++)
				{
					if (face[j] > mNumVertices - 1)
					{
						mNumVertices = face[j] + 1;
					}
				}
			}
			mFaces[i][0] = face[0];
			mFaces[i][1] = face[1];
			mFaces[i][2] = face[2];

//			S32 j;
//			for(j = 0; j < 3; j++)
//			{
//				std::vector<S32> *face_list = mVertFaceMap.getIfThere(face[j]);
//				if (!face_list)
//				{
//					face_list = new std::vector<S32>;
//					mVertFaceMap.addData(face[j], face_list);
//				}
//				face_list->push_back(i);
//			}

			numTris++;
		}

                LL_DEBUGS() << "verts: " << numVertices 
			<< ", faces: "   << numFaces
			<< ", tris: "    << numTris
                         << LL_ENDL;

		//----------------------------------------------------------------
		// NumSkinJoints
		//----------------------------------------------------------------
		if (!isLOD())
		{
			U16 numSkinJoints = 0;
			if ( mHasWeights )
			{
				numRead = fread(&numSkinJoints, sizeof(U16), 1, fp);
				llendianswizzle(&numSkinJoints, sizeof(U16), 1);
				if (numRead != 1)
				{
                                        LL_ERRS() << "can't read NumSkinJoints from " << fileName << LL_ENDL;
					return FALSE;
				}
				allocateJointNames( numSkinJoints );
			}

			//----------------------------------------------------------------
			// SkinJoints
			//----------------------------------------------------------------
			for (i=0; i < numSkinJoints; i++)
			{
				char jointName[64+1];
				numRead = fread(jointName, sizeof(jointName)-1, 1, fp);
				jointName[sizeof(jointName)-1] = '\0'; // ensure nul-termination
				if (numRead != 1)
				{
                                        LL_ERRS() << "can't read Skin[" << i << "].Name from " << fileName << LL_ENDL;
					return FALSE;
				}

				std::string *jn = &mJointNames[i];
				*jn = jointName;
			}

			//-------------------------------------------------------------------------
			// look for morph section
			//-------------------------------------------------------------------------
			char morphName[64+1];
			morphName[sizeof(morphName)-1] = '\0'; // ensure nul-termination
			while(fread(morphName, sizeof(char), 64, fp) == 64)
			{
				if (!strcmp(morphName, "End Morphs"))
				{
					// we reached the end of the morphs
					break;
				}
				LLPolyMorphData* morph_data = new LLPolyMorphData(std::string(morphName));

				BOOL result = morph_data->loadBinary(fp, this);

				if (!result)
				{
					delete morph_data;
					continue;
				}

                                mMorphData.insert(morph_data);

                                if (!strcmp(morphName, "Breast_Female_Cleavage"))
                                {
                                        mMorphData.insert(clone_morph_param_cleavage(morph_data,
                                                                                     .75f,
                                                                                     "Breast_Physics_LeftRight_Driven"));
                                        mMorphData.insert(clone_morph_param_duplicate(morph_data,
										      "Breast_Physics_InOut_Driven"));
                                }
                                if (!strcmp(morphName, "Breast_Gravity"))
                                {
                                        mMorphData.insert(clone_morph_param_duplicate(morph_data,
										      "Breast_Physics_UpDown_Driven"));
                                }

                                if (!strcmp(morphName, "Big_Belly_Torso"))
                                {
                                        mMorphData.insert(clone_morph_param_direction(morph_data,
										      LLVector3(0,0,0.05f),
										      "Belly_Physics_Torso_UpDown_Driven"));
                                }

                                if (!strcmp(morphName, "Big_Belly_Legs"))
                                {
                                        mMorphData.insert(clone_morph_param_direction(morph_data,
										      LLVector3(0,0,0.05f),
										      "Belly_Physics_Legs_UpDown_Driven"));
                                }

                                if (!strcmp(morphName, "skirt_belly"))
                                {
                                        mMorphData.insert(clone_morph_param_direction(morph_data,
										      LLVector3(0,0,0.05f),
										      "Belly_Physics_Skirt_UpDown_Driven"));
                                }

                                if (!strcmp(morphName, "Small_Butt"))
                                {
                                        mMorphData.insert(clone_morph_param_direction(morph_data,
										      LLVector3(0,0,0.05f),
										      "Butt_Physics_UpDown_Driven"));
                                        mMorphData.insert(clone_morph_param_direction(morph_data,
										      LLVector3(0,0.03f,0),
										      "Butt_Physics_LeftRight_Driven"));
                                }
                        }

			S32 numRemaps;
			if (fread(&numRemaps, sizeof(S32), 1, fp) == 1)
			{
				llendianswizzle(&numRemaps, sizeof(S32), 1);
				for (S32 i = 0; i < numRemaps; i++)
				{
					S32 remapSrc;
					S32 remapDst;
					if (fread(&remapSrc, sizeof(S32), 1, fp) != 1)
					{
                                                LL_ERRS() << "can't read source vertex in vertex remap data" << LL_ENDL;
						break;
					}
					if (fread(&remapDst, sizeof(S32), 1, fp) != 1)
					{
                                                LL_ERRS() << "can't read destination vertex in vertex remap data" << LL_ENDL;
						break;
					}
					llendianswizzle(&remapSrc, sizeof(S32), 1);
					llendianswizzle(&remapDst, sizeof(S32), 1);

					mSharedVerts[remapSrc] = remapDst;
				}
			}
		}

		status = TRUE;
	}
	else
	{
                LL_ERRS() << "invalid mesh file header: " << fileName << LL_ENDL;
		status = FALSE;
	}

	if (0 == mNumJointNames)
	{
		allocateJointNames(1);
	}

	fclose( fp );

	return status;
}

//-----------------------------------------------------------------------------
// getSharedVert()
//-----------------------------------------------------------------------------
const S32 *LLPolyMeshSharedData::getSharedVert(S32 vert)
{
	if (mSharedVerts.count(vert) > 0)
	{
		return &mSharedVerts[vert];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getUV()
//-----------------------------------------------------------------------------
const LLVector2 &LLPolyMeshSharedData::getUVs(U32 index)
{
	// TODO: convert all index variables to S32
	llassert((S32)index < mNumVertices);

	return mTexCoords[index];
}

//-----------------------------------------------------------------------------
// LLPolyMesh()
//-----------------------------------------------------------------------------
LLPolyMesh::LLPolyMesh(LLPolyMeshSharedData *shared_data, LLPolyMesh *reference_mesh)
{	
	llassert(shared_data);

	mSharedData = shared_data;
	mReferenceMesh = reference_mesh;
	mAvatarp = NULL;
	mVertexData = NULL;

	mCurVertexCount = 0;
	mFaceIndexCount = 0;
	mFaceIndexOffset = 0;
	mFaceVertexCount = 0;
	mFaceVertexOffset = 0;

	if (shared_data->isLOD() && reference_mesh)
	{
		mCoords = reference_mesh->mCoords;
		mNormals = reference_mesh->mNormals;
		mScaledNormals = reference_mesh->mScaledNormals;
		mBinormals = reference_mesh->mBinormals;
		mScaledBinormals = reference_mesh->mScaledBinormals;
		mTexCoords = reference_mesh->mTexCoords;
		mClothingWeights = reference_mesh->mClothingWeights;
	}
	else
	{
		// Allocate memory without initializing every vector
		// NOTE: This makes asusmptions about the size of LLVector[234]
		S32 nverts = mSharedData->mNumVertices;
		//make sure it's an even number of verts for alignment
		nverts += nverts%2;
		S32 nfloats = nverts * (
					4 + //coords
					4 + //normals
					4 + //weights
					2 + //coords
					4 + //scaled normals
					4 + //binormals
					4); //scaled binormals

		//use 16 byte aligned vertex data to make LLPolyMesh SSE friendly
		mVertexData = (F32*) ll_aligned_malloc_16(nfloats*4);
		S32 offset = 0;
		mCoords				= 	(LLVector4a*)(mVertexData + offset); offset += 4*nverts;
		mNormals			=	(LLVector4a*)(mVertexData + offset); offset += 4*nverts;
		mClothingWeights	= 	(LLVector4a*)(mVertexData + offset); offset += 4*nverts;
		mTexCoords			= 	(LLVector2*)(mVertexData + offset);  offset += 2*nverts;
		mScaledNormals		=   (LLVector4a*)(mVertexData + offset); offset += 4*nverts;
		mBinormals			=   (LLVector4a*)(mVertexData + offset); offset += 4*nverts;
		mScaledBinormals	=   (LLVector4a*)(mVertexData + offset); offset += 4*nverts; 
		initializeForMorph();
	}
}


//-----------------------------------------------------------------------------
// ~LLPolyMesh()
//-----------------------------------------------------------------------------
LLPolyMesh::~LLPolyMesh()
{
	delete_and_clear(mJointRenderData);
	ll_aligned_free_16(mVertexData);
}


//-----------------------------------------------------------------------------
// LLPolyMesh::getMesh()
//-----------------------------------------------------------------------------
LLPolyMesh *LLPolyMesh::getMesh(const std::string &name, LLPolyMesh* reference_mesh)
{
	//-------------------------------------------------------------------------
	// search for an existing mesh by this name
	//-------------------------------------------------------------------------
	LLPolyMeshSharedData* meshSharedData = get_if_there(sGlobalSharedMeshList, name, (LLPolyMeshSharedData*)NULL);
	if (meshSharedData)
	{
//		LL_INFOS() << "Polymesh " << name << " found in global mesh table." << LL_ENDL;
		LLPolyMesh *poly_mesh = new LLPolyMesh(meshSharedData, reference_mesh);
		return poly_mesh;
	}

	//-------------------------------------------------------------------------
	// if not found, create a new one, add it to the list
	//-------------------------------------------------------------------------
	std::string full_path;
	full_path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,name);

	LLPolyMeshSharedData *mesh_data = new LLPolyMeshSharedData();
	if (reference_mesh)
	{
		mesh_data->setupLOD(reference_mesh->getSharedData());
	}
	if ( ! mesh_data->loadMesh( full_path ) )
	{
		delete mesh_data;
		return NULL;
	}

	LLPolyMesh *poly_mesh = new LLPolyMesh(mesh_data, reference_mesh);

//	LL_INFOS() << "Polymesh " << name << " added to global mesh table." << LL_ENDL;
	sGlobalSharedMeshList[name] = poly_mesh->mSharedData;

	return poly_mesh;
}

//-----------------------------------------------------------------------------
// LLPolyMesh::getMeshData()
//-----------------------------------------------------------------------------
LLPolyMeshSharedData *LLPolyMesh::getMeshData(const std::string &name)
{
	//-------------------------------------------------------------------------
	// search for an existing mesh by this name
	//-------------------------------------------------------------------------
	LLPolyMeshSharedData* mesh_shared_data = get_if_there(sGlobalSharedMeshList, name, (LLPolyMeshSharedData*)NULL);

	return mesh_shared_data;
}

//-----------------------------------------------------------------------------
// LLPolyMesh::saveLLM()
//-----------------------------------------------------------------------------
BOOL LLPolyMesh::saveLLM(LLFILE *fp)
{
	if (!fp)
		return FALSE;

	//-------------------------------------------------------------------------
	// Write a header
	//-------------------------------------------------------------------------
	if (fwrite(HEADER_BINARY, 1, strlen(HEADER_BINARY), fp) != strlen(HEADER_BINARY))
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	if (strlen(HEADER_BINARY) < 24)
	{
		char padding[24] = {};	// zeroes
		int pad = 24 - strlen(HEADER_BINARY);
		if (fwrite(&padding, 1, pad, fp) != pad)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}

	//----------------------------------------------------------------
	// HasWeights
	//----------------------------------------------------------------
	U8 hasWeights = (U8) mSharedData->mHasWeights;
	if (fwrite(&hasWeights, sizeof(U8), 1, fp) != 1)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	//----------------------------------------------------------------
	// HasDetailTexCoords
	//----------------------------------------------------------------
	U8 hasDetailTexCoords = (U8) mSharedData->mHasDetailTexCoords;
	if (fwrite(&hasDetailTexCoords, sizeof(U8), 1, fp) != 1)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	//----------------------------------------------------------------
	// Position
	//----------------------------------------------------------------
	LLVector3 position = mSharedData->mPosition;
	llendianswizzle(position.mV, sizeof(float), 3);
	if (fwrite(position.mV, sizeof(float), 3, fp) != 3)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	//----------------------------------------------------------------
	// Rotation
	//----------------------------------------------------------------
	LLQuaternion rotation = mSharedData->mRotation;
	F32 roll;
	F32 pitch;
	F32 yaw;

	rotation.getEulerAngles(&roll, &pitch, &yaw);

	roll  *= RAD_TO_DEG;
	pitch *= RAD_TO_DEG;
	yaw   *= RAD_TO_DEG;

	LLVector3 rotationAngles (roll, pitch, yaw);
	llendianswizzle(rotationAngles.mV, sizeof(float), 3);
	if (fwrite(rotationAngles.mV, sizeof(float), 3, fp) != 3)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	U8 rotationOrder = 0;
	if (fwrite(&rotationOrder, sizeof(U8), 1, fp) != 1)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	//----------------------------------------------------------------
	// Scale
	//----------------------------------------------------------------
	LLVector3 scale = mSharedData->mScale;
	llendianswizzle(scale.mV, sizeof(float), 3);
	if (fwrite(scale.mV, sizeof(float), 3, fp) != 3)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	//----------------------------------------------------------------
	// NumVertices
	//----------------------------------------------------------------
	U16 numVertices = mSharedData->mNumVertices;

	if (!isLOD())
	{
		llendianswizzle(&numVertices, sizeof(U16), 1);
		if (fwrite(&numVertices, sizeof(U16), 1, fp) != 1)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		numVertices = mSharedData->mNumVertices;  // without the swizzle again

		//----------------------------------------------------------------
		// Coords
		//----------------------------------------------------------------
		for (U16 i = 0; i < numVertices; ++i)
		{
			LLVector4a& coords = mSharedData->mBaseCoords[i];

			llendianswizzle(coords.getF32ptr(), sizeof(float), 3);
			if (fwrite(coords.getF32ptr(), 3*sizeof(float), 1, fp) != 1)
			{
				LL_WARNS() << "Short write" << LL_ENDL;
			}
			llendianswizzle(coords.getF32ptr(), sizeof(float), 3);
		}

		//----------------------------------------------------------------
		// Normals
		//----------------------------------------------------------------
		for (U16 i = 0; i < numVertices; ++i)
		{
			LLVector4a& normals = mSharedData->mBaseNormals[i];

			llendianswizzle(normals.getF32ptr(), sizeof(float), 3);
			if (fwrite(normals.getF32ptr(), 3*sizeof(float), 1, fp) != 1)
			{
				LL_WARNS() << "Short write" << LL_ENDL;
			}
			llendianswizzle(normals.getF32ptr(), sizeof(float), 3);
		}

		//----------------------------------------------------------------
		// Binormals
		//----------------------------------------------------------------
		for (U16 i = 0; i < numVertices; ++i)
		{
			LLVector4a& binormals = mSharedData->mBaseBinormals[i];

			llendianswizzle(binormals.getF32ptr(), sizeof(float), 3);
			if (fwrite(binormals.getF32ptr(), 3*sizeof(float), 1, fp) != 1)
			{
				LL_WARNS() << "Short write" << LL_ENDL;
			}
			llendianswizzle(binormals.getF32ptr(), sizeof(float), 3);
		}

		//----------------------------------------------------------------
		// TexCoords
		//----------------------------------------------------------------
		LLVector2* tex = mSharedData->mTexCoords;

		llendianswizzle(tex, sizeof(float), 2*numVertices);
		if (fwrite(tex, 2*sizeof(float), numVertices, fp) != numVertices)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		llendianswizzle(tex, sizeof(float), 2*numVertices);

		//----------------------------------------------------------------
		// DetailTexCoords
		//----------------------------------------------------------------
		if (hasDetailTexCoords)
		{
			LLVector2* detail = mSharedData->mDetailTexCoords;

			llendianswizzle(detail, sizeof(float), 2*numVertices);
			if (fwrite(detail, 2*sizeof(float), numVertices, fp) != numVertices)
			{
				LL_WARNS() << "Short write" << LL_ENDL;
			}
			llendianswizzle(detail, sizeof(float), 2*numVertices);
		}

		//----------------------------------------------------------------
		// Weights
		//----------------------------------------------------------------
		if (hasWeights)
		{
			F32* weights = mSharedData->mWeights;

			llendianswizzle(weights, sizeof(float), numVertices);
			if (fwrite(weights, sizeof(float), numVertices, fp) != numVertices)
			{
				LL_WARNS() << "Short write" << LL_ENDL;
			}
			llendianswizzle(weights, sizeof(float), numVertices);
		}
	}

	//----------------------------------------------------------------
	// NumFaces
	//----------------------------------------------------------------
	U16 numFaces = mSharedData->mNumFaces;

	llendianswizzle(&numFaces, sizeof(U16), 1);
	if (fwrite(&numFaces, sizeof(U16), 1, fp) != 1)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}
	numFaces = mSharedData->mNumFaces;	// without the swizzle again

	//----------------------------------------------------------------
	// Faces
	//----------------------------------------------------------------
	LLPolyFace* faces = mSharedData->mFaces;
	S16 face[3];
	U32 i;
	for (i = 0; i < numFaces; i++)
	{
		face[0] = faces[i][0];
		face[1] = faces[i][1];
		face[2] = faces[i][2];

		llendianswizzle(face, sizeof(U16), 3);
		if (fwrite(face, sizeof(U16), 3, fp) != 3)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}

	//----------------------------------------------------------------
	// NumSkinJoints
	//----------------------------------------------------------------

	// When reading LOD mesh files, we stop here, but the actual Linden
	// .llm files have data with zero items for these, so we do the same.

	U16 numSkinJoints = mSharedData->mNumJointNames;

	// At least one element is always allocated but it may be empty
	std::string *jn = &mSharedData->mJointNames[0];

	if ((numSkinJoints == 1)
	&&  (jn->length() == 0))
	{
		numSkinJoints = 0;
	}

	if ( hasWeights )
	{
		llendianswizzle(&numSkinJoints, sizeof(U16), 1);
		if (fwrite(&numSkinJoints, sizeof(U16), 1, fp) != 1)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
		llendianswizzle(&numSkinJoints, sizeof(U16), 1);

		//----------------------------------------------------------------
		// SkinJoints
		//----------------------------------------------------------------
		char padding[64] = {};	// zeroes
		for (i=0; i < numSkinJoints; i++)
		{
			jn = &mSharedData->mJointNames[i];
			if (fwrite(jn->c_str(), 1, jn->length(), fp) != jn->length())
			{
				LL_WARNS() << "Short write" << LL_ENDL;
			}

			if (jn->length() < 64)
			{
				int pad = 64 - jn->length();
				if (fwrite(&padding, 1, pad, fp) != pad)
				{
					LL_WARNS() << "Short write" << LL_ENDL;
				}
			}
		}
	}

	//-------------------------------------------------------------------------
	// look for morph section
	//-------------------------------------------------------------------------
	LLPolyMeshSharedData::morphdata_list_t::iterator iter = mSharedData->mMorphData.begin();
	LLPolyMeshSharedData::morphdata_list_t::iterator end  = mSharedData->mMorphData.end();

	// Sort them
	morph_list_t morph_list;
	for (; iter != end; ++iter)
	{
		LLPolyMorphData *morph_data = *iter;
		std::string morph_name = morph_data->getName();

		morph_list.insert(std::pair<std::string,LLPolyMorphData*>(morph_name, morph_data));
	}

	char padding[64] = {};	// zeroes
	for (morph_list_t::iterator morph_iter = morph_list.begin();
		morph_iter != morph_list.end(); ++morph_iter)
	{
		const std::string& morph_name = morph_iter->first;
		LLPolyMorphData* morph_data = morph_iter->second;

		if (fwrite(morph_name.c_str(), 1, morph_name.length(), fp) != morph_name.length())
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}

		if (morph_name.length() < 64)
		{
			int pad = 64 - morph_name.length();
			if (fwrite(&padding, 1, pad, fp) != pad)
			{
				LL_WARNS() << "Short write" << LL_ENDL;
			}
		}

		if (!morph_data->saveLLM(fp))
		{
			LL_WARNS() << "Problem writing morph" << LL_ENDL;
		}
	}

	char end_morphs[64] = "End Morphs";  // padded with zeroes
	if (fwrite(end_morphs, sizeof(char), 64, fp) != 64)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	//-------------------------------------------------------------------------
	// Remaps
	//-------------------------------------------------------------------------
	S32 numRemaps = mSharedData->mSharedVerts.size();
	llendianswizzle(&numRemaps, sizeof(S32), 1);
	if (fwrite(&numRemaps, sizeof(S32), 1, fp) != 1)
	{
		LL_WARNS() << "Short write" << LL_ENDL;
	}

	std::map<S32, S32>::iterator remap_iter = mSharedData->mSharedVerts.begin();
	std::map<S32, S32>::iterator remap_end  = mSharedData->mSharedVerts.end();

	for (; remap_iter != remap_end; ++remap_iter)
	{
		S32 remapSrc = remap_iter->first;

		llendianswizzle(&remapSrc, sizeof(S32), 1);
		if (fwrite(&remapSrc, sizeof(S32), 1, fp) != 1)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}

		S32 remapDst = remap_iter->second;

		llendianswizzle(&remapDst, sizeof(S32), 1);
		if (fwrite(&remapDst, sizeof(S32), 1, fp) != 1)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMesh::saveOBJ()
//-----------------------------------------------------------------------------
BOOL LLPolyMesh::saveOBJ(LLFILE *fp)
{
	if (!fp)
		return FALSE;

	// If it's an LOD mesh, the LOD vertices are usually at the start of the
	// list of vertices, so the number of vertices is just that subset.
	// We could also write out the rest of the vertices in case someone wants
	// to choose new vertices for the LOD mesh, but that may confuse some people.

	int nverts = mSharedData->mNumVertices;
	int nfaces = mSharedData->mNumFaces;
	int i;

	LLVector4a* coords = getWritableCoords();
	for ( i=0; i<nverts; i++) {
		std::string outstring = llformat("v %f %f %f\n",
										 coords[i].getF32ptr()[0],
										 coords[i].getF32ptr()[1],
										 coords[i].getF32ptr()[2]);
		if (fwrite(outstring.c_str(), 1, outstring.length(), fp) != outstring.length())
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}

	LLVector4a* normals = getWritableNormals();
	for ( i=0; i<nverts; i++) {
		std::string outstring = llformat("vn %f %f %f\n",
										 normals[i].getF32ptr()[0],
										 normals[i].getF32ptr()[1],
										 normals[i].getF32ptr()[2]);
		if (fwrite(outstring.c_str(), 1, outstring.length(), fp) != outstring.length())
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}

	LLVector2* tex = getWritableTexCoords();
	for ( i=0; i<nverts; i++) {
		std::string outstring = llformat("vt %f %f\n",
										 tex[i][0],
										 tex[i][1]);
		if (fwrite(outstring.c_str(), 1, outstring.length(), fp) != outstring.length())
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}

	LLPolyFace* faces = getFaces();
	for ( i=0; i<nfaces; i++) {
		S32 f1 = faces[i][0] + 1;
		S32 f2 = faces[i][1] + 1;
		S32 f3 = faces[i][2] + 1;
		std::string outstring = llformat("f %d/%d/%d %d/%d/%d %d/%d/%d\n",
										 f1, f1, f1,
										 f2, f2, f2,
										 f3, f3, f3);
		if (fwrite(outstring.c_str(), 1, outstring.length(), fp) != outstring.length())
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}

	return TRUE;
}

// Finds binormal based on three vertices with texture coordinates.
// Fills in dummy values if the triangle has degenerate texture coordinates.
void calc_binormal_from_triangle(LLVector4a& binormal,

	const LLVector4a& pos0,
	const LLVector2& tex0,
	const LLVector4a& pos1,
	const LLVector2& tex1,
	const LLVector4a& pos2,
	const LLVector2& tex2)
{
	LLVector4a rx0( pos0[VX], tex0.mV[VX], tex0.mV[VY] );
	LLVector4a rx1( pos1[VX], tex1.mV[VX], tex1.mV[VY] );
	LLVector4a rx2( pos2[VX], tex2.mV[VX], tex2.mV[VY] );
	
	LLVector4a ry0( pos0[VY], tex0.mV[VX], tex0.mV[VY] );
	LLVector4a ry1( pos1[VY], tex1.mV[VX], tex1.mV[VY] );
	LLVector4a ry2( pos2[VY], tex2.mV[VX], tex2.mV[VY] );

	LLVector4a rz0( pos0[VZ], tex0.mV[VX], tex0.mV[VY] );
	LLVector4a rz1( pos1[VZ], tex1.mV[VX], tex1.mV[VY] );
	LLVector4a rz2( pos2[VZ], tex2.mV[VX], tex2.mV[VY] );
	
	LLVector4a lhs, rhs;

	LLVector4a r0; 
	lhs.setSub(rx0, rx1); rhs.setSub(rx0, rx2);
	r0.setCross3(lhs, rhs);
		
	LLVector4a r1;
	lhs.setSub(ry0, ry1); rhs.setSub(ry0, ry2);
	r1.setCross3(lhs, rhs);

	LLVector4a r2;
	lhs.setSub(rz0, rz1); rhs.setSub(rz0, rz2);
	r2.setCross3(lhs, rhs);

	if( r0[VX] && r1[VX] && r2[VX] )
	{
		binormal.set(
				-r0[VZ] / r0[VX],
				-r1[VZ] / r1[VX],
				-r2[VZ] / r2[VX]);
		// binormal.normVec();
	}
	else
	{
		binormal.set( 0, 1 , 0 );
	}
}

//-----------------------------------------------------------------------------
// LLPolyMesh::loadOBJ()
//-----------------------------------------------------------------------------
BOOL LLPolyMesh::loadOBJ(LLFILE *fp)
{
	if (!fp)
		return FALSE;

	int nverts = mSharedData->mNumVertices;
	int ntris  = mSharedData->mNumFaces;

	int nfaces     = 0;
	int ncoords    = 0;
	int nnormals   = 0;
	int ntexcoords = 0;

	LLVector4a* coords    = getWritableCoords();
	LLVector4a* normals   = getWritableNormals();
	LLVector4a* binormals = getWritableBinormals();
	LLVector2* tex       = getWritableTexCoords();
	LLPolyFace* faces    = getFaces();

	const S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];
	// *NOTE: changing the size or type of these buffers will require
	// changing the sscanf below.
	char keyword[256];
	keyword[0] = 0;
	F32 tempX;
	F32 tempY;
	F32 tempZ;
	S32 values;

	while (!feof(fp))
	{
		if (fgets(buffer, BUFSIZE, fp) == NULL)
		{
			buffer[0] = '\0';
		}

		if (sscanf (buffer," %255s", keyword) != 1)
		{
			// blank line
			continue;
		}

		if (!strcmp("v", keyword))
		{
			values = sscanf (buffer," %255s %f %f %f", keyword, &tempX, &tempY, &tempZ);
			if (values != 4)
			{
				LL_WARNS() << "Expecting v x y z, but found: " << buffer <<LL_ENDL;
				continue;
			}
			if (ncoords == nverts)
			{
				LL_WARNS() << "Too many vertices.  Ignoring from: " << buffer <<LL_ENDL;
			}
			if (ncoords < nverts)
			{
				coords[ncoords].set (tempX, tempY, tempZ);
			}
			ncoords++;
		}
		else if (!strcmp("vn",keyword))
		{
			values = sscanf (buffer," %255s %f %f %f", keyword, &tempX, &tempY, &tempZ);
			if (values != 4)
			{
				LL_WARNS() << "Expecting vn x y z, but found: " << buffer <<LL_ENDL;
				continue;
			}
			if (nnormals == nverts)
			{
				LL_WARNS() << "Too many normals.  Ignoring from: " << buffer <<LL_ENDL;
			}
			if (nnormals < nverts)
			{
				normals[nnormals].set (tempX, tempY, tempZ);
			}
			nnormals++;
		}
		else if (!strcmp("vt", keyword))
		{
			values = sscanf (buffer," %255s %f %f", keyword, &tempX, &tempY);
			if (values != 3)
			{
				LL_WARNS() << "Expecting vt x y, but found: " << buffer <<LL_ENDL;
				continue;
			}
			if (ntexcoords == nverts)
			{
				LL_WARNS() << "Too many texture vertices.  Ignoring from: " << buffer <<LL_ENDL;
			}
			if (ntexcoords < nverts)
			{
				tex[ntexcoords].set (tempX, tempY);
			}
			ntexcoords++;
		}
		else if (!strcmp("f",keyword))
		{
			if (nfaces == 0)
			{
				LL_WARNS() << "Ignoring face keywords for now." <<LL_ENDL;
			}
			nfaces++;
		}
		else
		{
			LL_INFOS() << "Unrecognized keyword.  Ignoring: " << buffer << LL_ENDL;
		}
	}

	// Compute the binormals
	// This computation is close, but not exactly what is being used in the
	// original meshes.

	int v;
	for ( v=0; v<nverts; v++)
	{
		binormals[v].clear();
	}

	int f;
	for ( f=0; f<ntris; f++)
	{
		S32 f0 = faces[f][0];
		S32 f1 = faces[f][1];
		S32 f2 = faces[f][2];

		LLVector4a binorm;
		calc_binormal_from_triangle(binorm, coords[f0], tex[f0], coords[f1], tex[f1], coords[f2], tex[f2]);
		binorm.normalize3fast();

		binormals[f0].add(binorm);
		binormals[f1].add(binorm);
		binormals[f2].add(binorm);
	}

	for ( v=0; v<nverts; v++)
	{
		binormals[v].normalize3();
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// LLPolyMesh::setSharedFromCurrent()
//-----------------------------------------------------------------------------
BOOL LLPolyMesh::setSharedFromCurrent()
{
	// Because meshes are set by continually updating morph weights
	// there is no easy way to reapply the morphs, so we just compute
	// the change in the base mesh and apply that.

	LLPolyMesh delta(mSharedData, NULL);
	U32 nverts = delta.getNumVertices();

	LLVector4a *delta_coords     = delta.getWritableCoords();
	LLVector4a *delta_normals    = delta.getWritableNormals();
	LLVector4a *delta_binormals  = delta.getWritableBinormals();
	LLVector2 *delta_tex_coords = delta.getWritableTexCoords();

	U32 vert_index;
	for( vert_index = 0; vert_index < nverts; vert_index++)
	{
		delta_coords[vert_index].sub(		mCoords[vert_index]);
		delta_normals[vert_index].sub(		mNormals[vert_index]);
		delta_binormals[vert_index].sub(	mBinormals[vert_index]);
		delta_tex_coords[vert_index]	-=	mTexCoords[vert_index];
	}

	// Now copy the new base mesh

	LLVector4a::memcpyNonAliased16((F32*) mSharedData->mBaseCoords, (F32*) mCoords, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mSharedData->mBaseNormals, (F32*) mNormals, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mSharedData->mBaseBinormals, (F32*) mBinormals, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mSharedData->mTexCoords, (F32*) mTexCoords, sizeof(LLVector2) * (mSharedData->mNumVertices + mSharedData->mNumVertices%2));

	// Update all avatars by applying the delta

	std::vector< LLCharacter* >::iterator avatar_it;
	for(avatar_it = LLCharacter::sInstances.begin(); avatar_it != LLCharacter::sInstances.end(); ++avatar_it)
	{
		LLAvatarAppearance* avatarp = (LLAvatarAppearance*)*avatar_it;
		LLPolyMesh* mesh = avatarp->getMesh(mSharedData);
		if (mesh)
		{
			LLVector4a *mesh_coords           = mesh->getWritableCoords();
			LLVector4a *mesh_normals          = mesh->getWritableNormals();
			LLVector4a *mesh_binormals        = mesh->getWritableBinormals();
			LLVector2 *mesh_tex_coords        = mesh->getWritableTexCoords();
			LLVector4a *mesh_scaled_normals   = mesh->getScaledNormals();
			LLVector4a *mesh_scaled_binormals = mesh->getScaledBinormals();

			for( vert_index = 0; vert_index < nverts; vert_index++)
			{
				mesh_coords[vert_index].sub(delta_coords[vert_index]);
				mesh_tex_coords[vert_index]       -= delta_tex_coords[vert_index];

				mesh_scaled_normals[vert_index].sub(delta_normals[vert_index]);
				LLVector4a normalized_normal       = mesh_scaled_normals[vert_index];
				normalized_normal.normalize3();
				mesh_normals[vert_index]           = normalized_normal;

				mesh_scaled_binormals[vert_index].sub(delta_binormals[vert_index]);
				LLVector4a tangent;
				tangent.setCross3(mesh_scaled_binormals[vert_index], normalized_normal);
				LLVector4a normalized_binormal;
				normalized_binormal.setCross3(normalized_normal, tangent);
				normalized_binormal.normalize3();
				mesh_binormals[vert_index]         = normalized_binormal;
			}

			avatarp->dirtyMesh();
		}
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMesh::getSharedMeshName()
//-----------------------------------------------------------------------------
std::string const* LLPolyMesh::getSharedMeshName(LLPolyMeshSharedData* shared)
{
	//-------------------------------------------------------------------------
	// search for an existing mesh with this shared data
	//-------------------------------------------------------------------------
	for(LLPolyMeshSharedDataTable::iterator iter = sGlobalSharedMeshList.begin(); iter != sGlobalSharedMeshList.end(); ++iter)
	{
		std::string const* mesh_name = &iter->first;
		LLPolyMeshSharedData* mesh = iter->second;

		if (mesh == shared)
			return mesh_name;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// LLPolyMesh::freeAllMeshes()
//-----------------------------------------------------------------------------
void LLPolyMesh::freeAllMeshes()
{
	// delete each item in the global lists
	for_each(sGlobalSharedMeshList.begin(), sGlobalSharedMeshList.end(), DeletePairedPointer());
	sGlobalSharedMeshList.clear();
}

LLPolyMeshSharedData *LLPolyMesh::getSharedData() const
{
	return mSharedData;
}


//--------------------------------------------------------------------
// LLPolyMesh::dumpDiagInfo()
//--------------------------------------------------------------------
void LLPolyMesh::dumpDiagInfo(void*)
{
	// keep track of totals
	U32 total_verts = 0;
	U32 total_faces = 0;
	U32 total_kb = 0;

	std::string buf;

        LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;
        LL_INFOS() << "       Global PolyMesh Table (DEBUG only)" << LL_ENDL;
        LL_INFOS() << "   Verts    Faces  Mem(KB) Name" << LL_ENDL;
        LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;

	// print each loaded mesh, and it's memory usage
	for(LLPolyMeshSharedDataTable::iterator iter = sGlobalSharedMeshList.begin();
		iter != sGlobalSharedMeshList.end(); ++iter)
	{
		const std::string& mesh_name = iter->first;
		LLPolyMeshSharedData* mesh = iter->second;

		S32 num_verts = mesh->mNumVertices;
		S32 num_faces = mesh->mNumFaces;
		U32 num_kb = mesh->getNumKB();

		std::string type;
		if (mesh->isLOD()) {
			type = "LOD ";
		} else {
			type = "base";
		}

		buf = llformat("%8d %8d %8d %s %s", num_verts, num_faces, num_kb, type.c_str(), mesh_name.c_str());
		LL_INFOS() << buf << LL_ENDL;

		total_verts += num_verts;
		total_faces += num_faces;
		total_kb += num_kb;
	}

        LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;
	buf = llformat("%8d %8d %8d TOTAL", total_verts, total_faces, total_kb );
        LL_INFOS() << buf << LL_ENDL;
        LL_INFOS() << "-----------------------------------------------------" << LL_ENDL;
}

//-----------------------------------------------------------------------------
// getWritableCoords()
//-----------------------------------------------------------------------------
LLVector4a *LLPolyMesh::getWritableCoords()
{
	return mCoords;
}

//-----------------------------------------------------------------------------
// getWritableNormals()
//-----------------------------------------------------------------------------
LLVector4a *LLPolyMesh::getWritableNormals()
{
	return mNormals;
}

//-----------------------------------------------------------------------------
// getWritableBinormals()
//-----------------------------------------------------------------------------
LLVector4a *LLPolyMesh::getWritableBinormals()
{
	return mBinormals;
}


//-----------------------------------------------------------------------------
// getWritableClothingWeights()
//-----------------------------------------------------------------------------
LLVector4a       *LLPolyMesh::getWritableClothingWeights()
{
	return mClothingWeights;
}

//-----------------------------------------------------------------------------
// getWritableTexCoords()
//-----------------------------------------------------------------------------
LLVector2	*LLPolyMesh::getWritableTexCoords()
{
	return mTexCoords;
}

//-----------------------------------------------------------------------------
// getScaledNormals()
//-----------------------------------------------------------------------------
LLVector4a *LLPolyMesh::getScaledNormals()
{
	return mScaledNormals;
}

//-----------------------------------------------------------------------------
// getScaledBinormals()
//-----------------------------------------------------------------------------
LLVector4a *LLPolyMesh::getScaledBinormals()
{
	return mScaledBinormals;
}


//-----------------------------------------------------------------------------
// initializeForMorph()
//-----------------------------------------------------------------------------
void LLPolyMesh::initializeForMorph()
{
	if (!mSharedData)
		return;

    LLVector4a::memcpyNonAliased16((F32*) mCoords, (F32*) mSharedData->mBaseCoords, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mNormals, (F32*) mSharedData->mBaseNormals, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mScaledNormals, (F32*) mSharedData->mBaseNormals, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mBinormals, (F32*) mSharedData->mBaseNormals, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mScaledBinormals, (F32*) mSharedData->mBaseNormals, sizeof(LLVector4a) * mSharedData->mNumVertices);
	LLVector4a::memcpyNonAliased16((F32*) mTexCoords, (F32*) mSharedData->mTexCoords, sizeof(LLVector2) * (mSharedData->mNumVertices + mSharedData->mNumVertices%2));

	for (U32 i = 0; i < (U32)mSharedData->mNumVertices; ++i)
	{
		mClothingWeights[i].clear();
	}
}


//-----------------------------------------------------------------------------
// getMorphList()
//-----------------------------------------------------------------------------
void LLPolyMesh::getMorphList (const std::string& mesh_name, morph_list_t* morph_list)
{
	if (!morph_list)
		return;

	LLPolyMeshSharedData* mesh_shared_data = get_if_there(sGlobalSharedMeshList, mesh_name, (LLPolyMeshSharedData*)NULL);

	if (!mesh_shared_data)
		return;

	LLPolyMeshSharedData::morphdata_list_t::iterator iter = mesh_shared_data->mMorphData.begin();
	LLPolyMeshSharedData::morphdata_list_t::iterator end = mesh_shared_data->mMorphData.end();

	for (; iter != end; ++iter)
	{
		LLPolyMorphData *morph_data = *iter;
		std::string morph_name = morph_data->getName();

		morph_list->insert(std::pair<std::string,LLPolyMorphData*>(morph_name, morph_data));
	}
	return;
}

//-----------------------------------------------------------------------------
// getMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData*	LLPolyMesh::getMorphData(const std::string& morph_name)
{
	if (!mSharedData)
		return NULL;
	for (LLPolyMeshSharedData::morphdata_list_t::iterator iter = mSharedData->mMorphData.begin();
		 iter != mSharedData->mMorphData.end(); ++iter)
	{
		LLPolyMorphData *morph_data = *iter;
		if (morph_data->getName() == morph_name)
		{
			return morph_data;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// removeMorphData()
//-----------------------------------------------------------------------------
// // erasing but not deleting seems bad, but fortunately we don't actually use this...
// void	LLPolyMesh::removeMorphData(LLPolyMorphData *morph_target)
// {
// 	if (!mSharedData)
// 		return;
// 	mSharedData->mMorphData.erase(morph_target);
// }

//-----------------------------------------------------------------------------
// deleteAllMorphData()
//-----------------------------------------------------------------------------
// void	LLPolyMesh::deleteAllMorphData()
// {
// 	if (!mSharedData)
// 		return;

// 	for_each(mSharedData->mMorphData.begin(), mSharedData->mMorphData.end(), DeletePointer());
// 	mSharedData->mMorphData.clear();
// }

//-----------------------------------------------------------------------------
// getWritableWeights()
//-----------------------------------------------------------------------------
F32*	LLPolyMesh::getWritableWeights() const
{
	return mSharedData->mWeights;
}

// End
