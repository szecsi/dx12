#include "AequorCb.hlsli"

#define CopyParticlesSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "UAV(u6)," \
    "UAV(u7)," \
    "UAV(u8)," \
    "UAV(u9)"

RWStructuredBuffer<float3> SrcPosition       : register(u0);
RWStructuredBuffer<float3> SrcNormal         : register(u1);
RWStructuredBuffer<float3> SrcTensorDiag     : register(u2);
RWStructuredBuffer<float3> SrcTensorOffdiag  : register(u3);
RWStructuredBuffer<uint>   SrcLabel          : register(u4);

RWStructuredBuffer<float3> DstPosition       : register(u5);
RWStructuredBuffer<float3> DstNormal         : register(u6);
RWStructuredBuffer<float3> DstTensorDiag     : register(u7);
RWStructuredBuffer<float3> DstTensorOffdiag  : register(u8);
RWStructuredBuffer<uint>   DstLabel          : register(u9);

// Copies SpatialGrid::Build()'s sorted output (its "back" buffers) into a
// permanently-fixed "current state" buffer set (always the same physical
// resources, bound at a fixed set of GPU addresses every dispatch and every
// render) -- used instead of DoubleBufferGpuBuffer::flip() so that no code
// outside SpatialGrid's own internal dispatches (which still need front/back
// for the sort itself) ever has to reason about which physical buffer is
// "current" at a given moment.
[RootSignature(CopyParticlesSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void copyParticlesCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;

    DstPosition[i] = SrcPosition[i];
    DstNormal[i] = SrcNormal[i];
    DstTensorDiag[i] = SrcTensorDiag[i];
    DstTensorOffdiag[i] = SrcTensorOffdiag[i];
    DstLabel[i] = SrcLabel[i];
}
