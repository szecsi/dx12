
#include "particle.hlsli"
#include "PBDSphere.hlsli"

Buffer<uint> controlParticleCounter;
StructuredBuffer<Sphere> testMesh;

RWStructuredBuffer<float4> newPos;
RWStructuredBuffer<float4> tesMeshTrans;

[numthreads(1, 1, 1)]
void csPBDSphereCollision(uint3 DTid : SV_GroupID) {
	unsigned int tid = DTid.x;

	if (tid < controlParticleCounter[0]) {

		const float boundaryEps = 0.0001;

		float4 center = testMesh[0].pos;
		float radius = sphereRadius;

		float3 radDis = newPos[tid].xyz - center.xyz;
		float dist = length (radDis);
		radDis = normalize(radDis);
		//if (length < radius - boundaryEps) {
		if (dist < radius) {
			//newPos[tid].xyz += radDis * dist / 2.0 * 0.01;
			float3 diff = radDis * 0.001;
			newPos[tid].xyz += diff;
			tesMeshTrans[tid].xyz += -diff;
			//tesMeshTrans[tid].xyz = float3 (0.0, 0.0, 1.0);
		}
	}
}