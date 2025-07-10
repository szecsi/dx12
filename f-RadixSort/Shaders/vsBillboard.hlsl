#include "billboard.hlsli"
#include "particle.hlsli"

StructuredBuffer<float4> positions;

VsosBillboard vsBillboard(uint vid : SV_VertexID)
{
	VsosBillboard output;
	output.pos = positions[vid].xyz;
	output.id = vid;
	float4 hWorldPosMinusEye = mul(output.pos, rayDirM);
	hWorldPosMinusEye /= hWorldPosMinusEye.w;
	output.rayDir = hWorldPosMinusEye.xyz;
	return output;
}



