
#include "particle.hlsli"
#include "PBDSphere.hlsli"

Buffer<uint> controlParticleCounter;
RWStructuredBuffer<float4> tesMeshTrans;

[numthreads(1, 1, 1)]
void csPBDSphereTransClear(uint3 DTid : SV_GroupID) {
	unsigned int tid = DTid.x;

	if (tid < controlParticleCounter[0]) {
		tesMeshTrans[tid] = float4 (0.0, 0.0, 0.0, 0.0);
	}
}