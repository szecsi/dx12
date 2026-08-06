#include "AequorCb.hlsli"
#include "GridUtils.hlsli"
#include "TorusSdf.hlsli"
#include "AnalyticShape.hlsli"

#define SpawnSig "RootFlags(0)," \
    "CBV(b0)," \
    "CBV(b1)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "UAV(u6)"

RWStructuredBuffer<uint> RasterLabel : register(u0);

RWStructuredBuffer<uint>   Counter        : register(u1); // single element: next free particle slot
RWStructuredBuffer<float3> Position       : register(u2);
RWStructuredBuffer<float3> Normal         : register(u3);
RWStructuredBuffer<float3> TensorDiag     : register(u4);
RWStructuredBuffer<float3> TensorOffdiag  : register(u5);
RWStructuredBuffer<uint>   Label          : register(u6);

// Finds the torii[] entry actually carrying `targetLabel`, or nTorii (not
// found -- true for targetLabel == 0, background, which has no shape entry
// of its own).
bool FindTorusByLabel(uint targetLabel, out uint idx)
{
    for (uint i = 0; i < nTorii; i++) {
        if (torii[i].label == targetLabel) { idx = i; return true; }
    }
    idx = nTorii;
    return false;
}

uint FindNearestTorus(float3 p)
{
    uint best = 0;
    float bestAbs = abs(ShapeSd(p, torii[0]));
    for (uint i = 1; i < nTorii; i++) {
        float ad = abs(ShapeSd(p, torii[i]));
        if (ad < bestAbs) { bestAbs = ad; best = i; }
    }
    return best;
}

// Projects onto the analytic surface for `targetLabel` at approximately
// `nearPos`, and writes that label's own outward-normal-convention
// position/normal/tensor into slot `dest`. Background (label 0) has no
// shape of its own -- it borrows the nearest shape's surface point, but
// keeps that shape's own canonical outward convention (background is
// exterior, on the same side the shape's own SDF gradient already points
// to). Any other label owns exactly one shape entry and sits on its
// INTERIOR side, so both normal and curvature flip -- same convention
// g-BCC's quadricSeedCS.hlsl SeedSlot uses.
void WriteParticle(uint dest, uint targetLabel, float3 nearPos)
{
    uint shapeIdx;
    bool isInterior = FindTorusByLabel(targetLabel, shapeIdx);
    if (!isInterior)
        shapeIdx = FindNearestTorus(nearPos);

    float3 canonicalOutward;
    float3x3 canonicalA = AnalyticShapeTensor(nearPos, torii[shapeIdx], canonicalOutward);
    float3 surfacePos = ShapeNearestPoint(nearPos, torii[shapeIdx]);

    float flip = isInterior ? -1.0 : 1.0;

    Position[dest] = surfacePos;
    Normal[dest] = flip * canonicalOutward;
    float3x3 A = flip * canonicalA;
    TensorDiag[dest] = float3(A[0][0], A[1][1], A[2][2]);
    TensorOffdiag[dest] = float3(A[0][1], A[0][2], A[1][2]);
    Label[dest] = targetLabel;
}

static const int3 FaceOffsets[6] = {
    int3(-1,0,0), int3(1,0,0), int3(0,-1,0), int3(0,1,0), int3(0,0,-1), int3(0,0,1)
};

// One thread per rasterization cell. Boundary cells (own label differs from
// at least one face neighbor) spawn one particle per distinct label found
// among {own} + {6 face neighbors} -- "boundary cells, all present labels
// granted some points". Non-boundary cells spawn nothing.
[RootSignature(SpawnSig)]
[numthreads(4, 4, 4)]
void spawnCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GRID_DIM)) return;

    uint ownLabel = RasterLabel[cellIndex((int3)tid)];

    uint candidates[7];
    uint candidateCount = 0;
    candidates[candidateCount++] = ownLabel;

    bool isBoundary = false;
    for (uint f = 0; f < 6; f++) {
        int3 nIdx = (int3)tid + FaceOffsets[f];
        if (nIdx.x < 0 || nIdx.y < 0 || nIdx.z < 0 ||
            nIdx.x >= (int)GRID_DIM || nIdx.y >= (int)GRID_DIM || nIdx.z >= (int)GRID_DIM)
            continue;
        uint nLabel = RasterLabel[cellIndex(nIdx)];
        if (nLabel != ownLabel) {
            isBoundary = true;
            candidates[candidateCount++] = nLabel;
        }
    }

    if (!isBoundary) return;

    float3 cellPos = CellCenterPos((int3)tid);

    for (uint i = 0; i < candidateCount; i++) {
        uint lbl = candidates[i];
        bool seenBefore = false;
        for (uint j = 0; j < i; j++)
            if (candidates[j] == lbl) { seenBefore = true; break; }
        if (seenBefore) continue;

        uint dest;
        InterlockedAdd(Counter[0], 1, dest);
        if (dest >= numParticles) return; // capacity reached

        WriteParticle(dest, lbl, cellPos);
    }
}
