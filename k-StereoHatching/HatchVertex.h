#pragma once

#include <Egg/Math/Float3.h>

namespace Hatch {

	// Vertex layout for marching-cubes-tessellated characters. hatchDirection
	// is named generically (not "tangent") so a future imported-mesh path
	// could populate it from real UV tangents without touching any downstream
	// shader -- v1's procedural characters always populate it from
	// CurvatureDirection.h's principal-curvature computation, since marching
	// cubes output has no UV/tangent data of its own.
	// Stride: 3*4 + 3*4 + 3*4 + 4 = 40 bytes.
	struct HatchVertex {
		Egg::Math::float3 position;            // POSITION,  offset 0
		Egg::Math::float3 normal;              // NORMAL,    offset 12
		Egg::Math::float3 hatchDirection;      // TEXCOORD0, offset 24 (unit, tangent-plane, local space)
		// SIGNED curvature (1/world-length) of the chosen principal direction
		// -- sign is load-bearing now (bends stroke arcs consistently
		// concave/convex, see strokeVS.hlsl), not just a magnitude.
		float curvature = 0.0f;                // TEXCOORD1, offset 36
	};

}
