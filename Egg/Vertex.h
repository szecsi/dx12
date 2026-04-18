#pragma once

#include "Math/float3.h"

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

}
