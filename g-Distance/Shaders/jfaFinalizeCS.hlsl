#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// JFA finalize, pass 3 of 3 (see jfaInitCS.hlsl/jfaStepCS.hlsl): converts
// the converged seed-node index into an actual real-space distance -- this
// A-node's "footvector length", the distance to the nearest A-node whose
// ground-truth label differs from its own. Read by buildCandidatesCS.hlsl
// to seed each candidate's initial potential as an approximate distance-
// to-boundary field. No known seed (SENTINEL_LABEL, only possible if the
// WHOLE domain is one uniform label with no boundary anywhere) falls back
// to 0 -- no distance information exists, so no bias is injected.
#define JfaFinalizeSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "CBV(b0)"

RWStructuredBuffer<uint> JfaSeedFinal : register(u0);
RWStructuredBuffer<float> NodeFootDist : register(u1);

[RootSignature(JfaFinalizeSig)]
[numthreads(4, 4, 4)]
void jfaFinalizeCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;
    uint idx = AIdx(tid.x, tid.y, tid.z);
    uint seed = JfaSeedFinal[idx];

    float dist = 0.0;
    if (seed != SENTINEL_LABEL) {
        bool isB; uint3 sIjk; DecodeNodeIndex(seed, isB, sIjk);
        dist = length(APos((int3)tid) - APos((int3)sIjk));
    }
    NodeFootDist[idx] = dist;
}
