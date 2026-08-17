#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// Scratch -> main copy for the synthetic-field path (see
// smoothnessJacobiSyntheticCS.hlsl). Mirrors commitPotentialBlockCS.hlsl's
// per-tile target enumeration (same dispatch grid, BlockSmoothingTilesPerAxis^3)
// but simpler: only 1 slot per target (not MAX_CANDIDATES), so one thread per
// target suffices, and it commits BOTH halves in one pass -- the scratch
// potential (NodePotentialScratch slot 0 -> NodePotential slot 0) and the
// scratch label (byte 1 of NodeCandidateLabel[node*2+0], written by
// smoothnessJacobiSyntheticCS.hlsl -> byte 0 of that same word, the "current
// label" every reader looks at). No reseed-before-sweep pass is needed here
// (unlike the multi-candidate block path) -- every tile always writes every
// one of its 16 targets every sweep, nothing is ever left stale.
#define CommitSyntheticSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "UAV(u6)," \
    "UAV(u7)," \
    "UAV(u8)," \
    "CBV(b0)"

RWStructuredBuffer<float> NodePotentialScratch : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);
RWStructuredBuffer<uint>  NodeCandidateLabel : register(u2);
// Volume-conservation buffers (see smoothnessJacobiSyntheticCS.hlsl) --
// same scratch->main commit pattern as NodePotential/NodePotentialScratch,
// just plain single-float copies (no packed-byte label handling needed).
RWStructuredBuffer<float> NodeSyntheticVolumeScratch : register(u3);
RWStructuredBuffer<float> NodeSyntheticVolume : register(u4);
RWStructuredBuffer<float> NodeSensitivityScratch : register(u5);
RWStructuredBuffer<float> NodeSensitivity : register(u6);
RWStructuredBuffer<float> NodeVolumeAlignmentScratch : register(u7);
RWStructuredBuffer<float> NodeVolumeAlignment : register(u8);

[RootSignature(CommitSyntheticSig)]
[numthreads(16, 1, 1)]
void commitSyntheticCS(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex)
{
    uint iTarget = tid;
    uint3 haloOriginA = gid * 2;
    uint local = iTarget & 7u;
    uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
    bool isB = iTarget >= 8u;
    uint3 pos = haloOriginA + inTilePos + 1u;
    uint targetIdx = isB ? BIdx(pos.x, pos.y, pos.z) : AIdx(pos.x, pos.y, pos.z);

    NodePotential[targetIdx * MAX_CANDIDATES + 0u] = NodePotentialScratch[targetIdx * MAX_CANDIDATES + 0u];
    NodeSyntheticVolume[targetIdx] = NodeSyntheticVolumeScratch[targetIdx];
    NodeSensitivity[targetIdx] = NodeSensitivityScratch[targetIdx];
    NodeVolumeAlignment[targetIdx] = NodeVolumeAlignmentScratch[targetIdx];

    uint word0 = NodeCandidateLabel[targetIdx * 2u + 0u];
    uint scratchLabel = (word0 >> 8u) & 0xFFu;
    NodeCandidateLabel[targetIdx * 2u + 0u] = (word0 & 0xFFFFFF00u) | scratchLabel;
}
