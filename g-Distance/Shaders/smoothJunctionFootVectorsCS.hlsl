#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// One-shot (manually triggered, 'S' key / "Smooth Junction Footvectors"
// button -- see DistanceApp.h), STEP 2 of the junction-footvector feature:
// reprojects each node with a valid raw footvector (extractJunctionFootVectorsCS.hlsl)
// onto a local cubic curve fit through nearby valid footpoints -- the g-Retam-
// style "fit a cubic to the footpoints in the stencil, find the closest point
// on the cubic" smoothing. A node with NO raw footvector stays invalid (no
// rescuing from neighbors -- matches the explicit narrow-band scoping); a
// node with too few nearby valid footpoints to even pin down a cubic's 4
// coefficients keeps its raw footvector unchanged rather than force a
// degenerate fit.
//
// Neighbor set: the same 18 (of 26) non-diagonal same-sublattice offsets +
// 8 cross-sublattice offsets gatherAlienDiscriminatorCS.hlsl already walks,
// plus this node's own raw footpoint -- up to 27 samples.
//
// Parametrization: project each collected footPOINT onto this node's own
// stored tangent direction (NodeJunctionTangent, from step 1) rather than
// computing a fresh PCA/eigen-decomposition here. The two transverse
// coordinates (in the plane perpendicular to that tangent) are each fit as
// an independent cubic in the axial parameter t via ordinary least squares.
//
// IMPORTANT (found the hard way): a first version of this shader collected
// all samples into three separate MAX_FIT_SAMPLES(27)-entry arrays and then
// solved two independent 4x4 systems via a function taking 2D arrays by
// value, called once per right-hand side. That combination of several large
// simultaneously-live dynamically-indexed local arrays overflowed the GPU's
// indexable-constant-bank budget -- confirmed via an Aftermath crash dump
// ("Invalid Const Address LDC Error... Constant bank beyond the supported
// range"), not a logic bug or an actual infinite loop. Fix: STREAM each
// sample directly into a running normal-equations accumulator as it's
// gathered (never store the sample list at all), and solve both right-hand
// sides in a single combined Gauss-Jordan pass on one flattened 1D array --
// the only dynamically-indexed local storage in the whole kernel is that one
// 24-float array (4 rows x 6 cols: AtA + both RHS columns), comfortably
// within the same scale as already-proven local arrays elsewhere in this
// codebase (e.g. GatherIncidentTets' 32-element array).
//
// Closest point on the fitted curve: in the orthonormal (tangent,e1,e2)
// frame centered at this node, a curve point at parameter t is exactly
// (t, U(t), V(t)), so the squared distance to minimize is t^2+U(t)^2+V(t)^2
// (Pythagorean -- NOT just U^2+V^2, the axial offset matters too). Solved by
// a small fixed number of Newton iterations on the derivative
// f(t)=t+U*U'+V*V'=0, starting from this node's own raw parameter value
// (already close).
#define SmoothJunctionFootVectorsSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<float4> NodeJunctionFootVector       : register(u0); // read: raw (step 1)
RWStructuredBuffer<float3> NodeJunctionTangent          : register(u1); // read: raw tangent (step 1)
RWStructuredBuffer<float4> NodeJunctionFootVectorSmooth : register(u2); // write

// Cross-sublattice ("cross", true BCC nearest-neighbor) node-index
// resolution -- same offset construction as gatherAlienDiscriminatorCS.hlsl's
// CrossNeighborLabelPot, but returning an index+validity instead of a
// label/phi fallback: a nonexistent virtual neighbor has no real footpoint
// at all, so it must be skipped, not faked as background data.
void CrossNeighborIndex(uint3 idx, bool isB, uint c, out uint ref, out bool valid)
{
    int3 d = int3(c & 1u, (c >> 1u) & 1u, (c >> 2u) & 1u);
    if (isB)
    {
        uint3 aIdx = idx + uint3(d);
        ref = AIdx(aIdx.x, aIdx.y, aIdx.z);
        valid = true;
        return;
    }
    int3 bIdx = (int3)idx + d - 1;
    if (all(bIdx >= 0) && all(bIdx < (int)BDim))
    {
        ref = BIdx((uint)bIdx.x, (uint)bIdx.y, (uint)bIdx.z);
        valid = true;
        return;
    }
    ref = 0;
    valid = false;
}

float3 OrthoBasisE1(float3 d)
{
    float3 refv = (abs(d.x) < 0.9) ? float3(1, 0, 0) : float3(0, 1, 0);
    return normalize(refv - d * dot(refv, d));
}

