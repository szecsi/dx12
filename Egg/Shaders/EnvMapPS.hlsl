#include "RootSignatures.hlsli"

struct VSOutput {
	float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD;
};

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirMat;
	float4 cameraPos;
	float4 lightPos;
	float4 lightPowerDensity;
}

Texture2D txt : register(t0);
TextureCube env : register(t1);
SamplerState sampl : register(s0);

[RootSignature(RootSig4)]
float4 main(VSOutput vso) : SV_Target{
	float3 viewDir = normalize(cameraPos.xyz - vso.worldPos.xyz);
	return env.Sample(sampl, reflect(-viewDir, normalize(vso.normal)));
}
