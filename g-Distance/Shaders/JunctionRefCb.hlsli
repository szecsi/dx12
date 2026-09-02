#pragma once

// Ground-truth parameters for TestShape_TiltedBoxJunction (see the approved
// plan / rasterLabelCS.hlsl / smoothnessJacobiAlienRefCS.hlsl) -- a single
// world-space plane (PlaneNormal, PlanePoint), evaluated only inside a cube
// of half-extent BoxHalfExtent centered at PlanePoint: outside the cube,
// background (label 0); inside, label 1 or 2 by which side of the plane a
// point falls on. Consumed by TWO different shaders, each with its own
// already-occupied register set, so (like DistanceGridCb.hlsli) the register
// is configurable per-file: #define JUNCTION_REF_CB_REGISTER bN BEFORE
// including this file; falls back to b0 if unset.

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _JREF_ALIGN __declspec(align(16))
#else
#define _JREF_ALIGN
#ifndef JUNCTION_REF_CB_REGISTER
#define JUNCTION_REF_CB_REGISTER b0
#endif
#endif

#ifndef __HLSL_VERSION
_JREF_ALIGN struct
#else
cbuffer
#endif

JunctionRefCb

#ifdef __HLSL_VERSION
: register(JUNCTION_REF_CB_REGISTER)
#endif
{
    float3 PlaneNormal; // unit normal of the ground-truth split plane
    float  _padJRef0;
    float3 PlanePoint;  // a point exactly ON the plane -- also the residual's position-anchor and the box's center
    float  BoxHalfExtent;
    // smoothnessJacobiAlienRefCS.hlsl's two Gauss-Newton term weights --
    // kept here (diagnostic-only, one-off tool) rather than spending
    // DistanceCb.hlsli's general solver-tunable slots on them.
    float  RefDirectionWeight; // weight on cross(gradD,PlaneNormal) -- wants the current interface gradient parallel to the true normal
    float  RefPositionWeight;  // weight on D(PlanePoint) -- wants the current interface to actually pass through the known true point
    float  _padJRef1;
    float  _padJRef2;
}
#ifndef __HLSL_VERSION
;
#endif
