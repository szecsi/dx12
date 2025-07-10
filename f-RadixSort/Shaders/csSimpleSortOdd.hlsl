

#include "particle.hlsli"

RWStructuredBuffer<float4> positions;
RWStructuredBuffer<float4> velocities;
RWStructuredBuffer<float> massDensities;
RWStructuredBuffer<float> pressures;
RWStructuredBuffer<uint> hashes;

[numthreads(1, 1, 1)]
void csSimpleSortOdd(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;
	unsigned int firstIdx = tid * 2 + 1;
	unsigned int secondIdx = firstIdx + 1;

	if (hashes[firstIdx] > hashes[secondIdx])
	{
		float4 temp4;

		temp4 = positions[firstIdx];
		positions[firstIdx] = positions[secondIdx];
		positions[secondIdx] = temp4;

		temp4 = velocities[firstIdx];
		velocities[firstIdx] = velocities[secondIdx];
		velocities[secondIdx] = temp4;


		float temp;

		temp = massDensities[firstIdx];
		massDensities[firstIdx] = massDensities[secondIdx];
		massDensities[secondIdx] = temp;

		temp = pressures[firstIdx];
		pressures[firstIdx] = pressures[secondIdx];
		pressures[secondIdx] = temp;
	}
}


