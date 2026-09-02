#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b1
#include "DistanceLattice.hlsli"
#define CLIPPED_SPHERES_CB_REGISTER b2
#include "ClippedSpheresCb.hlsli"

// TestShape_ClippedSpheres analytic init (see DistanceApp.h / ClippedSpheresCb.hlsli):
// two overlapping spheres, each clipped by their radical plane (the plane
// through their intersection circle) into disjoint "caps" -- exactly 3
// regions meeting along that circle, all THREE ground-truth signed
// distances known in closed form everywhere. Writes label/phi/beta/
// discriminator directly from those 3 SDFs -- no Jacobi solve, no
// gatherAlienDiscriminatorCS heuristic -- so the (label,phi,beta,discrim)
// representation gets to reproduce a REAL curved triple junction exactly,
// with nothing to blame on solver convergence. This shader REPLACES
// rasterLabelCS/the JFA pass/buildCandidatesCS/buildSyntheticBCS entirely
// for this one test shape -- see RunTopologyBuild's early-out branch.
//
// Insides positive, per the approved plan:
//   sdA   = RadiusA - |p-CenterA|         (sphere A, uncapped)
//   sdB   = RadiusB - |p-CenterB|         (sphere B, uncapped)
//   capA  = min(sdA, -planeD)             (inside A AND on A's side of the radical plane)
//   capB  = min(sdB,  planeD)             (inside B AND on B's side)
//   out   = -max(capA, capB)              (outside both caps)
// where planeD is the signed distance to the radical plane (>0 toward B,
// the plane through the intersection circle where a point is equidistant
// in "power" from both spheres -- the standard construction that makes
// capA/capB disjoint by construction, regardless of how the raw lens
// (sphere A ∩ sphere B) straddles it). Each of these 3 is then clamped to
// +-ClippedSpheresCb's ClampDistance (a "Reinit param" GUI slider) before
// biasing -- "out" especially is otherwise unbounded far from both spheres.
//
// The 3 raw values [out,capA,capB] (index == label) are re-centered to sum
// to exactly zero (same invariant as CornerR's own phi+beta+(-phi-beta)=0)
// before picking the winner (own label -> phi), runner-up (routed label ->
// beta), leaving the third to reconstruct exactly as -phi-beta downstream
// (raymarchLatticePS.hlsl's CornerR).
// ZeroBeta (TestShape_ClippedSpheresZeroBeta): same label/phi/discriminator
// as the normal scene -- ONLY the stored beta is forced to 0 instead of the
// true biased[second] -- isolating whether the ROUTING structure alone
// (same discriminator, same "which label is second") helps at all if the
// stored VALUE isn't the correct one, vs. the normal scene's exact
// reconstruction. See the chat derivation for why plain -phi (what a zeroed
// beta degenerates the "else" role to) is measurably wrong here even on
// ordinary-looking 2-label boundaries, not just at the triple line.
#define BuildAnalyticClippedSpheresSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b1)," \
    "CBV(b2)"

cbuffer ModeConsts : register(b0) {
    uint ZeroBeta; // 0 = normal (beta = true biased[second]), 1 = force beta = 0 everywhere
};

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential      : register(u1);
RWStructuredBuffer<float> NodeAlienPotential : register(u2);
RWStructuredBuffer<uint>  NodeDiscriminator  : register(u3);

[RootSignature(BuildAnalyticClippedSpheresSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void buildAnalyticClippedSpheresCS(uint3 dtid : SV_DispatchThreadID)
{
    uint node = dtid.x;
    if (node >= NodeCount) return;

    bool isB; uint3 idx;
    DecodeNodeIndex(node, isB, idx);
    float3 p = isB ? BPos((int3)idx) : APos((int3)idx);

    float sdA = RadiusA - length(p - CenterA);
    float sdB = RadiusB - length(p - CenterB);

    float3 diff = CenterB - CenterA;
    float d = length(diff);
    float3 n = diff / max(d, 1.0e-6);
    float t = (d * d + RadiusA * RadiusA - RadiusB * RadiusB) / (2.0 * max(d, 1.0e-6));
    float3 P0 = CenterA + n * t;
    float planeD = dot(p - P0, n); // >0 towards B's side, <0 towards A's side

    float capA = min(sdA, -planeD);
    float capB = min(sdB, planeD);
    float outside = -max(capA, capB);

    // Clamp each of the 3 raw SDFs (both directions) before biasing -- the
    // "outside" field especially is otherwise unbounded far from either
    // sphere, dragging phi/beta to arbitrarily large magnitudes purely as a
    // function of domain size rather than anything geometrically local.
    outside = clamp(outside, -ClampDistance, ClampDistance);
    capA = clamp(capA, -ClampDistance, ClampDistance);
    capB = clamp(capB, -ClampDistance, ClampDistance);

    float raw[3] = { outside, capA, capB }; // index == label (0=background,1=A,2=B)
    float mean = (1.0/raw[0] + 1.0/raw[1] + 1.0/raw[2]) / 3.0;
    float biased[3] = { 1.0/(1.0/raw[0] - mean), 1.0/(1.0/raw[1] - mean), 1.0/(1.0/raw[2] - mean) };

    uint winner = 0;
    if (biased[1] > biased[winner]) winner = 1;
    if (biased[2] > biased[winner]) winner = 2;
    uint second = (winner == 0) ? 1u : 0u;
    for (uint k = 1; k < 3u; k++) if (k != winner && biased[k] > biased[second]) second = k;
    NodeCandidateLabel[node * 2u + 0u] = winner & 0xFFu;
    NodePotential[node * MAX_CANDIDATES + 0u] = biased[winner];
    NodeAlienPotential[node] = (ZeroBeta != 0u) ? 0.0 : biased[second];
    NodeDiscriminator[node] = EncodeDiscriminator(true, second); // discriminator now stores the routed label directly, no bit-search needed
}
