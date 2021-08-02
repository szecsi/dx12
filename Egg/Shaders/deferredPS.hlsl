#include "quad.hlsli"

Texture2D fat : register(t0);
SamplerState sampl : register(s0);

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirTransform;
	float4 eyePos;
	float4 lightPos; // w = 0 irány , w = 1 pont
	float4 lightPowerDensity;
	float4 lightMainDir; // xyz dir, w falloff
}

float4 main(VSOutput input) : SV_Target
{
	float4 fat0 = 
		fat.Sample(sampl, input.tex);

	float3 normal = fat0.xyz;
	float3 worldPos = fat0.w * normalize(input.rayDir) + eyePos;
	float3 viewDir = normalize(eyePos.xyz - worldPos);
	float3 lightDiff = lightPos.xyz - worldPos.xyz * lightPos.w;
	float3 lightDir = normalize(lightDiff);
	float3 halfway = normalize(viewDir + lightDir);
	float3 reflDir = reflect(-viewDir, normal);
	return float4(
		lightPowerDensity.rgb / dot(lightDiff, lightDiff) 
		* pow(dot(lightDir, lightMainDir.xyz), lightMainDir.w) *
		(
//			txt.Sample(sampl, input.texCoord).rgb
			float3(1, 0.5, 0.5)
			* saturate(dot(normal, lightDir)) +

			float3(5, 5, 5)
			* pow(saturate(dot(normal, halfway)), 55.0)
			)
//		* 0.1 +
//		env.Sample(sampl, reflDir) * 0.9
		,
		1);
}