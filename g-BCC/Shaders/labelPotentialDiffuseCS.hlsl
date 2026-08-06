#include "bccCommon.hlsli"
#include "LabelPotential.hlsli"

#define LabelPotentialDiffuseSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=2, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))"

cbuffer RootConsts : register(b0) {
    uint pingIndex;
    uint relaxRateBits; // bit-reinterpreted float, see BccApp.h's dispatch loop
};

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gA1 : register(u1);
RWTexture3D<uint4> gB1 : register(u3);

// One relaxation step: for lattice point tid on B, blend its current
// potentials (from srcB) toward the average of its 8 fixed BCC-nearest
// A-neighbors' virtual potentials (derived from A0 on the fly via
// TexelToPotentials -- A0 never changes, so it needs no separate potential
// storage of its own), writing the result to dstB. Mirrors jfaStepCS.hlsl's
// DoStep/pingIndex idiom for ping-ponging between two scratch buffers
// (repurposed A1/B1 here, since JFA's own ping-pong is done by this point).
// Out-of-bounds neighbors clamp to the nearest valid lattice point (a simple
// reflective boundary) rather than being skipped -- the torii of interest
// are well inside the grid, so this only affects otherwise-irrelevant
// boundary voxels.
void DoDiffuseStep(RWTexture3D<uint4> srcB, RWTexture3D<uint4> dstB, uint3 tid, float relaxRate)
{
    float4 selfPhi = UnpackPotentials4(srcB[tid].x);

    float4 sum = float4(0, 0, 0, 0);
    for (uint j = 0; j < 8; j++) {
        int3 nIdx = clamp((int3)tid + CrossOffsetsBtoA[j], 0, (int)GridRes - 1);
        sum += TexelToPotentials(gA0[nIdx]);
    }
    float4 avgNeighbor = sum / 8.0;

    float4 newPhi = lerp(selfPhi, avgNeighbor, relaxRate);
    dstB[tid] = uint4(PackPotentials4(newPhi), 0, 0, 0);
}

[RootSignature(LabelPotentialDiffuseSig)]
[numthreads(4, 4, 4)]
void labelPotentialDiffuseCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    float relaxRate = asfloat(relaxRateBits);
    if (pingIndex == 0) DoDiffuseStep(gB1, gA1, tid, relaxRate);
    else                 DoDiffuseStep(gA1, gB1, tid, relaxRate);
}
