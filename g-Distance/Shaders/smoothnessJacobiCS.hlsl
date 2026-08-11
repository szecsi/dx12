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
// Term 1 (smoothness) lists face-adjacent tet pairs one of TWO ways, picked
// by a GUI checkbox (UseEdgeWalkTraversal, DistanceCb.hlsli) -- both feed
// the same AccumulateEdgePair per pair found, just discover pairs
// differently:
//   EDGE-WALKING (UseEdgeWalkTraversal!=0): for each of a node's up to 14
//     actual geometric neighbors (GatherEdgeTets's NodeNeighborOffsets,
//     DistanceLattice.hlsli), GatherEdgeTets finds the (up to 6) tets
//     actually containing that edge, and every genuinely FACE-ADJACENT pair
//     within that set (sharing 3 of 4 corners, verified directly) gets the
//     gradient-mismatch penalty.
//   NODE-ADJACENT-TETS (UseEdgeWalkTraversal==0): walk this node's own
//     ~24 incident tets (GatherIncidentTets) directly and check every PAIR
//     of them for the same 3-vertex coincidence -- no edges involved at
//     all. See that block's own comment for how this differs from
//     edge-walking (misses the 2 "outlier" corners edge-walking also
//     misses, for the same underlying reason).
// assignInterfacePairsCS.hlsl/TetInterfacePair (the old cube-vote scheme)
// are gone entirely either way.
//
// This is the fix for the old design's core failure mode: a cube-level
// vote could make two face-adjacent tets from DIFFERENT cubes disagree on
// which pair was "active", silently dropping the smoothness constraint
// between them.
//
// Li is this node's own LIVE winner (myLabel, recomputed every sweep from
// the current NodePotential snapshot) -- the once-per-round FROZEN snapshot
// (NodeFrozenWinner, snapshotWinnerCS.hlsl) has been removed from Term 1.
// NodeFrozenWinner/snapshotWinnerCS.hlsl are consequently UNUSED again
// (buffer/root-sig slot/dispatch left wired up, not ripped out, same as
// every other "leave dead infrastructure in place" call this project has
// made) -- nothing here reads a frozen label anymore.
//
// There is no bail-out on the neighbor's own label anymore -- see the Term 1
// body's own comment for why (in short: the old scheme never actually
// gated on a tet's own corners agreeing either). Lj is read live, not
// hardcoded: for the edge-walking traversal it's the edge's other
// endpoint's own current winner; for the alternate node-adjacent-tets
// traversal (GUI checkbox, UseEdgeWalkTraversal) it's this node's own
// runner-up candidate. Works for any number of labels, not just 2.
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
// Term 2 (margin hinge) and Term 3 (regularizer) are unchanged -- both
// operate purely on this node's own candidate slots, never needed the
// combinatorics vote at all.
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
// Written once per outer round by snapshotWinnerCS.hlsl -- UNUSED here now,
// see Term 1's header comment above (frozen winner removed, back to live).
RWStructuredBuffer<uint>   NodeFrozenWinner : register(u5);

// Finds a REAL node's own slot for a label (works for any real node index,
// not just the dispatched thread's own -- it's a plain NodeCandidateLabel
// lookup) -- distinct from DistanceLattice.hlsli's GetCornerPotential/
// GetCornerTopLabel, which also handle an arbitrary (possibly virtual)
// corner; this is specifically "where does THIS node's own candidate array
// carry this label", needed both for writing into grad[]/diag[] by slot
// index (always called with the dispatched thread's own node for that) and
// for CornerTracksLabel below (called with an arbitrary tet corner).
int FindSlot(uint node, uint label)
{
    if (label == SENTINEL_LABEL) return -1;
    for (int s = 0; s < (int)MAX_CANDIDATES; s++)
        if (GetCandidateLabelAt(NodeCandidateLabel, node, (uint)s) == label) return s;
    return -1;
}

// Whether a tet CORNER (real node ref or virtual/SENTINEL_LABEL) genuinely
// tracks `label` as a real candidate, real potential value -- not the
// GetCornerPotential/TetFieldGrad missing-candidate 0.0 fallback. A virtual
// corner tracks only label 0 (the fixed background convention), nothing
// else.
bool CornerTracksLabel(uint cornerRef, uint label)
{
    if (cornerRef == SENTINEL_LABEL) return label == 0u;
    return FindSlot(cornerRef, label) >= 0;
}

