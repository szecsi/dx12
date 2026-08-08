#include "DistanceLattice.hlsli"

// Outer-round MTV step 1 of 3: for every node F, compare its CURRENT winning
// label against NodePrevLabel[F] (recorded at the end of the previous
// round). If it changed, F "flipped" -- gather its own incident-tet
// neighbors' current labels to count how many still share its OLD label
// (countOld) and how many already share its NEW label (countNew), then
// record per-neighbor-share amounts (MTV[F]/count) for mtvDiffuseCS.hlsl
// (step 2) to apply. This is a pure GATHER (F only reads its own
// neighborhood, never writes to another node) so no atomics are needed --
// consistent with the rest of this solver.
//
// Rationale for the whole flip-share mechanism: MTV only diffuses between
// same-label neighbors (mtvDiffuseCS.hlsl), so a node that just changed
// label is momentarily "stranded" -- its accumulated MTV has no diffusion
// partners under its new label, and its old same-label neighbors still
// think they're due to receive diffusion contributions from it. Per the
// user-confirmed design: rather than a full historical ledger, instantly
// redistribute F's CURRENT MTV (unchanged in value, F itself is never
// touched by this mechanism) in equal shares added to its OLD-label
// neighbors (the group F is leaving absorbs back what F had accumulated as
// a member, so that group's total doesn't shrink just because a member
// departed) and subtracted from its NEW-label neighbors (a small
// compensating draw, since F arrives already carrying its own value) --
// self-heals over subsequent rounds if imprecise, since diffusion runs
// every round regardless. mtvDiffuseCS.hlsl applies the actual +/-; this
// file only computes the per-neighbor share magnitudes (MTV[F]/count).
#define MtvFlipDetectSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "UAV(u6)," \
    "UAV(u7)," \
    "UAV(u8)," \
    "UAV(u9)"

RWStructuredBuffer<uint4>  Tets : register(u0);
RWStructuredBuffer<uint>   NodeIncidentCount : register(u1);
RWStructuredBuffer<uint>   NodeIncidentTets : register(u2);
RWStructuredBuffer<uint>   NodeCandidateLabel : register(u3);
RWStructuredBuffer<float>  NodePotential : register(u4);
RWStructuredBuffer<uint>   NodePrevLabel : register(u5);
RWStructuredBuffer<float>  NodeMTV : register(u6);
RWStructuredBuffer<uint>   NodeFlipFlag : register(u7);
RWStructuredBuffer<float>  NodeFlipShareOld : register(u8);
RWStructuredBuffer<float>  NodeFlipShareNew : register(u9);

uint TopLabelOf(uint node)
{
    uint bestLabel = SENTINEL_LABEL;
    float bestPot = -1.0e30;
    for (uint s = 0; s < 8; s++) {
        uint l = NodeCandidateLabel[node * 8 + s];
        if (l == SENTINEL_LABEL) continue;
        float p = NodePotential[node * 8 + s];
        if (p > bestPot) { bestPot = p; bestLabel = l; }
    }
    return bestLabel;
}

[RootSignature(MtvFlipDetectSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void mtvFlipDetectCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;

    uint newLabel = TopLabelOf(node);
    uint oldLabel = NodePrevLabel[node];

    if (newLabel == oldLabel) {
        NodeFlipFlag[node] = 0;
        NodeFlipShareOld[node] = 0.0;
        NodeFlipShareNew[node] = 0.0;
        return;
    }

    uint countOld = 0, countNew = 0;
    uint incCount = min(NodeIncidentCount[node], MAX_INCIDENT_TETS);
    for (uint e = 0; e < incCount; e++) {
        uint tetIdx = NodeIncidentTets[node * MAX_INCIDENT_TETS + e];
        uint4 t = Tets[tetIdx];
        uint verts[4] = { t.x, t.y, t.z, t.w };
        for (uint c = 0; c < 4; c++) {
            uint other = verts[c];
            if (other == node) continue;
            uint otherLabel = TopLabelOf(other);
            if (otherLabel == oldLabel) countOld++;
            if (otherLabel == newLabel) countNew++;
        }
    }

    float mtv = NodeMTV[node];
    NodeFlipFlag[node] = 1;
    NodeFlipShareOld[node] = mtv / (float)max(countOld, 1u);
    NodeFlipShareNew[node] = mtv / (float)max(countNew, 1u);
}
