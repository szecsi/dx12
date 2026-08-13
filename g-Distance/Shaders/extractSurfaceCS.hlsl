#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"
#include "DistanceSurface.hlsli"

// Render-only postprocess, run once after the solve settles (not part of the
// optimization, not run per Jacobi sweep): a tet's active label pair
// (li,lj) is picked INDEPENDENTLY per tet now (no TetInterfacePair/
// assignInterfacePairsCS.hlsl at all -- removed along with the smoothness
// solve's cube-level vote, see the edge-centric design discussion) --
// frequency-count this tet's own 4 corners' CURRENT winning labels
// (GetCornerTopLabel), top-2 by frequency, tie-broken by ascending label.
// Purely a function of this one tet's own 4 corners, no cross-tet
// dependency, so it can never disagree with a neighboring tet's own pick
// the way the old cube-vote could -- accepted as an approximation for
// genuine 3+-label tets (there's no single pair that renders a triple
// junction "correctly" anyway; see the design discussion).
//
// Once (li,lj) is picked, the in-tet interface is a single plane --
// marching tetrahedra on the scalar field g = phi_i - phi_j (affine over
// the tet) -- so a tet's surface fragment is at most one triangle (1-3
// vertex split) or one quad, i.e. 2 triangles (2-2 split). Always writes
// exactly 6 vertices per tet (2 triangles) into a fixed-size buffer;
// unused triangles are collapsed to a single degenerate point (zero area,
// never rasterizes) -- same "always-fixed-stride, degenerate-collapse
// inactive slots" pattern used throughout this codebase (e.g.
// particlePointVS.hlsl's SENTINEL_LABEL handling).
//
// No Tets buffer -- a tet's corners are computed on the fly
// (GetTetCornerQs/QWorldPos, DistanceLattice.hlsli). A corner outside the
// real grid resolves to a fixed virtual background node via
// GetCornerPotential (label 0, potential 1.0, everything else the same
// MISSING_CANDIDATE_POTENTIAL fallback used for a real node's genuinely
// absent candidate) -- no special handling needed for tets near/past the
// domain boundary.
//
// WINDOWED: dispatched over WindowTetCount (a small, GridRes-independent
// render window, see DistanceGridCb.hlsli), not the whole domain -- the
// grid can be huge (up to GridRes=256), but SurfaceVertices only needs to
// hold what's actually rendered. Every thread here works in two index
// spaces: LOCAL (0..WindowTetCount, where it writes in SurfaceVertices) and
// GLOBAL (the real domain-wide tetIndex, where it reads NodeCandidateLabel/
// NodePotential and computes corners/positions via the unchanged
// GetTetCornerQs).
//
// A q-space axis-aligned box is NOT a compact region in real space -- the
// bccToRhombo change of basis is a SHEAR, so an axis-aligned q-box's real-
// space preimage is an elongated parallelepiped whose per-axis span is
// ~1.5x the q-box's own size (confirmed empirically: WindowCubeDim=40 gave
// a real-space span of 60 units, not a small local window at all). So
// WindowCubeDim alone only controls the q-space DISPATCH size (deliberately
// oversized to safely contain the intended real region); the actual
// "local window" cutoff is a separate, explicit real-space filter
// (IsWithinRealWindow below, using WindowRealHalfExtent) applied on top.
#define ExtractSurfaceSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<uint>   NodeCandidateLabel : register(u0);
RWStructuredBuffer<float>  NodePotential : register(u1);
RWStructuredBuffer<SurfaceVertex> SurfaceVertices : register(u2);

// A missing candidate means "this label is not locally viable here" -- e.g.
// a corner deep in one label's territory that never picked up the other
// label within its 26-neighborhood/cube-corner scan (see buildCandidatesCS
// .hlsl), or a virtual corner past the domain boundary. Treat that as a
// confidently-negative potential rather than bailing out of the whole tet:
// the crossing point then lands close to whichever corner *does* have real
// data, instead of leaving a hole.
static const float MISSING_CANDIDATE_POTENTIAL = -10.0;

void WriteDegenerate(uint base)
{
    SurfaceVertex z;
    z.pos = float3(0, 0, 0);
    z.normal = float3(0, 0, 1);
    z.labelI = SENTINEL_LABEL;
    z.labelJ = SENTINEL_LABEL;
    for (uint i = 0; i < 6; i++) SurfaceVertices[base + i] = z;
}

float3 CrossPoint(float3 Pa, float3 Pb, float ga, float gb)
{
    float denom = ga - gb;
    float t = (abs(denom) > 1.0e-12) ? (ga / denom) : 0.5;
    return lerp(Pa, Pb, t);
}

// Maps a LOCAL window-relative tet index to its GLOBAL tetIndex. Returns
// false if the local slot maps outside the global bounding box entirely --
// only possible if the window is configured wider than the whole grid, in
// which case the caller degenerates that slot.
bool GlobalTetIndexFromWindowLocal(uint localT, out uint globalT)
{
    uint localCubeLin = localT / 6u;
    uint slot = localT % 6u;
    uint wd = WindowCubeDim;
    uint lz = localCubeLin / (wd * wd);
    uint rem = localCubeLin % (wd * wd);
    uint ly = rem / wd;
    uint lx = rem % wd;
    int3 globalOrigin = int3((int)lx, (int)ly, (int)lz) + int3(WindowOriginCubeX, WindowOriginCubeY, WindowOriginCubeZ);
    uint globalLin;
    if (!CubeLinearIndex(globalOrigin, globalLin)) { globalT = 0; return false; }
    globalT = globalLin * 6u + slot;
    return true;
}

