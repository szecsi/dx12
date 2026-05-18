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
	float4x4 rigging[12];
	float  stripWidth;
	float  p1;
	float  p2;
	float  p3;
}
#ifndef __HLSL_VERSION
;
#endif