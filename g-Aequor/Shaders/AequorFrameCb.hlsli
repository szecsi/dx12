#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _AEQ_ALIGN __declspec(align(16))
#define _AEQ_ROWMAJOR
#else
#define _AEQ_ALIGN
#define _AEQ_ROWMAJOR row_major
#endif

// Camera + viewport constants shared by the analytic-shape raymarch
// background and the particle point-sprite pass. Mirrors g-BCC's
// BccFrameCb.hlsli/FootVizCb.hlsli, folded into one cbuffer since g-Aequor
// only has these two render passes (no footvector lines).

#ifndef __HLSL_VERSION
_AEQ_ALIGN struct
#else
cbuffer
#endif

AequorFrameCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    _AEQ_ROWMAJOR float4x4 viewProjTransform;
    _AEQ_ROWMAJOR float4x4 rayDirTransform;
    float4 cameraPos;
    float4 raymarchParams;   // x = maxSteps, y = maxDist, z/w unused
    float4 pointParams;      // x = viewport width px, y = viewport height px, z = point radius px, w unused
}
#ifndef __HLSL_VERSION
;
#endif
