#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"
#include "FootVector.hlsli"

#define QuadricPatchBuildSig "RootFlags(0)," \
    "CBV(b0)," \
    "RootConstants(num32BitConstants=4, b1)," \
    "SRV(t0)," \
    "SRV(t1)," \
    "UAV(u0)"

cbuffer PatchConsts : register(b1) {
    uint PatchOffset;
    uint PatchStride;
    uint NumSelected;
    uint PatchRadiusBits; // bit-reinterpreted float, see asfloat() below
};

StructuredBuffer<PackedNode4> Nodes : register(t0);
StructuredBuffer<SparseNodeSeeds> Seeds : register(t1);
RWStructuredBuffer<FootVectorEntry> PatchSegments : register(u0);

// Must match BccApp::QuadricPatchRingSamples/QuadricPatchSegmentsPerNode.
static const uint RingSamples = 16;
// 2 open "normal section" curves (RingSamples-1 segments each, see
// SolveNormalSection) + 1 closed "tangent ring" curve (RingSamples segments,
// see SolveTangentRing).
static const uint SegmentsPerNode = 2 * (RingSamples - 1) + RingSamples;

FootVectorEntry InvalidSegment()
{
    FootVectorEntry e;
    e.start = float3(0, 0, 0);
    e.end = float3(0, 0, 0);
    e.len = -1.0; // quadricPatchLineVS.hlsl's degenerate-collapse check
    e.label = 0;
    return e;
}

FootVectorEntry MakeSegment(float3 a, float3 b)
{
    FootVectorEntry e;
    e.start = a;
    e.end = b;
    e.len = distance(a, b);
    e.label = 0;
    return e;
}

// The n-t1 and n-t2 planes are "normal sections": true differential-geometry
// curves showing how far the surface bends away from the tangent plane
// along t1 (or t2). Because axisA is always n here, na=dot(n,n)=1 exactly,
// and ProjectTensorToNormal already zeroed A's n-tangent cross term, so
// Q(u,v) = u + 0.5*(mu*u^2 + K*v^2) = 0 (u = height along n, v = tangent
// coordinate, mu = n.A.n, K = the tangent curvature in this direction) is a
// genuine GRAPH -- u is a single-valued function of v -- not a general
// implicit curve needing angular/radial sampling. Solving the quadratic in u
// directly (picking the root that -> -0.5*K*v^2 as mu -> 0, i.e. the branch
// actually passing through the footpoint) gives a clean, monotonic curve
// with no zigzag; sampling this by angle around a circle (the old approach)
// sent half the predictor points out along the normal, where the true
// surface isn't, forcing Newton's correction to yank them back toward the
// footpoint and criss-cross the ones that started near the tangent.
// Returns false only where the true surface doesn't reach this far along v
// at all (a genuine curvature/discriminant limit, not a sampling artifact).
//
// Solved via the numerically stable form -- NOT the textbook
// u = (-1+sqrt(disc))/mu. That formula subtracts two nearly-equal values
// whenever mu is small-but-nonzero (sqrt(disc) -> 1 as mu -> 0), which is
// exactly the common case for a fairly flat patch: catastrophic cancellation
// there, then dividing the near-zero, noisy result by a small mu, produced
// wildly wrong (occasionally huge) u values -- the "super long lines" bug.
// Rationalizing by the conjugate (-1-sqrt(disc)) cancels mu out of the
// denominator entirely: u = -rhs/(1+sqrt(disc)), well-conditioned for every
// mu including exactly 0 (no special-case branch needed either).
bool SolveNormalSection(float mu, float K, float v, out float u)
{
    float rhs = K * v * v; // mu*u^2 + 2*u + rhs = 0
    float disc = 1.0 - mu * rhs;
    if (disc < 0.0) {
        u = 0.0;
        return false;
    }
    u = -rhs / (1.0 + sqrt(disc));
    return true;
}

