#include "BccFrameCb.hlsli"

#define RaymarchSig "RootFlags(0)," \
    "CBV(b0)," \
    "DescriptorTable(SRV(t0, numDescriptors=4))"

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

// Full-screen triangle from SV_VertexID alone -- no vertex/index buffers.
[RootSignature(RaymarchSig)]
VsOut raymarchVS(uint vid : SV_VertexID)
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