// Real-space window cutoff: decodes a cube origin's (i,j,k) via the plain
// bccToRhombo inverse formula (no isB branch/range check needed -- this is
// only a rendering cutoff, not a node lookup, so a rough estimate using the
// cube's own origin corner is sufficient) and checks it against a cube of
// half-extent WindowRealHalfExtent around the domain center (GridRes/2 in
// every axis -- the same center BuildShapeList() places test shapes at).
bool IsWithinRealWindow(int3 cubeOrigin)
{
    int i = (cubeOrigin.x + cubeOrigin.y - cubeOrigin.z) / 2;
    int j = (cubeOrigin.x - cubeOrigin.y + cubeOrigin.z) / 2;
    int k = (-cubeOrigin.x + cubeOrigin.y + cubeOrigin.z) / 2;
    int center = (int)(GridRes / 2);
    int half = (int)WindowRealHalfExtent;
    return abs(i - center) <= half && abs(j - center) <= half && abs(k - center) <= half;
}

[RootSignature(ExtractSurfaceSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void extractSurfaceCS(uint3 tid : SV_DispatchThreadID)
{
    uint localT = tid.x;
    if (localT >= WindowTetCount) return;
    uint base = localT * 6;

    uint globalT;
    if (!GlobalTetIndexFromWindowLocal(localT, globalT)) { WriteDegenerate(base); return; }

    {
        uint cubeLinCheck = globalT / 6u;
        int3 cubeOriginCheck = CubeOriginFromLinear(cubeLinCheck);
        if (!IsWithinRealWindow(cubeOriginCheck)) { WriteDegenerate(base); return; }
    }

    int3 q0, q1, q2, q3; GetTetCornerQs(globalT, q0, q1, q2, q3);
    int3 qArr[4] = { q0, q1, q2, q3 };
    uint verts[4] = { ResolveCorner(q0), ResolveCorner(q1), ResolveCorner(q2), ResolveCorner(q3) };

    // Independent per-tet dominant pair: this tet's own 4 corners' current
    // winning labels, frequency-counted, top-2 by frequency (ties broken by
    // ascending label) -- same rule assignInterfacePairsCS.hlsl used to
    // apply per-cube, now purely local to this one tet.
    uint li, lj;
    {
        uint cornerLabels[4];
        for (uint cc = 0; cc < 4; cc++) {
            float unusedPot;
            GetCornerTopLabel(verts[cc], NodeCandidateLabel, NodePotential, cornerLabels[cc], unusedPot);
        }
        uint uniqueLabels[4] = { SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE };
        int freq[4] = { 0, 0, 0, 0 };
        uint nUnique = 0;
        for (uint cc2 = 0; cc2 < 4; cc2++) {
            uint l = cornerLabels[cc2];
            bool found = false;
            for (uint u = 0; u < nUnique; u++) if (uniqueLabels[u] == l) { freq[u]++; found = true; break; }
            if (!found) { uniqueLabels[nUnique] = l; freq[nUnique] = 1; nUnique++; }
        }
        for (uint p = 0; p < 4; p++) {
            for (uint s = 0; s < 3; s++) {
                bool doSwap = (freq[s] < freq[s + 1]) || (freq[s] == freq[s + 1] && uniqueLabels[s] > uniqueLabels[s + 1]);
                if (doSwap) {
                    int tf = freq[s]; freq[s] = freq[s + 1]; freq[s + 1] = tf;
                    uint tl = uniqueLabels[s]; uniqueLabels[s] = uniqueLabels[s + 1]; uniqueLabels[s + 1] = tl;
                }
            }
        }
        li = uniqueLabels[0];
        lj = (freq[1] > 0) ? uniqueLabels[1] : li;
        if (lj < li) { uint tmp = li; li = lj; lj = tmp; }
    }
    if (li == lj) { WriteDegenerate(base); return; }

    float3 P[4];
    float g[4];
    for (uint c2 = 0; c2 < 4; c2++) {
        P[c2] = QWorldPos(qArr[c2]);
        g[c2] = GetCornerPotential(verts[c2], li, NodeCandidateLabel, NodePotential, MISSING_CANDIDATE_POTENTIAL)
              - GetCornerPotential(verts[c2], lj, NodeCandidateLabel, NodePotential, MISSING_CANDIDATE_POTENTIAL);
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
    for (uint k = 0; k < 6; k++) {
        v[k].normal = n;
        v[k].pos = float3(0, 0, 0);
        v[k].labelI = li;
        v[k].labelJ = lj;
    }

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
        float3 q0v = CrossPoint(P[p0], P[n0], g[p0], g[n0]);
        float3 q1v = CrossPoint(P[p1], P[n0], g[p1], g[n0]);
        float3 q2v = CrossPoint(P[p1], P[n1], g[p1], g[n1]);
        float3 q3v = CrossPoint(P[p0], P[n1], g[p0], g[n1]);
        v[0].pos = q0v; v[1].pos = q1v; v[2].pos = q2v;
        v[3].pos = q0v; v[4].pos = q2v; v[5].pos = q3v;
    }

    for (uint w = 0; w < 6; w++) SurfaceVertices[base + w] = v[w];
}
