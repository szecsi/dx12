#pragma once
#include "DistanceConfig.hlsli"

// BCC lattice topology + indexing shared by every g-Distance shader. A-nodes
// are the original cubic grid's corners (GridRes^3, ground-truth labels);
// B-nodes are cube centers ((GridRes-1)^3, solved). Both sublattices are
// packed into one "global node index" space -- [0,ACount) for A, then
// [ACount,ACount+BCount) for B -- so NodeCandidateLabel/NodePotential can be
// single flat buffers addressed uniformly, no separate A/B buffer pairs
// anywhere. Tet connectivity has no buffers of its own at all -- see the
// "Rhombohedral cube-based tet indexing" section below, everything is
// computed on the fly from a tet or node index.
//
// Position/offset conventions (APos/BPos/CrossOffsets*) are ported from
// g-BCC's bccCommon.hlsli.
//
// GridRes/BDim/ACount/BCount/NodeCount (and the q-space bounding-box/window
// fields further below) are all RUNTIME values now -- grid resolution is a
// GUI slider applied on Reinitialize, not a compile-time constant -- see
// DistanceGridCb.hlsli. Every function in this file already just references
// these names, so nothing else here changes.
#include "DistanceGridCb.hlsli"

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

// Rhombohedral cube-based tet indexing (bccToRhombo change of basis): every
// unit cube of a virtual "q-space" simple-cubic lattice decomposes into 6
// tets sharing the cube's main diagonal D0=(0,0,0)-D1=(1,1,1); each tet's
// remaining 2 vertices are one of the cube's 6 equatorial (non-diagonal-
// touching) edges. q relates to true BCC lattice points via
// q=(i+j,i+k,j+k) for A-node (i,j,k) and q=(i+j+1,i+k+1,j+k+1) for B-node
// (i,j,k); node type is recoverable as the parity of q.x+q.y+q.z (even=A,
// odd=B). No buffer stores tet connectivity anywhere in this pipeline --
// every consumer (smoothnessJacobiCS.hlsl, extractSurfaceCS.hlsl,
// assignInterfacePairsCS.hlsl) computes a tet's corners, a node's incident
// tets, and a tet's cross-cube neighbors on the fly, purely from a tet or
// node index plus the small constant tables below. Tables derived and
// cross-verified earlier this session (see the approved plan,
// soft-stargazing-biscuit.md) -- NOT the same as the initially proposed
// edgeMap, which had a bug caught during derivation.
//
// A corner is not resolved to "real node or nothing" -- a q-space point
// outside the real grid is treated as a fixed VIRTUAL node (background
// label 0, potential 1.0, everything else absent) rather than making the
// whole tet/cube invalid. This means every dispatched tet is always
// meaningful (uniformly computed, no per-cube valid/invalid branch), at
// the cost of some genuinely wasted work on tets far outside the domain --
// an explicit, accepted tradeoff (see the plan).
static const int3 CubeVertexOffsets[6][2] = {
    { int3(1, 0, 0), int3(1, 0, 1) }, // slot0
    { int3(1, 0, 0), int3(1, 1, 0) }, // slot1
    { int3(0, 1, 0), int3(1, 1, 0) }, // slot2
    { int3(0, 1, 0), int3(0, 1, 1) }, // slot3
    { int3(0, 0, 1), int3(0, 1, 1) }, // slot4
    { int3(0, 0, 1), int3(1, 0, 1) }, // slot5
};

// Ring vertex shared between slot i and slot (i+1)%6 -- a node sitting at
// this local offset relative to a cube origin is a corner of exactly those
// 2 tets (used by GatherIncidentTets below).
static const int3 RingBetween[6] = {
    int3(1, 0, 0), int3(1, 1, 0), int3(0, 1, 0), int3(0, 1, 1), int3(0, 0, 1), int3(1, 0, 1)
};

