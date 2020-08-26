#include "RootSignatures.hlsli"

struct IAOutput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
    float3 viewDir : VIEWDIR;
};

cbuffer PerFrameCb : register(b1)
{
    float4x4 viewProj;
    float4 lightPos;
    float4 eyePos;
    float4 lightIntensity;
    float4x4 invViewProj;
}

[RootSignature(NormalMapRS)]
VSOutput main(IAOutput iao)
{
    VSOutput vso;
    vso.position = float4(iao.position.xy, 0.9999f, 1.0f);
    vso.texCoord = iao.texCoord;
    float4 wPosMinusEye = mul(invViewProj, float4(iao.position, 1));
    vso.viewDir = wPosMinusEye.xyz / wPosMinusEye.w;


    return vso;
}
