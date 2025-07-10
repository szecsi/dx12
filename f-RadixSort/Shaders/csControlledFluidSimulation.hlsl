

#include "particle.hlsli"
#include "PBDSphere.hlsli"
#include "fluid.hlsli"


RWStructuredBuffer<Particle> particles;
StructuredBuffer<ControlParticle> controlParticles;
Buffer<uint> controlParticleCounter;
StructuredBuffer<Sphere> testMesh;

cbuffer controlParamsCB
{
	float4 controlParams[2];
};

[numthreads(particlePerCore, 1, 1)]
void csControlledFluidSimulation (uint3 DTid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	unsigned int tid = DTid.x * particlePerCore + GTid;

	// I. Find close neighbors and II. calc mass density
	{
		float massDensity = 0.0;
		for (int i = 0; i < particleCount; i++)
		{
			float3 deltaPos = particles[tid].position - particles[i].position;
			massDensity += massPerParticle * defaultSmoothingKernel(deltaPos, supportRadius_w);
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
										* massPerParticle * pressureSmoothingKernelGradient (deltaPos, supportRadius_w);
				}
			}
		}
		pressureForce *= -particles[tid].massDensity;

		// IV.b Viscosity force
		float3 viscosityForce = float3(0.0, 0.0, 0.0);
		{
			for (int i = 0; i < particleCount; i++)
			{
				if (i != tid)
				{
					float3 deltaPos = particles[tid].position - particles[i].position;
					viscosityForce += (particles[i].velocity - particles[tid].velocity) * (massPerParticle / particles[i].massDensity) * viscositySmoothingKernelLaplace (deltaPos, supportRadius_w);
					
				}
			}
			viscosityForce *= viscosity;
		}

		// IV.c SurfaceTension force
		// IV.c SurfaceTension force
		float3 surfaceTensionForce = float3(0.0, 0.0, 0.0);
		{
			float3 inwardSurfaceNormal = float3(0.0, 0.0, 0.0);
			for (int i = 0; i < particleCount; i++)
			{
				if (i != tid)
				{
					float3 deltaPos = particles[tid].position - particles[i].position;
					inwardSurfaceNormal += (massPerParticle / particles[i].massDensity) * defaultSmoothingKernelGradient(deltaPos, supportRadius_w);
				}
			}

			uint tempCount = 0;
			float surfaceTensionForceAmplitude = 0.0;
			for (int i = 0; i < particleCount; i++)
			{
				if (i != tid)
				{
					float3 deltaPos = particles[tid].position - particles[i].position;
					surfaceTensionForceAmplitude += (massPerParticle / particles[i].massDensity) * defaultSmoothingKernelLaplace(deltaPos, supportRadius_w);
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
		float3 gravitationalForce = float3 (0.0, -g * particles[tid].massDensity, 0.0) * 0.3;

		// V. Control force
		float3 controlForce = float3(0.0, 0.0, 0.0);
		{
			for (int i = 0; i < controlParticleCounter[0]; i++)
			{
				//if (i != tid && controlParticles[i].pressure == 1.0)
				{
					float3 deltaPos = particles[tid].position - controlParticles[i].position + float3 (0, controlParams[1].w,0);

					//controlForce += 0.9 * pressureSmoothingKernelGradient(deltaPos, supportRadius_w * 0.8);
					controlForce += controlParticles[i].controlPressureRatio * 0.9 * particles[i].massDensity * pressureSmoothingKernelGradient(deltaPos, supportRadius_w * 1.2);
				}
			}
		}

		float controlAmplitude = length(controlForce);
		if (controlAmplitude > 0.001)
		{
			gravitationalForce = float3(0.0, 0.0, 0.0);
			//controlForce *=  1.0 / length(controlForce);
		}

		const float maxConrtolForce = 50000.0;
		if (controlAmplitude > maxConrtolForce)
		{
			controlForce *= maxConrtolForce / controlAmplitude;
			particles[tid].velocity *= 0.9;
		}

		//float3 controlForce = float3 (0.0f, 0.0f, 0.0f);
		//controlForce = float3 (0.0f, 0.0f, 0.0f);
		/*
		for (int i = 0; i < controlParticleCount; i++)
		{
			float3 deltaPos = particles[tid].position - controlParticles[i].position;
			controlForce -=  10.0f * deltaPos;
		}
		*/
		

		// IV.e sum forces
		//sumForce = pressureForce + gravitationalForce;
		//sumForce = gravitationalForce;
		//sumForce = pressureForce + gravitationalForce;
		
		//sumForce = pressureForce + viscosityForce + gravitationalForce;
		//sumForce = pressureForce + viscosityForce + surfaceTensionForce;
		//sumForce = pressureForce + surfaceTensionForce + gravitationalForce;

		//sumForce = pressureForce + viscosityForce + surfaceTensionForce + gravitationalForce + controlForce;
		//sumForce = pressureForce + viscosityForce + controlForce;
		//sumForce = pressureForce + viscosityForce + surfaceTensionForce;
		//sumForce = pressureForce;

		if (controlParams[0].x > 0.5)
		{
			sumForce += gravitationalForce;
		}
		if (controlParams[0].y >0.5)
		{
			sumForce += pressureForce;
		}
		if (controlParams[0].z > 0.5)
		{
			sumForce += viscosityForce;
		}
		if (controlParams[0].w > 0.5)
		{
			//sumForce += surfaceTensionForce;
		}
		//if (controlParams[1].x > 0.5)
		{
			sumForce += controlForce;
		}

	}

	// V. Apply force
	if (length (sumForce) > 0.001) // TODO: Why?
	{
		particles[tid].velocity += dt * sumForce / particles[tid].massDensity;
		//particles[tid].velocity *= 0.9;
		particles[tid].position += dt * particles[tid].velocity;		
	}

	// VI. Check boundaries

	//const float boundary = 0.07;

	const float boundaryEps = 0.0001;
	const float boundaryVelDec = 0.3;

	float3 radDis = particles[tid].position - testMesh[0].pos.xyz;
	float sphereDist = length(radDis);
	radDis = normalize(radDis);
	if (sphereDist < sphereRadius) {
		particles[tid].position += radDis;
	}

	if (particles[tid].position.y < boundaryBottom)
	{
		particles[tid].position.y = boundaryBottom + boundaryEps;
		particles[tid].velocity *= boundaryVelDec;
		particles[tid].velocity.y *= -1.0;
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


