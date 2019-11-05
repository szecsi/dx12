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
	float3 lightDir = lightPos.xyz;
	float3 halfWay = normalize(viewDir + lightDir);
	float3 normal = normalize(vso.normal);
	if (dot(vso.normal, lightPos.xyz) < 0)
		return 0;
	float cosa = saturate(dot(vso.normal, lightDir));
	float cosb = saturate(dot(vso.normal, viewDir));
	return (txt.Sample(sampl, vso.texCoord) *
		saturate(dot(vso.normal, lightPos.xyz))
		+
		float4(4, 4, 4, 4) * pow(saturate(dot(halfWay, normal)), 15)
		* cosa / max(cosb, cosa)
		)* lightPowerDensity;
}
