#include "particle.hlsli"

StructuredBuffer<float4> olds;
RWStructuredBuffer<float4> news;
RWByteAddressBuffer sortedPins;

//particlePerCore
[numthreads(128, 1, 1)]
void csSortParticles( uint3 DTid : SV_DispatchThreadID )
{
	uint tid = DTid.x;
	uint i = sortedPins.Load(tid << 2);

	news[tid] = olds[i];
}