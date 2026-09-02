#pragma once

// Ground-truth parameters for TestShape_ClippedSpheres (see
// buildAnalyticClippedSpheresCS.hlsl): two overlapping spheres, each clipped
// by the radical plane of the pair (the plane through their intersection
// circle) so they partition space into exactly 3 disjoint regions --
// background, "capped" sphere A, "capped" sphere B -- meeting along that
// circle with no double-counted lens. Consumed by one shader with its own
// already-occupied register set, so (like DistanceGridCb.hlsli) the
// register is configurable per-file: #define CLIPPED_SPHERES_CB_REGISTER bN
// BEFORE including this file; falls back to b0 if unset.

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _CSPH_ALIGN __declspec(align(16))
#else
#define _CSPH_ALIGN
#ifndef CLIPPED_SPHERES_CB_REGISTER
#define CLIPPED_SPHERES_CB_REGISTER b0
#endif
#endif

#ifndef __HLSL_VERSION
_CSPH_ALIGN struct
#else
cbuffer
#endif

ClippedSpheresCb

#ifdef __HLSL_VERSION
: register(CLIPPED_SPHERES_CB_REGISTER)
#endif
{
    float3 CenterA;
    float  RadiusA;
    float3 CenterB;
    float  RadiusB;
    // Each of the 3 SDFs (outside, capA, capB) is clamped to
    // [-ClampDistance, ClampDistance] before biasing -- keeps phi/beta from
    // growing unboundedly far from the spheres (the "outside" field is
    // otherwise unbounded), and reads directly as "how far" in the GUI.
    // "Reinit param" -- takes effect on the next Reinitialize, see
    // BuildShapeList().
    float  ClampDistance;
    float  _padCSph0;
    float  _padCSph1;
    float  _padCSph2;
}
#ifndef __HLSL_VERSION
;
#endif
