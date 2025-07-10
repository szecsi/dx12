
#include "particle.hlsli"
#include "window.hlsli"

RWByteAddressBuffer offsetBuffer;
RWByteAddressBuffer counterBuffer;

#define groupDim_x 1
#define groupthreads 512

groupshared uint2 bucket[groupthreads];

void CSScan(uint3 DTid, uint GI, uint x, uint3 Gid)         
{
	bucket[GI].x = x;
	bucket[GI].y = 0;

	// Up sweep    
	[unroll]
	for (uint stride = 2; stride <= groupthreads; stride <<= 1)
	{
		GroupMemoryBarrierWithGroupSync();

		if ((GI & (stride - 1)) == (stride - 1))
		{
			bucket[GI].x += bucket[GI - stride / 2].x;
		}
	}

	if (GI == (groupthreads - 1))
	{
		bucket[GI].x = 0;
	}

	// Down sweep
	bool n = true;
	[unroll]
	for (stride = groupthreads / 2; stride >= 1; stride >>= 1)
	{
		GroupMemoryBarrierWithGroupSync();

		uint a = stride - 1;
		uint b = stride | a;

		if (n)        // ping-pong between passes
		{
			if ((GI & b) == b)
			{
				bucket[GI].y = bucket[GI - stride].x + bucket[GI].x;
			}
			else
				if ((GI & a) == a)
				{
					bucket[GI].y = bucket[GI + stride].x;
				}
				else
				{
					bucket[GI].y = bucket[GI].x;
				}
		}
		else
		{
			if ((GI & b) == b)
			{
				bucket[GI].x = bucket[GI - stride].y + bucket[GI].y;
			}
			else
				if ((GI & a) == a)
				{
					bucket[GI].x = bucket[GI + stride].y;
				}
				else
				{
					bucket[GI].x = bucket[GI].y;
				}
		}

		n = !n;
	}
	uint temp;
	if (Gid.x < (windowHeight*windowWidth / counterSize)){
		uint o = offsetBuffer.Load((Gid.x + counterSize + GI * counterSize) * 4);
		if (o != 0)
		{
			uint fullCount = 0;
			if (Gid.x > 0) 
			{
				fullCount = counterBuffer.Load((Gid.x-1)*4);
			}

			offsetBuffer.InterlockedExchange((Gid.x + counterSize + GI * counterSize) * 4, bucket[GI].y + x + fullCount, temp);
		}
	}
}

// scan in each bucket
[numthreads(groupthreads, 1, 1)]
void csPrefixSumV2(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint x = offsetBuffer.Load((Gid.x +  GI * counterSize) * 4);  

	CSScan(DTid, GI, x, Gid);	
}
