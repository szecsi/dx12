#include "DistanceFrameCb.hlsli"
#include "DistanceLattice.hlsli"

// Billboard point-sprite per node (A + B together), colored by each node's
// CURRENT winning label (argmax over its 8 candidate slots) AND its
// sublattice (nodePointPS.hlsl uses a separate red/green palette for A vs
// yellow/blue for B, so misclassified nodes -- an A-node showing the "wrong"
// side of its own red/green pair, or a B-node sitting in an otherwise
// uniform yellow/blue region -- are easy to spot against their neighbors).
// Sprite size also scales with the winning candidate's raw potential value
// (relative to raymarchParams.z, a GUI-tunable reference scale) so a node
// that's only barely winning -- near-tied against its runner-up -- renders
// visibly smaller than a confidently-decided one, at a glance.
// Optionally (pointParams.w != 0, "Hide Uniform-Neighborhood Nodes") skips
// any node whose structural neighbors (same-sublattice 26-neighborhood for
// A, own cube's 8 A-corners for B -- same neighbor definition
// buildCandidatesCS.hlsl uses to build candidate sets) all currently share
// its own winning label, leaving only nodes near an actual label boundary
// (or genuinely isolated misclassifications) visible.
#define NodePointSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)"

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    nointerpolation uint label : TEXCOORD1;
    nointerpolation uint isB   : TEXCOORD2;
    nointerpolation float fade : TEXCOORD3;
};

static const float2 QuadCorners[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };

uint TopLabelAndPot(uint node, out float bestPot)
{
    uint bestLabel = SENTINEL_LABEL;
    bestPot = -1.0e30;
    for (uint s = 0; s < 8; s++) {
        uint l = NodeCandidateLabel[node * 8 + s];
        if (l == SENTINEL_LABEL) continue;
        float p = NodePotential[node * 8 + s];
        if (p > bestPot) { bestPot = p; bestLabel = l; }
    }
    return bestLabel;
}

uint TopLabelOf(uint node)
{
    float unused;
    return TopLabelAndPot(node, unused);
}

bool HasOnlySameLabelNeighbors(uint node, uint myLabel)
{
    bool isB; uint3 idx;
    DecodeNodeIndex(node, isB, idx);

    if (!isB) {
        for (uint n = 0; n < 26; n++) {
            int3 nb = int3(idx) + SameLatticeOffsets[n];
            if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
            uint neighbor = AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z);
            if (TopLabelOf(neighbor) != myLabel) return false;
        }
    } else {
        for (uint dz = 0; dz < 2; dz++)
            for (uint dy = 0; dy < 2; dy++)
                for (uint dx = 0; dx < 2; dx++) {
                    uint neighbor = AIdx(idx.x + dx, idx.y + dy, idx.z + dz);
                    if (TopLabelOf(neighbor) != myLabel) return false;
                }
    }
    return true;
}

[RootSignature(NodePointSig)]
VsOut nodePointVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;
    uint node = iid;

    float bestPot;
    uint bestLabel = TopLabelAndPot(node, bestPot);
    o.label = bestLabel;
    o.isB = (node >= ACount) ? 1 : 0;

    bool hideUniform = pointParams.w > 0.5;
    bool hide = (bestLabel == SENTINEL_LABEL) ||
                (hideUniform && HasOnlySameLabelNeighbors(node, bestLabel));

    if (hide) {
        o.pos = float4(0, 0, 0, 0);
        o.uv = float2(0, 0);
        o.fade = 0.0;
        return o;
    }

    float3 worldPos = NodeWorldPos(node);

    // Distant nodes otherwise all render at the same fixed screen-space
    // size and fully opaque, so with many nodes at very different depths
    // the far ones visually clutter/obscure whatever is actually near the
    // camera. Nodes within nodeFadeParams.y (normalized camera distance)
    // stay fully solid; beyond that, fade exponentially at rate
    // nodeFadeParams.x -- a pure exponential from distance 0 faded out
    // things that were still close enough to matter, hence the floor.
    float camDist = length(worldPos - cameraPos.xyz);
    float normDist = camDist / (GridRes * CELL_SIZE);
    float beyondFloor = max(0.0, normDist - nodeFadeParams.y);
    o.fade = exp(-nodeFadeParams.x * beyondFloor);

    float4 clipCenter = mul(float4(worldPos, 1), viewProjTransform);
    float2 corner = QuadCorners[vid];
    float potentialRef = max(raymarchParams.z, 1.0e-4);
    float sizeMul = 0.35 + 0.65 * saturate(bestPot / potentialRef);
    float radiusPx = pointParams.z * sizeMul;
    float2 offsetNdc = corner * radiusPx * (2.0 / pointParams.xy);

    o.pos = clipCenter;
    o.pos.xy += offsetNdc * clipCenter.w;
    o.uv = corner;
    return o;
}
