#define DISTANCE_GRID_CB_REGISTER b1
#include "DistanceLattice.hlsli"
#define CLIPPED_SPHERES_CB_REGISTER b2
#include "ClippedSpheresCb.hlsli"

// TestShape_ClippedSpheres-only: solves for per-NODE edge-derivative
// multipliers (see the approved plan's per-face redesign) via GPU Jacobi
// relaxation -- SUPERSEDES the earlier per-tet analytic solve
// (buildAnalyticEdgeDerivMultCS.hlsl, deleted), which was structurally wrong
// (see the plan for the barycentric-restriction proof): a corner's
// multiplier is a genuinely per-NODE quantity, not a per-tet one, because
// the reconstructed field restricted to any FACE depends only on that
// face's 3 corners, never on whichever tet (which apex) you view it from.
//
// One equation per circle-crossing FACE (not per tet): the circle's plane
// intersected with a face's plane gives a line (unless parallel), intersected
// with the circle itself gives up to 2 points; kept only if exactly ONE lands
// inside that triangular face (0 or 2+ is skipped as ambiguous/degenerate).
// At that point Q, the TRUE analytic scene fields (outside/capA/capB, same
// formula as buildAnalyticClippedSpheresCS.hlsl) are evaluated directly (not
// from the discrete corner labels) to find the locally-tied label pair
// l1,l2 -- this also removes the old per-tet design's "must sample all 3
// labels" restriction, since the tie is determined by true position, not by
// which labels happen to be sampled at the corners.
//
// Dispatched one thread per NODE (SmoothnessGroups/NodeCount, like every
// other Jacobi smoothing pass), reusing extractJunctionFootVectorsCS.hlsl's
// own already-proven traversal (GatherIncidentTets -> up to
// MAX_INCIDENT_TETS tets, each tet's own 4 faces via FaceCorners) rather than
// GetFaceAdjacentPartner/shared-corner-intersection -- a face is visited
// once per incident tet that has it (twice total for an interior face, since
// both tets sharing it are typically in this node's own ring1Tets too, once
// for a domain-boundary face) which is harmless: the equation's value is
// PROVABLY identical either way (that's the whole point of the per-face
// proof), so revisiting it just applies the same equation with double
// weight, a uniform, harmless bias (see the plan).
//
// Per node, for each face that includes it among its 3 corners AND the
// circle crosses: isolate this node's own coefficient (holding the OTHER 2
// face corners at their CURRENT/previous-sweep stored value) and accumulate
// into a running weighted-least-squares closed form (no per-equation
// division, avoids blowing up on a near-zero individual coefficient), with a
// Tikhonov anchor to the identity (1.0) so a node with no real equations
// converges to exactly 1.0 within one sweep, matching
// smoothJunctionFootVectorsCS.hlsl's own "degrade gracefully, don't go
// singular" precedent.
//
// Purely incremental: every dispatch continues from whatever
// NodeEdgeDerivMult currently holds (buildEdgeDataCS.hlsl/'H' is what seeds
// it to the identity 1.0, unconditionally, on every scene -- not this
// shader), so repeated 'I' presses keep making progress instead of each one
// silently re-converging from scratch to the same fixed point (an earlier
// bug: an "IsFirstSweep" reset ran on every press, making further presses
// look like "no motion" since they always started from the same seed).
//
// RelaxFactor (root constant, GUI-tunable): damped/under-relaxed Jacobi --
// mNew = mOld + RelaxFactor*(mRaw-mOld) -- rather than jumping straight to
// each sweep's raw weighted-least-squares solution. Plain (undamped) Jacobi
// on this system isn't guaranteed to converge (no diagonal-dominance
// guarantee), and was observed to overshoot badly enough to visibly break
// the rendered junction after a batch of sweeps ("knocks the junction
// surfaces off") -- damping (RelaxFactor<1) trades slower convergence for
// stability, letting the user watch it settle over repeated presses instead
// of jumping straight to a possibly-diverged state.
#define SmoothEdgeDerivMultSig "RootFlags(0)," \
    "RootConstants(num32BitConstants=1, b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b1)," \
    "CBV(b2)"

cbuffer ModeConsts : register(b0) {
    float RelaxFactor;
};

RWStructuredBuffer<uint>   NodeCandidateLabel     : register(u0); // read
RWStructuredBuffer<float>  NodePotential          : register(u1); // read
RWStructuredBuffer<float>  NodeEdgeDerivMult      : register(u2); // read ("current", previous sweep)
RWStructuredBuffer<float>  NodeEdgeDerivMultScratch : register(u3); // write (this sweep's result)

