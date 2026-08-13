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

static const uint4 kernelbits = uint4(
    3 | (5 << 5) | (12 << 10) | (15 << 15) | (17 << 20) | (20 << 25), // w 8
    0x00100401u,  // w 192 | w 16
    0x403F3C30u,  // w -48
    0x3B2F2C2Bu);

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
        uint myRawLabel = (NodeCandidateLabel[authIdx * 2u + tid / 4u] >> ((tid % 4u) * 8u)) & 0xFFu;
        uint myLabel = myRawLabel | myRawLabel << 8u; // 16-bit label, repeated in both bytes
        myLabel = myLabel | myLabel << 16u; // 32-bit label, repeated four times
        // buildCandidatesCS.hlsl seeds unused slots to a plain 0.0 (never
        // overridden -- other terms, e.g. smoothnessJacobiCS.hlsl's Term 3
        // regularizer, sum all 8 raw potentials unconditionally and rely on
        // that being a harmless neutral value, so it can't just be changed
        // to something very negative there). A genuine *other* real
        // candidate seeds to -myDist+jitter, which is typically well BELOW
        // 0.0 -- so an unused slot's 0.0 was silently beating real
        // candidates in this max search. Exclude sentinel-labeled slots by
        // their LABEL instead of trusting potential ordering.
        float myPot = (myRawLabel == SENTINEL_CANDIDATE) ? -1.0e30 : NodePotential[authIdx * MAX_CANDIDATES + tid];
        float maxPot = WaveActiveMax(myPot);
        if (myPot == maxPot) { li = myLabel; }
        float myPot2 = (myPot == maxPot) ? -1.0e30 : myPot; // knock the winner out for the runner-up pass
        float maxPot2 = WaveActiveMax(myPot2);
        if (myPot2 == maxPot2) { lj = myLabel; }
    }               
    GroupMemoryBarrierWithGroupSync();

    uint lli = li;
    uint llj = lj;

    // If the authority node has fewer than 2 real candidates this rotation,
    // the "knock out the winner, take the max of what's left" pass in Stage
    // 1 has nothing but unused slots to pick from -- buildCandidatesCS fills
    // those with SENTINEL_CANDIDATE(255), which myLabel's 4x byte-replicate
    // turns into exactly 0xFFFFFFFF. Bail the whole tile uniformly: every
    // thread just saw the same li/lj write behind the same barrier, so this
    // branch is identical across all 128 threads (no divergence), safe to
    // return before any further GroupMemoryBarrierWithGroupSync() call.
    if (llj == 0xFFFFFFFFu) return;

    // mine slots and potentials for Li, Lj
    {
        bool isB = tid >= 64u;
        uint tidLocal = tid & 63u; // rebase B half (tid 64..127) back to local 0..63
        uint3 inHaloPosA = uint3(tidLocal % HALO_DIM, (tidLocal / HALO_DIM) % HALO_DIM, tidLocal / (HALO_DIM * HALO_DIM));
        uint3 posA = haloOriginA + inHaloPosA;
        uint inHaloIdx = isB ? inHaloPosToInHaloIdxB(inHaloPosA) : inHaloPosToInHaloIdxA(inHaloPosA);
        uint idx = isB ? posToIdxB(posA) : posToIdxA(posA);
        uint candidates0to3 = NodeCandidateLabel[idx * 2u + 0u];
        uint candidates4to7 = NodeCandidateLabel[idx * 2u + 1u];
        uint maski0to3 = ~(candidates0to3 ^ lli);
        maski0to3 =  maski0to3 & (maski0to3 >> 4u);
        maski0to3 =  maski0to3 & (maski0to3 >> 2u);
        maski0to3 = (maski0to3 & (maski0to3 >> 1u)) & 0x01010101u;
        uint maski4to7 = ~(candidates4to7 ^ lli);
        maski4to7 = maski4to7 & (maski4to7 >> 4u);
        maski4to7 = maski4to7 & (maski4to7 >> 2u);
        maski4to7 = (maski4to7 & (maski4to7 >> 1u)) & 0x01010101u;

        uint maskj0to3 = ~(candidates0to3 ^ llj);
        maskj0to3 =  maskj0to3 & (maskj0to3 >> 4u);
        maskj0to3 =  maskj0to3 & (maskj0to3 >> 2u);
        maskj0to3 = (maskj0to3 & (maskj0to3 >> 1u)) & 0x01010101u;
        uint maskj4to7 = ~(candidates4to7 ^ llj);
        maskj4to7 = maskj4to7 & (maskj4to7 >> 4u);
        maskj4to7 = maskj4to7 & (maskj4to7 >> 2u);
        maskj4to7 = (maskj4to7 & (maskj4to7 >> 1u)) & 0x01010101u;

        uint iSlot = 0xFFu;
        float potLi = MissingFallback;
        if (maski0to3 != 0u) {
            iSlot = (uint) firstbithigh(maski0to3) / 8u;
            potLi = NodePotential[idx * MAX_CANDIDATES + iSlot];
        } else if (maski4to7 != 0u) {
            iSlot = (uint) firstbithigh(maski4to7) / 8u + 4u;
            potLi = NodePotential[idx * MAX_CANDIDATES + iSlot];
        }

        uint jSlot = 0xFFu;
        float potLj = MissingFallback;
        if (maskj0to3 != 0u) {
            jSlot = (uint) firstbithigh(maskj0to3) / 8u;
            potLj = NodePotential[idx * MAX_CANDIDATES + jSlot];
        } else if (maskj4to7 != 0u) {
            jSlot = (uint) firstbithigh(maskj4to7) / 8u + 4u;
            potLj = NodePotential[idx * MAX_CANDIDATES + jSlot];
        }

        lijSlots[inHaloIdx] = iSlot << 16u | jSlot;
        lijPotDiff[inHaloIdx] = potLi - potLj;
    }
    GroupMemoryBarrierWithGroupSync();
                
    // four warps, targets split in four groups
    uint warpId = tid / 32u;
    uint wid = tid % 32u;
    // 27 threads each read a kernel element
    if(wid < 27u){
        for (int iTarget = warpId; iTarget < 16; iTarget += 4)
        {
            uint local = iTarget & 7u;
            uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);        
            bool isB = iTarget >= 8u;
            uint3 inHaloPos = inTilePos + 1u;
            uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
            
            // multiply by kernel term
            uint wid18 = wid % 18u;
            int offset;
            float weight;
            if(wid18 < 6u){
                offset = kernelbits.x >> (wid18 * 5u) & 0x1Fu;
                weight = 8.0;
            } else {
                offset = (kernelbits[(wid18-2u) / 4u] >> ((wid18 + 2u) % 4u) * 8u) & 0xFFu;
                weight = 16.0;
            }
            bool negate = (wid >= 18u) || (isB && wid18 >= 10u);
            if(negate) offset = -offset;
            if(wid18 >= 9u) weight = 192.0;
            if(wid18 >= 10u) weight = -48.0;
            float contrib = lijPotDiff[centerIdx + offset] * weight;
            // group reduce to sum getting grad and dial                    
            float total = WaveActiveSum(contrib);
            if(wid < 8u){
                uint targetSlots = lijSlots[centerIdx];
                uint targetTarget = isB ? posToIdxB(haloOriginA + inHaloPos) : posToIdxA(haloOriginA + inHaloPos);
                float phi = NodePotential[targetTarget * MAX_CANDIDATES + wid];
                float w = SmoothnessWeight;
                float gradLi = w * total;
                float diagLi = w * 192.0; // sum of absolute kernel weights
                float step = clamp(-gradLi / (diagLi + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
                if(wid == (targetSlots & 0xffu)){
                    phi -= step;
                }
                else if(wid == (targetSlots >> 16u)){
                    phi += step;
                }                
                NodePotentialScratch[targetTarget * MAX_CANDIDATES + wid] = phi;
            }
        }
    }
          
}
