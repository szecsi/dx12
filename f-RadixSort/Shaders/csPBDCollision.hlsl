/*
#version 430
#extension GL_ARB_compute_shader : enable
#extension GL_ARB_shader_storage_buffer_object : enable

layout(std430, binding = 0) buffer positionBuffer
{
	vec4 position[];
};

layout(std430, binding = 1) buffer positionBufferTmp
{
	vec4 positionTmp[];
};

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

const float fact = 1.0;
const float radeps = 0.0;

void main()
{
	uint idx = gl_GlobalInvocationID.x;
	uint idy = gl_GlobalInvocationID.y;
	uint id = idx + idy * 64;

	vec3 center = vec3 (0.1, -0.7, 0.0);
	float radius = 0.5;

	float dist = distance (center, positionTmp[id].xyz);
	if (dist < radius)
	{
		vec3 dir = normalize(positionTmp[id].xyz - center);
		dir *= (radius + radeps) * fact;
		positionTmp[id].xyz = center + dir;
	}
}
*/

#include "particle.hlsli"

Buffer<uint> controlParticleCounter;
RWStructuredBuffer<float4> newPos;

[numthreads(1, 1, 1)]
void csPBDCollision(uint3 DTid : SV_GroupID) {
	unsigned int tid = DTid.x;

	const float boundaryEps = 0.0001;

	if (tid < controlParticleCounter[0]) {
		if (newPos[tid].y < boundaryBottom)
		{
			newPos[tid].y = boundaryBottom + boundaryEps;
		}

		if (newPos[tid].y > boundaryTop)
		{
			newPos[tid].y = boundaryTop - boundaryEps;
		}

		if (newPos[tid].z > boundarySide)
		{
			newPos[tid].z = boundarySide - boundaryEps;
		}

		if (newPos[tid].z < -boundarySide)
		{
			newPos[tid].z = -boundarySide + boundaryEps;
		}
		
		if (newPos[tid].x > boundarySide)
		{
			newPos[tid].x = boundarySide - boundaryEps;
		}

		if (newPos[tid].x < -boundarySide)
		{
			newPos[tid].x = -boundarySide + boundaryEps;
		}
		
	}
}