// Folds one (t,U,V) sample directly into the running 4x6 row-major
// normal-equations accumulator (flattened: index = row*6+col; cols 0-3 =
// AtA, col 4 = AtU, col 5 = AtV) -- every index here is a COMPILE-TIME
// constant (0..23), so this needs no indexable addressing at all, just
// plain scalar reads/writes into the caller's array.
void AccumulateSample(inout float AtA[24], float t, float U, float V)
{
    float r0 = 1.0, r1 = t, r2 = t * t, r3 = t * t * t;
    AtA[0]  += r0 * r0; AtA[1]  += r0 * r1; AtA[2]  += r0 * r2; AtA[3]  += r0 * r3; AtA[4]  += r0 * U; AtA[5]  += r0 * V;
    AtA[6]  += r1 * r0; AtA[7]  += r1 * r1; AtA[8]  += r1 * r2; AtA[9]  += r1 * r3; AtA[10] += r1 * U; AtA[11] += r1 * V;
    AtA[12] += r2 * r0; AtA[13] += r2 * r1; AtA[14] += r2 * r2; AtA[15] += r2 * r3; AtA[16] += r2 * U; AtA[17] += r2 * V;
    AtA[18] += r3 * r0; AtA[19] += r3 * r1; AtA[20] += r3 * r2; AtA[21] += r3 * r3; AtA[22] += r3 * U; AtA[23] += r3 * V;
}

// Gauss-Jordan with partial pivoting on the flattened 4x6 augmented matrix --
// solves BOTH right-hand sides (cols 4,5) in one pass. The pivot row `piv`
// is the only genuinely data-dependent (non-compile-time-constant) index in
// this whole shader -- everything else above is constant-indexed and should
// scalarize away entirely.
bool Solve4x2(inout float M[24], out float x1[4], out float x2[4])
{
    [unroll]
    for (uint col = 0; col < 4; col++)
    {
        uint piv = col;
        float best = abs(M[col * 6 + col]);
        [unroll]
        for (uint r2 = col + 1; r2 < 4; r2++)
        {
            float v = abs(M[r2 * 6 + col]);
            if (v > best) { best = v; piv = r2; }
        }
        if (best < 1.0e-9)
        {
            x1[0] = 0.0; x1[1] = 0.0; x1[2] = 0.0; x1[3] = 0.0;
            x2[0] = 0.0; x2[1] = 0.0; x2[2] = 0.0; x2[3] = 0.0;
            return false;
        }
        if (piv != col)
        {
            [unroll]
            for (uint c2 = 0; c2 < 6; c2++)
            {
                float tmp = M[col * 6 + c2]; M[col * 6 + c2] = M[piv * 6 + c2]; M[piv * 6 + c2] = tmp;
            }
        }
        float pv = M[col * 6 + col];
        [unroll]
        for (uint c3 = 0; c3 < 6; c3++) M[col * 6 + c3] /= pv;
        [unroll]
        for (uint r3 = 0; r3 < 4; r3++)
        {
            if (r3 == col) continue;
            float f = M[r3 * 6 + col];
            if (f == 0.0) continue;
            [unroll]
            for (uint c4 = 0; c4 < 6; c4++) M[r3 * 6 + c4] -= f * M[col * 6 + c4];
        }
    }
    x1[0] = M[0 * 6 + 4]; x1[1] = M[1 * 6 + 4]; x1[2] = M[2 * 6 + 4]; x1[3] = M[3 * 6 + 4];
    x2[0] = M[0 * 6 + 5]; x2[1] = M[1 * 6 + 5]; x2[2] = M[2 * 6 + 5]; x2[3] = M[3 * 6 + 5];
    return true;
}

[RootSignature(SmoothJunctionFootVectorsSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void smoothJunctionFootVectorsCS(uint3 dtid : SV_DispatchThreadID)
{
    uint node = dtid.x;
    if (node >= NodeCount) return;

    float4 myRaw = NodeJunctionFootVector[node];
    if (myRaw.w < 0.5)
    {
        NodeJunctionFootVectorSmooth[node] = float4(0, 0, 0, 0);
        return;
    }

    float3 nodePos = NodeWorldPos(node);
    float3 tangent = NodeJunctionTangent[node];
    float tanLen = length(tangent);
    if (tanLen < 1.0e-8)
    {
        // No usable tangent to parametrize against -- keep raw unchanged.
        NodeJunctionFootVectorSmooth[node] = myRaw;
        return;
    }
    tangent /= tanLen;
    float3 e1 = OrthoBasisE1(tangent);
    float3 e2 = cross(tangent, e1);

    float t0raw = dot(myRaw.xyz, tangent);

    float AtA[24];
    [unroll]
    for (uint i = 0; i < 24; i++) AtA[i] = 0.0;

    uint sampleCount = 0;
    AccumulateSample(AtA, t0raw, dot(myRaw.xyz, e1), dot(myRaw.xyz, e2));
    sampleCount++;

    bool isB; uint3 idx;
    DecodeNodeIndex(node, isB, idx);

    [unroll]
    for (uint n = 0; n < 26u; n++)
    {
        int3 off = SameLatticeOffsets[n];
        int nnz = (off.x != 0 ? 1 : 0) + (off.y != 0 ? 1 : 0) + (off.z != 0 ? 1 : 0);
        if (nnz == 3) continue; // same-sublattice diagonal -- not part of this stencil, see the cross-tap loop instead

        int3 nb = (int3)idx + off;
        bool valid = isB
            ? (all(nb >= 0) && all(nb < (int)BDim))
            : (all(nb >= 0) && all(nb < (int)GridRes));
        if (!valid) continue;
        uint ref = isB ? BIdx((uint)nb.x, (uint)nb.y, (uint)nb.z) : AIdx((uint)nb.x, (uint)nb.y, (uint)nb.z);

        float4 nRaw = NodeJunctionFootVector[ref];
        if (nRaw.w < 0.5) continue;

        float3 nPos = NodeWorldPos(ref);
        float3 rel = (nPos + nRaw.xyz) - nodePos;
        AccumulateSample(AtA, dot(rel, tangent), dot(rel, e1), dot(rel, e2));
        sampleCount++;
    }

    [unroll]
    for (uint c = 0; c < 8u; c++)
    {
        uint ref; bool valid;
        CrossNeighborIndex(idx, isB, c, ref, valid);
        if (!valid) continue;

        float4 nRaw = NodeJunctionFootVector[ref];
        if (nRaw.w < 0.5) continue;

        float3 nPos = NodeWorldPos(ref);
        float3 rel = (nPos + nRaw.xyz) - nodePos;
        AccumulateSample(AtA, dot(rel, tangent), dot(rel, e1), dot(rel, e2));
        sampleCount++;
    }

    if (sampleCount < 4u)
    {
        NodeJunctionFootVectorSmooth[node] = myRaw;
        return;
    }

    // Tikhonov regularization on AtA's diagonal (cols/rows 0-3) -- same
    // fallback shape as the browser-prototype artifacts this design was
    // worked out against, so a thin/near-collinear neighborhood degrades
    // gracefully instead of the solve going singular.
    const float kReg = 1.0e-4;
    AtA[0 * 6 + 0] += kReg;
    AtA[1 * 6 + 1] += kReg;
    AtA[2 * 6 + 2] += kReg;
    AtA[3 * 6 + 3] += kReg;

    float coefU[4];
    float coefV[4];
    bool ok = Solve4x2(AtA, coefU, coefV);
    if (!ok)
    {
        NodeJunctionFootVectorSmooth[node] = myRaw;
        return;
    }

    float t0 = t0raw; // this node's own raw parameter -- the Newton start
    [unroll]
    for (uint iter = 0; iter < 6u; iter++)
    {
        float t = t0;
        float t2 = t * t, t3 = t2 * t;
        float U = coefU[0] + coefU[1] * t + coefU[2] * t2 + coefU[3] * t3;
        float V = coefV[0] + coefV[1] * t + coefV[2] * t2 + coefV[3] * t3;
        float dU = coefU[1] + 2.0 * coefU[2] * t + 3.0 * coefU[3] * t2;
        float dV = coefV[1] + 2.0 * coefV[2] * t + 3.0 * coefV[3] * t2;
        float ddU = 2.0 * coefU[2] + 6.0 * coefU[3] * t;
        float ddV = 2.0 * coefV[2] + 6.0 * coefV[3] * t;

        // Minimize t^2+U(t)^2+V(t)^2 (the true squared distance in this
        // orthonormal frame): f(t)=t+U*U'+V*V', f'(t)=1+U'^2+U*U''+V'^2+V*V''.
        float f = t + U * dU + V * dV;
        float fp = 1.0 + dU * dU + U * ddU + dV * dV + V * ddV;
        if (abs(fp) < 1.0e-9) break;
        t0 = t0 - f / fp;
    }

    float t2f = t0 * t0, t3f = t2f * t0;
    float Uf = coefU[0] + coefU[1] * t0 + coefU[2] * t2f + coefU[3] * t3f;
    float Vf = coefV[0] + coefV[1] * t0 + coefV[2] * t2f + coefV[3] * t3f;

    float3 smoothedFoot = nodePos + t0 * tangent + Uf * e1 + Vf * e2;
    NodeJunctionFootVectorSmooth[node] = float4(smoothedFoot - nodePos, 1.0);
}
