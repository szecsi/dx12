#include "bccCommon.hlsli"

#define SeedSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=4))"

cbuffer RootConsts : register(b0) {
    uint domainIsB;
};

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gA1 : register(u1);
RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gB1 : register(u3);

// Marks lattice points adjacent (across the BCC nearest-neighbor links) to a
// differently-labeled point on the other sublattice as JFA seeds: distance 0,
// own label, and the label found across the interface. Must run after both
// sublattices' torusInitCS dispatches have completed.
[RootSignature(SeedSig)]
[numthreads(4, 4, 4)]
void jfaSeedCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    uint ownLabel = (domainIsB != 0) ? gB0[tid].x : gA0[tid].x;

    uint otherLabel = SENTINEL_LABEL;
    for (uint i = 0; i < 8; i++) {
        int3 nIdx = (int3)tid + ((domainIsB != 0) ? CrossOffsetsBtoA[i] : CrossOffsetsAtoB[i]);
        if (!InBounds(nIdx)) continue;
        uint nLabel = (domainIsB != 0) ? gA0[nIdx].x : gB0[nIdx].x;
        if (nLabel != ownLabel) {
            otherLabel = nLabel;
            break;
        }
    }

    if (otherLabel != SENTINEL_LABEL) {
        uint4 texel = uint4(ownLabel, otherLabel, PackSeed(domainIsB != 0, tid), asuint(0.0));
        if (domainIsB != 0)
            gB0[tid] = texel;
        else
            gA0[tid] = texel;
    }
}
