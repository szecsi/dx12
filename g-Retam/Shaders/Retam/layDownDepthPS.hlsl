#include "Retam.hlsli"
#include "RetamCb.hlsli"

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat   : packoffset(c0);
	float4   cameraPos     : packoffset(c8);
	float4   ahead         : packoffset(c22);
}

struct VSOutput {
	float4 position      : SV_Position;
	float4 texCoord      : TEXCOORD;
	float4 worldNormal   : NORMAL;
	float4 worldTangent  : TANGENT;
	float4 worldPosition : WORLDPOS;
};

[RootSignature(RootSigRetam)]
void layDownDepthPS(VSOutput input) {}
