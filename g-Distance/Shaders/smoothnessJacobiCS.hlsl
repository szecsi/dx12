#include "DistanceCb.hlsli"
#include "DistanceLattice.hlsli"

// Outer Lloyd-loop step 2: one Jacobi sweep over every (node, candidate-slot)
// unknown. All three energy terms are quadratic in the raw potentials, so
// this is exact Newton/Gauss-Newton per unknown (diagonal-only, ignoring
// off-diagonal coupling -- the standard Jacobi approximation), reading the
// previous sweep's snapshot from NodePotential and writing to
// NodePotentialScratch (commitPotentialCS.hlsl copies scratch back -- same
// read-stable/write-scratch/commit split as every other relaxation in this
// codebase).
//
// Term 1 (smoothness): for every pair of face-adjacent tets (both "fan"
// neighbors within one grid face, see buildTetsCS.hlsl, AND cross-orientation
// neighbors around the same A-edge, see buildTetFaceNeighborsCS.hlsl) that
// currently share the same active interface label pair (i,j), penalize
// ||gradA-gradB||^2 where grad = (grad phi_i - grad phi_j) is the tet's
// constant (affine-interpolation) gradient.
//
// Gathered per node via NodeIncidentTets: for each tet touching this node,
// all 4 of its face-neighbors (fanNext, fanPrev, and the 2 cross-neighbors)
// are candidate pairs. Each candidate pair (tetX,P) is processed by exactly
// one of the two tets' node-threads -- whichever tet has the smaller index
// treats itself as "tetA"; a node holding only the larger-indexed tet
// processes it as "tetB" itself, but only if the smaller-indexed partner
// is NOT also in its own incident list (in which case that other tet's own
// forward pass already covers it). This is the fix for a real gap in an
// earlier version of this pass: processing only (tetX, fanNext(tetX)) for
// each of a node's own tets silently drops any node that is a corner of a
// pair's *second* tet only (never its first) -- about a third of a fan's
// A-vertices, structurally, every sweep.
// Term 2 (margin hinge): A-nodes -- own/input label (slot 0) kept above
// every other candidate by MarginTarget. B-nodes -- whichever candidate is
// CURRENTLY winning kept above the runner-up by MarginTarget (no ground
// truth to anchor to, but B needs *some* floor on its commitment gap too,
// see the term's own comment below).
// Term 3 (regularizer): penalty pushing the SUM of a node's valid candidate
// potentials toward 0 (not each slot independently) -- see its own comment
// below for why. Also bounds dynamic range for the eventual (deferred)
// 8-bit quantization.
// Term 4 (volume conservation): pushes this node's reconstructed volume --
// summed, per incident tet, as a proportional share of that tet's winning-
// side sub-volume (TetPositiveSideVolume, DistanceLattice.hlsli) -- toward
// its Momentary Target Volume (NodeMTV, mtvSeedCS/mtvDiffuseCS/mtvCommitCS).
// Per-tet VPos and the same-side potential sum are frozen at this sweep's
// snapshot (same Gauss-Newton-style linearization as every other term
// here), giving a linear local model for the total volume in terms of this
// node's own unknowns.
#define SmoothnessSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "UAV(u6)," \
    "UAV(u7)," \
    "UAV(u8)," \
    "UAV(u9)"

RWStructuredBuffer<uint4>  Tets : register(u0);
RWStructuredBuffer<uint2>  TetInterfacePair : register(u1);
RWStructuredBuffer<uint>   NodeCandidateLabel : register(u2);
RWStructuredBuffer<float>  NodePotential : register(u3);
RWStructuredBuffer<float>  NodePotentialScratch : register(u4);
RWStructuredBuffer<uint>   NodeIncidentCount : register(u5);
RWStructuredBuffer<uint>   NodeIncidentTets : register(u6);
RWStructuredBuffer<uint2>  TetFaceNeighbors : register(u7);
RWStructuredBuffer<float>  NodeMTV : register(u8);
// This node's own winning-label reconstructed volume (volSum[ownSlot]),
// written every sweep -- mtvDiffuseCS.hlsl reads the round's last-written
// value for the volume-pushback term (VolumePushbackRate, DistanceCb.hlsli).
RWStructuredBuffer<float>  NodeCurrentVolume : register(u9);

