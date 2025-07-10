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

groupshared uint s[rowSize * nRowsPerPage]; // sort step buffer, then sorted rows
groupshared uint d[rowSize * nRowsPerPage]; // sort step buffer, then bucket counts for sorted rows
//groupshared uint ls[rowSize * nRowsPerPage]; // lookup
//groupshared uint ld[rowSize * nRowsPerPage]; // lookup

uint mortonMask(uint a) {
	return
		(a >> (maskOffsets & 0xff)) & 0x1 |
		(a >> ((maskOffsets & 0xff00) >> 8) << 1) & 0x2 |
		(a >> ((maskOffsets & 0xff0000) >> 16) << 2) & 0x4 |
		(a >> ((maskOffsets & 0xff000000) >> 24) << 3) & 0x8;
}

[RootSignature(SortSig)]
[numthreads(rowSize, nRowsPerPage, 1)]
void csLocalSort( uint3 tid : SV_GroupThreadID , uint3 gid : SV_GroupID )
{
	uint rowst = tid.y << 5;
	uint flatid = rowst | tid.x;
	uint initialElementIndex = flatid + gid.x * rowSize * nRowsPerPage;
	uint key = input.Load(initialElementIndex << 2 );
	uint locals = initialElementIndex << 5 | mortonMask(key);
	//scan on bit i
	for (uint i = 0; i < 4; i++) {
		bool pred = (locals >> i) & 0x1;
		uint prefixBits = WavePrefixCountBits(pred);
		uint allBits = WaveActiveCountBits(pred);
		if (pred) {
			s[rowst | (rowSize - (allBits - prefixBits))] = locals;
		}
		else {
			s[flatid - prefixBits] = locals;
		}
		GroupMemoryBarrierWithGroupSync();
		locals = s[flatid];
	}
	//compute step

	uint bucketId = locals & 0x1f;
	d[flatid] = 0; // count goes here
	uint bucketIdNeighbor = s[flatid + 1] & 0x1f;
	uint step = (tid.x == 31)?1:(bucketIdNeighbor - bucketId);
	uint stepMask =  WaveActiveBallot(step).x;
	if (stepMask & (0x1 << tid.x)) {
		d[bucketId + rowst] = 32 - firstbithigh(((stepMask << 1) | 0x1) << (31-tid.x));
	}

	GroupMemoryBarrierWithGroupSync();

	// d has count now (every second 16-segment is 0)
	// count': we scan the count matrix 32-length row by 32-length row, adding previous row sum sum
	uint crossid = (tid.x << 5) | tid.y;

	if (tid.y < 16) {
		uint perRowBucketCount = d[crossid];
		d[16 + crossid] = WavePrefixSum(perRowBucketCount) + perRowBucketCount;
	}

	GroupMemoryBarrierWithGroupSync();
	if (tid.y == 1 && tid.x < 16) {
		uint perPageBucketCount = d[(32 * 31 + 16) + tid.x];// +d[32 * 31 + tid.x];
		uint perPageBucketOffset = WavePrefixSum(perPageBucketCount);
		perPageBucketCounts.Store((tid.x | (gid.x << 4)) << 2, 
			perPageBucketOffset + perPageBucketCount
			);
		d[tid.x + 16] = perPageBucketOffset;
	}
	// write these out to resource mem, per 1024-page

	GroupMemoryBarrierWithGroupSync();
	if (tid.x < 16) {
		// we do not add perPageBucketOffset in the first row as it is already there
		d[16 + flatid] += (tid.y?d[16 + tid.x] - d[flatid]:0) - WavePrefixSum(d[flatid]);
	}

	GroupMemoryBarrierWithGroupSync();
	uint target = d[16 + bucketId + rowst] + tid.x;

	uint pin = inputIndices.Load( (locals >> 5) /*pin only*/ << 2);
	key = input.Load((locals >> 5) /*pin only*/ << 2);

	output.Store((target + rowSize * nRowsPerPage * gid.x) << 2 , key);
	outputIndices.Store((target + rowSize * nRowsPerPage * gid.x) << 2, pin);
//	input.Store(flatid << 2, d[flatid]);
/*
	DeviceMemoryBarrierWithGroupSync();
	if (gid.x == 0) {
		if (pid.x < 16) { // pull in counts
			d[flatid] = bucketCounts.Load(tid.x | rowst);
		}
		GroupMemoryBarrierWithGroupSync();
		if (tid.y < 16) { // compute global offsets
			d[16 + crossid] = WavePrefixSum(d[crossid]);
		}
		GroupMemoryBarrierWithGroupSync();
		DeviceMemoryBarrierWithGroupSync();

		if (tid.y == 1 && tid.x < 16) {
			uint globalBucketCount = d[(32 * 31 + 16) + tid.x] + d[32 * 31 + tid.x];
			bucketCounts.Store((tid.x | gid.x << 4) << 2, globalBucketCount);
			d[tid.x + 16] = WavePrefixSum(globalBucketCount);
		}
	}
	DeviceMemoryBarrierWithGroupSync();
	//fetch bucketoffsets


	uint gtarget = d[bucketid + (tid.y << 5)] + target ;
	input.Store(gtarget << 2, s[flatid]);
*/
	//single group: columnwise prefix sum on per-page count matrix -> per-all bucket offset matrix
	//other group: rowwise prefix sum on per-page count matrix -> per-page offset matrix
	//all threads: compute target
	//scatter to target


//	//idea: put step into waveballot, shift by lane index, getfirstbit -> obtain next non-empty bucket start
//	if (step)
//		bucketOffsets.Store(s[tid.x] *4, tid.x+1);
}
// next pass:
// we have the offset matrix 32x16
// with substractions we get the count matrix 32x16
// write out count'[0] to meta-offset matrix
// count'': we subtract the offset matrix from the count' matrix
// for i in 0..31, j in 0..31 scatter to: count''[i][bucketOf(locally-sorted[i*32+j])] + j
// UNOPTIMZED would be: scatter destination: scanned-count[i][bucketOf(locally-sorted[i])] + i - offset[i][bucketOf(locally-sorted[i])]

//LAST pass: j in 0..1023
// offsets, counts can be as high as 1024