#define VrTestSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(num32BitConstants=16, b0)"

cbuffer WvpCb : register(b0) {
    row_major float4x4 wvp;
}

struct VsIn {
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct VsOut {
    float4 position : SV_POSITION;
    float3 color    : COLOR;
};

[RootSignature(VrTestSig)]
VsOut vrTestVS(VsIn i)
{
    VsOut o;
    o.position = mul(float4(i.position, 1.0), wvp);
    o.color = i.color;
    return o;
}
