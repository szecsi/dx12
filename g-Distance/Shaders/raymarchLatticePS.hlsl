#include "DistanceFrameCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b1
#include "DistanceLattice.hlsli"
#include "LabelPalette.hlsli"

// Junction-aware lattice raymarch: walks the ray tet-by-tet through the BCC
// lattice's own tet decomposition (DistanceLattice.hlsli), using each tet's
// ACTUAL corner labels/potentials to find where the winning label changes --
// unlike extractSurfaceSyntheticCS.hlsl's static mesh extraction (which can
// only ever resolve 2 labels per tet, a frequency-vote li/lj that folds any
// 3rd/4th corner label into a fixed fallback), every non-homogeneous tet
// reconstructs one affine field PER DISTINCT LABEL present (up to 4, fewer
// whenever corners share a label -- identically-labeled corners POOL into
// one shared field rather than competing as independent entries), plain
// own-potential/minus-everyone-else's-potential per corner, no beta/gamma
// involved at all -- so a real 3/4-way junction renders as an actual
// crossing instead of a flattened artifact. See the design notes
// (soft-stargazing-biscuit.md) for the full derivation: q-space is a fixed
// LINEAR map of world space for both sublattices, so a world ray's parameter
// t carries over unchanged into q-space (q(t)=q0+t*qd) -- letting the whole
// walk (point-location, face intersections) happen in q-space while the
// final hit's world position/depth is just ro+rd*t, exactly like
// raymarchPS.hlsl's own pattern.
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

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

