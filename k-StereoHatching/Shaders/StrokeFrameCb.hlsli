#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _HAT_ALIGN __declspec(align(16))
#else
#define _HAT_ALIGN
#endif

// Constants for strokeVS/PS.hlsl (Pass C).

#ifndef __HLSL_VERSION
_HAT_ALIGN struct
#else
cbuffer
#endif

StrokeFrameCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    float4 viewportParams; // x = width px, y = height px, z/w unused
    float4 strokeParams;   // x = length px, y = width px, z = maxSeeds (as float), w = curvature bend scale
}
#ifndef __HLSL_VERSION
;
#endif
