#pragma once

#include "QuadricOptimizationCb.hlsli"
#include "SparseLabelSeed.hlsli"

// Shared types/helpers for the quadric footpoint-field optimization -- split
// across quadricSeedCS.hlsl, CSOffsetGaussNewton.hlsl, CSNormalRotation.hlsl,
// CSTransportTensor.hlsl, CSTensorGradient.hlsl, quadricRecoverCS.hlsl --
// plus the K-JFA sparse label seeding that feeds it (SparseLabelSeed.hlsli,
// sparseLabelSeedCS.hlsl/sparseLabelHarvestCS.hlsl).
//
// BCC layout:
//   [0, LatticeNodeCount)                    : corner sublattice (s = 0)
//   [LatticeNodeCount, 2*LatticeNodeCount)   : body-center sublattice (s = 1)
// A body-center node (x,y,z,1) is at GridOrigin + CellSize * ((x,y,z) + 0.5).
//
// Each node carries SparseLabelCount (4) label "slots" (see
// SparseLabelSeed.hlsli) -- WHICH label occupies which slot is fixed forever
// at seed time and lives in the (read-only, from then on) sparseNodeSeeds
// buffer, not here. This header only describes each slot's MUTABLE
// relaxation state: a persistent unit normal that always points OUTWARD --
// from inside this slot's label's territory, through the boundary, into
// open space -- honestly and consistently regardless of which side of that
// boundary a given node sits on (never re-derived from a vector that can
// flip sign -- see quadricSeedCS.hlsl/CSOffsetGaussNewton.hlsl/
// CSNormalRotation.hlsl), a signed
// scalar offset along it (footpoint = nodeX + offset*normal, offset can go
// negative smoothly, no discontinuity), a confidence, and a symmetric 3x3
// curvature tensor. Two neighboring nodes are only ever compared slot-to-slot
// when they track the SAME label -- no "own vs. other"/reversed-pair
// reconciliation needed, since a label's normal convention is global and
// consistent by construction, not derived from which side of it a node is on.
//
// Storage is explicitly FP16, packed into uints. Arithmetic is FP32 because
// the per-node Gauss-Newton solves are usually too fragile in native FP16.

#define THREADS_X 64
#define INVALID_INDEX 0xffffffffu

struct LabelSlotState
{
    float3   normal;
    float    offset;
    float    confidence;
    float3x3 A;
};

// Packed per-slot: 3 half4 (normal.xyz+offset, Axx/Ayy/Azz/Axy,
// Axz/Ayz/confidence/pad) -- same footprint per slot as the old single-slot
// PackedNode this replaces. 4 slots per node, named rather than arrayed to
// avoid dynamic struct-member indexing (see DecodeSlot/EncodeSlot below).
struct PackedNode4
{
    uint2 slot0a, slot0b, slot0c;
    uint2 slot1a, slot1b, slot1c;
    uint2 slot2a, slot2b, slot2c;
    uint2 slot3a, slot3b, slot3c;
};

// -----------------------------------------------------------------------------
// Packing
// -----------------------------------------------------------------------------

float2 UnpackHalf2(uint p)
{
    return float2(f16tof32(p & 0xffffu), f16tof32(p >> 16u));
}

float4 UnpackHalf4(uint2 p)
{
    return float4(UnpackHalf2(p.x), UnpackHalf2(p.y));
}

uint PackHalf2(float2 v)
{
    uint lo = f32tof16(v.x) & 0xffffu;
    uint hi = f32tof16(v.y) & 0xffffu;
    return lo | (hi << 16u);
}

uint2 PackHalf4(float4 v)
{
    return uint2(PackHalf2(v.xy), PackHalf2(v.zw));
}

LabelSlotState DecodeSlotRaw(uint2 a, uint2 b, uint2 c)
{
    float4 fa = UnpackHalf4(a);
    float4 fb = UnpackHalf4(b);
    float4 fc = UnpackHalf4(c);

    LabelSlotState s;
    s.normal = fa.xyz;
    s.offset = fa.w;
    s.confidence = fc.z;
    s.A = float3x3(
        fb.x, fb.w, fc.x,
        fb.w, fb.y, fc.y,
        fc.x, fc.y, fb.z);
    return s;
}

