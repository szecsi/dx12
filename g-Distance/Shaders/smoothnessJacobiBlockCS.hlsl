#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"

// Tile-based, groupshared reimplementation of smoothnessJacobiCS.hlsl's Term 1
// (smoothness) ONLY -- see the approved plan (soft-stargazing-biscuit.md) for
// the full design writeup and the derivation that produced the tables below.
// Terms 2-5 are NOT computed here; this is a standalone, GUI-toggled
// alternative to smoothnessJacobiCS.hlsl for validating/benchmarking this
// architecture, not a drop-in replacement.
//
// One thread group = one 2x2x2(A)+2x2x2(B) = 16-node TARGET tile. Every
// target's actual tet-adjacency neighborhood (own incident tets + every tet
// face-adjacent to those) never reaches beyond the same-sublattice 26-
// neighbor stencil (Chebyshev radius 1) -- verified this session by direct
// enumeration of the rhombohedral corner tables, so a 1-node halo suffices:
// groupshared holds a 4x4x4(A)+4x4x4(B) = 128-node LOAD region, one thread
// per halo node.
//
// ---- The "wedge" geometry (VERIFIED numerically against GetTetCornerQs/
// ResolveCorner via a brute-force Python cross-check, see the plan) ----
// A wedge is ONE Freudenthal tet (not a pair), uniquely identified by an
// A-grid-cube FACE (normal axis + plane position + in-plane cube-column) and
// which of that face's 4 PERIMETER EDGES supplies the tet's 2 A-corners. The
// face's 2 B-apexes (the cube-centers bordering it on either side along its
// own normal axis) are shared by all 4 of its wedges. This is NOT a face-
// diagonal split (an earlier hand-derived guess that turned out wrong) --
// every tet's A-A same-sublattice edge is an ordinary AXIS-aligned cube edge.
//
// Local (halo-relative) coordinates: p in [0,3] = face-plane position along
// its normal axis; c0,c1 in [0,2] = face's cube-column position along the
// other two axes (increasing axis order). wedge-in-face (wif) in [0,3] picks
// one of 4 perimeter edges. Flat wedge index (per the user's spec):
//   wedgeIndex = wif + p*4 + c1*16 + c0*48 + axis*144   (0..431)
//
// Per-wedge corner order used throughout this file: [A0, A1, Bminus, Bplus]
// (cornerIdx 0,1,2,3) -- Bminus = cube at (axis-coord = p-1), Bplus = cube at
// (axis-coord = p), both at in-plane cube-column (c0,c1).
//
// Fixed face-adjacency (every wedge has exactly 4 face-adjacent partners,
// verified against a brute-force 3-corner-subset match):
//   drop A0 -> partner is (axis,p,c0,c1, (wif+1)%4)   [same face, "fanNext"]
//   drop A1 -> partner is (axis,p,c0,c1, (wif+3)%4)   [same face, "fanPrev"]
//   drop Bminus / drop Bplus -> partner crosses to a DIFFERENT face/axis --
//     see CapPartner[] below (also verified against the same brute-force
//     cross-check).
#define SmoothnessBlockSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "CBV(b2)"

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);
RWStructuredBuffer<float> NodePotentialScratch : register(u2);

cbuffer BlockConsts : register(b1) {
    uint RotationOffset; // 0..7, which local A-target is this sweep's li/lj authority (CPU: sweepIndex % 8)
};

#define HALO_DIM 4u
#define HALO_NODES 128u // 64 A + 64 B
#define WEDGE_COUNT 432u

groupshared uint  gsmLabel[HALO_NODES * 2u];   // packed candidate labels, same layout as NodeCandidateLabel
groupshared float gsmPot[HALO_NODES * 8u];     // candidate potentials, MAX_CANDIDATES per node
// false = this halo slot is outside the REAL grid domain (GridRes/BDim) --
// i.e. a virtual node exactly like ResolveCorner's SENTINEL_LABEL case, not
// merely outside this tile's halo window. Needed so GsmCornerPotential can
// apply the same "label 0 -> potential 1.0... " convention
// GetCornerPotential/DistanceLattice.hlsli uses for a virtual corner, instead
// of silently falling through to missingFallback for every label.
groupshared bool  gsmRealNode[HALO_NODES];
groupshared float3 gsmWedgeG[WEDGE_COUNT];     // per-wedge (li,lj) field gradient: TetFieldGrad(li)-TetFieldGrad(lj)
groupshared bool  gsmWedgeValid[WEDGE_COUNT];  // false if the wedge touches a corner outside this tile's halo window
groupshared uint  gsmLi;
groupshared uint  gsmLj;

