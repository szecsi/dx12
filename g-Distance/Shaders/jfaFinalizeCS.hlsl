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
//
// Also writes NodeFootVector: the actual direction, not just the length --
// APos(this node) - APos(seed), i.e. pointing AWAY from the nearest
// differently-labeled node, TOWARD this node. Same SENTINEL fallback (0,0,0)
// as NodeFootDist's 0. Used by computeConnectingNodesCS.hlsl's divergent-
// node test (comparing two same-label neighbors' footvector directions via
// dot product) -- NodeFootDist alone only carries magnitude, not enough to
// tell whether two nearby nodes are pointing "away from the same patch of
// boundary" or "away from opposite patches."
#define JfaFinalizeSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<uint>   JfaSeedFinal : register(u0);
RWStructuredBuffer<float>  NodeFootDist : register(u1);
RWStructuredBuffer<float3> NodeFootVector : register(u2);

[RootSignature(JfaFinalizeSig)]
[numthreads(4, 4, 4)]
void jfaFinalizeCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;
    uint idx = AIdx(tid.x, tid.y, tid.z);
    uint seed = JfaSeedFinal[idx];

    float3 myPos = APos((int3)tid);
    float dist = 0.0;
    float3 vec = float3(0, 0, 0);
    if (seed != SENTINEL_LABEL) {
        bool isB; uint3 sIjk; DecodeNodeIndex(seed, isB, sIjk);
        float3 seedPos = APos((int3)sIjk);
        dist = length(myPos - seedPos) + 0.5;
        vec = myPos - seedPos;
    }
    NodeFootDist[idx] = dist;
    NodeFootVector[idx] = vec;
}
