
#include "particle.hlsli"
#include "PBD.hlsli"
#include "PBDTetrahedron.hlsli"

[numthreads(1, 1, 1)]
void csPBDTetrahedron24(uint3 DTid : SV_GroupID) {
	uint x = DTid.x;
	uint y = DTid.y;
	uint z = DTid.z;

	executeConstraintsOnVertices(uint4( changeToArrayIndex(0,0,0,0), changeToArrayIndex(1,0,0,0), changeToArrayIndex(0,1,0,0), changeToArrayIndex(0,0,1,0) ));
}