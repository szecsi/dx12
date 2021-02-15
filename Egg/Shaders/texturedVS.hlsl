#include "RootSignatures.hlsli"
#include "cbBasic.hlsli"

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
	float4x4 modelMatInv;
}

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirTransform;
	float4 eyePos;
	float4 lightPos;
	float4 lightPowerDensity;
}

[RootSignature(RootSig4)]
VSOutput main(IAOutput iao) {
	VSOutput vso;
	vso.worldPosition = mul(modelMat, float4(iao.position, 1.0f));
	vso.position = mul(viewProjMat, vso.worldPosition);
//	vso.position = float4(iao.position + float3(0, 0, 0.6), 1.0f);
//	vso.position.x += iao.vertexId;
	vso.normal = mul(float4(iao.normal, 0.0f), modelMatInv);
    vso.texCoord = iao.texCoord;
	return vso;
}
