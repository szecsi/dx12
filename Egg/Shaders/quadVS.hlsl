#include "quad.hlsli"

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirMat;
	float4 eyePos;
	float4 lightPos;
	float4 lightPowerDensity;
}

[RootSignature(RootSig3)]
VSOutput main(IAOutput iao) {
	VSOutput vso;
	vso.position = float4(iao.position, 1.0f);
	vso.position.z = 0.999999;
	vso.rayDir = mul(rayDirMat, float4(iao.position, 1.0f)).xyz;
	vso.tex = iao.tex;
	return vso;
}
