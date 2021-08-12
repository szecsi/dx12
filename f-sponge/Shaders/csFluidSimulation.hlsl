

#include "particle.hlsli"

RWStructuredBuffer<Particle> particles;

#define pi 3.1415

float defaultSmoothingKernel (float3 deltaPos, float supportRadius)
{
	if (length(deltaPos) > supportRadius)
	{
		return 0.0;
	}
	else
	{
		return (315.0 / (64.0 * pi * pow(supportRadius, 9))) * pow((pow(supportRadius, 2) - dot(deltaPos, deltaPos)), 3);
	}
}

float3 defaultSmoothingKernelGradient (float3 deltaPos, float supportRadius)
{
	if (length(deltaPos) > supportRadius)
	{
		return float3 (0.0, 0.0, 0.0);
	}
	else
	{
		return (-945.0 / (32.0 * pi * pow(supportRadius, 9))) * deltaPos * pow((pow(supportRadius, 2) - dot(deltaPos, deltaPos)), 2);
	}
}

float defaultSmoothingKernelLaplace (float3 deltaPos, float supportRadius)
{
	if (length(deltaPos) > supportRadius)
	{
		return 0.0;
	}
	else
	{
		//return (-945.0 / (32.0 * pi * pow(supportRadius, 9))) * deltaPos * (pow(supportRadius, 2) - dot(deltaPos, deltaPos)) * (3.0 * pow(supportRadius, 2) - 7.0 * dot(deltaPos, deltaPos));
		return (-945.0 / (32.0 * pi * pow(supportRadius, 9))) * (pow(supportRadius, 2) - dot(deltaPos, deltaPos)) * (3.0 * pow(supportRadius, 2) - 7.0 * dot(deltaPos, deltaPos));
	}
}

float pressureSmoothingKernel(float3 deltaPos, float supportRadius)
{
	float lengthOfDeltaPos = length(deltaPos);
	if (lengthOfDeltaPos > supportRadius)
	{
		return 0.0;
	}
	else
	{
		return (15.0 / (pi * pow(supportRadius, 6))) * pow(supportRadius - lengthOfDeltaPos, 3);
	}
}

float3 pressureSmoothingKernelGradient (float3 deltaPos, float supportRadius)
{
	float lengthOfDeltaPos = length(deltaPos);
	if (lengthOfDeltaPos > supportRadius)
	{
		return float3 (0.0, 0.0, 0.0);
	}
	else
	{
		return (-45.0 / (pi * pow(supportRadius, 6))) * (deltaPos/ lengthOfDeltaPos) * pow(supportRadius - lengthOfDeltaPos, 2);
	}
}

