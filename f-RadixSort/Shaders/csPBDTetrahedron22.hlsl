
#include "particle.hlsli"
#include "PBD.hlsli"
#include "PBDTetrahedron.hlsli"

[numthreads(1, 1, 1)]
void csPBDTetrahedron22(uint3 DTid : SV_GroupID) {
	uint x = DTid.x;
	uint y = DTid.y;
	uint z = DTid.z;

	if (z < gridSize - 1 && y > 0 && ((y + z) % 2 == 1)) {
		executeConstraintsOnVertices(uint4(changeToArrayIndex(x,y,z,0), changeToArrayIndex(x,y,z + 1,0), changeToArrayIndex(x,y,z,1), changeToArrayIndex(x,y - 1,z,1) ));
	}
}