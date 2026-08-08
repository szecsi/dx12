#pragma once
#include "DistanceConfig.hlsli"

// BCC lattice topology + indexing shared by every g-Distance shader. A-nodes
// are the original cubic grid's corners (GridRes^3, ground-truth labels);
// B-nodes are cube centers ((GridRes-1)^3, solved). Both sublattices are
// packed into one "global node index" space -- [0,ACount) for A, then
// [ACount,ACount+BCount) for B -- so Tets/NodeIncidentTets/NodeCandidateLabel/
// NodePotential can all be single flat buffers addressed uniformly, no
// separate A/B buffer pairs anywhere.
//
// Position/offset conventions (APos/BPos/CrossOffsets*) are ported from
// g-BCC's bccCommon.hlsli.

static const uint GridRes = GRID_RES;
static const uint BDim    = GRID_RES - 1u;
static const uint ACount  = GRID_RES * GRID_RES * GRID_RES;
static const uint BCount  = BDim * BDim * BDim;
static const uint NodeCount = ACount + BCount;

uint AIdx(uint i, uint j, uint k) { return i + j * GridRes + k * GridRes * GridRes; }
uint BIdxLocal(uint i, uint j, uint k) { return i + j * BDim + k * BDim * BDim; }
uint BIdx(uint i, uint j, uint k) { return ACount + BIdxLocal(i, j, k); }

float3 APos(int3 idx) { return float3(idx) * CELL_SIZE; }
float3 BPos(int3 idx) { return (float3(idx) + 0.5) * CELL_SIZE; }

// Decode a global node index into its sublattice + (i,j,k) grid index --
// shared by NodeWorldPos below and nodePointVS.hlsl's neighbor traversal.
void DecodeNodeIndex(uint g, out bool isB, out uint3 idx)
{
    if (g < ACount) {
        isB = false;
        uint k = g / (GridRes * GridRes);
        uint rem = g % (GridRes * GridRes);
        uint j = rem / GridRes;
        uint i = rem % GridRes;
        idx = uint3(i, j, k);
    } else {
        isB = true;
        uint l = g - ACount;
        uint k = l / (BDim * BDim);
        uint rem = l % (BDim * BDim);
        uint j = rem / BDim;
        uint i = rem % BDim;
        idx = uint3(i, j, k);
    }
}

// World position of a global node index -- used by render (nodePointVS.hlsl)
// and by the smoothness solve (smoothnessJacobiCS.hlsl needs actual tet
// geometry for gradients).
float3 NodeWorldPos(uint g)
{
    bool isB; uint3 idx;
    DecodeNodeIndex(g, isB, idx);
    return isB ? BPos((int3)idx) : APos((int3)idx);
}

// The 26 same-sublattice neighbor offsets (3x3x3 minus the center) -- ported
// verbatim from g-BCC's bccCommon.hlsli, used by buildCandidatesCS.hlsl to
// gather an A-node's candidate label set from nearby A-nodes.
static const int3 SameLatticeOffsets[26] = {
    int3(-1,-1,-1), int3(0,-1,-1), int3(1,-1,-1),
    int3(-1, 0,-1), int3(0, 0,-1), int3(1, 0,-1),
    int3(-1, 1,-1), int3(0, 1,-1), int3(1, 1,-1),
    int3(-1,-1, 0), int3(0,-1, 0), int3(1,-1, 0),
    int3(-1, 0, 0),                int3(1, 0, 0),
    int3(-1, 1, 0), int3(0, 1, 0), int3(1, 1, 0),
    int3(-1,-1, 1), int3(0,-1, 1), int3(1,-1, 1),
    int3(-1, 0, 1), int3(0, 0, 1), int3(1, 0, 1),
    int3(-1, 1, 1), int3(0, 1, 1), int3(1, 1, 1),
};

// Interior-face tet generation constants (buildTetsCS.hlsl): every interior
// face of the A corner-grid emits 4 disphenoids (2 A + 2 B each), see that
// file's header comment for the full derivation. Nx = count of interior
// positions along a face's own normal axis; Ni = count of positions along
// each of the two in-plane axes (same for all 3 orientations since the grid
// is cubic).
static const uint Nx = GridRes - 2u;
static const uint Ni = GridRes - 1u;
static const uint FacesPerOrientation = Nx * Ni * Ni;
static const uint TotalFaces = FacesPerOrientation * 3u;
static const uint TetCount = TotalFaces * 4u;

// Standard affine/barycentric shape-function gradients for a tetrahedron:
// for any scalar field affine over the tet with corner values phi_c,
// grad(phi) = sum_c phi_c * w_c, with w_c the "dual basis" vector of the
// tet's edge frame (reciprocal-vector construction). Shared by
// smoothnessJacobiCS.hlsl (energy gradients) and extractSurfaceCS.hlsl
// (interface-plane normal).
void TetShapeGradients(float3 P0, float3 P1, float3 P2, float3 P3,
                        out float3 w0, out float3 w1, out float3 w2, out float3 w3)
{
    float3 e1 = P1 - P0, e2 = P2 - P0, e3 = P3 - P0;
    float3 c23 = cross(e2, e3), c31 = cross(e3, e1), c12 = cross(e1, e2);
    float V = dot(e1, c23);
    float invV = (abs(V) > 1.0e-8) ? (1.0 / V) : 0.0;
    w1 = c23 * invV; w2 = c31 * invV; w3 = c12 * invV;
    w0 = -(w1 + w2 + w3);
}

