#include "AequorCb.hlsli"

#define ClearParticlesSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)"

RWStructuredBuffer<float3> Position       : register(u0);
RWStructuredBuffer<float3> Normal         : register(u1);
RWStructuredBuffer<float3> TensorDiag     : register(u2);
RWStructuredBuffer<float3> TensorOffdiag  : register(u3);
RWStructuredBuffer<uint>   Label          : register(u4);
RWStructuredBuffer<uint>   Counter        : register(u5); // spawnCS's atomic slot counter -- zeroed here too

// Resets every one of the MAX_PARTICLES slots to inert before spawnCS claims
// however many it actually needs -- unclaimed slots stay label =
// SENTINEL_LABEL forever and are skipped by every constraint kernel.
[RootSignature(ClearParticlesSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void clearParticlesCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i == 0) Counter[0] = 0;
    if (i >= numParticles) return;

    Position[i] = float3(0, 0, 0);
    Normal[i] = float3(0, 0, 1);
    TensorDiag[i] = float3(0, 0, 0);
    TensorOffdiag[i] = float3(0, 0, 0);
    Label[i] = SENTINEL_LABEL;
}
