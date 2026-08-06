#include "bccCommon.hlsli"
#include "FootVector.hlsli"

#define FootVectorBuildSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(SRV(t0, numDescriptors=6))," \
    "UAV(u0)"

cbuffer RootConsts : register(b0) {
    uint useExactFoot; // nonzero only when initMethod == Analytic, see BccApp::RunInitPasses
};

Texture3D<uint4> gA0 : register(t0);
Texture3D<uint4> gB0 : register(t2);
Texture3D<uint4> gAFoot : register(t4);
Texture3D<uint4> gBFoot : register(t5);

RWStructuredBuffer<FootVectorEntry> gOut : register(u0);

// `footTexel` is only meaningful (and only read from a texture actually
// written this init pass) when useExactFoot is set -- see analyticInitCS.hlsl;
// JFA has no continuous foot point, only the quantized seed lattice site
// already carried in texel.z, so it always falls back to that.
FootVectorEntry BuildEntry(float3 start, uint4 texel, uint4 footTexel, bool useExact)
{
    FootVectorEntry e;
    e.start = start;
    if (texel.y != SENTINEL_LABEL) {
        e.end = useExact ? asfloat(footTexel.xyz) : SeedWorldPos(texel.z);
        e.len = distance(start, e.end);
    } else {
        e.end = start;
        e.len = 1.0e6;
    }
    e.label = (float)texel.x;
    return e;
}

// Turns the converged A0/B0 (and, in analytic mode, AFoot/BFoot) footvector
// field into a flat buffer of (start, end, length) entries -- one per lattice
// point per sublattice -- that the footLine/footPoint draw passes can index
// directly by SV_VertexID / SV_InstanceID, instead of re-deriving each vector
// from the raw texel data every frame per draw. Run once at init and again
// whenever the grid is reinitialized (see BccApp::RunInitPasses), never
// per-frame.
[RootSignature(FootVectorBuildSig)]
[numthreads(4, 4, 4)]
void footVectorBuildCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    uint idx = tid.x + tid.y * GridRes + tid.z * GridRes * GridRes;
    const uint total = GridRes * GridRes * GridRes;
    bool useExact = useExactFoot != 0;

    gOut[idx]         = BuildEntry(APos((int3)tid), gA0.Load(int4(tid, 0)), gAFoot.Load(int4(tid, 0)), useExact);
    gOut[idx + total] = BuildEntry(BPos((int3)tid), gB0.Load(int4(tid, 0)), gBFoot.Load(int4(tid, 0)), useExact);
}
