#define SortSig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1), UAV(u1, numDescriptors=1), UAV(u2, numDescriptors=1), UAV(u3, numDescriptors=1), UAV(u4, numDescriptors=1))" 

//SRV(t0, numDescriptors=1), 
RWByteAddressBuffer keys : register(u0);
RWByteAddressBuffer perPageBucketOffsets : register(u1);
RWByteAddressBuffer indices20KeyBits12out : register(u2);
RWByteAddressBuffer indices20KeyBits12in : register(u3);
RWByteAddressBuffer globalBucketOffsets : register(u4);
uint maskOffset : register(b0);

#define groupSize 32
#define nBuckets 16
#define nPagesPerChunk 32
#define nChunks 32
#define rowSize 32
#define nRowsPerPage 32


groupshared uint bucketSat[nBuckets * 32];

[RootSignature(SortSig)]
[numthreads(groupSize, groupSize, 1)]
void csPackGamma(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    uint onpageindex = tid.x | (tid.y << 5);
    uint globalid = onpageindex | (gid.x << 10);

    uint value = indices20KeyBits12in.Load(globalid << 2);
    uint bucketId = value & 0xf;
    uint key = keys.Load(((value >> 12) & 0xfffff) << 2);
    value = (value & 0xfffff000) | ((key >> 20) & 0xfff);
    
    GroupMemoryBarrierWithGroupSync();

    uint pageIndex = gid.x;
    uint nPages = nPagesPerChunk * nChunks;

    uint globalBucketStart = bucketId ? globalBucketOffsets.Load(((bucketId - 1) * nPagesPerChunk * nChunks + (nPages - 1)) << 2) : 0;
    uint bucketInPageStart = bucketId ? globalBucketOffsets.Load(((bucketId - 1) * nPagesPerChunk * nChunks + pageIndex) << 2) :
    0;
    uint pageInBucketStart = pageIndex ? globalBucketOffsets.Load((bucketId * nPagesPerChunk * nChunks + (pageIndex - 1)) << 2) : 0;
	
    uint target =
			onpageindex
			+ globalBucketStart
			+ pageInBucketStart
			- bucketInPageStart;

    indices20KeyBits12out.Store(target << 2, value);
}