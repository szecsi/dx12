#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"
#include "SparseLabelSeed.hlsli"

#define SparseLabelClearSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)"

RWStructuredBuffer<SparseNodeSeeds> Seeds : register(u0);

// Resets every node's sparse label record to "all 4 slots empty" before the
// K per-label harvest passes (sparseLabelHarvestCS.hlsl) start inserting into
// them.
[RootSignature(SparseLabelClearSig)]
[numthreads(THREADS_X, 1, 1)]
void sparseLabelClearCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    uint totalCount = 2u * LatticeNodeCount;
    if (index >= totalCount) return;

    SparseNodeSeeds s;
    s.labels = uint4(SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL);
    s.dists  = float4(1.0e30, 1.0e30, 1.0e30, 1.0e30);
    s.seeds  = uint4(0, 0, 0, 0);
    Seeds[index] = s;
}
