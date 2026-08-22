#pragma once

#include "MetaballSdf.h"
#include <Egg/Math/Float3.h>
#include <vector>

// Three concrete "cute, low-detail, smooth" characters (a bunny, a blob-cat,
// and a chick), each a hand-placed list of sphere/capsule primitives fed to
// MetaballSdf's smooth-min blend. Placement between characters uses a HARD
// min (see StereoHatchingApp.h's SceneSdf) so they never visually fuse with
// each other -- only a single character's own primitives blend together.
namespace Hatch {

	struct AABB {
		Egg::Math::float3 min;
		Egg::Math::float3 max;
	};

	// Local-space bounding box of a character's primitives, padded generously
	// (2x the blend radius) since smooth-min can bulge outside the naive
	// per-primitive union near blend joints.
	inline AABB ComputeLocalBounds(const CharacterSdf& sdf) {
		Egg::Math::float3 mn(1e9f, 1e9f, 1e9f);
		Egg::Math::float3 mx(-1e9f, -1e9f, -1e9f);

		auto expand = [&](const Egg::Math::float3& c, float r) {
			mn.x = std::min(mn.x, c.x - r); mn.y = std::min(mn.y, c.y - r); mn.z = std::min(mn.z, c.z - r);
			mx.x = std::max(mx.x, c.x + r); mx.y = std::max(mx.y, c.y + r); mx.z = std::max(mx.z, c.z + r);
		};

		for (const Primitive& p : sdf.primitives) {
			if (p.kind == Primitive::Kind::Sphere) {
				expand(p.sphere.center, p.sphere.radius);
			} else {
				expand(p.capsule.a, p.capsule.radius);
				expand(p.capsule.b, p.capsule.radius);
			}
		}

		float pad = sdf.blendRadius * 2.0f;
		mn = mn - Egg::Math::float3(pad, pad, pad);
		mx = mx + Egg::Math::float3(pad, pad, pad);
		return { mn, mx };
	}

	struct Character {
		CharacterSdf sdf;
		Egg::Math::float3 worldOffset;
		const char* name;
	};

	// blendRadius is shared across all three characters by the caller (GUI
	// "Curvature Blend Radius" slider) -- BuildCharacters takes it so
	// "Rebuild Characters" can regenerate with a new value without editing code.
	inline std::vector<Character> BuildCharacters(float blendRadius) {
		using Egg::Math::float3;
		std::vector<Character> chars;

		// -- Bunny --
		{
			CharacterSdf sdf; sdf.blendRadius = blendRadius;
			auto& P = sdf.primitives;
			P.push_back(Primitive::MakeSphere(float3(0, 0.5f, 0), 0.5f));           // lower body
			P.push_back(Primitive::MakeSphere(float3(0, 0.9f, 0.02f), 0.38f));      // upper body/chest
			P.push_back(Primitive::MakeSphere(float3(0, 1.35f, 0.08f), 0.32f));     // head
			P.push_back(Primitive::MakeCapsule(float3(-0.12f, 1.55f, -0.02f), float3(-0.16f, 2.05f, -0.06f), 0.09f)); // left ear
			P.push_back(Primitive::MakeCapsule(float3(0.12f, 1.55f, -0.02f), float3(0.16f, 2.05f, -0.06f), 0.09f));  // right ear
			P.push_back(Primitive::MakeSphere(float3(0, 0.55f, -0.48f), 0.15f));    // tail
			P.push_back(Primitive::MakeCapsule(float3(-0.25f, 0.28f, 0.2f), float3(-0.25f, 0.0f, 0.2f), 0.13f));  // front-left leg
			P.push_back(Primitive::MakeCapsule(float3(0.25f, 0.28f, 0.2f), float3(0.25f, 0.0f, 0.2f), 0.13f));   // front-right leg
			P.push_back(Primitive::MakeCapsule(float3(-0.25f, 0.28f, -0.22f), float3(-0.25f, 0.0f, -0.22f), 0.13f)); // back-left leg
			P.push_back(Primitive::MakeCapsule(float3(0.25f, 0.28f, -0.22f), float3(0.25f, 0.0f, -0.22f), 0.13f));  // back-right leg
			chars.push_back({ sdf, float3(-1.6f, 0, 0), "Bunny" });
		}

		// -- Blob-cat --
		{
			CharacterSdf sdf; sdf.blendRadius = blendRadius;
			auto& P = sdf.primitives;
			P.push_back(Primitive::MakeSphere(float3(0, 0.5f, 0), 0.5f));          // body
			P.push_back(Primitive::MakeSphere(float3(0, 1.15f, 0.05f), 0.32f));    // head
			P.push_back(Primitive::MakeSphere(float3(-0.18f, 1.42f, 0.0f), 0.12f)); // left ear
			P.push_back(Primitive::MakeSphere(float3(0.18f, 1.42f, 0.0f), 0.12f));  // right ear
			P.push_back(Primitive::MakeCapsule(float3(0, 0.4f, -0.5f), float3(0.15f, 0.65f, -0.75f), 0.09f)); // tail base
			P.push_back(Primitive::MakeCapsule(float3(0.15f, 0.65f, -0.75f), float3(0.35f, 0.9f, -0.7f), 0.07f)); // tail tip
			P.push_back(Primitive::MakeCapsule(float3(-0.22f, 0.25f, 0.22f), float3(-0.22f, 0.0f, 0.22f), 0.12f));
			P.push_back(Primitive::MakeCapsule(float3(0.22f, 0.25f, 0.22f), float3(0.22f, 0.0f, 0.22f), 0.12f));
			P.push_back(Primitive::MakeCapsule(float3(-0.22f, 0.25f, -0.22f), float3(-0.22f, 0.0f, -0.22f), 0.12f));
			P.push_back(Primitive::MakeCapsule(float3(0.22f, 0.25f, -0.22f), float3(0.22f, 0.0f, -0.22f), 0.12f));
			chars.push_back({ sdf, float3(0, 0, 0), "Blob-cat" });
		}

		// -- Chick --
		{
			CharacterSdf sdf; sdf.blendRadius = blendRadius;
			auto& P = sdf.primitives;
			P.push_back(Primitive::MakeSphere(float3(0, 0.42f, 0.05f), 0.4f));     // front body/egg
			P.push_back(Primitive::MakeSphere(float3(0, 0.40f, -0.15f), 0.32f));   // back body/egg
			P.push_back(Primitive::MakeSphere(float3(0, 0.95f, 0.15f), 0.24f));    // head
			P.push_back(Primitive::MakeCapsule(float3(0, 0.95f, 0.35f), float3(0, 0.9f, 0.48f), 0.06f)); // beak
			P.push_back(Primitive::MakeSphere(float3(-0.32f, 0.5f, 0.0f), 0.16f)); // left wing
			P.push_back(Primitive::MakeSphere(float3(0.32f, 0.5f, 0.0f), 0.16f));  // right wing
			P.push_back(Primitive::MakeCapsule(float3(-0.12f, 0.25f, 0), float3(-0.12f, 0.02f, 0), 0.045f)); // left leg
			P.push_back(Primitive::MakeCapsule(float3(0.12f, 0.25f, 0), float3(0.12f, 0.02f, 0), 0.045f));  // right leg
			chars.push_back({ sdf, float3(1.6f, 0, 0), "Chick" });
		}

		return chars;
	}

}
