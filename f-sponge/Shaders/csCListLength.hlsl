


RWByteAddressBuffer clistBegin;
RWByteAddressBuffer clistLength;
RWByteAddressBuffer clistCellCount;

[numthreads(1, 1, 1)]
void csCListLength(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;
	if (tid < clistCellCount.Load(0))
	{
		clistLength.Store(tid * 4, clistBegin.Load((tid + 1) * 4) - clistBegin.Load(tid * 4));
	}	
}


