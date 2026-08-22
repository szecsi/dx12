#include "CrossProjectCb.hlsli"

// b1 root constants: xyz = this character's world-space translation offset,
// w unused. ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT required, same reason as
// hatchGeometryVS.hlsl -- this PSO reuses the same HatchVertex vertex
// buffers via a real input layout.
#define CrossProjectSig "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=4, b1)," \
    "DescriptorTable(SRV(t0, numDescriptors=2))"

cbuffer CharParamsCb : register(b1) {
    float4 charParams;
};

// Only position is read -- the input layout still matches the full
// HatchVertex buffer (normal/hatchDir/curvature simply go unconsumed, which
// D3D12 allows).
struct VsIn {
    float3 position : POSITION;
};

struct VsOut {
    float4 pos      : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

[RootSignature(CrossProjectSig)]
VsOut crossProjectVS(VsIn i)
{
    VsOut o;
    float3 worldPos = i.position + charParams.xyz;
    o.pos = mul(float4(worldPos, 1), ownViewProjTransform);
    o.worldPos = worldPos;
    return o;
}
