#include "RootSignatures.hlsli"

struct IAOutput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 lightDirTS : TEXCOORD1;
    float3 viewDirTS : TEXCOORD2;
};

cbuffer PerObjectCb : register(b0)
{
    float4x4 modelMat;
    float4x4 invModelMat;
}

cbuffer PerFrameCb : register(b1)
{
    float4x4 viewProj;
    float4 lightPos;
    float4 eyePos;
    float4 lightIntensity;
}

[RootSignature(NormalMapRS)]
VSOutput main(IAOutput iao)
{
    float4 worldPos = mul(modelMat, float4(iao.position, 1.0f));
    float3 descartesPos = worldPos.xyz / worldPos.w;

    float3 lightDir = normalize(lightPos.xyz - descartesPos * lightPos.w);
    float3 viewDir = normalize(eyePos.xyz - descartesPos);

    float3 t = normalize(mul(float4(iao.tangent, 0.0f), invModelMat).xyz);
    float3 b = normalize(mul(float4(iao.binormal, 0.0f), invModelMat).xyz);
    float3 n = normalize(mul(float4(iao.normal, 0.0f), invModelMat).xyz);

    float3x3 tbn = { t, -b, n };

    VSOutput vso;
    vso.position = mul(viewProj, worldPos);
    vso.normal = n;
	vso.texCoord = iao.texCoord;//float2(iao.texCoord.x, 1.0f - iao.texCoord.y);
    vso.lightDirTS = mul(tbn, lightDir);
	vso.viewDirTS = mul(tbn, viewDir);

    return vso;
}
