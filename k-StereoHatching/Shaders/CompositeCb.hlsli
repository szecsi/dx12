#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _HAT_ALIGN __declspec(align(16))
#else
#define _HAT_ALIGN
#endif

// Constants for compositePS.hlsl (Pass D, the anaglyph combine).

#ifndef __HLSL_VERSION
_HAT_ALIGN struct
#else
cbuffer
#endif

CompositeCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    float4 paperColor; // rgb = paper base color, a unused
    float4 inkTint;    // x = left ink strength, y = right ink strength, z = show-luminance-map flag (0/1), w unused
    // x = render mode (0 = Unsynced, 1 = Cross-Projected), y = show-
    // unprojected-hatching flag (0/1), z = show-cross-projected-hatching
    // flag (0/1), w unused. y/z are only meaningful (and only sampled) when
    // x == 1 -- in Unsynced mode the cross-projected textures were never
    // rendered this frame and may hold stale data from an earlier mode.
    float4 modeParams;
}
#ifndef __HLSL_VERSION
;
#endif
