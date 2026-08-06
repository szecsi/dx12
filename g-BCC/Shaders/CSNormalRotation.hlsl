#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"

#define NormalRotationSig "RootFlags(0)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "SRV(t0)," \
    "SRV(t2)," \
    "UAV(u1)"

// Jump-flood-style same-lattice neighbor reach for this iteration -- see
// CSOffsetGaussNewton.hlsl's matching comment / BccApp::quadricNeighborStep.
cbuffer RootConsts : register(b1) {
    uint NeighborStep;
};

// Pass 1b: runs right after CSOffsetGaussNewton.hlsl, reading its freshly
// updated offsets (normals still untouched) plus the fixed (label -> slot)
// map.
StructuredBuffer<PackedNode4> FootUpdatedNodes : register(t0);
StructuredBuffer<SparseNodeSeeds> Seeds : register(t2);
RWStructuredBuffer<PackedNode4> NormalUpdatedNodes : register(u1);

// Local 2D Gauss-Newton update of each populated slot's persistent normal,
// as a small rotation within its own tangent plane (du, dv along an
// arbitrary orthonormal t1/t2 basis) -- offset is held fixed here (just
// updated by CSOffsetGaussNewton.hlsl). Rotating the normal by (du, dv)
// moves the footpoint by offset*(du*t1 + dv*t2), since footpoint =
// nodeX + offset*normal -- that's the whole reason this needs to be its own
// small least-squares problem rather than reusing the offset pass's scalar
// system: the same neighbor-agreement energy, differentiated with respect
// to a different (now 2D, tangential) parametrization. No anchor to the
// original seed normal here, deliberately -- letting same-label neighbor
// consensus alone determine orientation is the point of splitting this out;
// FootDataWeight already anchors magnitude via the offset pass.
[RootSignature(NormalRotationSig)]
[numthreads(THREADS_X, 1, 1)]
void CSNormalRotation(uint3 dispatchThreadID : SV_DispatchThreadID)
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
    PackedNode4 selfPacked = FootUpdatedNodes[index];
    PackedNode4 outPacked = selfPacked;

    [unroll]
    for (uint slot = 0; slot < SparseLabelCount; slot++)
    {
        uint label = GetSlotLabel(selfSeeds, slot);
        if (label == SENTINEL_LABEL)
            continue;

        LabelSlotState self = DecodeSlot(selfPacked, slot);
        float3 n = self.normal;
        float o = self.offset;
        float3 p = nodeX + o * n;

        float3 t1, t2;
        TangentBasis(n, t1, t2);

        float2x2 H = float2x2(MinSystemDiagonal, 0.0, 0.0, MinSystemDiagonal);
        float2 b = float2(0.0, 0.0);

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
            LabelSlotState nb = DecodeSlot(FootUpdatedNodes[j], nbSlot);
            float3 pj = SlotFootpoint(nbX, nb);

            float e;
            float3 g3;
            EvaluateSlotQuadric(nb, pj, p, e, g3);
            float wd = FootDistanceWeight(p, pj);
            float w = NeighborWeight(self.confidence, nb.confidence, n, nb.normal, e) * wd;

            float gu = o * dot(g3, t1);
            float gv = o * dot(g3, t2);
            H[0][0] += w * gu * gu; H[0][1] += w * gu * gv;
            H[1][0] += w * gu * gv; H[1][1] += w * gv * gv;
            b += w * e * float2(gu, gv);

            float3 r = p - pj;
            float su = o * dot(t1, r);
            float sv = o * dot(t2, r);
            H[0][0] += FootSmoothWeight * wd * o * o; H[1][1] += FootSmoothWeight * wd * o * o;
            b += FootSmoothWeight * wd * float2(su, sv);
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
            LabelSlotState nb = DecodeSlot(FootUpdatedNodes[j], nbSlot);
            float3 pj = SlotFootpoint(nbX, nb);

            float e;
            float3 g3;
            EvaluateSlotQuadric(nb, pj, p, e, g3);
            float wd = FootDistanceWeight(p, pj);
            float w = NeighborWeight(self.confidence, nb.confidence, n, nb.normal, e) * wd;

            float gu = o * dot(g3, t1);
            float gv = o * dot(g3, t2);
            H[0][0] += w * gu * gu; H[0][1] += w * gu * gv;
            H[1][0] += w * gu * gv; H[1][1] += w * gv * gv;
            b += w * e * float2(gu, gv);

            float3 r = p - pj;
            float su = o * dot(t1, r);
            float sv = o * dot(t2, r);
            H[0][0] += FootSmoothWeight * wd * o * o; H[1][1] += FootSmoothWeight * wd * o * o;
            b += FootSmoothWeight * wd * float2(su, sv);
        }

        // Same-node, cross-slot consistency -- see the matching comment in
        // CSOffsetGaussNewton.hlsl. Pulls this slot's footpoint toward this
        // node's OTHER populated slots' footpoints too, via a rotation of
        // its own normal (offset is held fixed in this pass).
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

            float3 r = p - pOther;
            float su = o * dot(t1, r);
            float sv = o * dot(t2, r);
            H[0][0] += w * o * o; H[1][1] += w * o * o;
            b += w * float2(su, sv);
        }

        float2 delta;
        bool solved = SolveSPD2(H, -b, delta);
        if (!solved)
            delta = float2(0.0, 0.0);

        float deltaLen = length(delta);
        if (MaxFootStep > 0.0 && deltaLen > MaxFootStep)
            delta *= MaxFootStep / deltaLen;

        float3 deltaN = FootStepDamping * (delta.x * t1 + delta.y * t2);

        LabelSlotState updated = self;
        updated.normal = normalize(n + deltaN);
        EncodeSlot(outPacked, slot, updated);
    }

    NormalUpdatedNodes[index] = outPacked;
}
