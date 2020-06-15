#include "RootSignatures.hlsli"

struct IAOutput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
};

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
}

[RootSignature(RootSig2)]
VSOutput main(IAOutput iao) {
	VSOutput vso;
	vso.position = mul(modelMat, float4(iao.position, 1.0f));
    vso.texCoord = iao.texCoord;
	return vso;
}
