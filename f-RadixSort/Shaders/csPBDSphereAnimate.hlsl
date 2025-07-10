
#include "particle.hlsli"
#include "PBD.hlsli"
#include "PBDSphere.hlsli"

Buffer<uint> controlParticleCounter;

RWStructuredBuffer<Sphere> testMesh;

RWStructuredBuffer<float4> testMeshTrans;

[numthreads(1, 1, 1)]
void csPBDSphereAnimate() {

	float3 pos = testMesh[0].pos.xyz;
	float3 vel = testMesh[0].vel.xyz;
	float dt_2 = dt * 0.1;

	// Prev
	pos += vel * dt_2;

	
	// Sponge
	float3 diffFromSponge = float3 (0.0, 0.0, 0.0);
	for (uint i = 0; i < controlParticleCounter[0]; i++) {
		diffFromSponge += testMeshTrans[i].xyz;
	}

	diffFromSponge *= 0.01;

	vel += diffFromSponge / dt;
	pos += diffFromSponge;

	// Grav
	const float3 grav = float3(0.0, -0.98, 0.0);
	vel += grav;
	pos += grav * dt_2;

	// Collision
	float3 velDir = normalize(vel);
	float speed = length(vel) * 0.9;
	const float boundaryEps = 0.0001;
	
	if (pos.y < boundaryBottom + sphereRadius)
	{
		pos.y = boundaryBottom + sphereRadius + boundaryEps;
		vel.xyz = reflect(velDir, float3(0.0, 1.0, 0.0)) * speed;
	}
	/*
	if (pos.y > boundaryTop)
	{
		pos.y = boundaryTop - boundaryEps;
	}
	*/
	if (pos.z > boundarySide - sphereRadius)
	{
		pos.z = boundarySide - sphereRadius - boundaryEps;
		vel.xyz = reflect(velDir, float3(0.0, 0.0, -1.0)) * speed;
	}

	if (pos.z < -boundarySide + sphereRadius)
	{
		pos.z = -boundarySide + sphereRadius + boundaryEps;
		vel.xyz = reflect(velDir, float3(0.0, 0.0, 1.0)) * speed;
	}

	if (pos.x > boundarySide - sphereRadius)
	{
		pos.x = boundarySide - sphereRadius - boundaryEps;
		vel.xyz = reflect(velDir, float3(-1.0, 0.0, 0.0)) * speed;
	}

	if (pos.x < -boundarySide + sphereRadius)
	{
		pos.x = -boundarySide + sphereRadius + boundaryEps;
		vel.xyz = reflect(velDir, float3(1.0, 0.0, 0.0)) * speed;
	}
	

	testMesh[0].pos = float4 (pos, 1.0);
	testMesh[0].vel = float4 (vel, 0.0);
}