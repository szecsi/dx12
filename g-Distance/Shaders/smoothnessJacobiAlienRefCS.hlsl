#include "DistanceCb.hlsli"
#define JUNCTION_REF_CB_REGISTER b1
#include "JunctionRefCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"

// Diagnostic "reference fit" for the alien-potential secondary pass -- see
// the approved plan. NOT the shipped Junction+Floor objective
// (smoothnessJacobiAlienCS.hlsl): instead of a self-consistency check
// between two tets' own tangent estimates, this solves beta directly
// against the KNOWN, analytic ground truth of TestShape_TiltedBoxJunction
// (a single world-space plane, JunctionRefCb's PlaneNormal/PlanePoint) --
// answering "can beta represent this junction at all, given the right
// objective", independent of whether the shipped objective's gradient
// actually points anywhere useful.
//
// Tile/halo scaffolding and AlienCornerR are identical to
// smoothnessJacobiAlienCS.hlsl (same frozen gLabel/gPot/gDiscrim, only
// gAlienPot/beta is live). The DISCOVERY loop is simpler than that file's,
// though: no face-adjacent tet-PAIR search is needed here (that machinery
// exists there only to compare two tets' own tangent estimates against each
// other) -- this residual is evaluated per SINGLE tet, directly against the
// external ground truth, so it reuses the simpler one-lane-per-ring1-tet
// pattern smoothnessJacobiSyntheticCS.hlsl's Eikonal term already uses
// (GatherIncidentTets, wid<24, no relation/GetFaceAdjacentPartner loop).
//
// Per ring-1 tet where BOTH label 1 and label 2 (TestShape_TiltedBoxJunction's
// two split labels) are present among its 4 corners (skipped otherwise --
// this is exactly the "near the junction" gate; a tet deep in the box's
// interior, away from the background boundary, never has both AND never has
// beta engaged there anyway, since interior nodes only ever see 1 distinct
// foreign label, so gatherAlienDiscriminatorCS.hlsl never enables their
// discriminator -- consistent with the existing design, not a special case
// added here):
//   gradD = sum_c (AlienCornerR(c,1)-AlienCornerR(c,2)) * w_c  -- current
//     interface gradient (label 1 minus label 2 confidence field).
//   D0 = AlienCornerR(corner0,1)-AlienCornerR(corner0,2) -- current
//     interface value at this tet's corner 0 (the affine anchor point).
//   Rdir = cross(gradD, PlaneNormal) -- wants gradD parallel to the TRUE
//     normal (zero regardless of gradD's magnitude/scale).
//   Rpos = D0 + dot(gradD, PlanePoint-P[0]) -- wants the CURRENT interface
//     to pass through the TRUE plane's own known point.
// Derivative w.r.t. this target's own beta: d1/d2 = 1 iff label 1/2
// respectively routes to THIS target's alien branch (else 0 -- own-label and
// default branches both return frozen phi, contributing 0) -- same
// convention as the shipped shader's da/db/dc, specialized to a single
// difference field instead of the cyclic 3-label T:
//   d(gradD)/dbeta = (d1-d2)*w[myIdx]
//   dRdir/dbeta    = (d1-d2)*cross(w[myIdx], PlaneNormal)
//   dRpos/dbeta    = (d1-d2)*((myIdx==0?1:0) + dot(w[myIdx], PlanePoint-P[0]))
// verified by hand: same product-rule shape as the shipped
// coeffA=(dc-db)*gaA+... derivation.
//
// Final combine: same Jacobi step + the SAME beta<phi*(1-0.1) safety bound
// already shipped (see smoothnessJacobiAlienCS.hlsl) -- this diagnostic must
// not reintroduce the sign-flip rendering-hole bug either.
#define SmoothnessAlienRefSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "CBV(b1)," \
    "CBV(b2)"

