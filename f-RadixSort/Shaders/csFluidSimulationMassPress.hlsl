

#include "particle.hlsli"
#include "fluid.hlsli"

StructuredBuffer<float4> positions;
RWStructuredBuffer<float> massDensities;
RWStructuredBuffer<float> pressures;

[numthreads(particlePerCore, 1, 1)]
void csFluidSimulationMassPress (uint3 DTid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	unsigned int tid = DTid.x * particlePerCore + GTid;

	// I. Find close neighbors and II. calc mass density
	float massDensity = 0.0;
	for (int i = 0; i < particleCount; i++)
	{
		float3 deltaPos = positions[tid].xyz - positions[i].xyz;
		massDensity += massPerParticle * defaultSmoothingKernel(deltaPos, supportRadius_w);
	}
	massDensities[tid] = massDensity;

	// III. Calc pressure
	pressures[tid] = gasStiffness * (massDensity - restMassDensity);
}