float3 TetFieldGrad(uint refs[4], float3 w[4], uint label)
{
    if (label == SENTINEL_LABEL) return float3(0, 0, 0);
    float3 g = float3(0, 0, 0);
    for (uint c = 0; c < 4; c++) {
        g += GetCornerPotential(refs[c], label, NodeCandidateLabel, NodePotential, -1.0) * w[c];
    }
    return g;
}

// UNUSED (kept, not deleted): Term 1's node-adjacent-tets traversal now uses
// a brute all-pairs scan over GatherIncidentTets instead of this table-based
// lookup, so nothing calls GetFaceAdjacentPartner below anymore. Left in
// place in case a future traversal wants direct partner lookup again rather
// than an O(n^2) scan.
//
// Spatial (GridRes-agnostic) cap tables for a tet slot's two OUT-OF-CUBE
// face-adjacent partners -- D0-cap and D1-cap cross into a NEIGHBORING
// cube's tet; fanNext/fanPrev ((slot+1)%6 / (slot+5)%6) stay within the SAME
// cube, no table needed. Cube-ORIGIN offsets + a target slot, not flat
// tet-index offsets -- unlike the flat PartnerOffset table this project
// briefly used (removed once GridRes became a runtime GUI value: flat
// tet-index strides depend on CubeBoundDim, these spatial offsets don't).
// Re-derived from GetTetCornerQs' own per-slot corner assignment, verified
// via a standalone reciprocity check (D0-cap[s] and D1-cap[D0CapTargetSlot[s]]
// are exact negations of each other).
static const int3 D0CapOffset[6] = { { 0, -1, 0 }, { 0, 0, -1 }, { 0, 0, -1 }, { -1, 0, 0 }, { -1, 0, 0 }, { 0, -1, 0 } };
static const uint D0CapTargetSlot[6] = { 2, 5, 4, 1, 0, 3 };
static const int3 D1CapOffset[6] = { { 1, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1 } };
static const uint D1CapTargetSlot[6] = { 4, 3, 0, 5, 2, 1 };

// The 4 face-adjacent partners of tet `tetX`: relation 0=fanNext,
// 1=fanPrev (same cube), 2=D0cap, 3=D1cap (neighboring cube, via the tables
// above). Returns false if the partner cube falls outside the current
// grid's bounding box (edge of the domain), same convention CubeLinearIndex
// itself uses. UNUSED -- see the comment above.
bool GetFaceAdjacentPartner(uint tetX, uint relation, out uint P)
{
    P = 0u;
    uint cubeLin = tetX / 6u, slot = tetX % 6u;
    if (relation == 0u) { P = cubeLin * 6u + (slot + 1u) % 6u; return true; }
    if (relation == 1u) { P = cubeLin * 6u + (slot + 5u) % 6u; return true; }
    int3 C = CubeOriginFromLinear(cubeLin);
    uint lin;
    if (relation == 2u) {
        if (!CubeLinearIndex(C + D0CapOffset[slot], lin)) return false;
        P = lin * 6u + D0CapTargetSlot[slot];
        return true;
    }
    if (!CubeLinearIndex(C + D1CapOffset[slot], lin)) return false;
    P = lin * 6u + D1CapTargetSlot[slot];
    return true;
}

