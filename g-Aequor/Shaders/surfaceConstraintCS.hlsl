#include "QuadricMath.hlsli"
#include "GridUtils.hlsli"

// Position/Normal/TensorDiag/TensorOffdiag/Label/cellCount/cellPrefixSum are
// bound as UAV (not SRV) even where this shader only reads them -- they flip
// between being written and read by different passes every iteration
// (SpatialGrid's own internal shaders access particle fields via UAV too),
// so keeping them permanently UAV avoids state-transition barriers between
// every pass, at the cost of only needing UAV (read-after-write) barriers.
#define SurfaceConstraintSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "UAV(u6)," \
    "UAV(u7)"

RWStructuredBuffer<float3> Position       : register(u0);
RWStructuredBuffer<float3> Normal         : register(u1);
RWStructuredBuffer<float3> TensorDiag     : register(u2);
RWStructuredBuffer<float3> TensorOffdiag  : register(u3);
RWStructuredBuffer<uint>   Label          : register(u4);
RWStructuredBuffer<uint>   cellCount      : register(u5);
RWStructuredBuffer<uint>   cellPrefixSum  : register(u6);

RWStructuredBuffer<float3> ScratchVec3 : register(u7);

// Any-label surface positioning constraint: a scalar Gauss-Newton step along
// the particle's OWN current normal that best satisfies every nearby
// particle's quadric (any label -- an interface is a shared surface between
// two labels, which is exactly what freeing particles from lattice topology
// was for). Same per-slot math as g-BCC's CSOffsetGaussNewton.hlsl, just
// gated by FootDistanceWeight alone (not normal alignment -- two labels
// meeting at a junction can have very different normals while still
// describing the same surface).
[RootSignature(SurfaceConstraintSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void surfaceConstraintCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;
    if (Label[i] == SENTINEL_LABEL) { ScratchVec3[i] = Position[i]; return; }

    float3 pi = Position[i];
    float3 ni = Normal[i];

    float sumWG = 0.0;
    float sumWGG = 0.0;

    NeighborCells nc = NeighborCellIndices(pi);
    for (uint c = 0; c < nc.count; c++) {
        uint ci = nc.indices[c];
        uint count = cellCount[ci];
        uint base = cellPrefixSum[ci];
        for (uint s = 0; s < count; s++) {
            uint j = base + s;
            if (j == i || Label[j] == SENTINEL_LABEL) continue;

            float3 pj = Position[j];
            float w = FootDistanceWeight(pi, pj) * SurfaceDataWeight;
            if (w <= 0.0) continue;

            float3x3 Aj = BuildTensor(TensorDiag[j], TensorOffdiag[j]);
            float residual; float3 gradient;
            EvaluateQuadric(Normal[j], Aj, pj, pi, residual, gradient);

            float g = dot(gradient, ni);
            sumWG += w * g * residual;
            sumWGG += w * g * g;
        }
    }

    float t = -sumWG / max(sumWGG, MinSystemDiagonal);
    t *= SurfaceStepDamping;
    t = clamp(t, -MaxStep, MaxStep);

    ScratchVec3[i] = pi + t * ni;
}
