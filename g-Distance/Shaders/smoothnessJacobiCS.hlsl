#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b1
#include "DistanceLattice.hlsli"

// Outer Lloyd-loop step 2: one Jacobi sweep over every (node, candidate-slot)
// unknown. All four energy terms are quadratic in the raw potentials, so
// this is exact Newton/Gauss-Newton per unknown (diagonal-only, ignoring
// off-diagonal coupling -- the standard Jacobi approximation), reading the
// previous sweep's snapshot from NodePotential and writing to
// NodePotentialScratch (commitPotentialCS.hlsl copies scratch back -- same
// read-stable/write-scratch/commit split as every other relaxation in this
// codebase).
//
// Term 1 (smoothness) is SIMPLIFIED FOR TESTING, hardcoded to GridRes==20:
// no active-label bookkeeping, no per-node exclusions, no runtime
// face-adjacency verification -- just the two fixed fields (label 0,
// label 1) smoothed everywhere in the domain, unconditionally, via a
// static tet-index partner table (PartnerOffset, below) valid only at
// CubeBoundDim=40. This deliberately strips out everything the
// edge-centric redesign (and the several rounds of guards/fixes layered on
// top of it) added, to re-verify the CORE gradient-mismatch math in
// isolation before re-introducing any of that complexity. `NodeFrozenWinner`
// /snapshotWinnerCS.hlsl are consequently UNUSED by Term 1 now (nothing
// here reads a node's own winning label at all) -- left wired up
// (buffer/root-sig slot still present) rather than ripped out, since nothing
// else currently depends on removing them.
//
// The traversal itself is back to the ORIGINAL (pre-edge-centric) shape:
// per node, walk its own incident tets (GatherIncidentTets, unchanged,
// still fully general/GridRes-agnostic), and for each one look up its 4
// face-adjacent partners via the static table -- with the same tetX<P /
// "not already in my incident list" dedup the original AccumulatePair
// used, so each face-adjacent pair is counted exactly once, no correction
// weight needed.
//
// Term 4 (volume floor) took the OTHER approach on purpose (per design
// discussion): a per-node SYNTHETIC field F(corner) = +winnerPotential if
// that corner's own current winner == this node's own winner, else
// -winnerPotential -- using only each corner's OWN winning label+potential
// (GetCornerTopLabel), no candidate-slot lookups, no missing-candidate
// fallback needed (every corner, real or virtual, always has a winner).
// TetPositiveSideVolume finds F's zero-crossing exactly as it already does
// for phi_li-phi_lj; the "positive" (F>=0) side is this node's own approximate
// volume share.
//
// SINGLE-POTENTIAL SCHEME (2-label testing only): rather than tracking
// phi(label0) and phi(label1) as two independently-relaxed unknowns kept
// near-mirrored by a SOFT sum-to-0 spring (the old Term 3), phi(label0) is
// now HARD-DEFINED as -phi(label1) every sweep -- one real degree of
// freedom per node, not two. Term 1 and Term 4 below are left completely
// UNCHANGED: they still independently write gradient/diagonal into
// whichever slot holds label0 and whichever holds label1 (cheap, and nice
// existing antisymmetry: Term 1's dK contributes +w to one slot and -w to
// the other by construction, same shape for Term 4's ownSlot/otherSlot
// pushback). What changed is how that dual output gets CONSUMED: the
// final-write block below folds both slots' (grad,diag) into a single
// combined (gradPhi,diagPhi) via the chain rule (phi1=phi, phi0=-phi =>
// dE/dphi = dE/dphi1 - dE/dphi0, d2E/dphi2 = d2E/dphi1^2 + d2E/dphi0^2,
// cross terms ignored same as every other diagonal-Jacobi term here), takes
// ONE Newton step, and mirrors the result into both slots -- exactly, not
// approximately. Term 3 (the old soft regularizer) is consequently
// redundant and commented out below, not deleted. Term 2 (margin hinge) WAS
// rewritten: instead of comparing an A-node's own slot against a runner-up
// slot, it pushes phi (the label-1 slot's value) past +MarginTarget/2 if
// this node's ground-truth label is 1, or past -MarginTarget/2 if it's 0 --
// an exact re-derivation of the old two-slot margin under phiOther=-phi0
// (old: phi0-phiOther>=MarginTarget => 2*phi0>=MarginTarget).
#define SmoothnessSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "CBV(b1)"

