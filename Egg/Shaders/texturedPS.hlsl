#include "RootSignatures.hlsli"
#include "cbBasic.hlsli"

Texture2D txt : register(t0);
TextureCube env : register(t1);
SamplerState sampl : register(s0);

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirTransform;
	float4 eyePos;
	float4 lightPos;
	float4 lightPowerDensity;
}

[RootSignature(RootSig4)]
float4 main(VSOutput input) : SV_Target
{
	float3 normal = normalize(input.normal.xyz);
	float3 viewDir = normalize(eyePos.xyz - 
		input.worldPosition.xyz / input.worldPosition.w);
	float3 lightDir = normalize(lightPos.xyz);
	float3 halfway = normalize(viewDir + lightDir);
	float3 reflDir = reflect(-viewDir, normal);
	return float4(
		lightPowerDensity.rgb *
		(
			txt.Sample(sampl, input.texCoord).rgb
			* saturate(dot(normal, lightDir)) +
			float3(5, 5, 5)
			* pow(saturate(dot(normal, halfway)), 55.0)
		 )
			* 0.9 +
			env.Sample(sampl, reflDir) * 0.1
		  , 
		1);
}
