#include "cbBasic.hlsli"

Texture2D txt : register(t1);
TextureCube env : register(t0);
SamplerState sampl : register(s0);

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirTransform;
	float4 eyePos;
	float4 lightPos;
	float4 lightPowerDensity;
}


float4 main(VSOutput input) : SV_Target
{
	float3 normal = normalize(input.normal.xyz);
	float3 worldPos = input.worldPosition.xyz / input.worldPosition.w;

	return float4(normal, length(worldPos - eyePos));
}
