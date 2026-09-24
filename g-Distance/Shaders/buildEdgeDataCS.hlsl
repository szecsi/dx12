#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// One-shot pass populating NodeEdgeData: 7 float4 slots per node, one per
// canonical "forward" edge direction (ForwardEdgeOffsets, DistanceLattice.hlsli)
// -- each undirected lattice edge is stored EXACTLY ONCE, at whichever
// endpoint reaches it via a forward offset (see ForwardSlotOf). Layout:
// .x = binary homo/hetero bit (1.0=hetero/labels differ, 0.0=homo/same
// label); .y/.z/.w reserved (unused -- edge-derivative multipliers now live
// in a dedicated per-NODE buffer, NodeEdgeDerivMult, since they're a
// genuinely per-node quantity, not a per-edge-end one -- see the approved
// plan's per-face redesign). Manually triggered ('H' key / "Build Edge
// Data" button) rather than wired into the automatic Reinit/Continue
// pipeline yet, same phased-rollout approach as the junction-footvector
// feature.
//
// Also seeds NodeEdgeDerivMult to the identity value 1.0 for every node --
// this runs (via 'H') on every scene that uses the edge-data raymarch, not
// just TestShape_ClippedSpheres, so this is the one place that guarantees
// the buffer is never read as uninitialized GPU memory by
// raymarchEdgePS.hlsl's CornerDerivMult before 'I' (TestShape_ClippedSpheres
// only, refines it further via smoothEdgeDerivMultCS.hlsl) has ever run.
#define BuildEdgeDataSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<uint>   NodeCandidateLabel : register(u0); // read
RWStructuredBuffer<float4> NodeEdgeData       : register(u1); // write
RWStructuredBuffer<float>  NodeEdgeDerivMult  : register(u2); // write (seeded to 1.0)

[RootSignature(BuildEdgeDataSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildEdgeDataCS(uint3 dtid : SV_DispatchThreadID)
{
    uint node = dtid.x;
    if (node >= NodeCount) return;

    uint myLabel = GetCandidateLabelAt(NodeCandidateLabel, node, 0u);
    int3 q = NodeQ(node);

    [unroll]
    for (uint s = 0; s < 7; s++) {
        uint neighborRef = ResolveCorner(q + ForwardEdgeOffsets[s]);
        uint neighborLabel = (neighborRef == SENTINEL_LABEL) ? 0u : GetCandidateLabelAt(NodeCandidateLabel, neighborRef, 0u);
        bool hetero = (neighborLabel != myLabel);
        NodeEdgeData[node * 7 + s] = float4(hetero ? 1.0 : 0.0, 0.0, 0.0, 0.0);
    }
    NodeEdgeDerivMult[node] = 1.0;
}
