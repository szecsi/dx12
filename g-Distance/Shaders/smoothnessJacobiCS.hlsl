#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b1
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
// neighbors within one rhombohedral cube AND cross-cube neighbors across a
// cube's D0/D1 "cap" faces) that currently share the same active interface
// label pair (i,j), penalize ||gradA-gradB||^2 where grad = (grad phi_i -
// grad phi_j) is the tet's constant (affine-interpolation) gradient.
// Tet connectivity is not stored anywhere -- every tet's corners
// (GetTetCornerQs/ResolveCorner/QWorldPos), a node's incident tets
// (GatherIncidentTets), and a tet's cross-cube neighbors (GetCrossNeighbors)
// are all computed fresh every sweep from DistanceLattice.hlsli's q-space
// arithmetic; see that file's header comment. A corner outside the real
// grid resolves to a fixed virtual background node (label 0, potential
// 1.0) rather than needing any special "is this tet valid" branch.
//
// Gathered per node: for each tet touching this node, all 4 of its face-
// neighbors (fanNext, fanPrev, and the 2 cross-neighbors) are candidate
// pairs. Each candidate pair (tetX,P) is processed by exactly one of the
// two tets' node-threads -- whichever tet has the smaller index treats
// itself as "tetA"; a node holding only the larger-indexed tet processes
// it as "tetB" itself, but only if the smaller-indexed partner is NOT also
// in its own incident list (in which case that other tet's own forward
// pass already covers it). This is the fix for a real gap in an earlier
// version of this pass: processing only (tetX, fanNext(tetX)) for each of
// a node's own tets silently drops any node that is a corner of a pair's
// *second* tet only (never its first) -- about a third of a fan's
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
// Term 4 (volume floor, connecting nodes only): see the term's own comment
// below -- a one-sided hinge, not a target, replacing the earlier
// MTV-diffusion system entirely (removed: mtvSeedCS/mtvFlipDetectCS/
// mtvDiffuseCS/mtvCommitCS, NodeMTV, NodeSmoothPressure -- superseded by
// connecting-node pinning, which is what actually mattered).
#define SmoothnessSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "CBV(b1)"

RWStructuredBuffer<uint2>  TetInterfacePair : register(u0);
RWStructuredBuffer<uint>   NodeCandidateLabel : register(u1);
RWStructuredBuffer<float>  NodePotential : register(u2);
RWStructuredBuffer<float>  NodePotentialScratch : register(u3);
// This node's own winning-label reconstructed volume (volSum[ownSlot]),
// written every sweep -- read by the Picked Tet debug panel and by Term 4's
// own floor check below.
RWStructuredBuffer<float>  NodeCurrentVolume : register(u4);
// ACount-sized (computeConnectingNodesCS.hlsl) -- only ever index with
// node<ACount.
RWStructuredBuffer<uint>   NodeIsConnecting : register(u5);

// Finds the CURRENT (always-real, dispatched) node's own slot for a label
// -- distinct from DistanceLattice.hlsli's GetCornerPotential/
// GetCornerTopLabel, which resolve an arbitrary (possibly virtual) OTHER
// tet corner; this is specifically "where does MY OWN candidate array
// carry this label", needed for writing into grad[]/diag[] by slot index.
int FindSlot(uint node, uint label)
{
    if (label == SENTINEL_LABEL) return -1;
    for (int s = 0; s < (int)MAX_CANDIDATES; s++)
        if (GetCandidateLabelAt(NodeCandidateLabel, node, (uint)s) == label) return s;
    return -1;
}

float3 TetFieldGrad(uint refs[4], float3 w[4], uint label)
{
    if (label == SENTINEL_LABEL) return float3(0, 0, 0);
    float3 g = float3(0, 0, 0);
    for (uint c = 0; c < 4; c++) {
        g += GetCornerPotential(refs[c], label, NodeCandidateLabel, NodePotential, 0.0) * w[c];
    }
    return g;
}