int FindSlot(uint node, uint label)
{
    if (label == SENTINEL_LABEL) return -1;
    for (int s = 0; s < 8; s++)
        if (NodeCandidateLabel[node * 8 + (uint)s] == label) return s;
    return -1;
}

// A missing candidate contributes 0 to that corner's field value for the
// label in question -- under the sum-to-0 regularizer (term 3 below) this
// is the semantically right fallback, not just a safe default: 0 is
// exactly the decision boundary in that scheme (whichever candidate is
// actually winning at any node is guaranteed positive, see term 3), so a
// label a node never tracked defaults to "definitely not winning" rather
// than an arbitrary value.
float GetPotBySlot(uint node, int slot)
{
    return (slot >= 0) ? NodePotential[node * 8 + (uint)slot] : 0.0;
}

float3 TetFieldGrad(uint4 tetNodes, float3 w[4], uint label)
{
    if (label == SENTINEL_LABEL) return float3(0, 0, 0);
    uint verts[4] = { tetNodes.x, tetNodes.y, tetNodes.z, tetNodes.w };
    float3 g = float3(0, 0, 0);
    for (uint c = 0; c < 4; c++) {
        int s = FindSlot(verts[c], label);
        g += GetPotBySlot(verts[c], s) * w[c];
    }
    return g;
}

// Accumulates node's gradient/diagonal contribution from the smoothness
// term of exactly one tet-pair (tetA,tetB) -- called at most once per pair
// per node (see the dedup rule in the caller).
void AccumulatePair(uint node, uint tetA, uint tetB, inout float grad[8], inout float diag[8])
{
    uint2 pairA = TetInterfacePair[tetA];
    uint2 pairB = TetInterfacePair[tetB];
    if (pairA.x == pairA.y) return;                                // no active interface in tetA
    if (pairA.x != pairB.x || pairA.y != pairB.y) return;          // combinatorics disagree this round

    uint4 tA = Tets[tetA];
    uint4 tB = Tets[tetB];
    uint li = pairA.x, lj = pairA.y;
    // Previously bailed out here whenever any of the 8 corners lacked li or
    // lj as an actual candidate (e.g. a B-node whose own cube never saw the
    // OTHER label, even though a neighboring cube's tet needs it) -- that
    // silently dropped the whole pair's smoothness constraint, leaving that
    // tet's geometry unconstrained by the solve entirely (it would still
    // render something, via extractSurfaceCS.hlsl's own fallback, but with
    // no optimization ever pulling it toward consistency with its
    // neighbors) -- this was the real cause of the wedge-shaped indents
    // that persisted even after fixing the A/B magnitude asymmetry. Now
    // just proceeds using GetPotBySlot's 0-fallback below instead.

    float3 PA[4] = { NodeWorldPos(tA.x), NodeWorldPos(tA.y), NodeWorldPos(tA.z), NodeWorldPos(tA.w) };
    float3 PB[4] = { NodeWorldPos(tB.x), NodeWorldPos(tB.y), NodeWorldPos(tB.z), NodeWorldPos(tB.w) };

    float3 wA[4]; TetShapeGradients(PA[0], PA[1], PA[2], PA[3], wA[0], wA[1], wA[2], wA[3]);
    float3 wB[4]; TetShapeGradients(PB[0], PB[1], PB[2], PB[3], wB[0], wB[1], wB[2], wB[3]);

    float3 gA = TetFieldGrad(tA, wA, li) - TetFieldGrad(tA, wA, lj);
    float3 gB = TetFieldGrad(tB, wB, li) - TetFieldGrad(tB, wB, lj);
    float3 diff = gA - gB;

    int cA = (tA.x == node) ? 0 : (tA.y == node) ? 1 : (tA.z == node) ? 2 : (tA.w == node) ? 3 : -1;
    int cB = (tB.x == node) ? 0 : (tB.y == node) ? 1 : (tB.z == node) ? 2 : (tB.w == node) ? 3 : -1;
    float3 deltaWA = (cA >= 0) ? wA[cA] : float3(0, 0, 0);
    float3 deltaWB = (cB >= 0) ? wB[cB] : float3(0, 0, 0);
    float3 K = deltaWA - deltaWB;

    int si = FindSlot(node, li);
    int sj = FindSlot(node, lj);
    float dK = dot(diff, K);
    float kk = dot(K, K);

    if (si >= 0) {
        grad[si] += 2.0 * SmoothnessWeight * dK;
        diag[si] += 2.0 * SmoothnessWeight * kk;
    }
    if (sj >= 0) {
        grad[sj] += -2.0 * SmoothnessWeight * dK;
        diag[sj] += 2.0 * SmoothnessWeight * kk;
    }
}

