

#include "particle.hlsli"
#include "hash.hlsli"

StructuredBuffer<Particle> particles;
RWByteAddressBuffer clistBegin;
RWByteAddressBuffer clistCellCount;
RWByteAddressBuffer hlist;

//uint tid : SV_GroupIndex, uint3 groupIdx : SV_GroupID
[numthreads(1, 1, 1)]
void csHListInit(uint3 DTid : SV_GroupID)
{
	//unsigned int tid = DTid.x;
	//hlist.Store(tid*4, 7);
	
	unsigned int tid = DTid.x;
	if (tid < clistCellCount.Load(0))
	{
		uint address = tid * 4;
		uint particleIdx = clistBegin.Load(address);
		uint hashValue = particles[particleIdx].zindex % hashCount;
		hlist.Store(address, hashValue);
	}

}


