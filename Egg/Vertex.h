#pragma once

#include "Math/float3.h"
#include "Math/float4.h"
#include "Math/uint4.h"

namespace Egg {
	/*
	PNT: Position, Normal, Texture
	*/
	struct PNT_Vertex {
		Egg::Math::float3 position;
		Egg::Math::float3 normal;
		Egg::Math::float2 tex;
	};

	/*
	PNTTB: Position, Normal, Texture, Tangent, Bitangent
	*/
	struct PNTTB_Vertex {
		Egg::Math::float3 position;
		Egg::Math::float3 normal;
		Egg::Math::float2 tex;
		Egg::Math::float3 tangent;
		Egg::Math::float3 bitangent;
	};

	/*
	PNTTB_RI: Position, Normal, Tangent, Texture, Rigging (BlendIndices + BlendWeights)
	Layout matches treeVS IAOutput: POSITION(0), NORMAL(12), TANGENT(24), TEXCOORD(36), BLENDINDICES(44), BLENDWEIGHT(60)
	*/
	struct PNTTB_RI_Vertex {
		Egg::Math::float3 position;     // offset 0
		Egg::Math::float3 normal;       // offset 12
		Egg::Math::float3 tangent;      // offset 24
		Egg::Math::float2 tex;          // offset 36
		Egg::Math::uint4  blendIndices; // offset 44
		Egg::Math::float4 blendWeights; // offset 60
	};

}
