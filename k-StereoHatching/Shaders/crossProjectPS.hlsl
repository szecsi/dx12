#include "CrossProjectCb.hlsli"

// t0 = the OPPOSING eye's already-finished strokeMask (R8_UNORM ink
// coverage), t1 = the OPPOSING eye's depth buffer sampled as R32_FLOAT (see
// StereoHatchingApp.h -- the depth resource is created R32_TYPELESS
// specifically so it can be both a DSV, for the opposing eye's own Pass A/
// this-pass depth test, and an SRV here).
Texture2D<float> OtherStrokeMask : register(t0);
Texture2D<float> OtherDepth      : register(t1);

struct VsOut {
    float4 pos      : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

// For each of THIS eye's own visible surface points (this PSO is drawn with
// a depth test against this eye's own already-written depth buffer, so only
// front-facing/visible fragments reach here -- see the PSO's DepthFunc), find
// where that same 3D point falls in the OPPOSING eye's screen, and -- only
// where that mapping is actually valid ("as far as the surface allows the
// mapping": in view frustum AND not occluded there by something else -- the
// depth compare below) -- pull that eye's ink there. The composite pass then
// draws this in THIS eye's own anaglyph color, so a stroke seeded for the
// opposing eye reads (reprojected) in the wrong (own) color -- e.g. a
// right-eye stroke shows up as red when the left eye's own composite draws
// this output.
float crossProjectPS(VsOut i) : SV_TARGET
{
    float4 clipOther = mul(float4(i.worldPos, 1), otherViewProjTransform);
    if (clipOther.w <= 0.0) return 0.0; // behind the opposing camera

    float3 ndcOther = clipOther.xyz / clipOther.w;
    if (any(abs(ndcOther.xy) > 1.0)) return 0.0; // outside the opposing eye's screen

    float2 pxOther = float2(ndcOther.x * 0.5 + 0.5, 1.0 - (ndcOther.y * 0.5 + 0.5)) * viewportParams.xy;
    int2 pxi = int2(pxOther);

    // This engine's projection matrices map view-space z directly to
    // NDC z in [0,1] (D3D convention, clip.w = view-space z), so the
    // opposing eye's stored depth is directly comparable to ndcOther.z --
    // no remap needed.
    float otherStoredDepth = OtherDepth.Load(int3(pxi, 0));
    if (abs(otherStoredDepth - ndcOther.z) > viewportParams.z) return 0.0; // occluded from the opposing eye's viewpoint

    return OtherStrokeMask.Load(int3(pxi, 0));
}
