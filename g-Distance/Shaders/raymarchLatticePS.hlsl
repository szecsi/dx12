#include "DistanceFrameCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b1
#include "DistanceLattice.hlsli"
#define DISTANCE_CB_REGISTER b2
#include "DistanceCb.hlsli"
#include "LabelPalette.hlsli"

// Junction-aware lattice raymarch: walks the ray tet-by-tet through the BCC
// lattice's own tet decomposition (DistanceLattice.hlsli), using each tet's
// ACTUAL corner labels/potentials to find where the winning label changes --
// unlike extractSurfaceSyntheticCS.hlsl's static mesh extraction (which can
// only ever resolve 2 labels per tet, a frequency-vote li/lj that folds any
// 3rd/4th corner label into a fixed fallback), this evaluates EVERY distinct
// label present at a tet's corners as its own affine field along the ray and
// finds the first one that overtakes the current winner -- so a genuine
// 3/4-way junction renders as a real crossing instead of a flattened
// artifact. See the design notes (soft-stargazing-biscuit.md) for the full
// derivation: q-space is a fixed LINEAR map of world space for both
// sublattices, so a world ray's parameter t carries over unchanged into
// q-space (q(t)=q0+t*qd) -- letting the whole walk (point-location, face
// intersections) happen in q-space while the final hit's world position/
// depth is just ro+rd*t, exactly like raymarchPS.hlsl's own pattern.
//
// v1 scope: synthetic-field pipeline only (single label+potential per node,
// see smoothnessJacobiSyntheticCS.hlsl) -- gated by useSyntheticField on the
// C++ side (DistanceApp.h). Also: only the FIRST winner-change along the
// whole ray is ever rendered (like any raymarcher stopping at its first
// hit) -- a tet with 3+ distinct corner labels can in principle have up to
// 3 internal winner-changes, but any beyond the first are further from the
// camera and hence occluded by it anyway, so finding just the first is
// exactly what's needed for correct rendering, not a shortcut.
//
// No [RootSignature(...)] here -- declared once by raymarchLatticeVS.hlsl,
// reused for both stages of this draw (this codebase's convention).

RWStructuredBuffer<uint>  NodeCandidateLabel : register(u0);
RWStructuredBuffer<float> NodePotential : register(u1);
// Alien-potential secondary pass (see the approved plan) -- read-only here.
RWStructuredBuffer<float> NodeAlienPotential : register(u2);
RWStructuredBuffer<uint>  NodeDiscriminator : register(u3);

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

