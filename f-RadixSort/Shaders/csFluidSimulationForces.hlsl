

#include "particle.hlsli"
#include "fluid.hlsli"

StructuredBuffer<float4> positions;
StructuredBuffer<float4> velocities;
StructuredBuffer<float> massDensities;
StructuredBuffer<float> pressures;
RWStructuredBuffer<float4> particleForce;
RWStructuredBuffer<float> frictions;

[numthreads(particlePerCore, 1, 1)]
void csFluidSimulationForces (uint3 DTid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	unsigned int tid = DTid.x * particlePerCore + GTid;

	// IV. Calc forces
	float3 sumForce = float3 (0.0, 0.0, 0.0);

	// IV.a Pressure force
	float3 pressureForce = float3(0.0, 0.0, 0.0);
	{
		for (int i = 0; i < particleCount; i++)
		{
			if (i != tid)
			{
				float3 deltaPos = positions[tid].xyz - positions[i].xyz;

				pressureForce +=	((pressures[tid] / pow(massDensities[tid], 2)) + (pressures[i] / pow(massDensities[i], 2)))
									* massPerParticle * pressureSmoothingKernelGradient (deltaPos, supportRadius_w);
			}
		}
		pressureForce *= -massDensities[tid];
	}

	// IV.b Viscosity force
	float3 viscosityForce = float3(0.0, 0.0, 0.0);
	{
		for (int i = 0; i < particleCount; i++)
		{
			if (i != tid)
			{
				float3 deltaPos = positions[tid].xyz - positions[i].xyz;
				viscosityForce += (velocities[i].xyz - velocities[tid].xyz) * (massPerParticle / massDensities[i]) * viscositySmoothingKernelLaplace (deltaPos, supportRadius_w);
					
			}
		}
		viscosityForce *= viscosity;
	}

	// IV.c SurfaceTension force
	float3 surfaceTensionForce = float3(0.0, 0.0, 0.0);
	{
		float3 inwardSurfaceNormal = float3(0.0, 0.0, 0.0);
		for (int i = 0; i < particleCount; i++)
		{
			if (i != tid)
			{
				float3 deltaPos = positions[tid].xyz - positions[i].xyz;
				inwardSurfaceNormal += (massPerParticle / massDensities[i]) * defaultSmoothingKernelGradient (deltaPos, supportRadius_w);
			}
		}

		uint tempCount = 0;
		float surfaceTensionForceAmplitude = 0.0;
		for (int i = 0; i < particleCount; i++)
		{
			if (i != tid)
			{					
				float3 deltaPos = positions[tid].xyz - positions[i].xyz;
				surfaceTensionForceAmplitude += (massPerParticle / massDensities[i]) * defaultSmoothingKernelLaplace(deltaPos, supportRadius_w);
				if (length(deltaPos) < supportRadius_w)
				{
					tempCount++;
				}
			}
		}
		if (tempCount > 0)
		{
			surfaceTensionForce = -surfaceTension * normalize(inwardSurfaceNormal) * surfaceTensionForceAmplitude;
		}			
	}

	// IV.d Gravitational force
	float3 gravitationalForce = float3 (0.0, -g * massDensities[tid], 0.0);


	// IV.e sum forces
	//sumForce = pressureForce + gravitationalForce;
	//sumForce = gravitationalForce;
	//sumForce = pressureForce + gravitationalForce;
		
	//sumForce = pressureForce + viscosityForce + gravitationalForce;
	//sumForce = pressureForce + viscosityForce + surfaceTensionForce;
	//sumForce = pressureForce + surfaceTensionForce + gravitationalForce;

	sumForce = pressureForce + viscosityForce + surfaceTensionForce + gravitationalForce;

	particleForce[tid].xyz = sumForce;

	frictions[tid] = 1.0;
}


