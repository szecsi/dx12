
#include "particle.hlsli"
#include "mortonHash.hlsli"

RWStructuredBuffer<Particle> particles;



[numthreads(1, 1, 1)]
void csMortonHash(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;
	particles[tid].zindex = mortonHash(particles[tid].position);
}


