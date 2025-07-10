
#include "hash.hlsli"

RWByteAddressBuffer hlistBegin;
RWByteAddressBuffer hlistLength;
RWByteAddressBuffer clistCellCount;

[numthreads(1, 1, 1)]
void csHListLength(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;
	/*
	if (tid < hashCount - 1)
	{
		hlistLength.Store(tid * 4, hlistBegin.Load((tid + 1) * 4) - hlistBegin.Load(tid * 4));
	}
	else
	{
		hlistLength.Store(tid * 4, clistCellCount.Load(0) - hlistBegin.Load(tid * 4));
	}
	*/

	if (tid < hashCount - 1)
	{
		uint address = tid * 4;
		uint length = hlistLength.Load(address) - hlistBegin.Load(address);
		hlistLength.Store(address, length);
	}
}


