#pragma once

#include "GridUtils.hlsli"

// Single-grid Jump Flood Algorithm helpers for the occurrence-distance field
// (occSeedCS/occStepCS/occFinalizeCS): for each label L, "is there a voxel of
// label L within radius R of my current position" (anchorBarrierCS.hlsl)
// needs the nearest voxel actually rasterized as label L, not just L's
// boundary -- unlike g-BCC's jfaSeedCS/jfaStepCS, there is exactly one
// simple-cubic grid here (no A/B sublattice split), so this is a plain
// single-buffer propagation.

// The 26 same-lattice neighbor offsets (3x3x3 minus the center), scaled by
// the current JFA step size when used -- same set g-BCC's bccCommon.hlsli
// uses, just not tied to a BCC sublattice here.
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

// bits[0:9]=i, [10:19]=j, [20:29]=k -- no sublattice bit needed (single grid).
uint EncodeCellSeed(uint3 idx)
{
    return (idx.x & 0x3FFu) | ((idx.y & 0x3FFu) << 10) | ((idx.z & 0x3FFu) << 20);
}

float3 CellSeedWorldPos(uint packed)
{
    uint3 idx;
    idx.x = packed & 0x3FFu;
    idx.y = (packed >> 10) & 0x3FFu;
    idx.z = (packed >> 20) & 0x3FFu;
    return CellCenterPos((int3)idx);
}

bool InGridBounds(int3 idx)
{
    return all(idx >= 0) && all(idx < (int)GRID_DIM);
}
