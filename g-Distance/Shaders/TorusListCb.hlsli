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
	// Only read by the ported synthetic scenes (ShapeKind >= 9, see
	// SyntheticScenes.hlsli/rasterLabelCS.hlsl); ignored by every analytic and
	// grid-pattern shape. SceneThreshold's meaning is per-scene -- an iso-level
	// for Marschner-Lobb, a uniform radius offset for the tree -- and
	// SceneMaterialCount is MlMultiLabel's Voronoi-cell count, clamped to
	// [1,16] there. Named to match the Vulkan side's "Vol. Threshold" /
	// "LM multi-material count" controls, which they must be set equal to for
	// the two renderers to produce the same field.
	float     SceneThreshold;
	uint      SceneMaterialCount;
}
#ifndef __HLSL_VERSION
;
#endif