RWStructuredBuffer<uint>   NodeCandidateLabel : register(u0);
RWStructuredBuffer<float>  NodePotential : register(u1);
RWStructuredBuffer<float>  NodePotentialScratch : register(u2);
// This node's own winning-label reconstructed volume, written every sweep
// -- read by the Picked Tet debug panel and by Term 4's own floor check.
RWStructuredBuffer<float>  NodeCurrentVolume : register(u3);
// ACount-sized (computeConnectingNodesCS.hlsl) -- only ever index with
// node<ACount.
RWStructuredBuffer<uint>   NodeIsConnecting : register(u4);
// Read-only here -- written once per outer round by snapshotWinnerCS.hlsl,
// see Term 1's header comment above.
RWStructuredBuffer<uint>   NodeFrozenWinner : register(u5);

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

// SIMPLIFIED-FOR-TESTING static partner table: for tet slot s (0-5), the 4
// FLAT TET-INDEX offsets (not spatial offsets) to its face-adjacent
// partners -- {fanNext, fanPrev, D0-cap, D1-cap} -- hardcoded for
// GridRes==20 specifically (CubeBoundDim=40). This is possible ONLY
// because CubeBoundDim is now assumed fixed: cubeLinearIndex's own strides
// are (X:1, Y:CubeBoundDim, Z:CubeBoundDim^2) tet-slots, i.e. (X:6,
// Y:240, Z:9600) flat tet-index units at CubeBoundDim=40 -- NOT
// compile-time constants in general (see the design discussion on why a
// node's Q doesn't reduce to a single static tet-index table for any
// GridRes). Derived from the exact same spatial D0CapOffset/
// D0CapTargetSlot/D1CapOffset/D1CapTargetSlot tables used before their
// removal, converted via those fixed strides -- cross-checked here by
// confirming D0-cap[s] and D1-cap[D0CapTargetSlot[s]] are exact negations
// of each other (the same round-trip consistency check that caught the
// original edgeMap's bug, still holds). NOT VALID at any other GridRes --
// would need recomputing, or reverting to GatherEdgeTets's general
// CubeLinearIndex-based approach, if GridRes changes again.
static const int PartnerOffset[6][4] = {
    // fanNext, fanPrev,  D0-cap, D1-cap
    {       1,       5,    -238,    10 }, // slot 0
    {       1,      -1,   -9596,     8 }, // slot 1
    {       1,      -1,   -9598,   238 }, // slot 2
    {       1,      -1,      -8,   242 }, // slot 3
    {       1,      -1,     -10,  9598 }, // slot 4
    {      -5,      -1,    -242,  9596 }, // slot 5
};

// Accumulates node's gradient/diagonal contribution from the smoothness
// term of exactly one face-adjacent tet-pair (tetA,tetB), for the fixed
// (li,lj) pair -- called once per pair (dedup via the caller's tetX<P /
// "not already in my own incident list" check, same as the original
// pre-edge-centric AccumulatePair). node is guaranteed to be a corner of
// both tetA and tetB (both are drawn from this node's own incident-tet
// list).
void AccumulateEdgePair(uint node, uint tetA, uint tetB, uint li, uint lj, inout float grad[6], inout float diag[6])
{
    int3 qA0, qA1, qA2, qA3; GetTetCornerQs(tetA, qA0, qA1, qA2, qA3);
    int3 qB0, qB1, qB2, qB3; GetTetCornerQs(tetB, qB0, qB1, qB2, qB3);
    uint refA[4] = { ResolveCorner(qA0), ResolveCorner(qA1), ResolveCorner(qA2), ResolveCorner(qA3) };
    uint refB[4] = { ResolveCorner(qB0), ResolveCorner(qB1), ResolveCorner(qB2), ResolveCorner(qB3) };

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

    float w = 2.0 * SmoothnessWeight;
    if (si >= 0) {
        grad[si] += w * dK;
        diag[si] += w * kk;
    }
    if (sj >= 0) {
        grad[sj] += -w * dK;
        diag[sj] += w * kk;
    }
}

