
#include "particle.hlsli"
#include "PBDSphere.hlsli"

#define pi 3.1415

StructuredBuffer<float4> controlPositions;
Buffer<uint> controlParticleCounter;

StructuredBuffer<float4> positions;
StructuredBuffer<float4> velocities;

RWStructuredBuffer<float4> controlVelocities;

float viscositySmoothingKernelLaplace(float3 deltaPos, float supportRadius)
{
	float lengthOfDeltaPos = length(deltaPos);
	if (lengthOfDeltaPos > supportRadius)
	{
		return 0.0;
	}
	else
	{
		return (45.0 / (pi * pow(supportRadius, 6))) * (supportRadius - lengthOfDeltaPos);
	}
}

[numthreads(1, 1, 1)]
void csPBDVelocityFilter(uint3 DTid : SV_GroupID) {
	float massPerParticle = 0.02;		// kg
	float restMassDensity = 998.29;	// kg/m3
	float supportRadius = 0.0457;	// m
	float gasStiffness = 3.0;		// J
	float viscosity = 3.5;		// Pa*s
	float surfaceTension = 0.0728;	// N/m

	unsigned int tid = DTid.x;

	if (tid < controlParticleCounter[0]) {

		// IV.b Viscosity force
		float3 viscosityForce = float3(0.0, 0.0, 0.0);
		{
			for (int i = 0; i < particleCount; i++)
			{
				//if (i != tid)
				{
					//float3 deltaPos = positions[tid].xyz - positions[i].xyz;
					//viscosityForce += (velocities[i].xyz - velocities[tid].xyz) * (massPerParticle / massDensities[i]) * viscositySmoothingKernelLaplace(deltaPos, supportRadius_w);

					float3 deltaPos = controlPositions[tid].xyz - positions[i].xyz;
					//viscosityForce += (particles[i].velocity - velocity[tid].xyz) * (massPerParticle / particles[i].massDensity) * viscositySmoothingKernelLaplace(deltaPos, supportRadius);
					//viscosityForce += (particles[i].velocity - velocity[tid].xyz) * viscositySmoothingKernelLaplace(deltaPos, supportRadius);
					//viscosityForce += (velocities[i].xyz - controlVelocities[tid].xyz) * viscositySmoothingKernelLaplace(deltaPos, supportRadius) * 0.0000000002;
					viscosityForce += (velocities[i].xyz - controlVelocities[tid].xyz) * viscositySmoothingKernelLaplace(deltaPos, supportRadius) * 0.0000000004;

				}
			}
			viscosityForce *= viscosity;
		}

		controlVelocities[tid].xyz += viscosityForce;
		//velocity[tid].xyz += float3 (0.0, 0.1, 0.0);

	}
}