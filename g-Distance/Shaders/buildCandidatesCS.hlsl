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
    // Mode==1 (B) only: per-slot corner-count / summed footvector length
    // over the halo scan below, for the uniform-halo averaging case in the
    // potential-seeding block further down. Declared here (not inside the
    // else block) so they're still in scope there.
    uint freq[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    float sumDist[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

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

        // Own cube is A-range [i,i+1]x[j,j+1]x[k,k+1]; scanning [i-1,i+2] in
        // each axis additionally covers all 6 face-neighbor cubes (e.g. the
        // -X neighbor cube needs x in {i-1,i}, already within this range)
        // plus some edge/corner-adjacent cubes for free -- harmless, capped
        // at 8 slots and nowhere near that cap with only a couple of labels
        // actually present in the scene.
        // freq/sumDist declared above (outer scope) -- per-slot corner count
        // and summed footvector length, freq[s] doubling as sumDist[s]'s
        // divisor for the uniform-halo averaging case further below.
        for (int dz = -1; dz <= 2; dz++) {
            for (int dy = -1; dy <= 2; dy++) {
                for (int dx = -1; dx <= 2; dx++) {
                    int ai = (int)i + dx, aj = (int)j + dy, ak = (int)k + dz;
                    if (ai < 0 || aj < 0 || ak < 0 || ai >= (int)GridRes || aj >= (int)GridRes || ak >= (int)GridRes) continue;
                    uint aIdx = (uint)ai + (uint)aj * GridRes + (uint)ak * GridRes * GridRes;
                    uint aLabel = RasterLabel[aIdx];
                    float aDist = NodeFootDist[aIdx];
                    bool found = false;
                    for (uint s = 0; s < count; s++) if (labels[s] == aLabel) { freq[s]++; sumDist[s] += aDist; found = true; break; }
                    if (!found && count < 8) { labels[count] = aLabel; freq[count] = 1; sumDist[count] = aDist; count++; }
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
    //   B-nodes: no ground truth, no OwnLabelSeed slot -- averaged instead,
    //     over the SAME 1-cube-halo scan used to discover B's candidate
    //     labels above (freq[]/sumDist[]). If that whole halo is uniformly
    //     ONE label (count==1 -- this B-node sits deep inside a single
    //     region, nowhere near a boundary), its one candidate seeds to the
    //     AVERAGE of those neighboring A-nodes' own footvector lengths --
    //     inheriting their "how deep inside" estimate directly, no jitter
    //     needed (nothing to break a tie against). Otherwise (count>1,
    //     genuinely near a boundary/junction, JFA has no single clean
    //     distance answer for this halo) every candidate seeds to plain
    //     zero-centered jitter -- deliberately no majority-vote/negShare
    //     confidence bias here, unlike the old scheme.
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
        bool isUniform = (count == 1);
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            float pot = 0.0;
            if (s < count) {
                pot = isUniform ? (sumDist[s] / (float)freq[s]) : (DistanceJitter(node, s) * SeedJitter);
            }
            NodePotential[node * MAX_CANDIDATES + s] = pot;
        }
    }
}
