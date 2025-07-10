#include "proximity.hlsli"

#define ProxySig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=2))" 

// sortedMortons => starterCounts
// uav offset @sortedMortons or @cbegin (#3 or #6)
RWByteAddressBuffer sorted : register(u0);
RWByteAddressBuffer starterCounts : register(u1);

groupshared uint perRowStarterCount[nRowsPerPage];
groupshared uint perRowLeadingNonstarterCount[nRowsPerPage];

[RootSignature(ProxySig)]
[numthreads(rowSize, nRowsPerPage, 1)]
void csStarterCount(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
	uint rowst = tid.y << 5;
	uint flatid = rowst | tid.x;
	uint initialElementIndex = flatid + gid.x * rowSize * nRowsPerPage;

	// TODO: naming: Morton may be hash when sorting hlist
	uint myMorton = sorted.Load(initialElementIndex << 2);
	uint prevMorton = initialElementIndex? sorted.Load((initialElementIndex-1) << 2):0xffffffff ;
	bool meNonstarter = (myMorton == prevMorton);
	uint nonStartersUpToMe = WavePrefixCountBits(meNonstarter) + (meNonstarter?1:0);
	if (tid.x == 31) {
		perRowStarterCount[tid.y] = 32 - nonStartersUpToMe;
	}
	perRowLeadingNonstarterCount[tid.y] = 0;
	if (nonStartersUpToMe == (tid.x + 1)) { //if all are starters, this never happens
		perRowLeadingNonstarterCount[tid.y] = WaveActiveMax(tid.x+1);
	}

	GroupMemoryBarrierWithGroupSync();

	if (tid.y == 0) {
		uint nonstarterCount = perRowLeadingNonstarterCount[tid.x];
		uint hasStarterMask = WaveActiveBallot(nonstarterCount != 32);
		//uint precedingRowsWithNoStarterCount = (tid.x)?firstbithigh(hasStarterMask):0;
		//uint hasStarterMask = WaveActiveBallot(nonstarterCount != 32).x;
			
		uint perPageStarterCount = WavePrefixSum(perRowStarterCount[tid.x]) + perRowStarterCount[31];
		if (tid.x == 31) {
			uint firstNotEmpty = firstbitlow(hasStarterMask); // no starter on entire page would be weird
			if (firstNotEmpty == 0xffffffff)
				firstNotEmpty = 0;
			uint leadingNonStarterCount = firstNotEmpty * 32 + perRowLeadingNonstarterCount[firstNotEmpty];
			starterCounts.Store(gid.x<<2, (leadingNonStarterCount << 16) | perPageStarterCount);
		}
	}
}