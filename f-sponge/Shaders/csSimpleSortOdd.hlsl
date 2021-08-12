

#include "particle.hlsli"

RWStructuredBuffer<Particle> particles;

[numthreads(1, 1, 1)]
void csSimpleSortOdd(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;
	unsigned int firstIdx = tid * 2 + 1;
	unsigned int secondIdx = firstIdx + 1;

	if (particles[firstIdx].zindex > particles[secondIdx].zindex)
	{
		Particle temp = particles[firstIdx];
		particles[firstIdx] = particles[secondIdx];
		particles[secondIdx] = temp;
	}
}


