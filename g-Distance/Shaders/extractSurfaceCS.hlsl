#include "DistanceLattice.hlsli"
#include "DistanceSurface.hlsli"

// Render-only postprocess, run once after the solve settles (not part of the
// optimization, not run per Jacobi sweep): since v1 only ever tracks 2
// competing labels per tet (TetInterfacePair), the in-tet interface is a
// single plane -- marching tetrahedra on the scalar field g = phi_i - phi_j
// (affine over the tet) -- so a tet's surface fragment is at most one
// triangle (1-3 vertex split) or one quad, i.e. 2 triangles (2-2 split).
// Always writes exactly 6 vertices per tet (2 triangles) into a fixed-size
// buffer; unused triangles are collapsed to a single degenerate point (zero
// area, never rasterizes) -- same "always-fixed-stride, degenerate-collapse
// inactive slots" pattern used throughout this codebase (e.g.
// particlePointVS.hlsl's SENTINEL_LABEL handling).
#define ExtractSurfaceSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)"

RWStructuredBuffer<uint4>  Tets : register(u0);
RWStructuredBuffer<uint2>  TetInterfacePair : register(u1);
RWStructuredBuffer<uint>   NodeCandidateLabel : register(u2);
RWStructuredBuffer<float>  NodePotential : register(u3);
RWStructuredBuffer<SurfaceVertex> SurfaceVertices : register(u4);

int FindSlot(uint node, uint label)
{
    if (label == SENTINEL_LABEL) return -1;
    for (int s = 0; s < 8; s++)
        if (NodeCandidateLabel[node * 8 + (uint)s] == label) return s;
    return -1;
}

// A missing candidate means "this label is not locally viable here" -- e.g.
// a corner deep in one label's territory that never picked up the other
// label within its 26-neighborhood/cube-corner scan (see buildCandidatesCS
// .hlsl). Treat that as a confidently-negative potential rather than
// bailing out of the whole tet: the crossing point then lands close to
// whichever corner *does* have real data, instead of leaving a hole.
static const float MISSING_CANDIDATE_POTENTIAL = -10.0;

float GetPotBySlot(uint node, int slot)
{
    return (slot >= 0) ? NodePotential[node * 8 + (uint)slot] : MISSING_CANDIDATE_POTENTIAL;
}

void WriteDegenerate(uint base)
{
    SurfaceVertex z;
    z.pos = float3(0, 0, 0);
    z.normal = float3(0, 0, 1);
    for (uint i = 0; i < 6; i++) SurfaceVertices[base + i] = z;
}

float3 CrossPoint(float3 Pa, float3 Pb, float ga, float gb)
{
    float denom = ga - gb;
    float t = (abs(denom) > 1.0e-12) ? (ga / denom) : 0.5;
    return lerp(Pa, Pb, t);
}

[RootSignature(ExtractSurfaceSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void extractSurfaceCS(uint3 tid : SV_DispatchThreadID)
{
    uint t = tid.x;
    if (t >= TetCount) return;
    uint base = t * 6;

    uint2 pair = TetInterfacePair[t];
    if (pair.x == pair.y) { WriteDegenerate(base); return; }
    uint li = pair.x, lj = pair.y;

    uint4 tet = Tets[t];
    uint verts[4] = { tet.x, tet.y, tet.z, tet.w };

    int slotI[4], slotJ[4];
    for (uint c = 0; c < 4; c++) {
        slotI[c] = FindSlot(verts[c], li);
        slotJ[c] = FindSlot(verts[c], lj);
    }

    float3 P[4];
    float g[4];
    for (uint c2 = 0; c2 < 4; c2++) {
        P[c2] = NodeWorldPos(verts[c2]);
        g[c2] = GetPotBySlot(verts[c2], slotI[c2]) - GetPotBySlot(verts[c2], slotJ[c2]);
    }

    bool pos[4];
    uint countPos = 0;
    for (uint c3 = 0; c3 < 4; c3++) { pos[c3] = g[c3] >= 0.0; if (pos[c3]) countPos++; }

    if (countPos == 0 || countPos == 4) { WriteDegenerate(base); return; }

    float3 w0, w1, w2, w3;
    TetShapeGradients(P[0], P[1], P[2], P[3], w0, w1, w2, w3);
    float3 wArr[4] = { w0, w1, w2, w3 };
    float3 gradG = g[0] * wArr[0] + g[1] * wArr[1] + g[2] * wArr[2] + g[3] * wArr[3];
    float gLen = length(gradG);
    float3 n = (gLen > 1.0e-8) ? (gradG / gLen) : float3(0, 0, 1);

    SurfaceVertex v[6];
    for (uint k = 0; k < 6; k++) { v[k].normal = n; v[k].pos = float3(0, 0, 0); }

    if (countPos == 1 || countPos == 3) {
        // Single-vertex-vs-triple split: one triangle from the 3 edges
        // connecting the lone-sign vertex to the other 3.
        bool loneVal = (countPos == 1);
        uint lone = 0;
        for (uint c4 = 0; c4 < 4; c4++) if (pos[c4] == loneVal) { lone = c4; break; }
        uint others[3];
        uint oc = 0;
        for (uint c5 = 0; c5 < 4; c5++) if (c5 != lone) others[oc++] = c5;

        v[0].pos = CrossPoint(P[lone], P[others[0]], g[lone], g[others[0]]);
        v[1].pos = CrossPoint(P[lone], P[others[1]], g[lone], g[others[1]]);
        v[2].pos = CrossPoint(P[lone], P[others[2]], g[lone], g[others[2]]);
        v[3] = v[0]; v[4] = v[0]; v[5] = v[0]; // second triangle degenerate
    } else {
        // 2-2 split: a planar quad. Its 4 edges each lie on one of the tet's
        // 4 faces, giving the cyclic (non-self-intersecting) vertex order
        // cross(p0,n0) -> cross(p1,n0) -> cross(p1,n1) -> cross(p0,n1).
        uint p0 = 0, p1 = 0, n0 = 0, n1 = 0;
        bool gotP0 = false, gotN0 = false;
        for (uint c6 = 0; c6 < 4; c6++) {
            if (pos[c6]) { if (!gotP0) { p0 = c6; gotP0 = true; } else p1 = c6; }
            else { if (!gotN0) { n0 = c6; gotN0 = true; } else n1 = c6; }
        }
        float3 q0 = CrossPoint(P[p0], P[n0], g[p0], g[n0]);
        float3 q1 = CrossPoint(P[p1], P[n0], g[p1], g[n0]);
        float3 q2 = CrossPoint(P[p1], P[n1], g[p1], g[n1]);
        float3 q3 = CrossPoint(P[p0], P[n1], g[p0], g[n1]);
        v[0].pos = q0; v[1].pos = q1; v[2].pos = q2;
        v[3].pos = q0; v[4].pos = q2; v[5].pos = q3;
    }

    for (uint w = 0; w < 6; w++) SurfaceVertices[base + w] = v[w];
}