// Cross-cube ("cap") neighbors: tet slot i's D0-cap face crosses to the
// neighbor cube at D0CapOffset[i] (always -1 along one axis), landing on
// slot D0CapTargetSlot[i] there; its D1-cap face crosses to D1CapOffset[i]
// (always +1), landing on D1CapTargetSlot[i]. The two target-slot tables
// are exact inverse permutations of each other -- the consistency check
// that caught the original edgeMap's bug (crossing to the neighbor and
// back must return you to where you started).
static const int3 D0CapOffset[6] = {
    int3(0, -1, 0), int3(0, 0, -1), int3(0, 0, -1), int3(-1, 0, 0), int3(-1, 0, 0), int3(0, -1, 0)
};
static const uint D0CapTargetSlot[6] = { 2, 5, 4, 1, 0, 3 };

static const int3 D1CapOffset[6] = {
    int3(1, 0, 0), int3(1, 0, 0), int3(0, 1, 0), int3(0, 1, 0), int3(0, 0, 1), int3(0, 0, 1)
};
static const uint D1CapTargetSlot[6] = { 4, 3, 0, 5, 2, 1 };

// Cube-origin bounding box for q-space dispatch: every candidate cube
// origin in this box gets a permanent 6-tet slot (tetBase=6*linearIndex)
// whether or not it's fully "interior" -- corners outside the real grid
// resolve to virtual background nodes (ResolveCorner below) rather than
// excluding the cube. The box only needs to be wide enough that every cube
// touching AT LEAST ONE real corner is included (a cube touching zero real
// corners just contributes harmless all-background tets if included, so
// there's no need to trim tightly -- only to never miss a cube that DOES
// touch real data).
//
// Checking each of the 8 corner-offset types' reach: the lowest an origin
// can be and still touch the real q=0 minimum is -1 (via a +1-offset
// corner, e.g. D1); the highest an origin can be and still touch the real
// q=2*GridRes-2 maximum is 2*GridRes-2 itself (via a +0-offset corner,
// i.e. D0). So origin range [-1, 2*GridRes-2] (width 2*GridRes) is exactly
// sufficient -- also verified empirically (every real node has >=1
// incident tet), see the approved plan. CubeOriginMin/CubeBoundDim/
// TotalCubeCandidates/TetCount are computed from GridRes once on the CPU
// side (DistanceApp.h's EnsureGridBuffersSized) and uploaded via
// DistanceGridCb.hlsli, rather than recomputed here every invocation --
// this also keeps them exactly in sync with each buffer's actual allocated
// size, which is what really matters.

// Linear index of a cube origin into the (widened, negative-capable) q-
// space bounding box -- out of range means this cube has no addressable
// tet-slot at all (outside the deliberately-oversized search space, see
// CubeOriginMin/CubeBoundDim above -- this can only happen right at the
// very edge of the box, never for a cube anywhere near real data).
bool CubeLinearIndex(int3 C, out uint idx)
{
    idx = 0;
    int3 shifted = C - int3(CubeOriginMin, CubeOriginMin, CubeOriginMin);
    if (any(shifted < 0) || any(shifted >= CubeBoundDim)) return false;
    idx = (uint)shifted.x + (uint)shifted.y * (uint)CubeBoundDim + (uint)shifted.z * (uint)CubeBoundDim * (uint)CubeBoundDim;
    return true;
}

// Decodes a flat cube-candidate dispatch index back into its q-space
// origin -- inverse of CubeLinearIndex.
int3 CubeOriginFromLinear(uint lin)
{
    uint cd = (uint)CubeBoundDim;
    uint z = lin / (cd * cd);
    uint rem = lin % (cd * cd);
    uint y = rem / cd;
    uint x = rem % cd;
    return int3((int)x + CubeOriginMin, (int)y + CubeOriginMin, (int)z + CubeOriginMin);
}

// Resolves a q-space point to its global node index -- SENTINEL_LABEL if
// it falls outside the real grid (a "virtual" corner, see GetCornerPotential
// /GetCornerTopLabel below for how those are treated). Every integer q
// decodes to *some* integer (i,j,k) via this inverse transform (exact, no
// rounding -- p's coordinate sum is always even by construction of the isB
// branch below, which is exactly the condition needed for all 3 divisions
// to be exact); only the final index-range check can fail.
uint ResolveCorner(int3 q)
{
    bool isB = ((q.x + q.y + q.z) & 1) != 0;
    int3 p = isB ? (q - int3(1, 1, 1)) : q;
    int i = (p.x + p.y - p.z) / 2;
    int j = (p.x - p.y + p.z) / 2;
    int k = (-p.x + p.y + p.z) / 2;
    if (!isB) {
        if (i < 0 || j < 0 || k < 0 || i >= (int)GridRes || j >= (int)GridRes || k >= (int)GridRes) return SENTINEL_LABEL;
        return AIdx((uint)i, (uint)j, (uint)k);
    } else {
        if (i < 0 || j < 0 || k < 0 || i >= (int)BDim || j >= (int)BDim || k >= (int)BDim) return SENTINEL_LABEL;
        return BIdx((uint)i, (uint)j, (uint)k);
    }
}

