#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// One-time (per RunAlienPhase invocation, i.e. once per Reinit/Continue,
// AFTER Phase 1's rounds have fully settled) pass that assigns each node its
// alien-potential discriminator and an informed initial beta. Same
// same-sublattice-26 + cross-sublattice-8 neighbor walk as before
// (SameLatticeOffsets' nnz==1/nnz==2 entries + the 8 opposite-sublattice
// nearest neighbors), but now records, for every DISTINCT foreign label
// found (not just the first 2), how many of the 34 neighbors carry it and
// the highest phi seen among them.
//
// Routing choice: among 2+ distinct foreign labels, route to whichever has
// the most neighbor VOTES (a local majority -- same spirit as
// SyntheticField.hlsli's existing SyntheticVote8, not a new untested idea),
// ties broken by higher best-neighbor-phi. Replaces the old "whichever was
// scanned first" (an artifact of loop order, not a real choice) -- a thin,
// one-corner-touching sliver of some other label now properly loses to a
// real neighboring region instead of winning by scan-order luck.
//
// Seed beta = the routed label's own best-neighbor-phi (a real nearby
// value for the joint phi/beta smoothing to refine) instead of the old
// flat "-phi" (a leftover no-op value from the additive corner rule that
// means nothing under the current reciprocal one).
//
// 0 or 1 distinct foreign labels found -> discrimination is meaningless
// (this node never needs to tell two foreign labels apart), so `enabled`
// stays false (IsAlienRoute always returns false for a disabled node,
// exactly as before this pass existed). MAX_GATHER_CANDIDATES+ distinct
// foreign labels ("crowded") is explicitly best-effort: only the first
// MAX_GATHER_CANDIDATES distinct labels encountered are tracked at all, so
// an extremely crowded node's true majority might be missed -- an already-
// low-quality-surface region regardless of what this pass does.
#define GatherAlienDiscriminatorSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b0)"

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0); // read: frozen label
RWStructuredBuffer<float> NodePotential      : register(u1); // read: frozen phi
RWStructuredBuffer<uint>  NodeDiscriminator  : register(u2); // write
RWStructuredBuffer<float> NodeAlienPotential : register(u3); // write: beta seed

// (label, phi) at a cross-sublattice ("cross", true BCC nearest-neighbor)
// tap -- mirrors smoothnessJacobiSyntheticSimpleCS.hlsl's BuildCrossNeighbor.
// Out-of-domain (A-target only, at the true grid boundary) falls back to
// the same fixed virtual background node (label 0, phi irrelevant here --
// see the count-only use below) used throughout this pipeline.
void CrossNeighborLabelPot(uint3 idx, bool isB, uint c, out uint lab, out float phi)
{
    int3 d = int3(c & 1u, (c >> 1u) & 1u, (c >> 2u) & 1u);
    if (isB)
    {
        uint3 aIdx = idx + uint3(d);
        uint ref = AIdx(aIdx.x, aIdx.y, aIdx.z);
        lab = GetCandidateLabelAt(NodeCandidateLabel, ref, 0u);
        phi = NodePotential[ref * MAX_CANDIDATES + 0u];
        return;
    }
    int3 bIdx = (int3)idx + d - 1;
    if (all(bIdx >= 0) && all(bIdx < (int)BDim))
    {
        uint ref = BIdx((uint)bIdx.x, (uint)bIdx.y, (uint)bIdx.z);
        lab = GetCandidateLabelAt(NodeCandidateLabel, ref, 0u);
        phi = NodePotential[ref * MAX_CANDIDATES + 0u];
        return;
    }
    lab = 0u;
    phi = 0.0;
}

#define MAX_GATHER_CANDIDATES 6u

// Records one neighbor sighting into the small fixed candidate list --
// bump its count/best-phi if already tracked, else add a new slot (ignored
// once all MAX_GATHER_CANDIDATES slots are full, see the header comment).
void RecordCandidate(inout uint candLabel[MAX_GATHER_CANDIDATES], inout uint candCount[MAX_GATHER_CANDIDATES],
                      inout float candBestPhi[MAX_GATHER_CANDIDATES], inout uint candN, uint lab, float phi)
{
    for (uint i = 0; i < candN; i++)
    {
        if (candLabel[i] == lab)
        {
            candCount[i]++;
            candBestPhi[i] = max(candBestPhi[i], phi);
            return;
        }
    }
    if (candN < MAX_GATHER_CANDIDATES)
    {
        candLabel[candN] = lab;
        candCount[candN] = 1u;
        candBestPhi[candN] = phi;
        candN++;
    }
}

[RootSignature(GatherAlienDiscriminatorSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void gatherAlienDiscriminatorCS(uint3 dtid : SV_DispatchThreadID)
{
    uint node = dtid.x;
    if (node >= NodeCount) return;

    bool isB; uint3 idx;
    DecodeNodeIndex(node, isB, idx);
    uint myLabel = GetCandidateLabelAt(NodeCandidateLabel, node, 0u);
    float myPot = NodePotential[node * MAX_CANDIDATES + 0u];

    uint candLabel[MAX_GATHER_CANDIDATES];
    uint candCount[MAX_GATHER_CANDIDATES];
    float candBestPhi[MAX_GATHER_CANDIDATES];
    uint candN = 0u;

    {
        [unroll]
        for (uint n = 0; n < 26u; n++)
        {
            int3 off = SameLatticeOffsets[n];
            int nnz = (off.x != 0 ? 1 : 0) + (off.y != 0 ? 1 : 0) + (off.z != 0 ? 1 : 0);
            if (nnz == 3) continue; // same-sublattice diagonal -- not part of this stencil, see the cross-tap loop instead

            int3 nb = (int3)idx + off;
            bool valid = isB
                ? (all(nb >= 0) && all(nb < (int)BDim))
                : (all(nb >= 0) && all(nb < (int)GridRes));
            uint lab = 0u; float phi = 0.0;
            if (valid)
            {
                uint ref = isB ? BIdx((uint)nb.x, (uint)nb.y, (uint)nb.z) : AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z);
                lab = GetCandidateLabelAt(NodeCandidateLabel, ref, 0u);
                phi = NodePotential[ref * MAX_CANDIDATES + 0u];
            }
            if (lab == myLabel) continue;
            RecordCandidate(candLabel, candCount, candBestPhi, candN, lab, phi);
        }
    }
    {
        [unroll]
        for (uint c = 0; c < 8u; c++)
        {
            uint lab; float phi;
            CrossNeighborLabelPot(idx, isB, c, lab, phi);
            if (lab == myLabel) continue;
            RecordCandidate(candLabel, candCount, candBestPhi, candN, lab, phi);
        }
    }

    bool enabled = false;
    uint routedLabel = 0u;
    float betaSeed = -myPot; // inert default when disabled -- nothing ever reads it (IsAlienRoute always false)
    if (candN >= 2u)
    {
        uint best = 0u;
        for (uint i = 1u; i < candN; i++)
        {
            bool better = (candCount[i] > candCount[best]) ||
                          (candCount[i] == candCount[best] && candBestPhi[i] > candBestPhi[best]);
            if (better) best = i;
        }
        enabled = true;
        routedLabel = candLabel[best];
        betaSeed = candBestPhi[best];
    }
    // candN <= 1: enabled stays false, this node's discriminator is never
    // consulted (IsAlienRoute always returns false).

    NodeDiscriminator[node] = EncodeDiscriminator(enabled, routedLabel);
    NodeAlienPotential[node] = betaSeed;
}
