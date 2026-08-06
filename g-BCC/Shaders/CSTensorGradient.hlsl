#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"

#define TensorGradientSig "RootFlags(0)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "SRV(t1)," \
    "SRV(t2)," \
    "SRV(t3)," \
    "UAV(u2)"

// Jump-flood-style same-lattice neighbor reach for this iteration -- see
// CSOffsetGaussNewton.hlsl's matching comment / BccApp::quadricNeighborStep.
cbuffer RootConsts : register(b1) {
    uint NeighborStep;
};

// Pass 3: immutable original tensors, the fixed (label -> slot) map, and the
// post-transport state (new normals, tensors rotated to follow them).
StructuredBuffer<PackedNode4> OriginalNodes : register(t1);
StructuredBuffer<SparseNodeSeeds> Seeds : register(t2);
StructuredBuffer<PackedNode4> TransportedNodes : register(t3);
RWStructuredBuffer<PackedNode4> OutputNodes : register(u2);

// Gradient update of each populated slot's A using same-label neighboring
// footpoint incidence: each slot's local quadric is fitted so it passes near
// same-label neighboring footpoints, with a mild pull back toward the
// original (zero) tensor and toward same-label neighboring tensors' values
// (world-coordinate smoothing -- no principal-frame matching needed since
// pass 2 already re-oriented every tensor to its own, honestly-consistent
// normal). Matching neighbors by label identity (via Seeds/FindSlotForLabel)
// replaces the old own/other-label texel lookup and reversed-pair gating
// entirely -- there's no "which side" ambiguity left to reconcile. Relevance
// beyond that is governed by FootDistanceWeight (see QuadricFootField.hlsli),
// not normal alignment: neighbors on opposite walls of a thin tube, or all
// the way around a small island, are supposed to have very different normals
// AND still contribute here -- that diversity is exactly what lets this fit
// recover a cylinder/sphere-shaped curvature instead of a flat plane. What
// this can't (and shouldn't) fit together is two points too far apart to
// plausibly share one local quadric.
[RootSignature(TensorGradientSig)]
[numthreads(THREADS_X, 1, 1)]
void CSTensorGradient(uint3 dispatchThreadID : SV_DispatchThreadID)
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
    PackedNode4 originalPacked = OriginalNodes[index];
    PackedNode4 selfPacked = TransportedNodes[index];
    PackedNode4 outPacked = selfPacked;

    [unroll]
    for (uint slot = 0; slot < SparseLabelCount; slot++)
    {
        uint label = GetSlotLabel(selfSeeds, slot);
        if (label == SENTINEL_LABEL)
            continue;

        LabelSlotState self = DecodeSlot(selfPacked, slot);
        LabelSlotState original = DecodeSlot(originalPacked, slot);
        float3 n = self.normal;
        float3 p = SlotFootpoint(nodeX, self);

        float3x3 gradA = TensorDataWeight * (self.A - original.A);

        [unroll]
        for (uint k = 0; k < 8; k++)
        {
            uint j = BCCNeighborIndex(c, sub, k);
            if (j == INVALID_INDEX)
                continue;

            SparseNodeSeeds nbSeeds = Seeds[j];
            uint nbSlot = FindSlotForLabel(nbSeeds, label);
            if (nbSlot >= SparseLabelCount)
                continue;

            uint3 cj;
            uint sj;
            DecodeIndex(j, cj, sj);
            float3 nbX = NodePosition(cj, sj);
            LabelSlotState nb = DecodeSlot(TransportedNodes[j], nbSlot);
            float3 pj = SlotFootpoint(nbX, nb);
            float3 r = pj - p;

            float3 Ar = mul(self.A, r);
            float e = dot(n, r) + 0.5 * dot(r, Ar);
            e = clamp(e, -MaxResidual, MaxResidual);

            float wd = FootDistanceWeight(p, pj);
            float w = NeighborWeight(self.confidence, nb.confidence, n, nb.normal, e) * wd;
            gradA += 0.5 * w * e * Outer(r, r);
            gradA += TensorSmoothWeight * wd * (self.A - nb.A);
        }

        for (uint m = 0; m < 26; m++)
        {
            uint j = EncodeIndex(int3(c) + SameLatticeOffsets[m] * (int)NeighborStep, sub);
            if (j == INVALID_INDEX)
                continue;

            SparseNodeSeeds nbSeeds = Seeds[j];
            uint nbSlot = FindSlotForLabel(nbSeeds, label);
            if (nbSlot >= SparseLabelCount)
                continue;

            uint3 cj;
            uint sj;
            DecodeIndex(j, cj, sj);
            float3 nbX = NodePosition(cj, sj);
            LabelSlotState nb = DecodeSlot(TransportedNodes[j], nbSlot);
            float3 pj = SlotFootpoint(nbX, nb);
            float3 r = pj - p;

            float3 Ar = mul(self.A, r);
            float e = dot(n, r) + 0.5 * dot(r, Ar);
            e = clamp(e, -MaxResidual, MaxResidual);

            float wd = FootDistanceWeight(p, pj);
            float w = NeighborWeight(self.confidence, nb.confidence, n, nb.normal, e) * wd;
            gradA += 0.5 * w * e * Outer(r, r);
            gradA += TensorSmoothWeight * wd * (self.A - nb.A);
        }

        float3x3 candidate = self.A - TensorStep * gradA;
        candidate = ClampFrobenius(ProjectTensorToNormal(candidate, n));

        LabelSlotState updated = self;
        updated.A = candidate;
        EncodeSlot(outPacked, slot, updated);
    }

    OutputNodes[index] = outPacked;
}
