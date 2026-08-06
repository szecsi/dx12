#include "AequorCb.hlsli"
#include "QuadricMath.hlsli"
#include "PatchSegment.hlsli"

#define QuadricPatchBuildSig "RootFlags(0)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=4, b1)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)"

cbuffer PatchConsts : register(b1) {
    uint PatchOffset;
    uint PatchStride;
    uint NumSelected;
    uint PatchRadiusBits; // bit-reinterpreted float, see asfloat() below
};

RWStructuredBuffer<float3> Position       : register(u0);
RWStructuredBuffer<float3> Normal         : register(u1);
RWStructuredBuffer<float3> TensorDiag     : register(u2);
RWStructuredBuffer<float3> TensorOffdiag  : register(u3);
RWStructuredBuffer<uint>   Label          : register(u4);

RWStructuredBuffer<PatchSegmentEntry> PatchSegments : register(u5);

// Must match AequorApp::QuadricPatchRingSamples/QuadricPatchSegmentsPerNode.
static const uint RingSamples = 16;
static const uint SegmentsPerNode = 2 * (RingSamples - 1) + RingSamples;

PatchSegmentEntry InvalidSegment()
{
    PatchSegmentEntry e;
    e.start = float3(0, 0, 0);
    e.end = float3(0, 0, 0);
    e.len = -1.0; // quadricPatchLineVS.hlsl's degenerate-collapse check
    e._pad = 0.0;
    return e;
}

PatchSegmentEntry MakeSegment(float3 a, float3 b)
{
    PatchSegmentEntry e;
    e.start = a;
    e.end = b;
    e.len = distance(a, b);
    e._pad = 0.0;
    return e;
}

// Same closed-form quadric-graph solve g-BCC's quadricPatchBuildCS.hlsl uses
// -- see that file for the full derivation/numerical-stability rationale.
// The n-t1/n-t2 "normal section" planes give a true graph (u single-valued
// in v), solved via the conjugate-rationalized form to avoid catastrophic
// cancellation as mu -> 0.
bool SolveNormalSection(float mu, float K, float v, out float u)
{
    float rhs = K * v * v;
    float disc = 1.0 - mu * rhs;
    if (disc < 0.0) {
        u = 0.0;
        return false;
    }
    u = -rhs / (1.0 + sqrt(disc));
    return true;
}

// The t1-t2 tangent plane's zero set is degenerate (just the footpoint), so
// this instead intersects a plane offset by `radius` along n, giving a real
// ellipse/hyperbola-branch curve.
bool SolveTangentRing(float Kaa, float Kab, float Kbb, float target, float angle, out float2 uv)
{
    float cs = cos(angle);
    float sn = sin(angle);
    float k = Kaa * cs * cs + 2.0 * Kab * cs * sn + Kbb * sn * sn;

    if (abs(k) < 1.0e-8) {
        uv = float2(0, 0);
        return false;
    }
    float s2 = target / k;
    if (s2 < 0.0) {
        uv = float2(0, 0);
        return false;
    }
    float s = sqrt(s2);
    uv = s * float2(cs, sn);
    return true;
}

// Samples a wireframe cage for the local quadric Q(x) = n.r + 0.5 r^T A r
// (r = x - particle position) that a stride-selected particle's own
// (normal, curvature tensor) define -- the exact same implicit surface
// EvaluateQuadric measures neighbor agreement against. Ported from g-BCC's
// quadricPatchBuildCS.hlsl, minus the PackedNode4/dominant-slot selection --
// a g-Aequor particle already has exactly one (position, normal, A), no
// per-node label slots to pick among.
[RootSignature(QuadricPatchBuildSig)]
[numthreads(64, 1, 1)]
void quadricPatchBuildCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint selIdx = dispatchThreadID.x;
    if (selIdx >= NumSelected)
        return;

    uint outBase = selIdx * SegmentsPerNode;
    uint realIndex = PatchOffset + selIdx * PatchStride;

    if (realIndex >= numParticles || Label[realIndex] == SENTINEL_LABEL) {
        [unroll]
        for (uint z = 0; z < SegmentsPerNode; z++)
            PatchSegments[outBase + z] = InvalidSegment();
        return;
    }

    float3 p = Position[realIndex];
    float3 n = Normal[realIndex];
    float3x3 A = BuildTensor(TensorDiag[realIndex], TensorOffdiag[realIndex]);

    float3 t1, t2;
    TangentBasis(n, t1, t2);

    float radius = asfloat(PatchRadiusBits);
    float mu = dot(n, mul(A, n));
    float maxDist = 4.0 * radius;

    uint outIdx = outBase;

    // -- the two open normal-section curves (n-t1 and n-t2) --
    float3 tangentAxis[2] = { t1, t2 };
    [unroll]
    for (uint ring = 0; ring < 2; ring++)
    {
        float3 axisB = tangentAxis[ring];
        float K = dot(axisB, mul(A, axisB));

        float3 pts[RingSamples];
        bool ptValid[RingSamples];

        [unroll]
        for (uint k = 0; k < RingSamples; k++)
        {
            float v = radius * (2.0 * (float(k) / float(RingSamples - 1)) - 1.0);
            float u;
            bool valid = SolveNormalSection(mu, K, v, u);
            pts[k] = p + u * n + v * axisB;
            ptValid[k] = valid && (distance(pts[k], p) <= maxDist);
        }

        [unroll]
        for (uint k2 = 0; k2 < RingSamples - 1; k2++)
        {
            if (ptValid[k2] && ptValid[k2 + 1])
                PatchSegments[outIdx] = MakeSegment(pts[k2], pts[k2 + 1]);
            else
                PatchSegments[outIdx] = InvalidSegment();
            outIdx++;
        }
    }

    // -- the closed tangent ring, offset by `radius` along n --
    {
        float Kaa = dot(t1, mul(A, t1));
        float Kbb = dot(t2, mul(A, t2));
        float Kab = dot(t1, mul(A, t2));
        float target = -2.0 * (radius + 0.5 * mu * radius * radius);

        float3 pts[RingSamples];
        bool ptValid[RingSamples];

        [unroll]
        for (uint k = 0; k < RingSamples; k++)
        {
            float angle = 6.28318530718 * (float(k) / float(RingSamples));
            float2 uv;
            bool valid = SolveTangentRing(Kaa, Kab, Kbb, target, angle, uv);
            pts[k] = p + radius * n + uv.x * t1 + uv.y * t2;
            ptValid[k] = valid && (distance(pts[k], p) <= maxDist);
        }

        [unroll]
        for (uint k2 = 0; k2 < RingSamples; k2++)
        {
            uint k2n = (k2 + 1) % RingSamples;
            if (ptValid[k2] && ptValid[k2n])
                PatchSegments[outIdx] = MakeSegment(pts[k2], pts[k2n]);
            else
                PatchSegments[outIdx] = InvalidSegment();
            outIdx++;
        }
    }
}
