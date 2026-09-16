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
// The 3 raw values [out,capA,capB] (index == label) rank winner (own label
// -> phi) and runner-up (routed label -> beta) directly by true magnitude;
// the remaining third is stored EXPLICITLY as gamma -- no normalization, no
// reciprocal or exponential correction of any kind needed anymore, since
// there's now a real per-node slot for it (CornerR3WayValue's explicit-gamma
// overload, DistanceLattice.hlsli). This REPLACES the old "derive gamma from
// phi/beta via a formula" scheme (first reciprocal, later exponential
// soft-min) for this shader -- both were only ever needed because gamma
// wasn't tracked as its own value; storing it directly makes the whole
// normalization question moot.
// ZeroBeta (TestShape_ClippedSpheresZeroBeta): same label/phi/gamma/
// discriminator as the normal scene -- ONLY the stored beta is forced to 0
// instead of the true raw[second] -- isolating whether the ROUTING structure
// alone (same discriminator, same "which label is second") helps at all if
// the stored VALUE isn't the correct one, vs. the normal scene's exact
// reconstruction. See the chat derivation for why plain -phi (what a zeroed
// beta degenerates the "else" role to) is measurably wrong here even on
// ordinary-looking 2-label boundaries, not just at the triple line.
#define BuildAnalyticClippedSpheresSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "CBV(b1)," \
    "CBV(b2)"

cbuffer ModeConsts : register(b0) {
    uint ZeroBeta; // 0 = normal (beta = true raw[second]), 1 = force beta = 0 everywhere
};

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential      : register(u1);
RWStructuredBuffer<float> NodeAlienPotential : register(u2);
RWStructuredBuffer<uint>  NodeDiscriminator  : register(u3);
RWStructuredBuffer<float> NodeGamma          : register(u4);

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
    // NOT -max(capA,capB): capA/capB are already clipped flush to the plane
    // (each is exactly 0 wherever planeD==0, regardless of how deep inside
    // the sphere the point is -- correct for THEM, since that flat cut IS
    // their own cap boundary). But "outside" means distance from the WHOLE
    // union of the two spheres, whose actual boundary is only the two outer
    // spherical surfaces -- the plane is an INTERNAL seam between the caps,
    // not part of the union's boundary at all. Using capA/capB here made
    // "outside" spuriously tie with both caps at 0 along the entire plane
    // inside the lens (deep inside both spheres, nowhere near the real
    // exterior), which the tie-break resolved to label 0 -- exactly the
    // "label 0 in the middle of the intersection" bug. The standard
    // uncapped-union-complement SDF fixes it: strongly negative (not
    // outside) deep in the lens, while still correctly handed to whichever
    // sphere's own "far sliver" (past the plane, outside the other sphere)
    // it belongs to, same as before.
    float outside = -max(sdA, sdB);

    // Clamp each of the 3 raw SDFs (both directions) before biasing -- the
    // "outside" field especially is otherwise unbounded far from either
    // sphere, dragging phi/beta to arbitrarily large magnitudes purely as a
    // function of domain size rather than anything geometrically local.
    outside = clamp(outside, -ClampDistance, ClampDistance);
    capA = clamp(capA, -ClampDistance, ClampDistance);
    capB = clamp(capB, -ClampDistance, ClampDistance);

    float raw[3] = { outside, capA, capB }; // index == label (0=background,1=A,2=B)

    // Rank by true magnitude directly -- no shared correction needed at all
    // anymore, since the remaining (third) value gets its own stored slot.
    uint winner = 0;
    if (raw[1] > raw[winner]) winner = 1;
    if (raw[2] > raw[winner]) winner = 2;
    uint second = (winner == 0) ? 1u : 0u;
    for (uint k = 1; k < 3u; k++) if (k != winner && raw[k] > raw[second]) second = k;
    uint third = 3u - winner - second; // the one remaining label index (0+1+2=3)

    NodeCandidateLabel[node * 2u + 0u] = winner & 0xFFu;
    NodePotential[node * MAX_CANDIDATES + 0u] = raw[winner];
    NodeAlienPotential[node] = (ZeroBeta != 0u) ? 0.0 : raw[second];
    NodeGamma[node] = raw[third];
    NodeDiscriminator[node] = EncodeDiscriminator(true, second); // discriminator now stores the routed label directly, no bit-search needed
}
