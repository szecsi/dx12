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
// every consumer (smoothnessJacobiCS.hlsl, extractSurfaceCS.hlsl) computes
// a tet's corners, a node's incident tets, and an edge's tet-fan on the fly,
// purely from a tet or node index plus the small constant tables below. Tables derived and
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

// Packed candidate-label accessor: nodeCandidateLabelBuffer stores TWO
// uints per node -- MAX_CANDIDATES(8) 8-bit fields, 4 packed per word (word
// = slot/4, shift = (slot%4)*8), see DistanceConfig.hlsli's
// SENTINEL_CANDIDATE comment. Safe as a plain (non-atomic) bitfield because
// every node's candidate slots are written exactly once, by exactly one
// thread, in buildCandidatesCS.hlsl -- no cross-thread read-modify-write
// ever touches this buffer afterward (only NodePotential/
// NodePotentialScratch, which stay one float per slot, change during the
// solve).
uint GetCandidateLabelAt(RWStructuredBuffer<uint> buf, uint node, uint slot)
{
    return (buf[node * 2u + slot / 4u] >> ((slot % 4u) * 8u)) & 0xFFu;
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
    if (cornerRef == SENTINEL_LABEL) return (label == 0u) ? 0.0 : missingFallback;
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
// Q-space coordinate of a global node index -- the forward bccToRhombo
// transform (A: q=(i+j,i+k,j+k), B: q=(i+j+1,i+k+1,j+k+1)), shared by
// GatherIncidentTets and GatherEdgeTets/edge-centric neighbor lookup below.
int3 NodeQ(uint node)
{
    bool isB; uint3 idx;
    DecodeNodeIndex(node, isB, idx);
    return isB
        ? int3((int)idx.x + (int)idx.y + 1, (int)idx.x + (int)idx.z + 1, (int)idx.y + (int)idx.z + 1)
        : int3((int)idx.x + (int)idx.y, (int)idx.x + (int)idx.z, (int)idx.y + (int)idx.z);
}

uint GatherIncidentTets(uint node, out uint tets[MAX_INCIDENT_TETS])
{
    for (uint z = 0; z < MAX_INCIDENT_TETS; z++) tets[z] = SENTINEL_LABEL;

    int3 q = NodeQ(node);

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

// Spatial (GridRes-agnostic) cap tables for a tet slot's two OUT-OF-CUBE
// face-adjacent partners -- D0-cap and D1-cap cross into a NEIGHBORING
// cube's tet; fanNext/fanPrev ((slot+1)%6 / (slot+5)%6) stay within the SAME
// cube, no table needed. Cube-ORIGIN offsets + a target slot, not flat
// tet-index offsets -- unlike the flat PartnerOffset table this project
// briefly used (removed once GridRes became a runtime GUI value: flat
// tet-index strides depend on CubeBoundDim, these spatial offsets don't).
// Re-derived from GetTetCornerQs' own per-slot corner assignment, verified
// via a standalone reciprocity check (D0-cap[s] and D1-cap[D0CapTargetSlot[s]]
// are exact negations of each other).
static const int3 D0CapOffset[6] = { { 0, -1, 0 }, { 0, 0, -1 }, { 0, 0, -1 }, { -1, 0, 0 }, { -1, 0, 0 }, { 0, -1, 0 } };
static const uint D0CapTargetSlot[6] = { 2, 5, 4, 1, 0, 3 };
static const int3 D1CapOffset[6] = { { 1, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1 } };
static const uint D1CapTargetSlot[6] = { 4, 3, 0, 5, 2, 1 };

// The 4 face-adjacent partners of tet `tetX`: relation 0=fanNext,
// 1=fanPrev (same cube), 2=D0cap, 3=D1cap (neighboring cube, via the tables
// above). Returns false if the partner cube falls outside the current
// grid's bounding box (edge of the domain), same convention CubeLinearIndex
// itself uses.
bool GetFaceAdjacentPartner(uint tetX, uint relation, out uint P)
{
    P = 0u;
    uint cubeLin = tetX / 6u, slot = tetX % 6u;
    if (relation == 0u) { P = cubeLin * 6u + (slot + 1u) % 6u; return true; }
    if (relation == 1u) { P = cubeLin * 6u + (slot + 5u) % 6u; return true; }
    int3 C = CubeOriginFromLinear(cubeLin);
    uint lin;
    if (relation == 2u) {
        if (!CubeLinearIndex(C + D0CapOffset[slot], lin)) return false;
        P = lin * 6u + D0CapTargetSlot[slot];
        return true;
    }
    if (!CubeLinearIndex(C + D1CapOffset[slot], lin)) return false;
    P = lin * 6u + D1CapTargetSlot[slot];
    return true;
}

// Which of a q-space unit cube's 6 tets (see CubeVertexOffsets) contains a
// point given its LOCAL fractional coords within the cube -- the standard
// Kuhn/Freudenthal triangulation of a cube by its main diagonal D0=(0,0,0)-
// D1=(1,1,1): sorting frac's 3 components descending picks one of 6
// orderings, each corresponding to exactly one slot. Matched by hand against
// CubeVertexOffsets/RingBetween for all 6 orderings (e.g. fx>=fy>=fz gives
// the Kuhn tet (0,0,0),(1,0,0),(1,1,0),(1,1,1) -- ring pair (1,0,0),(1,1,0)
// = RingBetween[0],RingBetween[1] = slot 1) -- see the raymarch-lattice
// design notes for the full 6-row derivation. Used to locate a ray's initial
// tet from its q-space entry point; GetTetCornerQs/CubeVertexOffsets etc.
// already assume a (cubeOrigin, slot) pair, this just supplies one from a
// continuous position instead of a dispatched tetIndex.
uint TetSlotFromFrac(float3 f)
{
    bool xy = f.x >= f.y;
    bool yz = f.y >= f.z;
    bool xz = f.x >= f.z;
    if (xy && yz) return 1u;            // fx>=fy>=fz
    if (xy && !yz) return xz ? 0u : 5u; // fx>=fz>=fy : fz>=fx>=fy
    if (!xy && yz) return xz ? 2u : 3u; // fy>=fx>=fz : fy>=fz>=fx
    return 4u;                          // fz>=fy>=fx
}

// Which of the 4 face-adjacency relations (0=fanNext,1=fanPrev,2=D0cap,
// 3=D1cap -- see GetFaceAdjacentPartner above) is crossed when a ray exits
// tet (C,slot) through the face OPPOSITE corner index 0..3 (q0=D0,q1=D1,
// q2/q3=CubeVertexOffsets[slot]). Derived by direct corner-substitution, not
// guessed: D1cap always sits opposite q0 and D0cap always opposite q1
// (both slot-independent, since D0/D1 only ever touch the 2 out-of-cube cap
// relations); which of q2/q3 is fanNext vs fanPrev alternates by slot parity,
// since CubeVertexOffsets[slot][0]/[1] alternately holds RingBetween[slot]
// vs RingBetween[(slot+5)%6] -- verified against CubeVertexOffsets/
// RingBetween for all 6 slots (see the raymarch-lattice design notes).
static const uint ExitCornerToRelation[6][4] = {
    { 3, 2, 1, 0 }, // slot 0 (even)
    { 3, 2, 0, 1 }, // slot 1 (odd)
    { 3, 2, 1, 0 }, // slot 2 (even)
    { 3, 2, 0, 1 }, // slot 3 (odd)
    { 3, 2, 1, 0 }, // slot 4 (even)
    { 3, 2, 0, 1 }, // slot 5 (odd)
};

// Advances a ray-walker's (C,slot) state across the face opposite corner
// index `exitCorner` (0..3) -- the counterpart to GetFaceAdjacentPartner for
// callers tracking a tet as (cube origin, slot) directly rather than a flat
// tetIndex (a pure ray walk never needs a buffer index at all, only
// CubeLinearIndex's domain-bounds check after advancing into a new cube).
// Returns false if the neighboring cube falls outside the addressable
// domain (same convention as CubeLinearIndex/GetFaceAdjacentPartner) --
// C/slot are left unchanged in that case.
bool AdvanceTetAcrossFace(inout int3 C, inout uint slot, uint exitCorner)
{
    uint relation = ExitCornerToRelation[slot][exitCorner];
    if (relation == 0u) { slot = (slot + 1u) % 6u; return true; }
    if (relation == 1u) { slot = (slot + 5u) % 6u; return true; }
    int3 newC = (relation == 2u) ? (C + D0CapOffset[slot]) : (C + D1CapOffset[slot]);
    uint newSlot = (relation == 2u) ? D0CapTargetSlot[slot] : D1CapTargetSlot[slot];
    uint lin;
    if (!CubeLinearIndex(newC, lin)) return false;
    C = newC; slot = newSlot;
    return true;
}

// 2-ring incident-tet gather: this node's own incident tets (ring 1, via
// GatherIncidentTets above) plus every tet FACE-ADJACENT to a ring-1 tet
// (ring 2, via GetFaceAdjacentPartner's 4 relations per tet), each tet
// listed at most once overall. Note ring 2 is NOT "every tet incident to a
// ring-1 corner node" (that would pull in a much wider, node-incidence-based
// set) -- only tets sharing an actual face (3 corners) with a ring-1 tet
// qualify. 3 of a ring-1 tet's 4 face-adjacent partners share the face that
// includes `node` itself, so they're already in ring 1; only the partner
// across the ONE face opposite `node`'s own corner is genuinely new -- so
// ring 2 contributes at most 1 new tet per ring-1 tet in practice (<=~24),
// which is what the 48 cap (MAX_INCIDENT_TETS2, DistanceConfig.hlsli) is
// sized around.
uint GatherIncidentTets2(uint node, out uint tets[MAX_INCIDENT_TETS2])
{
    for (uint z = 0; z < MAX_INCIDENT_TETS2; z++) tets[z] = SENTINEL_LABEL;
    uint count = 0;

    uint ring1[MAX_INCIDENT_TETS];
    uint ring1Count = GatherIncidentTets(node, ring1);
    for (uint r1 = 0; r1 < ring1Count && count < MAX_INCIDENT_TETS2; r1++) tets[count++] = ring1[r1];

    for (uint t = 0; t < ring1Count; t++) {
        for (uint rel = 0; rel < 4; rel++) {
            uint P;
            if (!GetFaceAdjacentPartner(ring1[t], rel, P)) continue;
            bool found = false;
            for (uint e = 0; e < count; e++) if (tets[e] == P) { found = true; break; }
            if (!found && count < MAX_INCIDENT_TETS2) tets[count++] = P;
        }
    }

    return count;
}

// Edge-centric connectivity: the 14 actual geometric neighbors of any node
// (opposite-sublattice near + same-sublattice far), and -- for a given such
// edge -- the tets that actually contain it. Replaces the old per-cube
// TetInterfacePair vote entirely: an "active pair" is now just (this node's
// own winning label, a neighbor's own winning label), read directly, with
// nothing to disagree about. See the design discussion for the derivation.
//
// Offsets derived directly in q-space from the bccToRhombo transform
// (translation-invariant, so the SAME 14 offsets apply whether the center
// node is A or B):
//   8 opposite-sublattice ("near") neighbors: the 6 signed axis unit
//     vectors, plus +-(1,1,1) (exactly the D0<->D1 relationship already
//     used elsewhere in this file) -- always 6 tets per edge.
//   6 same-sublattice ("far") neighbors: +-(1,1,0)-type vectors (sum of two
//     axis vectors) -- always 4 tets per edge.
static const int3 NodeNeighborOffsets[14] = {
    int3(1, 0, 0), int3(-1, 0, 0), int3(0, 1, 0), int3(0, -1, 0), int3(0, 0, 1), int3(0, 0, -1),
    int3(1, 1, 1), int3(-1, -1, -1),
    int3(1, 1, 0), int3(-1, -1, 0), int3(1, 0, 1), int3(-1, 0, -1), int3(0, 1, 1), int3(0, -1, -1)
};

// The 8 corner roles of one cube, in the same order GetTetCornerQs/
// RingBetween use: D0, D1, ring[0..5]. Duplicated here (rather than built
// from D0/D1/RingBetween at HLSL scope) only because HLSL static-const
// array initializers can't reference another array's elements; kept
// byte-for-byte identical to RingBetween's own values.
static const int3 CornerOffsets8[8] = {
    int3(0, 0, 0), int3(1, 1, 1),
    int3(1, 0, 0), int3(1, 1, 0), int3(0, 1, 0), int3(0, 1, 1), int3(0, 0, 1), int3(1, 0, 1)
};

// Which of the 8 corner roles a LOCAL (cube-relative) offset is -- always
// exactly one of the 8 for any offset actually produced by GatherEdgeTets
// below (guaranteed by construction: see its own comment).
int CornerRoleIndex(int3 localOffset)
{
    for (int r = 0; r < 8; r++) if (all(CornerOffsets8[r] == localOffset)) return r;
    return -1; // unreachable for a valid edge offset
}

// Given two corner roles (0=D0, 1=D1, 2..7=ring[0..5]) that are edge-
// adjacent within one cube (guaranteed for the role pairs GatherEdgeTets
// ever passes in), returns which tet slot(s) of that cube share that edge:
// D0-D1 (the main diagonal) is shared by all 6; a D0/D1-to-ring edge (a
// cube edge or its "long" counterpart) by 2; a ring-to-ring edge (the
// hexagonal path around the diagonal) by exactly 1 -- all read directly off
// GetTetCornerQs' own per-slot corner assignment (q2=ring[slot-1],
// q3=ring[slot]), not a new independently-derived rule.
uint EdgeRoleSlots(int roleA, int roleB, out uint slots[6])
{
    if ((roleA == 0 && roleB == 1) || (roleA == 1 && roleB == 0)) {
        for (uint s = 0; s < 6; s++) slots[s] = s;
        return 6;
    }
    if (roleA == 0 || roleA == 1 || roleB == 0 || roleB == 1) {
        int ringIdx = (roleA == 0 || roleA == 1) ? (roleB - 2) : (roleA - 2);
        slots[0] = (uint)ringIdx;
        slots[1] = (uint)((ringIdx + 1) % 6);
        return 2;
    }
    int ra = roleA - 2, rb = roleB - 2;
    slots[0] = (uint)((rb == (ra + 1) % 6) ? rb : ra);
    return 1;
}

// Gathers the (up to 6) global tet indices sharing the edge (Q, Q+D), for D
// one of the 14 NodeNeighborOffsets above. General algorithm, not 14
// hardcoded special cases: for each axis, D_axis==0 means the candidate
// cube origin could be Q_axis-1 OR Q_axis (2 choices, doubling the running
// candidate set); D_axis==+-1 forces exactly one choice. So a diagonal
// offset (0 free axes) yields 1 candidate cube (6 slots -- the whole cube's
// fan); an axis offset (2 free axes) yields 4 candidate cubes (6 tets
// total: 2 contribute a D-ring edge each, 2 contribute a ring-ring edge
// each); a far offset (1 free axis) yields 2 candidate cubes (4 tets total,
// both D-ring edges). Every candidate's two corner roles are guaranteed to
// land in {0,1}^3 (i.e. always one of the 8 known corner roles) by
// construction of how the candidates are generated -- see the design
// discussion for why a "bad" (non-edge, e.g. face-diagonal) role pair can
// never arise for these specific 14 offsets.
uint GatherEdgeTets(int3 Q, int3 D, out uint tets[6])
{
    int3 cand[4] = { Q, Q, Q, Q };
    uint nCand = 1;

    if (D.x == 0) {
        for (uint c = 0; c < nCand; c++) cand[nCand + c] = cand[c];
        for (uint c = 0; c < nCand; c++) cand[c].x -= 1;
        nCand *= 2;
    } else if (D.x == -1) {
        for (uint c = 0; c < nCand; c++) cand[c].x -= 1;
    }
    if (D.y == 0) {
        for (uint c = 0; c < nCand; c++) cand[nCand + c] = cand[c];
        for (uint c = 0; c < nCand; c++) cand[c].y -= 1;
        nCand *= 2;
    } else if (D.y == -1) {
        for (uint c = 0; c < nCand; c++) cand[c].y -= 1;
    }
    if (D.z == 0) {
        for (uint c = 0; c < nCand; c++) cand[nCand + c] = cand[c];
        for (uint c = 0; c < nCand; c++) cand[c].z -= 1;
        nCand *= 2;
    } else if (D.z == -1) {
        for (uint c = 0; c < nCand; c++) cand[c].z -= 1;
    }

    uint count = 0;
    for (uint ci = 0; ci < nCand; ci++) {
        int3 C = cand[ci];
        int roleA = CornerRoleIndex(Q - C);
        int roleB = CornerRoleIndex((Q + D) - C);
        uint slots[6];
        uint nSlots = EdgeRoleSlots(roleA, roleB, slots);
        uint cubeLin;
        if (!CubeLinearIndex(C, cubeLin)) continue; // candidate cube outside the (deliberately oversized) dispatch box
        for (uint si = 0; si < nSlots; si++) tets[count++] = cubeLin * 6 + slots[si];
    }
    return count;
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

// Alien-potential discriminator packing (nodeDiscriminatorBuffer): bit 0 =
// whether discrimination is even meaningful for this node (false for a node
// touching 0 or 1 distinct foreign labels in its neighborhood --
// gatherAlienDiscriminatorCS.hlsl), bits 1-7 = the routed label's own 7-bit
// ID directly. (Earlier version of this encoding stored a single
// distinguishing BIT position + target value instead of the label itself,
// needed only because there was nowhere to directly store a 7-bit label --
// there's ample room, so direct storage is simpler and -- see the phi/beta
// joint-smoothing scheme -- lets a solve that needs the actual routed
// label's VALUE (not just a same/different test against some already-known
// candidate) get it directly, which a bit-test alone can never provide.)
// Bits 8-31 reserved/unused. Shared by the gather pass, the Phase-2 solve
// (smoothnessJacobiAlienCS.hlsl), and the renderer (raymarchLatticePS.hlsl)
// so all three agree on one encoding.
uint EncodeDiscriminator(bool enabled, uint routedLabel)
{
    return (enabled ? 1u : 0u) | ((routedLabel & 0x7Fu) << 1u);
}
void DecodeDiscriminator(uint packed, out bool enabled, out uint routedLabel)
{
    enabled = (packed & 1u) != 0u;
    routedLabel = (packed >> 1u) & 0x7Fu;
}

// True iff queryLabel should route to this node's alien potential (beta)
// rather than its derived default, given this node's own label != queryLabel
// -- callers are responsible for checking the own-label match case first.
bool IsAlienRoute(uint discrimPacked, uint queryLabel)
{
    bool enabled; uint routedLabel;
    DecodeDiscriminator(discrimPacked, enabled, routedLabel);
    return enabled && (queryLabel == routedLabel);
}

// The psi(own)/beta(routed)/gamma(else) corner rule, unconditionally (no
// UseAlienPotential gate -- that's raymarchLatticePS.hlsl's CornerR's own
// concern, a render TOGGLE layered on top of this). Shared by CornerR
// (raymarchLatticePS.hlsl, the main render) and footSlicePS.hlsl's
// "chosen-label field" debug slice -- factored out here rather than
// duplicated a third time.
//
// own label -> pot; discriminator-routed label -> beta; otherwise -> gamma,
// UNCONDITIONALLY (no `enabled` bypass -- see below), via the identity
// exp(-pot) + exp(-beta) - exp(-gamma) = 0, i.e.
// gamma = -ln(exp(-pot)+exp(-beta)) -- a soft-min of (pot,beta). This
// REPLACES the earlier reciprocal identity (1/pot+1/beta+1/gamma=0): that
// formula has a genuine pole at pot+beta==0 (previously worked around via an
// `enabled` bypass, now removed -- no longer needed, see below) and, worse,
// a whole open interval (-pot,-pot/2) where gamma actually EXCEEDS pot -- the
// label-blind fallback
// out-voting the true label owner, a real, provable failure mode. The
// exponential identity has NO such failure mode: log-sum-exp obeys
// `logsumexp(x,y) >= max(x,y)` UNCONDITIONALLY for all real x,y, so
// substituting x=-pot,y=-beta gives `gamma <= min(pot,beta)` always -- no
// case analysis, no pole, no interval to avoid. A disabled corner's inert
// beta=-pot seed degrades GRACEFULLY here instead of catastrophically:
// gamma = -ln(2*cosh(pot)) -> -pot for large pot (recovers the old
// pre-alien-potential fallback), a bounded, sane value near pot=0 (never a
// pole) -- so the `enabled` bypass this function used to need is no longer
// necessary at all; the formula itself already does the right thing whether
// or not this corner is actually routed to something.
float CornerR3WayValue(uint label, float pot, float beta, uint discrim, uint queryLabel)
{
    if (label == queryLabel) return pot;
    if (IsAlienRoute(discrim, queryLabel)) return beta;
    // logsumexp(-pot,-beta), shifted by its max (== -min(pot,beta)) so both
    // exp() arguments stay <=0 -- avoids overflow for large-magnitude
    // negative pot/beta ("deep outside") values; exp(0)=1 guards against
    // underflowing the whole sum to zero.
    float m = min(pot, beta);
    return m - log(exp(m - pot) + exp(m - beta));
}

// "Next" (scratch) discriminator, packed into bits 8-15 of the SAME uint
// word as the current discriminator (bits 0-7 -- see EncodeDiscriminator).
// Mirrors NodeCandidateLabel's existing byte0(current)/byte1(scratch) split:
// needed now that the live-topology alien pass (smoothnessJacobiAlienCS.hlsl)
// can change a node's routing mid-sweep and must defer that change past a
// barrier (see commitAlienCS.hlsl) rather than overwriting the very bits
// other threads in the same dispatch are concurrently reading as neighbor
// data. Bits 0-7 are never touched by this -- only ever updated by the
// commit pass, exactly like the label's byte0.
uint PackDiscriminatorScratch(uint currentWord, bool enabled, uint routedLabel)
{
    uint scratch = ((enabled ? 1u : 0u) | ((routedLabel & 0x7Fu) << 1u)) << 8u;
    return (currentWord & 0xFFFF00FFu) | scratch;
}

// Sentinel used by FaceBetaValidInterval for an unbounded end (either this
// corner's beta doesn't affect the face, or the face is already broken
// independent of this corner) -- comfortably larger than any beta this
// scheme ever produces, so callers can just min()/max() it in
// unconditionally, or compare against it to skip clamping that side.
static const float kBetaBoundUnconstrained = 3.0e8;

// Exact valid interval [lo,hi] for THIS corner's beta -- holding the other
// two corners of a tet face with 3 distinct own-labels (l0,l1,l2) fixed at
// their current AlienCornerR values -- within which the face's triple-point
// (where the l0/l1/l2 confidence fields all tie) stays inside the physical
// triangle rather than sliding off one of its edges. See the approved plan
// for the full derivation; summary:
//
// Map each of the face's 3 corners X to a point in "discriminant space",
// (D01_X, D12_X) = (G(l0,X)-G(l1,X), G(l1,X)-G(l2,X)). The triple-point
// (G(l0)=G(l1)=G(l2)) is exactly the ORIGIN of this space, and its
// barycentric coordinates w.r.t. the face are exactly the barycentric
// coordinates of the origin in the triangle formed by the 3 corners'
// mapped points -- so the endpoint stays on the face iff the origin lies
// inside that triangle (u,v,w all the same sign).
//
// This corner's own (D01,D12) is affine in its beta (slope 1 on whichever
// of l0/l1/l2 its discriminator routes to, else 0 -- own-label and default
// both frozen). The other two corners are constants this sweep. That makes
// two of the three barycentric numerators, v(beta)=Av+Sv*beta and
// w(beta)=Aw+Sw*beta, simple MONOTONIC affine functions of beta (u is the
// third, constant). The valid set is {v matches sign(u)} INTERSECT {w
// matches sign(u)} -- an intersection of two half-lines, which in 1D is
// ALWAYS a single interval (possibly unbounded either end, possibly empty)
// -- never two disjoint pieces. That's what makes combining this across
// every face a node touches a plain, associative interval intersection
// (max of los, min of his), not something needing an ordered/sequential
// combine.
//
// An earlier version of this function assumed the valid range was always
// unbounded BELOW (only ever searching upward from beta=-inf for the first
// break) -- a real tet (309540 in the verification log) proved that false:
// three corners of the same face simultaneously routed can make the valid
// range a genuinely BOUNDED interval, and a corner's beta can fail by being
// too NEGATIVE, not just too positive. Hence: probe all up-to-3 candidate
// sub-intervals explicitly, don't assume which end (if any) is open.
//
// myIntercept0/1/2, mySlope0/1/2: this corner's G(l0),G(l1),G(l2) as
// intercept+slope*beta (slope is 1 for at most one of the three, 0 for the
// rest). otherA0/1/2, otherB0/1/2: the OTHER two face corners' current
// (already-evaluated, frozen this sweep) G(l0),G(l1),G(l2) values.
float2 FaceBetaValidInterval(
    float myIntercept0, float mySlope0,
    float myIntercept1, float mySlope1,
    float myIntercept2, float mySlope2,
    float otherA0, float otherA1, float otherA2,
    float otherB0, float otherB1, float otherB2)
{
    float D01_A = otherA0 - otherA1, D12_A = otherA1 - otherA2;
    float D01_B = otherB0 - otherB1, D12_B = otherB1 - otherB2;

    float A01 = myIntercept0 - myIntercept1, S01 = mySlope0 - mySlope1;
    float A12 = myIntercept1 - myIntercept2, S12 = mySlope1 - mySlope2;

    float u = D01_A * D12_B - D01_B * D12_A; // constant -- doesn't depend on this corner's beta
    float Av = D01_B * A12 - A01 * D12_B, Sv = D01_B * S12 - S01 * D12_B;       // v(beta) = Av + Sv*beta
    float Aw = A01 * D12_A - D01_A * A12, Sw = S01 * D12_A - D01_A * S12;       // w(beta) = Aw + Sw*beta

    float signU = (u >= 0.0) ? 1.0 : -1.0;
    // A face whose OTHER two corners both have near-zero phi (a real,
    // legitimate configuration -- phi=0 is a valid node state) makes D01/D12
    // at those corners near-zero too, which can make Sv/Sw tiny without
    // being exactly zero -- an absolute "> 1e-8" threshold alone still lets
    // -A/S explode to a numerically meaningless multi-million-magnitude
    // "root" (measured directly: node 2234 computed lo=hi=~-75,497,472).
    // Filter on the RESULTING root's magnitude too, not just the slope --
    // this system's betas/phis never legitimately need a boundary anywhere
    // near this cap, so a root beyond it is noise, not a real constraint.
    const float kRootSaneCap = 1.0e5;
    bool hasRv = abs(Sv) > 1.0e-8 && abs(Av / Sv) < kRootSaneCap;
    bool hasRw = abs(Sw) > 1.0e-8 && abs(Aw / Sw) < kRootSaneCap;
    float2 unconstrained = float2(-kBetaBoundUnconstrained, kBetaBoundUnconstrained);

    if (!hasRv && !hasRw) return unconstrained; // beta doesn't move either numerator here

    if (!hasRv || !hasRw)
    {
        // Exactly one root -- two half-lines, probe just below/above it.
        float r = hasRv ? (-Av / Sv) : (-Aw / Sw);
        float scale = max(1.0, abs(r));
        float eps = 1.0e-3 * scale;
        float vLo = Av + Sv * (r - eps), wLo = Aw + Sw * (r - eps);
        float vHi = Av + Sv * (r + eps), wHi = Aw + Sw * (r + eps);
        bool loValid = ((vLo >= 0.0) == (signU > 0.0)) && ((wLo >= 0.0) == (signU > 0.0));
        bool hiValid = ((vHi >= 0.0) == (signU > 0.0)) && ((wHi >= 0.0) == (signU > 0.0));
        if (loValid) return float2(-kBetaBoundUnconstrained, r);
        if (hiValid) return float2(r, kBetaBoundUnconstrained);
        return unconstrained; // face already broken independent of this corner
    }

    // Two roots -- up to 3 candidate sub-intervals; probe each directly
    // rather than assuming which end (if any) is open.
    float ra = -Av / Sv, rb = -Aw / Sw;
    float r0 = min(ra, rb), r1 = max(ra, rb);
    float scale0 = max(1.0, abs(r0)), scale1 = max(1.0, abs(r1));

    float vL = Av + Sv * (r0 - 1.0e-3 * scale0), wL = Aw + Sw * (r0 - 1.0e-3 * scale0);
    float vM = Av + Sv * (0.5 * (r0 + r1)), wM = Aw + Sw * (0.5 * (r0 + r1));
    float vH = Av + Sv * (r1 + 1.0e-3 * scale1), wH = Aw + Sw * (r1 + 1.0e-3 * scale1);
    bool validL = ((vL >= 0.0) == (signU > 0.0)) && ((wL >= 0.0) == (signU > 0.0));
    bool validM = ((vM >= 0.0) == (signU > 0.0)) && ((wM >= 0.0) == (signU > 0.0));
    bool validH = ((vH >= 0.0) == (signU > 0.0)) && ((wH >= 0.0) == (signU > 0.0));

    if (validL && validM && validH) return unconstrained;
    if (!validL && validM && !validH) return float2(r0, r1);
    if (validL && validM && !validH) return float2(-kBetaBoundUnconstrained, r1);
    if (!validL && validM && validH) return float2(r0, kBetaBoundUnconstrained);
    if (validL && !validM && !validH) return float2(-kBetaBoundUnconstrained, r0);
    if (!validL && !validM && validH) return float2(r1, kBetaBoundUnconstrained);
    // validL && !validM && validH: the "two disjoint pieces" pattern the
    // interval proof above rules out -- only reachable via numerical noise
    // right at a root. Treat as unconstrained rather than guess a side.
    return unconstrained;
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
