// Zeroes a per-eye SeedCounterBuffer[0] each frame, before seedGenerateCS.
// One thread; the caller inserts a UAV barrier before dispatching
// seedGenerateCS so the clear is visible to it.
#define ClearSeedCounterSig "RootFlags(0)," \
    "UAV(u0)"

RWStructuredBuffer<uint> SeedCounter : register(u0);

[RootSignature(ClearSeedCounterSig)]
[numthreads(1, 1, 1)]
void clearSeedCounterCS(uint3 tid : SV_DispatchThreadID)
{
    SeedCounter[0] = 0;
}
