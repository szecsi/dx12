#pragma once

#include <Egg/Math/Float3.h>
#include <vector>
#include <algorithm>

// Closed-form signed-distance primitives (sphere, capsule) with exact analytic
// gradients, combined via a smooth-min whose gradient is also exact (derived
// via the chain rule through the blend weight, not numerically differenced --
// see MetaballSdf.cpp-level comment on SmoothMin below). Used to build the
// "cute, smooth, low-detail" character shapes that MarchingCubes.h tessellates
// and CurvatureDirection.h differentiates a second time (numerically, from
// this exact gradient) to get principal curvature directions.
namespace Hatch {

	struct SdfSample {
		float distance = 0.0f;
		Egg::Math::float3 gradient = Egg::Math::float3(0, 1, 0); // NOT necessarily unit length
	};

	inline float Saturate(float x) {
		return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
	}

	struct Sphere {
		Egg::Math::float3 center;
		float radius;

		SdfSample Evaluate(const Egg::Math::float3& p) const {
			Egg::Math::float3 d = p - center;
			float len = d.Length();
			SdfSample s;
			s.distance = len - radius;
			s.gradient = (len > 1e-6f) ? d.Normalize() : Egg::Math::float3(0, 1, 0);
			return s;
		}
	};

	struct Capsule {
		Egg::Math::float3 a, b;
		float radius;

		SdfSample Evaluate(const Egg::Math::float3& p) const {
			Egg::Math::float3 ab = b - a;
			float abLenSq = ab.Dot(ab);
			float h = (abLenSq > 1e-8f) ? Saturate((p - a).Dot(ab) / abLenSq) : 0.0f;
			Egg::Math::float3 closest = a + ab * h;
			Egg::Math::float3 d = p - closest;
			float len = d.Length();
			SdfSample s;
			s.distance = len - radius;
			s.gradient = (len > 1e-6f) ? d.Normalize() : Egg::Math::float3(0, 1, 0);
			return s;
		}
	};

	// Quadratic polynomial smooth-min (Quilez), value AND its exact analytic
	// gradient. Deriving value(a,b) = b + h*(a-b) - k*h*(1-h), with
	// h = saturate(0.5 + 0.5*(b-a)/k), the total derivatives (chain rule
	// through h, which itself depends on both a and b) are:
	//   dvalue/da = h       + dh/da * [(a-b) - k*(1-2h)]
	//   dvalue/db = (1 - h) + dh/db * [(a-b) - k*(1-2h)]
	// with dh/da = -0.5/k, dh/db = +0.5/k inside the unclamped region
	// (0 < h < 1), and both zero where h is clamped (matching value reducing
	// to exactly a or b there, with zero cross-sensitivity). The final
	// gradient w.r.t. position is then dvalue/da*grad(a) + dvalue/db*grad(b).
	inline SdfSample SmoothMin(const SdfSample& sa, const SdfSample& sb, float k) {
		if (k <= 1e-6f) {
			return (sa.distance < sb.distance) ? sa : sb;
		}
		float a = sa.distance, b = sb.distance;
		float t = 0.5f + 0.5f * (b - a) / k;
		bool clamped = (t <= 0.0f) || (t >= 1.0f);
		float h = Saturate(t);

		SdfSample s;
		s.distance = b + h * (a - b) - k * h * (1.0f - h);

		float dhda = clamped ? 0.0f : (-0.5f / k);
		float dhdb = clamped ? 0.0f : (0.5f / k);
		float dCommon = (a - b) - k * (1.0f - 2.0f * h);

		float dvda = h + dhda * dCommon;
		float dvdb = (1.0f - h) + dhdb * dCommon;

		s.gradient = sa.gradient * dvda + sb.gradient * dvdb;
		return s;
	}

	// Tagged-union primitive so a character can be stored as one flat list.
	struct Primitive {
		enum class Kind { Sphere, Capsule } kind;
		Sphere sphere;
		Capsule capsule;

		static Primitive MakeSphere(const Egg::Math::float3& center, float radius) {
			Primitive p; p.kind = Kind::Sphere; p.sphere = { center, radius }; return p;
		}
		static Primitive MakeCapsule(const Egg::Math::float3& a, const Egg::Math::float3& b, float radius) {
			Primitive p; p.kind = Kind::Capsule; p.capsule = { a, b, radius }; return p;
		}

		SdfSample Evaluate(const Egg::Math::float3& p) const {
			return (kind == Kind::Sphere) ? sphere.Evaluate(p) : capsule.Evaluate(p);
		}
	};

	// A character: a flat list of primitives, all smooth-min blended together
	// with one shared blend radius. Local-space only -- placement in the
	// world happens via a separate translation offset (see MetaballCharacters.h).
	struct CharacterSdf {
		std::vector<Primitive> primitives;
		float blendRadius = 0.25f;

		SdfSample Evaluate(const Egg::Math::float3& p) const {
			SdfSample acc = primitives[0].Evaluate(p);
			for (size_t i = 1; i < primitives.size(); ++i) {
				acc = SmoothMin(acc, primitives[i].Evaluate(p), blendRadius);
			}
			return acc;
		}
	};

}
