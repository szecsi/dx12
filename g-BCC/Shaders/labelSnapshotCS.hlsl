#include "bccCommon.hlsli"

#define LabelSnapshotSig "RootFlags(0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))"

RWTexture3D<uint4> gA0    : register(u0);
RWTexture3D<uint4> gA1    : register(u1);
RWTexture3D<uint4> gB0    : register(u2);
RWTexture3D<uint4> gB1    : register(u3);
RWTexture3D<uint4> gAFoot : register(u4);
RWTexture3D<uint4> gBFoot : register(u5);

// Snapshots the ground-truth per-voxel label (freshly written by torusInitCS,
// before jfaSeedCS/jfaStepCS or any of the K per-label passes in
// sparseLabelSeedCS.hlsl overwrite A0/B0) into AFoot/BFoot -- otherwise idle
// at this point in the init sequence -- so every one of the K per-label
// passes has a stable reference to compare against regardless of what A0/B0
// currently hold.
[RootSignature(LabelSnapshotSig)]
[numthreads(4, 4, 4)]
void labelSnapshotCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    gAFoot[tid] = uint4(gA0[tid].x, 0, 0, 0);
    gBFoot[tid] = uint4(gB0[tid].x, 0, 0, 0);
}
