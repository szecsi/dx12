
#include "particle.hlsli"

RWByteAddressBuffer clist;
RWByteAddressBuffer clistNonZero;
RWByteAddressBuffer clistBegin;
RWByteAddressBuffer clistCellCount;

[numthreads(1, 1, 1)]
void csCListCompact(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;

	uint address = tid * 4;
	uint originalValue = clist.Load(address);
	if (originalValue != 0)
	{
		clistBegin.Store(clistNonZero.Load(address) * 4, originalValue);
	}

	if (tid == particleCount - 1)
	{
		address += 4;
		originalValue = clist.Load(address);
		if (originalValue != 0)
		{
			uint cellCount = clistNonZero.Load(address);
			clistBegin.Store(cellCount * 4, originalValue);
			clistCellCount.Store(0, cellCount);
		}
	}
}


