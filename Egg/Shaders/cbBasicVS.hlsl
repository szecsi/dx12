#include "RootSignatures.hlsli"
#include "cbBasic.hlsli"

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
}

[RootSignature(RootSig1)]
VSOutput main(IAOutput iao) {
	VSOutput vso;
	vso.position = mul(modelMat, float4(iao.position, 1.0f));
//	vso.position = float4(iao.position + float3(0, 0, 0.6), 1.0f);
//	vso.position.x += iao.vertexId;
	vso.position.z += 0.65;
	vso.normal = float4(iao.normal, 0.0f);
    vso.texCoord = iao.texCoord;
	return vso;
}
