#include "DistanceFrameCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"
#include "LabelPalette.hlsli"

// No [RootSignature(...)] here -- declared once by footSliceVS.hlsl, reused
// for both stages of this draw (this codebase's convention).
cbuffer SliceConsts : register(b1) {
    uint  SliceAxis;    // 0 = X, 1 = Y, 2 = Z -- which world axis the plane is normal to
    float SliceCoord;   // world-space coordinate of the plane along SliceAxis
    float ColorScale;   // potential/footdist value that maps to full channel brightness
    uint  DisplayMode;  // 0 = potentials (label0/label1 heatmap, below), 1 = NodeFootDist (grayscale, A-nodes only), 2 = synthetic-field potential (label-tinted), 3 = synthetic-field volume (label-tinted)
};

// Repurposed from the original NodeFootDist/JFA debug view: now a heatmap of
// LABEL 0's potential (red channel) and LABEL 1's potential (green channel)
// -- ground-truth label indices, not candidate slot indices (a node's own
// label isn't necessarily slot 0, and which slot holds label 0/1 varies node
// to node). Falls back to potential 0 wherever a node doesn't carry that
// label as a candidate at all (same convention as the Picked Tet panel's
// "falls back to 0") -- but that fallback alone is indistinguishable from a
// genuinely-tracked candidate that just happens to sit near 0, so the BLUE
// channel is a separate flag: lit whenever EITHER label 0 or label 1 is
// missing from this node's candidate set entirely. Reading the combined
// color: red/green/yellow = both labels tracked, competing normally; add a
// blue tint (magenta/cyan/pure blue) = at least one of them isn't even a
// candidate here (a third label's territory, most likely). Samples the
// nearest node OF EITHER SUBLATTICE (A or B) to the ray-plane hit point --
// unlike the DisplayMode==1 NodeFootDist view below (A-only, since JFA
// never touches B), NodePotential is defined for every node, so this picks
// whichever of the nearest A-corner or nearest B-center is actually closer.
//
// DisplayMode==1 (GUI combo, DistanceApp.h) switches to a grayscale
// NodeFootDist view instead -- always samples the nearest A-CORNER
// specifically (JFA/NodeFootDist has no B-node values at all), scaled by
// the same ColorScale slider.
RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);
RWStructuredBuffer<float> NodeFootDist : register(u2);
// Synthetic-field only (see smoothnessJacobiSyntheticCS.hlsl) -- each node's
// own closed-form tet-fan "current volume", used by DisplayMode==3 below.
// Never written outside useSyntheticField, so this view is meaningless (last
// Reinit's zero-fill from initSyntheticVolumeCS.hlsl) in the general
// multi-candidate mode.
RWStructuredBuffer<float> NodeSyntheticVolume : register(u3);

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

struct PsOut {
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

PsOut footSlicePS(VsOut input)
{
    PsOut result;
    result.color = float4(0, 0, 0, 0);
    result.depth = 1.0;

    float3 ro = cameraPos.xyz;
    float3 rd = normalize(input.rayDir);

    float originC = SliceAxis == 0 ? ro.x : (SliceAxis == 1 ? ro.y : ro.z);
    float dirC    = SliceAxis == 0 ? rd.x : (SliceAxis == 1 ? rd.y : rd.z);
    if (abs(dirC) < 1.0e-6) discard;

    float t = (SliceCoord - originC) / dirC;
    if (t <= 0.0) discard;

    float3 p = ro + rd * t;

    // Clip to the grid's real extent on the two axes NOT normal to the
    // plane -- otherwise this is an infinite plane, occluding the whole
    // view well past the actual lattice.
    float maxCoord = (float)(GridRes - 1) * CELL_SIZE;
    float2 inPlane = SliceAxis == 0 ? p.yz : (SliceAxis == 1 ? p.xz : p.xy);
    if (any(inPlane < -0.5 * CELL_SIZE) || any(inPlane > maxCoord + 0.5 * CELL_SIZE)) discard;

    int3 aGi = clamp(int3(round(p / CELL_SIZE)),
        int3(0, 0, 0), int3((int)GridRes - 1, (int)GridRes - 1, (int)GridRes - 1));

    float4 clipHit = mul(float4(p, 1), viewProjTransform);
    result.depth = clipHit.z / clipHit.w;
    float scale = max(ColorScale, 1.0e-4);

    if (DisplayMode == 1u) {
        uint aNode = AIdx((uint)aGi.x, (uint)aGi.y, (uint)aGi.z);
        float v = saturate(NodeFootDist[aNode] / scale);
        result.color = float4(v, v, v, 1.0);
        return result;
    }

    float distA = length(p - APos(aGi));

    int3 bGi = clamp(int3(round(p / CELL_SIZE - 0.5)),
        int3(0, 0, 0), int3((int)BDim - 1, (int)BDim - 1, (int)BDim - 1));
    float distB = length(p - BPos(bGi));

    uint node = (distA <= distB)
        ? AIdx((uint)aGi.x, (uint)aGi.y, (uint)aGi.z)
        : BIdx((uint)bGi.x, (uint)bGi.y, (uint)bGi.z);

    // DisplayMode==2: synthetic-field mode's "Potentials" view -- a node
    // carries exactly ONE (label, potential) pair now (slot 0, see
    // smoothnessJacobiSyntheticCS.hlsl), not competing label-0/label-1
    // candidates, so the label-0-vs-label-1 heatmap below doesn't apply
    // (slots 1-7 are stale leftover multi-candidate data in this mode, not
    // meaningful). Color = this node's own label's palette color, scaled by
    // its potential (brighter = more confident).
    if (DisplayMode == 2u) {
        uint synLabel = GetCandidateLabelAt(NodeCandidateLabel, node, 0u);
        float synPot = NodePotential[node * MAX_CANDIDATES + 0u];
        float3 col = LabelColorA(synLabel) * saturate(synPot / scale);
        result.color = float4(col, 1.0);
        return result;
    }

    // DisplayMode==3: synthetic-field "Volume" view -- same visual language
    // as DisplayMode==2 (label-tinted color, scaled by ColorScale), but
    // showing NodeSyntheticVolume (the closed-form tet-fan current volume,
    // smoothnessJacobiSyntheticCS.hlsl) instead of the potential -- lets you
    // directly compare where a label is confidently supported (Potentials)
    // versus where it actually holds geometric volume (Volume).
    if (DisplayMode == 3u) {
        uint synLabel = GetCandidateLabelAt(NodeCandidateLabel, node, 0u);
        float synVol = NodeSyntheticVolume[node];
        float3 col = LabelColorA(synLabel) * saturate(synVol / scale);
        result.color = float4(col, 1.0);
        return result;
    }

    bool has0 = false, has1 = false;
    float phi0 = 0.0, phi1 = 0.0;
    for (uint s = 0; s < MAX_CANDIDATES; s++) {
        uint l = GetCandidateLabelAt(NodeCandidateLabel, node, s);
        float p_s = NodePotential[node * MAX_CANDIDATES + s];
        if (l == 0u) { has0 = true; phi0 = p_s; }
        if (l == 1u) { has1 = true; phi1 = p_s; }
    }

    float missingFlag = (has0 && has1) ? 0.0 : 1.0;
    result.color = float4(saturate(phi0 / scale), saturate(phi1 / scale), missingFlag, 1.0);
    return result;
}