uint HaloIdxA(uint3 l) { return l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }
uint HaloIdxB(uint3 l) { return 64u + l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }

uint GsmLabelAt(uint haloIdx, uint slot) {
    return (gsmLabel[haloIdx * 2u + slot / 4u] >> ((slot % 4u) * 8u)) & 0xFFu;
}

// Mirrors DistanceLattice.hlsli's GetCornerPotential exactly, but reading
// groupshared halo data. Two distinct "no real data here" cases, both
// treated the same way GetCornerPotential treats a SENTINEL_LABEL corner
// (label==0 -> 0.0, else missingFallback): haloIdx<0 (outside this tile's
// halo window -- see gsmWedgeValid) and gsmRealNode[haloIdx]==false (inside
// the halo window but outside the REAL grid domain, GridRes/BDim).
float GsmCornerPotential(int haloIdx, uint label, float missingFallback) {
    if (haloIdx < 0 || !gsmRealNode[(uint)haloIdx]) return (label == 0u) ? 0.0 : missingFallback;
    uint h = (uint)haloIdx;
    for (uint s = 0; s < MAX_CANDIDATES; s++)
        if (GsmLabelAt(h, s) == label) return gsmPot[h * 8u + s];
    return missingFallback;
}

// -- axis helpers: the two in-plane axes for a given face normal, in
// increasing order (matches the Python derivation's axes_for()) --
uint2 InPlaneAxes(uint axis) {
    if (axis == 0u) return uint2(1, 2);
    if (axis == 1u) return uint2(0, 2);
    return uint2(0, 1);
}

// Perimeter-edge corner offsets (du,dv) in the face's own (u,v) local frame,
// matching PERIM_EDGES from the verified derivation script exactly.
static const int2 PerimEdgeA0[4] = { int2(0, 0), int2(1, 0), int2(1, 1), int2(0, 1) };
static const int2 PerimEdgeA1[4] = { int2(1, 0), int2(1, 1), int2(0, 1), int2(0, 0) };

// Resolves a wedge's 4 corners to halo-local integer grid coordinates (A0,A1
// as A-index, Bminus/Bplus as CUBE ORIGIN -- B halo index equals cube origin
// directly since B(i,j,k) sits at the center of the A-cube with that same
// origin). Returns false (via the `valid` out param per corner) for any
// corner that falls outside the [0,3]^3 halo -- defensive; the tile+halo
// sizing already guarantees this never happens for any wedge actually
// reachable from a real target, but every out-of-halo case is handled the
// same way GetCornerPotential treats a virtual corner (haloIdx<0 path above).
void WedgeCornersLocal(uint axis, uint p, uint c0, uint c1, uint wif,
                        out int haloA0, out int haloA1, out int haloBm, out int haloBp)
{
    uint2 uv = InPlaneAxes(axis);
    int3 c0v = int3(0, 0, 0); c0v[axis] = (int)p; c0v[uv.x] = (int)c0; c0v[uv.y] = (int)c1;

    int3 a0 = c0v; a0[uv.x] += PerimEdgeA0[wif].x; a0[uv.y] += PerimEdgeA0[wif].y;
    int3 a1 = c0v; a1[uv.x] += PerimEdgeA1[wif].x; a1[uv.y] += PerimEdgeA1[wif].y;

    int3 bmOrigin = c0v; bmOrigin[axis] -= 1; // side 0: cube just before the face plane
    int3 bpOrigin = c0v;                      // side 1: cube just at/after the face plane

    bool inA0 = all(a0 >= 0) && all(a0 < (int)HALO_DIM);
    bool inA1 = all(a1 >= 0) && all(a1 < (int)HALO_DIM);
    bool inBm = all(bmOrigin >= 0) && all(bmOrigin < (int)HALO_DIM);
    bool inBp = all(bpOrigin >= 0) && all(bpOrigin < (int)HALO_DIM);

    haloA0 = inA0 ? (int)HaloIdxA((uint3)a0) : -1;
    haloA1 = inA1 ? (int)HaloIdxA((uint3)a1) : -1;
    haloBm = inBm ? (int)HaloIdxB((uint3)bmOrigin) : -1;
    haloBp = inBp ? (int)HaloIdxB((uint3)bpOrigin) : -1;
}

