// First of two grid-build passes. Each particle computes which cell it belongs to
// (from its position) and atomically increments that cell's count.
//
// In: position, cellCount
// Out: cellCount

#define CountGridRootSig "CBV(b0), DescriptorTable(UAV(u0, numDescriptors = 2))"

#include "AequorCb.hlsli"
#include "GridUtils.hlsli" // posToCell(), cellIndex()

RWStructuredBuffer<float3> position : register(u0);
RWStructuredBuffer<uint> cellCount : register(u1);

[RootSignature(CountGridRootSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles)
        return;

    int3 cell = posToCell(position[i]);
    uint ci = cellIndex(cell);

    InterlockedAdd(cellCount[ci], 1);
}
