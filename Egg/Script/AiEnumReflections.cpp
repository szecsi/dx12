#include "../Common.h"
#include "EnumReflectionMap.h"
#include <assimp/importer.hpp>      // C++ importer interface
#include <assimp/scene.h>       // Output data structure
#include <assimp/postProcess.h> // Post processing flags
#include "AiEnumReflections.h"

#define ENUM(x) { typedef x STRUCTTYPE; Egg::Script::EnumReflectionMap< x  >::getMap()
#define FIELD(x) .add( #x , STRUCTTYPE :: x)

void Egg::Script::AiEnumReflections::initialize()
{
	ENUM(aiPostProcessSteps)
		FIELD(aiProcess_CalcTangentSpace)
		FIELD(aiProcess_Debone)
		FIELD(aiProcess_FindDegenerates)
		FIELD(aiProcess_FindInstances)
		FIELD(aiProcess_FindInvalidData)
		FIELD(aiProcess_FixInfacingNormals)
		FIELD(aiProcess_FlipUVs)
		FIELD(aiProcess_FlipWindingOrder)
		FIELD(aiProcess_GenNormals)
		FIELD(aiProcess_GenSmoothNormals)
		FIELD(aiProcess_GenUVCoords)
		FIELD(aiProcess_ImproveCacheLocality)
		FIELD(aiProcess_JoinIdenticalVertices)
		FIELD(aiProcess_LimitBoneWeights)
		FIELD(aiProcess_MakeLeftHanded)
		FIELD(aiProcess_OptimizeGraph)
		FIELD(aiProcess_OptimizeMeshes)
		FIELD(aiProcess_PreTransformVertices)
		FIELD(aiProcess_RemoveComponent)
		FIELD(aiProcess_RemoveRedundantMaterials)
		FIELD(aiProcess_SortByPType)
		FIELD(aiProcess_SplitByBoneCount)
		FIELD(aiProcess_SplitLargeMeshes)
		FIELD(aiProcess_TransformUVCoords)
		FIELD(aiProcess_Triangulate)
		FIELD(aiProcess_ValidateDataStructure);}
}