// Volume of the sub-region within a tet where an affine field g (given by
// its 4 corner values, matching the extractSurfaceCS.hlsl convention: g =
// phi_i - phi_j, "positive side" = label i wins) is >= 0 -- exact marching-
// tet decomposition, matching extractSurfaceCS.hlsl's case classification
// exactly (same posSide/countPos meaning) so the volume-conservation energy
// term (smoothnessJacobiCS.hlsl) and the rendered geometry always agree on
// where the interface actually is.
//
// 1-3 split: the small tet cut off at the lone-sign vertex is similar to
// the original tet, scaled by a different factor t_k along each of the 3
// edges emanating from that vertex -- scaling 3 edges from a shared vertex
// by independent factors scales the tet's volume by their product (the
// determinant of the corresponding diagonal transform), giving the closed
// form Vtet*t0*t1*t2 with no need to build the small tet's geometry at all.
// If the lone vertex is the POSITIVE one, that small tet IS the positive
// region; if it's the lone NEGATIVE one, the positive region is everything
// else (Vtet minus the small tet).
//
// 2-2 split: the "wedge" cut by the plane has 6 vertices (the 2 same-side
// corners p0,p1 plus the 4 crossing points q0..q3, exactly as
// extractSurfaceCS.hlsl builds them). Since q0..q3 lie on a single plane
// (the g=0 cross-section of a convex tet is always planar and convex), a
// fan of 4 tets from the shared ridge edge (p0,p1) around the quad loop
// exactly tiles the wedge with no gaps or double-counting -- same
// "cone over a planar polygon from a line segment" argument that makes a
// simple polygon fan-triangulation exact in 2D, extended by one dimension.
void TetPositiveSideVolume(float3 P[4], float g[4], out bool posSide[4], out uint countPos, out float Vtet, out float VPos)
{
    countPos = 0;
    for (uint c = 0; c < 4; c++) { posSide[c] = g[c] >= 0.0; if (posSide[c]) countPos++; }

    float3 e1 = P[1] - P[0], e2 = P[2] - P[0], e3 = P[3] - P[0];
    Vtet = abs(dot(e1, cross(e2, e3))) / 6.0;

    if (countPos == 0) { VPos = 0.0; return; }
    if (countPos == 4) { VPos = Vtet; return; }

    if (countPos == 1 || countPos == 3) {
        bool loneVal = (countPos == 1);
        uint lone = 0;
        for (uint c2 = 0; c2 < 4; c2++) if (posSide[c2] == loneVal) { lone = c2; break; }
        uint others[3];
        uint oc = 0;
        for (uint c3 = 0; c3 < 4; c3++) if (c3 != lone) others[oc++] = c3;

        float t0 = g[lone] / (g[lone] - g[others[0]]);
        float t1 = g[lone] / (g[lone] - g[others[1]]);
        float t2 = g[lone] / (g[lone] - g[others[2]]);
        float Vlone = Vtet * abs(t0 * t1 * t2);
        VPos = loneVal ? Vlone : (Vtet - Vlone);
        return;
    }

    // countPos == 2
    uint p0 = 0, p1 = 0, n0 = 0, n1 = 0;
    bool gotP0 = false, gotN0 = false;
    for (uint c4 = 0; c4 < 4; c4++) {
        if (posSide[c4]) { if (!gotP0) { p0 = c4; gotP0 = true; } else p1 = c4; }
        else { if (!gotN0) { n0 = c4; gotN0 = true; } else n1 = c4; }
    }
    float tP0N0 = g[p0] / (g[p0] - g[n0]);
    float tP1N0 = g[p1] / (g[p1] - g[n0]);
    float tP1N1 = g[p1] / (g[p1] - g[n1]);
    float tP0N1 = g[p0] / (g[p0] - g[n1]);
    float3 q0 = lerp(P[p0], P[n0], tP0N0);
    float3 q1 = lerp(P[p1], P[n0], tP1N0);
    float3 q2 = lerp(P[p1], P[n1], tP1N1);
    float3 q3 = lerp(P[p0], P[n1], tP0N1);

    float3 quad[5] = { q0, q1, q2, q3, q0 };
    float vol = 0.0;
    for (uint k = 0; k < 4; k++) {
        float3 ee1 = P[p1] - P[p0], ee2 = quad[k] - P[p0], ee3 = quad[k + 1] - P[p0];
        vol += abs(dot(ee1, cross(ee2, ee3))) / 6.0;
    }
    VPos = vol;
}

// Cheap deterministic hash -> [-1,1], used to jitter initial non-winning
// candidate potentials so B-nodes (which start with no a-priori winner)
// don't begin in an exact tie.
float DistanceJitter(uint a, uint b)
{
    uint h = a * 747796405u + b * 2891336453u + 12345u;
    h = (h ^ (h >> 16)) * 2246822519u;
    h = (h ^ (h >> 13)) * 3266489917u;
    h = h ^ (h >> 16);
    return (float(h & 0xFFFFu) / 65535.0) * 2.0 - 1.0;
}
