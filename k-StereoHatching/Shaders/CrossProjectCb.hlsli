#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _HAT_ALIGN __declspec(align(16))
#define _HAT_ROWMAJOR
#else
#define _HAT_ALIGN
#define _HAT_ROWMAJOR row_major
#endif

// Constants for crossProjectVS/PS.hlsl ("Cross-Projected" render mode):
// ownViewProjTransform places the vertex for THIS eye's rasterization
// (same as EyeFrameCb, just this pass's own small cbuffer); otherViewProjTransform
// re-projects the same world-space surface point into the OPPOSING eye's
// clip space so its pixel shader can sample that eye's already-finished
// hatching (and depth, to reject points the opposing eye doesn't actually see).

#ifndef __HLSL_VERSION
_HAT_ALIGN struct
#else
cbuffer
#endif

CrossProjectCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    _HAT_ROWMAJOR float4x4 ownViewProjTransform;
    _HAT_ROWMAJOR float4x4 otherViewProjTransform;
    float4 viewportParams; // x = width px, y = height px, z = depth compare epsilon (NDC z), w unused
}
#ifndef __HLSL_VERSION
;
#endif
