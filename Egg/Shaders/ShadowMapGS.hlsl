#include "ShadowMap.hlsli"

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
	float4x4 modelMatInv;
}


[maxvertexcount(6)]
void main(
	triangle VSOutput input[3],
	inout TriangleStream< GSOutput > output
)
{
	for (uint i = 0; i < 3; i++)
	{
		GSOutput element;
		element.worldPosition = mul(modelMat, float4(input[i].position, 1.0f));
		element.position = mul(lightViewProjMat, element.worldPosition);
		element.renderTargetArrayIndex = 0;
		output.Append(element);
	}
	output.RestartStrip();
	for (uint i = 0; i < 3; i++)
	{
		GSOutput element;
		element.worldPosition = mul(modelMat, float4(input[i].position, 1.0f));
		element.position = mul(lightViewProjMat2, element.worldPosition);
		element.renderTargetArrayIndex = 1;
		output.Append(element);
	}
}