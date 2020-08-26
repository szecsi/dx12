#include "RootSignatures.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
    float3 viewDir : VIEWDIR;
};


TextureCube envTex : register(t3);

SamplerState sampl : register(s0);

cbuffer PerFrameCb : register(b1)
{
    float4x4 viewProj;
    float4 lightPos;
    float4 eyePos;
    float4 lightIntensity;
}

[RootSignature(NormalMapRS)]
float4 main(VSOutput vso) : SV_Target
{
    return envTex.Sample(sampl, vso.viewDir);

}
