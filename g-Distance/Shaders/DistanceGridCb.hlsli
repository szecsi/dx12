#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _DIST_ALIGN_GRID __declspec(align(16))
#else
#define _DIST_ALIGN_GRID
// Register is configurable per-file (unlike DistanceCb.hlsli's fixed b0):
// this header is included by ~8 different compute/vertex shaders, each with
// its own existing root-signature slot usage, so every consuming file must
// #define DISTANCE_GRID_CB_REGISTER bN to its own next-free CBV slot BEFORE
// including this file (see DistanceApp.h's per-file slot table / the
// approved plan). Falls back to b0 if unset.
#ifndef DISTANCE_GRID_CB_REGISTER
#define DISTANCE_GRID_CB_REGISTER b0
#endif
#endif

// Runtime grid-size parameters -- everything about the lattice used to be a
// compile-time GRID_RES; now the grid resolution is a GUI slider (applied on
// Reinitialize), so every value derived from it must be read from this
// cbuffer instead. DistanceLattice.hlsli's functions (AIdx/BIdx/ResolveCorner
// /QWorldPos/GetTetCornerQs/GatherIncidentTets/GetCrossNeighbors/etc.) are
// otherwise UNCHANGED -- they already just reference GridRes/BDim/ACount/
// CubeBoundDim/etc. by name; only the declaration of those names moved here.
//
// Explicit 4-field (16-byte) rows -- no automatic HLSL cbuffer packing
// relied on, matching DistanceCb.hlsli's own convention (the C++ struct view
// has to match byte-for-byte).
#ifndef __HLSL_VERSION
_DIST_ALIGN_GRID struct
#else
cbuffer
#endif

DistanceGridCb

#ifdef __HLSL_VERSION
: register(DISTANCE_GRID_CB_REGISTER)
#endif
{
    // Row 0 -- sublattice sizes
    uint GridRes;
    uint BDim;
    uint ACount;
    uint BCount;

    // Row 1 -- combined node count + q-space cube-candidate bounding box
    uint NodeCount;
    int  CubeOriginMin;
    uint CubeBoundDim;
    uint TotalCubeCandidates;

    // Row 2 -- total tet slots + render-window origin (q-space cube coords,
    // see extractSurfaceCS.hlsl/FindEnclosingTet's local-to-global mapping)
    uint TetCount;
    int  WindowOriginCubeX;
    int  WindowOriginCubeY;
    int  WindowOriginCubeZ;

    // Row 3 -- render window: WindowCubeDim is the q-space DISPATCH size
    // (deliberately oversized -- see extractSurfaceCS.hlsl's header comment
    // on why a q-space axis-aligned box does NOT correspond to a compact
    // real-space region under the bccToRhombo shear). WindowRealHalfExtent
    // is the actual real-space cutoff applied on top, in real grid units,
    // GridRes-independent -- THIS is what makes the rendered region
    // genuinely local regardless of grid resolution.
    uint WindowCubeDim;
    uint WindowTetCount;
    uint WindowRealHalfExtent;
    float _padGrid1;
}
#ifndef __HLSL_VERSION
;
#endif