float3 WedgeCornerPos(uint axis, uint p, uint c0, uint c1, uint wif, uint cornerIdx, int3 tileOriginLocalMin)
{
    // Real world position of one of the 4 corners, in the SAME world space
    // APos/BPos use (local halo coords + tileOriginLocalMin, since the halo's
    // local (0,0,0) is tileOriginLocalMin in real A-index space).
    uint2 uv = InPlaneAxes(axis);
    int3 c0v = int3(0, 0, 0); c0v[axis] = (int)p; c0v[uv.x] = (int)c0; c0v[uv.y] = (int)c1;
    if (cornerIdx == 0u) { c0v[uv.x] += PerimEdgeA0[wif].x; c0v[uv.y] += PerimEdgeA0[wif].y; return APos(c0v + tileOriginLocalMin); }
    if (cornerIdx == 1u) { c0v[uv.x] += PerimEdgeA1[wif].x; c0v[uv.y] += PerimEdgeA1[wif].y; return APos(c0v + tileOriginLocalMin); }
    if (cornerIdx == 2u) { c0v[axis] -= 1; return BPos(c0v + tileOriginLocalMin); }
    return BPos(c0v + tileOriginLocalMin); // cornerIdx==3 (Bplus)
}

// TetShapeGradients wants the 4 corners in [A0,A1,Bminus,Bplus] order to
// match this file's own cornerIdx convention (0,1,2,3) -- geometry only,
// recomputed on demand (cheap: a few cross products) rather than cached, to
// keep groupshared usage down to one float3 (the label-dependent gradient
// `g`) per wedge instead of five.
void WedgeShapeGrads(uint axis, uint p, uint c0, uint c1, uint wif, int3 tileOriginLocalMin, out float3 w[4])
{
    float3 P[4];
    P[0] = WedgeCornerPos(axis, p, c0, c1, wif, 0, tileOriginLocalMin);
    P[1] = WedgeCornerPos(axis, p, c0, c1, wif, 1, tileOriginLocalMin);
    P[2] = WedgeCornerPos(axis, p, c0, c1, wif, 2, tileOriginLocalMin);
    P[3] = WedgeCornerPos(axis, p, c0, c1, wif, 3, tileOriginLocalMin);
    TetShapeGradients(P[0], P[1], P[2], P[3], w[0], w[1], w[2], w[3]);
}

uint WedgeIndex(uint axis, uint p, uint c0, uint c1, uint wif) {
    return wif + p * 4u + c1 * 16u + c0 * 48u + axis * 144u;
}

// Fixed cross-face ("cap") adjacency table -- dropping Bminus (relation 0) or
// Bplus (relation 1) -- as {realDelta.xyz, newAxis, newWif}, verified against
// a brute-force 3-corner-subset match over a wide window (see the plan). Real
// delta is applied to the SOURCE face's own anchor (coord[axis]=p,
// coord[u]=c0, coord[v]=c1) to get the partner face's anchor, then decomposed
// via the PARTNER's own axis to get its (p,c0,c1).
struct CapEntry { int3 delta; uint newAxis; uint newWif; };

