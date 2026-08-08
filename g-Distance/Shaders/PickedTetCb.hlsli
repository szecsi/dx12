#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _DIST_ALIGN4 __declspec(align(16))
#else
#define _DIST_ALIGN4
#endif

// The 4 world-space corners of whichever tet DistanceApp::PerformPick()
// last found under the mouse cursor (via depth readback -> world position ->
// brute-force point-in-tet search, see DistanceApp.h) -- consumed only by
// wireframeVS.hlsl to draw that one tet's 6 edges. Valid=0 means nothing is
// currently picked (or the last pick missed the surface) -- draw nothing.

#ifndef __HLSL_VERSION
_DIST_ALIGN4 struct
#else
cbuffer
#endif

PickedTetCb

#ifdef __HLSL_VERSION
: register(b1)
#endif
{
    float4 Corner0;
    float4 Corner1;
    float4 Corner2;
    float4 Corner3;
    uint   Valid;
#ifndef __HLSL_VERSION
    float  _padPicked[3];
#else
    float3 _padPicked;
#endif
}
#ifndef __HLSL_VERSION
;
#endif
