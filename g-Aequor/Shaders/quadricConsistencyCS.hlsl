#include "QuadricMath.hlsli"
#include "GridUtils.hlsli"

#define QuadricConsistencySig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "UAV(u6)," \
    "UAV(u7)," \
    "UAV(u8)," \
    "UAV(u9)"

RWStructuredBuffer<float3> Position       : register(u0);
RWStructuredBuffer<float3> Normal         : register(u1);
RWStructuredBuffer<float3> TensorDiag     : register(u2);
RWStructuredBuffer<float3> TensorOffdiag  : register(u3);
RWStructuredBuffer<uint>   Label          : register(u4);
RWStructuredBuffer<uint>   cellCount      : register(u5);
RWStructuredBuffer<uint>   cellPrefixSum  : register(u6);

RWStructuredBuffer<float3> ScratchNormal        : register(u7);
RWStructuredBuffer<float3> ScratchTensorDiag    : register(u8);
RWStructuredBuffer<float3> ScratchTensorOffdiag : register(u9);

// Same-label quadric consistency constraint. Normal relaxes toward
// same-label neighbor agreement, same as before. Curvature is NOT blended
// from neighbors' own tensor values anymore -- repeated value-blending is
// diffusion, and diffusion drives toward spatial uniformity (constant
// curvature, i.e. a sphere) regardless of what the actual geometry looks
// like. There is no external ground truth for curvature to anchor to
// either -- labels constrain POSITION (via the anchor barrier), and
// curvature must be an emergent property of that label-constrained point
// cloud, nothing else. So instead this re-fits the local quadric graph
// h(u,v) = -0.5*(Auu*u^2 + 2*Auv*u*v + Avv*v^2) -- u,v,h being same-label
// neighbor positions expressed in THIS particle's own tangent frame --
// by weighted least squares each iteration: classic point-cloud curvature
// estimation (moving-least-squares height-function fit), re-derived fresh
// from wherever the neighbors currently are.
[RootSignature(QuadricConsistencySig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void quadricConsistencyCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;
    uint myLabel = Label[i];
    if (myLabel == SENTINEL_LABEL) {
        ScratchNormal[i] = Normal[i];
        ScratchTensorDiag[i] = TensorDiag[i];
        ScratchTensorOffdiag[i] = TensorOffdiag[i];
        return;
    }

    float3 pi = Position[i];
    float3 ni = Normal[i];
    float3x3 Ai = BuildTensor(TensorDiag[i], TensorOffdiag[i]);

    NeighborCells nc = NeighborCellIndices(pi);

    // -- normal: same neighbor-consensus blend as before --
    float sumW = 0.0;
    float3 sumN = float3(0, 0, 0);
    for (uint c = 0; c < nc.count; c++) {
        uint ci = nc.indices[c];
        uint count = cellCount[ci];
        uint base = cellPrefixSum[ci];
        for (uint s = 0; s < count; s++) {
            uint j = base + s;
            if (j == i || Label[j] != myLabel) continue;

            float3 pj = Position[j];
            float3 nj = Normal[j];
            float w = FootDistanceWeight(pi, pj) * NormalAlignmentWeight(ni, nj);
            if (w <= 0.0) continue;

            sumW += w;
            sumN += w * nj;
        }
    }
    float3 newN = (sumW > 1.0e-6) ? normalize(lerp(ni, normalize(sumN), QuadricConsistencyWeight)) : ni;

    // -- curvature: weighted least-squares quadric-graph fit through
    // same-label neighbor positions, in the (just-updated) tangent frame --
    float3 t1, t2;
    TangentBasis(newN, t1, t2);

    float3x3 M = float3x3(0,0,0, 0,0,0, 0,0,0);
    float3 rhs = float3(0, 0, 0);
    float fitWeight = 0.0;

    for (uint c2 = 0; c2 < nc.count; c2++) {
        uint ci2 = nc.indices[c2];
        uint count2 = cellCount[ci2];
        uint base2 = cellPrefixSum[ci2];
        for (uint s2 = 0; s2 < count2; s2++) {
            uint j2 = base2 + s2;
            if (j2 == i || Label[j2] != myLabel) continue;

            float3 pj = Position[j2];
            float w = FootDistanceWeight(pi, pj);
            if (w <= 0.0) continue;

            float3 r = pj - pi;
            float u = dot(r, t1);
            float v = dot(r, t2);
            float h = dot(r, newN);

            // Basis for the unknowns (Auu, Auv, Avv) in
            // h = -0.5*Auu*u^2 - Auv*u*v - 0.5*Avv*v^2.
            float3 a = float3(-0.5 * u * u, -u * v, -0.5 * v * v);

            M += w * Outer(a, a);
            rhs += w * a * h;
            fitWeight += w;
        }
    }

    float3x3 newA = Ai; // no update this iteration if the fit is under-determined
    if (fitWeight > 1.0e-6) {
        M[0][0] += MinSystemDiagonal;
        M[1][1] += MinSystemDiagonal;
        M[2][2] += MinSystemDiagonal;

        float3 coeffs;
        if (SolveSPD3(M, rhs, coeffs)) {
            float3x3 fittedA = coeffs.x * Outer(t1, t1)
                + coeffs.y * (Outer(t1, t2) + Outer(t2, t1))
                + coeffs.z * Outer(t2, t2);
            newA = ClampFrobenius(fittedA);
        }
    }

    ScratchNormal[i] = newN;
    ScratchTensorDiag[i] = float3(newA[0][0], newA[1][1], newA[2][2]);
    ScratchTensorOffdiag[i] = float3(newA[0][1], newA[0][2], newA[1][2]);
}
