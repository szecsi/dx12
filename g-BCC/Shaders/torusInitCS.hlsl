#include "TorusListCb.hlsli"
#include "TorusSdf.hlsli"
#include "bccCommon.hlsli"

#define InitSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=4))," \
    "CBV(b1)"

cbuffer RootConsts : register(b0) {
    uint domainIsB;
};

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gA1 : register(u1);
RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gB1 : register(u3);

[RootSignature(InitSig)]
[numthreads(4, 4, 4)]
void torusInitCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    float3 pos = (domainIsB != 0) ? BPos((int3)tid) : APos((int3)tid);

    // Last matching shape in array order wins; label 0 = background.
    uint label = 0;
    for (uint i = 0; i < nTorii; i++) {
        if (ShapeSd(pos, torii[i]) <= 0.0)
            label = torii[i].label;
    }

    uint4 texel = uint4(label, SENTINEL_LABEL, 0, 0);
    if (domainIsB != 0)
        gB0[tid] = texel;
    else
        gA0[tid] = texel;
}
