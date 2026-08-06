#pragma once

#include "TorusListCb.hlsli"
#include "QuadricMath.hlsli"

// Analytic curvature tensor + outward normal for the current test shape,
// evaluated as if `p` sits exactly on its surface -- extracted verbatim (no
// BCC dependency in the original) from g-BCC's quadricSeedCS.hlsl, where the
// full derivation/verification is documented. Used by spawnCS.hlsl to seed
// exact position/normal/curvature for the synthetic torus/ellipsoid test
// scene, same role Analytic init mode played in g-BCC.

float3x3 AnalyticTorusTensor(float3 p, TorusDesc t, out float3 outwardNormal)
{
    float3 d = p - t.center;
    float axial = dot(d, t.axis);
    float3 planarVec = d - axial * t.axis;
    float planarLen = length(planarVec);
    float3 planarDir = (planarLen > 1.0e-6) ? (planarVec / planarLen) : float3(1, 0, 0);

    float3 ringPoint = t.center + planarDir * t.majorRadius;
    float3 toP = p - ringPoint;
    float toPLen = length(toP);
    float3 dir = (toPLen > 1.0e-6) ? (toP / toPLen) : planarDir;
    outwardNormal = dir;

    float cosT = dot(dir, planarDir);
    float sinT = dot(dir, t.axis);
    float3 tTheta = -sinT * planarDir + cosT * t.axis;
    float3 tPhi = cross(t.axis, planarDir);

    float kTheta = 1.0 / max(t.minorRadius, 1.0e-6);
    float kPhi = cosT / max(t.majorRadius + t.minorRadius * cosT, 1.0e-6);

    return kTheta * Outer(tTheta, tTheta) + kPhi * Outer(tPhi, tPhi);
}

float3x3 AnalyticEllipsoidTensor(float3 p, TorusDesc t, out float3 outwardNormal)
{
    float3 r = t.axis;
    float3 invR2 = 1.0 / max(r * r, 1.0e-6);
    float3 d = p - t.center;
    float3 g = 2.0 * (d * invR2);
    float gLen = length(g);
    outwardNormal = (gLen > 1.0e-8) ? (g / gLen) : float3(0, 0, 1);

    float3x3 hess = float3x3(
        2.0 * invR2.x, 0.0, 0.0,
        0.0, 2.0 * invR2.y, 0.0,
        0.0, 0.0, 2.0 * invR2.z);
    float3x3 P = Identity3() - Outer(outwardNormal, outwardNormal);
    return mul(P, mul(hess, P)) / max(gLen, 1.0e-8);
}

float3x3 AnalyticShapeTensor(float3 p, TorusDesc t, out float3 outwardNormal)
{
    return (ShapeKind != 0)
        ? AnalyticEllipsoidTensor(p, t, outwardNormal)
        : AnalyticTorusTensor(p, t, outwardNormal);
}
