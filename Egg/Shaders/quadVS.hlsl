#include "RootSignatures.hlsli"

struct IAOutput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 rayDir: RAYDIR;
	float2 texCoord : TEXCOORD;
};

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirMat;
}

[RootSignature(RootSig4)]
VSOutput main(IAOutput iao) {
	VSOutput vso;
	vso.position = float4(iao.position.xy, 0.999f, 1.0f);
    vso.texCoord = iao.texCoord;
	vso.rayDir = mul(rayDirMat, float4(iao.position, 1.0f)).xyz;
	return vso;
}
