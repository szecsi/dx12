#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// Scratch -> main copy, closing out one Jacobi sweep (mirrors g-Aequor's
// commitPositionCS.hlsl / pbf's positionFromScratchCS.hlsl).
#define CommitPotentialSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "CBV(b0)"

RWStructuredBuffer<float> NodePotentialScratch : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);

[RootSignature(CommitPotentialSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void commitPotentialCS(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x;
    if (idx >= NodeCount * MAX_CANDIDATES) return;
    NodePotential[idx] = NodePotentialScratch[idx];
}
