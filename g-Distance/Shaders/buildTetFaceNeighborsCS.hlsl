#include "DistanceLattice.hlsli"

// Cross-orientation ("Type-1") face neighbors: each disphenoid tet {A0,A1,
// B0,B1} has 4 triangular faces -- {A0,A1,B0}, {A0,A1,B1} (Type-1, shared
// with a tet from a *different* grid-face orientation around the same
// A-edge) and {A0,B0,B1}, {A1,B0,B1} (Type-2/"fan", already handled
// arithmetically in smoothnessJacobiCS.hlsl via tetBase+-1/+1 within the
// same face). This pass finds each tet's up to 2 Type-1 neighbors (one per
// B-corner) so smoothness can propagate across face orientations, not just
// within one grid face's local 4-tet fan.
//
// Found by search rather than closed-form index arithmetic: for a Type-1
// face via corner B, the matching tet is some OTHER tet that also has B as
// one of its 2 B-corners AND has the same {A0,A1} edge. Every tet touching
// B is already enumerated in NodeIncidentTets[B] (built by
// buildIncidentCS.hlsl, which must run before this pass), so this is a
// bounded linear scan (<=MAX_INCIDENT_TETS) per face -- run once at init,
// not per Jacobi sweep, so the extra robustness is free where it matters.
// (Deliberately not derived by hand from the 3 face orientations' index
// arithmetic -- that derivation was attempted and got the axis bookkeeping
// wrong more than once; a search against already-correct topology data is
// far less error-prone than another 12-case manual rederivation.)
#define BuildTetFaceNeighborsSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)"

RWStructuredBuffer<uint4> Tets : register(u0);
RWStructuredBuffer<uint>  NodeIncidentCount : register(u1);
RWStructuredBuffer<uint>  NodeIncidentTets : register(u2);
RWStructuredBuffer<uint2> TetFaceNeighbors : register(u3);

uint FindSharedFaceNeighbor(uint selfTet, uint A0, uint A1, uint B)
{
    uint cnt = min(NodeIncidentCount[B], MAX_INCIDENT_TETS);
    for (uint e = 0; e < cnt; e++) {
        uint cand = NodeIncidentTets[B * MAX_INCIDENT_TETS + e];
        if (cand == selfTet) continue;
        uint4 ct = Tets[cand];
        bool hasB = (ct.z == B || ct.w == B);
        if (!hasB) continue;
        bool matchEdge = (ct.x == A0 && ct.y == A1) || (ct.x == A1 && ct.y == A0);
        if (matchEdge) return cand;
    }
    return SENTINEL_LABEL;
}

[RootSignature(BuildTetFaceNeighborsSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildTetFaceNeighborsCS(uint3 tid : SV_DispatchThreadID)
{
    uint t = tid.x;
    if (t >= TetCount) return;

    uint4 tet = Tets[t];
    uint cross0 = FindSharedFaceNeighbor(t, tet.x, tet.y, tet.z); // via B0
    uint cross1 = FindSharedFaceNeighbor(t, tet.x, tet.y, tet.w); // via B1
    TetFaceNeighbors[t] = uint2(cross0, cross1);
}