bool IsInMyIncidentList(uint node, uint incCount, uint tetIdx)
{
    for (uint e = 0; e < incCount; e++)
        if (NodeIncidentTets[node * MAX_INCIDENT_TETS + e] == tetIdx) return true;
    return false;
}

[RootSignature(SmoothnessSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void smoothnessJacobiCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;

    float grad[8] = { 0,0,0,0,0,0,0,0 };
    float diag[8] = { 0,0,0,0,0,0,0,0 };
    float volSum[8] = { 0,0,0,0,0,0,0,0 };
    float kSum[8] = { 0,0,0,0,0,0,0,0 };

    // Term 3: sum-to-0 regularizer -- pushes the SUM of this node's valid
    // candidate potentials toward 0, instead of shrinking each slot toward
    // 0 independently. For the (current) 2-label case this makes
    // phi_i-phi_j exactly 2*phi_i (since phi_j=-phi_i), i.e. the interface
    // is exactly the zero-level-set of phi_i itself -- no affine shift
    // needed, unlike a sum-to-1 target -- and whichever candidate is
    // actually winning at a node is guaranteed positive (phi_i>phi_j=-phi_i
    // implies phi_i>0). Just as importantly, it gives every node, A or B
    // alike, the same fixed potential "budget" centered at 0. The original
    // independent-shrink-to-0-per-slot version had no such anchor for
    // B-nodes (only A-nodes get the margin hinge below), so B winners
    // drifted down near 0 while A winners sat near OwnLabelSeed;
    // extractSurfaceCS.hlsl's crossing point t=ga/(ga-gb) skews toward
    // whichever tet-edge endpoint has the smaller |g|, so that asymmetry
    // was pulling crossings almost onto weakly-committed B-nodes --
    // exactly the wedge-shaped indents observed in practice.
    {
        float sumPhi = 0.0;
        uint validCount = 0;
        for (uint s = 0; s < 8; s++) {
            if (NodeCandidateLabel[node * 8 + s] == SENTINEL_LABEL) continue;
            sumPhi += NodePotential[node * 8 + s];
            validCount++;
        }
        if (validCount > 0) {
            float residual = sumPhi; // target sum is 0
            for (uint s = 0; s < 8; s++) {
                if (NodeCandidateLabel[node * 8 + s] == SENTINEL_LABEL) continue;
                grad[s] += 2.0 * RegularizerWeight * residual;
                diag[s] += 2.0 * RegularizerWeight;
            }
        }
    }

    // Term 1: smoothness, gathered over this node's incident tets and all 4
    // of each one's face-neighbors (2 fan + 2 cross-orientation).
    uint incCount = min(NodeIncidentCount[node], MAX_INCIDENT_TETS);
    for (uint e = 0; e < incCount; e++) {
        uint tetX = NodeIncidentTets[node * MAX_INCIDENT_TETS + e];

        // Term 4 gather: this tet's contribution to node's reconstructed
        // volume, if it currently has an active interface. Runs once per
        // incident tet (not deduped/paired like term 1 -- each tet's own
        // volume belongs to it alone, no shared-pair bookkeeping needed).
        {
            uint2 pr = TetInterfacePair[tetX];
            if (pr.x != pr.y) {
                uint li = pr.x, lj = pr.y;
                uint4 tX = Tets[tetX];
                uint vertsX[4] = { tX.x, tX.y, tX.z, tX.w };
                float3 PX[4] = {
                    NodeWorldPos(vertsX[0]), NodeWorldPos(vertsX[1]),
                    NodeWorldPos(vertsX[2]), NodeWorldPos(vertsX[3])
                };
                float phiLi[4], phiLj[4], g[4];
                int myCorner = -1;
                for (uint c = 0; c < 4; c++) {
                    phiLi[c] = GetPotBySlot(vertsX[c], FindSlot(vertsX[c], li));
                    phiLj[c] = GetPotBySlot(vertsX[c], FindSlot(vertsX[c], lj));
                    g[c] = phiLi[c] - phiLj[c];
                    if (vertsX[c] == node) myCorner = (int)c;
                }
                if (myCorner >= 0) {
                    bool posSideX[4]; uint countPosX; float Vtet, VPos;
                    TetPositiveSideVolume(PX, g, posSideX, countPosX, Vtet, VPos);

                    const float epsFloor = 1.0e-4;
                    bool mySide = posSideX[myCorner];
                    float VSide = mySide ? VPos : (Vtet - VPos);
                    uint sideLabel = mySide ? li : lj;

                    float sumSide = 0.0;
                    for (uint c2 = 0; c2 < 4; c2++) {
                        if (posSideX[c2] != mySide) continue;
                        sumSide += max(mySide ? phiLi[c2] : phiLj[c2], epsFloor);
                    }

                    int mySlot = FindSlot(node, sideLabel);
                    if (mySlot >= 0 && sumSide > epsFloor) {
                        float myPhiSide = mySide ? phiLi[myCorner] : phiLj[myCorner];
                        float myShare = VSide * max(myPhiSide, epsFloor) / sumSide;
                        float K = VSide / sumSide;
                        volSum[mySlot] += myShare;
                        kSum[mySlot] += K;
                    }
                }
            }
        }

        uint tetBase = (tetX / 4) * 4;
        uint slot = tetX % 4;
        uint2 cross = TetFaceNeighbors[tetX];

        uint partners[4] = {
            tetBase + ((slot + 1) % 4), // fan next
            tetBase + ((slot + 3) % 4), // fan prev
            cross.x,
            cross.y,
        };

        for (uint pi = 0; pi < 4; pi++) {
            uint P = partners[pi];
            if (P == SENTINEL_LABEL) continue;

            if (tetX < P) {
                AccumulatePair(node, tetX, P, grad, diag);
            } else if (tetX > P) {
                // Only process here if P's own forward pass won't already
                // cover this pair -- i.e. P is not also one of my own
                // incident tets (if it is, tetX will be reached again in
                // this same loop from a node that had P first, or already
                // was, via P<tetX there).
                if (!IsInMyIncidentList(node, incCount, P)) {
                    AccumulatePair(node, P, tetX, grad, diag);
                }
            }
        }
    }

    // Term 4: apply the volume-conservation gradient accumulated during the
    // gather above. volSum[s]/kSum[s] are this node's TOTAL reconstructed
    // volume/sensitivity for candidate label s, summed across every
    // incident tet where s was the winning side -- linear in phi(node,s)
    // under this sweep's frozen-VPos/frozen-sumSide approximation, so the
    // (volSum-MTV)^2 loss contributes exactly like every other quadratic
    // term in this file.
    for (uint s4 = 0; s4 < 8; s4++) {
        if (kSum[s4] == 0.0) continue;
        float residual = volSum[s4] - NodeMTV[node];
        grad[s4] += 2.0 * VolumeWeight * residual * kSum[s4];
        diag[s4] += 2.0 * VolumeWeight * kSum[s4] * kSum[s4];
    }

    // Record this node's own current winning-label volume for mtvDiffuseCS's
    // volume-pushback term. "Own winning label" here means the same argmax
    // TopLabelOf uses elsewhere -- not necessarily an active-interface slot,
    // so volSum for that slot may legitimately be 0 (no active pair touched
    // it this sweep).
    {
        int ownSlot = -1;
        float ownPot = -1.0e30;
        for (uint s5 = 0; s5 < 8; s5++) {
            if (NodeCandidateLabel[node * 8 + s5] == SENTINEL_LABEL) continue;
            float p = NodePotential[node * 8 + s5];
            if (p > ownPot) { ownPot = p; ownSlot = (int)s5; }
        }
        NodeCurrentVolume[node] = (ownSlot >= 0) ? volSum[ownSlot] : 0.0;
    }

    // Term 2: margin hinge. A-nodes: own/input label (slot 0) must stay
    // above every other candidate by MarginTarget -- this is a correctness
    // anchor (corrects a wrong current winner back toward ground truth).
    // B-nodes get the analogous but weaker claim: whichever candidate is
    // CURRENTLY winning must stay ahead of the runner-up by MarginTarget.
    // Without this, nothing bounds a B-node's commitment gap away from
    // zero -- the sum-to-0 regularizer (term 3) only anchors phi_i+phi_j
    // toward 0, never phi_i-phi_j, and smoothness (term 1) only
    // constrains how that gap's *gradient* varies between tets, never its
    // absolute size at a single node. A near-zero gap is a "winner" in name
    // only (visibly tiny ball size) and numerically twitchy in
    // extractSurfaceCS.hlsl's crossing-point math -- this is what was
    // producing the wedge-shaped indents even after the earlier fixes.
    if (node < ACount) {
        float phi0 = NodePotential[node * 8 + 0];
        for (uint s = 1; s < 8; s++) {
            if (NodeCandidateLabel[node * 8 + s] == SENTINEL_LABEL) continue;
            float phiS = NodePotential[node * 8 + s];
            float violation = MarginTarget - (phi0 - phiS);
            if (violation > 0.0) {
                grad[0] += -2.0 * MarginWeight * violation;
                diag[0] += 2.0 * MarginWeight;
                grad[s] += 2.0 * MarginWeight * violation;
                diag[s] += 2.0 * MarginWeight;
            }
        }
    } else {
        int topSlot = -1;
        float topPot = -1.0e30;
        for (uint s = 0; s < 8; s++) {
            if (NodeCandidateLabel[node * 8 + s] == SENTINEL_LABEL) continue;
            float p = NodePotential[node * 8 + s];
            if (p > topPot) { topPot = p; topSlot = (int)s; }
        }
        if (topSlot >= 0) {
            for (uint s = 0; s < 8; s++) {
                if ((int)s == topSlot) continue;
                if (NodeCandidateLabel[node * 8 + s] == SENTINEL_LABEL) continue;
                float phiS = NodePotential[node * 8 + s];
                float violation = MarginTarget - (topPot - phiS);
                if (violation > 0.0) {
                    grad[topSlot] += -2.0 * MarginWeight * violation;
                    diag[topSlot] += 2.0 * MarginWeight;
                    grad[s] += 2.0 * MarginWeight * violation;
                    diag[s] += 2.0 * MarginWeight;
                }
            }
        }
    }

    for (uint s = 0; s < 8; s++) {
        float phi = NodePotential[node * 8 + s];
        float newPhi = phi;
        if (NodeCandidateLabel[node * 8 + s] != SENTINEL_LABEL) {
            // Hard step clamp -- plain per-unknown-diagonal Jacobi on this
            // coupled system (each unknown's diagonal ignores that the other
            // unknowns feeding the same residual are moving simultaneously
            // this same sweep) overshoots and diverges without one. Same
            // role/necessity as g-Aequor's MaxStep.
            float step = -grad[s] / (diag[s] + JacobiDiagEpsilon);
            step = clamp(step, -MaxPotentialStep, MaxPotentialStep);
            newPhi = phi + step;
        }
        NodePotentialScratch[node * 8 + s] = newPhi;
    }
}