float viscositySmoothingKernelLaplace (float3 deltaPos, float supportRadius)
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
void csFluidSimulation (uint3 DTid : SV_GroupID)
{
	float dt	= 0.01; // s
	float g		= 9.82; // m/s2

	// Water
	float massPerParticle	= 0.02;		// kg
	float restMassDensity	= 998.29;	// kg/m3
	float supportRadius		= 0.0457;	// m
	float gasStiffness		= 3.0;		// J
	float viscosity			= 3.5;		// Pa*s
	float surfaceTension	= 0.0728;	// N/m

	unsigned int tid = DTid.x;

	// I. Find close neighbors and II. calc mass density
	{
		float massDensity = 0.0;
		for (int i = 0; i < particleCount; i++)
		{
			float3 deltaPos = particles[tid].position - particles[i].position;
			massDensity += massPerParticle * defaultSmoothingKernel(deltaPos, supportRadius);
		}
		particles[tid].massDensity = massDensity;
	}

	// III. Calc pressure
	{
		particles[tid].pressure = gasStiffness * (particles[tid].massDensity - restMassDensity);
	}
	
	AllMemoryBarrierWithGroupSync();

	// IV. Calc forces
	float3 sumForce = float3 (0.0, 0.0, 0.0);
	{
		// IV.a Pressure force
		float3 pressureForce = float3(0.0, 0.0, 0.0);
		{
			for (int i = 0; i < particleCount; i++)
			{
				if (i != tid)
				{
					float3 deltaPos = particles[tid].position - particles[i].position;

					pressureForce +=	((particles[tid].pressure / pow(particles[tid].massDensity, 2)) + (particles[i].pressure / pow(particles[i].massDensity, 2)))
										* massPerParticle * pressureSmoothingKernelGradient (deltaPos, supportRadius);
				}
			}
			pressureForce *= -particles[tid].massDensity;
		}

		// IV.b Viscosity force
		float3 viscosityForce = float3(0.0, 0.0, 0.0);
		{
			for (int i = 0; i < particleCount; i++)
			{
				if (i != tid)
				{
					float3 deltaPos = particles[tid].position - particles[i].position;
					viscosityForce += (particles[i].velocity - particles[tid].velocity) * (massPerParticle / particles[i].massDensity) * viscositySmoothingKernelLaplace (deltaPos, supportRadius);
					
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
					float3 deltaPos = particles[tid].position - particles[i].position;
					inwardSurfaceNormal += (massPerParticle / particles[i].massDensity) * defaultSmoothingKernelGradient (deltaPos, supportRadius);
				}
			}

			uint tempCount = 0;
			float surfaceTensionForceAmplitude = 0.0;
			for (int i = 0; i < particleCount; i++)
			{
				if (i != tid)
				{					
					float3 deltaPos = particles[tid].position - particles[i].position;
					surfaceTensionForceAmplitude += (massPerParticle / particles[i].massDensity) * defaultSmoothingKernelLaplace(deltaPos, supportRadius);
					if (length(deltaPos) < supportRadius)
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
		float3 gravitationalForce = float3 (0.0, -g * particles[tid].massDensity, 0.0);


		// IV.e sum forces
		//sumForce = pressureForce + gravitationalForce;
		//sumForce = gravitationalForce;
		//sumForce = pressureForce + gravitationalForce;
		
		//sumForce = pressureForce + viscosityForce + gravitationalForce;
		//sumForce = pressureForce + viscosityForce + surfaceTensionForce;
		//sumForce = pressureForce + surfaceTensionForce + gravitationalForce;

		sumForce = pressureForce + viscosityForce + surfaceTensionForce + gravitationalForce;
	}

	// V. Apply force
	if (length (sumForce) > 0.001) // TODO: Why?
	{
		particles[tid].velocity += dt * sumForce / particles[tid].massDensity;
		particles[tid].position += dt * particles[tid].velocity;		
	}

	// VI. Check boundaries

	//const float boundary = 0.07;
	
	/*
	if (particles[tid].position.y < boundaryBottom)
	{
		particles[tid].position.y = boundaryBottom;
		particles[tid].velocity.y = 0.0;
	}

	if (particles[tid].position.y > boundaryTop)
	{
		particles[tid].position.y = boundaryTop;
		particles[tid].velocity.y = 0.0;
	}

	if (particles[tid].position.z > boundarySide)
	{
		particles[tid].position.z = boundarySide;
		particles[tid].velocity.z = 0.0;
	}

	if (particles[tid].position.z < -boundarySide)
	{
		particles[tid].position.z = -boundarySide;
		particles[tid].velocity.z = 0.0;
	}

	if (particles[tid].position.x > boundarySide)
	{
		particles[tid].position.x = boundarySide;
		particles[tid].velocity.x = 0.0;
	}

	if (particles[tid].position.x < -boundarySide)
	{
		particles[tid].position.x = -boundarySide;
		particles[tid].velocity.x = 0.0;
	}
	*/

	const float boundaryEps = 0.0001;
	const float boundaryVelDec = 0.2;

	if (particles[tid].position.y < boundaryBottom)
	{
		particles[tid].position.y = boundaryBottom + boundaryEps;
		particles[tid].velocity *= boundaryVelDec;
		particles[tid].velocity.y *= - 1.0;
	}

	if (particles[tid].position.y > boundaryTop)
	{
		particles[tid].position.y = boundaryTop - boundaryEps;
		particles[tid].velocity *= boundaryVelDec;
		particles[tid].velocity.y *= -1.0;
	}

	if (particles[tid].position.z > boundarySide)
	{
		particles[tid].position.z = boundarySide - boundaryEps;
		particles[tid].velocity *= boundaryVelDec;
		particles[tid].velocity.z *= -1.0;
	}

	if (particles[tid].position.z < -boundarySide)
	{
		particles[tid].position.z = -boundarySide + boundaryEps;
		particles[tid].velocity *= boundaryVelDec;
		particles[tid].velocity.z *= -1.0;
	}

	if (particles[tid].position.x > boundarySide)
	{
		particles[tid].position.x = boundarySide - boundaryEps;
		particles[tid].velocity *= boundaryVelDec;
		particles[tid].velocity.x *= -1.0;
	}

	if (particles[tid].position.x < -boundarySide)
	{
		particles[tid].position.x = -boundarySide + boundaryEps;
		particles[tid].velocity *= boundaryVelDec;
		particles[tid].velocity.x *= -1.0;
	}
}


