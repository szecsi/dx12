#define SortSig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1), UAV(u1, numDescriptors=1), UAV(u2, numDescriptors=1), UAV(u3, numDescriptors=1), UAV(u4, numDescriptors=1))" 

//SRV(t0, numDescriptors=1), 
RWByteAddressBuffer output : register(u0);
RWByteAddressBuffer outputIndices : register(u1);
RWByteAddressBuffer perPageBucketCounts : register(u2);
RWByteAddressBuffer inputIndices : register(u3);
RWByteAddressBuffer input : register(u4);
uint maskOffsets : register(b0);

#define rowSize 32
#define nRowsPerPage 32
#define nPagesPerChunk 32
#define nChunks 32
#define groupDivisor 4
#define nBuckets 16

groupshared uint s[rowSize * nRowsPerPage]; // sort step buffer, then sorted rows
//groupshared uint d[rowSize * nRowsPerPage]; // sort step buffer, then bucket counts for sorted rows
//groupshared uint perPageBucketOffsets[16];
groupshared uint sat[nBuckets * nRowsPerPage];

uint mortonMask(uint a)
{
    return
		(a >> (maskOffsets & 0xff)) & 0x1 |
		(a >> ((maskOffsets & 0xff00) >> 8) << 1) & 0x2 |
		(a >> ((maskOffsets & 0xff0000) >> 16) << 2) & 0x4 |
		(a >> ((maskOffsets & 0xff000000) >> 24) << 3) & 0x8;
}

[RootSignature(SortSig)]
[numthreads(rowSize, nRowsPerPage / groupDivisor, 1)]
void csLocalSort(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    uint localasses[groupDivisor];

    for (int did = 0; did < groupDivisor; did++)
    {
        uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
        uint flatid = rowst | tid.x;

        uint initialElementIndex = flatid + gid.x * rowSize * nRowsPerPage;
        uint key = input.Load(initialElementIndex << 2);
        localasses[did] = (initialElementIndex << 5) | mortonMask(key);
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

		//compute step
/*	for (int did = 0; did < groupDivisor; did++) {
		uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
		uint flatid = rowst | tid.x;

		uint bucketId = localasses[did] & 0xf;  //TODO 0x1f here caused an error????
		uint bucketIdNeighbor = s[flatid + 1] & 0xf; //TODO 0x1f here caused an error????
		uint step = (tid.x == 31) ? 1 : (bucketIdNeighbor - bucketId); //TODO the test here should not matter as we shift that bit out later
		uint stepMask = WaveActiveBallot(step).x;
		if (stepMask & (0x1 << tid.x)) {
			d[bucketId + rowst] = 32 - firstbithigh(((stepMask << 1) | 0x1) << (31 - tid.x));
//			sat[bucketId + rowst] = 32 - firstbithigh(((stepMask << 1) | 0x1) << (31 - tid.x));
		}
	}*/

    GroupMemoryBarrierWithGroupSync();

    for (int did = 0; did < groupDivisor / 2; did++)
    { // tid.y < 16 for 4 bit keys
        uint bucketId = (tid.y + did * 32 / groupDivisor);
        uint crossid = (tid.x * nBuckets) + bucketId;

//		if (tid.y < 16) {
        uint perRowBucketCount = sat[crossid];
			//d[16 + crossid] = WavePrefixSum(perRowBucketCount) + perRowBucketCount;
        sat[crossid] = WavePrefixSum(perRowBucketCount) + perRowBucketCount;
        if (tid.x == 31)
        {
            perPageBucketCounts.Store(((bucketId * nPagesPerChunk * nChunks) | gid.x) << 2,
					sat[crossid]
				);
        }

//		}
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

        uint pin = inputIndices.Load((localasses[did] >> 5) << 2);
        uint key = input.Load((localasses[did] >> 5) << 2);

        output.Store((target + rowSize * nRowsPerPage * gid.x) << 2, key);
        outputIndices.Store((target + rowSize * nRowsPerPage * gid.x) << 2, pin);
		//outputIndices.Store((flatid + rowSize * nRowsPerPage * gid.x) << 2, (flatid<32*32)?sat[flatid]:13);
    }

/*	if (tid.y == 0 && tid.x < 16) {
		uint perPageBucketCount = d[(32 * 31 + 16) + tid.x];// +d[32 * 31 + tid.x];
		uint perPageBucketOffset = WavePrefixSum(perPageBucketCount);
		perPageBucketCounts.Store(((tid.x * nPagesPerChunk * nChunks) | gid.x) << 2,
			perPageBucketOffset + perPageBucketCount
			);
		perPageBucketOffsets[tid.x] = perPageBucketOffset;
	}
	// write these out to resource mem, per 1024-page

	GroupMemoryBarrierWithGroupSync();
	for (int did = 0; did < groupDivisor; did++) {
		uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
		uint flatid = rowst | tid.x;

		if (tid.x < 16) {
			// we do not add perPageBucketOffset in the first row as it is already there
			d[16 + flatid] += perPageBucketOffsets[tid.x] - d[flatid] - WavePrefixSum(d[flatid]);
		}
	}

	GroupMemoryBarrierWithGroupSync();

	for (int did = 0; did < groupDivisor; did++) {
		uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
		uint bucketId = localasses[did] & 0xf;
		uint flatid = rowst | tid.x;

		uint target = d[16 + bucketId + rowst] + tid.x;

		uint pin = inputIndices.Load((localasses[did] >> 5) << 2);
		uint key = input.Load((localasses[did] >> 5) << 2);

		output.Store((target + rowSize * nRowsPerPage * gid.x) << 2, key);
		outputIndices.Store((target + rowSize * nRowsPerPage * gid.x) << 2, pin);
	}
	*/
}