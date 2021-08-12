#include "billboard.hlsli"
#include "particle.hlsli"

StructuredBuffer<Particle> particles;

VsosBillboard vsBillboard(uint vid : SV_VertexID)
{
	VsosBillboard output;
	output.pos = particles[vid].position.xyz;
	output.id = vid;
	float4 hWorldPosMinusEye = mul(output.pos, rayDirM);
	hWorldPosMinusEye /= hWorldPosMinusEye.w;
	output.rayDir = hWorldPosMinusEye.xyz;
	return output;
}



