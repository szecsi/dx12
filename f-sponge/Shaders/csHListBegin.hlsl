

RWByteAddressBuffer clistCellCount;
RWByteAddressBuffer hlist;
RWByteAddressBuffer hlistBegin;
RWByteAddressBuffer hlistLegth;


//uint tid : SV_GroupIndex, uint3 groupIdx : SV_GroupID
[numthreads(1, 1, 1)]
void csHListBegin(uint3 DTid : SV_GroupID)
{	
	unsigned int tid = DTid.x;
	uint cellCount = clistCellCount.Load(0);
	if (tid < cellCount)
	{
		//if (tid == 0)
		//{
		//	
		//}
		//else
		//if (tid != 0)
		if (tid != cellCount - 1)
		{
			uint address = tid * 4;			
			uint hash = hlist.Load(address);
			uint nextHash = hlist.Load(address + 4);
			if (hash != nextHash)
			{
				hlistBegin.Store(nextHash * 4, tid + 1);
				hlistLegth.Store(hash * 4, tid + 1);
			}
		}
		else
		{
			uint address = tid * 4;
			uint hash = hlist.Load(address);
			hlistLegth.Store(hash * 4, tid + 1);

		}
	}

}


