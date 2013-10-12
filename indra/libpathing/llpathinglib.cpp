#include "sys.h"
#include "llpathinglib.h"

void LLPathingLib::initSystem()
{
}

void LLPathingLib::quitSystem()
{
}

LLPathingLib* LLPathingLib::getInstance()
{
	static LLPathingLib sObj;
	return &sObj;
}

bool LLPathingLib::isFunctional()
{
	return false;
}

void LLPathingLib::extractNavMeshSrcFromLLSD( std::vector< unsigned char > const &aMeshData, int aNavigation )
{
}

LLPathingLib::LLPLResult LLPathingLib::generatePath( PathingPacket const& )
{
	return LLPL_NO_PATH;
}

void LLPathingLib::cleanupResidual()
{
}

void LLPathingLib::stitchNavMeshes( )
{
}

void LLPathingLib::renderNavMesh( void )
{
}

void LLPathingLib::renderNavMeshShapesVBO( unsigned int aShapeTypeBitmask )
{
}

void LLPathingLib::renderPath()
{
}

void LLPathingLib::cleanupVBOManager()
{
}

void LLPathingLib::setNavMeshColors( NavMeshColors const &aColors )
{
}

void LLPathingLib::setNavMeshMaterialType( LLPLCharacterType aType )
{
}

void LLPathingLib::renderNavMeshEdges()
{
}

void LLPathingLib::renderPathBookend( LLRender&, LLPLRenderType )
{
}

void LLPathingLib::renderSimpleShapes( LLRender&, float )
{
}

void LLPathingLib::createPhysicsCapsuleRep( float, float, bool, LLUUID const& )
{
}

void LLPathingLib::cleanupPhysicsCapsuleRepResiduals()
{
}

void LLPathingLib::processNavMeshData()
{
}

void LLPathingLib::renderSimpleShapeCapsuleID( LLRender&, LLUUID const&, LLVector3 const&, LLQuaternion const& )
{
}
