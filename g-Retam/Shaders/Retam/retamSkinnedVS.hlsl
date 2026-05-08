#include "Retam.hlsli"

struct IAOutput {
	float3 position     : POSITION;
	float3 normal       : NORMAL;
	float3 tangent      : TANGENT;
	float2 texCoord     : TEXCOORD;
	uint4  blendIndices : BLENDINDICES;
	float4 blendWeights : BLENDWEIGHT;
};

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat : packoffset(c0);
}

cbuffer PerObjectCb : register(b2) {
	float4x4 modelMat;
	float4x4 modelMatInv;
}

cbuffer SkinCb : register(b3) {
	float4x4 bones[128];
}

struct VSOutput {
	float4 position      : SV_Position;
	float4 texCoord      : TEXCOORD;
	float4 worldNormal   : NORMAL;
	float4 worldTangent  : TANGENT;
	float4 worldPosition : WORLDPOS;
};

[RootSignature(RootSigRetamSkinned)]
VSOutput retamSkinnedVS(IAOutput iao) {
	float4x4 skinMat =
		iao.blendWeights.x * bones[iao.blendIndices.x] +
		iao.blendWeights.y * bones[iao.blendIndices.y] +
		iao.blendWeights.z * bones[iao.blendIndices.z] +
		iao.blendWeights.w * bones[iao.blendIndices.w];

	float4 skinnedPos  = mul(skinMat, float4(iao.position, 1.0f));
	float4 skinnedNorm = mul(skinMat, float4(iao.normal,   0.0f));
	float4 skinnedTang = mul(skinMat, float4(iao.tangent,  0.0f));

	VSOutput vso;
	vso.worldPosition = mul(modelMat,    skinnedPos);
	vso.position      = mul(viewProjMat, vso.worldPosition);
	vso.worldNormal   = mul(float4(skinnedNorm.xyz, 0.0f), modelMatInv);
	vso.worldTangent  = mul(modelMat,    float4(skinnedTang.xyz, 0.0f));
	vso.texCoord      = float4(iao.texCoord, 0.0f, 1.0f);
	return vso;
}