[RootSignature(SmoothnessSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void smoothnessJacobiCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;

    float grad[6] = { 0,0,0,0,0,0 };
    float diag[6] = { 0,0,0,0,0,0 };

    // This node's own current winner -- needed by Term 4 (defines the
    // synthetic field's sign convention and which slot receives the volume
    // floor) and NodeCurrentVolume's bookkeeping. NOT used by Term 1
    // anymore (see its own header comment -- hardcoded to labels 0/1
    // unconditionally now). Computed once, up front.
    uint myLabel = SENTINEL_CANDIDATE;
    float myPot = -1.0e30;
    int ownSlot = -1;
    for (uint s0 = 0; s0 < MAX_CANDIDATES; s0++) {
        uint l = GetCandidateLabelAt(NodeCandidateLabel, node, s0);
        if (l == SENTINEL_CANDIDATE) continue;
        float p = NodePotential[node * MAX_CANDIDATES + s0];
        if (p > myPot) { myPot = p; myLabel = l; ownSlot = (int)s0; }
    }

    // Term 3 (sum-to-0 regularizer): SUPERSEDED by the single-potential
    // scheme (see header comment) -- phi(label0)=-phi(label1) is now a hard
    // identity enforced in the final-write block, not a soft spring. Kept
    // here, commented out, in case multi-label support (where "the other
    // potential is always the opposite" doesn't apply -- 3+ candidates have
    // no single antipode) comes back and this softer approach is needed
    // again.
    /*
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
    */

    // Term 1: SIMPLIFIED FOR TESTING -- see header comment. No active-pair
    // determination, no per-node exclusion, no face-adjacency verification:
    // every one of this node's own incident tets gets compared against its
    // 4 static-offset partners (PartnerOffset), unconditionally, always for
    // the fixed pair (label 0, label 1). Dedup (tetX<P, or "not already in
    // my own incident list" otherwise) is the same bookkeeping the original
    // pre-edge-centric AccumulatePair used, so each face-adjacent pair is
    // counted exactly once.
    {
        uint incidentTets[MAX_INCIDENT_TETS];
        uint incCount = GatherIncidentTets(node, incidentTets);
        for (uint e = 0; e < incCount; e++) {
            uint tetX = incidentTets[e];
            uint slot = tetX % 6;
            for (uint pi = 0; pi < 4; pi++) {
                uint P = (uint)((int)tetX + PartnerOffset[slot][pi]);
                if (tetX < P) {
                    AccumulateEdgePair(node, tetX, P, 0u, 1u, grad, diag);
                } else if (tetX > P) {
                    bool inMyList = false;
                    for (uint ee = 0; ee < incCount; ee++) if (incidentTets[ee] == P) { inMyList = true; break; }
                    if (!inMyList) AccumulateEdgePair(node, P, tetX, 0u, 1u, grad, diag);
                }
            }
        }
    }

    // Term 4 gather: synthetic-field volume approximation (see header
    // comment) over this node's own incident tets. F(corner) is defined
    // purely from that corner's OWN current winner vs myLabel -- no
    // candidate-slot lookup, no missing-candidate case, since
    // GetCornerTopLabel always returns SOME winner (real or the label-0/
    // potential-1.0 virtual-corner convention). The "positive" side of
    // TetPositiveSideVolume's split (F>=0, i.e. corners that also currently
    // agree with myLabel) is this node's own approximate volume share --
    // this uniformly covers what used to be two separate cases (active-
    // interface split vs "uniform tet" whole-volume assignment), since a
    // tet where ALL 4 corners agree with myLabel is simply countPos==4 here.
    float myCurrentVolume = 0.0;
    float myKSum = 0.0;
    if (ownSlot >= 0) {
        uint incidentTets[MAX_INCIDENT_TETS];
        uint incCount = GatherIncidentTets(node, incidentTets);
        const float epsFloor = 1.0e-4;
        for (uint e = 0; e < incCount; e++) {
            uint tetX = incidentTets[e];
            int3 qX0, qX1, qX2, qX3; GetTetCornerQs(tetX, qX0, qX1, qX2, qX3);
            uint refsX[4] = { ResolveCorner(qX0), ResolveCorner(qX1), ResolveCorner(qX2), ResolveCorner(qX3) };
            int myCorner = -1;
            for (uint c0 = 0; c0 < 4; c0++) if (refsX[c0] == node) myCorner = (int)c0;
            if (myCorner < 0) continue;

            float3 PX[4] = { QWorldPos(qX0), QWorldPos(qX1), QWorldPos(qX2), QWorldPos(qX3) };
            float F[4];
            for (uint c = 0; c < 4; c++) {
                uint cLabel; float cPot;
                GetCornerTopLabel(refsX[c], NodeCandidateLabel, NodePotential, cLabel, cPot);
                F[c] = (cLabel == myLabel) ? cPot : -cPot;
            }
            bool posSideX[4]; uint countPosX; float Vtet, VPos;
            TetPositiveSideVolume(PX, F, posSideX, countPosX, Vtet, VPos);

            if (!posSideX[myCorner]) continue; // my own corner doesn't even dominate its own field (myPot<=0) -- no share from this tet

            float sumSide = 0.0;
            for (uint c2 = 0; c2 < 4; c2++) {
                if (!posSideX[c2]) continue;
                sumSide += max(F[c2], epsFloor);
            }
            if (sumSide > epsFloor) {
                float myShare = VPos * max(F[myCorner], epsFloor) / sumSide;
                float K = VPos / sumSide;
                myCurrentVolume += myShare;
                myKSum += K;
            }
        }
    }
    NodeCurrentVolume[node] = myCurrentVolume;

    // Term 4 (volume floor): ONLY for nodes flagged NodeIsConnecting
    // (computeConnectingNodesCS.hlsl -- the sole local connector of their
    // same-label neighborhood; without one, a thin feature like the Line
    // test shape pinches apart into separate blobs under smoothness's
    // perpetual, never-converging push at any topologically point- or
    // line-like feature). This is a FLOOR, not a target: a one-sided hinge
    // that only activates when this node's own reconstructed volume dips
    // below VolumeFloor (default 1 unit, DistanceCb.hlsli) -- if it's
    // already at or above the floor, there is NO penalty at all.
    if (ownSlot >= 0 && node < ACount && NodeIsConnecting[node] != 0) {
        float violation = VolumeFloor - myCurrentVolume;
        if (violation > 0.0 && myKSum != 0.0) {
            grad[ownSlot] += -2.0 * VolumeWeight * violation * myKSum;
            diag[ownSlot] += 2.0 * VolumeWeight * myKSum * myKSum;

            // Symmetric pushback: pulling the winning slot UP toward the
            // volume floor must pull every OTHER live candidate slot DOWN by
            // a matching amount, mirroring Term 1/Term 2's already-exact
            // antisymmetry between a node's slots -- otherwise this term
            // (alone among the four) drags a node's candidate sum away from
            // 0 with nothing to restore it. Split evenly across the other
            // valid slots; for the common 2-candidate case this is an exact
            // mirror of ownSlot's own update above.
            uint otherCount = 0;
            for (uint so = 0; so < MAX_CANDIDATES; so++) {
                if (so == (uint)ownSlot) continue;
                if (GetCandidateLabelAt(NodeCandidateLabel, node, so) == SENTINEL_CANDIDATE) continue;
                otherCount++;
            }
            if (otherCount > 0) {
                float share = 1.0 / (float)otherCount;
                for (uint so2 = 0; so2 < MAX_CANDIDATES; so2++) {
                    if (so2 == (uint)ownSlot) continue;
                    if (GetCandidateLabelAt(NodeCandidateLabel, node, so2) == SENTINEL_CANDIDATE) continue;
                    grad[so2] += 2.0 * VolumeWeight * violation * myKSum * share;
                    diag[so2] += 2.0 * VolumeWeight * myKSum * myKSum;
                }
            }
        }
    }

    // Term 2: margin hinge, reformulated for the single-potential scheme
    // (see header comment). A-nodes only (correctness anchor pulling a
    // wrong current winner back toward ground truth) -- push phi (this
    // node's stored value at the label-1 slot) past +MarginTarget/2 if its
    // own ground-truth label is 1, past -MarginTarget/2 if it's 0. Writes
    // ONLY grad[s1]/diag[s1] -- label-0's slot (if present) is mirrored,
    // not independently stepped, in the final-write block below.
    if (node < ACount) {
        int s1 = FindSlot(node, 1u);
        if (s1 >= 0) {
            uint ownLabel = GetCandidateLabelAt(NodeCandidateLabel, node, 0);
            float sign = (ownLabel == 1u) ? 1.0 : -1.0;
            float phi = NodePotential[node * MAX_CANDIDATES + (uint)s1];
            float violation = 0.5 * MarginTarget - sign * phi;
            if (violation > 0.0) {
                grad[s1] += -2.0 * MarginWeight * violation * sign;
                diag[s1] += 2.0 * MarginWeight;
            }
        }
    } /*else {
        if (ownSlot >= 0) {
            for (uint s = 0; s < MAX_CANDIDATES; s++) {
                if ((int)s == ownSlot) continue;
                if (GetCandidateLabelAt(NodeCandidateLabel, node, s) == SENTINEL_CANDIDATE) continue;
                float phiS = NodePotential[node * MAX_CANDIDATES + s];
                float violation = MarginTarget - (myPot - phiS);
                if (violation > 0.0) {
                    grad[ownSlot] += -2.0 * MarginWeight * violation;
                    diag[ownSlot] += 2.0 * MarginWeight;
                    grad[s] += 2.0 * MarginWeight * violation;
                    diag[s] += 2.0 * MarginWeight;
                }
            }
        }
    }*/

    // Final write: SINGLE-POTENTIAL SCHEME (see header comment). Fold the
    // label-0/label-1 slots' independently-computed (grad,diag) into one
    // combined (gradPhi,diagPhi) via the chain rule (phi1=phi, phi0=-phi),
    // take a single Newton step, and mirror it into both slots exactly --
    // hard-enforcing phi0=-phi1 every sweep instead of Term 3's old soft
    // spring. Any slot beyond {label0,label1} (dormant under the current
    // 2-label testing setup; label 0/1 has no room for a third candidate
    // anyway, but kept for when multi-label support returns) falls back to
    // the original independent per-slot Jacobi step, unchanged.
    {
        bool mirrored[6] = { false, false, false, false, false, false };
        int s0 = FindSlot(node, 0u);
        int s1 = FindSlot(node, 1u);
        if (s1 >= 0) {
            float gradPhi = grad[s1] - ((s0 >= 0) ? grad[s0] : 0.0);
            float diagPhi = diag[s1] + ((s0 >= 0) ? diag[s0] : 0.0);
            float step = -gradPhi / (diagPhi + JacobiDiagEpsilon);
            step = clamp(step, -MaxPotentialStep, MaxPotentialStep);
            float newPhi1 = NodePotential[node * MAX_CANDIDATES + (uint)s1] + step;
            NodePotentialScratch[node * MAX_CANDIDATES + (uint)s1] = newPhi1;
            mirrored[s1] = true;
            if (s0 >= 0) {
                NodePotentialScratch[node * MAX_CANDIDATES + (uint)s0] = -newPhi1;
                mirrored[s0] = true;
            }
        }
        for (uint s = 0; s < MAX_CANDIDATES; s++) {
            if (mirrored[s]) continue;
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
}