// World position of a q-space point -- always succeeds, no range check
// (extrapolates past the real grid for a virtual corner using the exact
// same formula as a real one, so there's no special case at all here).
// Same inverse transform as ResolveCorner, evaluated in float since a
// virtual corner's (i,j,k) is only used geometrically, never as a buffer
// index.
float3 QWorldPos(int3 q)
{
    bool isB = ((q.x + q.y + q.z) & 1) != 0;
    int3 p = isB ? (q - int3(1, 1, 1)) : q;
    float i = (float)(p.x + p.y - p.z) * 0.5;
    float j = (float)(p.x - p.y + p.z) * 0.5;
    float k = (float)(-p.x + p.y + p.z) * 0.5;
    float3 base = float3(i, j, k);
    return (isB ? (base + 0.5) : base) * CELL_SIZE;
}

// The 4 q-space corners of a tet -- D0, D1, and the slot's ring edge
// (U0,U1). Pure arithmetic from tetIndex, no data read.
void GetTetCornerQs(uint tetIndex, out int3 q0, out int3 q1, out int3 q2, out int3 q3)
{
    uint cubeLin = tetIndex / 6u;
    uint slot = tetIndex % 6u;
    int3 C = CubeOriginFromLinear(cubeLin);
    q0 = C;
    q1 = C + int3(1, 1, 1);
    q2 = C + CubeVertexOffsets[slot][0];
    q3 = C + CubeVertexOffsets[slot][1];
}

// Convenience wrapper for callers that only need corner IDENTITY (not
// position), e.g. assignInterfacePairsCS.hlsl's label vote.
uint4 GetTetCornerRefs(uint tetIndex)
{
    int3 q0, q1, q2, q3;
    GetTetCornerQs(tetIndex, q0, q1, q2, q3);
    return uint4(ResolveCorner(q0), ResolveCorner(q1), ResolveCorner(q2), ResolveCorner(q3));
}

// Packed candidate-label accessor: nodeCandidateLabelBuffer stores ONE uint
// per node (not per slot) -- MAX_CANDIDATES(6) 5-bit fields packed into the
// low 30 bits, see DistanceConfig.hlsli's SENTINEL_CANDIDATE comment. Safe
// as a plain (non-atomic) bitfield because every node's candidate slots are
// written exactly once, by exactly one thread, in buildCandidatesCS.hlsl --
// no cross-thread read-modify-write ever touches this buffer afterward
// (only NodePotential/NodePotentialScratch, which stay one float per slot,
// change during the solve).
uint GetCandidateLabelAt(RWStructuredBuffer<uint> buf, uint node, uint slot)
{
    return (buf[node] >> (slot * 5u)) & 0x1Fu;
}

// Candidate-label potential lookup for a tet corner, real or virtual.
// Virtual corners (cornerRef==SENTINEL_LABEL) are a fixed background node:
// label 0 with potential 1.0, everything else "missing" -- missingFallback
// stays a parameter rather than a hardcoded constant because
// smoothnessJacobiCS.hlsl and extractSurfaceCS.hlsl already use different
// fallback values (0.0 vs -10.0) for a real node's genuinely absent
// candidate, for reasons documented in each file; a virtual corner falls
// back the same way each caller already does for "label not present here".
float GetCornerPotential(uint cornerRef, uint label, RWStructuredBuffer<uint> candLabel, RWStructuredBuffer<float> candPot, float missingFallback)
{
    if (cornerRef == SENTINEL_LABEL) return (label == 0u) ? 1.0 : missingFallback;
    for (uint s = 0; s < MAX_CANDIDATES; s++) {
        if (GetCandidateLabelAt(candLabel, cornerRef, s) == label) return candPot[cornerRef * MAX_CANDIDATES + s];
    }
    return missingFallback;
}

