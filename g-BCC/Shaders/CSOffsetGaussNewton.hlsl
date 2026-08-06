#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"

#define OffsetGaussNewtonSig "RootFlags(0)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "SRV(t0)," \
    "SRV(t1)," \
    "SRV(t2)," \
    "UAV(u0)"

// Jump-flood-style same-lattice neighbor reach for this iteration -- see
// BccApp::quadricNeighborStep. FootDistanceWeight is what makes reaching
// further than one lattice cell safe: a same-label "neighbor" found this way
// that turns out to not be plausibly the same local patch just gets zero
// weight, rather than needing lattice adjacency to guarantee relevance.
cbuffer RootConsts : register(b1) {
    uint NeighborStep;
};

// Pass 1a: current state, immutable original offsets, and the fixed
// (label -> slot) map every node needs to find its same-label neighbors.
StructuredBuffer<PackedNode4> CurrentNodes : register(t0);
StructuredBuffer<PackedNode4> OriginalNodes : register(t1);
StructuredBuffer<SparseNodeSeeds> Seeds : register(t2);
RWStructuredBuffer<PackedNode4> FootUpdatedNodes : register(u0);

// Local 1D Gauss-Newton update of each populated slot's signed offset along
// its OWN (fixed for this pass -- see CSNormalRotation.hlsl) normal: pulls
// it toward agreement with same-label neighbors' quadric surfaces, toward
// this same node's OTHER populated slots' footpoints (see the cross-slot
// term below -- otherwise two labels sharing a boundary relax as fully
// independent fields with nothing forcing them to the same physical
// location), while anchoring near its original seed offset and lightly
// regularizing against same-label neighboring footpoints. Splitting the old
// single 3D solve into this scalar pass plus CSNormalRotation.hlsl's 2D
// tangent-rotation pass is what lets offset cross zero smoothly (no
// discontinuity: it's just a scalar sign, not a re-derived direction) while
// still letting the footpoint move tangentially over several iterations,
// via the normal rotating.
[RootSignature(OffsetGaussNewtonSig)]
[numthreads(THREADS_X, 1, 1)]
void CSOffsetGaussNewton(uint3 dispatchThreadID : SV_DispatchThreadID)
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
    PackedNode4 selfPacked = CurrentNodes[index];
    PackedNode4 originalPacked = OriginalNodes[index];
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
        float3 p = nodeX + self.offset * n;

        float H = FootDataWeight + MinSystemDiagonal;
        float b = FootDataWeight * (self.offset - original.offset);

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
            LabelSlotState nb = DecodeSlot(CurrentNodes[j], nbSlot);
            float3 pj = SlotFootpoint(nbX, nb);

            float e;
            float3 g3;
            EvaluateSlotQuadric(nb, pj, p, e, g3);
            float gs = dot(g3, n);
            float wd = FootDistanceWeight(p, pj);
            float w = NeighborWeight(self.confidence, nb.confidence, n, nb.normal, e) * wd;

            H += w * gs * gs;
            b += w * e * gs;

            // Mild same-label Euclidean regularization, projected onto n.
            H += FootSmoothWeight * wd;
            b += FootSmoothWeight * wd * dot(p - pj, n);
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
            LabelSlotState nb = DecodeSlot(CurrentNodes[j], nbSlot);
            float3 pj = SlotFootpoint(nbX, nb);

            float e;
            float3 g3;
            EvaluateSlotQuadric(nb, pj, p, e, g3);
            float gs = dot(g3, n);
            float wd = FootDistanceWeight(p, pj);
            float w = NeighborWeight(self.confidence, nb.confidence, n, nb.normal, e) * wd;

            H += w * gs * gs;
            b += w * e * gs;

            H += FootSmoothWeight * wd;
            b += FootSmoothWeight * wd * dot(p - pj, n);
        }

        // Same-node, cross-slot consistency: this node's OTHER populated
        // slots (e.g. its label-0 and label-1 slots, if this is a boundary
        // node tracking both) should describe the same nearby shared
        // interface, so pull this slot's footpoint toward theirs too --
        // without this, two labels sharing a boundary each relax as fully
        // independent fields with nothing forcing their zero-crossings to
        // coincide. Same math as the same-label smoothing term above, just
        // targeting another slot on THIS node instead of a neighbor node.
        [unroll]
        for (uint s2 = 0; s2 < SparseLabelCount; s2++)
        {
            if (s2 == slot)
                continue;
            uint label2 = GetSlotLabel(selfSeeds, s2);
            if (label2 == SENTINEL_LABEL)
                continue;

            LabelSlotState other = DecodeSlot(selfPacked, s2);
            float3 pOther = nodeX + other.offset * other.normal;
            float w = CrossLabelWeight * self.confidence * other.confidence * FootDistanceWeight(p, pOther);

            H += w;
            b += w * dot(p - pOther, n);
        }

        float delta = (abs(H) > 1.0e-12) ? (-b / H) : 0.0;
        if (MaxFootStep > 0.0)
            delta = clamp(delta, -MaxFootStep, MaxFootStep);

        LabelSlotState updated = self;
        updated.offset = self.offset + FootStepDamping * delta;
        EncodeSlot(outPacked, slot, updated);
    }

    FootUpdatedNodes[index] = outPacked;
}
