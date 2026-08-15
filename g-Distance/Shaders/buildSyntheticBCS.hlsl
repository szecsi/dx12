#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"
#include "SyntheticField.hlsli"

// Synthetic-field B-node init (see smoothnessJacobiSyntheticCS.hlsl for the
// runtime shader this seeds, and SyntheticField.hlsli for the vote formula
// shared between the two). Replaces buildCandidatesCS.hlsl's Mode=1 (B)
// dispatch for this pipeline -- A-node seeding is UNCHANGED: buildCandidatesCS's
// Mode=0 output already writes exactly what this field wants at slot 0
// (label=RasterLabel, potential=NodeFootDist), so RunTopologyBuild keeps
// dispatching that unmodified for A and only swaps in this shader for B.
//
// Fast path: if this B-node's own 8 A-corners unanimously agree on one label
// (mirrors buildCandidatesCS.hlsl's bOwnCubeUniform/bOwnLabel/bOwnAvgDist
// tight scan exactly), assume that label with the confidence of the average
// NodeFootDist over those corners -- same reasoning as an A-node's own
// ground truth.
// Fallback: non-unanimous own cube -- run SyntheticVote8 over the same 8
// corners for the label, and seed potential at 0 -- genuinely no confidence
// yet, unlike the unanimous case's real footdist-derived average.
#define BuildSyntheticBSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b2)"

RWStructuredBuffer<uint>  RasterLabel : register(u0);
RWStructuredBuffer<uint>  NodeCandidateLabel : register(u1);
RWStructuredBuffer<float> NodePotential : register(u2);
RWStructuredBuffer<float> NodeFootDist : register(u3);

[RootSignature(BuildSyntheticBSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildSyntheticBCS(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= BCount) return;
    uint node = ACount + tid.x;

    uint k = tid.x / (BDim * BDim);
    uint rem = tid.x % (BDim * BDim);
    uint j = rem / BDim;
    uint i = rem % BDim;

    uint labels[8];
    float potentials[8];
    uint ownLabel0 = RasterLabel[AIdx(i, j, k)];
    bool ownUniform = true;
    for (uint c = 0; c < 8; c++) {
        uint aIdx = AIdx(i + (c & 1u), j + ((c >> 1) & 1u), k + ((c >> 2) & 1u));
        labels[c] = RasterLabel[aIdx];
        potentials[c] = NodeFootDist[aIdx];
        if (labels[c] != ownLabel0) ownUniform = false;
    }

    uint newLabel;
    float newPot;
    if (ownUniform) {
        float sum = 0.0;
        for (uint c2 = 0; c2 < 8; c2++) sum += potentials[c2];
        newLabel = ownLabel0;
        newPot = sum * 0.125; // /8
    } else {
        uint winnerLabel; float winnerScore;
        SyntheticVote8(labels, potentials, winnerLabel, winnerScore);
        newLabel = winnerLabel;
        newPot = 0.0;
    }

    // Byte 0 = current label. Byte 1 (scratch, written by
    // smoothnessJacobiSyntheticCS.hlsl) and bytes 2-3 (unused leftover
    // multi-candidate capacity) are never read in this mode -- no need to
    // preserve whatever garbage a prior allocation left there.
    NodeCandidateLabel[node * 2u + 0u] = newLabel & 0xFFu;
    NodePotential[node * MAX_CANDIDATES + 0u] = newPot;
}
