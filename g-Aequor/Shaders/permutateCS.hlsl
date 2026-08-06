// Reads the permutation table computed by sortCS (perm[i] = sorted destination
// of particle i) and scatters all particle fields to the corresponding sorted
// positions -- g-Aequor's field set (5 double-buffered fields: Position,
// Normal, TensorDiag, TensorOffdiag, Label), not pbf's 7.
//
// In: perm, position, normal, tensorDiag, tensorOffdiag, label
// Out: sortedPosition, sortedNormal, sortedTensorDiag, sortedTensorOffdiag, sortedLabel

#define GatherRootSig "CBV(b0), DescriptorTable(UAV(u0, numDescriptors = 11))"

#include "AequorCb.hlsli"

RWStructuredBuffer<float3> position       : register(u0);
RWStructuredBuffer<float3> normal         : register(u1);
RWStructuredBuffer<float3> tensorDiag     : register(u2);
RWStructuredBuffer<float3> tensorOffdiag  : register(u3);
RWStructuredBuffer<uint>   label          : register(u4);

RWStructuredBuffer<float3> sortedPosition       : register(u5);
RWStructuredBuffer<float3> sortedNormal         : register(u6);
RWStructuredBuffer<float3> sortedTensorDiag     : register(u7);
RWStructuredBuffer<float3> sortedTensorOffdiag  : register(u8);
RWStructuredBuffer<uint>   sortedLabel          : register(u9);

RWStructuredBuffer<uint> perm : register(u10);

[RootSignature(GatherRootSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles)
        return;

    uint dest = perm[i];
    sortedPosition[dest]      = position[i];
    sortedNormal[dest]        = normal[i];
    sortedTensorDiag[dest]    = tensorDiag[i];
    sortedTensorOffdiag[dest] = tensorOffdiag[i];
    sortedLabel[dest]         = label[i];
}
