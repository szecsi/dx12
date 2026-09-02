#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"
#include "SyntheticField.hlsli"

// Profiling-only counterpart to smoothnessJacobiSyntheticCS.hlsl's tile/
// groupshared/wave-op architecture: the SAME Term-1-only 27-tap signed-
// kernel smoothing (self=192, face=16x6 and edge=8x12 over the SAME-
// sublattice 3x3x3-minus-center neighborhood -- SameLatticeOffsets[26]'s
// nnz==1/nnz==2 entries -- plus cross=-48x8 over the 8 OPPOSITE-sublattice
// nearest neighbors, see BuildCrossNeighbor below) and the SAME B-node
// relabel rule (SyntheticVote8 or dumb binary flip via UseLabelVote, gated
// by AllowBFlips, with SyntheticEpsilon as the head-start potential), but
// dispatched ONE THREAD PER TARGET NODE, reading NodeCandidateLabel/
// NodePotential straight out of global memory for each neighbor -- no
// per-tile halo load into groupshared arrays, no warp-cooperative
// WaveActiveSum reduction, no block dispatch geometry at all (just a flat
// NodeCount-wide dispatch, same SmoothnessGroups bound smoothnessJacobiCS.hlsl
// already uses).
//
// CORRECTNESS NOTE: SameLatticeOffsets[26]'s nnz==3 entries (offset along
// all three axes) are the SAME-sublattice diagonal -- NOT what the shipped
// kernel's weight=-48 "cross" tap actually reaches. Decoding the real
// kernelbits table (smoothnessJacobiSyntheticCS.hlsl) confirms those 8
// weight=-48 taps are OPPOSITE-sublattice nearest neighbors (the true BCC
// nearest-neighbor distance), not a same-sublattice diagonal -- an earlier
// version of this file (and eikonal.tex's Smoothing Term section, since
// corrected) mischaracterized this and summed the wrong 8 neighbors with
// weight -48, which never actually couples A and B nodes together at all --
// confirmed as the cause of a visibly spiky/decoupled "falling apart"
// surface versus the tile kernel.
//
// Exists purely to isolate what the tile/shared-mem/wave-op machinery in
// smoothnessJacobiSyntheticCS.hlsl actually buys in practice: here, each
// same-sublattice neighbor is re-read from global memory independently by
// every target thread that needs it (up to 26x redundant traffic per node),
// versus once per tile via the halo cache there. Never meant to replace it.
// Junction straightness and the Eikonal floor (that file's Terms 2/3) are
// not implemented here at all -- for an apples-to-apples comparison, disable
// them there via ENABLE_JUNCTION_TERM/ENABLE_EIKONAL_TERM=0 rather than
// comparing this file against its default (all 3 terms on) build.
//
// Out-of-domain same-sublattice neighbors (only possible at the true grid
// boundary) are treated as the same fixed virtual background node (label 0,
// potential 1.0) GetCornerPotential/GetCornerTopLabel already use elsewhere
// in this pipeline -- there's no halo to clamp within here, so this can't
// reuse smoothnessJacobiSyntheticCS.hlsl's halo-clamp-to-nearest-valid-B
// convention; the two conventions only disagree in this thin boundary
// shell, not in the profiling-relevant interior.
#define SmoothnessSyntheticSimpleSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u4)," \
    "RootConstants(num32BitConstants=2, b1)," \
    "CBV(b2)"

cbuffer SyntheticConsts : register(b1)
{
    uint UseLabelVote; // see smoothnessJacobiSyntheticCS.hlsl's SyntheticConsts
    uint AllowBFlips;
};

RWStructuredBuffer<uint> NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);
RWStructuredBuffer<float> NodePotentialScratch : register(u2);
// See smoothnessJacobiSyntheticCS.hlsl's matching declaration -- beta-tracking write below.
RWStructuredBuffer<float> NodeAlienPotential : register(u4);

// Weight for a same-sublattice offset, keyed by its nonzero-axis count --
// 1 axis (face) = 16, 2 axes (edge) = 8. (3-axis/diagonal same-sublattice
// offsets are NOT part of this kernel at all -- see BuildCrossNeighbor for
// the real weight=-48 opposite-sublattice taps.)
float OffsetWeight(int3 off)
{
    int nnz = (off.x != 0 ? 1 : 0) + (off.y != 0 ? 1 : 0) + (off.z != 0 ? 1 : 0);
    return (nnz == 1) ? 16.0 : 8.0;
}

