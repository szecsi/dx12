#include "DistanceCb.hlsli"
#include "DistanceLattice.hlsli"

// Per-node candidate label arrays (<=8 slots) + initial raw potentials.
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
// BCount -- unused slots are written as SENTINEL_LABEL, masked out by every
// consumer (FindSlot-style helpers never match SENTINEL_LABEL).
#define BuildCandidatesSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)"

cbuffer ModeConsts : register(b1) {
    uint Mode; // 0 = A-nodes, 1 = B-nodes
};

RWStructuredBuffer<uint>  RasterLabel : register(u0);
RWStructuredBuffer<uint>  NodeCandidateLabel : register(u1);
RWStructuredBuffer<float> NodePotential : register(u2);

[RootSignature(BuildCandidatesSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildCandidatesCS(uint3 tid : SV_DispatchThreadID)
{
    uint labels[8] = {
        SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL,
        SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL
    };
    uint count = 0;
    uint node;
    uint seedSlot = 0; // which slot (if any beyond A's fixed slot 0) gets OwnLabelSeed instead of jitter

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
        uint freq[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (int dz = -1; dz <= 2; dz++) {
            for (int dy = -1; dy <= 2; dy++) {
                for (int dx = -1; dx <= 2; dx++) {
                    int ai = (int)i + dx, aj = (int)j + dy, ak = (int)k + dz;
                    if (ai < 0 || aj < 0 || ak < 0 || ai >= (int)GridRes || aj >= (int)GridRes || ak >= (int)GridRes) continue;
                    uint aLabel = RasterLabel[(uint)ai + (uint)aj * GridRes + (uint)ak * GridRes * GridRes];
                    bool found = false;
                    for (uint s = 0; s < count; s++) if (labels[s] == aLabel) { freq[s]++; found = true; break; }
                    if (!found && count < 8) { labels[count] = aLabel; freq[count] = 1; count++; }
                }
            }
        }

        // B has no ground truth to fix as slot 0, but it still needs a
        // decisive starting point, same reasoning as A's OwnLabelSeed:
        // starting every candidate near a jittered 0 meant the margin hinge
        // (smoothnessJacobiCS.hlsl term 2) had to separate them from
        // scratch using only MaxPotentialStep-clamped steps -- far too slow
        // to reach MarginTarget within a practical iteration count (this
        // was confirmed directly: a debug readback found EVERY multi-
        // candidate B-node still sitting at a near-zero gap after a full
        // Reinitialize run). Seed the majority-vote label among the node's
        // 8 A-corners instead -- a well-informed guess the optimization is
        // still fully free to overturn later, exactly like A's seed.
        for (uint s = 1; s < count; s++) if (freq[s] > freq[seedSlot]) seedSlot = s;
    }

    for (uint s = 0; s < 8; s++) {
        NodeCandidateLabel[node * 8 + s] = labels[s];
        float pot = 0.0;
        if (s < count) {
            bool isSeed = (Mode == 0 && s == 0) || (Mode == 1 && s == seedSlot);
            pot = isSeed ? OwnLabelSeed : (DistanceJitter(node, s) * SeedJitter);
        }
        NodePotential[node * 8 + s] = pot;
    }
}
