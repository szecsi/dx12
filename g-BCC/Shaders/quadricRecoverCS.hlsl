#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"

#define QuadricRecoverSig "RootFlags(0)," \
    "CBV(b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))," \
    "SRV(t0)," \
    "SRV(t1)"

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gAFoot : register(u4);
RWTexture3D<uint4> gBFoot : register(u5);

StructuredBuffer<PackedNode4> FinalNodes : register(t0);
StructuredBuffer<SparseNodeSeeds> Seeds : register(t1);

// Converts the quadric optimization's final per-node state back into this
// project's usual grid-texture footvector representation, so
// raymarchPS.hlsl/footVectorBuildCS.hlsl need no changes to consume it --
// same strategy already used for the multiphase label-potential filter.
// The node's own/other label pair is derived fresh here from whichever two
// populated slots ended up with the largest (most confidently "inside")
// current offset -- NOT preserved from whatever the preceding ordinary JFA
// pass wrote, since this optimization's own sparse label seeding is now the
// authoritative source for which labels are even relevant at this node.
[RootSignature(QuadricRecoverSig)]
[numthreads(THREADS_X, 1, 1)]
void quadricRecoverCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    uint totalCount = 2u * LatticeNodeCount;
    if (index >= totalCount)
        return;

    uint3 c;
    uint sub;
    DecodeIndex(index, c, sub);
    float3 nodeX = NodePosition(c, sub);

    SparseNodeSeeds selfSeeds = Seeds[index];
    PackedNode4 finalPacked = FinalNodes[index];

    uint dom = SparseLabelCount;
    uint dom2 = SparseLabelCount;
    float domOffset = -1.0e30;
    float dom2Offset = -1.0e30;

    [unroll]
    for (uint slot = 0; slot < SparseLabelCount; slot++)
    {
        uint label = GetSlotLabel(selfSeeds, slot);
        if (label == SENTINEL_LABEL)
            continue;

        float offset = DecodeSlot(finalPacked, slot).offset;
        if (offset > domOffset) {
            dom2 = dom; dom2Offset = domOffset;
            dom = slot; domOffset = offset;
        } else if (offset > dom2Offset) {
            dom2 = slot; dom2Offset = offset;
        }
    }

    if (dom == SparseLabelCount) {
        // No label boundary anywhere in the whole scene -- leave whatever
        // the preceding ordinary JFA pass wrote into gA0/gB0 untouched, and
        // just mark "no exact foot point" here, same as the analogous
        // early-out every other init mode uses.
        uint4 emptyFoot = uint4(asuint(nodeX.x), asuint(nodeX.y), asuint(nodeX.z), 0);
        if (sub != 0) gBFoot[c] = emptyFoot; else gAFoot[c] = emptyFoot;
        return;
    }

    LabelSlotState domSlot = DecodeSlot(finalPacked, dom);
    float3 p = SlotFootpoint(nodeX, domSlot);
    uint ownLabel = GetSlotLabel(selfSeeds, dom);
    uint otherLabel = (dom2 < SparseLabelCount) ? GetSlotLabel(selfSeeds, dom2) : SENTINEL_LABEL;

    // Quantize the recovered (continuous) foot point to its nearest lattice
    // site -- same convention analyticInitCS.hlsl/labelPotentialRecoverCS.hlsl
    // use -- so EstimateFilteredNormal's SeedWorldPos(packedSeed) direction
    // lookup keeps working for this init mode too.
    int3 footA = clamp((int3)round(p / CellSize), 0, (int)GridRes - 1);
    int3 footB = clamp((int3)round(p / CellSize - 0.5), 0, (int)GridRes - 1);
    bool footIsB = distance(BPos(footB), p) < distance(APos(footA), p);
    uint packedSeed = PackSeed(footIsB, (uint3)(footIsB ? footB : footA));

    uint4 outTexel = uint4(ownLabel, otherLabel, packedSeed, asuint(abs(domSlot.offset)));
    uint4 outFoot = uint4(asuint(p.x), asuint(p.y), asuint(p.z), 0);

    if (sub != 0) {
        gB0[c] = outTexel;
        gBFoot[c] = outFoot;
    } else {
        gA0[c] = outTexel;
        gAFoot[c] = outFoot;
    }
}
