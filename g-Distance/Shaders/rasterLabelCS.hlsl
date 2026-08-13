#include "TorusListCb.hlsli"
#include "TorusSdf.hlsli"
#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// A-sublattice only -- B has no ground truth (that's what the whole rest of
// g-Distance solves for).
//
// ShapeKind 0/1 (Torus/Ellipsoid): same last-shape-wins analytic-SDF rule as
// g-BCC's torusInitCS.hlsl.
// ShapeKind 2-6: discrete grid-index patterns, no SDF at all -- deliberately
// pathological "ambiguous cube" stress tests (DistanceApp.h's
// DistanceTestShape enum). All centered at the grid midpoint, +-
// PatternHalfLen along the pattern's axis/diagonal/plane:
//   2 SinglePoint:     one lone A-node, label 1.
//   3 Line:            a straight axis-aligned run of A-nodes.
//   4 DiagonalLine2D:  A-nodes touching only along a cube's FACE diagonal
//                      (e.g. (c,c,c) and (c+1,c+1,c) are opposite corners of
//                      one cube's top face; the other two corners of that
//                      face are background) -- a classic ambiguous-face case.
//   5 DiagonalLine3D:  A-nodes touching only along a cube's BODY diagonal
//                      (opposite corners of the whole cube) -- the classic
//                      ambiguous-cube case.
//   6 Slab:            a one-voxel-thick, axis-aligned square sheet (thin
//                      along Z, extending in X and Y) -- the 2D/sheet-like
//                      counterpart to Line's 1D/curve-like stress test.
//                      Interior nodes have up to 8 same-label same-sublattice
//                      neighbors (full in-plane Moore neighborhood) instead
//                      of Line's at most 2, stress-testing the connecting-
//                      node topology test and volume floor against a
//                      genuinely higher-connectivity thin feature.
//   8 Box2x2x2:        the single A-cube [c,c+1]^3 (all 8 corners), label 1
//                      -- the smallest genuinely-3D solid feature, exercises
//                      every B-node/tet fully interior to one filled A-cube
//                      without any of Line/Slab's 1D/2D degeneracy.
#define RasterSig "RootFlags(0)," \
    "CBV(b1)," \
    "UAV(u0)," \
    "CBV(b0)"

RWStructuredBuffer<uint> RasterLabel : register(u0);

static const int PatternHalfLen = 4;

[RootSignature(RasterSig)]
[numthreads(4, 4, 4)]
void rasterLabelCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    uint label = 0;

    if (ShapeKind <= 1) {
        float3 pos = APos((int3)tid);
        for (uint i = 0; i < nTorii; i++) {
            if (ShapeSd(pos, torii[i]) <= 0.0)
                label = torii[i].label;
        }
    } else {
        int c = (int)(GridRes / 2);
        int3 d = (int3)tid - int3(c, c, c);
        bool hit = false;
        if (ShapeKind == 2) {
            hit = (d.x == 0 && d.y == 0 && d.z == 0);
        } else if (ShapeKind == 3) {
            hit = (d.y == 0 && d.z == 0 && abs(d.x) <= PatternHalfLen);
        } else if (ShapeKind == 4) {
            hit = (d.z == 0 && d.x == d.y && abs(d.x) <= PatternHalfLen);
        } else if (ShapeKind == 5) {
            hit = (d.x == d.y && d.y == d.z && abs(d.x) <= PatternHalfLen);
        } else if (ShapeKind == 6) {
            hit = (d.z == 0 && abs(d.x) <= PatternHalfLen && abs(d.y) <= PatternHalfLen);
        } else if (ShapeKind == 8) {
            hit = (d.x == 0 || d.x == 1) && (d.y == 0 || d.y == 1) && (d.z == 0 || d.z == 1);
        }
        if (hit) label = 1;
    }

    RasterLabel[AIdx(tid.x, tid.y, tid.z)] = label;
}
