#include "bccCommon.hlsli"

#define StepSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=3, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=4))"

cbuffer RootConsts : register(b0) {
    uint stepSize;
    uint pingIndex;
    uint domainIsB;
};

RWTexture3D<uint4> gA0 : register(u0);
RWTexture3D<uint4> gA1 : register(u1);
RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gB1 : register(u3);

// One Jump Flood propagation step for a single sublattice: compares the
// current best (distance, other-side label, seed coord) against candidates
// fetched from same-sublattice neighbors (26, scaled by `step`) and
// cross-sublattice BCC neighbors (8, scaled by `step`), keeping the closest.
void DoStep(RWTexture3D<uint4> srcSelf, RWTexture3D<uint4> srcOther, RWTexture3D<uint4> dstSelf,
            uint3 tid, bool selfIsB, uint step)
{
    float3 selfPos = selfIsB ? BPos((int3)tid) : APos((int3)tid);

    uint4 own = srcSelf[tid];
    uint  bestLabel = own.y;
    uint  bestSeed  = own.z;
    float bestDist  = (own.y != SENTINEL_LABEL) ? asfloat(own.w) : 1.0e30;

    for (uint i = 0; i < 26; i++) {
        int3 nIdx = (int3)tid + SameLatticeOffsets[i] * (int)step;
        if (!InBounds(nIdx)) continue;
        uint4 cand = srcSelf[nIdx];
        if (cand.y == SENTINEL_LABEL) continue;
        float d = distance(SeedWorldPos(cand.z), selfPos);
        if (d < bestDist) { bestDist = d; bestLabel = cand.y; bestSeed = cand.z; }
    }

    for (uint j = 0; j < 8; j++) {
        int3 co = selfIsB ? CrossOffsetsBtoA[j] : CrossOffsetsAtoB[j];
        int3 nIdx = (int3)tid + co * (int)step;
        if (!InBounds(nIdx)) continue;
        uint4 cand = srcOther[nIdx];
        if (cand.y == SENTINEL_LABEL) continue;
        float d = distance(SeedWorldPos(cand.z), selfPos);
        if (d < bestDist) { bestDist = d; bestLabel = cand.y; bestSeed = cand.z; }
    }

    dstSelf[tid] = uint4(own.x, bestLabel, bestSeed, asuint(bestDist));
}

[RootSignature(StepSig)]
[numthreads(4, 4, 4)]
void jfaStepCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    if (domainIsB == 0) {
        if (pingIndex == 0) DoStep(gA0, gB0, gA1, tid, false, stepSize);
        else                 DoStep(gA1, gB1, gA0, tid, false, stepSize);
    } else {
        if (pingIndex == 0) DoStep(gB0, gA0, gB1, tid, true, stepSize);
        else                 DoStep(gB1, gA1, gB0, tid, true, stepSize);
    }
}
