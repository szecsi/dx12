#include "Retam.hlsli"

struct IAOutput {
	float3 position : POSITION;
	float3 normal   : NORMAL;
	float3 tangent  : TANGENT;
	float2 texCoord : TEXCOORD;
};

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat : packoffset(c0);
}

cbuffer PerObjectCb : register(b2) {
	float4x4 modelMat;
	float4x4 modelMatInv;
}

struct VSOutput {
	float4 position      : SV_Position;
	float4 texCoord      : TEXCOORD;
	float4 worldNormal   : NORMAL;
	float4 worldTangent  : TANGENT;
	float4 worldPosition : WORLDPOS;
};

[RootSignature(RootSigRetamCollect)]
VSOutput retamCollectVS(IAOutput iao) {
	VSOutput vso;
	vso.worldPosition = mul(modelMat, float4(iao.position, 1.0f));
	vso.position      = mul(viewProjMat, vso.worldPosition);
	vso.worldNormal   = mul(float4(iao.normal,  0.0f), modelMatInv);
	vso.worldTangent  = mul(modelMat, float4(iao.tangent, 0.0f));
	vso.texCoord      = float4(iao.texCoord, 0.0f, 1.0f);
	return vso;
}
