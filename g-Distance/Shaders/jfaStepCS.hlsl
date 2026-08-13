#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// JFA propagation, pass 2 of 3 (see jfaInitCS.hlsl/jfaFinalizeCS.hlsl): one
// Jump-Flooding step. Every A-node adopts whichever of its own current-best
// known seed and its 26 same-sublattice neighbors' seeds -- sampled at
// Step*offset, not offset -- is nearest in real space, standard JFA. The
// caller (DistanceApp.h's RunTopologyBuild) dispatches this repeatedly with
// Step halving from the smallest power of two >= GridRes down to 1,
// ping-ponging JfaSeedIn/JfaSeedOut each pass; this shader only does one
// pass of that schedule.
#define JfaStepSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "CBV(b0)"

cbuffer JfaStepConsts : register(b1) {
    uint Step;
};

RWStructuredBuffer<uint> JfaSeedIn : register(u0);
RWStructuredBuffer<uint> JfaSeedOut : register(u1);

[RootSignature(JfaStepSig)]
[numthreads(4, 4, 4)]
void jfaStepCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;
    int3 iTid = (int3)tid;
    uint idx = AIdx(tid.x, tid.y, tid.z);
    float3 myPos = APos(iTid);

    uint bestSeed = JfaSeedIn[idx];
    float bestDist = 1.0e30;
    if (bestSeed != SENTINEL_LABEL) {
        bool isB; uint3 sIjk; DecodeNodeIndex(bestSeed, isB, sIjk);
        bestDist = length(myPos - APos((int3)sIjk));
    }

    for (uint n = 0; n < 26; n++) {
        int3 nb = iTid + SameLatticeOffsets[n] * (int)Step;
        if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
        uint candSeed = JfaSeedIn[AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z)];
        if (candSeed == SENTINEL_LABEL) continue;
        bool isBc; uint3 cIjk; DecodeNodeIndex(candSeed, isBc, cIjk);
        float d = length(myPos - APos((int3)cIjk));
        if (d < bestDist) { bestDist = d; bestSeed = candSeed; }
    }

    JfaSeedOut[idx] = bestSeed;
}