RWStructuredBuffer<uint>  NodeCandidateLabel       : register(u0); // frozen label, read
RWStructuredBuffer<float> NodePotential            : register(u1); // frozen phi, read
RWStructuredBuffer<uint>  NodeDiscriminator        : register(u2); // frozen discriminator, read
RWStructuredBuffer<float> NodeAlienPotential       : register(u3); // beta, "current" (Jacobi read buffer)
RWStructuredBuffer<float> NodeAlienPotentialScratch: register(u4); // beta, Jacobi write buffer

#define HALO_DIM 4u
// Same constants as smoothnessJacobiAlienCS.hlsl -- kept identical so the
// two files stay directly comparable; not load-bearing here either (no
// kernelbits table).
#define HALO_BBASE 71u
#define HALO_NODES 135u
#define TARGET_NODES 16u

groupshared uint gLabel[HALO_NODES];
groupshared float gPot[HALO_NODES];
groupshared float gAlienPot[HALO_NODES];
groupshared uint gDiscrim[HALO_NODES];
// This sweep's reference-fit grad/diag contribution per target.
groupshared float gRefGrad[TARGET_NODES];
groupshared float gRefDiag[TARGET_NODES];
// Tightest exact valid interval for this target's beta -- the intersection
// across every 3-distinct-own-label face of every ring-1 tet it's a corner
// of (FaceBetaValidInterval, DistanceLattice.hlsli) -- see
// smoothnessJacobiAlienCS.hlsl's copy of this same mechanism for the full
// derivation. Independent of this file's own label-1/2 residual gate: a
// corner can leak into an unrelated label's territory regardless of
// whether THIS diagnostic's own fit cares about that label.
groupshared float gAlienBetaLower[TARGET_NODES];
groupshared float gAlienBetaUpper[TARGET_NODES];

uint posToIdxA(uint3 l) { return AIdx(l.x, l.y, l.z); }
uint posToIdxB(uint3 l) { return BIdx(l.x, l.y, l.z); }

uint inHaloPosToInHaloIdxA(uint3 l) { return l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }
uint inHaloPosToInHaloIdxB(uint3 l) { return HALO_BBASE + l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }

int ResolveHaloLocalIndex(int3 q, uint3 haloOriginA)
{
    uint globalRef = ResolveCorner(q);
    if (globalRef == SENTINEL_LABEL) return -1;
    bool cIsB; uint3 cIdx;
    DecodeNodeIndex(globalRef, cIsB, cIdx);
    int3 rel = (int3)cIdx - (int3)haloOriginA;
    if (any(rel < 0) || any(rel >= (int)HALO_DIM)) return -1;
    return cIsB ? (int)inHaloPosToInHaloIdxB((uint3)rel) : (int)inHaloPosToInHaloIdxA((uint3)rel);
}

int FindHaloCorner(int halo[4], int key)
{
    for (int c = 0; c < 4; c++) if (halo[c] == key) return c;
    return -1;
}

// Tet corner indices bounding the face OPPOSITE corner index i (0..3) --
// used only by the dynamic beta upper bound scan below, matches
// raymarchLatticePS.hlsl's own FaceCorners convention.
static const uint kFaceCorners[4][3] = { {1,2,3},{0,2,3},{0,1,3},{0,1,2} };

// The 3-way corner rule (own -> phi, alien route -> beta, else -> -phi) --
// identical to smoothnessJacobiAlienCS.hlsl's.
float AlienCornerR(uint haloIdx, uint queryLabel)
{
    uint lab = gLabel[haloIdx];
    if (lab == queryLabel) return gPot[haloIdx];
    if (IsAlienRoute(gDiscrim[haloIdx], queryLabel)) return gAlienPot[haloIdx];
    return -gPot[haloIdx];
}

