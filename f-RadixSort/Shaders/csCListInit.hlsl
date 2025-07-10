

#include "particle.hlsli"

StructuredBuffer<uint> hashes;
RWByteAddressBuffer clist;
RWByteAddressBuffer clistNonZero;

//uint tid : SV_GroupIndex, uint3 groupIdx : SV_GroupID
[numthreads(1, 1, 1)]
void csCListInit(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;

	uint value = 0;
	uint address = tid * 4;
	if (tid > 0 && hashes[tid - 1] != hashes[tid])
	{
		value = tid;
	}
	clist.Store(address, value);
	
	uint nonzero = 0;
	if (value > 0)
	{
		nonzero = 1;
	}
	clistNonZero.Store(address, nonzero);

	if (tid == particleCount-1)
	{
		address += 4;
		clist.Store(address, particleCount);
		clistNonZero.Store(address, 1);
	}
}


