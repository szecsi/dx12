#include "EyeFrameCb.hlsli"

// b1 root constants: xyz = this character's world-space translation offset,
// w = color index (as float, truncated to uint in the PS).
// ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT is required because this PSO uses a real
// vertex/index buffer input layout (HatchVertex) -- unlike every other shader
// in this codebase, which renders via SV_VertexID/UAV buffers with an empty
// input layout and never needed this flag. Omitting it fails
// CreateGraphicsPipelineState with E_INVALIDARG.
#define HatchGeometrySig "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=4, b1)"

cbuffer CharParamsCb : register(b1) {
    float4 charParams;
};

struct VsIn {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float3 hatchDir : TEXCOORD0;
    float  curvature: TEXCOORD1;
};

struct VsOut {
    float4 pos                    : SV_POSITION;
    float3 worldNormal            : TEXCOORD0;
    float3 worldPos               : TEXCOORD1;
    float2 screenDir              : TEXCOORD2;
    float  curvaturePx            : TEXCOORD4;
    nointerpolation uint colorIdx : TEXCOORD3;
};

static const float HatchDirStepEps = 0.02;

// Transforms the character's local-space HatchVertex to world/clip space,
// and additionally projects a second point stepped along the local
// curvature/tangent direction through the SAME viewProjTransform -- the
// normalized pixel-space delta between the two is the screen-projected
// hatch direction the rasterizer then interpolates across the (coarse,
// low-poly) triangle, so Pass C's stroke extrusion never needs its own
// reprojection step. That same delta's raw (pre-normalize) LENGTH is the
// local world-to-screen scale (pixels per world unit at this vertex), which
// converts the vertex's signed world-space curvature (1/world-length) into
// screen-space curvature (1/pixel) for Pass C's arc bending.
[RootSignature(HatchGeometrySig)]
VsOut hatchGeometryVS(VsIn i)
{
    VsOut o;

    float3 worldPos = i.position + charParams.xyz;
    float4 clip0 = mul(float4(worldPos, 1), viewProjTransform);

    float3 worldPos2 = worldPos + i.hatchDir * HatchDirStepEps;
    float4 clip1 = mul(float4(worldPos2, 1), viewProjTransform);

    float2 ndc0 = clip0.xy / clip0.w;
    float2 ndc1 = clip1.xy / clip1.w;
    float2 px0 = float2( ndc0.x * 0.5 + 0.5, 1.0 - (ndc0.y * 0.5 + 0.5)) * viewportParams.xy;
    float2 px1 = float2( ndc1.x * 0.5 + 0.5, 1.0 - (ndc1.y * 0.5 + 0.5)) * viewportParams.xy;

    float2 rawScreenDelta = px1 - px0;
    float pixelsPerWorldUnit = length(rawScreenDelta) / HatchDirStepEps;
    pixelsPerWorldUnit = max(pixelsPerWorldUnit, 1e-4);

    o.pos = clip0;
    o.worldNormal = i.normal;
    o.worldPos = worldPos;
    o.screenDir = rawScreenDelta;
    o.curvaturePx = i.curvature / pixelsPerWorldUnit;
    o.colorIdx = (uint)(charParams.w + 0.5);
    return o;
}
