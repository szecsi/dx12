#define SortSig "RootFlags( 0 )," \
				"RootConstants(num32BitConstants=1, b0)," \
                "DescriptorTable(UAV(u0, numDescriptors=1), UAV(u1, numDescriptors=1))" 

RWByteAddressBuffer input : register(u0);
RWByteAddressBuffer output : register(u1);

#define groupSize 32

groupshared uint inpipe[32*32];
groupshared uint inprog[32];
groupshared uint outpipe[32];
groupshared uint op;
groupshared uint outprog;

[RootSignature(SortSig)]
[numthreads(groupSize, 1, 1)]
void csMerge( uint3 tid : SV_GroupThreadID)
{
	outprog = 0;
	inprog[tid.x] = 0;
	uint ip = 0x20;
	//while (WaveActiveAny(inprog[tid.x] < 32 * 32)) {
	while (outprog < 32 * 32 * 32) {
		uint emptyPipes = WaveActiveBallot(ip == 0x20 && inprog[tid.x] < 32 * 32).x;
		while (emptyPipes) {
			uint emptyPipeIndex = firstbitlow(emptyPipes);
			inpipe[emptyPipeIndex * 32 + tid.x] = input.Load((emptyPipeIndex * 32 * 32 + inprog[emptyPipeIndex] + tid.x) << 2);
			if (tid.x == emptyPipeIndex) {
				inprog[emptyPipeIndex] += 32;
				ip = 0;
			}
			emptyPipes = WaveActiveBallot(ip == 0x20 && inprog[tid.x] < 32 * 32).x;
		}
		DeviceMemoryBarrierWithGroupSync();
		uint candidate = inpipe[ip | (tid.x << 5)];
		uint winner = WaveActiveMin(candidate);
		if (candidate == winner) {
			if (WaveIsFirstLane()) {
				outpipe[op] = winner;
				op++;
				ip++;
			}
		}
		if (op == 0x20) {
			output.Store((outprog + tid.x) << 2, outpipe[tid.x] 
	);
			outprog += 0x20;
			op = 0;
		}
	}
//	output.Store(tid.x, outpipe[tid.x]*/);
}