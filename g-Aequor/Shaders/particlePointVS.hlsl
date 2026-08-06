#include "AequorFrameCb.hlsli"
#include "AequorConfig.hlsli"

// Position/Label bound as UAV, not SRV -- they stay permanently
// UNORDERED_ACCESS (shared with every compute pass all iteration long), so
// the render pass reads them the same way rather than needing a
// UAV<->SRV state-transition dance every frame just for this draw.
#define ParticlePointSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)"

RWStructuredBuffer<float3> Position : register(u0);
RWStructuredBuffer<uint>   Label    : register(u1);

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    nointerpolation uint label : TEXCOORD1;
};

static const float2 QuadCorners[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };

// A billboard quad at each active particle's position, shaded as a fake
// sphere impostor in particlePointPS.hlsl -- same technique as g-BCC's
// footPointVS.hlsl/footPointPS.hlsl. Inactive slots (label == SENTINEL_LABEL)
// are degenerate-collapsed off screen.
[RootSignature(ParticlePointSig)]
VsOut particlePointVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;
    uint label = Label[iid];
    o.label = label;

    if (label == SENTINEL_LABEL) {
        o.pos = float4(0, 0, 0, 0);
        o.uv = float2(0, 0);
        return o;
    }

    float4 clipCenter = mul(float4(Position[iid], 1), viewProjTransform);
    float2 corner     = QuadCorners[vid];
    float  radiusPx   = pointParams.z;
    float2 offsetNdc  = corner * radiusPx * (2.0 / pointParams.xy);

    o.pos = clipCenter;
    o.pos.xy += offsetNdc * clipCenter.w;
    o.uv = corner;
    return o;
}
