#define SortSig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1), UAV(u1, numDescriptors=1), UAV(u2, numDescriptors=1), UAV(u3, numDescriptors=1), UAV(u4, numDescriptors=1))" 

// uav offset @mortons or @ (#0 or #4)
RWByteAddressBuffer input : register(u0);
RWByteAddressBuffer inputIndices : register(u1);
RWByteAddressBuffer perPageBucketCounts : register(u2);
RWByteAddressBuffer outputIndices : register(u3);
RWByteAddressBuffer output : register(u4);
uint maskOffsets : register(b0);

#define groupSize 32
#define nBuckets 16

groupshared uint bucketSat[nBuckets * 32];

uint mortonMask(uint a) {
	return
		(a >> (maskOffsets & 0xff)) & 0x1 |
		(a >> ((maskOffsets & 0xff00) >> 8) << 1) & 0x2 |
		(a >> ((maskOffsets & 0xff0000) >> 16) << 2) & 0x4 |
		(a >> ((maskOffsets & 0xff000000) >> 24) << 3) & 0x8;
}

[RootSignature(SortSig)]
[numthreads(groupSize, groupSize, 1)]
void csPack( uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
	uint onpageindex = tid.x | (tid.y << 5);
	uint globalid = onpageindex | (gid.x << 10);

	uint value = input.Load(globalid << 2);
	uint lvalue = inputIndices.Load(globalid << 2);
	uint bucketId = mortonMask(value);

	if (tid.y < 16) {
		uint bucketCount = perPageBucketCounts.Load(((tid.x << 4) | tid.y) << 2);
		bucketSat[tid.y | (tid.x << 4)] = WavePrefixSum(bucketCount) + bucketCount;
	}
	uint mybucketoffsetonpage = 
		bucketId  ? 
		perPageBucketCounts.Load( ( (bucketId - 1)  | ( gid.x << 4) ) << 2)
		:
		0;

	GroupMemoryBarrierWithGroupSync();

//	output.Store(onpageindex - mybucketoffsetperpage + wheredoesmyperpagebucketstartinglobal, );
	uint target = (
		bucketId ? 
			onpageindex - mybucketoffsetonpage
			+ bucketSat[bucketId - 1 + 31*16]
			+ (gid.x?bucketSat[bucketId + (gid.x-1) * 16]:0)
			- (gid.x?bucketSat[bucketId - 1 + (gid.x-1) * 16]:0)
		:
			(
			onpageindex
		    + (gid.x?bucketSat[bucketId + (gid.x-1) * 16]:0)
			)
		) << 2;
	output.Store(target, value);
	outputIndices.Store(target, lvalue);


}