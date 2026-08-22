#pragma once

// Synthetic label fields ported from the Vulkan renderer (render-engine-ng's
// src/shaders/volume/volume_funcs.glsl and its CPU mirror in
// scene/components/voxel_data_component.cpp), so both renderers can be run on
// the exact same test scenes when comparing volumetric-fairing results.
//
// Everything here works in the SAME normalized domain the Vulkan side uses:
//   p = float3(gridIndex) / gridRes * 2 - 1
// i.e. the grid maps onto [-1,1) per axis regardless of resolution. That is
// the whole point of the port -- the shapes must be resolution-independent and
// identical between the two renderers, so nothing here may reference CELL_SIZE
// or a world-space center the way TorusSdf.hlsli's analytic torii do.
//
// The formulas below are transcribed bone-for-bone from the GLSL originals;
// any edit here must be mirrored there (and in the C++ mirror) or the two
// renderers silently stop agreeing.

static const float SyntheticPi = 3.14159265;

// Synthetic scenes keep this many grid nodes of guaranteed background around
// every edge, so a generated shape never touches the domain boundary --
// matches label_common.glsl's kSyntheticPadding / voxel_data_component.cpp's
// kSyntheticPadding exactly.
static const int SyntheticPadding = 3;

// -- Marschner-Lobb -------------------------------------------------------
// The classic high-frequency test field. Matches volume_funcs.glsl's mltest.
float MlTest(float3 p, float f, float alpha)
{
    return (1 - sin(SyntheticPi * p.z * 0.5)
              + alpha * (1 + cos(2 * SyntheticPi * f * cos(SyntheticPi * sqrt(p.x * p.x + p.y * p.y) * 0.5))))
           * 0.5 / (1 + alpha);
}

// Fixed, deterministic Halton(2,3,5) point set used as Voronoi seeds by
// MlMultiLabel -- must match volume_funcs.glsl's voronoiSeeds (and
// voxel_data_component.cpp's kVoronoiSeeds) exactly, so the same material
// lands in the same cell in both renderers.
static const float3 SyntheticVoronoiSeeds[16] = {
    float3(0.0, -0.3333, -0.6),
    float3(-0.5, 0.3333, -0.2),
    float3(0.5, -0.7778, 0.2),
    float3(-0.75, -0.1111, 0.6),
    float3(0.25, 0.5556, -0.92),
    float3(-0.25, -0.5556, -0.52),
    float3(0.75, 0.1111, -0.12),
    float3(-0.875, 0.7778, 0.28),
    float3(0.125, -0.9259, 0.68),
    float3(-0.375, -0.2593, -0.84),
    float3(0.625, 0.4074, -0.44),
    float3(-0.625, -0.7037, -0.04),
    float3(0.375, -0.037, 0.36),
    float3(-0.125, 0.6296, 0.76),
    float3(0.875, -0.4815, -0.76),
    float3(-0.9375, 0.1852, -0.36)
};

// Two-label Marschner-Lobb: threshold is an ISO-LEVEL on the field (not a
// radius offset the way TreeLabel's is), matching ml_label.
uint MlLabel(float3 p, float threshold)
{
    return (MlTest(p, 6, 0.25) > threshold) ? 1u : 0u;
}

