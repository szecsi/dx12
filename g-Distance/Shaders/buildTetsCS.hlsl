#include "DistanceLattice.hlsli"

// Disphenoid tet enumeration -- the one genuinely new piece of geometry math
// in this project (nothing in g-BCC enumerates actual tets, only implicit
// 8-neighbor cross-sublattice offset tables).
//
// Take any interior face of the A cubic grid (an axis-aligned unit square,
// shared by exactly 2 cubes -- boundary faces, with only 1 adjacent cube,
// are skipped entirely: no tets, no interface possible at the domain edge).
// That face has 4 edges (a cyclic loop v0-v1-v2-v3-v0 around the square) and
// exactly 2 B-centers (the two cubes on either side). Each edge, combined
// with those same 2 B-centers, is one disphenoid: {edge.v0, edge.v1, B0, B1}
// -- 2 A + 2 B, matching the target shape exactly. A face therefore emits
// exactly 4 tets, built once here (interior faces are only visited once,
// not once per adjacent cube).
//
// Tet indexing is faceIndex*4 + slot (slot = which of the face's 4 edges).
// This is deliberate: consecutive slots (slot, (slot+1)%4) are exactly the
// two tets that share the face's shared-A-vertex triangle {sharedVertex, B0,
// B1} (a "fan" neighbor pair) -- smoothnessJacobiCS.hlsl computes these
// neighbor tet indices with pure arithmetic (tetBase + (slot+1)%4), no
// separate TetFaceNeighbors buffer needed. (v1 simplification: a disphenoid
// also has two more triangular faces of the form {A0,A1,B} shared with tets
// from *other* face orientations around the same A-edge -- not linked here,
// so smoothness only propagates within each grid-face's local 4-tet fan.
// Worth revisiting if that turns out too weak to visibly converge.)

#define BuildTetsSig "RootFlags(0)," \
    "UAV(u0)"

RWStructuredBuffer<uint4> Tets : register(u0);

[RootSignature(BuildTetsSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildTetsCS(uint3 tid : SV_DispatchThreadID)
{
    uint faceIndex = tid.x;
    if (faceIndex >= TotalFaces) return;

    uint orientation = faceIndex / FacesPerOrientation;
    uint local = faceIndex % FacesPerOrientation;
    uint a = local / (Ni * Ni);   // 0..Nx-1 -> normal-axis coordinate = a+1 (interior only)
    uint rem = local % (Ni * Ni);
    uint b = rem / Ni;            // 0..Ni-1
    uint c = rem % Ni;            // 0..Ni-1

    uint4 v; // face's 4 A-corners in loop order v0,v1,v2,v3
    uint b0, b1; // the face's 2 adjacent B-centers

    if (orientation == 0) {
        // X-facing (normal along X): face at x=a+1, spanning j..j+1, k..k+1.
        uint x = a + 1, j = b, k = c;
        v = uint4(AIdx(x, j, k), AIdx(x, j + 1, k), AIdx(x, j + 1, k + 1), AIdx(x, j, k + 1));
        b0 = BIdx(x - 1, j, k);
        b1 = BIdx(x, j, k);
    } else if (orientation == 1) {
        // Y-facing (normal along Y): face at y=a+1, spanning i..i+1, k..k+1.
        uint i = b, y = a + 1, k = c;
        v = uint4(AIdx(i, y, k), AIdx(i + 1, y, k), AIdx(i + 1, y, k + 1), AIdx(i, y, k + 1));
        b0 = BIdx(i, y - 1, k);
        b1 = BIdx(i, y, k);
    } else {
        // Z-facing (normal along Z): face at z=a+1, spanning i..i+1, j..j+1.
        uint i = b, j = c, z = a + 1;
        v = uint4(AIdx(i, j, z), AIdx(i + 1, j, z), AIdx(i + 1, j + 1, z), AIdx(i, j + 1, z));
        b0 = BIdx(i, j, z - 1);
        b1 = BIdx(i, j, z);
    }

    uint tetBase = faceIndex * 4;
    Tets[tetBase + 0] = uint4(v.x, v.y, b0, b1); // edge0: v0-v1
    Tets[tetBase + 1] = uint4(v.y, v.z, b0, b1); // edge1: v1-v2
    Tets[tetBase + 2] = uint4(v.z, v.w, b0, b1); // edge2: v2-v3
    Tets[tetBase + 3] = uint4(v.w, v.x, b0, b1); // edge3: v3-v0
}
