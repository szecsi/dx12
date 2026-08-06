// Same-label PBF position correction -- pbf's deltaCS.hlsl, restricted to
// same-label neighbors, minus the tensile-instability sCorr term (not
// clearly meaningful for a surface point distribution, left for later if
// clumping/rarefaction artifacts show up in practice).
#define SpacingDeltaSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)"

#include "AequorCb.hlsli"
#include "SphKernels.hlsli"
#include "GridUtils.hlsli"

RWStructuredBuffer<float3> Position       : register(u0);
RWStructuredBuffer<uint>   Label          : register(u1);
RWStructuredBuffer<uint>   cellCount      : register(u2);
RWStructuredBuffer<uint>   cellPrefixSum  : register(u3);
RWStructuredBuffer<float>  Lambda         : register(u4);

RWStructuredBuffer<float3> ScratchVec3 : register(u5);

[RootSignature(SpacingDeltaSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void spacingDeltaCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;
    float3 pi = Position[i];
    uint myLabel = Label[i];
    if (myLabel == SENTINEL_LABEL) { ScratchVec3[i] = pi; return; }

    float lambdaI = Lambda[i];
    float3 deltaP = float3(0, 0, 0);

    NeighborCells nc = NeighborCellIndices(pi);
    for (uint c = 0; c < nc.count; c++) {
        uint ci = nc.indices[c];
        uint count = cellCount[ci];
        uint base = cellPrefixSum[ci];
        for (uint s = 0; s < count; s++) {
            uint j = base + s;
            if (j == i || Label[j] != myLabel) continue;

            float3 r = pi - Position[j];
            float r2 = dot(r, r);
            if (r2 < EPSILON * EPSILON) {
                deltaP += overlapJitter(i, j) * (H * 0.001);
                continue;
            }

            float3 gradW = SpikyGrad(r, r2);
            deltaP += (lambdaI + Lambda[j]) * gradW;
        }
    }

    float3 step = deltaP / RHO0;
    float stepLen = length(step);
    if (stepLen > MaxStep)
        step *= MaxStep / stepLen;

    ScratchVec3[i] = pi + step;
}
