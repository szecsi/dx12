#include "OccurrenceField.hlsli"

#define OccStepSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=2, b0)," \
    "UAV(u0)," \
    "UAV(u1)"

cbuffer RootConsts : register(b0) {
    uint stepSize;
    uint pingIndex;
};

RWStructuredBuffer<uint2> Occ0 : register(u0);
RWStructuredBuffer<uint2> Occ1 : register(u1);

// One Jump Flood propagation step, single grid (no cross-sublattice term,
// unlike g-BCC's DoStep -- there's only one lattice here). Takes src/dst as
// explicit parameters (rather than a ternary picking between Occ0/Occ1
// inside the body) -- HLSL doesn't allow a resource-typed local to be
// dynamically selected between two distinct globals, only passed as a
// statically-resolved parameter at each call site.
void DoStep(RWStructuredBuffer<uint2> src, RWStructuredBuffer<uint2> dst, uint3 tid, uint stepSize)
{
    float3 selfPos = CellCenterPos((int3)tid);
    uint ci = cellIndex((int3)tid);

    uint2 own = src[ci];
    uint  bestSeed = own.x;
    float bestDist = (own.x != SENTINEL_LABEL) ? asfloat(own.y) : 1.0e30;

    for (uint i = 0; i < 26; i++) {
        int3 nIdx = (int3)tid + SameLatticeOffsets[i] * (int)stepSize;
        if (!InGridBounds(nIdx)) continue;
        uint2 cand = src[cellIndex(nIdx)];
        if (cand.x == SENTINEL_LABEL) continue;
        float d = distance(CellSeedWorldPos(cand.x), selfPos);
        if (d < bestDist) { bestDist = d; bestSeed = cand.x; }
    }

    dst[ci] = uint2(bestSeed, asuint(bestDist));
}

[RootSignature(OccStepSig)]
[numthreads(4, 4, 4)]
void occStepCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GRID_DIM)) return;

    if (pingIndex == 0) DoStep(Occ0, Occ1, tid, stepSize);
    else                 DoStep(Occ1, Occ0, tid, stepSize);
}
