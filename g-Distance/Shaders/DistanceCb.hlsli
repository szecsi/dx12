#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _DIST_ALIGN2 __declspec(align(16))
#else
#define _DIST_ALIGN2
#endif

// Per-run tunables for the combinatorics/Jacobi solve loop -- one shared
// cbuffer, always bound at b0 for every compute pass that needs it (same
// "single tunables cbuffer" convention g-BCC/g-Aequor used).
//
// Laid out as 2 explicit 4-field (16-byte) rows -- no automatic HLSL cbuffer
// packing relied on, since the C++ struct view has to match byte-for-byte.

#ifndef __HLSL_VERSION
_DIST_ALIGN2 struct
#else
cbuffer
#endif

DistanceCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    // Row 0 -- smoothness (energy term 1) + margin hinge (term 2)
    float SmoothnessWeight;   // per-tet-pair normal-disagreement penalty weight
    float MarginWeight;       // margin hinge weight
    float MarginTarget;       // required gap: A-node's own potential vs. any competitor; B-node's current winner vs. runner-up
    float RegularizerWeight;  // sum-to-0 weight (term 3): pushes each node's candidate potentials to sum to 0

    // Row 1 -- Jacobi solve mechanics + candidate seeding
    float JacobiDiagEpsilon;  // denominator floor, same role as g-Aequor's MinSystemDiagonal
    float SeedJitter;         // amplitude of buildCandidatesCS's symmetry-breaking jitter
    float OwnLabelSeed;       // initial raw potential for an A-node's own/input label slot
    // Hard per-sweep clamp on |newPhi-phi| for every (node,slot) unknown --
    // without this, naive per-unknown-diagonal Jacobi on this coupled system
    // overshoots and diverges (confirmed: potentials blew up ~2000x/round
    // without a clamp). Same role as g-Aequor's MaxStep -- see
    // smoothnessJacobiCS.hlsl.
    float MaxPotentialStep;

    // Row 2 -- volume conservation (MTV = Momentary Target Volume)
    float VolumeWeight;         // energy term 4 weight: pushes each node's reconstructed volume toward its MTV
    float MTVDiffusionRate;     // mtvDiffuseCS.hlsl: how fast MTV equalizes between same-label neighbors each round
    // mtvDiffuseCS.hlsl: lets a node's ACTUAL reconstructed volume
    // (NodeCurrentVolume, written by smoothnessJacobiCS's Term 4 every
    // sweep) pull its own MTV target toward whatever it's already
    // achieving, gated by the same diffCount>0 condition as the plain
    // equalization term above -- so it has NO effect on an isolated node
    // (nothing to diffuse with, full protection preserved), but lets a
    // well-connected region's target relax toward its natural (possibly
    // non-uniform) smoothness-driven distribution instead of fighting to
    // match a flat diffused average everywhere.
    float VolumePushbackRate;
    float _pad2b;
}
#ifndef __HLSL_VERSION
;
#endif
