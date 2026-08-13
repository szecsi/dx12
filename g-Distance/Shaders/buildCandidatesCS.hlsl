#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"

// Per-node candidate label arrays (<=MAX_CANDIDATES(8) slots, packed 8 bits
// each into two uints -- see DistanceConfig.hlsli's SENTINEL_CANDIDATE
// comment) + initial raw potentials, seeded from NodeFootDist (a JFA-
// computed per-A-node distance to the nearest differently-labeled A-node --
// see jfaInitCS/jfaStepCS/jfaFinalizeCS.hlsl, dispatched earlier in
// RunTopologyBuild) rather than a flat OwnLabelSeed constant -- see the
// potential-seeding block near the end of this file for the exact A/B
// formulas.
//   B-node candidates = the distinct input labels among its own cube's 8
//     A-corners AND its 6 face-neighbor cubes' corners (a 1-cube halo, not
//     just its own cube as originally planned) -- confirmed necessary by
//     directly inspecting a picked wedge-inset tet: a B-node whose own cube
//     is uniform (single candidate) can still be a corner of an
//     actively-interfaced tet purely because a face-neighbor cube (sharing
//     that tet's B0/B1 pair) touches the other label. Missing that
//     candidate entirely doesn't just leave that one corner uncommitted --
//     smoothnessJacobiCS.hlsl's tet-gradient sum (TetFieldGrad) mixes ALL 4
//     corners' values via shape-function weights, so a missing candidate's
//     0-fallback there corrupts the WHOLE tet's computed gradient, and the
//     optimizer distorts the node's one REAL candidate trying to chase
//     consistency against that phantom target (observed directly: 0.857 at
//     a corner whose 3 real-2-candidate neighbors all sat around 0.19-0.27).
//   A-node candidates = its own input label (always slot 0, never evicted)
//     union the distinct labels found among its same-sublattice 26-neighbor
//     A-nodes, capped at 8 -- needed so a neighboring tet's interface
//     computation always has *some* potential value for a competing label at
//     this A-node's corner, even though its own winner is fixed.
// Dispatched twice (Mode root constant): once over ACount threads, once over
// BCount -- unused slots are written as SENTINEL_CANDIDATE, masked out by
// every consumer (FindSlot-style helpers never match SENTINEL_CANDIDATE).
#define BuildCandidatesSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=2, b1)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b2)"

cbuffer ModeConsts : register(b1) {
    uint Mode;          // 0 = A-nodes, 1 = B-nodes
    uint NeutralBSeed;  // 1 = B-nodes get pure jitter (no majority-vote seed slot), see GUI checkbox -- UNUSED by potential seeding now, see the JFA-based seeding block below; candidate label DISCOVERY (freq[]/labels[] above) is unaffected either way.
};

RWStructuredBuffer<uint>  RasterLabel : register(u0);
RWStructuredBuffer<uint>  NodeCandidateLabel : register(u1);
RWStructuredBuffer<float> NodePotential : register(u2);
// ACount-sized: distance from an A-node to the nearest A-node with a
// DIFFERENT ground-truth label ("footvector length"), precomputed by
// jfaInitCS/jfaStepCS/jfaFinalizeCS.hlsl earlier in RunTopologyBuild. Only
// ever indexed with a real A-node index (this thread's own `node` in Mode 0,
// or a halo A-corner index in Mode 1) -- never B, which JFA doesn't cover.
RWStructuredBuffer<float> NodeFootDist : register(u3);

