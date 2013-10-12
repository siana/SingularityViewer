#ifndef LL_PATHINGLIB_H
#define LL_PATHINGLIB_H

#include <vector>

#include "v3math.h"
#include "v4coloru.h"
#include "llquaternion.h"
#include "lluuid.h"

class LLRender;

class LLPathingLib
{
public:
	enum LLPLResult
	{
		LLPL_NO_PATH,
		LLPL_PATH_GENERATED_OK
	};

	enum LLPLCharacterType
	{
		LLPL_CHARACTER_TYPE_NONE,
		LLPL_CHARACTER_TYPE_A,
		LLPL_CHARACTER_TYPE_B,
		LLPL_CHARACTER_TYPE_C,
		LLPL_CHARACTER_TYPE_D
	};

	enum LLShapeType
	{
		LLST_WalkableObjects = 1,
		LLST_ObstacleObjects = 2,
		LLST_MaterialPhantoms = 3,
		LLST_ExclusionPhantoms = 4
	};

	enum LLPLRenderType
	{
		LLPL_START,
		LLPL_END
	};

	struct Vector
	{
		double mX;
		double mY;
		double mZ;
		
		Vector& operator=( LLVector3 const &aRHS )
		{
			mX = aRHS.mV[0];
			mY = aRHS.mV[1];
			mZ = aRHS.mV[2];

			return *this;
		}
		
		operator LLVector3() const
		{
			LLVector3 ret;
			ret.mV[0] = (F32) mX;
			ret.mV[1] = (F32) mY;
			ret.mV[2] = (F32) mZ;
			return ret;
		}
	};

	struct PathingPacket
	{
		Vector mStartPointA;
		Vector mEndPointA;

		Vector mStartPointB;
		Vector mEndPointB;

		bool mHasPointA;
		bool mHasPointB;

		double mCharacterWidth;
		LLPLCharacterType mCharacterType;
	};

	struct NavMeshColor
	{
		unsigned char mR;
		unsigned char mG;
		unsigned char mB;
		unsigned char mAlpha;

		NavMeshColor& operator=( LLColor4U const &aRHS )
		{
			mR = aRHS.mV[0];
			mG = aRHS.mV[1];
			mB = aRHS.mV[2];
			mAlpha = aRHS.mV[3];

			return *this;
		}
		
	};

	struct NavMeshColors
	{
		NavMeshColor mWalkable;
		NavMeshColor mObstacle;
		NavMeshColor mMaterial;
		NavMeshColor mExclusion;
		NavMeshColor mConnectedEdge;
		NavMeshColor mBoundaryEdge;
		NavMeshColor mHeatColorBase;
		NavMeshColor mHeatColorMax;
		NavMeshColor mFaceColor;
		NavMeshColor mStarValid;
		NavMeshColor mStarInvalid;
		NavMeshColor mTestPath;
		NavMeshColor mWaterColor;
	};

	static void initSystem();
	static void quitSystem();
	static LLPathingLib* getInstance();

	bool isFunctional();

	void extractNavMeshSrcFromLLSD( std::vector< unsigned char> const &aMeshData, int aNavigation );

	LLPLResult generatePath( PathingPacket const& );
	void cleanupResidual();

	void stitchNavMeshes( );

	void renderNavMesh( void );
	void renderNavMeshShapesVBO( unsigned int aShapeTypeBitmask );
	void renderPath();
	void cleanupVBOManager();

	void renderNavMeshEdges();
	void renderPathBookend( LLRender&, LLPLRenderType );

	void setNavMeshColors( NavMeshColors const& );
	void setNavMeshMaterialType( LLPLCharacterType );

	void renderSimpleShapes( LLRender&, float );

	void createPhysicsCapsuleRep( float, float, bool, LLUUID const& );
	void cleanupPhysicsCapsuleRepResiduals();

	void processNavMeshData();
	void renderSimpleShapeCapsuleID( LLRender&, LLUUID const&, LLVector3 const&, LLQuaternion const& );
};

#endif
