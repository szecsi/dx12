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
#define WEDGE_COUNT 324u

groupshared uint  lijSlots[HALO_NODES];   // packed candidate labels slots in their respective nodes
groupshared float lijPot[HALO_NODES * 2u];     // candidate potentials, 2 per node
groupshared float3 wedgeGrad[WEDGE_COUNT];     // per-wedge (li,lj) field gradient: TetFieldGrad(li)-TetFieldGrad(lj)
groupshared uint  li;
groupshared uint  lj;

uint AIdx(uint i, uint j, uint k) { return i + j * GridRes + k * GridRes * GridRes; }
uint BIdxLocal(uint i, uint j, uint k) { return i + j * BDim + k * BDim * BDim; }
uint BIdx(uint i, uint j, uint k) { return ACount + BIdxLocal(i, j, k); }
        
uint posToIdxA(uint3 l) { return AIdx(l.x, l.y, l.z); }
uint posToIdxB(uint3 l) { return BIdx(l.x, l.y, l.z); }
        
uint inHaloPosToInHaloIdxA(uint3 l) { return l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }
uint inHaloPosToInHaloIdxB(uint3 l) { return 64u + l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }

uint gsmLabelAt(uint haloIdx, uint slot) {
    return (gsmLabel[haloIdx * 2u + slot / 4u] >> ((slot % 4u) * 8u)) & 0xFFu;
}

float getPotentialByLabel(uint haloIdx, uint label) {
    for (uint s = 0; s < MAX_CANDIDATES; s++)
        if (gsmLabelAt(haloIdx, s) == label) return gsmPot[haloIdx * 8u + s];
    return -3.0; // must be about this far from interface to have no record
}

// -- axis helpers: the two in-plane axes for a given face normal, in
// increasing order (matches the Python derivation's axes_for()) --
uint2 inPlaneAxes(uint axis) {
    if (axis == 0u) return uint2(1, 2);
    if (axis == 1u) return uint2(2, 0);
    return uint2(0, 1);
}

// Resolves a wedge's 4 corners to halo-local integer grid coordinates (A0,A1
// as A-index, Bminus/Bplus as CUBE ORIGIN -- B halo index equals cube origin
// directly since B(i,j,k) sits at the center of the A-cube with that same
// origin). Returns false (via the `valid` out param per corner) for any
// corner that falls outside the [0,3]^3 halo -- defensive; the tile+halo
// sizing already guarantees this never happens for any wedge actually
// reachable from a real target, but every out-of-halo case is handled the
// same way GetCornerPotential treats a virtual corner (haloIdx<0 path above).
uint4 getWedgeCornersInHalo(uint axis, uint x, uint y, uint z, uint wif)
{
    uint3 b0 = uint3(x, y, z);
    uint3 b1 = uint3(x, y, z+1);
    uint3 a0 = b0 + uint3(wif/2, wif%2, z+1);
    uint3 a1 = b1 + uint3(1-(wif%2), wif/2, z+1);    

    if(axis==1){
        b0 = b0.zxy;
        b1 = b1.zxy;
        a0 = a0.zxy;
        a1 = a1.zxy;
    } else if(axis == 0){
        b0 = b0.yzx;
        b1 = b1.yzx;
        a0 = a0.yzx;
        a1 = a1.yzx;                
    }

    return uint4(
        inHaloPosToInHaloIdxA(a0),
        inHaloPosToInHaloIdxA(a1),
        inHaloPosToInHaloIdxB(b0),
        inHaloPosToInHaloIdxB(b1)
    );
}

uint wedgeIndex(uint axis, uint x, uint y, uint z, uint wif) {
    return wif + z * 4u + y * 12u + x * 36u + axis * 108u;
}

