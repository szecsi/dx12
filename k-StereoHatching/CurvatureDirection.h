#pragma once

#include "MetaballSdf.h"
#include "HatchVertex.h"
#include <Egg/Math/Float3.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

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

		struct PrincipalCurvature {
			Egg::Math::float3 dir;
			float kappa; // SIGNED curvature of the chosen direction
		};

		// Closed-form 2x2 symmetric eigen-decomposition of the tangent-plane
		// curvature tensor [[a,b],[b,c]] (expressed in the (t1,t2) basis),
		// shared by both the analytic-SDF path (below) and ImportedMesh.h's
		// discrete one-ring tensor fit -- same tensor-to-direction logic
		// regardless of how a/b/c were obtained.
		inline PrincipalCurvature PrincipalDirectionFromTensor(const Egg::Math::float3& t1, const Egg::Math::float3& t2, float a, float b, float c) {
			float discriminant = (a - c) * (a - c) + 4.0f * b * b;
			PrincipalCurvature result;

			if (discriminant < 1e-8f) {
				// Near-umbilic point -- direction is undefined, fall back
				// deterministically. No temporal coherence requirement to
				// preserve, so this is safe.
				result.dir = t1;
				result.kappa = 0.5f * (a + c);
				return result;
			}

			float theta = 0.5f * std::atan2(2.0f * b, a - c);
			float ct = std::cos(theta), st = std::sin(theta);
			Egg::Math::float3 d1 = t1 * ct + t2 * st;
			Egg::Math::float3 d2 = t1 * (-st) + t2 * ct;
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
			if (std::fabs(k1) >= std::fabs(k2)) { result.dir = d1; result.kappa = k1; }
			else { result.dir = d2; result.kappa = k2; }
			return result;
		}

		// Builds deduplicated vertex one-ring adjacency from a triangle index
		// buffer. Shared by both curvature paths below (and by the discrete
		// mesh tensor fit in ImportedCharacter.h) as the connectivity the
		// tensor-smoothing pass walks.
		inline std::vector<std::vector<uint32_t>> BuildOneRingAdjacency(size_t vertCount, const std::vector<uint32_t>& indices) {
			std::vector<std::vector<uint32_t>> neighbors(vertCount);
			for (size_t i = 0; i + 2 < indices.size(); i += 3) {
				uint32_t tri[3] = { indices[i], indices[i + 1], indices[i + 2] };
				for (int e = 0; e < 3; ++e) {
					neighbors[tri[e]].push_back(tri[(e + 1) % 3]);
					neighbors[tri[e]].push_back(tri[(e + 2) % 3]);
				}
			}
			for (size_t vi = 0; vi < vertCount; ++vi) {
				auto& nb = neighbors[vi];
				std::sort(nb.begin(), nb.end());
				nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
			}
			return neighbors;
		}

		// A tangent-plane curvature tensor [[a,b],[b,c]] is only meaningful
		// relative to the (t1,t2) basis it was expressed in -- and every
		// vertex has its OWN basis (built from its own normal), so two
		// neighboring vertices' raw (a,b,c) triples can't be averaged
		// directly. Embedding into world space as the 3x3 symmetric matrix
		// T = a*(t1 t1^T) + b*(t1 t2^T + t2 t1^T) + c*(t2 t2^T) removes that
		// basis-dependence entirely -- T lives in world coordinates, so a
		// one-ring average of neighboring vertices' T's is well-defined with
		// no parallel transport or angle bookkeeping needed, unlike trying to
		// average the chosen principal DIRECTIONS themselves (which are only
		// defined mod 180 degrees, and swap between two orthogonal
		// eigenvectors wherever |k1| and |k2| are close).
		struct Tensor3 { float m[3][3]; };

		inline Tensor3 TensorToWorld(const Egg::Math::float3& t1, const Egg::Math::float3& t2, float a, float b, float c) {
			float t1a[3] = { t1.x, t1.y, t1.z };
			float t2a[3] = { t2.x, t2.y, t2.z };
			Tensor3 T;
			for (int r = 0; r < 3; ++r)
				for (int cc = 0; cc < 3; ++cc)
					T.m[r][cc] = a * t1a[r] * t1a[cc] + b * (t1a[r] * t2a[cc] + t2a[r] * t1a[cc]) + c * t2a[r] * t2a[cc];
			return T;
		}

		inline Egg::Math::float3 ApplyTensor(const Tensor3& T, const Egg::Math::float3& v) {
			return Egg::Math::float3(
				T.m[0][0] * v.x + T.m[0][1] * v.y + T.m[0][2] * v.z,
				T.m[1][0] * v.x + T.m[1][1] * v.y + T.m[1][2] * v.z,
				T.m[2][0] * v.x + T.m[2][1] * v.y + T.m[2][2] * v.z);
		}

		// Umbrella-smooths a per-vertex world-space tensor field (self +
		// one-ring neighbors, equally weighted) over `iterations` rounds.
		// This is what actually suppresses the chaotic direction flips:
		// PrincipalDirectionFromTensor's max-|eigenvalue| selection is a
		// genuinely discontinuous function of a raw, per-vertex-noisy
		// tensor (it flips whenever noise nudges |k1| vs |k2|), but once the
		// INPUT tensor is spatially coherent, that same selection is stable
		// almost everywhere except genuine, isolated umbilic points.
		inline void SmoothWorldTensors(std::vector<Tensor3>& tensors, const std::vector<std::vector<uint32_t>>& neighbors, int iterations) {
			std::vector<Tensor3> next(tensors.size());
			for (int it = 0; it < iterations; ++it) {
				for (size_t vi = 0; vi < tensors.size(); ++vi) {
					const std::vector<uint32_t>& nb = neighbors[vi];
					if (nb.empty()) { next[vi] = tensors[vi]; continue; }
					Tensor3 sum = tensors[vi];
					for (uint32_t ni : nb)
						for (int r = 0; r < 3; ++r)
							for (int cc = 0; cc < 3; ++cc)
								sum.m[r][cc] += tensors[ni].m[r][cc];
					float invCount = 1.0f / (float)(nb.size() + 1);
					for (int r = 0; r < 3; ++r)
						for (int cc = 0; cc < 3; ++cc)
							next[vi].m[r][cc] = sum.m[r][cc] * invCount;
				}
				tensors.swap(next);
			}
		}

	}

	// eps is an absolute world-space offset for the central-difference
	// Hessian -- default tuned for this project's character scale (~0.1-2
	// units); callers with very differently-scaled SDFs should pass a
	// smaller/larger value explicitly.
	// `indices` is the same triangle list TessellateCharacter produced
	// alongside `verts` -- needed here only to build one-ring adjacency for
	// the tensor-smoothing pass (see Detail::SmoothWorldTensors), not to
	// evaluate curvature itself (that's still the exact analytic SDF).
	inline void ComputeCurvatureDirections(const CharacterSdf& sdf, std::vector<HatchVertex>& verts,
		const std::vector<uint32_t>& indices, float eps = 1e-3f, int smoothIterations = 2) {
		using namespace Egg::Math;

		std::vector<Detail::Tensor3> tensors(verts.size());

		// Pass 1: raw per-vertex tensor from the analytic SDF's numerical
		// Hessian, embedded into world space (see Detail::TensorToWorld) so
		// it can be smoothed against neighbors below with no basis
		// bookkeeping.
		for (size_t vi = 0; vi < verts.size(); ++vi) {
			const HatchVertex& v = verts[vi];
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

			tensors[vi] = Detail::TensorToWorld(t1, t2, a, b, c);
		}

		// Pass 2: smooth, then decode each vertex's own (now spatially
		// coherent) tensor back into its local (t1,t2) basis and pick the
		// final principal direction/curvature exactly as before.
		std::vector<std::vector<uint32_t>> neighbors = Detail::BuildOneRingAdjacency(verts.size(), indices);
		Detail::SmoothWorldTensors(tensors, neighbors, smoothIterations);

		for (size_t vi = 0; vi < verts.size(); ++vi) {
			HatchVertex& v = verts[vi];
			float3 t1, t2;
			Detail::BranchlessOnb(v.normal, t1, t2);

			const Detail::Tensor3& T = tensors[vi];
			float3 Tt1 = Detail::ApplyTensor(T, t1);
			float3 Tt2 = Detail::ApplyTensor(T, t2);
			float a = t1.Dot(Tt1);
			float c = t2.Dot(Tt2);
			float b = t1.Dot(Tt2);

			Detail::PrincipalCurvature pc = Detail::PrincipalDirectionFromTensor(t1, t2, a, b, c);
			v.hatchDirection = pc.dir;
			v.curvature = pc.kappa; // SIGNED curvature of the chosen direction (see HatchVertex.h)
		}
	}

}