// The 8 opposite-sublattice ("cross") nearest neighbors of a node at grid
// index idx, corner combo c=0..7 (bits = dx,dy,dz each in {0,1}):
//  - A-target (isB=false): cross neighbor is the B cube-center at
//    (idx.x-1+dx, idx.y-1+dy, idx.z-1+dz) -- the 8 unit cubes that share
//    this A corner (inverse of a B-node's own "8 A corners" relationship
//    below), may fall outside BDim at the domain boundary.
//  - B-target (isB=true): cross neighbor is the A corner at
//    (idx.x+dx, idx.y+dy, idx.z+dz) -- the SAME 8 corners the B-flip vote
//    below already uses -- always in range since BDim=GridRes-1.
// Out-of-range (A-target only, at the domain boundary) falls back to the
// same fixed virtual background node (label 0, potential 1.0) used
// elsewhere in this file for an out-of-domain same-sublattice neighbor.
void BuildCrossNeighbor(uint3 idx, bool isB, uint c, out uint nbLabel, out float nbPot)
{
    int3 d = int3(c & 1u, (c >> 1u) & 1u, (c >> 2u) & 1u);
    if (isB)
    {
        uint3 aIdx = idx + uint3(d);
        uint a = AIdx(aIdx.x, aIdx.y, aIdx.z);
        nbLabel = GetCandidateLabelAt(NodeCandidateLabel, a, 0u);
        nbPot = NodePotential[a * MAX_CANDIDATES + 0u];
    }
    else
    {
        int3 bIdx = (int3)idx + d - 1;
        bool valid = all(bIdx >= 0) && all(bIdx < (int)BDim);
        if (valid)
        {
            uint b = BIdx((uint)bIdx.x, (uint)bIdx.y, (uint)bIdx.z);
            nbLabel = GetCandidateLabelAt(NodeCandidateLabel, b, 0u);
            nbPot = NodePotential[b * MAX_CANDIDATES + 0u];
        }
        else
        {
            nbLabel = 0u;
            nbPot = 1.0;
        }
    }
}

[RootSignature(SmoothnessSyntheticSimpleSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void smoothnessJacobiSyntheticSimpleCS(uint3 dtid : SV_DispatchThreadID)
{
    uint targetIdx = dtid.x;
    if (targetIdx >= NodeCount) return;

    bool isB; uint3 idx;
    DecodeNodeIndex(targetIdx, isB, idx);

    uint myLabel = GetCandidateLabelAt(NodeCandidateLabel, targetIdx, 0u);
    float myPot = NodePotential[targetIdx * MAX_CANDIDATES + 0u];

    float total = 192.0 * myPot; // self tap -- always agrees with itself
    [unroll]
    for (uint n = 0; n < 26u; n++)
    {
        int3 off = SameLatticeOffsets[n];
        int nnz = (off.x != 0 ? 1 : 0) + (off.y != 0 ? 1 : 0) + (off.z != 0 ? 1 : 0);
        if (nnz == 3) continue; // same-sublattice diagonal -- not part of this kernel, see BuildCrossNeighbor instead

        int3 nb = (int3)idx + off;
        bool valid = isB
            ? (all(nb >= 0) && all(nb < (int)BDim))
            : (all(nb >= 0) && all(nb < (int)GridRes));

        uint nbLabel;
        float nbPot;
        if (valid)
        {
            uint nbIdx = isB ? BIdx((uint)nb.x, (uint)nb.y, (uint)nb.z) : AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z);
            nbLabel = GetCandidateLabelAt(NodeCandidateLabel, nbIdx, 0u);
            nbPot = NodePotential[nbIdx * MAX_CANDIDATES + 0u];
        }
        else
        {
            nbLabel = 0u;
            nbPot = 1.0;
        }

        float signedVal = nbPot * ((nbLabel == myLabel) ? 1.0 : -1.0);
        total += signedVal * OffsetWeight(off);
    }

    [unroll]
    for (uint c = 0; c < 8u; c++)
    {
        uint nbLabel;
        float nbPot;
        BuildCrossNeighbor(idx, isB, c, nbLabel, nbPot);
        float signedVal = nbPot * ((nbLabel == myLabel) ? 1.0 : -1.0);
        total += signedVal * -48.0;
    }

    float grad = SmoothnessWeight * total;
    float diag = SmoothnessWeight * 192.0;
    float step = clamp(-grad / (diag + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
    float newPot = myPot + step;
    uint newLabel = myLabel;

    if (newPot < 0.0)
    {
        // A-nodes never relabel, so this reflected magnitude is their final
        // newPot. For a B-node this is only a placeholder, overwritten below
        // with a real head-start once it's actually relabeled.
        newPot = -newPot;
        if (isB && AllowBFlips != 0u)
        {
            if (UseLabelVote == 0u)
            {
                newLabel = 1u - myLabel;
            }
            else
            {
                uint labels8[8];
                float pots8[8];
                [unroll]
                for (uint c = 0; c < 8u; c++)
                {
                    uint3 cornerIdx = idx + uint3(c & 1u, (c >> 1u) & 1u, (c >> 2u) & 1u);
                    uint a = AIdx(cornerIdx.x, cornerIdx.y, cornerIdx.z);
                    labels8[c] = GetCandidateLabelAt(NodeCandidateLabel, a, 0u);
                    pots8[c] = NodePotential[a * MAX_CANDIDATES + 0u];
                }
                float winnerScore;
                SyntheticVote8(labels8, pots8, newLabel, winnerScore);
            }
            newPot = SyntheticEpsilon;
        }
    }

    NodePotentialScratch[targetIdx * MAX_CANDIDATES + 0u] = newPot;
    uint word0 = NodeCandidateLabel[targetIdx * 2u + 0u];
    NodeCandidateLabel[targetIdx * 2u + 0u] = (word0 & 0xFFFF00FFu) | ((newLabel & 0xFFu) << 8u);

    // See smoothnessJacobiSyntheticCS.hlsl's matching write for the full
    // rationale -- keep EVERY node's beta tracking -phi every sweep,
    // unconditionally (including previously-enabled ones -- deliberate,
    // resets Stage 3's routed betas if Phase 1 ever runs after an alien step).
    NodeAlienPotential[targetIdx] = -newPot;
}
