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
//
// Label-validated candidates (bug fix): a candidate seed is only ever
// accepted if RasterLabel[seed] != this node's own label -- including
// rejecting this node's own currently-held seed if it turns out to violate
// that (self-heals a bad value from a stale pre-fix run rather than
// propagating it further). Earlier versions compared purely by distance,
// which let two mutually-adjacent differently-labeled boundary nodes each
// discover the OTHER as its own seed in Init, then in the very first step
// each one scans its neighbor and reads that neighbor's seed == itself,
// computes distance-to-self == 0 (unbeatable), and adopts itself --
// producing self-referencing nodes, and even after guarding against literal
// self-adoption, nodes could still pick up a same-label neighbor's
// (non-self but irrelevant) seed the same way, producing spuriously short
// "one-length" distances that don't correspond to any real label crossing
// from this node's own position. Validating the candidate's actual label
// (not just its index) against this node's own label closes both cases.
#define JfaStepSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

cbuffer JfaStepConsts : register(b1) {
    uint Step;
};

RWStructuredBuffer<uint> JfaSeedIn : register(u0);
RWStructuredBuffer<uint> JfaSeedOut : register(u1);
// A-sublattice ground-truth labels, ACount-sized -- every JFA seed value IS
// an A-node global index (JFA only ever seeds/propagates over A), so it's
// directly usable as a RasterLabel index with no decode needed.
RWStructuredBuffer<uint> RasterLabel : register(u2);

[RootSignature(JfaStepSig)]
[numthreads(4, 4, 4)]
void jfaStepCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;
    int3 iTid = (int3)tid;
    uint idx = AIdx(tid.x, tid.y, tid.z);
    uint myLabel = RasterLabel[idx];
    float3 myPos = APos(iTid);

    uint bestSeed = JfaSeedIn[idx];
    float bestDist = 1.0e30;
    if (bestSeed != SENTINEL_LABEL) {
        if (RasterLabel[bestSeed] == myLabel) {
            bestSeed = SENTINEL_LABEL; // heal: an invalid same-label/self seed, don't keep propagating it
        } else {
            bool isB; uint3 sIjk; DecodeNodeIndex(bestSeed, isB, sIjk);
            bestDist = length(myPos - APos((int3)sIjk));
        }
    }

    for (uint n = 0; n < 26; n++) {
        int3 nb = iTid + SameLatticeOffsets[n] * (int)Step;
        if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
        uint candSeed = JfaSeedIn[AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z)];
        if (candSeed == SENTINEL_LABEL) continue;
        if (RasterLabel[candSeed] == myLabel) continue; // reject same-label (incl. self) candidate
        bool isBc; uint3 cIjk; DecodeNodeIndex(candSeed, isBc, cIjk);
        float d = length(myPos - APos((int3)cIjk));
        if (d < bestDist) { bestDist = d; bestSeed = candSeed; }
    }

    JfaSeedOut[idx] = bestSeed;
}
