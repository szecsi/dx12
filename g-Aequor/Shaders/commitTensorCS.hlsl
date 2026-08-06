#include "AequorCb.hlsli"

#define CommitTensorSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)"

RWStructuredBuffer<float3> ScratchNormal        : register(u0);
RWStructuredBuffer<float3> ScratchTensorDiag    : register(u1);
RWStructuredBuffer<float3> ScratchTensorOffdiag : register(u2);

RWStructuredBuffer<float3> Normal        : register(u3);
RWStructuredBuffer<float3> TensorDiag    : register(u4);
RWStructuredBuffer<float3> TensorOffdiag : register(u5);

// Jacobi-safe scratch->field commit for quadricConsistencyCS's combined
// normal + curvature-tensor output.
[RootSignature(CommitTensorSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void commitTensorCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;
    Normal[i] = ScratchNormal[i];
    TensorDiag[i] = ScratchTensorDiag[i];
    TensorOffdiag[i] = ScratchTensorOffdiag[i];
}