// Accumulates node's gradient/diagonal contribution from the smoothness
// term of exactly one tet-pair (tetA,tetB) -- called at most once per pair
// per node (see the dedup rule in the caller).
void AccumulatePair(uint node, uint tetA, uint tetB, inout float grad[6], inout float diag[6])
{
    uint2 pairA = TetInterfacePair[tetA];
    uint2 pairB = TetInterfacePair[tetB];
    if (pairA.x == pairA.y) return;                                // no active interface in tetA
    if (pairA.x != pairB.x || pairA.y != pairB.y) return;          // combinatorics disagree this round
    uint li = pairA.x, lj = pairA.y;

    int3 qA0, qA1, qA2, qA3; GetTetCornerQs(tetA, qA0, qA1, qA2, qA3);
    int3 qB0, qB1, qB2, qB3; GetTetCornerQs(tetB, qB0, qB1, qB2, qB3);
    uint refA[4] = { ResolveCorner(qA0), ResolveCorner(qA1), ResolveCorner(qA2), ResolveCorner(qA3) };
    uint refB[4] = { ResolveCorner(qB0), ResolveCorner(qB1), ResolveCorner(qB2), ResolveCorner(qB3) };
    // Previously bailed out here whenever any of the 8 corners lacked li or
    // lj as an actual candidate (e.g. a B-node whose own cube never saw the
    // OTHER label, even though a neighboring cube's tet needs it) -- that
    // silently dropped the whole pair's smoothness constraint, leaving that
    // tet's geometry unconstrained by the solve entirely (it would still
    // render something, via extractSurfaceCS.hlsl's own fallback, but with
    // no optimization ever pulling it toward consistency with its
    // neighbors) -- this was the real cause of the wedge-shaped indents
    // that persisted even after fixing the A/B magnitude asymmetry. Now
    // just proceeds using GetCornerPotential's 0-fallback below instead
    // (which also transparently covers a virtual/out-of-grid corner).

    float3 PA[4] = { QWorldPos(qA0), QWorldPos(qA1), QWorldPos(qA2), QWorldPos(qA3) };
    float3 PB[4] = { QWorldPos(qB0), QWorldPos(qB1), QWorldPos(qB2), QWorldPos(qB3) };

    float3 wA[4]; TetShapeGradients(PA[0], PA[1], PA[2], PA[3], wA[0], wA[1], wA[2], wA[3]);
    float3 wB[4]; TetShapeGradients(PB[0], PB[1], PB[2], PB[3], wB[0], wB[1], wB[2], wB[3]);

    float3 gA = TetFieldGrad(refA, wA, li) - TetFieldGrad(refA, wA, lj);
    float3 gB = TetFieldGrad(refB, wB, li) - TetFieldGrad(refB, wB, lj);
    float3 diff = gA - gB;

    int cA = (refA[0] == node) ? 0 : (refA[1] == node) ? 1 : (refA[2] == node) ? 2 : (refA[3] == node) ? 3 : -1;
    int cB = (refB[0] == node) ? 0 : (refB[1] == node) ? 1 : (refB[2] == node) ? 2 : (refB[3] == node) ? 3 : -1;
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

bool IsInMyIncidentList(uint incCount, uint incidentTets[MAX_INCIDENT_TETS], uint tetIdx)
{
    for (uint e = 0; e < incCount; e++)
        if (incidentTets[e] == tetIdx) return true;
    return false;
}

[RootSignature(SmoothnessSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void smoothnessJacobiCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;

    float grad[6] = { 0,0,0,0,0,0 };
    float diag[6] = { 0,0,0,0,0,0 };
    float volSum[6] = { 0,0,0,0,0,0 };
    float kSum[6] = { 0,0,0,0,0,0 };

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
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            if (GetCandidateLabelAt(NodeCandidateLabel, node, s) == SENTINEL_CANDIDATE) continue;
            sumPhi += NodePotential[node * MAX_CANDIDATES + s];
            validCount++;
        }
        if (validCount > 0) {
            float residual = sumPhi; // target sum is 0
            for (uint s = 0; s < MAX_CANDIDATES; s++) {
                if (GetCandidateLabelAt(NodeCandidateLabel, node, s) == SENTINEL_CANDIDATE) continue;
                grad[s] += 2.0 * RegularizerWeight * residual;
                diag[s] += 2.0 * RegularizerWeight;
            }
        }
    }

    // Term 1: smoothness, gathered over this node's incident tets (computed
    // on the fly, see GatherIncidentTets) and all 4 of each one's face-
    // neighbors (2 fan + 2 cross-cube, GetCrossNeighbors). Also gathers
    // Term 4's per-tet volume contribution (see below) in the same pass,
    // since both need the same incident-tet list.
    uint incidentTets[MAX_INCIDENT_TETS];
    uint incCount = GatherIncidentTets(node, incidentTets);
    for (uint e = 0; e < incCount; e++) {
        uint tetX = incidentTets[e];

        // Term 4 gather: this tet's contribution to node's reconstructed
        // volume. Runs once per incident tet (not deduped/paired like term 1
        // -- each tet's own volume belongs to it alone, no shared-pair
        // bookkeeping needed). Two cases:
        //  - Active interface (pr.x != pr.y): the tet's positive/negative
        //    split is computed via TetPositiveSideVolume, and each side's
        //    volume is divided among its corners proportionally by
        //    potential (myShare = VSide*phi/sumSide) -- equivalent to
        //    finding, for each pair of same-side corners, the zero-crossing
        //    of a virtual field where one corner's potential is sign-
        //    flipped (t* = phiA/(phiA+phiB) for two corners A,B, which is
        //    exactly what this proportional formula reduces to).
        //  - Uniform tet (pr.x == pr.y, assignInterfacePairsCS.hlsl's
        //    "labelI==labelJ" no-active-interface sentinel): all 4 corners
        //    currently agree on one label, so there's no crossing to find --
        //    the WHOLE tet belongs to that shared label, and gets split
        //    among its 4 corners via the SAME proportional formula (VSide
        //    is simply the full Vtet here). Without this case, a uniform
        //    tet contributed NOTHING to any of its corners' CurrentVolume.
        {
            uint2 pr = TetInterfacePair[tetX];
            int3 qX0, qX1, qX2, qX3; GetTetCornerQs(tetX, qX0, qX1, qX2, qX3);
            uint refsX[4] = { ResolveCorner(qX0), ResolveCorner(qX1), ResolveCorner(qX2), ResolveCorner(qX3) };
            int myCorner = -1;
            for (uint c0 = 0; c0 < 4; c0++) if (refsX[c0] == node) myCorner = (int)c0;

            if (myCorner >= 0) {
                float3 PX[4] = { QWorldPos(qX0), QWorldPos(qX1), QWorldPos(qX2), QWorldPos(qX3) };
                const float epsFloor = 1.0e-4;

                if (pr.x != pr.y) {
                    uint li = pr.x, lj = pr.y;
                    float phiLi[4], phiLj[4], g[4];
                    for (uint c = 0; c < 4; c++) {
                        phiLi[c] = GetCornerPotential(refsX[c], li, NodeCandidateLabel, NodePotential, 0.0);
                        phiLj[c] = GetCornerPotential(refsX[c], lj, NodeCandidateLabel, NodePotential, 0.0);
                        g[c] = phiLi[c] - phiLj[c];
                    }
                    bool posSideX[4]; uint countPosX; float Vtet, VPos;
                    TetPositiveSideVolume(PX, g, posSideX, countPosX, Vtet, VPos);

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
                } else {
                    uint sharedLabel = pr.x;
                    float phiShared[4];
                    for (uint c3 = 0; c3 < 4; c3++) {
                        phiShared[c3] = GetCornerPotential(refsX[c3], sharedLabel, NodeCandidateLabel, NodePotential, 0.0);
                    }
                    float3 e1 = PX[1] - PX[0], e2 = PX[2] - PX[0], e3 = PX[3] - PX[0];
                    float Vtet = abs(dot(e1, cross(e2, e3))) / 6.0;

                    float sumAll = 0.0;
                    for (uint c4 = 0; c4 < 4; c4++) sumAll += max(phiShared[c4], epsFloor);

                    int mySlot2 = FindSlot(node, sharedLabel);
                    if (mySlot2 >= 0 && sumAll > epsFloor) {
                        float myShare2 = Vtet * max(phiShared[myCorner], epsFloor) / sumAll;
                        float K2 = Vtet / sumAll;
                        volSum[mySlot2] += myShare2;
                        kSum[mySlot2] += K2;
                    }
                }
            }
        }

        uint tetBase = (tetX / 6) * 6; // rhombohedral cube-based indexing: 6 tets/cube, always contiguous
        uint slot = tetX % 6;
        uint2 cross = GetCrossNeighbors(tetX);

        uint partners[4] = {
            tetBase + ((slot + 1) % 6), // fan next
            tetBase + ((slot + 5) % 6), // fan prev
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
                if (!IsInMyIncidentList(incCount, incidentTets, P)) {
                    AccumulatePair(node, P, tetX, grad, diag);
                }
            }
        }
    }

    // Determine this node's own current winning slot (same argmax
    // TopLabelOf uses elsewhere) once, for both recording NodeCurrentVolume
    // and (if this is a connecting node) applying the volume floor below.
    int ownSlot = -1;
    {
        float ownPot = -1.0e30;
        for (uint s5 = 0; s5 < MAX_CANDIDATES; s5++) {
            if (GetCandidateLabelAt(NodeCandidateLabel, node, s5) == SENTINEL_CANDIDATE) continue;
            float p = NodePotential[node * MAX_CANDIDATES + s5];
            if (p > ownPot) { ownPot = p; ownSlot = (int)s5; }
        }
    }
    float myCurrentVolume = (ownSlot >= 0) ? volSum[ownSlot] : 0.0;
    NodeCurrentVolume[node] = myCurrentVolume;

    // Term 4 (volume floor): ONLY for nodes flagged NodeIsConnecting
    // (computeConnectingNodesCS.hlsl -- the sole local connector of their
    // same-label neighborhood; without one, a thin feature like the Line
    // test shape pinches apart into separate blobs under smoothness's
    // perpetual, never-converging push at any topologically point- or
    // line-like feature). This is a FLOOR, not a target: a one-sided hinge
    // that only activates when this node's own reconstructed volume dips
    // below VolumeFloor (default 1 unit, DistanceCb.hlsli) -- if it's
    // already at or above the floor, there is NO penalty at all, so
    // smoothness stays completely free to let it (or a same-label
    // neighbor, e.g. a newly recruited B-node) grow further without any
    // resistance from this term. Reuses the SAME kSum sensitivity the
    // gather above already computed (K = d(volSum)/d(phi), same
    // Gauss-Newton-style frozen-this-sweep linearization as every other
    // term in this file).
    //
    // Supersedes the earlier MTV-diffusion system entirely (removed:
    // mtvSeedCS.hlsl, mtvFlipDetectCS.hlsl, mtvDiffuseCS.hlsl,
    // mtvCommitCS.hlsl, NodeMTV, NodeSmoothPressure, and the
    // VolumeWeight-vs-dynamically-redistributed-target design) -- that
    // whole apparatus did nothing measurable for the torus (needed 60+
    // rounds even before its own mass-conservation fix slowed it further)
    // and is fully subsumed, for the cases it actually mattered (Line and
    // similar thin features), by connecting-node pinning.
    if (ownSlot >= 0 && node < ACount && NodeIsConnecting[node] != 0) {
        float violation = VolumeFloor - myCurrentVolume;
        if (violation > 0.0 && kSum[ownSlot] != 0.0) {
            grad[ownSlot] += -2.0 * VolumeWeight * violation * kSum[ownSlot];
            diag[ownSlot] += 2.0 * VolumeWeight * kSum[ownSlot] * kSum[ownSlot];
        }
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
        float phi0 = NodePotential[node * MAX_CANDIDATES + 0];
        for (uint s = 1; s < MAX_CANDIDATES; s++) {
            if (GetCandidateLabelAt(NodeCandidateLabel, node, s) == SENTINEL_CANDIDATE) continue;
            float phiS = NodePotential[node * MAX_CANDIDATES + s];
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
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            if (GetCandidateLabelAt(NodeCandidateLabel, node, s) == SENTINEL_CANDIDATE) continue;
            float p = NodePotential[node * MAX_CANDIDATES + s];
            if (p > topPot) { topPot = p; topSlot = (int)s; }
        }
        if (topSlot >= 0) {
            for (uint s = 0; s < MAX_CANDIDATES; s++) {
                if ((int)s == topSlot) continue;
                if (GetCandidateLabelAt(NodeCandidateLabel, node, s) == SENTINEL_CANDIDATE) continue;
                float phiS = NodePotential[node * MAX_CANDIDATES + s];
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

    for (uint s = 0; s < MAX_CANDIDATES; s++) {
        float phi = NodePotential[node * MAX_CANDIDATES + s];
        float newPhi = phi;
        if (GetCandidateLabelAt(NodeCandidateLabel, node, s) != SENTINEL_CANDIDATE) {
            // Hard step clamp -- plain per-unknown-diagonal Jacobi on this
            // coupled system (each unknown's diagonal ignores that the other
            // unknowns feeding the same residual are moving simultaneously
            // this same sweep) overshoots and diverges without one. Same
            // role/necessity as g-Aequor's MaxStep.
            float step = -grad[s] / (diag[s] + JacobiDiagEpsilon);
            step = clamp(step, -MaxPotentialStep, MaxPotentialStep);
            newPhi = phi + step;
        }
        NodePotentialScratch[node * MAX_CANDIDATES + s] = newPhi;
    }
}
