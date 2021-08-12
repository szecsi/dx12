#include "billboard.hlsli"
#include "particle.hlsli"

StructuredBuffer<ControlParticle> particles;

VsosBillboard vsBillboardControl(uint vid : SV_VertexID)
{
	VsosBillboard output;
	output.pos = particles[vid].position.xyz;
	output.id = vid;
	float4 hWorldPosMinusEye = mul(output.pos, rayDirM);
	hWorldPosMinusEye /= hWorldPosMinusEye.w;
	output.rayDir = hWorldPosMinusEye.xyz;
	return output;
}



