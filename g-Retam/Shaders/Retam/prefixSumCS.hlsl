#define PrefixSumSig "RootFlags( 0 )," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))"

RWByteAddressBuffer strokeCounts  : register(u3);
RWByteAddressBuffer strokeOffsets : register(u5);

#define TOTAL_ELEMENTS   16384
#define GROUP_THREADS    1024
#define ELEMS_PER_THREAD (TOTAL_ELEMENTS / GROUP_THREADS)

groupshared uint partialSums[GROUP_THREADS];

[RootSignature(PrefixSumSig)]
[numthreads(GROUP_THREADS, 1, 1)]
void prefixSumCS(uint3 groupThreadID : SV_GroupThreadID)
{
    uint t    = groupThreadID.x;
    uint base = t * ELEMS_PER_THREAD;

    // Phase 1: each thread sums its chunk of ELEMS_PER_THREAD elements
    uint localTotal = 0;
    [unroll]
    for (uint i = 0; i < ELEMS_PER_THREAD; i++)
        localTotal += strokeCounts.Load((base + i) * 4);
    partialSums[t] = localTotal;
    GroupMemoryBarrierWithGroupSync();

    // Phase 2: Hillis-Steele inclusive scan — partialSums[t] becomes sum[0..t]
    for (uint stride = 1; stride < GROUP_THREADS; stride <<= 1) {
        uint val = (t >= stride) ? partialSums[t - stride] : 0;
        GroupMemoryBarrierWithGroupSync();
        partialSums[t] += val;
        GroupMemoryBarrierWithGroupSync();
    }

    // Phase 3: write exclusive prefix sums — groupOffset is sum of all chunks before t
    uint offset = (t > 0) ? partialSums[t - 1] : 0;
    [unroll]
    for (uint i = 0; i < ELEMS_PER_THREAD; i++) {
        uint count = strokeCounts.Load((base + i) * 4);
        strokeOffsets.Store((base + i) * 4, offset);
        offset += count;
    }
}