void EncodeSlotRaw(LabelSlotState s, out uint2 a, out uint2 b, out uint2 c)
{
    a = PackHalf4(float4(s.normal, s.offset));
    b = PackHalf4(float4(s.A[0][0], s.A[1][1], s.A[2][2], s.A[0][1]));
    c = PackHalf4(float4(s.A[0][2], s.A[1][2], s.confidence, 0.0));
}

// Branch-based, not dynamically indexed -- same convention this whole
// project uses for anything shaped like a small fixed-size lookup (e.g.
// SparseLabelSeed's harvest insertion sort).
LabelSlotState DecodeSlot(PackedNode4 n, uint slot)
{
    if (slot == 0) return DecodeSlotRaw(n.slot0a, n.slot0b, n.slot0c);
    if (slot == 1) return DecodeSlotRaw(n.slot1a, n.slot1b, n.slot1c);
    if (slot == 2) return DecodeSlotRaw(n.slot2a, n.slot2b, n.slot2c);
    return DecodeSlotRaw(n.slot3a, n.slot3b, n.slot3c);
}

void EncodeSlot(inout PackedNode4 n, uint slot, LabelSlotState s)
{
    uint2 a, b, c;
    EncodeSlotRaw(s, a, b, c);
    if (slot == 0)      { n.slot0a = a; n.slot0b = b; n.slot0c = c; }
    else if (slot == 1) { n.slot1a = a; n.slot1b = b; n.slot1c = c; }
    else if (slot == 2) { n.slot2a = a; n.slot2b = b; n.slot2c = c; }
    else                { n.slot3a = a; n.slot3b = b; n.slot3c = c; }
}

// -----------------------------------------------------------------------------
// BCC indexing and geometry
// -----------------------------------------------------------------------------

uint Flatten(uint3 c)
{
    return c.x + GridSize.x * (c.y + GridSize.y * c.z);
}

void DecodeIndex(uint index, out uint3 c, out uint sublattice)
{
    sublattice = index >= LatticeNodeCount;
    uint local = index - sublattice * LatticeNodeCount;

    c.x = local % GridSize.x;
    local /= GridSize.x;
    c.y = local % GridSize.y;
    c.z = local / GridSize.y;
}

uint EncodeIndex(int3 c, uint sublattice)
{
    if (any(c < 0) || any(c >= int3(GridSize)))
        return INVALID_INDEX;
    return Flatten(uint3(c)) + sublattice * LatticeNodeCount;
}

float3 NodePosition(uint3 c, uint sublattice)
{
    return GridOrigin + NodeCellSize * (float3(c) + (sublattice ? 0.5 : 0.0));
}

uint BCCNeighborIndex(uint3 c, uint sublattice, uint neighbor)
{
    // Corner -> centers of the 8 incident cells: offsets in {-1,0}^3.
    // Center -> its 8 corner vertices: offsets in {0,1}^3.
    int3 bits = int3(neighbor & 1u, (neighbor >> 1u) & 1u, (neighbor >> 2u) & 1u);
    int3 offset = sublattice ? bits : (bits - 1);
    return EncodeIndex(int3(c) + offset, 1u - sublattice);
}

// Same-sublattice neighbors beyond BCCNeighborIndex's 8 (always the OTHER
// sublattice, making that graph alone bipartite) now come from
// bccCommon.hlsli's SameLatticeOffsets[26], scaled by a per-iteration jump
// step directly at each call site (see CSOffsetGaussNewton.hlsl/
// CSNormalRotation.hlsl/CSTensorGradient.hlsl) -- a fixed-step, 6-neighbor
// version of this used to live here, replaced once same-label matching plus
// FootDistanceWeight made longer, variable-length reach both meaningful and
// safe.

float3 SlotFootpoint(float3 nodeX, LabelSlotState slot)
{
    return nodeX + slot.offset * slot.normal;
}

