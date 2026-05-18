#include "Shaders/treeCB.hlsli"
#include "Shaders/tree.hlsli"

struct VSOutput {
	float4 position      : SV_Position;
	float4 texCoord      : TEXCOORD;
	float4 worldNormal   : NORMAL;
	float4 worldTangent  : TANGENT;
	float4 worldPosition : WORLDPOS;
};

[RootSignature(RootSigTree)]
float4 treePS(VSOutput input) : SV_TARGET
{
	return float4(input.texCoord.rgb, 1.0f);
}