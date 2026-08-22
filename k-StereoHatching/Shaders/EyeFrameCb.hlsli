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

// Per-eye camera + lighting constants for hatchGeometryVS/PS (Pass A).
// Mirrors g-Aequor's AequorFrameCb.hlsli dual C++/HLSL cbuffer pattern.

#ifndef __HLSL_VERSION
_HAT_ALIGN struct
#else
cbuffer
#endif

EyeFrameCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    _HAT_ROWMAJOR float4x4 viewProjTransform;
    float4 lightDirWorld;   // xyz = normalized direction TOWARD the light, w unused
    float4 lightColor;      // rgb = light color*intensity, w = ambient
    float4 viewportParams;  // x = width px, y = height px, z/w unused
    float4 cameraPosWorld;  // xyz = eye position, w unused
    float4 brdfParams;      // x = kd (diffuse coeff), y = ks (specular coeff), z = specular exponent, w unused
}
#ifndef __HLSL_VERSION
;
#endif