// Returns the slot label at a fixed slot index (branch-based, no dynamic
// vector indexing -- see DecodeSlot/EncodeSlot above).
uint GetSlotLabel(SparseNodeSeeds seeds, uint slot)
{
    if (slot == 0) return seeds.labels.x;
    if (slot == 1) return seeds.labels.y;
    if (slot == 2) return seeds.labels.z;
    return seeds.labels.w;
}

// Finds which of a node's (up to SparseLabelCount) slots tracks `label`, or
// SparseLabelCount if it doesn't track that label at all -- this is the ONLY
// thing that gates whether two neighboring nodes' slots are ever compared:
// no "own vs. other"/reversed-pair reconciliation needed anymore, since
// matching by label identity is unambiguous by construction.
uint FindSlotForLabel(SparseNodeSeeds seeds, uint label)
{
    if (seeds.labels.x == label) return 0;
    if (seeds.labels.y == label) return 1;
    if (seeds.labels.z == label) return 2;
    if (seeds.labels.w == label) return 3;
    return SparseLabelCount;
}

// -----------------------------------------------------------------------------
// Matrix helpers
// -----------------------------------------------------------------------------

float3x3 Identity3()
{
    return float3x3(1,0,0, 0,1,0, 0,0,1);
}

float3x3 Outer(float3 a, float3 b)
{
    return float3x3(
        a.x*b.x, a.x*b.y, a.x*b.z,
        a.y*b.x, a.y*b.y, a.y*b.z,
        a.z*b.x, a.z*b.y, a.z*b.z);
}

float3x3 Symmetrize(float3x3 A)
{
    return 0.5 * (A + transpose(A));
}

float FrobeniusSquared(float3x3 A)
{
    return dot(A[0], A[0]) + dot(A[1], A[1]) + dot(A[2], A[2]);
}

float3x3 ClampFrobenius(float3x3 A)
{
    float f2 = FrobeniusSquared(A);
    float max2 = MaxTensorFrobenius * MaxTensorFrobenius;
    if (f2 > max2 && max2 > 0.0)
        A *= MaxTensorFrobenius * rsqrt(f2);
    return A;
}

float3x3 ProjectTensorToNormal(float3x3 A, float3 n)
{
    A = Symmetrize(A);
    float3x3 P = Identity3() - Outer(n, n);
    float mu = dot(n, mul(A, n));
    return Symmetrize(mul(P, mul(A, P)) + mu * Outer(n, n));
}

float Determinant3(float3x3 M)
{
    return dot(M[0], cross(M[1], M[2]));
}

bool SolveSPD3(float3x3 H, float3 rhs, out float3 x)
{
    // Adjugate solve. H is regularized before this function is called.
    float3 c0 = cross(H[1], H[2]);
    float3 c1 = cross(H[2], H[0]);
    float3 c2 = cross(H[0], H[1]);
    float det = dot(H[0], c0);

    if (abs(det) < 1.0e-12)
    {
        x = 0.0;
        return false;
    }

    // Rows of inverse are cofactors divided by determinant for row-major H.
    float3x3 invH = transpose(float3x3(c0, c1, c2)) / det;
    x = mul(invH, rhs);
    return all(isfinite(x));
}

bool SolveSPD2(float2x2 H, float2 rhs, out float2 x)
{
    float det = H[0][0] * H[1][1] - H[0][1] * H[1][0];
    if (abs(det) < 1.0e-12)
    {
        x = 0.0;
        return false;
    }
    float2x2 invH = float2x2(H[1][1], -H[0][1], -H[1][0], H[0][0]) / det;
    x = mul(invH, rhs);
    return all(isfinite(x));
}

float3x3 MinimalRotation(float3 fromN, float3 toN)
{
    float c = clamp(dot(fromN, toN), -1.0, 1.0);
    float3 v = cross(fromN, toN);
    float s2 = dot(v, v);

    if (s2 < 1.0e-12)
    {
        if (c > 0.0)
            return Identity3();

        // 180 degree rotation around a stable axis perpendicular to fromN.
        float3 helper = (abs(fromN.x) < 0.8) ? float3(1,0,0) : float3(0,1,0);
        float3 axis = normalize(cross(fromN, helper));
        return 2.0 * Outer(axis, axis) - Identity3();
    }

    float3x3 K = float3x3(
         0.0, -v.z,  v.y,
         v.z,  0.0, -v.x,
        -v.y,  v.x,  0.0);

    // Rodrigues in the form R = I + K + K^2/(1+c), where v = from x to.
    return Identity3() + K + mul(K, K) / max(1.0 + c, 1.0e-6);
}