// Accumulates node's gradient/diagonal contribution from the smoothness
// term of exactly one face-adjacent tet-pair (tetA,tetB), for the (li,lj)
// pair the caller determined. Two callers, two guarantees: the edge-walking
// traversal's GatherEdgeTets only ever returns tets containing both of an
// edge's endpoints (one of which is this node), so node is guaranteed a
// corner of BOTH tetA and tetB there; the node-adjacent-tets traversal only
// guarantees node is a corner of whichever side came from its own
// GatherIncidentTets list (the other side's cA/cB below just resolves to
// -1, deltaW 0 -- a smaller, still-correct contribution, not an error).
//
// pairWeight corrects for a real double-counting bug in the outer edge
// loop: a face-adjacent pair (tetA,tetB) shares exactly 3 corners with
// node -- node itself, plus the two OTHER vertices Y,Z of the shared face
// {node,Y,Z} -- so this exact pair gets rediscovered once from the Y-edge
// iteration AND once from the Z-edge iteration whenever BOTH are active
// (proven combinatorially: the two tets' only common vertices are
// {node,Y,Z}, so those are the only 2 of node's edges that can ever find
// both tets at once). The caller determines, per pair, whether the OTHER
// discovering edge is also active and passes 0.5 in that case (so the two
// discoveries sum to exactly 1x) or 1.0 if this is the pair's only
// discovery -- NOT a flat weight, since near a thin/curved label boundary
// the 2x-vs-1x split correlates with local label majority, which is
// exactly what was producing the asymmetric erosion toward whichever label
// has more locally-active edges.
void AccumulateEdgePair(uint node, uint tetA, uint tetB, uint li, uint lj, float pairWeight, inout float grad[8], inout float diag[8])
{
    //if(li == lj) return;
            
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

    float w = pairWeight * 2.0 * SmoothnessWeight;
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

    float grad[8] = { 0,0,0,0,0,0,0,0 };
    float diag[8] = { 0,0,0,0,0,0,0,0 };

    // This node's own current winner -- needed by Term 1 (defines Li for
    // every edge), Term 4 (defines the synthetic field's sign convention
    // and which slot receives the volume floor), and NodeCurrentVolume's
    // bookkeeping. Computed once, up front (unlike the old per-tet-vote
    // version, nothing here depends on any other term running first).
    uint myLabel = SENTINEL_CANDIDATE;
    uint myOtherLabel = SENTINEL_CANDIDATE;            
    float myPot = -1.0e30;
    float myOtherPot = -1.0e30;
    int ownSlot = -1;
    uint myValidCount = 0;
    for (uint s0 = 0; s0 < MAX_CANDIDATES; s0++)
    {
        uint l = GetCandidateLabelAt(NodeCandidateLabel, node, s0);
        if (l == SENTINEL_CANDIDATE)
            continue;
        myValidCount++;
        float p = NodePotential[node * MAX_CANDIDATES + s0];
        if (p > myPot)
        {
            myOtherLabel = myLabel;
            myOtherPot = myPot;
            myPot = p;
            myLabel = l;
            ownSlot = (int) s0;
        }
        else if (p > myOtherPot)
        {
            myOtherLabel = l;
            myOtherPot = p;
        }
    }
            
    if(myValidCount == 0) return;

    // Term 3: sum-to-0 regularizer -- pushes the SUM of this node's valid
    // candidate potentials toward 0, instead of shrinking each slot toward
    // 0 independently. For the (current) 2-label case this makes
    // phi_i-phi_j exactly 2*phi_i (since phi_j=-phi_i), i.e. the interface
    // is exactly the zero-level-set of phi_i itself -- no affine shift
    // needed, unlike a sum-to-1 target -- and whichever candidate is
    // actually winning at a node is guaranteed positive (phi_i>phi_j=-phi_i
    // implies phi_i>0). Just as importantly, it gives every node, A or B
    // alike, the same fixed potential "budget" centered at 0.
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

    // Term 1: edge-centric smoothness -- see header comment. For each of
    // this node's up to 14 real geometric neighbors, an active edge (Li !=
    // Lj, using the FROZEN per-round snapshot, not the live winner) gets
    // its tet-fan gathered on the fly. Only genuinely FACE-ADJACENT pairs
    // within that fan (sharing 3 of 4 corners, not just the edge's own 2
    // endpoints) get the gradient-mismatch penalty -- verified directly by
    // comparing each pair's resolved corners, not by trusting any derived
    // ordering.
    //
    // Single-candidate nodes (myValidCount==1, e.g. an A-node deep enough
    // in its own ground-truth region that buildCandidatesCS never found a
    // competing label nearby) do NOT participate as the acting node here at
    // all -- they have no second hypothesis to smooth against in the first
    // place, and every "active" edge they'd process would evaluate
    // TetFieldGrad for a label they don't track, falling into
    // GetCornerPotential's 0.0 missing-candidate fallback and corrupting
    // the whole tet's computed gradient (the same failure mode this project
    // already found and fixed once for B-nodes, by widening their candidate
    // halo -- see buildCandidatesCS.hlsl's own header comment). Unlike a
    // multi-candidate node, a single-candidate node also has zero Term 2
    // margin-hinge defense (nothing to compare its one slot against), so
    // nothing was ever stopping that corrupted gradient from persistently
    // dragging its own ground-truth label negative every sweep -- confirmed
    // directly via a per-sweep probe (term1Grad0 was large and
    // consistently positive from sweep 1 onward, never flipping sign).
    // No bail-out on Li==Lj -- an edge's two endpoints agreeing doesn't mean
    // there's nothing to keep smooth, it means the OLD per-tet "uniform, no
    // crossing" case (which the old cube-vote scheme still fed a real
    // energy contribution whenever some OTHER corner of the same cube
    // disagreed -- see the design discussion's confirmation that the old
    // scheme never actually gated on a tet's own corners' agreement at
    // all). Every edge is processed unconditionally; Lj is this edge's
    // OTHER endpoint's own live winner (GetCornerTopLabel at myQ+D, same
    // helper Term 4 already uses), not a hardcoded `1-Li` -- works for any
    // number of labels, not just the current 2-label test domain. Li==Lj is
    // harmless when it happens: AccumulateEdgePair resolves both to the
    // SAME slot, so its +w*dK/-w*dK contributions land in the same grad[]
    // entry and cancel exactly (diag[] still grows, just a slightly more
    // conservative step that sweep -- no wrong bias).
    //
    // Because literally every edge is now processed (never skipped), every
    // face-adjacent pair is discovered from BOTH of its two discovering
    // edges unconditionally (the combinatorial proof in AccumulateEdgePair's
    // header comment: a pair's only common corners with node are the two
    // edge endpoints of its shared face) -- so the correction weight is a
    // flat, unconditional 0.5 for every pair, no per-pair "is the other
    // edge also active" check needed anymore.
    if (UseEdgeWalkTraversal != 0 && myValidCount > 1) {
        int3 myQ = NodeQ(node);
        for (uint ne = 0; ne < 14; ne++) {
            int3 D = NodeNeighborOffsets[ne];
            uint neighborRef = ResolveCorner(myQ + D);
            uint otherLabel; float otherPot;
            GetCornerTopLabel(neighborRef, NodeCandidateLabel, NodePotential, otherLabel, otherPot);
            //if(myLabel == otherLabel){
                otherLabel = myOtherLabel;
            //}

            uint edgeTets[6];
            uint n = GatherEdgeTets(myQ, D, edgeTets);

            uint fanRefs[6][4];
            for (uint t = 0; t < n; t++) {
                int3 tq0, tq1, tq2, tq3; GetTetCornerQs(edgeTets[t], tq0, tq1, tq2, tq3);
                fanRefs[t][0] = ResolveCorner(tq0); fanRefs[t][1] = ResolveCorner(tq1);
                fanRefs[t][2] = ResolveCorner(tq2); fanRefs[t][3] = ResolveCorner(tq3);
            }

            for (uint a = 0; a < n; a++) {
                for (uint b = a + 1; b < n; b++) {
                    uint sharedCount = 0;
                    for (uint ca = 0; ca < 4; ca++)
                        for (uint cb = 0; cb < 4; cb++)
                            if (fanRefs[a][ca] == fanRefs[b][cb]) sharedCount++;
                    if (sharedCount != 3) continue; // not a true face-adjacent pair

                    // Whole-pair protection, matching the OLD scheme's
                    // whole-TET exclusion (a bailed cube's tets were never
                    // read by ANYONE's Term 1, from any direction) -- if
                    // any of the 8 corner-slots across tetA/tetB doesn't
                    // genuinely track both labels, skip this pair entirely
                    // rather than silently mixing in GetCornerPotential's
                    // 0.0 missing-candidate fallback. Without this, a
                    // single-candidate node (excluded from ACTING in Term 1,
                    // decaying toward 0 via Term 3 alone with nothing
                    // opposing it) still gets its decayed value READ here
                    // whenever it's a shared corner of an active
                    // neighbor's tet-pair, leaking that decay into the
                    // actively-smoothed region via gradient-matching.
               /*     bool allTrack = true;
                    for (uint ct = 0; ct < 4 && allTrack; ct++) {
                        if (!CornerTracksLabel(fanRefs[a][ct], myLabel) || !CornerTracksLabel(fanRefs[a][ct], otherLabel)) allTrack = false;
                    }
                    for (uint ct2 = 0; ct2 < 4 && allTrack; ct2++) {
                        if (!CornerTracksLabel(fanRefs[b][ct2], myLabel) || !CornerTracksLabel(fanRefs[b][ct2], otherLabel)) allTrack = false;
                    }
                    if (!allTrack) continue;*/

                    AccumulateEdgePair(node, edgeTets[a], edgeTets[b], myLabel, otherLabel, 0.5, grad, diag);
                }
            }
        }
    }

    // Term 1, ALTERNATE traversal (UseEdgeWalkTraversal==0, GUI checkbox):
    // instead of walking this node's 14 geometric edges, walk its own
    // ~24 incident tets directly (GatherIncidentTets, same helper Term 4
    // uses below) and check EVERY PAIR of them for the SAME 3-vertex
    // coincidence the edge-walking traversal uses (sharedCount==3) -- no
    // GetFaceAdjacentPartner/cap tables, no relation enumeration, just the
    // brute all-pairs scan already proven correct by the edge-walking
    // block's own inner loop. Node-centric, not edge-centric -- there's no
    // per-edge "other endpoint" to read a live winner from here, so
    // otherLabel is simply this node's own runner-up candidate
    // (myOtherLabel), matching Term 2/Term 4's already node-local-only
    // quantities.
    //
    // Because a pair only ever shows up here when BOTH tets are ALSO in
    // node's own incident list (by construction of the all-pairs scan),
    // this traversal reaches the same 3 shared-face nodes per pair that
    // edge-walking does -- it can no longer reach the 2 "outlier" corners
    // (each a corner of only ONE of the pair's two tets) the
    // GetFaceAdjacentPartner version could, since an outlier's other tet is
    // by definition absent from ITS OWN incident list. Each pair appears
    // exactly once in the a<b scan (no duplicate entries in incidentTets),
    // so pairWeight is always 1.0 -- no 0.5 correction needed, unlike
    // edge-walking's two-discovering-edges-per-pair.
    if (UseEdgeWalkTraversal == 0 && myValidCount > 1) {
        uint incidentTets[MAX_INCIDENT_TETS];
        uint incCount = GatherIncidentTets(node, incidentTets);

        uint incRefs[MAX_INCIDENT_TETS][4];
        for (uint e = 0; e < incCount; e++) {
            int3 tq0, tq1, tq2, tq3; GetTetCornerQs(incidentTets[e], tq0, tq1, tq2, tq3);
            incRefs[e][0] = ResolveCorner(tq0); incRefs[e][1] = ResolveCorner(tq1);
            incRefs[e][2] = ResolveCorner(tq2); incRefs[e][3] = ResolveCorner(tq3);
        }

        for (uint a = 0; a < incCount; a++) {
            for (uint b = a + 1; b < incCount; b++) {
                uint sharedCount = 0;
                for (uint ca = 0; ca < 4; ca++)
                    for (uint cb = 0; cb < 4; cb++)
                        if (incRefs[a][ca] == incRefs[b][cb]) sharedCount++;
                if (sharedCount != 3) continue; // not a true face-adjacent pair

                AccumulateEdgePair(node, incidentTets[a], incidentTets[b], myLabel, myOtherLabel, 1.0, grad, diag);
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

    // Term 2: margin hinge. A-nodes: own/input label (slot 0) must stay
    // above every other candidate by MarginTarget -- this is a correctness
    // anchor (corrects a wrong current winner back toward ground truth).
    // B-nodes get the analogous but weaker claim: whichever candidate is
    // CURRENTLY winning must stay ahead of the runner-up by MarginTarget.
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
