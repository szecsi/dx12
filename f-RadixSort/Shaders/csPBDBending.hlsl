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

const float fact = 0.01;
const float fact2 = 0.003;

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

	//A + dot(AP,AB) / dot(AB,AB) * AB
	
	if (idx < 63 && idx > 0)
	{
		vec3 AP = positionTmp[id].xyz - positionTmp[neighbourId1].xyz;
		vec3 AB = positionTmp[neighbourId2].xyz - positionTmp[neighbourId1].xyz;
		vec3 projected = positionTmp[neighbourId1].xyz + dot(AP, AB) /  dot(AB, AB) * AB;
		sumdir += (projected - positionTmp[id].xyz) * fact;
	}
	if (idy < 63 && idy > 0)
	{
		vec3 AP = positionTmp[id].xyz - positionTmp[neighbourId3].xyz;
		vec3 AB = positionTmp[neighbourId4].xyz - positionTmp[neighbourId3].xyz;
		vec3 projected = positionTmp[neighbourId3].xyz + dot(AP, AB) /  dot(AB, AB) * AB;
		sumdir += (projected - positionTmp[id].xyz) * fact;
	}
	if (idx < 63 && idx > 0 && idy < 63 && idy > 0)
	{
		vec3 AP = positionTmp[id].xyz - positionTmp[neighbourId5].xyz;
		vec3 AB = positionTmp[neighbourId8].xyz - positionTmp[neighbourId5].xyz;
		vec3 projected = positionTmp[neighbourId5].xyz + dot(AP, AB) /  dot(AB, AB) * AB;
		sumdir += (projected - positionTmp[id].xyz) * fact2;
	}
	if (idx < 63 && idx > 0 && idy < 63 && idy > 0)
	{
		vec3 AP = positionTmp[id].xyz - positionTmp[neighbourId6].xyz;
		vec3 AB = positionTmp[neighbourId7].xyz - positionTmp[neighbourId6].xyz;
		vec3 projected = positionTmp[neighbourId6].xyz + dot(AP, AB) /  dot(AB, AB) * AB;
		sumdir += (projected - positionTmp[id].xyz) * fact2;
	}
	
	positionTmp[id].xyz += sumdir;
}
*/
[numthreads(1, 1, 1)]
void csPBDBending() {

}