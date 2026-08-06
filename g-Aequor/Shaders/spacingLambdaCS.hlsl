// Same-label PBF density-constraint Lagrange multiplier -- pbf's lambdaCS.hlsl,
// restricted to same-label neighbors (this is the "even distribution (same
// label)" constraint agreed in design discussion).
#define SpacingLambdaSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)"

#include "AequorCb.hlsli"
#include "SphKernels.hlsli"
#include "GridUtils.hlsli"

RWStructuredBuffer<float3> Position       : register(u0);
RWStructuredBuffer<uint>   Label          : register(u1);
RWStructuredBuffer<uint>   cellCount      : register(u2);
RWStructuredBuffer<uint>   cellPrefixSum  : register(u3);

RWStructuredBuffer<float> Lambda : register(u4);

[RootSignature(SpacingLambdaSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void spacingLambdaCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;
    uint myLabel = Label[i];
    if (myLabel == SENTINEL_LABEL) { Lambda[i] = 0.0; return; }

    float3 pi = Position[i];

    float rho = 0.0;
    float3 gradI = float3(0, 0, 0);
    float gradSqSum = 0.0;

    NeighborCells nc = NeighborCellIndices(pi);
    for (uint c = 0; c < nc.count; c++) {
        uint ci = nc.indices[c];
        uint count = cellCount[ci];
        uint base = cellPrefixSum[ci];
        for (uint s = 0; s < count; s++) {
            uint j = base + s;
            if (Label[j] != myLabel) continue;

            float3 r = pi - Position[j];
            float r2 = dot(r, r);
            rho += Poly6(r, r2);

            if (j != i) {
                float3 gradW = SpikyGrad(r, r2);
                float3 gradJ = -(1.0 / RHO0) * gradW;
                gradSqSum += dot(gradJ, gradJ);
                gradI += gradW / RHO0;
            }
        }
    }

    gradSqSum += dot(gradI, gradI);
    float C = rho / RHO0 - 1.0;
    Lambda[i] = -C / (gradSqSum + SpacingEpsilon);
}
