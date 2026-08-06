#include "AequorCb.hlsli"
#include "GridUtils.hlsli"
#include "TorusSdf.hlsli"

#define RasterLabelSig "RootFlags(0)," \
    "CBV(b0)," \
    "CBV(b1)," \
    "UAV(u0)"

RWStructuredBuffer<uint> RasterLabel : register(u0);

// Ground-truth per-voxel label for the (single, simple-cubic) rasterization
// grid -- last-matching-shape-wins, same rule g-BCC's torusInitCS.hlsl uses.
[RootSignature(RasterLabelSig)]
[numthreads(4, 4, 4)]
void rasterLabelCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GRID_DIM)) return;

    float3 pos = CellCenterPos((int3)tid);

    uint label = 0;
    for (uint i = 0; i < nTorii; i++) {
        if (ShapeSd(pos, torii[i]) <= 0.0)
            label = torii[i].label;
    }

    RasterLabel[cellIndex((int3)tid)] = label;
}
