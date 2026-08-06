#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"
#include "SparseLabelSeed.hlsli"

#define SparseLabelHarvestSig "RootFlags(0)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=2, b1)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))," \
    "UAV(u6)"

cbuffer RootConsts : register(b1) {
    uint domainIsB;
    uint targetLabel;
};

RWTexture3D<uint4> gA0    : register(u0);
RWTexture3D<uint4> gA1    : register(u1);
RWTexture3D<uint4> gB0    : register(u2);
RWTexture3D<uint4> gB1    : register(u3);
RWTexture3D<uint4> gAFoot : register(u4);
RWTexture3D<uint4> gBFoot : register(u5);

RWStructuredBuffer<SparseNodeSeeds> Seeds : register(u6);

// After sparseLabelSeedCS + jfaStepCS + jfaFinalizeCS have converged
// targetLabel's boundary-distance field into A0/B0, inserts (targetLabel,
// signedDist, seed) into this node's 4-slot record, keeping it sorted
// ascending by |dist| -- an unrolled insertion sort (no dynamic vector
// indexing, matching QuadricFootField.hlsli's DecodeSlot/EncodeSlot style).
// signedDist is positive when this voxel is on the targetLabel side of its
// own boundary, negative otherwise (see sparseLabelSeedCS.hlsl's isK
// passthrough) -- a targetLabel with no voxels anywhere in the scene has no
// boundary at all, so .y stays SENTINEL_LABEL forever and this never
// displaces anything, correctly leaving that slot unused.
[RootSignature(SparseLabelHarvestSig)]
[numthreads(4, 4, 4)]
void sparseLabelHarvestCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    uint4 texel = (domainIsB != 0) ? gB0[tid] : gA0[tid];
    bool found = texel.y != SENTINEL_LABEL;
    bool isK = texel.x != 0;
    float dist = found ? asfloat(texel.w) : 1.0e30;
    float signedDist = isK ? dist : -dist;
    uint  seed = texel.z;

    uint index = EncodeIndex(int3(tid), domainIsB);
    SparseNodeSeeds s = Seeds[index];

    float d0 = s.dists.x, d1 = s.dists.y, d2 = s.dists.z, d3 = s.dists.w;
    uint  l0 = s.labels.x, l1 = s.labels.y, l2 = s.labels.z, l3 = s.labels.w;
    uint  sd0 = s.seeds.x, sd1 = s.seeds.y, sd2 = s.seeds.z, sd3 = s.seeds.w;

    float aNew = abs(signedDist);
    if (found && aNew < abs(d3)) {
        if (aNew < abs(d0)) {
            d3 = d2; l3 = l2; sd3 = sd2;
            d2 = d1; l2 = l1; sd2 = sd1;
            d1 = d0; l1 = l0; sd1 = sd0;
            d0 = signedDist; l0 = targetLabel; sd0 = seed;
        } else if (aNew < abs(d1)) {
            d3 = d2; l3 = l2; sd3 = sd2;
            d2 = d1; l2 = l1; sd2 = sd1;
            d1 = signedDist; l1 = targetLabel; sd1 = seed;
        } else if (aNew < abs(d2)) {
            d3 = d2; l3 = l2; sd3 = sd2;
            d2 = signedDist; l2 = targetLabel; sd2 = seed;
        } else {
            d3 = signedDist; l3 = targetLabel; sd3 = seed;
        }
    }

    s.dists  = float4(d0, d1, d2, d3);
    s.labels = uint4(l0, l1, l2, l3);
    s.seeds  = uint4(sd0, sd1, sd2, sd3);
    Seeds[index] = s;
}
