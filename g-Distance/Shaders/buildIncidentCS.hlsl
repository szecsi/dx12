#include "DistanceLattice.hlsli"

// Reverse adjacency: for each node, which tets touch it. smoothnessJacobiCS
// needs to GATHER, per unknown, the tet pairs it participates in (matching
// this codebase's established gather-not-scatter relaxation style -- no
// float atomics are used anywhere). Built via a scatter pass using ONLY
// standard uint InterlockedAdd (always available, unlike float atomics),
// once at init, since tet connectivity never changes.
#define BuildIncidentSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)"

RWStructuredBuffer<uint4> Tets : register(u0);
RWStructuredBuffer<uint>  NodeIncidentCount : register(u1);
RWStructuredBuffer<uint>  NodeIncidentTets : register(u2);

[RootSignature(BuildIncidentSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildIncidentCS(uint3 tid : SV_DispatchThreadID)
{
    uint tetIdx = tid.x;
    if (tetIdx >= TetCount) return;

    uint4 t = Tets[tetIdx];
    uint verts[4] = { t.x, t.y, t.z, t.w };

    for (uint c = 0; c < 4; c++) {
        uint node = verts[c];
        uint idx;
        InterlockedAdd(NodeIncidentCount[node], 1, idx);
        if (idx < MAX_INCIDENT_TETS) {
            NodeIncidentTets[node * MAX_INCIDENT_TETS + idx] = tetIdx;
        }
    }
}
