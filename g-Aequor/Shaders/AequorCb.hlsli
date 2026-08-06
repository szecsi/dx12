#pragma once

#ifndef __HLSL_VERSION
#include "../../Egg/Common.h"
using namespace Egg::Math;
#define _AEQ_ALIGN __declspec(align(16))
#else
#define _AEQ_ALIGN
#include "AequorConfig.hlsli" // THREAD_GROUP_SIZE, GRID_DIM, SENTINEL_LABEL, MAX_PARTICLES, etc.
#endif

// Per-run tunables for every g-Aequor compute pass (grid build, spawn,
// occurrence field, and all 4 relaxation constraints) -- one shared cbuffer,
// always bound at b0, same "single tunables cbuffer" convention g-BCC used
// (QuadricOptimizationCb.hlsli). Also carries `numParticles`, which the
// copied pbf grid-build shaders (clearGridCS/countGridCS/sortCS/prefixSum*)
// expect from whatever cbuffer they include as their ComputeCb replacement.
//
// Laid out as 4 explicit 4-field (16-byte) rows -- no automatic HLSL cbuffer
// packing relied on, since the C++ struct view (#ifndef __HLSL_VERSION) has
// to match byte-for-byte and C++ doesn't know HLSL's packing rules.

#ifndef __HLSL_VERSION
_AEQ_ALIGN struct
#else
cbuffer
#endif

AequorCb

#ifdef __HLSL_VERSION
: register(b0)
#endif
{
    // Row 0
    uint  numParticles;         // = MAX_PARTICLES; bounds check in every particle-indexed shader
    uint  activeParticleCount;  // how many of numParticles slots spawnCS actually claimed
    float AnchorRadius;         // R -- anchorBarrierCS's one-sided barrier radius
    float MinSystemDiagonal;    // surfaceConstraintCS's Gauss-Newton denominator floor

    // Row 1 -- surface positioning constraint (any label) + quadric consistency (same label)
    float SurfaceDataWeight;         // per-neighbor weight multiplier, surfaceConstraintCS
    float SurfaceStepDamping;        // Gauss-Newton step damping, surfaceConstraintCS
    float QuadricConsistencyWeight;  // normal/tensor blend factor, quadricConsistencyCS
    float MaxTensorFrobenius;        // ClampFrobenius cap

    // Row 2 -- spacing (even-distribution) constraint + robust weighting
    float SpacingEpsilon;   // pbf lambdaCS's constraint-force mixing relaxation
    float ResidualScale;    // Cauchy IRLS scale, NeighborWeight
    float NormalPower;      // NormalAlignmentWeight shaping (<=0 = fully permissive)
    float MinNormalDot;     // NormalAlignmentWeight hard reject threshold

    // Row 3
    float MaxResidual;               // EvaluateQuadric residual clamp
    uint  UseResidualNormalization;  // EvaluateQuadric residual/|gradient| normalization
    float FootGateNear;              // FootDistanceWeight: full weight within this radius
    float FootGateFar;               // FootDistanceWeight: zero weight beyond this radius

    // Row 4
    // Hard per-iteration displacement cap, applied by every position-
    // correcting constraint (surface, spacing, anchor) -- without this, a
    // particle with a poorly-conditioned/near-empty neighborhood (few or no
    // weighted neighbors within the foot gate) can take an arbitrarily large
    // corrective step in one iteration, which the next iteration then has to
    // correct from an even worse starting point -- runaway oscillation that
    // gets WORSE with more iterations instead of converging. Same safety
    // role as g-BCC's MaxFootStep.
    float MaxStep;
    float _pad4a;
    float _pad4b;
    float _pad4c;
}
#ifndef __HLSL_VERSION
;
#endif
