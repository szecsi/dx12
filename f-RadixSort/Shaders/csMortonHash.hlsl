
#include "particle.hlsli"
#include "mortonHash.hlsli"

StructuredBuffer<float4> positions;
RWByteAddressBuffer hashes;

[numthreads(128, 1, 1)]
void csMortonHash(uint3 DTid : SV_DispatchThreadID)
{
	unsigned int tid = DTid.x;
//	hashes.Store( tid << 2, mortonHash(positions[tid].xyz) );
	hashes.Store(tid << 2, packedIndex(positions[tid].xyz));
}