// Standard 2-plane intersection line: planes (n1,p1) and (n2,p2), each given
// as a point and a normal. Returns false if the planes are (near-)parallel.
bool PlanePlaneLine(float3 n1, float3 p1, float3 n2, float3 p2, out float3 L0, out float3 Ld)
{
    Ld = cross(n1, n2);
    float denom = dot(Ld, Ld);
    if (denom < 1.0e-12) { L0 = float3(0, 0, 0); return false; }
    float d1 = dot(n1, p1), d2 = dot(n2, p2);
    L0 = (cross(n2, Ld) * d1 + cross(Ld, n1) * d2) / denom;
    return true;
}

// Intersects a line (L0+s*Ld) with a circle (center P0c, radius Rc) known to
// lie IN the same plane as the line (true here, since Ld is always the
// circle-plane / face-plane intersection). Up to 2 real roots.
uint LineCircle(float3 L0, float3 Ld, float3 P0c, float Rc, out float s0, out float s1)
{
    float3 w = L0 - P0c;
    float a = dot(Ld, Ld);
    float b = 2.0 * dot(Ld, w);
    float c = dot(w, w) - Rc * Rc;
    float disc = b * b - 4.0 * a * c;
    s0 = 0.0; s1 = 0.0;
    if (disc < 0.0 || a < 1.0e-12) return 0;
    float sq = sqrt(disc);
    s0 = (-b - sq) / (2.0 * a);
    s1 = (-b + sq) / (2.0 * a);
    return 2;
}

// Barycentric weights of X (already known to lie in the triangle's plane)
// w.r.t. triangle (A,B,C).
float3 BaryInTri(float3 X, float3 A, float3 B, float3 C)
{
    float3 v0 = B - A, v1 = C - A, v2 = X - A;
    float d00 = dot(v0, v0), d01 = dot(v0, v1), d11 = dot(v1, v1);
    float d20 = dot(v2, v0), d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float bB = (d11 * d20 - d01 * d21) / denom;
    float bC = (d00 * d21 - d01 * d20) / denom;
    return float3(1.0 - bB - bC, bB, bC);
}

// True analytic scene fields at an arbitrary world point -- same formula as
// buildAnalyticClippedSpheresCS.hlsl, including the same clamp (for
// consistency with what the corners' own stored potentials represent).
void EvalAnalyticScene(float3 p, out float outside, out float capA, out float capB)
{
    float sdA = RadiusA - length(p - CenterA);
    float sdB = RadiusB - length(p - CenterB);
    float3 diff = CenterB - CenterA;
    float d = length(diff);
    float3 n = diff / max(d, 1.0e-6);
    float t = (d * d + RadiusA * RadiusA - RadiusB * RadiusB) / (2.0 * max(d, 1.0e-6));
    float3 P0 = CenterA + n * t;
    float planeD = dot(p - P0, n);
    capA = min(sdA, -planeD);
    capB = min(sdB, planeD);
    outside = -max(sdA, sdB);
    outside = clamp(outside, -ClampDistance, ClampDistance);
    capA = clamp(capA, -ClampDistance, ClampDistance);
    capB = clamp(capB, -ClampDistance, ClampDistance);
}

// Top-2 ranked labels (0=outside,1=capA,2=capB) by true value, matching
// buildAnalyticClippedSpheresCS.hlsl's own winner/second ranking exactly.
void RankTop2(float outside, float capA, float capB, out uint l1, out uint l2)
{
    float raw[3] = { outside, capA, capB };
    uint winner = 0;
    if (raw[1] > raw[winner]) winner = 1;
    if (raw[2] > raw[winner]) winner = 2;
    uint second = (winner == 0) ? 1u : 0u;
    [unroll]
    for (uint k = 1; k < 3u; k++) if (k != winner && raw[k] > raw[second]) second = k;
    l1 = winner; l2 = second;
}

[RootSignature(SmoothEdgeDerivMultSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void smoothEdgeDerivMultCS(uint3 dtid : SV_DispatchThreadID)
{
    uint node = dtid.x;
    if (node >= NodeCount) return;

    float mOld = NodeEdgeDerivMult[node];

    float3 diffC = CenterB - CenterA;
    float dC = length(diffC);
    float3 nCirc = diffC / max(dC, 1.0e-6);
    float tC = (dC * dC + RadiusA * RadiusA - RadiusB * RadiusB) / (2.0 * max(dC, 1.0e-6));
    float3 P0c = CenterA + nCirc * tC;
    float Rc2 = RadiusA * RadiusA - tC * tC;
    if (Rc2 <= 1.0e-8) { NodeEdgeDerivMultScratch[node] = mOld; return; } // spheres don't really intersect -- nothing to do
    float Rc = sqrt(Rc2);

    float sumCoefSq = 0.0;
    float sumCoefTarget = 0.0;

    uint ring1Tets[MAX_INCIDENT_TETS];
    uint ring1Count = GatherIncidentTets(node, ring1Tets);

    for (uint t = 0; t < ring1Count; t++) {
        uint tetA = ring1Tets[t];
        int3 qArr[4];
        GetTetCornerQs(tetA, qArr[0], qArr[1], qArr[2], qArr[3]);

        uint cA[4];
        bool anySentinel = false;
        [unroll]
        for (uint ci = 0; ci < 4; ci++) {
            cA[ci] = ResolveCorner(qArr[ci]);
            if (cA[ci] == SENTINEL_LABEL) anySentinel = true;
        }
        if (anySentinel) continue;

        float3 PwFull[4];
        [unroll]
        for (uint ci2 = 0; ci2 < 4; ci2++) PwFull[ci2] = QWorldPos(qArr[ci2]);

        float3 w0, w1, w2, w3;
        TetShapeGradients(PwFull[0], PwFull[1], PwFull[2], PwFull[3], w0, w1, w2, w3);
        float3 wFull[4] = { w0, w1, w2, w3 };

        [unroll]
        for (uint f = 0; f < 4; f++) {
            uint i0 = FaceCorners[f][0], i1 = FaceCorners[f][1], i2 = FaceCorners[f][2];
            uint faceNode[3] = { cA[i0], cA[i1], cA[i2] };

            int myK = -1;
            [unroll]
            for (uint k = 0; k < 3; k++) if (faceNode[k] == node) myK = (int)k;
            if (myK < 0) continue; // this face doesn't touch the query node

            float3 P0f = PwFull[i0], P1f = PwFull[i1], P2f = PwFull[i2];
            float3 Fn = cross(P1f - P0f, P2f - P0f);
            if (dot(Fn, Fn) < 1.0e-12) continue; // degenerate face -- shouldn't happen for a real tet

            float3 L0, Ld;
            if (!PlanePlaneLine(nCirc, P0c, Fn, P0f, L0, Ld)) continue;

            float s0, s1;
            uint nRoots = LineCircle(L0, Ld, P0c, Rc, s0, s1);
            float3 Q = float3(0, 0, 0);
            uint validCount = 0;
            [unroll]
            for (uint ri = 0; ri < 2; ri++) {
                if (ri >= nRoots) break;
                float s = (ri == 0) ? s0 : s1;
                float3 X = L0 + Ld * s;
                float3 bary = BaryInTri(X, P0f, P1f, P2f);
                float eps = 1.0e-5;
                if (bary.x >= -eps && bary.y >= -eps && bary.z >= -eps) { Q = X; validCount++; }
            }
            if (validCount != 1) continue; // not a clean single crossing on this face -- skip (ambiguous/degenerate)

            float rawOut, rawA, rawB;
            EvalAnalyticScene(Q, rawOut, rawA, rawB);
            uint l1, l2;
            RankTop2(rawOut, rawA, rawB, l1, l2);
            if (l1 == l2) continue;

            float G1[3], G2[3];
            uint wIdx[3] = { i0, i1, i2 };
            [unroll]
            for (uint k2 = 0; k2 < 3; k2++) {
                uint cornerRef = faceNode[k2];
                uint lab = GetCandidateLabelAt(NodeCandidateLabel, cornerRef, 0u);
                float pot = NodePotential[cornerRef * MAX_CANDIDATES + 0u];
                G1[k2] = (lab == l1) ? pot : -pot;
                G2[k2] = (lab == l2) ? pot : -pot;
            }

            float coef[3];
            [unroll]
            for (uint k3 = 0; k3 < 3; k3++) coef[k3] = (G1[k3] - G2[k3]) * dot(Q - P0f, wFull[wIdx[k3]]);
            float rhs = -(G1[0] - G2[0]);

            // Isolate this node's own equation, holding the OTHER 2 face
            // corners at their current (previous-sweep) stored value --
            // standard Jacobi: neighbors are read from the "current" buffer,
            // never the scratch one being written this sweep.
            float known = rhs;
            [unroll]
            for (uint k4 = 0; k4 < 3; k4++) {
                if ((int)k4 == myK) continue;
                float otherM = NodeEdgeDerivMult[faceNode[k4]];
                known += coef[k4] * otherM;
            }
            float coefMy = coef[myK];
            sumCoefSq += coefMy * coefMy;
            sumCoefTarget += -coefMy * known;
        }
    }

    const float RegEps = 0.05; // small Tikhonov anchor to the identity -- representative, not tuned
    float mRaw = (sumCoefTarget + RegEps * 1.0) / (sumCoefSq + RegEps);
    if (!isfinite(mRaw)) mRaw = 1.0;

    // Damped update -- see the file header for why (plain Jacobi on this
    // system isn't guaranteed to converge and was observed to overshoot).
    float mNew = mOld + RelaxFactor * (mRaw - mOld);

    const float MaxPlausibleMult = 20.0;
    if (!isfinite(mNew)) mNew = 1.0;
    mNew = clamp(mNew, -MaxPlausibleMult, MaxPlausibleMult);

    NodeEdgeDerivMultScratch[node] = mNew;
}
