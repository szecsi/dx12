#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _QOPT_ALIGN __declspec(align(16))
#else
#define _QOPT_ALIGN
#endif

#ifndef __HLSL_VERSION
_QOPT_ALIGN struct
#else
cbuffer
#endif

OptimizationCB

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    uint3 GridSize;
    uint  LatticeNodeCount;

    float3 GridOrigin;
    float  NodeCellSize; // named distinctly from bccCommon.hlsli's own CellSize -- files that include both (quadricSeedCS.hlsl/quadricRecoverCS.hlsl) would otherwise collide

    // Footpoint Gauss-Newton parameters.
    float FootDataWeight;
    float FootSmoothWeight;
    float FootStepDamping;
    float MinFootLength;

    // Tensor gradient parameters.
    float TensorDataWeight;
    float TensorSmoothWeight;
    float TensorStep;
    float MaxTensorFrobenius;

    // Robust neighborhood weighting.
    float ResidualScale;
    float NormalPower;
    float MinNormalDot;
    float MinSystemDiagonal;

    // Optional limits. MaxFootStep doubles as the max per-iteration step for
    // both CSOffsetGaussNewton.hlsl's scalar offset update and
    // CSNormalRotation.hlsl's small tangent-plane rotation (interpreted there
    // as a small-angle radian magnitude) -- distinct units, same "don't move
    // too far in one iteration" role, not worth a second knob for now.
    float MaxFootStep;
    float MaxResidual;
    uint  UseResidualNormalization;
    // Same-node, cross-slot consistency: pulls two of THIS node's own
    // populated slots' footpoints toward each other (e.g. its label-0 slot
    // and label-1 slot, when both track the same nearby shared boundary) --
    // see CSOffsetGaussNewton.hlsl/CSNormalRotation.hlsl. Without this, two
    // labels sharing an interface each relax as a fully independent field
    // with nothing forcing their zero-crossings to coincide.
    float CrossLabelWeight;

    // Soft footpoint-to-footpoint distance gate (see FootDistanceWeight in
    // QuadricFootField.hlsli) -- the primary relevance filter for whether two
    // footpoints (same-label neighbors, or same-node cross-label slots)
    // should influence each other at all: full weight within FootGateNear,
    // smoothly fading to zero by FootGateFar. Deliberately NOT normal
    // alignment -- a real surface can curve back on itself enough that two
    // genuinely nearby points (opposite walls of a thin tube, all the way
    // around a small island) have very different normals, and that's
    // exactly the diversity the tensor-gradient fit needs to recover a
    // cylinder/sphere-shaped curvature instead of a flat plane.
    float FootGateNear;
    float FootGateFar;
}
#ifndef __HLSL_VERSION
;
#endif
