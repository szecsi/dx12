#include "DistanceLattice.hlsli"

// Zeroes NodeIncidentCount before buildIncidentCS.hlsl's atomic scatter --
// this codebase has no ClearUnorderedAccessViewUint path available (no
// shader-visible descriptor heap is created for these raw root-UAV buffers),
// so a tiny dedicated clear kernel stands in for it.
#define ClearIncidentSig "RootFlags(0)," \
    "UAV(u0)"

RWStructuredBuffer<uint> NodeIncidentCount : register(u0);

[RootSignature(ClearIncidentSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void clearIncidentCS(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= NodeCount) return;
    NodeIncidentCount[tid.x] = 0;
}
