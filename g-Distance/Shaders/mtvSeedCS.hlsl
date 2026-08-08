#include "DistanceLattice.hlsli"

// Seeds the Momentary Target Volume (MTV) field once per Reinitialize, right
// after buildCandidatesCS.hlsl has (re)seeded candidates/potentials. A-nodes
// start at MTV=1 (one full "unit" -- their own raster voxel); B-nodes start
// at MTV=0 (they own nothing yet, matching their zero-jitter/no-ground-truth
// start). Also seeds NodePrevLabel to each node's initial winning label, so
// mtvFlipDetectCS.hlsl's very first round doesn't spuriously treat "starting
// state" as "everyone just flipped".
#define MtvSeedSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)"

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);
RWStructuredBuffer<float> NodeMTV : register(u2);
RWStructuredBuffer<uint>  NodePrevLabel : register(u3);

uint TopLabelOf(uint node)
{
    uint bestLabel = SENTINEL_LABEL;
    float bestPot = -1.0e30;
    for (uint s = 0; s < 8; s++) {
        uint l = NodeCandidateLabel[node * 8 + s];
        if (l == SENTINEL_LABEL) continue;
        float p = NodePotential[node * 8 + s];
        if (p > bestPot) { bestPot = p; bestLabel = l; }
    }
    return bestLabel;
}

[RootSignature(MtvSeedSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void mtvSeedCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;

    NodeMTV[node] = (node < ACount) ? 1.0 : 0.0;
    NodePrevLabel[node] = TopLabelOf(node);
}
