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
groupshared float lijPotDiff[HALO_NODES];     // candidate potentials, 2 per node
groupshared uint  li;
groupshared uint  lj;

uint posToIdxA(uint3 l) { return AIdx(l.x, l.y, l.z); }
uint posToIdxB(uint3 l) { return BIdx(l.x, l.y, l.z); }
        
uint inHaloPosToInHaloIdxA(uint3 l) { return l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }
uint inHaloPosToInHaloIdxB(uint3 l) { return 64u + l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }


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
        lijPotDiff[inHaloIdx] = NodePotential[idx * MAX_CANDIDATES + iSlot]
                    - NodePotential[idx * MAX_CANDIDATES + jSlot];
    }
    GroupMemoryBarrierWithGroupSync();
                
    uint lli = li;
    uint llj = lj;

    // four warps, targets split in four groups
    uint warpId = tid / 32u;
    uint wid = tid % 32u;
    // 27 threads each read a kernel element
    if(wid < 27u){
        for (int iTarget = warpId; iTarget < 16; iTarget += 4)
        {
            uint3 inTilePos = uint3(iTarget % HALO_DIM, (iTarget / HALO_DIM) % HALO_DIM, iTarget / (HALO_DIM * HALO_DIM));
            bool isB = iTarget >= 8u;
            uint3 inHaloPos = inTilePos + 1u;
            uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
            
            // multiply by kernel term
            int offset = 0x10u;
            uint widh = (wid+1)/2;
            offset = (offset << (widh * 2u)) >> 6u;

            if(wid > 6u){
                
            }
            
            offset *= (wid%2)?-1:1;
            
            if(wid >= 20u){ //cross-B
              offset += 64;
            }
            float contrib = lijPotDiff[centerIdx + offset] * 1.0/*weights[wid]*/;
                    
                    
            // group reduce to sum getting grad and dial                    
            float total = WaveActiveSum(contrib);
            if(wid == 0u){}
        
        
        // compute step from grad and diag
        // apply change to buffer in-place?
        }
    }
            
    GroupMemoryBarrierWithGroupSync();

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
