#pragma once

#define MAX_TORII 8

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _BCC_ALIGN __declspec(align(16))
#else
#define _BCC_ALIGN
#endif

_BCC_ALIGN struct TorusDesc {
	float3 center;
	float  majorRadius;
	float3 axis;
	float  minorRadius;
	uint   label;
#ifndef __HLSL_VERSION
	float  _padTorus[3];
#else
	float3 _padTorus;
#endif
};

#ifndef __HLSL_VERSION
_BCC_ALIGN struct
#else
cbuffer
#endif

TorusListCb

#ifdef __HLSL_VERSION
: register(b1)
#endif
{
	TorusDesc torii[MAX_TORII];
	uint      nTorii;
	// 0 = torus (SdTorus/NearestTorusPoint), 1 = ellipsoid (SdEllipsoid/
	// NearestEllipsoidPoint) -- see TorusSdf.hlsli's ShapeSd/ShapeNearestPoint
	// dispatchers and quadricSeedCS.hlsl's AnalyticShapeTensor. Applies
	// uniformly to every entry in torii[] this frame; BccApp::BuildShapeList
	// rebuilds the whole list per shape kind, never mixed.
	uint      ShapeKind;
#ifndef __HLSL_VERSION
	float     _padList[2];
#else
	float2    _padList;
#endif
}
#ifndef __HLSL_VERSION
;
#endif
