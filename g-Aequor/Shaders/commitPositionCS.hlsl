#include "AequorCb.hlsli"

#define CommitPositionSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)"

RWStructuredBuffer<float3> ScratchVec3 : register(u0);
RWStructuredBuffer<float3> Position    : register(u1);

// Jacobi-safe scratch->field commit, reused after each of the 3
// position-correcting constraints (surface, spacing, anchor) -- same pattern
// as pbf's positionFromScratchCS.hlsl.
[RootSignature(CommitPositionSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void commitPositionCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;
    Position[i] = ScratchVec3[i];
}
