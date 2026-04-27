#define ArgsSig "RootFlags( 0 )," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=9))"

RWByteAddressBuffer strokeCounts : register(u3);
RWByteAddressBuffer strokeOffsets: register(u5);
RWByteAddressBuffer dispatchArgs : register(u8);

#define CUBIC_GROUP_THREADS 64

[RootSignature(ArgsSig)]
[numthreads(1, 1, 1)]
void argsCS()
{
    uint lastPage = 1024u * 16u - 1;
    uint total    = strokeOffsets.Load(lastPage * 4) + strokeCounts.Load(lastPage * 4);
    uint groupsX  = (total + CUBIC_GROUP_THREADS - 1) / CUBIC_GROUP_THREADS;
    dispatchArgs.Store(0, groupsX);
    dispatchArgs.Store(4, 1);
    dispatchArgs.Store(8, 1);
    // D3D12_DRAW_ARGUMENTS at offset 12: one point per stroke, single instance
    dispatchArgs.Store(12, total);
    dispatchArgs.Store(16, 1);
    dispatchArgs.Store(20, 0);
    dispatchArgs.Store(24, 0);
}