struct PsOut {
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

// Synthetic-field corner lookup -- mirrors extractSurfaceSyntheticCS.hlsl's
// SyntheticCornerLabel, generalized to also return the potential, beta, and
// discriminator in one call since every caller here needs all four.
// Virtual/out-of-grid corners are a fixed background node (label 0,
// potential 1.0, beta -1.0/discriminator disabled -- irrelevant, since a
// virtual corner's label 0 can never be queried as a genuinely competing
// "other" label here), same convention as GetCornerTopLabel.
void CornerLabelPotAlien(uint cornerRef, out uint label, out float pot, out float beta, out uint discrim)
{
    if (cornerRef == SENTINEL_LABEL) { label = 0u; pot = 1.0; beta = -1.0; discrim = 0u; return; }
    label = GetCandidateLabelAt(NodeCandidateLabel, cornerRef, 0u);
    pot = NodePotential[cornerRef * MAX_CANDIDATES + 0u];
    beta = NodeAlienPotential[cornerRef];
    discrim = NodeDiscriminator[cornerRef];
}

// The corner-value rule for query label ell: own label -> pot; alien route
// (only when UseAlienPotential is on -- see DistanceCb.hlsli) -> beta;
// otherwise -> the reciprocal-derived value (or -pot if disabled). Degenerates
// to byte-identical output to before the alien-potential pass existed
// whenever UseAlienPotential<=0.5, regardless of what beta/discrim actually
// hold. The actual 3-way rule (own/routed/disabled/reciprocal) is
// DistanceLattice.hlsli's CornerR3WayValue -- shared with footSlicePS.hlsl's
// "chosen-label field" debug slice, this just layers the render toggle on
// top of it.
float CornerR(uint label, float pot, float beta, uint discrim, uint queryLabel)
{
    if (UseAlienPotential > 0.5) return CornerR3WayValue(label, pot, beta, discrim, queryLabel);
    if (label == queryLabel) return pot;
    return -pot;
}

// q-space forward map (bccToRhombo's linear part -- see NodeQ), applied here
// to a continuous real-space vector rather than just an integer node index
// -- valid for both positions and directions since it's linear, which is
// exactly what preserves the ray parameter t between world space and
// q-space (see the file header comment above).
float3 QOf(float3 r) { return float3(r.x + r.y, r.x + r.z, r.y + r.z); }

bool IntersectWorldBox(float3 ro, float3 rd, out float tEnter, out float tExit)
{
    float3 boxMin = float3(-0.5, -0.5, -0.5) * CELL_SIZE;
    float3 boxMax = (float3((float)GridRes, (float)GridRes, (float)GridRes) - 0.5) * CELL_SIZE;
    float3 invD = 1.0 / rd;
    float3 t0 = (boxMin - ro) * invD;
    float3 t1 = (boxMax - ro) * invD;
    float3 tSmall = min(t0, t1);
    float3 tBig = max(t0, t1);
    tEnter = max(max(tSmall.x, tSmall.y), tSmall.z);
    tExit = min(min(tBig.x, tBig.y), tBig.z);
    return tEnter <= tExit;
}

// Which 3 corners bound the face OPPOSITE corner index i (0..3) -- matches
// ExitCornerToRelation's (DistanceLattice.hlsli) own "opposite corner"
// convention, so its result can be fed straight into AdvanceTetAcrossFace.
static const uint FaceCorners[4][3] = {
    { 1, 2, 3 }, { 0, 2, 3 }, { 0, 1, 3 }, { 0, 1, 2 }
};

PsOut raymarchLatticePS(VsOut input)
{
    PsOut result;
    result.color = float4(0.05, 0.06, 0.08, 1.0);
    result.depth = 1.0;

    float3 ro = cameraPos.xyz;
    float3 rd = normalize(input.rayDir);

    float tEnter, tExit;
    if (!IntersectWorldBox(ro, rd, tEnter, tExit) || tExit < 0.0) return result;

    float3 q0 = QOf(ro / CELL_SIZE);
    float3 qd = QOf(rd / CELL_SIZE);

    float tCur = max(tEnter, 0.0);
    float3 qp = q0 + qd * tCur;
    int3 C = int3(floor(qp));
    uint slot = TetSlotFromFrac(qp - (float3)C);

    const float epsT = 1.0e-4;
    const float epsSlope = 1.0e-6;
    uint maxSteps = (uint)max(raymarchParams.w, 1.0);

    for (uint iter = 0; iter < maxSteps; iter++) {
        if (tCur > tExit + epsT) break; // left the visible domain

        int3 qArr[4] = {
            C, C + int3(1, 1, 1),
            C + CubeVertexOffsets[slot][0], C + CubeVertexOffsets[slot][1]
        };
        float3 P[4];
        for (uint c = 0; c < 4; c++) P[c] = (float3)qArr[c];

        float bestT = 1.0e30;
        int bestExit = -1;
        for (uint i = 0; i < 4; i++) {
            float3 Pa = P[FaceCorners[i][0]], Pb = P[FaceCorners[i][1]], Pc = P[FaceCorners[i][2]];
            float3 N = cross(Pb - Pa, Pc - Pa);
            float denom = dot(N, qd);
            if (abs(denom) < 1.0e-8) continue;
            float ti = dot(N, Pa - q0) / denom;
            if (ti > tCur + epsT && ti < bestT) { bestT = ti; bestExit = (int)i; }
        }
        if (bestExit < 0) break; // degenerate (ray exits exactly along an edge/vertex) -- bail out rather than loop forever

        uint cornerLabel[4]; float cornerPot[4]; float cornerBeta[4]; uint cornerDiscrim[4]; uint cornerRef[4];
        for (uint c2 = 0; c2 < 4; c2++) {
            cornerRef[c2] = ResolveCorner(qArr[c2]);
            CornerLabelPotAlien(cornerRef[c2], cornerLabel[c2], cornerPot[c2], cornerBeta[c2], cornerDiscrim[c2]);
        }

        bool homo = (cornerLabel[0] == cornerLabel[1]) && (cornerLabel[0] == cornerLabel[2]) && (cornerLabel[0] == cornerLabel[3]);
        if (!homo) {
            uint distinctLabels[4]; uint nDistinct = 0;
            for (uint c3 = 0; c3 < 4; c3++) {
                bool found = false;
                for (uint u = 0; u < nDistinct; u++) if (distinctLabels[u] == cornerLabel[c3]) { found = true; break; }
                if (!found) { distinctLabels[nDistinct] = cornerLabel[c3]; nDistinct++; }
            }

            float3 Pw[4];
            for (uint c4 = 0; c4 < 4; c4++) Pw[c4] = QWorldPos(qArr[c4]);
            float3 w0, w1, w2, w3;
            TetShapeGradients(Pw[0], Pw[1], Pw[2], Pw[3], w0, w1, w2, w3);
            float3 wArr[4] = { w0, w1, w2, w3 };

            // Each distinct label L's "own confidence" field: every corner
            // contributes its OWN real potential, signed by whether that
            // corner actually carries L (+) or not (-) -- NOT a constant
            // fallback for non-matching corners (an earlier version of this
            // used a fixed -10 there, which swamped the real, evolving
            // potentials entirely -- exactly the "colors change, surface
            // doesn't"/discrete-flips-only symptom extractSurfaceSyntheticCS
            // .hlsl's own SyntheticCornerG comment already warns about for
            // the identical mistake). This construction is affine over the
            // tet (same TetShapeGradients machinery), so its value along the
            // ray, G_label(t) = base + slope*t, is a single line -- computed
            // once via the SAME wArr (tet geometry only, independent of
            // which field), no per-step re-solving needed. Reduces exactly
            // to a 2-label tet's G_i=phi_i-phi_j field when only 2 labels
            // are present (every corner is then either +ownPot or -ownPot,
            // the two labels' fields are then exact negations of each
            // other), and generalizes cleanly to 3/4 via the same rule.
            float lineBase[4], lineSlope[4];
            for (uint u2 = 0; u2 < nDistinct; u2++) {
                float G[4];
                for (uint c5 = 0; c5 < 4; c5++) G[c5] = CornerR(cornerLabel[c5], cornerPot[c5], cornerBeta[c5], cornerDiscrim[c5], distinctLabels[u2]);
                float3 gradG = G[0] * wArr[0] + G[1] * wArr[1] + G[2] * wArr[2] + G[3] * wArr[3];
                lineBase[u2] = G[0] + dot(ro - Pw[0], gradG);
                lineSlope[u2] = dot(rd, gradG);
            }

            uint winner = 0;
            for (uint u3 = 1; u3 < nDistinct; u3++)
                if (lineBase[u3] + lineSlope[u3] * tCur > lineBase[winner] + lineSlope[winner] * tCur) winner = u3;

            // First t (nearest the camera) where another present label's
            // line overtakes the current winner's -- the closed-form
            // crossing of two affine lines, generalized from
            // extractSurfaceSyntheticCS.hlsl's CrossPoint (2 corner values)
            // to 2 label-lines evaluated at arbitrary ray t.
            float crossT = 1.0e30; uint crossWinner = winner;
            for (uint u4 = 0; u4 < nDistinct; u4++) {
                if (u4 == winner) continue;
                float slopeDiff = lineSlope[u4] - lineSlope[winner];
                if (slopeDiff <= epsSlope) continue; // never overtakes for t>tCur
                float tc = (lineBase[winner] - lineBase[u4]) / slopeDiff;
                if (tc > tCur + epsT && tc <= bestT + epsT && tc < crossT) { crossT = tc; crossWinner = u4; }
            }

            if (crossWinner != winner) {
                float3 worldHit = ro + rd * crossT;
                float4 clipHit = mul(float4(worldHit, 1), viewProjTransform);
                result.depth = clipHit.z / clipHit.w;

                // Interface normal: gradient of (G_crossWinner - G_winner),
                // same convention as extractSurfaceSyntheticCS.hlsl's g[]/
                // gradG (which corner is "positive" is arbitrary there too --
                // only the SIGN used for orienting toward the camera below
                // matters for shading).
                float gDiff[4];
                for (uint c6 = 0; c6 < 4; c6++) {
                    float gW = CornerR(cornerLabel[c6], cornerPot[c6], cornerBeta[c6], cornerDiscrim[c6], distinctLabels[winner]);
                    float gO = CornerR(cornerLabel[c6], cornerPot[c6], cornerBeta[c6], cornerDiscrim[c6], distinctLabels[crossWinner]);
                    gDiff[c6] = gO - gW;
                }
                float3 gradN = gDiff[0] * wArr[0] + gDiff[1] * wArr[1] + gDiff[2] * wArr[2] + gDiff[3] * wArr[3];
                float3 n = (length(gradN) > 1.0e-8) ? normalize(gradN) : float3(0, 0, 1);
                float3 toCam = -rd;
                float3 nFacing = (dot(n, toCam) > 0.0) ? n : -n;

                float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
                float diff = saturate(dot(nFacing, lightDir)) * 0.7 + 0.3;
                // crossWinner is the label that overtakes as t increases --
                // i.e. what's actually beyond the surface from the camera's
                // side (winner is the near/background side that recedes),
                // matching surfacePS.hlsl's "far side" color convention.
                float3 baseColor = LabelColorA(distinctLabels[crossWinner]);
                result.color = float4(baseColor * diff, 1.0);
                return result;
            }
        }

        if (!AdvanceTetAcrossFace(C, slot, (uint)bestExit)) break;
        tCur = bestT;
    }

    return result;
}
