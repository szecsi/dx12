#include "proximity.hlsli"
#include "hhash.hlsli"

#define ProxySig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=3))"

// uav offset @sortedhlist (#8)
RWByteAddressBuffer sorted : register(u0);
RWByteAddressBuffer starterCounts : register(u1);
RWByteAddressBuffer hbegin : register(u2);

groupshared uint perRowStarterCount[nRowsPerPage];
groupshared uint perRowLeadingNonstarterCount[nRowsPerPage];
groupshared uint perPageStarterCountsSummed[32];
groupshared uint perRowStarterCountsSummed[32];

[RootSignature(ProxySig)]
[numthreads(rowSize, nRowsPerPage, 1)]
void csCreateHashList(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
	if (tid.y == 0) {
		uint perPageStarterCount = starterCounts.Load(tid.x << 2) & 0xffff;
		perPageStarterCountsSummed[tid.x] = WavePrefixSum(perPageStarterCount);
	}

	uint rowst = tid.y << 5;
	uint flatid = rowst | tid.x;
	uint initialElementIndex = flatid + gid.x * rowSize * nRowsPerPage;

	uint myHash = sorted.Load(initialElementIndex << 2);
	uint prevHash = initialElementIndex ? sorted.Load((initialElementIndex - 1) << 2) : 0xffffffff;
	bool meNonstarter = (myHash == prevHash);
	uint nonStartersUpToMe = WavePrefixCountBits(meNonstarter) + (meNonstarter ? 1 : 0);
	if (tid.x == 31) {
		perRowStarterCount[tid.y] = 32 - nonStartersUpToMe;
	}
	perRowLeadingNonstarterCount[tid.y] = 0;
	if (nonStartersUpToMe == (tid.x + 1)) { //if all are starters, this never happens
		perRowLeadingNonstarterCount[tid.y] = WaveActiveMax(tid.x + 1);
	}

	GroupMemoryBarrierWithGroupSync();

	if (tid.y == 0) {
		bool rowNotFull = perRowLeadingNonstarterCount[tid.x] != 32;
		uint notFullMask = WaveActiveBallot(rowNotFull).x >> (tid.x + 1);
		if (tid.x == 31)
			notFullMask = 0;
		uint nFullRowsAfterMe = firstbitlow(notFullMask);
		if (!rowNotFull && nFullRowsAfterMe != 0xffffffff) {
			uint nextNonFullLeadingNonStarterCount = perRowLeadingNonstarterCount[tid.x + 1 + nFullRowsAfterMe];
			perRowLeadingNonstarterCount[tid.x] += nFullRowsAfterMe * 32 + nextNonFullLeadingNonStarterCount;
		}

		perRowStarterCountsSummed[tid.x] = WavePrefixSum(perRowStarterCount[tid.x]);
	}
	GroupMemoryBarrierWithGroupSync();

	uint starterMask = WaveActiveBallot(!meNonstarter).x >> (tid.x + 1);
	if (tid.x == 31)
		starterMask = 0;
	uint clength = firstbitlow(starterMask) + 1;
	if (clength == 0) { // runs over row end
		clength = 32 - tid.x;
		clength += (tid.y==31) ? starterCounts.Load((gid.x+1)<<2)>>16 : perRowLeadingNonstarterCount[tid.y + 1];
	}
	if (!meNonstarter) {
		hbegin.Store(myHash << 2, clength << 16 | initialElementIndex);
	}
}