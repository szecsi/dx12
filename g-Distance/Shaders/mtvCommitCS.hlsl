#include "DistanceLattice.hlsli"

// Outer-round MTV step 3 of 3: scratch -> main copy, closing out one round's
// MTV diffusion + flip-share adjustment (mtvDiffuseCS.hlsl). Mirrors
// commitPotentialCS.hlsl, but NodeCount-sized (one MTV/prev-label per node,
// not per candidate slot) and copies two buffer pairs instead of one.
#define MtvCommitSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)"

RWStructuredBuffer<float> NodeMTVScratch : register(u0);
RWStructuredBuffer<float> NodeMTV : register(u1);
RWStructuredBuffer<uint>  NodePrevLabelScratch : register(u2);
RWStructuredBuffer<uint>  NodePrevLabel : register(u3);

[RootSignature(MtvCommitSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void mtvCommitCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;
    NodeMTV[node] = NodeMTVScratch[node];
    NodePrevLabel[node] = NodePrevLabelScratch[node];
}