struct PsOut {
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

// Synthetic-field corner lookup -- mirrors extractSurfaceSyntheticCS.hlsl's
// SyntheticCornerLabel. Virtual/out-of-grid corners are a fixed background
// node (label 0, potential 1.0), same convention as GetCornerTopLabel.
//
// Plain own=+pot/foreign=-pot per CORNER (not per label, see the file header
// comment) is the ONLY rule this file uses -- reverted back to this from the
// psi/beta/gamma (CornerR3WayValue) 3-way rule and its NodeAlienPotential/
// NodeDiscriminator buffers, which had become dead plumbing here anyway (the
// 4-way-per-corner junction search below never called it -- see git history
// for the removed CornerR/CornerLabelPotAlien if that's ever worth
// resurrecting), in preparation for the per-edge-derivative scheme (see the
// approved plan).
void CornerLabelPot(uint cornerRef, out uint label, out float pot)
{
    if (cornerRef == SENTINEL_LABEL) { label = 0u; pot = 1.0; return; }
    label = GetCandidateLabelAt(NodeCandidateLabel, cornerRef, 0u);
    pot = NodePotential[cornerRef * MAX_CANDIDATES + 0u];
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

        uint cornerLabel[4]; float cornerPot[4]; uint cornerRef[4];
        for (uint c2 = 0; c2 < 4; c2++) {
            cornerRef[c2] = ResolveCorner(qArr[c2]);
            CornerLabelPot(cornerRef[c2], cornerLabel[c2], cornerPot[c2]);
        }

        // Distinct labels present among this tet's 4 corners (up to 4,
        // fewer whenever any corners share a label).
        uint present[4]; uint presentCount = 0;
        for (uint cp = 0; cp < 4; cp++) {
            bool already = false;
            for (uint pp = 0; pp < presentCount; pp++) if (present[pp] == cornerLabel[cp]) { already = true; break; }
            if (!already) present[presentCount++] = cornerLabel[cp];
        }

        if (presentCount > 1) {
            float3 Pw[4];
            for (uint c4 = 0; c4 < 4; c4++) Pw[c4] = QWorldPos(qArr[c4]);
            float3 w0, w1, w2, w3;
            TetShapeGradients(Pw[0], Pw[1], Pw[2], Pw[3], w0, w1, w2, w3);
            float3 wArr[4] = { w0, w1, w2, w3 };

            // One field PER DISTINCT LABEL (not per corner) -- corners
            // sharing a label pool into the SAME shared field (each
            // contributing its own real potential as a genuine sample,
            // exactly like every other corner's own reading), rather than
            // competing as independent entries. Reverted back to this from
            // an intermediate "always exactly 4 fields, one per corner,
            // same-label crossings skipped via an envelope-of-lines search"
            // version -- that version never actually merged same-labeled
            // corners into one field at all, just avoided rendering a seam
            // between them by special-casing the search; this restores a
            // genuine single gradient for a repeated label instead. Field l's
            // value at corner c is +itsOwnPotential where c's label matches
            // l, -itsOwnPotential everywhere else -- unconditionally, plain
            // own/foreign, no beta/gamma. Affine over the tet (same
            // TetShapeGradients machinery), so each field's value along the
            // ray, G_l(t) = base + slope*t, is a single line -- computed
            // once via the same wArr (tet geometry only), no per-step
            // re-solving.
            float lineBase[4], lineSlope[4]; // indexed by position in `present`, not corner index
            for (uint pi = 0; pi < presentCount; pi++) {
                uint l = present[pi];
                float G[4];
                for (uint c5 = 0; c5 < 4; c5++) G[c5] = (cornerLabel[c5] == l) ? cornerPot[c5] : -cornerPot[c5];
                float3 gradG = G[0] * wArr[0] + G[1] * wArr[1] + G[2] * wArr[2] + G[3] * wArr[3];
                lineBase[pi] = G[0] + dot(ro - Pw[0], gradG);
                lineSlope[pi] = dot(rd, gradG);
            }

            // First t (nearest the camera) after tCur where two present
            // labels' lines tie AND are jointly the maximum among all
            // present labels there -- the classic "first breakpoint of an
            // upper envelope of lines" search, now genuinely simple since
            // each present label is exactly one line (no per-label envelope-
            // of-several-lines bookkeeping needed anymore).
            float crossT = 1.0e30; uint crossA = 0, crossB = 0; bool foundCross = false;
            for (uint pi = 0; pi < presentCount; pi++) {
                for (uint pj = pi + 1; pj < presentCount; pj++) {
                    float slopeDiff = lineSlope[pj] - lineSlope[pi];
                    if (abs(slopeDiff) <= epsSlope) continue; // parallel (or coincident) -- never a fresh crossing
                    float tc = (lineBase[pi] - lineBase[pj]) / slopeDiff;
                    if (tc <= tCur + epsT || tc > bestT + epsT || tc >= crossT) continue;
                    float tieVal = lineBase[pi] + lineSlope[pi] * tc;
                    bool valid = true;
                    for (uint pk = 0; pk < presentCount; pk++) {
                        if (pk == pi || pk == pj) continue;
                        float vk = lineBase[pk] + lineSlope[pk] * tc;
                        if (vk > tieVal + 1.0e-6) { valid = false; break; } // a 3rd label is already ahead here -- not the true crossing
                    }
                    if (valid) { crossT = tc; crossA = pi; crossB = pj; foundCross = true; }
                }
            }

            if (foundCross) {
                // Whichever of the pair has the SMALLER slope was dominant
                // just before the tie (the near/background side, receding);
                // the other is what's actually beyond the surface from the
                // camera's side (approaching, overtakes for t>tc).
                uint winner = (lineSlope[crossA] < lineSlope[crossB]) ? crossA : crossB;
                uint crossWinner = (winner == crossA) ? crossB : crossA;
                uint winnerLabel = present[winner];
                uint crossLabel = present[crossWinner];

                float3 worldHit = ro + rd * crossT;
                float4 clipHit = mul(float4(worldHit, 1), viewProjTransform);
                result.depth = clipHit.z / clipHit.w;

                // Interface normal: gradient of (G_crossLabel - G_winnerLabel),
                // same convention as extractSurfaceSyntheticCS.hlsl's g[]/
                // gradG (which label is "positive" is arbitrary there too --
                // only the SIGN used for orienting toward the camera below
                // matters for shading).
                float gDiff[4];
                for (uint c6 = 0; c6 < 4; c6++) {
                    float gW = (cornerLabel[c6] == winnerLabel) ? cornerPot[c6] : -cornerPot[c6];
                    float gO = (cornerLabel[c6] == crossLabel) ? cornerPot[c6] : -cornerPot[c6];
                    gDiff[c6] = gO - gW;
                }
                float3 gradN = gDiff[0] * wArr[0] + gDiff[1] * wArr[1] + gDiff[2] * wArr[2] + gDiff[3] * wArr[3];
                float3 n = (length(gradN) > 1.0e-8) ? normalize(gradN) : float3(0, 0, 1);
                float3 toCam = -rd;
                float3 nFacing = (dot(n, toCam) > 0.0) ? n : -n;

                float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
                float diff = saturate(dot(nFacing, lightDir)) * 0.7 + 0.3;
                // crossLabel is the label that overtakes as t increases --
                // i.e. what's actually beyond the surface from the camera's
                // side (winnerLabel is the near/background side that
                // recedes), matching surfacePS.hlsl's "far side" color
                // convention.
                float3 baseColor = LabelColorA(crossLabel);
                result.color = float4(baseColor * diff, 1.0);
                return result;
            }
        }

        if (!AdvanceTetAcrossFace(C, slot, (uint)bestExit)) break;
        tCur = bestT;
    }

    return result;
}