static const CapEntry CapTable[3][4][2] = {
    { // axis 0
        { {int3( 0,0,0),2,3}, {int3(-1,0,0),2,1} }, // wif0: dropBm, dropBp
        { {int3( 0,1,0),1,3}, {int3(-1,1,0),1,1} }, // wif1
        { {int3( 0,0,1),2,3}, {int3(-1,0,1),2,1} }, // wif2
        { {int3( 0,0,0),1,3}, {int3(-1,0,0),1,1} }, // wif3
    },
    { // axis 1
        { {int3( 0,0,0),2,0}, {int3(0,-1,0),2,2} },
        { {int3( 1,0,0),0,3}, {int3(1,-1,0),0,1} },
        { {int3( 0,0,1),2,0}, {int3(0,-1,1),2,2} },
        { {int3( 0,0,0),0,3}, {int3(0,-1,0),0,1} },
    },
    { // axis 2
        { {int3( 0,0,0),1,0}, {int3(0,0,-1),1,2} },
        { {int3( 1,0,0),0,0}, {int3(1,0,-1),0,2} },
        { {int3( 0,1,0),1,0}, {int3(0,1,-1),1,2} },
        { {int3( 0,0,0),0,0}, {int3(0,0,-1),0,2} },
    },
};

// Resolves one of a wedge's 4 face-adjacent partners. relation: 0=fanNext
// (drop A0), 1=fanPrev (drop A1), 2=cap via dropped Bminus, 3=cap via dropped
// Bplus. Returns false if the partner's face falls outside the local
// [0,3]/[0,2] halo window (only possible for wedges at the very edge of the
// halo, never for any wedge actually incident to a real target).
bool GetWedgePartner(uint axis, uint p, uint c0, uint c1, uint wif, uint relation,
                      out uint pAxis, out uint pP, out uint pC0, out uint pC1, out uint pWif)
{
    if (relation == 0u) { pAxis = axis; pP = p; pC0 = c0; pC1 = c1; pWif = (wif + 1u) % 4u; return true; }
    if (relation == 1u) { pAxis = axis; pP = p; pC0 = c0; pC1 = c1; pWif = (wif + 3u) % 4u; return true; }

    CapEntry e = CapTable[axis][wif][relation - 2u];
    uint2 uv = InPlaneAxes(axis);
    int3 anchor = int3(0, 0, 0); anchor[axis] = (int)p; anchor[uv.x] = (int)c0; anchor[uv.y] = (int)c1;
    int3 partnerAnchor = anchor + e.delta;

    pAxis = e.newAxis;
    pWif = e.newWif;
    uint2 uv2 = InPlaneAxes(pAxis);
    int pi = partnerAnchor[pAxis], c0i = partnerAnchor[uv2.x], c1i = partnerAnchor[uv2.y];
    if (pi < 0 || pi > 3 || c0i < 0 || c0i > 2 || c1i < 0 || c1i > 2) { pP = 0; pC0 = 0; pC1 = 0; return false; }
    pP = (uint)pi; pC0 = (uint)c0i; pC1 = (uint)c1i;
    return true;
}

// -- per-A-target incidence pattern (verified: 24 wedges, 8 per axis) --
// For a target A-node at local axis-coordinate `own`, and in-plane
// coordinates (ownU,ownV): the 4 in-plane cube-columns bordering it are
// (ownU+du, ownV+dv) for (du,dv) in TargetPattern[]; each contributes 2
// (wif,cornerIdx) entries.
static const int2 TargetPatternDUV[4] = { int2(-1,-1), int2(-1,0), int2(0,-1), int2(0,0) };
static const uint2 TargetPatternWifCorner[4][2] = { // [duv-index][which of 2] = (wif,cornerIdx)
    { uint2(1,1), uint2(2,0) },
    { uint2(0,1), uint2(1,0) },
    { uint2(2,1), uint2(3,0) },
    { uint2(0,0), uint2(3,1) },
};