[RootSignature(SmoothnessAlienRefSig)]
[numthreads(128, 1, 1)]
void smoothnessJacobiAlienRefCS(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex)
{
    uint3 haloOriginA = gid * 2;

    // Load -- identical to smoothnessJacobiAlienCS.hlsl's.
    {
        bool isBHalo = tid >= 64u;
        uint tidLocal = tid & 63u;
        uint3 inHaloPos = uint3(tidLocal % HALO_DIM, (tidLocal / HALO_DIM) % HALO_DIM, tidLocal / (HALO_DIM * HALO_DIM));
        uint3 pos = haloOriginA + inHaloPos;
        uint3 bPos = min(pos, uint3(BDim - 1u, BDim - 1u, BDim - 1u));
        uint idx = isBHalo ? posToIdxB(bPos) : posToIdxA(pos);
        uint inHaloIdx = isBHalo ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
        gLabel[inHaloIdx] = GetCandidateLabelAt(NodeCandidateLabel, idx, 0u);
        gPot[inHaloIdx] = NodePotential[idx * MAX_CANDIDATES + 0u];
        gAlienPot[inHaloIdx] = NodeAlienPotential[idx];
        gDiscrim[inHaloIdx] = NodeDiscriminator[idx];
    }
    GroupMemoryBarrierWithGroupSync();

    uint warpId = tid / 32u;
    uint wid = tid % 32u;

    // Reference-fit residual: one lane per ring-1 incident tet (up to 24),
    // no tet-pair discovery needed -- see the file header comment.
    {
        for (int iTarget = warpId; iTarget < 16; iTarget += 4)
        {
            uint local = iTarget & 7u;
            uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
            bool isB = iTarget >= 8u;
            uint3 inHaloPos = inTilePos + 1u;
            uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
            uint myLabelAtTarget = gLabel[centerIdx];
            uint myDiscrimAtTarget = gDiscrim[centerIdx];
            float myPhiAtTarget = gPot[centerIdx];
            uint targetGlobalIdx = isB ? posToIdxB(haloOriginA + inHaloPos) : posToIdxA(haloOriginA + inHaloPos);

            uint ring1Tets[MAX_INCIDENT_TETS];
            uint ring1Count = GatherIncidentTets(targetGlobalIdx, ring1Tets);

            float refGradAccum = 0.0;
            float refDiagAccum = 0.0;
            float dynamicLoAccum = -kBetaBoundUnconstrained;
            float dynamicHiAccum = kBetaBoundUnconstrained;

            if (wid < 24u && wid < ring1Count)
            {
                uint tetX = ring1Tets[wid];
                int3 qX0, qX1, qX2, qX3;
                GetTetCornerQs(tetX, qX0, qX1, qX2, qX3);
                int haloX[4] = {
                    ResolveHaloLocalIndex(qX0, haloOriginA),
                    ResolveHaloLocalIndex(qX1, haloOriginA),
                    ResolveHaloLocalIndex(qX2, haloOriginA),
                    ResolveHaloLocalIndex(qX3, haloOriginA)
                };
                if (haloX[0] >= 0 && haloX[1] >= 0 && haloX[2] >= 0 && haloX[3] >= 0)
                {
                    int myIdx = FindHaloCorner(haloX, (int)centerIdx);
                    if (myIdx >= 0)
                    {
                        // Dynamic beta upper bound (see the plan /
                        // DistanceLattice.hlsli's FaceBetaValidInterval) --
                        // independent of the has1&&has2 gate below: this
                        // scans ALL of tetX's 3-distinct-own-label faces
                        // that include this corner, not just a label-1/2
                        // split (this diagnostic's own residual only cares
                        // about that pair, but a leak into some other
                        // label's territory is just as much a hole).
                        {
                            for (uint fIdx = 0; fIdx < 4u; fIdx++)
                            {
                                if (fIdx == (uint)myIdx) continue; // this face excludes myIdx entirely
                                uint fa = kFaceCorners[fIdx][0], fb = kFaceCorners[fIdx][1], fc = kFaceCorners[fIdx][2];
                                uint faceCorner[3] = { haloX[fa], haloX[fb], haloX[fc] };
                                uint faceLabel[3] = { gLabel[faceCorner[0]], gLabel[faceCorner[1]], gLabel[faceCorner[2]] };
                                if (faceLabel[0] == faceLabel[1] || faceLabel[1] == faceLabel[2] || faceLabel[0] == faceLabel[2]) continue;

                                uint mySlot = (fa == (uint)myIdx) ? 0u : ((fb == (uint)myIdx) ? 1u : 2u);
                                uint o0 = faceCorner[(mySlot + 1u) % 3u], o1 = faceCorner[(mySlot + 2u) % 3u];
                                uint l0f = faceLabel[0], l1f = faceLabel[1], l2f = faceLabel[2];

                                float daf = (l0f != myLabelAtTarget && IsAlienRoute(myDiscrimAtTarget, l0f)) ? 1.0 : 0.0;
                                float dbf = (l1f != myLabelAtTarget && IsAlienRoute(myDiscrimAtTarget, l1f)) ? 1.0 : 0.0;
                                float dcf = (l2f != myLabelAtTarget && IsAlienRoute(myDiscrimAtTarget, l2f)) ? 1.0 : 0.0;
                                if (daf < 0.5 && dbf < 0.5 && dcf < 0.5) continue; // beta doesn't affect this face

                                float mySlope0f = (l0f == myLabelAtTarget) ? 0.0 : daf;
                                float myIntercept0f = (l0f == myLabelAtTarget) ? myPhiAtTarget : (daf > 0.5 ? 0.0 : -myPhiAtTarget);
                                float mySlope1f = (l1f == myLabelAtTarget) ? 0.0 : dbf;
                                float myIntercept1f = (l1f == myLabelAtTarget) ? myPhiAtTarget : (dbf > 0.5 ? 0.0 : -myPhiAtTarget);
                                float mySlope2f = (l2f == myLabelAtTarget) ? 0.0 : dcf;
                                float myIntercept2f = (l2f == myLabelAtTarget) ? myPhiAtTarget : (dcf > 0.5 ? 0.0 : -myPhiAtTarget);

                                float2 faceInterval = FaceBetaValidInterval(
                                    myIntercept0f, mySlope0f, myIntercept1f, mySlope1f, myIntercept2f, mySlope2f,
                                    AlienCornerR(o0, l0f), AlienCornerR(o0, l1f), AlienCornerR(o0, l2f),
                                    AlienCornerR(o1, l0f), AlienCornerR(o1, l1f), AlienCornerR(o1, l2f));
                                dynamicLoAccum = max(dynamicLoAccum, faceInterval.x);
                                dynamicHiAccum = min(dynamicHiAccum, faceInterval.y);
                            }
                        }

                        bool has1 = false, has2 = false;
                        for (uint c0 = 0; c0 < 4u; c0++)
                        {
                            uint l = gLabel[haloX[c0]];
                            if (l == 1u) has1 = true;
                            if (l == 2u) has2 = true;
                        }
                        if (has1 && has2)
                        {
                            float3 PX[4] = { QWorldPos(qX0), QWorldPos(qX1), QWorldPos(qX2), QWorldPos(qX3) };
                            float3 wX0, wX1, wX2, wX3;
                            TetShapeGradients(PX[0], PX[1], PX[2], PX[3], wX0, wX1, wX2, wX3);
                            float3 wX[4] = { wX0, wX1, wX2, wX3 };

                            float3 gradD = float3(0, 0, 0);
                            for (uint c1 = 0; c1 < 4u; c1++)
                                gradD += (AlienCornerR(haloX[c1], 1u) - AlienCornerR(haloX[c1], 2u)) * wX[c1];
                            float D0 = AlienCornerR(haloX[0], 1u) - AlienCornerR(haloX[0], 2u);

                            float3 Rdir = cross(gradD, PlaneNormal);
                            float Rpos = D0 + dot(gradD, PlanePoint - PX[0]);

                            // d(R)/d(myBeta): 1 iff this query routes to MY
                            // corner's alien branch (own-label/default both
                            // use frozen phi, contributing 0).
                            float d1 = (1u != myLabelAtTarget && IsAlienRoute(myDiscrimAtTarget, 1u)) ? 1.0 : 0.0;
                            float d2 = (2u != myLabelAtTarget && IsAlienRoute(myDiscrimAtTarget, 2u)) ? 1.0 : 0.0;
                            float dd = d1 - d2;

                            float3 dGradD = dd * wX[myIdx];
                            float3 dRdir = cross(dGradD, PlaneNormal);
                            float dRpos = dd * ((myIdx == 0 ? 1.0 : 0.0) + dot(wX[myIdx], PlanePoint - PX[0]));

                            refGradAccum = RefDirectionWeight * 2.0 * dot(Rdir, dRdir)
                                         + RefPositionWeight * 2.0 * Rpos * dRpos;
                            refDiagAccum = RefDirectionWeight * 2.0 * dot(dRdir, dRdir)
                                         + RefPositionWeight * 2.0 * dRpos * dRpos;
                        }
                    }
                }
            }

            gRefGrad[iTarget] = WaveActiveSum(refGradAccum);
            gRefDiag[iTarget] = WaveActiveSum(refDiagAccum);
            gAlienBetaLower[iTarget] = WaveActiveMax(dynamicLoAccum);
            gAlienBetaUpper[iTarget] = WaveActiveMin(dynamicHiAccum);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint iTarget = tid / 8u;
    uint local2 = iTarget & 7u;
    uint3 inTilePos2 = uint3(local2 & 1u, (local2 >> 1u) & 1u, (local2 >> 2u) & 1u);
    bool isB2 = iTarget >= 8u;
    uint3 inHaloPos2 = inTilePos2 + 1u;
    uint centerIdx2 = isB2 ? inHaloPosToInHaloIdxB(inHaloPos2) : inHaloPosToInHaloIdxA(inHaloPos2);
    uint targetGlobalIdx2 = isB2 ? posToIdxB(haloOriginA + inHaloPos2) : posToIdxA(haloOriginA + inHaloPos2);

    float myBeta = gAlienPot[centerIdx2];
    float myPhi = gPot[centerIdx2];
    float grad = gRefGrad[iTarget];
    float diag = gRefDiag[iTarget];

    float step = clamp(-grad / (diag + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
    float newBeta = myBeta + step;

    // Same dynamic bound as smoothnessJacobiAlienCS.hlsl, BOTH sides -- see
    // that file's final-combine comment for the full derivation (an earlier
    // version assumed unbounded-below and dropped the lower half entirely;
    // tet 309540 proved that false in the coupled/multi-route case).
    float dynamicLo = gAlienBetaLower[iTarget];
    float dynamicHi = gAlienBetaUpper[iTarget];
    // Swap-guard: see smoothnessJacobiAlienCS.hlsl's copy of this comment --
    // two different discovered faces can legitimately impose non-overlapping
    // valid ranges on the same node, and applying min() then max() on an
    // inverted pair let the second one silently override the first (node
    // 5487 measured lo=50.3>hi=5.05, beta ran away to 4244).
    if (dynamicLo > dynamicHi) { float t = dynamicLo; dynamicLo = dynamicHi; dynamicHi = t; }
    // Margin can invert a tight-but-valid interval too (see
    // smoothnessJacobiAlienCS.hlsl's copy of this comment -- node 8060
    // measured this directly) -- swap-guard the MARGINED pair, not just the
    // raw one.
    const float kAlienSafetyMarginFraction = 0.1;
    float marginedLo = (dynamicLo > -kBetaBoundUnconstrained * 0.99) ? (dynamicLo + kAlienSafetyMarginFraction * myPhi) : dynamicLo;
    float marginedHi = (dynamicHi < kBetaBoundUnconstrained * 0.99) ? (dynamicHi - kAlienSafetyMarginFraction * myPhi) : dynamicHi;
    if (marginedLo > marginedHi) { float t = marginedLo; marginedLo = marginedHi; marginedHi = t; }
    newBeta = min(newBeta, marginedHi);
    newBeta = max(newBeta, marginedLo);

    NodeAlienPotentialScratch[targetGlobalIdx2] = newBeta;
}
