#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"
#include "DistanceSurface.hlsli"

// Synthetic-field surface extraction -- a copy of extractSurfaceCS.hlsl
// adapted to decode the synthetic field's single (label, potential) per node
// (see smoothnessJacobiSyntheticCS.hlsl) instead of the multi-candidate
// layout. Every downstream computation (the per-tet li/lj frequency vote,
// the marching-tet crossing math, the quad/triangle split, degenerate
// collapse) is IDENTICAL to extractSurfaceCS.hlsl -- only the two corner
// lookups differ, replacing GetCornerTopLabel/GetCornerPotential
// (DistanceLattice.hlsli, multi-candidate search) with trivial single-slot
// reads of NodeCandidateLabel's byte 0 / NodePotential's slot 0. See that
// file for the full design rationale (comments not repeated here).
#define ExtractSurfaceSyntheticSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<uint>   NodeCandidateLabel : register(u0);
RWStructuredBuffer<float>  NodePotential : register(u1);
RWStructuredBuffer<SurfaceVertex> SurfaceVertices : register(u2);

static const float MISSING_CANDIDATE_POTENTIAL = -10.0;

// Mirrors GetCornerTopLabel's virtual-corner convention (label 0) -- the
// potential GetCornerTopLabel would also return there is unused by
// extractSurfaceCS.hlsl's li/lj vote, so this only returns the label.
uint SyntheticCornerLabel(uint cornerRef)
{
    if (cornerRef == SENTINEL_LABEL) return 0u;
    return GetCandidateLabelAt(NodeCandidateLabel, cornerRef, 0u);
}

// NOT a port of GetCornerPotential's two-independent-lookups pattern --
// that would (and did) break here. A multi-candidate node can genuinely
// track BOTH li and lj as independent slots, so "missing" is the rare edge
// case (a node deep in one label's territory that never picked up the
// other). A synthetic-field node only EVER carries ONE label -- so for
// EVERY corner of EVERY genuine li/lj interface tet, exactly one of the two
// per-label lookups would "miss" by construction, not as an edge case.
// Falling back to a large constant (missingFallback, -10) for that always-
// happens case swamped the real, evolving potential entirely: g[c] ended up
// dominated by +-10 regardless of the actual (much smaller) potential
// magnitude, so the crossing point sat frozen near each tet's geometric
// midpoint no matter how much the potentials themselves moved -- exactly
// the "colors change, surface doesn't" symptom this was written to fix.
// Instead: if this corner's own label is li, +itsOwnPotential; if lj,
// -itsOwnPotential (same sign convention smoothnessJacobiSyntheticCS.hlsl's
// kernel already uses for same/different-label neighbors); only fall back
// to missingFallback when the corner's label is genuinely NEITHER li nor lj
// (an actual 3rd-label junction corner -- still rare, that fallback still
// makes sense there).
float SyntheticCornerG(uint cornerRef, uint li, uint lj, float missingFallback)
{
    uint ownLabel;
    float ownPot;
    if (cornerRef == SENTINEL_LABEL) { ownLabel = 0u; ownPot = 1.0; } // mirrors GetCornerTopLabel's virtual-corner convention
    else {
        ownLabel = GetCandidateLabelAt(NodeCandidateLabel, cornerRef, 0u);
        ownPot = NodePotential[cornerRef * MAX_CANDIDATES + 0u];
    }
    if (ownLabel == li) return ownPot;
    if (ownLabel == lj) return -ownPot;
    return missingFallback;
}

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

bool IsWithinRealWindow(int3 cubeOrigin)
{
    int i = (cubeOrigin.x + cubeOrigin.y - cubeOrigin.z) / 2;
    int j = (cubeOrigin.x - cubeOrigin.y + cubeOrigin.z) / 2;
    int k = (-cubeOrigin.x + cubeOrigin.y + cubeOrigin.z) / 2;
    int center = (int)(GridRes / 2);
    int half = (int)WindowRealHalfExtent;
    return abs(i - center) <= half && abs(j - center) <= half && abs(k - center) <= half;
}

[RootSignature(ExtractSurfaceSyntheticSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void extractSurfaceSyntheticCS(uint3 tid : SV_DispatchThreadID)
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

    uint li, lj;
    {
        uint cornerLabels[4];
        for (uint cc = 0; cc < 4; cc++) cornerLabels[cc] = SyntheticCornerLabel(verts[cc]);
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
        g[c2] = SyntheticCornerG(verts[c2], li, lj, MISSING_CANDIDATE_POTENTIAL);
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
        bool loneVal = (countPos == 1);
        uint lone = 0;
        for (uint c4 = 0; c4 < 4; c4++) if (pos[c4] == loneVal) { lone = c4; break; }
        uint others[3];
        uint oc = 0;
        for (uint c5 = 0; c5 < 4; c5++) if (c5 != lone) others[oc++] = c5;

        v[0].pos = CrossPoint(P[lone], P[others[0]], g[lone], g[others[0]]);
        v[1].pos = CrossPoint(P[lone], P[others[1]], g[lone], g[others[1]]);
        v[2].pos = CrossPoint(P[lone], P[others[2]], g[lone], g[others[2]]);
        v[3] = v[0]; v[4] = v[0]; v[5] = v[0];
    } else {
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