// An orthonormal basis for the plane tangent to n (arbitrary but
// deterministic in-plane orientation -- only used as a scratch frame to
// parametrize a small rotation of n, so its absolute orientation doesn't
// matter, only that it's perpendicular to n).
void TangentBasis(float3 n, out float3 t1, out float3 t2)
{
    float3 helper = (abs(n.x) < 0.8) ? float3(1, 0, 0) : float3(0, 1, 0);
    t1 = normalize(cross(n, helper));
    t2 = cross(n, t1);
}

// -----------------------------------------------------------------------------
// Quadric evaluation and robust weights
// -----------------------------------------------------------------------------

// Q(x) = normal.r + 0.5 r^T A r (r = queryPoint - evaluatorPoint), evaluated
// using evaluator's OWN (normal, A) -- unlike the single-slot scheme this
// replaces, evaluator.normal is never re-derived from a foot vector, so it's
// always the same honest, persistent direction regardless of which side of
// its own boundary the evaluator currently sits on.
void EvaluateSlotQuadric(LabelSlotState evaluator, float3 evaluatorPoint, float3 queryPoint,
                          out float residual, out float3 gradient)
{
    float3 r = queryPoint - evaluatorPoint;
    gradient = evaluator.normal + mul(evaluator.A, r);
    residual = 0.5 * dot(r, gradient + evaluator.normal);

    residual = clamp(residual, -MaxResidual, MaxResidual);
    if (UseResidualNormalization != 0u)
        residual /= max(length(gradient), 1.0e-3);
}

// Normal-alignment shaping -- OFF by default (NormalPower<=0 -> normalW==1,
// unconditionally permissive). See FootDistanceWeight below for why: with
// same-label matching, two neighbors are already guaranteed to describe the
// same physical boundary, so normal disagreement is no longer a "these are
// unrelated" signal -- it's curvature information a thin-tube/small-island
// fit needs, not something to gate on. MinNormalDot is still a genuine hard
// reject on the RAW (unfloored) dot product if ever wanted again (e.g. for
// future crease-preservation); NormalPower's shaping remaps dot(ni,nj) from
// [-1,1] to [0,1] before the pow() so it stays well-defined for anti-
// parallel normals too, unlike the old max(dot,0)-then-pow formula (which
// forced anti-parallel/orthogonal neighbors to exactly zero regardless of
// any threshold, making "loosen the threshold" alone not actually loosen
// anything).
float NormalAlignmentWeight(float3 ni, float3 nj)
{
    float nd = dot(ni, nj); // raw, in [-1,1] -- no floor
    if (nd < MinNormalDot)
        return 0.0;
    if (NormalPower <= 0.0)
        return 1.0;
    return pow(saturate(0.5 * (nd + 1.0)), NormalPower);
}

float NeighborWeight(float confidenceI, float confidenceJ, float3 ni, float3 nj, float residual)
{
    float normalW = NormalAlignmentWeight(ni, nj);
    float x = residual / max(ResidualScale, 1.0e-4);
    float robustW = rcp(1.0 + x*x); // Cauchy IRLS weight.
    return max(confidenceI, 0.0) * max(confidenceJ, 0.0) * normalW * robustW;
}

// Soft footpoint-to-footpoint distance gate: 1 within FootGateNear, smoothly
// fading to 0 by FootGateFar. This -- not normal alignment -- is now the
// primary relevance filter for whether two footpoints (same-label
// neighbors, or same-node cross-label slots) should influence each other:
// what a quadric genuinely can't do is describe two points that are just
// far apart, regardless of their normals.
float FootDistanceWeight(float3 pa, float3 pb)
{
    float d = distance(pa, pb);
    return 1.0 - smoothstep(FootGateNear, FootGateFar, d);
}
