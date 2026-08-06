#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _FVZ_ALIGN __declspec(align(16))
#else
#define _FVZ_ALIGN
#endif

#ifndef __HLSL_VERSION
_FVZ_ALIGN struct
#else
cbuffer
#endif

FootVizParamsCb

#ifdef __HLSL_VERSION
: register(b1)
#endif
{
    // xyz = footvector start-point lower bound (world space); w = max
    // footvector length to draw. Shared by the line and footpoint passes
    // (see BccApp::footBoxMin/footBoxMax/footMaxLen).
    float4 boxMin;
    // xyz = footvector start-point upper bound; w = line half-width, in
    // pixels (screen-space quad expansion, see footLineVS.hlsl).
    float4 boxMax;
    // xy = viewport size in pixels; z = footpoint sphere radius, in pixels
    // (see footPointVS.hlsl); w unused.
    float4 viewport;
}
#ifndef __HLSL_VERSION
;
#endif
