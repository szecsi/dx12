#define SortSig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1), UAV(u1, numDescriptors=1), UAV(u2, numDescriptors=1), UAV(u3, numDescriptors=1), UAV(u4, numDescriptors=1))" 

RWByteAddressBuffer fragmentCounters : register(u0);
RWBuffer<uint4> fragments : register(u1);
RWBuffer<int> strokeCounts : register(u2);
RWBuffer<float4> designs : register(u3);
RWByteAddressBuffer debugBuffer : register(u4);

#define rowSize 32
#define nRowsPerPage 32
#define groupDivisor 4
#define nBuckets 16

groupshared uint s[rowSize * nRowsPerPage]; // sort step buffer, then sorted rows
groupshared uint sat[nBuckets * nRowsPerPage];
groupshared float4 overdesign[(nRowsPerPage-1)*5];

[RootSignature(SortSig)]
[numthreads(rowSize * nRowsPerPage / groupDivisor, 1, 1)]
void sortCS(uint3 ltid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    uint nValid = fragmentCounters.Load(gid.x << 2);
    uint3 tid = uint3(ltid.x % WaveGetLaneCount(), ltid.x / WaveGetLaneCount(), 1);
    
//        (tid.y & 0xfffffffe) | ((tid.x ^ (tid.x >> 1)) & 1)
  //  ;
    
    uint localasses[groupDivisor];
    uint serials[groupDivisor];    
  
    for (int did = 0; did < groupDivisor; did++)
    {
        uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
        uint flatid = rowst | tid.x;
        if (flatid < nValid) {
            uint initialElementIndex = flatid + gid.x * rowSize * nRowsPerPage;
            uint sid = fragments.Load(initialElementIndex << 4);
            localasses[did] = (flatid << 16) | (sid & 0xffff);
        } else {
            localasses[did] = 0xffffffff;
        }
    }
    GroupMemoryBarrierWithGroupSync();
    
    for (uint j = 0; j < 32; j += 4)
    {
        for (uint i = j; i < j + 4; i++)
        {
            for (int did = 0; did < groupDivisor; did++)
            {
                uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
                uint flatid = rowst | tid.x;

                bool pred = (localasses[did] >> (i & 0xf)) & 0x1;
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
            uint bucketId = (localasses[did] >> (j & 0xf)) & 0xf; //TODO 0x1f here caused an error????
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
        for (int did = 0; did < groupDivisor / 2 ; did++)
        { // tid.y < 16 for 4 bit keys
            uint bucketId = (tid.y + did * 32 / groupDivisor);
            uint crossid = (tid.x * nBuckets) + bucketId;

            //crossid = tid.x;
            //crossid = tid.x + tid.y * 32;
            
            uint perRowBucketCount = sat[crossid];
            sat[crossid] = 
                //tid.x;
                //WaveGetLaneIndex();
                //WavePrefixSum(32);
                WavePrefixSum(perRowBucketCount) + perRowBucketCount;
        }
        
        GroupMemoryBarrierWithGroupSync();

        for (int did = 0; did < groupDivisor; did++)
        {
            uint rid = (tid.y + did * nRowsPerPage / groupDivisor);
            uint bucketId = (localasses[did] >> (j & 0xf)) & 0xf;
            uint flatid = rid << 5 | tid.x;

            uint target =
			    tid.x 
			    + (bucketId ? sat[(bucketId - 1) + (nRowsPerPage - 1) * nBuckets] : 0)
			    + (rid ? sat[bucketId + (rid - 1) * nBuckets] : 0)
			    - (bucketId ? sat[(bucketId - 1) + rid * nBuckets] : 0);

            if (j == 12) {
                uint flatid = localasses[did] >> 16;
                if (flatid < nValid){
                    uint elementIndex = flatid + gid.x * rowSize * nRowsPerPage;
                    uint sid = fragments.Load(elementIndex << 4);
                    s[target] = (localasses[did] & 0xffff0000) | (sid >> 16);
                }
            } else {
                s[target] = localasses[did];
            }
        
        }
        
        GroupMemoryBarrierWithGroupSync();
            
        for (int did = 0; did < groupDivisor; did++){
            uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
            uint flatid = rowst | tid.x;
            localasses[did] = s[flatid];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    GroupMemoryBarrierWithGroupSync();

    for (int did = 0; did < groupDivisor; did++){
        uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
        uint flatid = rowst | tid.x;
            
        uint rei = localasses[did] >> 16;
        if (rei < nValid){
            uint elementIndex = rei + gid.x * rowSize * nRowsPerPage;
            uint sid = fragments.Load(elementIndex << 4);
            localasses[did] = sid;
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    // now it is sorted
        ////  verify sorting    
        //    for (int did = 0; did < groupDivisor; did++){
        //        uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
        //        uint flatid = rowst | tid.x;
        //        debugBuffer.Store( (gid.x * nRowsPerPage * rowSize + flatid) << 2, 
        //            s[flatid]);
        //    }
        // return;
    
    //let's number the strokes for each sid, we can do this by looking at the localasses values, since they are sorted by sid. We can write out the stroke counts to a RWBuffer<int> using atomic adds, and we can write out the designs to a RWBuffer<float4> using the fragment data. We just need to make sure to only write out one design per sid, which we can do by checking if the next localasses value has a different sid or if we are at the end of the group.
    GroupMemoryBarrierWithGroupSync();

    uint oversteps = WaveActiveBallot( s[(tid.x << 5)+31] != s[(tid.x << 5) + 32]).x;
    
    for (int did = 0; did < groupDivisor; did++)
    {
        uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
        uint flatid = rowst | tid.x;
        
        uint groupMask = WaveMatch(localasses[did]).x;
        uint step;
        if(tid.x == 0){
            step = 0;      
        } else {
            step = 1 - ((groupMask >> (tid.x - 1)) & 0x1u);
        }
        s[flatid] = WavePrefixSum(step) + step;
    }
    
    GroupMemoryBarrierWithGroupSync();

//    sat[tid.x] = WavePrefixSum(s[(tid.x << 5)+31] + 
//        (tid.x ? countbits(oversteps << (32-tid.x)) : 0)
//    );
//    for (int did = 0; did < groupDivisor; did++) {
//        //serials[did] = 0; //TODO
//        uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
//        uint flatid = rowst | tid.x;
//        
//        s[flatid] += sat[tid.y];
//    }
//
    GroupMemoryBarrierWithGroupSync();
    
        for (int did = 0; did < groupDivisor; did++){
            uint rowst = (tid.y + did * nRowsPerPage / groupDivisor) << 5;
            uint flatid = rowst | tid.x;
            debugBuffer.Store( (gid.x * nRowsPerPage * rowSize + flatid) << 2, 
                //nValid);
                //ltid.x);
                //WaveGetLaneIndex());
                //tid.x);
                //WavePrefixSum(1) + 1);
            //localasses[did]);
            s[flatid]);
            //sat[flatid]);
        }
    
    
    GroupMemoryBarrierWithGroupSync();
    
    // we read fragments and sum designs here
    uint4 fragment[groupDivisor];
    for (int did = 0; did < groupDivisor; did++)
    {
        fragment[did] = fragments.Load((localasses[did] >> 16) << 4);
 
        float t = fragment[did].w / 256.0 / 256.0;
        float t2 = t * t;
        float t3 = t2 * t;
        float4 tPowers = float4(1, t, t2, t3);

        float4 xtpoly = tPowers * fragment[did].y;
        float4 ytpoly = tPowers * fragment[did].z;

        float4 design0 = float4(1, t, t2, t3);
        float4 design1 = design0 * t3;
        float4 extrema;
        if (fragment[did].z < 128)
            extrema = float4(0, 0, 0, 0);
        else
            extrema = float4(1 - t, t, 0, 0);
    }
    //sum designs for identical sids, get maximum of extrema for identical sids, write out design and stroke count for each sid   
 
}