[RootSignature(SmoothnessBlockSig)]
[numthreads(128, 1, 1)]
void smoothnessJacobiBlockCS(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex)
{
    // Tile's own A-target origin, in real A-index space; halo covers
    // [tileOriginA-1, tileOriginA+2]. B-target block uses the SAME local
    // index range (see the plan's tile-layout section).
    uint3 haloOriginA = gid * 2;
    uint3 tileOriginA = haloOriginA + 1;

    // ---- Stage 1: tile-wide (li,lj) = the winner/runner-up of ONE rotating
    // authority A-target (RotationOffset selects which of the 8 local
    // A-targets), found via WaveActiveMax across threads 0-7 (assumes lanes
    // 0-7 share a wave -- true for every current wave size: 32/64/etc all
    // start a new group's lane numbering at 0). NOT a per-node pair, NOT a
    // cross-node vote -- explicit user decision, see the plan. ----            
    if (tid < 8){            
        uint3 authA =  haloOriginA +
                uint3((RotationOffset >> 0) & 1u, (RotationOffset >> 1) & 1u, (RotationOffset >> 2) & 1u)
                + 1u;
        uint authIdx = posToIdxA(authA);
        float myPot = NodePotential[authIdx * MAX_CANDIDATES + tid];
        uint myLabel = (NodeCandidateLabel[authIdx * 2u + tid / 4u] >> ((tid % 4u) * 8u)) & 0xFFu;
        myLabel = myLabel | myLabel << 8u; // 16-bit label, repeated in both bytes
        myLabel = myLabel | myLabel << 16u; // 32-bit label, repeated four times
        float maxPot = WaveActiveMax(myPot);
        if (myPot == maxPot) { li = myLabel; }
        float myPot2 = (myPot == maxPot) ? -1.0e30 : myPot; // knock the winner out for the runner-up pass
        float maxPot2 = WaveActiveMax(myPot2);
        if (myPot2 == maxPot2) { lj = myLabel; }
    }               
    GroupMemoryBarrierWithGroupSync();

    // mine slots and potentials for Li, Lj
    {
        uint3 inHaloPosA = uint3(tid % HALO_DIM, (tid / HALO_DIM) % HALO_DIM, tid / (HALO_DIM * HALO_DIM));
        uint3 posA = haloOriginA + inHaloPosA;
        uint inHaloIdx = inHaloPosToInHaloIdxA(inHaloPosA);
        uint idx = posToIdxA(posA);
        uint candidates0to3 = NodeCandidateLabel[idx * 2u + 0u];
        uint candidates4to7 = NodeCandidateLabel[idx * 2u + 1u];
        uint maski0to3 = ~(candidates0to3 ^ li);
        maski0to3 =  maski0to3 & (maski0to3 >> 4u);
        maski0to3 =  maski0to3 & (maski0to3 >> 2u);
        maski0to3 = (maski0to3 & (maski0to3 >> 1u)) & 0x01010101u;
        uint maski4to7 = ~(candidates4to7 ^ li);
        maski4to7 = maski4to7 & (maski4to7 >> 4u);
        maski4to7 = maski4to7 & (maski4to7 >> 2u);
        maski4to7 = (maski4to7 & (maski4to7 >> 1u)) & 0x01010101u;
        uint iSlot = ((maski0to3 != 0u) ? (uint) firstbithigh(maski0to3) : (uint) firstbithigh(maski4to7) + 4u) / 8u;

        uint maskj0to3 = ~(candidates0to3 ^ lj);
        maskj0to3 =  maskj0to3 & (maskj0to3 >> 4u);
        maskj0to3 =  maskj0to3 & (maskj0to3 >> 2u);
        maskj0to3 = (maskj0to3 & (maskj0to3 >> 1u)) & 0x01010101u;
        uint maskj4to7 = ~(candidates4to7 ^ lj);
        maskj4to7 = maskj4to7 & (maskj4to7 >> 4u);
        maskj4to7 = maskj4to7 & (maskj4to7 >> 2u);
        maskj4to7 = (maskj4to7 & (maskj4to7 >> 1u)) & 0x01010101u;
        uint jSlot = ((maskj0to3 != 0u) ? (uint) firstbithigh(maskj0to3) : (uint) firstbithigh(maskj4to7) + 4u) / 8u;

        lijSlots[inHaloIdx] = iSlot << 16u | jSlot;
        lijPot[inHaloIdx * 2u + 0u] = NodePotential[idx * MAX_CANDIDATES + iSlot];
        lijPot[inHaloIdx * 2u + 1u] = NodePotential[idx * MAX_CANDIDATES + jSlot]; // Initialize to zero
    }
    GroupMemoryBarrierWithGroupSync();
                
    uint lli = li;
    uint llj = lj;

    // ---- Stage 2: precompute all WEDGE_COUNT wedges' (li,lj) field gradient ----
    for (uint w = tid; w < WEDGE_COUNT; w += 128u) {
        uint axis = w / 108u;
        uint rem108= w % 108;
        uint x = rem108 / 36u;
        uint rem36 = rem108 % 36u;
        uint y = rem36 / 12u;
        uint rem12 = rem36 % 12u;
        uint z = rem12 / 4u;
        uint wif = rem12 % 4u;

        uint haloA0, haloA1, haloBm, haloBp;
        uint4 wedgeCorners = getWedgeCornersInHalo(axis, z, y, x, wif);
        
        float4 wdiff = float4(
            lijPot[wedgeCorners.x * 2u + 0u] - lijPot[wedgeCorners.x * 2u + 1u],
            lijPot[wedgeCorners.y * 2u + 0u] - lijPot[wedgeCorners.y * 2u + 1u],
            lijPot[wedgeCorners.z * 2u + 0u] - lijPot[wedgeCorners.z * 2u + 1u],
            lijPot[wedgeCorners.w * 2u + 0u] - lijPot[wedgeCorners.w * 2u + 1u]
        );
        float3 g = float3(wdiff.y - wdiff.x, wdiff.w + wdiff.z - wdiff.y - wdiff.x, wdiff.w - wdiff.z);
        // keep in wedge local                
        //if (axis == 0)
        //    g = g.yzx;
        //else if (axis == 1)
        //    g = g.zxy;
        //        
        wedgeGrad[w] = g;
    }
    GroupMemoryBarrierWithGroupSync();

    // ---- Stage 3: per-target accumulation
    uint iWarp = tid / 32u;
    for(uint iTarget = iWarp; iTarget < 16; iTarget += 4u )
    {
        uint wid = tid % 32u;
        if (wid < 24)
        {
            uint3 targetPosInHalo = uint3(iTarget & 1u, (iTarget >> 1) & 1u, (iTarget >> 2) & 1u) + 1;
            uint offForB = (iTarget >> 3) * HALO_DIM * HALO_DIM;
            uint srcAxis = wid/8;
            uint rest = wid % 8;
            uint axisSign = rest / 4;
            rest = rest % 4;
            uint wif = rest;
                    
            if (srcAxis == 0)
            {
                targetPosInHalo = targetPosInHalo.yzx;
            }
            else if (srcAxis == 1)
            {
                targetPosInHalo = targetPosInHalo.zxy;
            }
            targetPosInHalo.z -= axisSign;
     
            //float3 diff = wedgeGrad[srcW] - wedgeGrad[dstW].toSrc();                    
            //float3 deltaWA = float3(-1.0, -1.0, 0.0);
            //float3 deltaWB = deltaWA.toSrc();
            //float3 K = deltaWA - deltaWB;
            //float dK = dot(diff, K);
            //float kk = dot(K, K);
            //float wgt = 2.0 * SmoothnessWeight;
            //
            //float gradLi = wgt * dK;
            //float diagLi = wgt * kk;
        }
    }

/*        // Write scratch: li/lj get the Term-1 Jacobi step, every other slot
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
*/
}
