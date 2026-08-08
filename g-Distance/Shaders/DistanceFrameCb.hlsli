#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _DIST_ALIGN __declspec(align(16))
#define _DIST_ROWMAJOR
#else
#define _DIST_ALIGN
#define _DIST_ROWMAJOR row_major
#endif

// Camera + viewport constants shared by the analytic-shape raymarch
// background and the node point-sprite pass -- identical shape to g-Aequor's
// AequorFrameCb.hlsli (renamed only, no field changes needed).

#ifndef __HLSL_VERSION
_DIST_ALIGN struct
#else
cbuffer
#endif

DistanceFrameCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    _DIST_ROWMAJOR float4x4 viewProjTransform;
    _DIST_ROWMAJOR float4x4 rayDirTransform;
    float4 cameraPos;
    float4 raymarchParams;   // x = maxSteps, y = maxDist, z = potential-to-size reference scale (nodePointVS.hlsl), w unused
    float4 pointParams;      // x = viewport width px, y = viewport height px, z = point radius px, w = hide-uniform-neighborhood-nodes flag (nodePointVS.hlsl)
    float4 nodeFadeParams;   // x = fade exponent, y = solid-within-this-distance floor (both nodePointVS.hlsl, both in units of GridRes*CellSize), z/w unused
}
#ifndef __HLSL_VERSION
;
#endif
