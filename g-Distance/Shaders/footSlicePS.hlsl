#include "DistanceFrameCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"

// No [RootSignature(...)] here -- declared once by footSliceVS.hlsl, reused
// for both stages of this draw (this codebase's convention).
cbuffer SliceConsts : register(b1) {
    uint  SliceAxis;   // 0 = X, 1 = Y, 2 = Z -- which world axis the plane is normal to
    float SliceCoord;  // world-space coordinate of the plane along SliceAxis
    float ColorScale;  // potential value that maps to full channel brightness
    float _padSlice;
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
// unlike the old NodeFootDist view (A-only, since JFA never touches B),
// NodePotential is defined for every node, so this picks whichever of the
// nearest A-corner or nearest B-center is actually closer.
RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);

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
    float distA = length(p - APos(aGi));

    int3 bGi = clamp(int3(round(p / CELL_SIZE - 0.5)),
        int3(0, 0, 0), int3((int)BDim - 1, (int)BDim - 1, (int)BDim - 1));
    float distB = length(p - BPos(bGi));

    uint node = (distA <= distB)
        ? AIdx((uint)aGi.x, (uint)aGi.y, (uint)aGi.z)
        : BIdx((uint)bGi.x, (uint)bGi.y, (uint)bGi.z);

    bool has0 = false, has1 = false;
    float phi0 = 0.0, phi1 = 0.0;
    for (uint s = 0; s < MAX_CANDIDATES; s++) {
        uint l = GetCandidateLabelAt(NodeCandidateLabel, node, s);
        float p_s = NodePotential[node * MAX_CANDIDATES + s];
        if (l == 0u) { has0 = true; phi0 = p_s; }
        if (l == 1u) { has1 = true; phi1 = p_s; }
    }

    float4 clipHit = mul(float4(p, 1), viewProjTransform);
    result.depth = clipHit.z / clipHit.w;
    float scale = max(ColorScale, 1.0e-4);
    float missingFlag = (has0 && has1) ? 0.0 : 1.0;
    result.color = float4(saturate(phi0 / scale), saturate(phi1 / scale), missingFlag, 1.0);
    return result;
}