[RootSignature(BuildCandidatesSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildCandidatesCS(uint3 tid : SV_DispatchThreadID)
{
    uint labels[8] = {
        SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE,
        SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE
    };
    uint count = 0;
    uint node;
    uint seedSlot = 0; // which slot (if any beyond A's fixed slot 0) got the old majority-vote seed -- UNUSED by potential seeding now, see below
    // Mode==1 (B) only: per-slot corner count over the wide halo scan below
    // (candidate DISCOVERY only now, see the tight own-cube scan inside the
    // else block for the actual identity/averaging decision). Declared here
    // (not inside the else block) so it's still in scope in the majority-
    // vote block below (itself unused by potential seeding).
    uint freq[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    // Mode==1 (B) only: whether this B-node's OWN cube's 8 corners (not the
    // wider halo) all agree on one label, that label, and the average of
    // just those 8 corners' footvector lengths -- see the tight scan at the
    // top of the else block and the potential-seeding block further down.
    bool bOwnCubeUniform = false;
    uint bOwnLabel = SENTINEL_CANDIDATE;
    float bOwnAvgDist = 0.0;

    if (Mode == 0) {
        if (tid.x >= ACount) return;
        node = tid.x;

        uint k = node / (GridRes * GridRes);
        uint rem = node % (GridRes * GridRes);
        uint j = rem / GridRes;
        uint i = rem % GridRes;

        uint ownLabel = RasterLabel[node];
        labels[0] = ownLabel;
        count = 1;

        for (uint n = 0; n < 26 && count < 8; n++) {
            int3 nb = int3((int)i, (int)j, (int)k) + SameLatticeOffsets[n];
            if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
            uint nLabel = RasterLabel[AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z)];
            if (nLabel == ownLabel) continue;
            bool found = false;
            for (uint s = 1; s < count; s++) if (labels[s] == nLabel) { found = true; break; }
            if (!found) { labels[count] = nLabel; count++; }
        }
    } else {
        if (tid.x >= BCount) return;
        node = ACount + tid.x;

        uint k = tid.x / (BDim * BDim);
        uint rem = tid.x % (BDim * BDim);
        uint j = rem / BDim;
        uint i = rem % BDim;

        // Tight scan: this B-node's OWN cube corners only -- exactly 8,
        // always in-bounds (i,j,k range over [0,BDim), BDim=GridRes-1, so
        // i+1<=GridRes-1 is always a valid A index). If they all agree on
        // one label, this B-node assumes that label with the SAME
        // confidence an A-node has in its own ground truth -- forced into
        // slot 0 exactly like A's own/input label, below -- regardless of
        // what a neighboring cube's corners say (that only matters for
        // candidate DISCOVERY, the wide scan right after this). Narrower
        // than the wide halo on purpose: a B-node dead center of a single-
        // label cube should confidently assume that label even if some
        // face-neighbor cube touches a different one.
        {
            uint ownLabel0 = RasterLabel[AIdx(i, j, k)];
            float ownSum = 0.0;
            bool ownUniform = true;
            for (uint c = 0; c < 8; c++) {
                uint aIdx0 = AIdx(i + (c & 1u), j + ((c >> 1) & 1u), k + ((c >> 2) & 1u));
                if (RasterLabel[aIdx0] != ownLabel0) ownUniform = false;
                ownSum += NodeFootDist[aIdx0];
            }
            bOwnCubeUniform = ownUniform;
            bOwnLabel = ownLabel0;
            bOwnAvgDist = ownSum * 0.125; // /8
        }
        if (bOwnCubeUniform) { labels[0] = bOwnLabel; count = 1; }

        // Wide scan, candidate DISCOVERY only now (the tight scan above
        // already decided identity/averaging): own cube is A-range
        // [i,i+1]x[j,j+1]x[k,k+1]; scanning [i-1,i+2] in each axis
        // additionally covers all 6 face-neighbor cubes (e.g. the -X
        // neighbor cube needs x in {i-1,i}, already within this range) plus
        // some edge/corner-adjacent cubes for free -- harmless, capped at 8
        // slots and nowhere near that cap with only a couple of labels
        // actually present in the scene. If bOwnCubeUniform pre-seeded
        // labels[0]/count=1 above, this loop's own dedup (checking s<count)
        // naturally folds any wide-scan match of that same label into
        // freq[0] instead of adding a duplicate slot.
        for (int dz = -1; dz <= 2; dz++) {
            for (int dy = -1; dy <= 2; dy++) {
                for (int dx = -1; dx <= 2; dx++) {
                    int ai = (int)i + dx, aj = (int)j + dy, ak = (int)k + dz;
                    if (ai < 0 || aj < 0 || ak < 0 || ai >= (int)GridRes || aj >= (int)GridRes || ak >= (int)GridRes) continue;
                    uint aIdx = (uint)ai + (uint)aj * GridRes + (uint)ak * GridRes * GridRes;
                    uint aLabel = RasterLabel[aIdx];
                    bool found = false;
                    for (uint s = 0; s < count; s++) if (labels[s] == aLabel) { freq[s]++; found = true; break; }
                    if (!found && count < 8) { labels[count] = aLabel; freq[count] = 1; count++; }
                }
            }
        }

        // B has no ground truth to fix as slot 0. Two seeding strategies,
        // toggled by NeutralBSeed (GUI checkbox, default ON):
        //   - Majority-vote seed (NeutralBSeed=0, the original strategy):
        //     give the majority-vote label among the node's up-to-8
        //     A-corners an OwnLabelSeed-strength start, same reasoning as
        //     A's OwnLabelSeed -- starting every candidate near a jittered 0
        //     meant the margin hinge (smoothnessJacobiCS.hlsl term 2) had to
        //     separate them from scratch using only MaxPotentialStep-
        //     clamped steps -- far too slow to reach MarginTarget within a
        //     practical iteration count (confirmed directly: a debug
        //     readback found EVERY multi-candidate B-node still sitting at
        //     a near-zero gap after a full Reinitialize run). Well-informed,
        //     but strongly confident in whichever label locally dominates --
        //     including immediately AGAINST an isolated single-voxel
        //     feature, since 7 of 8 corners of any cube touching it are
        //     background. Confirmed via direct derivation this is why an
        //     isolated point's CurrentVolume never reaches its geometric
        //     ceiling (1 full unit under symmetric, unforced conditions) --
        //     B's confident opposition cuts the crossing back before
        //     VolumeWeight ever gets a chance to push it there.
        //   - Neutral seed (NeutralBSeed=1, default): every B candidate
        //     gets pure jitter, no majority-vote boost at all -- slower to
        //     separate via the margin hinge (the original problem this was
        //     built to avoid), but lets an isolated feature's B-neighbors
        //     actually start near the "B ~ 0 weight" condition its natural
        //     volume ceiling assumes, instead of starting pre-committed
        //     against it.
        // UNUSED by potential seeding now -- see the JFA-based block below,
        // which branches on whether the halo is uniform (count==1) instead
        // of NeutralBSeed. Left in place; only seedSlot itself goes unread.
        if (NeutralBSeed == 0) {
            for (uint s = 1; s < count; s++) if (freq[s] > freq[seedSlot]) seedSlot = s;
        } else {
            seedSlot = 8; // no valid slot index
        }
    }

    // Pack all 8 labels (8 bits each, 4 per word) into two uints -- see
    // SENTINEL_CANDIDATE/GetCandidateLabelAt. Safe as a non-atomic
    // read-modify-write-free write: this thread owns this node's whole
    // candidate words exclusively, written here once and never again.
    uint packed0 = 0, packed1 = 0;
    for (uint s = 0; s < 4; s++) packed0 |= (labels[s] & 0xFFu) << (s * 8u);
    for (uint s = 4; s < 8; s++) packed1 |= (labels[s] & 0xFFu) << ((s - 4) * 8u);
    NodeCandidateLabel[node * 2u + 0u] = packed0;
    NodeCandidateLabel[node * 2u + 1u] = packed1;

    // Initial candidate potentials -- JFA-based distance-field seeding
    // (NodeFootDist, from jfaInitCS/jfaStepCS/jfaFinalizeCS.hlsl earlier in
    // RunTopologyBuild), replacing the old flat OwnLabelSeed/negShare-jitter
    // scheme entirely:
    //   A-nodes: this node's own footvector length (distance to the nearest
    //     A-node with a DIFFERENT ground-truth label) seeds its own/input
    //     label (slot 0) POSITIVE -- "this far inside my own region" -- and
    //     every OTHER candidate (a label found nearby but not this node's
    //     own) the same magnitude NEGATIVE, plus a small jitter to break
    //     ties among multiple competing candidates. Matches the existing
    //     own-positive/others-negative convention Term 3/the margin hinge
    //     already assume, and naturally seeds everything near 0 right at a
    //     boundary node (myDist==0 there), where no candidate should start
    //     with an unearned advantage.
    //   B-nodes: no ground truth, no OwnLabelSeed slot -- driven instead by
    //     the tight own-cube scan above (bOwnCubeUniform/bOwnLabel/
    //     bOwnAvgDist). If this B-node's own 8 A-corners all agree on one
    //     label, it ASSUMES that label exactly like an A-node assumes its
    //     ground truth: slot 0 (=bOwnLabel) seeds POSITIVE to the average
    //     of those 8 corners' footvector lengths, and every OTHER candidate
    //     (found only by the wider halo scan, e.g. a neighboring cube
    //     touching a different label) seeds to that SAME magnitude
    //     NEGATIVE plus jitter -- exactly A's own-positive/others-negative
    //     convention. Otherwise (own cube itself already mixed -- no single
    //     confident answer even before looking at neighbors) every
    //     candidate seeds to plain zero-centered jitter, deliberately no
    //     majority-vote/negShare confidence bias here, unlike the old
    //     scheme.
    if (Mode == 0) {
        float myDist = NodeFootDist[node];
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            float pot = 0.0;
            if (s < count) {
                pot = (s == 0) ? myDist : (-myDist + DistanceJitter(node, s) * SeedJitter);
            }
            NodePotential[node * MAX_CANDIDATES + s] = pot;
        }
    } else {
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            float pot = 0.0;
            if (s < count) {
                pot = bOwnCubeUniform
                    ? ((s == 0) ? bOwnAvgDist : (-bOwnAvgDist + DistanceJitter(node, s) * SeedJitter))
                    : (DistanceJitter(node, s) * SeedJitter);
            }
            NodePotential[node * MAX_CANDIDATES + s] = pot;
        }
    }
}