// Top (argmax) candidate label at a tet corner, real or virtual.
void GetCornerTopLabel(uint cornerRef, RWStructuredBuffer<uint> candLabel, RWStructuredBuffer<float> candPot, out uint label, out float pot)
{
    if (cornerRef == SENTINEL_LABEL) { label = 0u; pot = 1.0; return; }
    label = SENTINEL_CANDIDATE;
    pot = -1.0e30;
    for (uint s = 0; s < MAX_CANDIDATES; s++) {
        uint l = GetCandidateLabelAt(candLabel, cornerRef, s);
        if (l == SENTINEL_CANDIDATE) continue;
        float p = candPot[cornerRef * MAX_CANDIDATES + s];
        if (p > pot) { pot = p; label = l; }
    }
}

// Gathers this node's incident tet indices (<=MAX_INCIDENT_TETS), computed
// directly from its q-space coordinate -- mirrors the disphenoid corner
// enumeration in reverse: for each of the 8 candidate cube origins where
// this node could be a corner (D0, D1, or one of the 6 ring positions), if
// that origin has an addressable tet-slot at all (CubeLinearIndex), its
// tets are incident -- all 6 for the D0/D1 case, exactly 2 for a ring
// position (via RingBetween's slot pairing). No data read/validity check
// beyond the bounds test: every in-box cube's tets are always meaningful
// now (see the header comment above), so there's nothing else to check.
uint GatherIncidentTets(uint node, out uint tets[MAX_INCIDENT_TETS])
{
    for (uint z = 0; z < MAX_INCIDENT_TETS; z++) tets[z] = SENTINEL_LABEL;

    bool isB; uint3 idx;
    DecodeNodeIndex(node, isB, idx);
    int3 q = isB
        ? int3((int)idx.x + (int)idx.y + 1, (int)idx.x + (int)idx.z + 1, (int)idx.y + (int)idx.z + 1)
        : int3((int)idx.x + (int)idx.y, (int)idx.x + (int)idx.z, (int)idx.y + (int)idx.z);

    uint count = 0;
    {
        uint lin;
        if (CubeLinearIndex(q, lin)) { // this node as D0
            uint tetBase = lin * 6;
            for (uint s = 0; s < 6 && count < MAX_INCIDENT_TETS; s++) tets[count++] = tetBase + s;
        }
    }
    {
        uint lin;
        if (CubeLinearIndex(q - int3(1, 1, 1), lin)) { // this node as D1
            uint tetBase = lin * 6;
            for (uint s = 0; s < 6 && count < MAX_INCIDENT_TETS; s++) tets[count++] = tetBase + s;
        }
    }
    for (uint i = 0; i < 6; i++) {
        uint lin;
        if (CubeLinearIndex(q - RingBetween[i], lin)) { // this node as a ring vertex
            uint tetBase = lin * 6;
            if (count < MAX_INCIDENT_TETS) tets[count++] = tetBase + i;
            if (count < MAX_INCIDENT_TETS) tets[count++] = tetBase + (i + 1) % 6;
        }
    }
    return count;
}

// Cross-cube ("cap") neighbor tets of a tet, computed directly -- no
// TetFaceNeighbors buffer. SENTINEL_LABEL means the neighbor cube's origin
// falls outside the (deliberately oversized) dispatch box entirely -- the
// only remaining "doesn't exist" case, confined to the very edge of the
// box, never affecting a tet anywhere near real data.
uint2 GetCrossNeighbors(uint tetIndex)
{
    uint cubeLin = tetIndex / 6u;
    uint slot = tetIndex % 6u;
    int3 C = CubeOriginFromLinear(cubeLin);

    uint lin0, lin1;
    uint cross0 = CubeLinearIndex(C + D0CapOffset[slot], lin0) ? (lin0 * 6 + D0CapTargetSlot[slot]) : SENTINEL_LABEL;
    uint cross1 = CubeLinearIndex(C + D1CapOffset[slot], lin1) ? (lin1 * 6 + D1CapTargetSlot[slot]) : SENTINEL_LABEL;
    return uint2(cross0, cross1);
}

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
