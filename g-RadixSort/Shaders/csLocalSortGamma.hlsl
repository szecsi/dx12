#define SortSig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1), UAV(u1, numDescriptors=1), UAV(u2, numDescriptors=1), UAV(u3, numDescriptors=1), UAV(u4, numDescriptors=1))" 

//SRV(t0, numDescriptors=1), 
RWByteAddressBuffer keys : register(u0);
RWByteAddressBuffer perPageBucketOffsets : register(u1);
RWByteAddressBuffer indices20KeyBits12in : register(u2);
RWByteAddressBuffer indices20KeyBits12out : register(u3);
RWByteAddressBuffer dontcare_globalBucketOffsets : register(u4);
uint maskOffset : register(b0);

#define rowSize 32
#define nRowsPerPage 32
#define nPagesPerChunk 32
#define nChunks 32
#define groupDivisor 4
#define nBuckets 16

groupshared uint s[rowSize * nRowsPerPage]; // sort step buffer, then sorted rows
groupshared uint sat[nBuckets * nRowsPerPage];

[RootSignature(SortSig)]
[numthreads(rowSize, nRowsPerPage / groupDivisor, 1)]
void csLocalSortGamma(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    uint localasses[groupDivisor];

    for (int did = 0; did < groupDivisor; did++)
    {
        uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
        uint flatid = rowst | tid.x;

        uint initialElementIndex = flatid + gid.x * rowSize * nRowsPerPage;
        uint ik = indices20KeyBits12in.Load(initialElementIndex << 2);
        uint key = keys.Load(((ik & 0xfffff000) >> 12) << 2);
        localasses[did] = (ik & 0xfffff000) | ((key >> 16) & 0xf);
    }
	//scan on bit i
    for (uint i = 0; i < 4; i++)
    {
        for (int did = 0; did < groupDivisor; did++)
        {
            uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
            uint flatid = rowst | tid.x;

            bool pred = (localasses[did] >> i) & 0x1;
            uint prefixBits = WavePrefixCountBits(pred);
            uint allBits = WaveActiveCountBits(pred);
            if (pred)
            {
                s[rowst | (rowSize - (allBits - prefixBits))] = localasses[did];
            }
            else
            {
                s[flatid - prefixBits] = localasses[did];
            }
            GroupMemoryBarrierWithGroupSync();
            localasses[did] = s[flatid];
        }
    }
    GroupMemoryBarrierWithGroupSync();
    

    for (int did = 0; did < groupDivisor; did++)
    {
        uint bucketId = localasses[did] & 0xf; //TODO 0x1f here caused an error????
        uint sic[3];
        sic[0] = WaveActiveSum((bucketId < 5) ? 0x01041041 << (bucketId * 6) : 0x0);
        sic[1] = WaveActiveSum((bucketId < 10) ? (bucketId >= 5) ? 0x01041041 << ((bucketId - 5) * 6) : 0x01041041 : 0x0);
        sic[2] = WaveActiveSum((bucketId >= 10) ? 0x01041041 << ((bucketId - 10) * 6) : 0x01041041);

        if (tid.x < 16)
        {
            sat[tid.x + (tid.y + did * nRowsPerPage / groupDivisor) * nBuckets] = (tid.x < nBuckets - 1) ?
				(sic[tid.x / 5] >> ((tid.x % 5) * 6)) & 0x3f
				: 32;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    for (int did = 0; did < groupDivisor / 2; did++)
    { // tid.y < 16 for 4 bit keys
        uint bucketId = (tid.y + did * 32 / groupDivisor);
        uint crossid = (tid.x * nBuckets) + bucketId;

        uint perRowBucketCount = sat[crossid];
        sat[crossid] = WavePrefixSum(perRowBucketCount) + perRowBucketCount;
        if (tid.x == 31)
        {
            perPageBucketOffsets.Store(((bucketId * nPagesPerChunk * nChunks) | gid.x) << 2,
					sat[crossid]
				);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    for (int did = 0; did < groupDivisor; did++)
    {
        uint rid = (tid.y + did * nRowsPerPage / groupDivisor);
        uint bucketId = localasses[did] & 0xf;
        uint flatid = rid << 5 | tid.x;

        uint target =
			tid.x 
			+ (bucketId ? sat[(bucketId - 1) + (nRowsPerPage - 1) * nBuckets] : 0)
			+ (rid ? sat[bucketId + (rid - 1) * nBuckets] : 0)
			- (bucketId ? sat[(bucketId - 1) + rid * nBuckets] : 0);

        indices20KeyBits12out.Store((target + rowSize * nRowsPerPage * gid.x) << 2, localasses[did]);
    }
}