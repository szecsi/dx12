
//#include "particle.hlsli"

RWByteAddressBuffer offsetBuffer;

#define groupDim_x 1
#define groupthreads 512

groupshared uint2 bucket[groupthreads];

void CSScan(uint3 DTid, uint GI, uint x)         
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
	offsetBuffer.InterlockedExchange(DTid.x * 4, bucket[GI].y + x, temp);

}

// scan in each bucket
[numthreads(groupthreads, 1, 1)]
void csPrefixSum(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint x = offsetBuffer.Load(DTid.x*4);                    // Change the type of x here if scan other types 
	CSScan(DTid, GI, x);
}
