#define SortSig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1), UAV(u1, numDescriptors=1), UAV(u2, numDescriptors=1), UAV(u3, numDescriptors=1), UAV(u4, numDescriptors=1))" 

// uav offset @mortons or @ (#0 or #4)
RWByteAddressBuffer input : register(u0);
RWByteAddressBuffer inputIndices : register(u1);
// u2 would be the bucket count, but merge is not using it
RWByteAddressBuffer outputIndices : register(u3);
RWByteAddressBuffer output : register(u4);

#define groupSize 32

groupshared uint inpipe[32*32];
groupshared uint linpipe[32 * 32];
groupshared uint loutpipe[32];
groupshared uint outpipe[32];
groupshared uint inprog[32];
//groupshared uint op;
//groupshared uint outprog;

[RootSignature(SortSig)]
[numthreads(groupSize, 1, 1)]
void csMerge( uint3 tid : SV_GroupThreadID)
{
//	outprog = 0;
	inprog[tid.x] = 0;
	uint ip = 0x20;
	//while (WaveActiveAny(inprog[tid.x] < 32 * 32)) {
//	while (outprog < 32 /* *32 *32 */) {
	for (uint iLine = 0; iLine < 32 * 32; iLine++) {
		for (uint iOp = 0; iOp < 32; iOp++) {
			uint emptyPipes = WaveActiveBallot(ip == 0x20 && inprog[tid.x] < 32 * 32).x;
			while (emptyPipes) {
				uint emptyPipeIndex = firstbitlow(emptyPipes);
				uint snoopy = (emptyPipeIndex * 32 * 32 + inprog[emptyPipeIndex] + tid.x) << 2;
				inpipe[emptyPipeIndex * 32 + tid.x] = input.Load(snoopy);
				linpipe[emptyPipeIndex * 32 + tid.x] = inputIndices.Load(snoopy);
				if (tid.x == emptyPipeIndex) {
					inprog[emptyPipeIndex] += 32;
					ip = 0;
				}
//				AllMemoryBarrierWithGroupSync();
				emptyPipes = WaveActiveBallot(ip == 0x20 && inprog[tid.x] < 32 * 32).x;
			}
//			AllMemoryBarrierWithGroupSync();
			uint candidate = inpipe[ip | (tid.x << 5)];
			if (ip > 0x1f && inprog[tid.x] == 32 * 32) {
				candidate = 0xffffffff;
			}
//			GroupMemoryBarrierWithGroupSync();
			uint winner = WaveActiveMin(candidate);
//			GroupMemoryBarrierWithGroupSync();
			if (candidate == winner) {
				if (WaveIsFirstLane()) {
					outpipe[iOp] = winner;
					//				GroupMemoryBarrierWithGroupSync();
					loutpipe[iOp] = linpipe[ip | (tid.x << 5)];
									//good di			loutpipe[op] = ip | (tid.x << 5);
													//op++;
					ip++;
				}
			}
			//AllMemoryBarrierWithGroupSync();
			//if (WaveIsFirstLane()) {
			//	op++;
			//}
			//GroupMemoryBarrierWithGroupSync();
			//}
			//AllMemoryBarrierWithGroupSync();
		}
		output.Store((iLine * 32 + tid.x) << 2, outpipe[tid.x]);
		outputIndices.Store((iLine * 32 + tid.x) << 2, loutpipe[tid.x]);
		//AllMemoryBarrierWithGroupSync();
		//outputIndices.Store((outprog + tid.x) << 2, loutpipe[tid.x]);
	}

//	output.Store(tid.x, outpipe[tid.x]*/);
}