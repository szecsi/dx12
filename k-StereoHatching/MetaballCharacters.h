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

	// LSD (lysergic acid diethylamide): a topologically-faithful but
	// hand-placed (not force-field-relaxed) stylization of the ergoline
	// skeleton -- fused benzene+pyrrole (indole) rings, two further fused
	// six-membered rings (one carrying N6), an N6-methyl, and a C8
	// carbonyl-diethylamide tail -- rendered as one metaball "ball and
	// stick" character, atoms as spheres and bonds as thin capsules, all
	// smooth-min blended together like the other characters' body parts.
	inline std::vector<Character> BuildLsdMolecule(float blendRadius) {
		using Egg::Math::float3;
		std::vector<Character> chars;

		CharacterSdf sdf; sdf.blendRadius = blendRadius;
		auto& P = sdf.primitives;

		const float rC = 0.11f;   // ring carbon / nitrogen
		const float rO = 0.13f;   // oxygen (carbonyl)
		const float rMe = 0.09f;  // terminal methyl/ethyl carbon
		const float rBond = 0.045f;

		auto atom = [&](const float3& p, float r) { P.push_back(Primitive::MakeSphere(p, r)); };
		auto bond = [&](const float3& a, const float3& b) { P.push_back(Primitive::MakeCapsule(a, b, rBond)); };

		// Indole benzo ring (aromatic 6-ring)
		float3 a1(-0.24f, 1.47f, -0.03f), a2(-0.24f, 1.73f, -0.03f), a3(0.00f, 1.86f, 0.00f);
		float3 a4(0.24f, 1.73f, 0.05f), a5(0.24f, 1.47f, 0.05f), a6(0.00f, 1.34f, 0.02f);
		// Indole pyrrole ring, fused at a1-a6
		float3 n1(-0.46f, 1.60f, -0.10f), c2(-0.50f, 1.34f, -0.12f), c3(-0.28f, 1.19f, -0.02f);
		// Ring C, fused at c3-a6
		float3 m1(-0.30f, 0.95f, 0.05f), m2(-0.10f, 0.79f, 0.10f), m3(0.16f, 0.85f, 0.10f), m4(0.22f, 1.11f, 0.06f);
		// Ring D (carries N6 = d1), fused at m2-m3
		float3 d1(-0.14f, 0.53f, 0.15f), d2(0.02f, 0.37f, 0.20f), d3(0.24f, 0.47f, 0.20f), d4(0.30f, 0.73f, 0.15f);
		// N6-methyl
		float3 me1(-0.34f, 0.43f, 0.20f);
		// C8 carbonyl-diethylamide tail
		float3 co1(0.44f, 0.52f, 0.30f), o1(0.54f, 0.72f, 0.35f), namide(0.66f, 0.42f, 0.35f);
		float3 e1a(0.86f, 0.57f, 0.40f), e1b(1.06f, 0.67f, 0.45f);
		float3 e2a(0.84f, 0.22f, 0.45f), e2b(1.04f, 0.10f, 0.55f);

		for (const float3& p : { a1, a2, a3, a4, a5, a6, n1, c2, c3, m1, m2, m3, m4, d2, d3, d4, co1, e1a, e2a })
			atom(p, rC);
		atom(d1, rC);   // N6
		atom(o1, rO);   // carbonyl oxygen
		atom(me1, rMe); // N6-methyl
		atom(e1b, rMe); // ethyl terminal
		atom(e2b, rMe); // ethyl terminal

		bond(a1, a2); bond(a2, a3); bond(a3, a4); bond(a4, a5); bond(a5, a6); bond(a6, a1);
		bond(a1, n1); bond(n1, c2); bond(c2, c3); bond(c3, a6);
		bond(c3, m1); bond(m1, m2); bond(m2, m3); bond(m3, m4); bond(m4, a6);
		bond(m2, d1); bond(d1, d2); bond(d2, d3); bond(d3, d4); bond(d4, m3);
		bond(d1, me1);
		bond(d3, co1); bond(co1, o1); bond(co1, namide);
		bond(namide, e1a); bond(e1a, e1b);
		bond(namide, e2a); bond(e2a, e2b);

		chars.push_back({ sdf, float3(0, 0, 0), "LSD Molecule" });
		return chars;
	}

}
