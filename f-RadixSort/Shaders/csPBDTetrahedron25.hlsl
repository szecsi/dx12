
#include "particle.hlsli"
#include "PBD.hlsli"
#include "PBDTetrahedron.hlsli"

[numthreads(1, 1, 1)]
void csPBDTetrahedron25(uint3 DTid : SV_GroupID) {
	uint x = DTid.x;
	uint y = DTid.y;
	uint z = DTid.z;

	uint maxIdx = gridSize - 1;
	executeConstraintsOnVertices(uint4(changeToArrayIndex(maxIdx, maxIdx, maxIdx, 1), changeToArrayIndex(maxIdx - 1, maxIdx, maxIdx, 1), changeToArrayIndex(maxIdx, maxIdx - 1, maxIdx, 1), changeToArrayIndex(maxIdx, maxIdx, maxIdx - 1, 1)));
}