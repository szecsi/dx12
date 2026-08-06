#include "bccCommon.hlsli"
#include "QuadricFootField.hlsli"
#include "SparseLabelSeed.hlsli"
#include "TorusListCb.hlsli"
#include "TorusSdf.hlsli"

#define QuadricSeedSig "RootFlags(0)," \
    "CBV(b0)," \
    "SRV(t0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "CBV(b1)"

StructuredBuffer<SparseNodeSeeds> Seeds : register(t0);

RWStructuredBuffer<PackedNode4> OriginalNodes : register(u0);
RWStructuredBuffer<PackedNode4> CurrentNodes : register(u1);

// Analytic curvature tensor of torus t in world space, using its own
// STANDARD outward normal convention (matching TorusSdf.hlsli's
// NearestTorusPoint -- positive SdTorus outside, negative inside, gradient
// points outward), evaluated as if `p` sits exactly on its surface (true
// for the footpoints this feeds -- see SeedSlot below). Textbook torus
// principal curvatures: 1/minorRadius around the tube's circular
// cross-section (the meridian, constant everywhere), and
// cos(theta)/(majorRadius + minorRadius*cos(theta)) around the big ring
// (theta = angle around the tube -- positive/convex at the outer equator,
// negative/saddle-shaped at the inner equator, zero at top/bottom). Both
// principal directions are perpendicular to the outward normal by
// construction, so mu = n.A.n comes out exactly 0 here -- matching the
// eikonal (|gradient|=1 everywhere) property a true signed distance
// function has, which the fitted (TensorStep-driven) tensor only
// approximates.
float3x3 AnalyticTorusTensor(float3 p, TorusDesc t, out float3 outwardNormal)
{
    float3 d = p - t.center;
    float axial = dot(d, t.axis);
    float3 planarVec = d - axial * t.axis;
    float planarLen = length(planarVec);
    float3 planarDir = (planarLen > 1.0e-6) ? (planarVec / planarLen) : float3(1, 0, 0);

    float3 ringPoint = t.center + planarDir * t.majorRadius;
    float3 toP = p - ringPoint;
    float toPLen = length(toP);
    float3 dir = (toPLen > 1.0e-6) ? (toP / toPLen) : planarDir;
    outwardNormal = dir;

    float cosT = dot(dir, planarDir);
    float sinT = dot(dir, t.axis);
    float3 tTheta = -sinT * planarDir + cosT * t.axis; // meridian tangent (around the tube)
    float3 tPhi = cross(t.axis, planarDir);             // ring tangent (around the big circle)

    float kTheta = 1.0 / max(t.minorRadius, 1.0e-6);
    float kPhi = cosT / max(t.majorRadius + t.minorRadius * cosT, 1.0e-6);

    return kTheta * Outer(tTheta, tTheta) + kPhi * Outer(tPhi, tPhi);
}

// Analytic curvature tensor of ellipsoid t (axis-aligned, semi-axes in
// t.axis -- see TorusSdf.hlsli's SdEllipsoid/BccApp::BuildShapeList),
// evaluated at a point `p` assumed to sit on its surface. For any quadratic
// implicit surface F(x) = (x-center)^T D (x-center) - 1 (D =
// diag(1/rx^2,1/ry^2,1/rz^2) here), the second fundamental form restricted
// to the tangent plane is exactly P*H*P/|g| -- H = 2D is the (constant,
// since F is quadratic) Hessian, g = grad(F) = 2*D*(p-center) is the
// gradient at p, and P = I - n(x)n projects onto the tangent plane. No
// angular/tangent-frame bookkeeping needed here, unlike the torus case,
// since H is already diagonal and constant in world space -- verified
// against the known sphere result (rx=ry=rz=R reduces this to the textbook
// (1/R)*P, same as AnalyticTorusTensor's sphere-case sanity check).
float3x3 AnalyticEllipsoidTensor(float3 p, TorusDesc t, out float3 outwardNormal)
{
    float3 r = t.axis;
    float3 invR2 = 1.0 / max(r * r, 1.0e-6);
    float3 d = p - t.center;
    float3 g = 2.0 * (d * invR2);
    float gLen = length(g);
    outwardNormal = (gLen > 1.0e-8) ? (g / gLen) : float3(0, 0, 1);

    float3x3 H = float3x3(
        2.0 * invR2.x, 0.0, 0.0,
        0.0, 2.0 * invR2.y, 0.0,
        0.0, 0.0, 2.0 * invR2.z);
    float3x3 P = Identity3() - Outer(outwardNormal, outwardNormal);
    return mul(P, mul(H, P)) / max(gLen, 1.0e-8);
}

