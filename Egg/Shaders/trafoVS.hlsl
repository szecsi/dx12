#include "RootSignatures.hlsli"

struct IAOutput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD;
};

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
	float4x4 modelMatInverse;
}

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirMat;
	float4 cameraPos;
	float4 lightPos;
	float4 lightPowerDensity;
	float4 billboardSize;
}

[RootSignature(RootSig4)]
VSOutput main(IAOutput iao) {
	VSOutput vso;
	vso.worldPos = mul(modelMat, float4(iao.position, 1.0f));
	vso.position = mul(viewProjMat, vso.worldPos);
    vso.texCoord = iao.texCoord;
	vso.normal = normalize(mul(float4(iao.normal, 0), modelMatInverse));
	return vso;
}
