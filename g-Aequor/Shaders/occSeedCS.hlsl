#include "OccurrenceField.hlsli"

// RasterLabel bound as UAV (not SRV) -- permanently UNORDERED_ACCESS from
// creation through every Reinit, so no state transition is ever needed for
// it between rasterLabelCS's write and this shader's read.
#define OccSeedSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "UAV(u0)," \
    "UAV(u1)"

cbuffer RootConsts : register(b0) {
    uint targetLabel;
};

RWStructuredBuffer<uint> RasterLabel : register(u0);
RWStructuredBuffer<uint2> Occ0 : register(u1); // x = packed seed cell, y = asuint(dist), SENTINEL_LABEL/1e30 if unseeded

// Occurrence seeding (unlike g-BCC's boundary-only jfaSeedCS): every voxel
// whose ground-truth label == targetLabel becomes a distance-0 seed of
// itself -- not just voxels adjacent to a different label. Must run once per
// label before occStepCS propagates.
[RootSignature(OccSeedSig)]
[numthreads(4, 4, 4)]
void occSeedCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GRID_DIM)) return;

    uint ci = cellIndex((int3)tid);
    bool isSeed = RasterLabel[ci] == targetLabel;

    Occ0[ci] = isSeed
        ? uint2(EncodeCellSeed(tid), asuint(0.0))
        : uint2(SENTINEL_LABEL, asuint(1.0e30));
}
