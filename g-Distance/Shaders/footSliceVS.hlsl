#include "DistanceFrameCb.hlsli"

// Debug visualization: draws a full-screen triangle (same trick as
// raymarchVS.hlsl) whose pixel shader ray-intersects an axis-aligned world
// plane and colors the hit by NodePotential's candidate slots #0/#1 -- a
// cross-section through the solved potential field, for visually inspecting
// it directly instead of guessing from readback numbers alone. Root
// signature declared once here, shared by footSlicePS.hlsl (same convention
// as raymarchVS/PS).
#define FootSliceSig "RootFlags(0)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=4, b1)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "CBV(b2)"

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

[RootSignature(FootSliceSig)]
VsOut footSliceVS(uint vid : SV_VertexID)
{
    VsOut o;
    float2 uv  = float2((vid << 1) & 2, vid & 2);
    float2 ndc = uv * 2.0f - 1.0f;
    o.pos = float4(ndc, 0, 1);

    float4 dir = mul(float4(ndc, 1, 1), rayDirTransform);
    dir /= dir.w;
    o.rayDir = dir.xyz;
    return o;
}
