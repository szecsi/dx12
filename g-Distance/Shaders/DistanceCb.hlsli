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
    // UNUSED (kept, not deleted, per this project's "leave dead
    // infrastructure in place" convention): initial raw potential for an
    // A-node's own/input label slot. Superseded by JFA-based seeding
    // (NodeFootDist, buildCandidatesCS.hlsl) -- see that file's potential-
    // seeding block.
    float OwnLabelSeed;
    // Hard per-sweep clamp on |newPhi-phi| for every (node,slot) unknown --
    // without this, naive per-unknown-diagonal Jacobi on this coupled system
    // overshoots and diverges (confirmed: potentials blew up ~2000x/round
    // without a clamp). Same role as g-Aequor's MaxStep -- see
    // smoothnessJacobiCS.hlsl.
    float MaxPotentialStep;

    // Row 2 -- volume floor (energy term 4, connecting nodes only -- see
    // smoothnessJacobiCS.hlsl). Supersedes the earlier MTV-diffusion system
    // (removed entirely: it did nothing measurable for the torus and was
    // fully subsumed, for the cases it actually mattered, by connecting-
    // node pinning).
    float VolumeWeight;  // term 4 weight: how hard to push a connecting node's volume back up when it dips below VolumeFloor
    float VolumeFloor;   // the floor itself, in tet-volume units (default 1 -- see the isolated-point geometric derivation in project memory)
    // Term 1's TetFieldGrad missing-candidate fallback (smoothnessJacobiCS.hlsl's
    // GetCornerPotential call) -- the potential value assumed for a real tet
    // corner that genuinely doesn't carry a given label as one of its own
    // candidates. GUI slider, range -2..1.
    float MissingFallback;
    // Term 1 pair-listing method, GUI checkbox: 1 = edge-walking (GatherEdgeTets
    // over this node's 14 neighbor edges), 0 = check all node-adjacent tets
    // (GatherIncidentTets + GetFaceAdjacentPartner) -- see smoothnessJacobiCS.hlsl.
    uint UseEdgeWalkTraversal;

    // Row 3 -- distance/Eikonal shaping (energy term 5, smoothnessJacobiCS.hlsl).
    // Pushes a same-label potential difference along a real geometric edge
    // toward that edge's actual Euclidean length -- shapes each candidate's
    // potential into an approximate unit-gradient distance field, the
    // consistency term a future JFA-footvector-length potential init will
    // need to not just decay back toward Term 3's sum-to-0 pull.
    float DistanceWeight;  // term 5 weight -- 0 disables the term entirely
    // Term 1 fast path, GUI checkbox (>0.5 = on): replaces tet-walking
    // (both UseEdgeWalkTraversal modes) with the closed-form 27-tap fixed
    // lattice kernel derived and numerically verified against the tet-walk
    // result (see smoothnessJacobiCS.hlsl's FixedKernelSmoothness) -- exact,
    // not an approximation, for any MissingFallback/candidate-tracking
    // pattern, since it's a direct algebraic reduction of the same sum over
    // face-adjacent tet pairs. Was _pad3a.
    float UseFixedKernelSmoothing;
    // Term 6 weight (smoothnessJacobiCS.hlsl): draws a DIVERGENT A-node's
    // (NodeIsConnecting bit2, computeConnectingNodesCS.hlsl) own/input-label
    // potential (slot 0) back toward its NodeFootDist -- a stabilizer for
    // exactly the nodes whose footvector conflicts with a same-label
    // neighbor's (a saddle/pinch region), where the solve is most likely to
    // distort phi0 away from its sensible JFA-distance value. Ramped like
    // Term 5: negligible pull for |phi0-NodeFootDist|<1, rising sharply as
    // that gap approaches/exceeds 1 unit. 0 disables the term entirely. Was
    // _pad3b.
    float DivergencePullWeight;
    // Synthetic-field pipeline (smoothnessJacobiSyntheticCS.hlsl/
    // buildSyntheticBCS.hlsl): potential floor a node's single synthetic
    // potential is clamped to whenever the 27-tap signed-kernel correction
    // drives it negative ("my label no longer holds here") -- also the fixed
    // potential a B-node gets whenever it's freshly relabeled (both at init
    // and at runtime), matching Term 5/6's small-positive-floor role but for
    // this entirely separate single-label/potential field, not the
    // multi-candidate one. Was _pad3c.
    float SyntheticEpsilon;
}
#ifndef __HLSL_VERSION
;
#endif