// Multi-material Marschner-Lobb: the same threshold carves out the same
// iso-shell (background stays label 0); inside it, materials 1..N are assigned
// by nearest Voronoi seed, so the interior is partitioned into regions
// unrelated to the MB frequency. That separates "does the high-frequency outer
// surface survive" from "do interior material junctions behave" -- the only
// ported scene that produces a genuine multi-label interior, which is what
// makes it worth having here next to the 3-tori scene.
uint MlMultiLabel(float3 p, float threshold, uint materialCount)
{
    if (MlTest(p, 6, 0.25) <= threshold) return 0u;

    int numMaterials = clamp((int)materialCount, 1, 16);
    int best = 0;
    float bestDist = distance(p, SyntheticVoronoiSeeds[0]);
    for (int i = 1; i < numMaterials; i++) {
        float d = distance(p, SyntheticVoronoiSeeds[i]);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return (uint)(1 + best);
}

// -- Tree -----------------------------------------------------------------
float SyntheticDot2(float3 v) { return dot(v, v); }

// Inigo Quilez, "Round Cone - exact" (https://iquilezles.org/articles/distfunctions/)
// -- a tapered capsule (convex hull of two spheres of radius r1/r2 at a/b), so
// a branch's girth can taper toward a thin tip.
float SdRoundCone(float3 p, float3 a, float3 b, float r1, float r2)
{
    float3 ba = b - a;
    float l2 = dot(ba, ba);
    float rr = r1 - r2;
    float a2 = l2 - rr * rr;
    float il2 = 1.0 / l2;

    float3 pa = p - a;
    float y = dot(pa, ba);
    float z = y - l2;
    float x2 = SyntheticDot2(pa * l2 - ba * y);
    float y2 = y * y * l2;
    float z2 = z * z * l2;

    float k = sign(rr) * rr * rr * x2;
    if (sign(z) * a2 * z2 > k) return sqrt(x2 + z2) * il2 - r2;
    if (sign(y) * a2 * y2 < k) return sqrt(x2 + y2) * il2 - r1;
    return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

// Rotates p around the segment (a,b)'s OWN local axis by an angle proportional
// to how far along the segment p projects -- a branch-local twist, so an
// off-axis branch spirals around its own centerline instead of swinging around
// a distant global axis.
float3 TwistAroundSegment(float3 p, float3 a, float3 b, float k)
{
    float3 dir = normalize(b - a);
    float3 up = abs(dir.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 u = normalize(cross(up, dir));
    float3 v = cross(dir, u);

    float3 pa = p - a;
    float t = dot(pa, dir);
    float3 perp = pa - dir * t;
    float pu = dot(perp, u);
    float pv = dot(perp, v);

    float angle = k * t;
    float c = cos(angle);
    float s = sin(angle);
    float ru = c * pu - s * pv;
    float rv = s * pu + c * pv;

    return a + dir * t + u * ru + v * rv;
}

// Hand-authored branching skeleton: trunk -> 2 primary branches (one twisted)
// -> 2 twigs each, radius tapering from 0.070 (trunk base) to 0.006 (thinnest
// twig tip, ~12x range). Every bone's start point/radius exactly matches its
// parent's end point/radius, so the union is watertight at every joint without
// a smooth-min blend. The thin-feature range is the interesting part here: it
// probes how thin a feature can get before the solve/extraction drops it.
float SdTree(float3 p)
{
    float d = 1e9;
    d = min(d, SdRoundCone(p, float3(0.0, -0.65, 0.0), float3(0.0, -0.05, 0.0), 0.070, 0.045)); // trunk
    d = min(d, SdRoundCone(TwistAroundSegment(p, float3(0.0, -0.05, 0.0), float3(0.38, 0.30, 0.12), 6.0),
                           float3(0.0, -0.05, 0.0), float3(0.38, 0.30, 0.12), 0.045, 0.020)); // branch A (twisted)
    d = min(d, SdRoundCone(p, float3(0.0, -0.05, 0.0), float3(-0.34, 0.32, -0.16), 0.045, 0.022)); // branch B
    d = min(d, SdRoundCone(p, float3(0.38, 0.30, 0.12), float3(0.58, 0.58, 0.05), 0.020, 0.008)); // twig A1
    d = min(d, SdRoundCone(p, float3(0.38, 0.30, 0.12), float3(0.52, 0.46, 0.36), 0.020, 0.006)); // twig A2
    d = min(d, SdRoundCone(p, float3(-0.34, 0.32, -0.16), float3(-0.58, 0.52, -0.36), 0.022, 0.007)); // twig B1
    d = min(d, SdRoundCone(p, float3(-0.34, 0.32, -0.16), float3(-0.46, 0.62, 0.10), 0.022, 0.009)); // twig B2
    return d;
}

// threshold acts as a uniform radius offset around the as-authored skeleton
// (unlike MlLabel's threshold-as-isolevel): at 0.5, SdTree(p)<0 is exactly the
// authored shape; above 0.5 grows every branch, below shrinks them -- so
// lowering it makes the thinnest twigs vanish first.
uint TreeLabel(float3 p, float threshold)
{
    return (SdTree(p) < threshold - 0.5) ? 1u : 0u;
}
