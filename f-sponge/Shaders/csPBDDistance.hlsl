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

const float fact = 0.5;
const float fact2 = 0.3;

void main()
{
	uint idx = gl_GlobalInvocationID.x;
	uint idy = gl_GlobalInvocationID.y;
	uint id = idx + idy * 64;

	float d1 = 1.0 / 64.0;
	uint neighbourId1 = (idx + 1) + (idy) * 64;
	uint neighbourId2 = (idx - 1) + (idy) * 64;
	uint neighbourId3 = (idx) + (idy + 1) * 64;
	uint neighbourId4 = (idx) + (idy - 1) * 64;

	float d2 = sqrt(2.0) * d1;
	uint neighbourId5 = (idx + 1) + (idy + 1) * 64;
	uint neighbourId6 = (idx - 1) + (idy + 1) * 64;
	uint neighbourId7 = (idx + 1) + (idy - 1) * 64;
	uint neighbourId8 = (idx - 1) + (idy - 1) * 64;

	vec3 sumdir = vec3 (0.0, 0.0, 0.0);
	if (idx < 63) { sumdir += normalize(positionTmp[neighbourId1].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId1].xyz,  positionTmp[id].xyz) - d1) * 0.5 * fact; }
	if (idx > 0)  { sumdir += normalize(positionTmp[neighbourId2].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId2].xyz,  positionTmp[id].xyz) - d1) * 0.5 * fact; }
	if (idy < 63) { sumdir += normalize(positionTmp[neighbourId3].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId3].xyz,  positionTmp[id].xyz) - d1) * 0.5 * fact; }
	if (idy > 0)  { sumdir += normalize(positionTmp[neighbourId4].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId4].xyz,  positionTmp[id].xyz) - d1) * 0.5 * fact; }

	if (idx < 63 && idy < 63) { sumdir += normalize(positionTmp[neighbourId5].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId5].xyz,  positionTmp[id].xyz) - d2) * 0.5 * fact2; }
	if (idx > 0  && idy < 63) { sumdir += normalize(positionTmp[neighbourId6].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId6].xyz,  positionTmp[id].xyz) - d2) * 0.5 * fact2; }
	if (idx < 63 && idy > 0)  { sumdir += normalize(positionTmp[neighbourId7].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId7].xyz,  positionTmp[id].xyz) - d2) * 0.5 * fact2; }
	if (idx > 0  && idy > 0)  { sumdir += normalize(positionTmp[neighbourId8].xyz - positionTmp[id].xyz) *  (distance (positionTmp[neighbourId8].xyz,  positionTmp[id].xyz) - d2) * 0.5 * fact2; }

	positionTmp[id].xyz += sumdir;

}
*/

#include "particle.hlsli"
#include "PBD.hlsli"

Buffer<uint> controlParticleCounter;
RWStructuredBuffer<float4> newPos;

[numthreads(1, 1, 1)]
void csPBDDistance(uint3 DTid : SV_GroupID) {
	const unsigned int xid = DTid.x;
	const unsigned int yid = DTid.y;
	const unsigned int zid = DTid.z;
	const unsigned int tid = xid + yid * gridSize + zid * gridSize * gridSize;
	const unsigned int gridEnd = gridSize - 1;

	const float fact = 0.5;
	const float fact2 = 0.2;
	const float fact3 = 0.1;

	const float defaultDist = 0.01;
	const float defaultDist2 = defaultDist * sqrt(2.0);
	const float defaultDist3 = defaultDist * sqrt(3.0);

	if (tid < controlParticleCounter[0]) {
		float3 sumdir = float3(0.0, 0.0, 0.0);

		// Sides
		
		if (xid > 0) {
			uint neighbourId = tid - 1;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist) * 0.5 * fact;
		}
		if (xid < gridEnd) {
			uint neighbourId = tid + 1;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist) * 0.5 * fact;
		}		
		if (yid > 0) {
			uint neighbourId = tid - gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist) * 0.5 * fact;
		}
		if (yid < gridEnd) {
			uint neighbourId = tid + gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist) * 0.5 * fact;
		}		
		if (zid > 0) {
			uint neighbourId = tid - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist) * 0.5 * fact;
		}
		if (zid < gridEnd) {
			uint neighbourId = tid + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist) * 0.5 * fact;
		}
		
		//Edges	
		if (xid > 0 && yid > 0) {
			uint neighbourId = tid - 1 - gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (xid < gridEnd && yid > 0) {
			uint neighbourId = tid + 1 - gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (xid > 0 && yid < gridEnd) {
			uint neighbourId = tid - 1 + gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (xid < gridEnd && yid < gridEnd) {
			uint neighbourId = tid + 1 + gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		
		
		if (xid > 0 && zid > 0) {
			uint neighbourId = tid - 1 - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (xid < gridEnd && zid > 0) {
			uint neighbourId = tid + 1 - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (xid > 0 && zid < gridEnd) {
			uint neighbourId = tid - 1 + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (xid < gridEnd && zid < gridEnd) {
			uint neighbourId = tid + 1 + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		
		
		if (yid > 0 && zid > 0) {
			uint neighbourId = tid - gridSize - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (yid < gridEnd && zid > 0) {
			uint neighbourId = tid + gridSize - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (yid > 0 && zid < gridEnd) {
			uint neighbourId = tid - gridSize + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		if (yid < gridEnd && zid < gridEnd) {
			uint neighbourId = tid + gridSize + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist2) * 0.5 * fact2;
		}
		
		
		//Corners
		if (xid > 0 && yid > 0 && zid > 0) {
			uint neighbourId = tid - 1 - gridSize - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}
		if (xid < gridEnd && yid > 0 && zid > 0) {
			uint neighbourId = tid + 1 - gridSize - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}
		if (xid > 0 && yid < gridEnd && zid > 0) {
			uint neighbourId = tid - 1 + gridSize - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}
		if (xid < gridEnd && yid < gridEnd && zid > 0) {
			uint neighbourId = tid + 1 + gridSize - gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}		
		if (xid > 0 && yid > 0 && zid < gridEnd) {
			uint neighbourId = tid - 1 - gridSize + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}
		if (xid < gridEnd && yid > 0 && zid < gridEnd) {
			uint neighbourId = tid + 1 - gridSize + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}
		if (xid > 0 && yid < gridEnd && zid < gridEnd) {
			uint neighbourId = tid - 1 + gridSize + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}
		if (xid < gridEnd && yid < gridEnd && zid < gridEnd) {
			uint neighbourId = tid + 1 + gridSize + gridSize * gridSize;
			sumdir += normalize(newPos[neighbourId].xyz - newPos[tid].xyz) *  (distance(newPos[neighbourId].xyz, newPos[tid].xyz) - defaultDist3) * 0.5 * fact3;
		}
		
		
		newPos[tid].xyz += sumdir;

	}

}