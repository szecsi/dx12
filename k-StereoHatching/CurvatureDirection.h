#pragma once

#include "MetaballSdf.h"
#include "HatchVertex.h"
#include <Egg/Math/Float3.h>
#include <vector>
#include <cmath>

// Per-vertex principal-curvature-direction computation for marching-
// tetrahedra-tessellated characters. Since the exact SDF and its exact
// (closed-form) gradient already exist (MetaballSdf.h), this differentiates
// the GRADIENT numerically (central differences -- one order of numerical
// differentiation, not two) to get the Hessian, rather than hand-deriving
// second derivatives through nested smooth-min compositions or falling back
// to a discrete mesh one-ring curvature estimator.
namespace Hatch {

	namespace Detail {

		// Duff et al. 2017 "Building an Orthonormal Basis, Revisited" --
		// branchless, numerically stable for any unit n (including near the
		// poles), unlike naive cross-product-with-an-arbitrary-axis schemes.
		inline void BranchlessOnb(const Egg::Math::float3& n, Egg::Math::float3& b1, Egg::Math::float3& b2) {
			float sign = (n.z >= 0.0f) ? 1.0f : -1.0f;
			float a = -1.0f / (sign + n.z);
			float b = n.x * n.y * a;
			b1 = Egg::Math::float3(1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x);
			b2 = Egg::Math::float3(b, sign + n.y * n.y * a, -n.y);
		}

	}

	// eps is an absolute world-space offset for the central-difference
	// Hessian -- default tuned for this project's character scale (~0.1-2
	// units); callers with very differently-scaled SDFs should pass a
	// smaller/larger value explicitly.
	inline void ComputeCurvatureDirections(const CharacterSdf& sdf, std::vector<HatchVertex>& verts, float eps = 1e-3f) {
		using namespace Egg::Math;

		for (HatchVertex& v : verts) {
			const float3& p = v.position;
			const float3& n = v.normal; // already unit length, from TessellateCharacter

			// Numerical Hessian of the analytic gradient (central differences),
			// symmetrized (smooth-min's clamped regions make the raw
			// numerical gradient-of-gradient slightly asymmetric).
			float H[3][3];
			const float3 axes[3] = { float3(1,0,0), float3(0,1,0), float3(0,0,1) };
			float3 gradCol[3];
			for (int j = 0; j < 3; ++j) {
				float3 gp = sdf.Evaluate(p + axes[j] * eps).gradient;
				float3 gm = sdf.Evaluate(p - axes[j] * eps).gradient;
				gradCol[j] = (gp - gm) * (1.0f / (2.0f * eps));
			}
			for (int r = 0; r < 3; ++r)
				for (int c = 0; c < 3; ++c) {
					float hrc = (r == 0 ? gradCol[c].x : (r == 1 ? gradCol[c].y : gradCol[c].z));
					float hcr = (c == 0 ? gradCol[r].x : (c == 1 ? gradCol[r].y : gradCol[r].z));
					H[r][c] = 0.5f * (hrc + hcr);
				}

			float3 t1, t2;
			Detail::BranchlessOnb(n, t1, t2);

			auto applyH = [&](const float3& v3) -> float3 {
				return float3(
					H[0][0] * v3.x + H[0][1] * v3.y + H[0][2] * v3.z,
					H[1][0] * v3.x + H[1][1] * v3.y + H[1][2] * v3.z,
					H[2][0] * v3.x + H[2][1] * v3.y + H[2][2] * v3.z);
			};

			float3 Ht1 = applyH(t1);
			float3 Ht2 = applyH(t2);
			float a = t1.Dot(Ht1);
			float c = t2.Dot(Ht2);
			float b = t1.Dot(Ht2);

			float discriminant = (a - c) * (a - c) + 4.0f * b * b;
			float3 dir;
			float kappa; // SIGNED curvature of the chosen direction -- sign preserved (see HatchVertex.h)

			if (discriminant < 1e-8f) {
				// Near-umbilic point (e.g. a sphere tip) -- direction is
				// undefined, fall back deterministically. No temporal
				// coherence requirement to preserve, so this is safe.
				dir = t1;
				kappa = 0.5f * (a + c);
			} else {
				float theta = 0.5f * std::atan2(2.0f * b, a - c);
				float ct = std::cos(theta), st = std::sin(theta);
				float3 d1 = t1 * ct + t2 * st;
				float3 d2 = t1 * (-st) + t2 * ct;
				float r = std::sqrt(0.25f * (a - c) * (a - c) + b * b);
				float k1 = 0.5f * (a + c) + r;
				float k2 = 0.5f * (a + c) - r;

				// Stroke direction follows the MOST-curved principal direction
				// (larger |curvature|) -- e.g. around a cylinder's
				// circumference, not along its axis -- so strokes read as
				// wrapping tightly around the form. The SIGN of that
				// direction's own curvature is kept (not discarded via fabs)
				// so stroke arcs bend consistently concave/convex instead of
				// all bowing the same way.
				if (std::fabs(k1) >= std::fabs(k2)) { dir = d1; kappa = k1; }
				else { dir = d2; kappa = k2; }
			}

			v.hatchDirection = dir;
			v.curvature = kappa;
		}
	}

}