[RootSignature(SmoothnessBlockSig)]
[numthreads(128, 1, 1)]
void smoothnessJacobiBlockCS(uint3 gid : SV_GroupID, uint GI : SV_GroupIndex)
{
    // Tile's own A-target origin, in real A-index space; halo covers
    // [tileOriginA-1, tileOriginA+2]. B-target block uses the SAME local
    // index range (see the plan's tile-layout section).
    int3 tileOriginA = int3(gid) * 2;
    int3 haloOriginA = tileOriginA - 1;

    // ---- load: one thread per halo node ----
    {
        uint3 l = uint3(GI % HALO_DIM, (GI / HALO_DIM) % HALO_DIM, GI / (HALO_DIM * HALO_DIM));
        int3 aIdx3 = haloOriginA + (int3)l;
        bool aValid = all(aIdx3 >= 0) && all(aIdx3 < (int)GridRes);
        uint haloA = HaloIdxA(l);
        gsmRealNode[haloA] = aValid;
        if (aValid) {
            uint node = AIdx((uint)aIdx3.x, (uint)aIdx3.y, (uint)aIdx3.z);
            gsmLabel[haloA * 2u + 0u] = NodeCandidateLabel[node * 2u + 0u];
            gsmLabel[haloA * 2u + 1u] = NodeCandidateLabel[node * 2u + 1u];
            for (uint s = 0; s < MAX_CANDIDATES; s++) gsmPot[haloA * 8u + s] = NodePotential[node * MAX_CANDIDATES + s];
        } else {
            gsmLabel[haloA * 2u + 0u] = 0xFFFFFFFFu; // all SENTINEL_CANDIDATE
            gsmLabel[haloA * 2u + 1u] = 0xFFFFFFFFu;
            for (uint s = 0; s < MAX_CANDIDATES; s++) gsmPot[haloA * 8u + s] = 0.0;
        }

        bool bValid = all(aIdx3 >= 0) && all(aIdx3 < (int)BDim); // B halo shares the same local index range as A
        uint haloB = HaloIdxB(l);
        gsmRealNode[haloB] = bValid;
        if (bValid) {
            uint node = BIdx((uint)aIdx3.x, (uint)aIdx3.y, (uint)aIdx3.z);
            gsmLabel[haloB * 2u + 0u] = NodeCandidateLabel[node * 2u + 0u];
            gsmLabel[haloB * 2u + 1u] = NodeCandidateLabel[node * 2u + 1u];
            for (uint s = 0; s < MAX_CANDIDATES; s++) gsmPot[haloB * 8u + s] = NodePotential[node * MAX_CANDIDATES + s];
        } else {
            gsmLabel[haloB * 2u + 0u] = 0xFFFFFFFFu;
            gsmLabel[haloB * 2u + 1u] = 0xFFFFFFFFu;
            for (uint s = 0; s < MAX_CANDIDATES; s++) gsmPot[haloB * 8u + s] = 0.0;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // ---- Stage 1: tile-wide (li,lj) = the winner/runner-up of ONE rotating
    // authority A-target (RotationOffset selects which of the 8 local
    // A-targets), found via WaveActiveMax across threads 0-7 (assumes lanes
    // 0-7 share a wave -- true for every current wave size: 32/64/etc all
    // start a new group's lane numbering at 0). NOT a per-node pair, NOT a
    // cross-node vote -- explicit user decision, see the plan. ----
    if (GI < 8u) {
        uint3 authLocal = uint3((RotationOffset >> 0) & 1u, (RotationOffset >> 1) & 1u, (RotationOffset >> 2) & 1u) + 1u;
        uint haloA = HaloIdxA(authLocal);
        float myPot = gsmPot[haloA * 8u + GI];
        uint myLabel = GsmLabelAt(haloA, GI);
        if (myLabel == SENTINEL_CANDIDATE) myPot = -1.0e30;

        float maxPot = WaveActiveMax(myPot);
        if (myPot == maxPot) { gsmLi = myLabel; }
        float myPot2 = (myPot == maxPot) ? -1.0e30 : myPot; // knock the winner out for the runner-up pass
        float maxPot2 = WaveActiveMax(myPot2);
        if (myPot2 == maxPot2) { gsmLj = myLabel; }
    }
    GroupMemoryBarrierWithGroupSync();
    uint li = gsmLi;
    uint lj = gsmLj;

    // ---- Stage 2: precompute all 432 wedges' (li,lj) field gradient ----
    for (uint w = GI; w < WEDGE_COUNT; w += 128u) {
        uint axis = w / 144u;
        uint rem144 = w % 144u;
        uint c0 = rem144 / 48u;
        uint rem48 = rem144 % 48u;
        uint c1 = rem48 / 16u;
        uint rem16 = rem48 % 16u;
        uint p = rem16 / 4u;
        uint wif = rem16 % 4u;

        int haloA0, haloA1, haloBm, haloBp;
        WedgeCornersLocal(axis, p, c0, c1, wif, haloA0, haloA1, haloBm, haloBp);

        gsmWedgeValid[w] = (haloA0 >= 0) && (haloA1 >= 0) && (haloBm >= 0) && (haloBp >= 0);
        if (!gsmWedgeValid[w]) { gsmWedgeG[w] = float3(0, 0, 0); continue; }

        float3 wgrad[4];
        WedgeShapeGrads(axis, p, c0, c1, wif, haloOriginA, wgrad);

        int haloRefs[4] = { haloA0, haloA1, haloBm, haloBp };
        float3 g = float3(0, 0, 0);
        for (uint c = 0; c < 4; c++) {
            float phiLi = GsmCornerPotential(haloRefs[c], li, MissingFallback);
            float phiLj = GsmCornerPotential(haloRefs[c], lj, MissingFallback);
            g += (phiLi - phiLj) * wgrad[c];
        }
        gsmWedgeG[w] = g;
    }
    GroupMemoryBarrierWithGroupSync();

    // ---- Stage 3: per-target accumulation, no gathers -- each of the 16
    // targets directly indexes its own 24 incident wedges (fixed pattern)
    // and each wedge's 4 fixed face-adjacent partners. ----
    if (GI < 16u) {
        bool isB = GI >= 8u;
        uint localIdx = GI & 7u;
        uint3 duv = uint3(localIdx & 1u, (localIdx >> 1) & 1u, (localIdx >> 2) & 1u);
        uint3 targetLocal = duv + 1u; // A-target local coords in [1,2]^3 (B-target uses the same local range)

        int3 targetA = haloOriginA + (int3)targetLocal;
        bool targetValid = isB
            ? (all(targetA >= 0) && all(targetA < (int)BDim))
            : (all(targetA >= 0) && all(targetA < (int)GridRes));
        if (!targetValid) return;

        uint node = isB ? BIdx((uint)targetA.x, (uint)targetA.y, (uint)targetA.z)
                        : AIdx((uint)targetA.x, (uint)targetA.y, (uint)targetA.z);
        uint haloIdx = isB ? HaloIdxB(targetLocal) : HaloIdxA(targetLocal);

        int siLi = -1, siLj = -1;
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            uint l = GsmLabelAt(haloIdx, s);
            if (l == li) siLi = (int)s;
            if (l == lj) siLj = (int)s;
        }

        float gradLi = 0.0, diagLi = 0.0, gradLj = 0.0, diagLj = 0.0;

        // Enumerate this target's 24 own incident wedges. A-targets use the
        // "4 bordering face-columns x 2 perimeter edges" pattern
        // (TargetPatternDUV/WifCorner, verified against node (2,2,2)'s own
        // 24-wedge incidence list). B-targets use a DIFFERENT, simpler
        // pattern (also verified): fixed face-column (own_u,own_v) = its own
        // cube-origin in-plane coords, ALL 4 wif of each of the 2 faces it's
        // an apex of (p=own_axis as Bplus/cornerIdx3, p=own_axis+1 as
        // Bminus/cornerIdx2).
        for (uint axis = 0; axis < 3; axis++) {
            uint2 uv = InPlaneAxes(axis);
            uint own_p = (uint)targetLocal[axis];
            uint own_u = (uint)targetLocal[uv.x];
            uint own_v = (uint)targetLocal[uv.y];

            uint numPat = isB ? 2u : 4u;
            for (uint pat = 0; pat < numPat; pat++) {
                uint c0, c1;
                if (isB) {
                    c0 = own_u; c1 = own_v; // B's own cube-column, fixed
                } else {
                    int c0i = (int)own_u + TargetPatternDUV[pat].x;
                    int c1i = (int)own_v + TargetPatternDUV[pat].y;
                    if (c0i < 0 || c0i > 2 || c1i < 0 || c1i > 2) continue; // outside halo's face-column range
                    c0 = (uint)c0i; c1 = (uint)c1i;
                }

                uint usedP = isB ? (pat == 0u ? own_p : own_p + 1u) : own_p; // Bplus face then Bminus face
                if (isB && (usedP > 3u)) continue; // out of halo's p range (shouldn't happen for a real target)
                uint bCorner = (pat == 0u) ? 3u : 2u; // Bplus=cornerIdx3, Bminus=cornerIdx2

                uint kCount = isB ? 4u : 2u;
                for (uint k = 0; k < kCount; k++) {
                    uint wif = isB ? k : TargetPatternWifCorner[pat][k].x;
                    uint myCorner = isB ? bCorner : TargetPatternWifCorner[pat][k].y;

                    uint srcW = WedgeIndex(axis, usedP, c0, c1, wif);
                    if (!gsmWedgeValid[srcW]) continue;
                    float3 wSrc[4]; WedgeShapeGrads(axis, usedP, c0, c1, wif, haloOriginA, wSrc);
                    float3 deltaWA = wSrc[myCorner];

                    for (uint rel = 0; rel < 4; rel++) {
                        uint pAxis, pP, pC0, pC1, pWif;
                        if (!GetWedgePartner(axis, usedP, c0, c1, wif, rel, pAxis, pP, pC0, pC1, pWif)) continue;
                        uint dstW = WedgeIndex(pAxis, pP, pC0, pC1, pWif);
                        if (!gsmWedgeValid[dstW]) continue;

                        // deltaWB: this target's shape-gradient role in the
                        // partner, if it's also one of the partner's 4
                        // corners (direct 4-way position compare -- mirrors
                        // AccumulateEdgePair's own `cA=(refA[c]==node)?...`,
                        // not a search).
                        float3 wDst[4]; WedgeShapeGrads(pAxis, pP, pC0, pC1, pWif, haloOriginA, wDst);
                        float3 myPos = WedgeCornerPos(axis, usedP, c0, c1, wif, myCorner, haloOriginA);
                        float3 deltaWB = float3(0, 0, 0);
                        bool foundOnOtherSide = false;
                        for (uint dc = 0; dc < 4; dc++) {
                            float3 dp = WedgeCornerPos(pAxis, pP, pC0, pC1, pWif, dc, haloOriginA);
                            if (all(abs(dp - myPos) < 1.0e-5)) { deltaWB = wDst[dc]; foundOnOtherSide = true; break; }
                        }

                        // Double-discovery correction (mirrors
                        // AccumulateEdgePair's pairWeight): this exact pair
                        // is re-found from the partner's own side too iff
                        // the partner ALSO has `node` as a corner -- which is
                        // always one of this SAME target's own 24-wedge scan
                        // entries, since the partner's relevant corner IS
                        // `node`. So: weight 0.5 whenever the partner also
                        // contains this target (found twice, once from each
                        // side), 1.0 otherwise (only this side touches it).
                        float pairWeight = foundOnOtherSide ? 0.5 : 1.0;

                        float3 diff = gsmWedgeG[srcW] - gsmWedgeG[dstW];
                        float3 K = deltaWA - deltaWB;
                        float dK = dot(diff, K);
                        float kk = dot(K, K);
                        float wgt = pairWeight * 2.0 * SmoothnessWeight;

                        if (siLi >= 0) { gradLi += wgt * dK; diagLi += wgt * kk; }
                        if (siLj >= 0) { gradLj += -wgt * dK; diagLj += wgt * kk; }
                    }
                }
            }
        }

        // Write scratch: li/lj get the Term-1 Jacobi step, every other slot
        // is copied through unchanged (this kernel doesn't touch Terms 2-5,
        // and commitPotentialCS reads scratch for every node/slot).
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            float phi = gsmPot[haloIdx * 8u + s];
            float newPhi = phi;
            if ((int)s == siLi) {
                float step = clamp(-gradLi / (diagLi + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
                newPhi = phi + step;
            } else if ((int)s == siLj) {
                float step = clamp(-gradLj / (diagLj + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
                newPhi = phi + step;
            }
            NodePotentialScratch[node * MAX_CANDIDATES + s] = newPhi;
        }
    }
}
