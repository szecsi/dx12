

#include "particle.hlsli"
#include "PBDSphere.hlsli"
#include "fluid.hlsli"

StructuredBuffer<float> massDensities;
StructuredBuffer<float> frictions;
RWStructuredBuffer<float4> positions;
RWStructuredBuffer<float4> velocities;
RWStructuredBuffer<float4> particleForce;
StructuredBuffer<Sphere> testMesh;

[numthreads(particlePerCore, 1, 1)]
void csFluidSimulationFinal (uint3 DTid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	unsigned int tid = DTid.x * particlePerCore + GTid;

	const float boundaryStiffness = 100.0;
	const float boundaryForce = 10.0;
	const float maxBoundaryForce = 1000000.0;

	float fluidFriction = frictions[tid];

	float distBottom = positions[tid].xyz.y - boundaryBottom_Fluid;
	float boundarySide1 = positions[tid].xyz.x + boundarySide_Fluid;
	float boundarySide2 = boundarySide_Fluid - positions[tid].xyz.x;
	float boundarySide3 = positions[tid].xyz.z + boundarySide_Fluid;
	float boundarySide4 = boundarySide_Fluid - positions[tid].xyz.z;

	float3 sumForce = particleForce[tid].xyz;
	sumForce.y += min (exp(-distBottom * boundaryStiffness) * boundaryForce, maxBoundaryForce);
	sumForce.x += min(exp(-boundarySide1 * boundaryStiffness) * boundaryForce, maxBoundaryForce);
	sumForce.x -= min(exp(-boundarySide2 * boundaryStiffness) * boundaryForce, maxBoundaryForce);
	sumForce.z += min(exp(-boundarySide3 * boundaryStiffness) * boundaryForce, maxBoundaryForce);
	sumForce.z -= min(exp(-boundarySide4 * boundaryStiffness) * boundaryForce, maxBoundaryForce);

	float3 radDis = positions[tid].xyz - testMesh[0].pos.xyz;
	float sphereDist = length(radDis);
	float3 radNorm = normalize(radDis);
	if (sphereDist < sphereRadius) {
		sumForce += radNorm * min(exp(10) * boundaryForce, maxBoundaryForce);
		fluidFriction *= 0.25;
	}

	//sumForce *= 0.0;

	// V. Apply force
	if (length (sumForce) > 0.001) // TODO: Why?
	{
		velocities[tid].xyz += dt * sumForce / massDensities[tid];
		velocities[tid].xyz *= fluidFriction;
		positions[tid].xyz += dt * velocities[tid].xyz;		
	}
	/*
	for (int i = 0; i < particleCount; i++)
	{
		if (i != tid)
		{
			float dist = length(positions[tid].xyz - positions[i].xyz);
			if (dist < 0.01) {
				if (tid < i) {
					positions[tid].xyz += (float3 (1.0, 1.0, 1.0) * 0.01);
				}
				else
				{
					positions[tid].xyz -= (float3 (1.0, 1.0, 1.0) * 0.01);
				}
			}
		}
	}
	*/
	//if (length(velocities[tid].xyz) > 3.0) {
	//	positions[tid].y = 0.8;
	//}

	// VI. Check boundaries

	//const float boundary = 0.07;
	/*
	
	if (positions[tid].xyz.y < boundaryBottom)
	{
		positions[tid].xyz.y = boundaryBottom;
		velocities[tid].xyz.y = 0.0;
	}

	if (positions[tid].xyz.y > boundaryTop)
	{
		positions[tid].xyz.y = boundaryTop;
		velocities[tid].xyz.y = 0.0;
	}

	if (positions[tid].xyz.z > boundarySide)
	{
		positions[tid].xyz.z = boundarySide;
		velocities[tid].xyz.z = 0.0;
	}

	if (positions[tid].xyz.z < -boundarySide)
	{
		positions[tid].xyz.z = -boundarySide;
		velocities[tid].xyz.z = 0.0;
	}

	if (positions[tid].xyz.x > boundarySide)
	{
		positions[tid].xyz.x = boundarySide;
		velocities[tid].xyz.x = 0.0;
	}

	if (positions[tid].xyz.x < -boundarySide)
	{
		positions[tid].xyz.x = -boundarySide;
		velocities[tid].xyz.x = 0.0;
	}
	*
	*/
	
	const float boundaryEps = 0.01;
	const float boundaryVelDec = 0.5;
	const float boundaryMult = 1.0;
	//const float boundaryVelDec = 0.0;
	/*
	float3 radDis = positions[tid].xyz - testMesh[0].pos.xyz;
	float sphereDist = length(radDis);
	float3 radNorm = normalize(radDis);
	if (sphereDist < sphereRadius) {
		positions[tid].xyz += radDis;
		velocities[tid].xyz = velocities[tid].xyz - radNorm * dot(radNorm, velocities[tid].xyz);
		velocities[tid].xyz *= 0.0;
	}
	*/
	
	

	if (positions[tid].xyz.y < boundaryBottom * boundaryMult)
	{
		positions[tid].xyz.y = boundaryBottom + boundaryEps;
		velocities[tid].xyz *= boundaryVelDec;
		velocities[tid].xyz.y *= - 1.0;
	}

	if (positions[tid].xyz.y > boundaryTop * boundaryMult)
	{
		positions[tid].xyz.y = boundaryTop - boundaryEps;
		velocities[tid].xyz *= boundaryVelDec;
		velocities[tid].xyz.y *= -1.0;
	}

	if (positions[tid].xyz.z > boundarySide * boundaryMult)
	{
		positions[tid].xyz.z = boundarySide - boundaryEps;
		velocities[tid].xyz *= boundaryVelDec;
		velocities[tid].xyz.z *= -1.0;
	}

	if (positions[tid].xyz.z < -boundarySide * boundaryMult)
	{
		positions[tid].xyz.z = -boundarySide + boundaryEps;
		velocities[tid].xyz *= boundaryVelDec;
		velocities[tid].xyz.z *= -1.0;
	}

	if (positions[tid].xyz.x > boundarySide * boundaryMult)
	{
		positions[tid].xyz.x = boundarySide - boundaryEps;
		velocities[tid].xyz *= boundaryVelDec;
		velocities[tid].xyz.x *= -1.0;
	}

	if (positions[tid].xyz.x < -boundarySide * boundaryMult)
	{
		positions[tid].xyz.x = -boundarySide + boundaryEps;
		velocities[tid].xyz *= boundaryVelDec;
		velocities[tid].xyz.x *= -1.0;
	}
	
	
}


