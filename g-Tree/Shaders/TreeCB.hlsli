#pragma once

#ifndef __HLSL_VERSION
#include "../../../Egg/Common.h"
using namespace Egg::Math;
__declspec(align(16)) struct
#else
cbuffer
#endif

TreeMaterialCb 

#ifdef __HLSL_VERSION
: register(b0) 
#endif
{
	float2 lineSize;
	float2 fading;
	float4 texScale;
	float4 crossAngle;
	float  stripWidth;
	float  overdraw;
}
#ifndef __HLSL_VERSION
;
#endif