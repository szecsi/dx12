#define CompactSig "RootFlags( 0 )," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=9))"

RWByteAddressBuffer strokeCounts : register(u3);
RWByteAddressBuffer strokeOffsets: register(u5);
RWByteAddressBuffer strokeList   : register(u7);

#define nMaxStrokesPerPage 16

[RootSignature(CompactSig)]
[numthreads(nMaxStrokesPerPage, 1, 1)]
void compactCS(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID)
{
    uint page = gid.x;
    uint j    = tid.x;
    if (j >= strokeCounts.Load(page * 4)) return;
    uint slot = strokeOffsets.Load(page * 4) + j;
    strokeList.Store(slot * 4, (page << 16) | j);
}
