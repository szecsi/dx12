

RWByteAddressBuffer clistBegin;
RWByteAddressBuffer clistLength;
RWByteAddressBuffer clistCellCount;
RWByteAddressBuffer hlist;

//uint tid : SV_GroupIndex, uint3 groupIdx : SV_GroupID
[numthreads(1, 1, 1)]
void csCListSortOdd(uint3 DTid : SV_GroupID)
{
	unsigned int tid = DTid.x;
	unsigned int firstAddress = (tid * 2 + 1) * 4;
	unsigned int secondAddress = firstAddress + 4;
	if (secondAddress < clistCellCount.Load(0)*4)
	{
		uint firstValue = hlist.Load(firstAddress);
		uint secondValue = hlist.Load(secondAddress);
		if (firstValue > secondValue)
		{
			hlist.Store(firstAddress, secondValue);
			hlist.Store(secondAddress, firstValue);

			firstValue = clistBegin.Load(firstAddress);
			secondValue = clistBegin.Load(secondAddress);
			clistBegin.Store(firstAddress, secondValue);
			clistBegin.Store(secondAddress, firstValue);

			firstValue = clistLength.Load(firstAddress);
			secondValue = clistLength.Load(secondAddress);
			clistLength.Store(firstAddress, secondValue);
			clistLength.Store(secondAddress, firstValue);
		}
	}

}


