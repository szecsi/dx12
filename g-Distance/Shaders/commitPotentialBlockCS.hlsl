#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// Scratch -> main copy for the block-smoothing path ONLY (see
// smoothnessJacobiBlockCS.hlsl). commitPotentialCS.hlsl commits every
// (node,slot) in [0, NodeCount*MAX_CANDIDATES) -- correct for
// smoothnessJacobiCS, which touches every node every sweep, but WRONG here:
// smoothnessJacobiBlockCS.hlsl only ever writes its 16 per-tile targets'
// li/lj slots into NodePotentialScratch, so every other entry holds stale
// data from a previous commit. Committing the full range would overwrite
// every untouched node with that staleness. Instead this mirrors
// smoothnessJacobiBlockCS.hlsl's own target enumeration exactly -- same
// dispatch grid (BlockSmoothingTilesPerAxis^3), same numthreads(128,1,1) =
// 16 targets * MAX_CANDIDATES(8) slots -- so it commits precisely (and only)
// the (target,slot) pairs that shader actually wrote.
#define CommitPotentialBlockSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "CBV(b0)"

RWStructuredBuffer<float> NodePotentialScratch : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);

[RootSignature(CommitPotentialBlockSig)]
[numthreads(128, 1, 1)]
void commitPotentialBlockCS(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex)
{
    uint iTarget = tid / MAX_CANDIDATES;
    uint slot = tid % MAX_CANDIDATES;

    uint3 haloOriginA = gid * 2;
    uint local = iTarget & 7u;
    uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
    bool isB = iTarget >= 8u;
    uint3 pos = haloOriginA + inTilePos + 1u;
    uint targetIdx = isB ? BIdx(pos.x, pos.y, pos.z) : AIdx(pos.x, pos.y, pos.z);

    NodePotential[targetIdx * MAX_CANDIDATES + slot] = NodePotentialScratch[targetIdx * MAX_CANDIDATES + slot];
}
