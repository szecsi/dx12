#include "RootSignatures.hlsli"
#include "ShadowMap.hlsli"

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
	float4x4 modelMatInv;
}


[RootSignature(RootSig4)]
VSOutput main(IAOutput iao) {
	return iao;
/*	VSOutput vso;
	vso.worldPosition = mul(modelMat, float4(iao.position, 1.0f));
	vso.position = mul(lightViewProjMat, vso.worldPosition);
	return vso;*/
}
