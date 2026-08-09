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
    "RootConstants(num32BitConstants=1, b1)," \
    "UAV(u0)," \
    "UAV(u1)"

cbuffer ConnectingConsts : register(b1) {
    uint EdgeConnectivityOnly; // 1 = require a shared edge (18-connectivity), 0 = any corner touch counts (26-connectivity)
};

RWStructuredBuffer<uint> RasterLabel : register(u0);
RWStructuredBuffer<uint> NodeIsConnecting : register(u1);

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

    int3 offsets[26];
    uint sameLabelCount = 0;
    for (uint n = 0; n < 26; n++) {
        int3 nb = int3((int)i, (int)j, (int)k) + SameLatticeOffsets[n];
        if (any(nb < 0) || any(nb >= (int)GridRes)) continue;
        if (RasterLabel[AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z)] != ownLabel) continue;
        offsets[sameLabelCount] = SameLatticeOffsets[n];
        sameLabelCount++;
    }

    // 0 same-label neighbors: a fully isolated node IS the entire feature --
    // there's nothing else to protect it, so it's the degenerate "sole
    // connector of itself" case and must be pinned too (otherwise an
    // isolated SinglePoint has no floor protection at all).
    if (sameLabelCount == 0) {
        NodeIsConnecting[node] = 1;
        return;
    }

    // Exactly 1 same-label neighbor (a line/chain endpoint) -- nothing to
    // disconnect among a single neighbor, and it isn't the feature's only
    // anchor (its one neighbor is), so it stays unpinned/free to shrink
    // under smoothness.
    if (sameLabelCount == 1) {
        NodeIsConnecting[node] = 0;
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
    NodeIsConnecting[node] = (visitedCount < sameLabelCount) ? 1 : 0;
}
