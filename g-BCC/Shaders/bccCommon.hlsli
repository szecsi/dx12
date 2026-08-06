#pragma once

// Must match BccApp::GridRes / BccApp::CellSize (kept in sync by hand).
static const uint  GridRes  = 48;
static const float CellSize = 1.0;

static const uint SENTINEL_LABEL = 0xFFFFFFFF;

// The 26 same-sublattice neighbor offsets (3x3x3 minus the center), scaled by
// the current JFA step size when used.
static const int3 SameLatticeOffsets[26] = {
    int3(-1,-1,-1), int3(0,-1,-1), int3(1,-1,-1),
    int3(-1, 0,-1), int3(0, 0,-1), int3(1, 0,-1),
    int3(-1, 1,-1), int3(0, 1,-1), int3(1, 1,-1),
    int3(-1,-1, 0), int3(0,-1, 0), int3(1,-1, 0),
    int3(-1, 0, 0),                int3(1, 0, 0),
    int3(-1, 1, 0), int3(0, 1, 0), int3(1, 1, 0),
    int3(-1,-1, 1), int3(0,-1, 1), int3(1,-1, 1),
    int3(-1, 0, 1), int3(0, 0, 1), int3(1, 0, 1),
    int3(-1, 1, 1), int3(0, 1, 1), int3(1, 1, 1),
};

// The 8 nearest neighbors of an A-lattice point (world (i,j,k)*CellSize) live
// on the B-lattice (world (i+.5,j+.5,k+.5)*CellSize) at B-index (i+di,j+dj,k+dk)
// for di,dj,dk in {-1,0}.
static const int3 CrossOffsetsAtoB[8] = {
    int3(-1,-1,-1), int3(0,-1,-1), int3(-1,0,-1), int3(0,0,-1),
    int3(-1,-1, 0), int3(0,-1, 0), int3(-1,0, 0), int3(0,0, 0),
};

// Mirror image: the 8 nearest A-neighbors of a B-lattice point, at
// A-index (i+di,j+dj,k+dk) for di,dj,dk in {0,1}.
static const int3 CrossOffsetsBtoA[8] = {
    int3(0,0,0), int3(1,0,0), int3(0,1,0), int3(1,1,0),
    int3(0,0,1), int3(1,0,1), int3(0,1,1), int3(1,1,1),
};

float3 APos(int3 idx) {
    return float3(idx) * CellSize;
}

float3 BPos(int3 idx) {
    return (float3(idx) + 0.5) * CellSize;
}

bool InBounds(int3 idx) {
    return all(idx >= 0) && all(idx < (int)GridRes);
}

// bit31 = sublattice (0=A,1=B), bits[0:9]=i, [10:19]=j, [20:29]=k
uint PackSeed(bool isB, uint3 idx) {
    return (isB ? 0x80000000u : 0u) | (idx.x & 0x3FFu) | ((idx.y & 0x3FFu) << 10) | ((idx.z & 0x3FFu) << 20);
}

void UnpackSeed(uint packed, out bool isB, out uint3 idx) {
    isB   = (packed & 0x80000000u) != 0;
    idx.x = packed & 0x3FFu;
    idx.y = (packed >> 10) & 0x3FFu;
    idx.z = (packed >> 20) & 0x3FFu;
}

float3 SeedWorldPos(uint packed) {
    bool isB; uint3 idx;
    UnpackSeed(packed, isB, idx);
    return isB ? BPos((int3)idx) : APos((int3)idx);
}
