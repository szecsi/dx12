#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// One-time pass (per Reinitialize, right after rasterLabelCS): flags every
// A-node whose removal would locally disconnect its same-label
// same-sublattice neighbors from each other -- the "simple point" test from
// digital topology / topology-preserving thinning (a voxel is safe to
// remove only if its foreground neighbors form a single connected
// component; this flags the FAILURE case, i.e. the voxel is the only thing
// holding its same-label 3x3x3 neighborhood together).
//
// A flagged "connecting" node gets a one-sided volume floor in
// smoothnessJacobiCS.hlsl's Term 4 (VolumeFloor, default 1.0): zero force
// once its CurrentVolume is at/above the floor, but pushed back up whenever
// it dips below. Non-connecting nodes get no floor at all -- free to shrink
// or grow purely under smoothness. This is what stops a 1-voxel-wide
// connected feature (e.g. the Line test shape) from pinching into separate
// blobs at its ends, and what protects a fully isolated single-voxel
// feature (sameLabelCount==0 below) from shrinking to nothing.
//
// NodeIsConnecting is now a 3-bit flag field, not a plain bool:
//   bit 0 (value 1): the "connecting" flag above.
//   bit 1 (value 2): a separate LOCAL MAX flag -- this node's NodeFootDist
//     (JFA footvector length) is >= every same-label same-sublattice
//     neighbor's (the same 26-neighbor/3x3x3 stencil used for the
//     connectivity test above, NOT gated by EdgeConnectivityOnly -- this is
//     about same-label membership only, unrelated to that test's edge-vs-
//     corner adjacency distinction). Ties count as still-local-max (>=, not
//     >) -- a flat plateau of equal footdist values all get flagged, not
//     just a single arbitrary peak. An isolated node (sameLabelCount==0)
//     has no same-label neighbor to lose to, so it's vacuously a local max
//     too.
//   bit 2 (value 4): DIVERGENT -- this node has a same-label 26-neighbor
//     whose NodeFootVector (JFA footvector DIRECTION, jfaFinalizeCS.hlsl)
//     disagrees with this node's own, i.e. dot(myVec, neighborVec) is below
//     DivergenceThreshold (GUI slider, root constant, applied on
//     Reinitialize -- "below zero" by default means genuinely opposite-
//     pointing vectors, but adjustable either direction). Marks a node
//     sitting between two DIFFERENT patches of boundary that happen to pull
//     its footvector in incompatible directions -- e.g. a saddle/pinch
//     region of the same-label volume -- distinct from both the
//     connectivity test (topological) and the local-max test (this node's
//     own distance ranking among its same-label peers).
//
// EdgeConnectivityOnly (root constant, GUI-toggleable, applied on
// Reinitialize) chooses which local adjacency counts as "still connected
// without me": 18-connectivity (share at least an edge, EdgeConnectivityOnly
// != 0) is the default -- stricter than 26-connectivity (any corner touch
// counts, EdgeConnectivityOnly == 0), since two same-label voxels touching
// only at a shared corner still visually reads as "about to separate," not
// solid. Every 1-voxel-thin test path (Line, DiagonalLine2D, DiagonalLine3D)
// flags its interior nodes as connecting regardless of this setting, since
// their same-label same-sublattice neighbors always sit 2 grid steps apart
// from EACH OTHER (well outside either connectivity's reach) -- the setting
// only matters for neighborhoods with some redundant shortcut connectivity.
#define ComputeConnectingSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=2, b1)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b0)"

cbuffer ConnectingConsts : register(b1) {
    uint EdgeConnectivityOnly; // 1 = require a shared edge (18-connectivity), 0 = any corner touch counts (26-connectivity)
    float DivergenceThreshold; // dot(myFootVector, neighborFootVector) below this => divergent
};

RWStructuredBuffer<uint>   RasterLabel : register(u0);
RWStructuredBuffer<uint>   NodeIsConnecting : register(u1);
RWStructuredBuffer<float>  NodeFootDist : register(u2);   // read-only here -- already finalized by jfaFinalizeCS earlier in RunTopologyBuild
RWStructuredBuffer<float3> NodeFootVector : register(u3); // read-only here -- ditto

[RootSignature(ComputeConnectingSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void computeConnectingNodesCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= ACount) return;

    uint k = node / (GridRes * GridRes);
    uint rem = node % (GridRes * GridRes);
    uint j = rem / GridRes;
    uint i = rem % GridRes;

    uint ownLabel = RasterLabel[node];
    float ownDist = NodeFootDist[node];
    float3 ownVec = NodeFootVector[node];

    int3 offsets[26];
    uint sameLabelCount = 0;
    bool isLocalMax = true;  // disqualified below by any strictly-greater same-label neighbor; ties keep it true
    bool isDivergent = false;
    for (uint n = 0; n < 26; n++) {
        int3 nb = int3((int)i, (int)j, (int)k) + SameLatticeOffsets[n];
        if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
        uint nbIdx = AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z);
        if (RasterLabel[nbIdx] != ownLabel) continue;
        if (NodeFootDist[nbIdx] > ownDist) isLocalMax = false;
        if (dot(ownVec, NodeFootVector[nbIdx]) < DivergenceThreshold) isDivergent = true;
        offsets[sameLabelCount] = SameLatticeOffsets[n];
        sameLabelCount++;
    }
    uint extraFlags = (isLocalMax ? 2u : 0u) | (isDivergent ? 4u : 0u);

    // 0 same-label neighbors: a fully isolated node IS the entire feature --
    // there's nothing else to protect it, so it's the degenerate "sole
    // connector of itself" case and must be pinned too (otherwise an
    // isolated SinglePoint has no floor protection at all).
    if (sameLabelCount == 0) {
        NodeIsConnecting[node] = extraFlags | 1u;
        return;
    }

    // Exactly 1 same-label neighbor (a line/chain endpoint) -- nothing to
    // disconnect among a single neighbor, and it isn't the feature's only
    // anchor (its one neighbor is), so it stays unpinned/free to shrink
    // under smoothness.
    if (sameLabelCount == 1) {
        NodeIsConnecting[node] = extraFlags;
        return;
    }

    // Local flood-fill among the same-label neighbor set (<=26 elements,
    // bounded fixed-point iteration -- cheap, one-time, no global data).
    bool visited[26];
    for (uint c0 = 0; c0 < 26; c0++) visited[c0] = (c0 == 0);

    for (uint iter = 0; iter < 26; iter++) {
        bool changed = false;
        for (uint a = 0; a < sameLabelCount; a++) {
            if (visited[a]) continue;
            for (uint b = 0; b < sameLabelCount; b++) {
                if (a == b || !visited[b]) continue;
                int3 d = offsets[a] - offsets[b];
                uint adx = (uint)abs(d.x), ady = (uint)abs(d.y), adz = (uint)abs(d.z);
                bool within26 = (adx <= 1) && (ady <= 1) && (adz <= 1);
                bool isCornerOnly = (adx == 1) && (ady == 1) && (adz == 1);
                bool adjacent = within26 && (EdgeConnectivityOnly == 0 || !isCornerOnly);
                if (adjacent) { visited[a] = true; changed = true; break; }
            }
        }
        if (!changed) break;
    }

    uint visitedCount = 0;
    for (uint c1 = 0; c1 < sameLabelCount; c1++) if (visited[c1]) visitedCount++;

    // Not all mutually reachable without going through this node -- it's the
    // sole connector, protect it.
    NodeIsConnecting[node] = extraFlags | ((visitedCount < sameLabelCount) ? 1u : 0u);
}
