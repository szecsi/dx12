
#include "particle.hlsli"
#include "PBD.hlsli"
#include "PBDTetrahedron.hlsli"

[numthreads(1, 1, 1)]
void csPBDTetrahedron1(uint3 DTid : SV_GroupID) {
	uint x = DTid.x;
	uint y = DTid.y;
	uint z = DTid.z;

	if (x < gridSize - 1 && y > 0 && z > 0 && ((x + y) % 2 == 0)) {
		executeConstraintsOnVertices(uint4(changeToArrayIndex(x,y,z,0), changeToArrayIndex(x + 1,y,z,0), changeToArrayIndex(x,y,z - 1,1), changeToArrayIndex(x,y - 1,z - 1,1) ));
	}
}