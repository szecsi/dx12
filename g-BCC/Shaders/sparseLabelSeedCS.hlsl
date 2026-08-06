#include "bccCommon.hlsli"

#define SparseLabelSeedSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=2, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))"

cbuffer RootConsts : register(b0) {
    uint domainIsB;
    uint targetLabel;
};

RWTexture3D<uint4> gA0    : register(u0);
RWTexture3D<uint4> gA1    : register(u1);
RWTexture3D<uint4> gB0    : register(u2);
RWTexture3D<uint4> gB1    : register(u3);
RWTexture3D<uint4> gAFoot : register(u4);
RWTexture3D<uint4> gBFoot : register(u5);

// Seeds a single label's BOUNDARY-distance JFA: this voxel is a seed
// (distance 0) only if it sits directly on the targetLabel/not-targetLabel
// boundary (at least one of its cross-lattice neighbors disagrees on isK) --
// exactly mirroring jfaSeedCS.hlsl's own/otherLabel boundary detection, but
// for the binary partition "is this voxel labeled targetLabel, or not".
// Seeding every targetLabel VOXEL (not just its boundary) would make the
// propagated distance 0 throughout that label's entire interior, useless as
// a "how deep into this territory" signal -- boundary-only seeding is what
// makes the propagated distance a genuine (unsigned) distance-to-interface,
// just like the ordinary single-label JFA already relies on.
//
// .x carries isK (1/0) for THIS voxel, preserved unchanged through every
// jfaStepCS propagation step (same passthrough jfaSeedCS/jfaStepCS already
// rely on for ownLabel), so sparseLabelHarvestCS.hlsl can recover the correct
// sign afterward: positive on the targetLabel side, negative on the other.
[RootSignature(SparseLabelSeedSig)]
[numthreads(4, 4, 4)]
void sparseLabelSeedCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    uint ownLabel = (domainIsB != 0) ? gBFoot[tid].x : gAFoot[tid].x;
    bool isK = (ownLabel == targetLabel);

    bool boundary = false;
    for (uint i = 0; i < 8; i++) {
        int3 nIdx = (int3)tid + ((domainIsB != 0) ? CrossOffsetsBtoA[i] : CrossOffsetsAtoB[i]);
        if (!InBounds(nIdx)) continue;
        uint nLabel = (domainIsB != 0) ? gAFoot[nIdx].x : gBFoot[nIdx].x;
        if ((nLabel == targetLabel) != isK) { boundary = true; break; }
    }

    uint4 texel = boundary
        ? uint4(isK ? 1u : 0u, targetLabel, PackSeed(domainIsB != 0, tid), asuint(0.0))
        : uint4(isK ? 1u : 0u, SENTINEL_LABEL, 0, asuint(1.0e30));

    if (domainIsB != 0) gB0[tid] = texel;
    else                gA0[tid] = texel;
}
