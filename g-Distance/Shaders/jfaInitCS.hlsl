#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// JFA (Jump Flooding Algorithm) seed init, pass 1 of 3 (see jfaStepCS.hlsl/
// jfaFinalizeCS.hlsl): marks every A-node whose ground-truth label differs
// from at least one of its 26 same-sublattice neighbors as a boundary seed
// (JfaSeedOut[idx]=idx, i.e. "nearest boundary node to me is myself");
// every other A-node starts with no known seed (SENTINEL_LABEL). A-
// sublattice only -- B has no ground-truth label to seed a boundary from.
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

    bool boundary = false;
    for (uint n = 0; n < 26 && !boundary; n++) {
        int3 nb = (int3)tid + SameLatticeOffsets[n];
        if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
        if (RasterLabel[AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z)] != myLabel) boundary = true;
    }

    JfaSeedOut[idx] = boundary ? idx : SENTINEL_LABEL;
}
