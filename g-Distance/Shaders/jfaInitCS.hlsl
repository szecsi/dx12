#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// JFA (Jump Flooding Algorithm) seed init, pass 1 of 3 (see jfaStepCS.hlsl/
// jfaFinalizeCS.hlsl): every A-node with at least one of its 26 same-
// sublattice neighbors carrying a DIFFERENT ground-truth label seeds itself
// with that NEIGHBOR's own index (the closest one, if more than one
// qualifies) -- NOT its own index. Seeding with itself would make a
// boundary node's own finalized distance come out as 0 (distance to
// itself), which is wrong: a boundary node's nearest node with a different
// label is genuinely one of its neighbors, at real (nonzero) grid spacing,
// same as everywhere else. Every other A-node starts with no known seed
// (SENTINEL_LABEL), resolved by propagation (jfaStepCS.hlsl). A-sublattice
// only -- B has no ground-truth label to seed a boundary from.
//
// Whole-domain feature transform, computed once per Reinitialize (never on
// Continue, same "topology is init-only" convention as rasterLabelCS/
// buildCandidatesCS) -- its result (nodeFootDistBuffer, via jfaFinalizeCS)
// seeds buildCandidatesCS's initial candidate potentials as an approximate
// distance-to-boundary field, the JFA-based replacement for the old flat
// OwnLabelSeed/jitter init.
#define JfaInitSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "CBV(b0)"

RWStructuredBuffer<uint> RasterLabel : register(u0);
RWStructuredBuffer<uint> JfaSeedOut : register(u1);

[RootSignature(JfaInitSig)]
[numthreads(4, 4, 4)]
void jfaInitCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;
    uint idx = AIdx(tid.x, tid.y, tid.z);
    uint myLabel = RasterLabel[idx];
    float3 myPos = APos((int3)tid);

    uint bestSeed = SENTINEL_LABEL;
    float bestDist = 1.0e30;
    for (uint n = 0; n < 26; n++) {
        int3 nb = (int3)tid + SameLatticeOffsets[n];
        if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
        uint nIdx = AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z);
        if (RasterLabel[nIdx] == myLabel) continue;
        float d = length(myPos - APos(nb));
        if (d < bestDist) { bestDist = d; bestSeed = nIdx; }
    }

    JfaSeedOut[idx] = bestSeed;
}
