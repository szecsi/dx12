#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// One-time-per-outer-round snapshot of each node's current winning
// candidate label, used ONLY by smoothnessJacobiCS.hlsl's Term 1 to decide
// which edges are "active" and which (Li,Lj) pair to compare gradients for
// -- frozen for the whole round's jacobiSweepsPerRound inner sweeps,
// matching the old TetInterfacePair/assignInterfacePairsCS cadence exactly
// ("freeze combinatorics for one round, solve, re-derive", the Lloyd-loop
// design this project has used throughout). Without this, deriving the
// active pair from the LIVE (every-sweep-changing) winner destabilizes the
// solve: a node whose margin is thin can flip its own winner mid-round,
// discontinuously changing what Term 1 is even optimizing toward from one
// sweep to the next -- confirmed empirically (raising SmoothnessWeight made
// the surface visibly wane/erode, worse not better, the opposite of what a
// correctly-signed-but-underweighted term would do).
//
// Only the LABEL identity is frozen here -- the actual potential VALUES
// AccumulateEdgePair reads (via GetCornerPotential) stay fully live every
// sweep, exactly like the old TetInterfacePair scheme: it froze WHICH
// labels were being compared, never the values being compared.
#define SnapshotWinnerSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);
RWStructuredBuffer<uint>  NodeFrozenWinner : register(u2);

[RootSignature(SnapshotWinnerSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void snapshotWinnerCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;

    uint bestLabel = SENTINEL_CANDIDATE;
    float bestPot = -1.0e30;
    for (uint s = 0; s < MAX_CANDIDATES; s++) {
        uint l = GetCandidateLabelAt(NodeCandidateLabel, node, s);
        if (l == SENTINEL_CANDIDATE) continue;
        float p = NodePotential[node * MAX_CANDIDATES + s];
        if (p > bestPot) { bestPot = p; bestLabel = l; }
    }
    NodeFrozenWinner[node] = bestLabel;
}
