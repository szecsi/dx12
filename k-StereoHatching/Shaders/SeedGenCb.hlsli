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

// Constants for seedGenerateCS.hlsl (Pass B) -- one instance per eye, with a
// distinct eyeSeed constant so the two eyes' jittered-grid RNG streams never
// correlate (independent seed points is a hard requirement, not tuning).

#ifndef __HLSL_VERSION
_HAT_ALIGN struct
#else
cbuffer
#endif

SeedGenCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    uint  eyeSeed;
    float cellSizePx;
    float densityGamma;
    float seedDensityScale;
    uint  maxSeeds;
    uint  viewportWidthPx;
    uint  viewportHeightPx;
    uint  _pad0;
}
#ifndef __HLSL_VERSION
;
#endif
