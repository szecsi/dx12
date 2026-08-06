#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _BCC_ALIGN __declspec(align(16))
#define _BCC_ROWMAJOR
#else
#define _BCC_ALIGN
#define _BCC_ROWMAJOR row_major
#endif

#ifndef __HLSL_VERSION
_BCC_ALIGN struct
#else
cbuffer
#endif

BccFrameCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
	_BCC_ROWMAJOR float4x4 viewProjTransform;
	_BCC_ROWMAJOR float4x4 rayDirTransform;
	float4   cameraPos;
	float4   ahead;
	float4   gridParams; // x = GridRes, y = cellSize, z = maxSteps, w = maxDist
	float4   renderParams; // x = render mode (0 = labels, 1 = normals)
}
#ifndef __HLSL_VERSION
;
#endif