// The t1-t2 plane is the exact tangent plane (na=nb=0 exactly, since t1,t2
// are both perpendicular to n by construction) -- Q restricted to it is a
// homogeneous quadratic form with no linear term, so its zero set is just
// the footpoint itself for a convex/concave (elliptic) patch: there is no
// nearby curve to trace there at all. Instead this intersects a plane
// offset from the tangent plane by `radius` along n, which turns the
// homogeneous form into a proper level set (a real ellipse for an elliptic
// point, or hyperbola branches for a saddle) -- a meaningful, non-degenerate
// curve instead of a Newton-iteration numerical residual. Solved directly
// via the quadratic form's scaling property: k(angle)*s^2 = target, so
// s = sqrt(target/k(angle)); returns false where the sign doesn't work out
// (a genuine angular gap, e.g. the excluded directions of a saddle).
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
// (r = x - footpoint) that the selected node's DOMINANT slot (largest
// current offset among its populated labels -- the same choice
// quadricRecoverCS.hlsl makes for its "own label") defines -- the same
// implicit surface EvaluateSlotQuadric measures neighbor agreement against.
// This is a genuine quadric, not necessarily a paraboloid graph:
// ProjectTensorToNormal only forces n to be AN eigenvector of A, it never
// forces the eigenvalue along n (mu = n.A.n) to zero. Only the dominant slot
// is visualized -- showing all (up to 4) populated slots per node is a
// possible future enhancement, not done here.
[RootSignature(QuadricPatchBuildSig)]
[numthreads(64, 1, 1)]
void quadricPatchBuildCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint selIdx = dispatchThreadID.x;
    if (selIdx >= NumSelected)
        return;

    uint outBase = selIdx * SegmentsPerNode;
    uint realIndex = PatchOffset + selIdx * PatchStride;
    uint totalNodes = 2u * LatticeNodeCount;

    if (realIndex >= totalNodes) {
        [unroll]
        for (uint z = 0; z < SegmentsPerNode; z++)
            PatchSegments[outBase + z] = InvalidSegment();
        return;
    }

    uint3 c;
    uint sub;
    DecodeIndex(realIndex, c, sub);
    float3 nodeX = NodePosition(c, sub);

    SparseNodeSeeds nodeSeeds = Seeds[realIndex];
    PackedNode4 packed = Nodes[realIndex];

    uint dom = SparseLabelCount;
    float domOffset = -1.0e30;
    [unroll]
    for (uint slot = 0; slot < SparseLabelCount; slot++) {
        if (GetSlotLabel(nodeSeeds, slot) == SENTINEL_LABEL)
            continue;
        float offset = DecodeSlot(packed, slot).offset;
        if (offset > domOffset) { dom = slot; domOffset = offset; }
    }

    if (dom == SparseLabelCount) {
        [unroll]
        for (uint z2 = 0; z2 < SegmentsPerNode; z2++)
            PatchSegments[outBase + z2] = InvalidSegment();
        return;
    }

    LabelSlotState self = DecodeSlot(packed, dom);
    float3 p = SlotFootpoint(nodeX, self);
    float3 n = self.normal;

    float3 t1, t2;
    TangentBasis(n, t1, t2);

    float radius = asfloat(PatchRadiusBits);
    float mu = dot(n, mul(self.A, n));
    // Defense-in-depth against any remaining numerically-degenerate case
    // (e.g. a near-singular tangent curvature in SolveTangentRing) actually
    // placing a point far from the footpoint -- this is a wireframe
    // "clipped to a distance", so nothing should ever land far outside that
    // distance regardless of how a particular formula misbehaves.
    float maxDist = 4.0 * radius;

    uint outIdx = outBase;

    // -- the two open normal-section curves (n-t1 and n-t2) --
    float3 tangentAxis[2] = { t1, t2 };
    [unroll]
    for (uint ring = 0; ring < 2; ring++)
    {
        float3 axisB = tangentAxis[ring];
        float K = dot(axisB, mul(self.A, axisB));

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
        float Kaa = dot(t1, mul(self.A, t1));
        float Kbb = dot(t2, mul(self.A, t2));
        float Kab = dot(t1, mul(self.A, t2));
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
