#include "bccCommon.hlsli"

#define FinalizeSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=4))"

cbuffer RootConsts : register(b0) {
    uint finalPingIndex;
};

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gA1 : register(u1);
RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gB1 : register(u3);

// Guarantees the converged JFA result always ends up in A0/B0, regardless of
// how many ping-pong steps ran (odd/even).
[RootSignature(FinalizeSig)]
[numthreads(4, 4, 4)]
void jfaFinalizeCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;
    if (finalPingIndex != 0) {
        gA0[tid] = gA1[tid];
        gB0[tid] = gB1[tid];
    }
}
