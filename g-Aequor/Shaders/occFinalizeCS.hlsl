#include "OccurrenceField.hlsli"

#define OccFinalizeSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=2, b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)"

cbuffer RootConsts : register(b0) {
    uint finalPingIndex;
    uint label;
};

RWStructuredBuffer<uint2> Occ0 : register(u0);
RWStructuredBuffer<uint2> Occ1 : register(u1);
// Persistent, GRID_DIM^3 * 4 entries -- slice [label*GRID_DIM^3, (label+1)*GRID_DIM^3)
// holds this label's converged nearest-same-label-voxel field, read by
// anchorBarrierCS.hlsl for the rest of the app's lifetime.
RWStructuredBuffer<uint2> OccurrenceSeed : register(u2);

[RootSignature(OccFinalizeSig)]
[numthreads(4, 4, 4)]
void occFinalizeCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GRID_DIM)) return;

    uint ci = cellIndex((int3)tid);
    uint2 result = (finalPingIndex == 0) ? Occ0[ci] : Occ1[ci];
    OccurrenceSeed[label * (GRID_DIM * GRID_DIM * GRID_DIM) + ci] = result;
}
