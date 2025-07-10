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

layout(std430, binding = 2) buffer velocityBuffer
{
	vec4 velocity[];
};

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

const float dt = 0.01;

void main()
{
	uint idx = gl_GlobalInvocationID.x;
	uint idy = gl_GlobalInvocationID.y;
	uint id = idx + idy * 64;

	velocity[id].xyz = (positionTmp[id].xyz - position[id].xyz) / dt;
	position[id].xyz = positionTmp[id].xyz;
}
*/

#include "particle.hlsli"
#include "PBD.hlsli"

RWStructuredBuffer<float4> controlPositions;
Buffer<uint> controlParticleCounter;
StructuredBuffer<float4> newPos;
RWStructuredBuffer<float4> velocity;

[numthreads(1, 1, 1)]
void csPBDFinalUpdate(uint3 DTid : SV_GroupID) {
	unsigned int tid = DTid.x;

	if (tid < controlParticleCounter[0]) {

		velocity[tid].xyz = (newPos[tid].xyz - controlPositions[tid].xyz) / dt;
		controlPositions[tid].xyz = newPos[tid].xyz;
		velocity[tid].xyz *= 0.9;
	}
}