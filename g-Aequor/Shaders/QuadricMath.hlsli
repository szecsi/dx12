#pragma once

#include "AequorCb.hlsli"

// Matrix/quadric helpers shared by surfaceConstraintCS.hlsl and
// quadricConsistencyCS.hlsl -- extracted from g-BCC's QuadricFootField.hlsli
// and adapted to particles (an absolute position + normal + curvature
// tensor A, no lattice node / label-slot bookkeeping).

// A particle's symmetric curvature tensor is stored split as (diag, offdiag)
// -- (Axx,Ayy,Azz) and (Axy,Axz,Ayz) -- across two float3 buffers rather than
// a packed float3x3, so it sorts/gathers like any other float3 field.
float3x3 BuildTensor(float3 diag, float3 offdiag)
{
    return float3x3(
        diag.x,    offdiag.x, offdiag.y,
        offdiag.x, diag.y,    offdiag.z,
        offdiag.y, offdiag.z, diag.z);
}

float3x3 Identity3()
{
    return float3x3(1,0,0, 0,1,0, 0,0,1);
}

float3x3 Outer(float3 a, float3 b)
{
    return float3x3(
        a.x*b.x, a.x*b.y, a.x*b.z,
        a.y*b.x, a.y*b.y, a.y*b.z,
        a.z*b.x, a.z*b.y, a.z*b.z);
}

float3x3 Symmetrize(float3x3 A)
{
    return 0.5 * (A + transpose(A));
}

float FrobeniusSquared(float3x3 A)
{
    return dot(A[0], A[0]) + dot(A[1], A[1]) + dot(A[2], A[2]);
}

float3x3 ClampFrobenius(float3x3 A)
{
    float f2 = FrobeniusSquared(A);
    float max2 = MaxTensorFrobenius * MaxTensorFrobenius;
    if (f2 > max2 && max2 > 0.0)
        A *= MaxTensorFrobenius * rsqrt(f2);
    return A;
}

// Restricts A to the tangent plane of n (zeroes out the normal-direction
// component, keeping only the part that describes in-plane curvature).
float3x3 ProjectTensorToNormal(float3x3 A, float3 n)
{
    A = Symmetrize(A);
    float3x3 P = Identity3() - Outer(n, n);
    float mu = dot(n, mul(A, n));
    return Symmetrize(mul(P, mul(A, P)) + mu * Outer(n, n));
}

// Adjugate solve -- H is expected to already be regularized (e.g. a small
// multiple of the identity added) before this is called.
bool SolveSPD3(float3x3 Mat, float3 rhs, out float3 x)
{
    float3 c0 = cross(Mat[1], Mat[2]);
    float3 c1 = cross(Mat[2], Mat[0]);
    float3 c2 = cross(Mat[0], Mat[1]);
    float det = dot(Mat[0], c0);

    if (abs(det) < 1.0e-12)
    {
        x = 0.0;
        return false;
    }

    float3x3 invH = transpose(float3x3(c0, c1, c2)) / det;
    x = mul(invH, rhs);
    return all(isfinite(x));
}

bool SolveSPD2(float2x2 Hm, float2 rhs, out float2 x)
{
    float det = Hm[0][0] * Hm[1][1] - Hm[0][1] * Hm[1][0];
    if (abs(det) < 1.0e-12)
    {
        x = 0.0;
        return false;
    }
    float2x2 invH = float2x2(Hm[1][1], -Hm[0][1], -Hm[1][0], Hm[0][0]) / det;
    x = mul(invH, rhs);
    return all(isfinite(x));
}

// An orthonormal basis for the plane tangent to n.
void TangentBasis(float3 n, out float3 t1, out float3 t2)
{
    float3 helper = (abs(n.x) < 0.8) ? float3(1, 0, 0) : float3(0, 1, 0);
    t1 = normalize(cross(n, helper));
    t2 = cross(n, t1);
}

// Q(x) = normal.r + 0.5 r^T A r (r = queryPoint - evaluatorPoint), evaluated
// using the evaluator particle's own (normal, A) -- same form as g-BCC's
// EvaluateSlotQuadric, minus the LabelSlotState/offset/confidence bookkeeping
// (a particle's position already IS its footpoint).
void EvaluateQuadric(float3 evalNormal, float3x3 evalA, float3 evaluatorPoint, float3 queryPoint,
                      out float residual, out float3 gradient)
{
    float3 r = queryPoint - evaluatorPoint;
    gradient = evalNormal + mul(evalA, r);
    residual = 0.5 * dot(r, gradient + evalNormal);

    residual = clamp(residual, -MaxResidual, MaxResidual);
    if (UseResidualNormalization != 0u)
        residual /= max(length(gradient), 1.0e-3);
}

// Normal-alignment shaping -- see QuadricFootField.hlsli's NormalAlignmentWeight
// for the full rationale (NormalPower<=0 -> unconditionally permissive;
// MinNormalDot is a hard reject on the raw, unfloored dot product).
float NormalAlignmentWeight(float3 ni, float3 nj)
{
    float nd = dot(ni, nj);
    if (nd < MinNormalDot)
        return 0.0;
    if (NormalPower <= 0.0)
        return 1.0;
    return pow(saturate(0.5 * (nd + 1.0)), NormalPower);
}

float NeighborWeight(float3 ni, float3 nj, float residual)
{
    float normalW = NormalAlignmentWeight(ni, nj);
    float x = residual / max(ResidualScale, 1.0e-4);
    float robustW = rcp(1.0 + x*x); // Cauchy IRLS weight.
    return normalW * robustW;
}

// Soft footpoint-to-footpoint distance gate: 1 within FootGateNear, smoothly
// fading to 0 by FootGateFar -- the primary relevance filter for both the
// any-label surface constraint and the same-label quadric-consistency
// constraint (see QuadricOptimizationCb.hlsli's FootGateNear/Far docs).
float FootDistanceWeight(float3 pa, float3 pb)
{
    float d = distance(pa, pb);
    return 1.0 - smoothstep(FootGateNear, FootGateFar, d);
}