// Dispatches to whichever shape TorusListCb.ShapeKind currently selects.
float3x3 AnalyticShapeTensor(float3 p, TorusDesc t, out float3 outwardNormal)
{
    return (ShapeKind != 0)
        ? AnalyticEllipsoidTensor(p, t, outwardNormal)
        : AnalyticTorusTensor(p, t, outwardNormal);
}

// Turns one slot of a node's fixed 4-slot label record (sparseNodeSeeds, from
// RunSparseLabelSeedInit -- signed distance to that label's own boundary,
// positive on the labeled side) into the initial mutable relaxation state for
// that slot. footpoint = nodeX + offset*normal must reconstruct the actual
// boundary point (nodeX + dist*dir, dir = direction to it) regardless of
// which side of the boundary this node is on, so normal and offset's signs
// have to be chosen together: offset = signedDist (+dist inside, -dist
// outside), and normal = dir when inside (+dist*dir = dist*dir, correct) or
// -dir when outside (-dist*(-dir) = dist*dir, correct too). The result is a
// normal that always points "outward" -- from inside the label's territory,
// through the boundary, into open space -- honestly and consistently for
// every node regardless of which side it's on, so two nodes on opposite
// sides of the SAME label's boundary get naturally aligned (not artificially
// anti-parallel) normals from the very first iteration onward.
//
// Curvature: analytic, from whichever torus is actually nearest this slot's
// footpoint (usually the only one), rather than zero -- lets the position/
// normal relaxation be tested with exact curvature from the start, isolating
// it from tensor-fitting/linear-approximation error (see TensorStep). The
// analytic tensor is computed once using the torus's own canonical outward
// convention, then flipped (A -> -A) if this slot's own normal turns out to
// be the opposite of that -- e.g. a background slot describing the same
// boundary from the far side -- since two implicit functions describing the
// same surface with opposite-signed normals must be exact negatives of each
// other (verified directly: Q'(x) = -Q(x) requires n' = -n and A' = -A).
// An empty slot (SENTINEL_LABEL) gets zero confidence so it never moves and
// never influences a neighbor.
LabelSlotState SeedSlot(float3 nodeX, uint label, float signedDist, uint seed)
{
    LabelSlotState s;
    s.A = float3x3(0, 0, 0, 0, 0, 0, 0, 0, 0);

    if (label == SENTINEL_LABEL) {
        s.normal = float3(0, 0, 1);
        s.offset = 0.0;
        s.confidence = 0.0;
        return s;
    }

    float3 toBoundary = SeedWorldPos(seed) - nodeX;
    float l2 = dot(toBoundary, toBoundary);
    float3 dir = (l2 > 1.0e-12) ? (toBoundary * rsqrt(l2)) : float3(0, 0, 1);

    s.normal = (signedDist >= 0.0) ? dir : -dir;
    s.offset = signedDist;
    s.confidence = 1.0;

    if (nTorii > 0) {
        float3 p = nodeX + s.offset * s.normal;

        uint nearest = 0;
        float bestAbsDist = abs(ShapeSd(p, torii[0]));
        for (uint i = 1; i < nTorii; i++) {
            float ad = abs(ShapeSd(p, torii[i]));
            if (ad < bestAbsDist) { bestAbsDist = ad; nearest = i; }
        }

        float3 canonicalOutward;
        float3x3 canonicalA = AnalyticShapeTensor(p, torii[nearest], canonicalOutward);
        float flip = (dot(s.normal, canonicalOutward) >= 0.0) ? 1.0 : -1.0;
        s.A = flip * canonicalA;
    }

    return s;
}

[RootSignature(QuadricSeedSig)]
[numthreads(THREADS_X, 1, 1)]
void quadricSeedCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    uint totalCount = 2u * LatticeNodeCount;
    if (index >= totalCount)
        return;

    uint3 c;
    uint sub;
    DecodeIndex(index, c, sub);
    float3 nodeX = NodePosition(c, sub);

    SparseNodeSeeds seeds = Seeds[index];

    PackedNode4 packed;
    EncodeSlot(packed, 0, SeedSlot(nodeX, seeds.labels.x, seeds.dists.x, seeds.seeds.x));
    EncodeSlot(packed, 1, SeedSlot(nodeX, seeds.labels.y, seeds.dists.y, seeds.seeds.y));
    EncodeSlot(packed, 2, SeedSlot(nodeX, seeds.labels.z, seeds.dists.z, seeds.seeds.z));
    EncodeSlot(packed, 3, SeedSlot(nodeX, seeds.labels.w, seeds.dists.w, seeds.seeds.w));

    OriginalNodes[index] = packed;
    CurrentNodes[index] = packed;
}
