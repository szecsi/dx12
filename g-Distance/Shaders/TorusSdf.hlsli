#pragma once
#include "TorusListCb.hlsli"

// Signed distance from p to torus t's surface (negative = inside). p is
// transformed into the torus's local frame (t.axis is the ring's rotation
// axis): q = (distance from the central ring - majorRadius, distance along
// axis); the surface is |q| == minorRadius.
float SdTorus(float3 p, TorusDesc t)
{
    float3 d = p - t.center;
    float axial = dot(d, t.axis);
    float3 planar = d - axial * t.axis;
    float2 q = float2(length(planar) - t.majorRadius, axial);
    return length(q) - t.minorRadius;
}

// Exact nearest point on torus t's surface to p: project onto the central
// ring, then step out from there by minorRadius toward p.
float3 NearestTorusPoint(float3 p, TorusDesc t)
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
    return ringPoint + dir * t.minorRadius;
}

// Ellipsoid, axis-aligned in world space -- TorusDesc.center is reused as
// the ellipsoid's center, and .axis (normally a unit rotation axis for the
// torus) is reused to hold its three semi-axis radii directly (rx,ry,rz);
// .majorRadius/.minorRadius are unused for this shape kind. See
// BccApp::BuildShapeList / TorusListCb.ShapeKind.
float SdEllipsoid(float3 p, TorusDesc t)
{
    float3 r = t.axis;
    float3 d = p - t.center;
    // Inigo Quilez's standard approximate ellipsoid SDF -- exact at the
    // surface (where boundary detection needs it), a reasonable
    // approximation elsewhere.
    float k0 = length(d / r);
    float k1 = length(d / (r * r));
    return (k1 > 1.0e-8) ? (k0 * (k0 - 1.0) / k1) : (k0 - 1.0);
}

// Approximate nearest point: scale space so the ellipsoid becomes a unit
// sphere, project onto that (exact for a sphere), scale back. Not exact for
// a general ellipsoid, but the relaxation refines it from there the same as
// it already does for the torus's own analytic projection.
float3 NearestEllipsoidPoint(float3 p, TorusDesc t)
{
    float3 r = t.axis;
    float3 d = p - t.center;
    float3 scaled = d / r;
    float sLen = length(scaled);
    float3 scaledDir = (sLen > 1.0e-6) ? (scaled / sLen) : float3(1, 0, 0);
    return t.center + scaledDir * r;
}

// Dispatches to whichever shape TorusListCb.ShapeKind currently selects --
// shared by torusInitCS.hlsl/analyticInitCS.hlsl/quadricSeedCS.hlsl so none
// of them need their own branch.
float ShapeSd(float3 p, TorusDesc t)
{
    return (ShapeKind != 0) ? SdEllipsoid(p, t) : SdTorus(p, t);
}

float3 ShapeNearestPoint(float3 p, TorusDesc t)
{
    return (ShapeKind != 0) ? NearestEllipsoidPoint(p, t) : NearestTorusPoint(p, t);
}
