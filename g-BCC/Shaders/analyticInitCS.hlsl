#include "TorusListCb.hlsli"
#include "TorusSdf.hlsli"
#include "bccCommon.hlsli"

#define AnalyticInitSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))," \
    "CBV(b1)"

cbuffer RootConsts : register(b0) {
    uint domainIsB;
};

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gA1 : register(u1);
RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gB1 : register(u3);
RWTexture3D<uint4> gAFoot : register(u4);
RWTexture3D<uint4> gBFoot : register(u5);

// Alternative to the rasterize+JFA pipeline: computes the exact footvector
// (distance and nearest surface point) to the nearest torus analytically, in
// a single pass, instead of propagating from seeded boundary voxels. No
// ping-pong buffers needed -- writes straight into A0/B0.
[RootSignature(AnalyticInitSig)]
[numthreads(4, 4, 4)]
void analyticInitCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    float3 pos = (domainIsB != 0) ? BPos((int3)tid) : APos((int3)tid);

    if (nTorii == 0) {
        uint4 empty = uint4(0, SENTINEL_LABEL, 0, 0);
        if (domainIsB != 0) gB0[tid] = empty; else gA0[tid] = empty;
        return;
    }

    // Own label: last matching shape wins, same rule torusInitCS rasterizes with.
    uint ownLabel = 0;
    for (uint i = 0; i < nTorii; i++) {
        if (ShapeSd(pos, torii[i]) <= 0.0)
            ownLabel = torii[i].label;
    }

    // Nearest shape surface, exactly -- not propagated from neighbors.
    float bestDist = 1.0e30;
    uint bestLabel = 0;
    float3 bestFoot = pos;
    for (uint j = 0; j < nTorii; j++) {
        float ad = abs(ShapeSd(pos, torii[j]));
        if (ad < bestDist) {
            bestDist = ad;
            bestLabel = torii[j].label;
            bestFoot = ShapeNearestPoint(pos, torii[j]);
        }
    }

    // Crossing the nearest torus's surface leaves its label (if we're inside
    // it) or enters it (if we're outside); overlapping torii are approximated
    // the same way the JFA path's neighbor-seeded interfaces are.
    uint otherLabel = (ownLabel == bestLabel) ? 0 : bestLabel;

    // Quantize the exact foot point to its nearest lattice site purely so the
    // shared texel format (and EstimateFilteredNormal's direction lookup)
    // keeps working unchanged -- the *distance* stored below is the true
    // analytic one, unaffected by this quantization.
    int3 footA = clamp((int3)round(bestFoot / CellSize), 0, (int)GridRes - 1);
    int3 footB = clamp((int3)round(bestFoot / CellSize - 0.5), 0, (int)GridRes - 1);
    bool footIsB = distance(BPos(footB), bestFoot) < distance(APos(footA), bestFoot);
    uint packedSeed = PackSeed(footIsB, (uint3)(footIsB ? footB : footA));

    uint4 texel = uint4(ownLabel, otherLabel, packedSeed, asuint(bestDist));
    // Also stash the *unquantized* foot point, bit-reinterpreted into a uint4
    // (same asuint()/asfloat() trick as the distance channel above) -- this is
    // what footVectorBuildCS.hlsl uses for the footvector visualization's
    // "footpoint" spheres/line endpoints in analytic mode, so they land
    // exactly on the surface instead of snapped to the nearest lattice site.
    uint4 footTexel = uint4(asuint(bestFoot.x), asuint(bestFoot.y), asuint(bestFoot.z), 0);
    if (domainIsB != 0) {
        gB0[tid] = texel;
        gBFoot[tid] = footTexel;
    } else {
        gA0[tid] = texel;
        gAFoot[tid] = footTexel;
    }
}
