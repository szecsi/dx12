#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"
#include "SyntheticField.hlsli"

// Tile-based single-label/potential "synthetic field" smoothing -- modeled
// on smoothnessJacobiBlockCS.hlsl's tile/groupshared architecture (same tile
// layout, same closed-form 27-tap kernel table) but structurally simpler:
// every node carries exactly ONE (label, potential) pair, not up to
// MAX_CANDIDATES(8), so there's no per-tile authority vote, no missing-
// candidate handling, no "authority node lacks 2 candidates" bail-out --
// every node always has a valid label+potential, so every tile always
// writes all 16 of its targets every sweep.
//
// Per target, the 27-tap kernel (self=192, face=16, edge=8, cross=-48, same
// table/offsets as smoothnessJacobiBlockCS.hlsl) sums each tap's potential
// SIGNED by whether that neighbor's label agrees with the target's own
// current label (+1 agree, -1 disagree; the self tap always agrees with
// itself, so it's always +192*ownPotential). That sum is a gradient exactly
// like the multi-candidate version's `total`, driving the SAME
// clamp-stepped Jacobi update (SmoothnessWeight/JacobiDiagEpsilon/
// MaxPotentialStep, all reused from DistanceCb.hlsli). If the corrected
// potential goes negative -- this node's label is no longer locally
// supported -- it's reflected (sign-flipped) for an A-node, which never
// changes label; for a B-node (no fixed ground truth) it instead picks a new
// label via SyntheticVote8 over its own 8 A-corners (SyntheticField.hlsli,
// the SAME formula buildSyntheticBCS.hlsl uses to seed a non-unanimous
// B-node at init) and its potential is reset to SyntheticEpsilon -- a real
// head start, not a near-zero floor, so its potential doesn't take dozens of
// sweeps (MaxPotentialStep-limited growth) to catch up to its long-settled
// same-region neighbors' scale.
//
// A second energy term penalizes crooked 3-label junctions: for every
// tet-tet interface triangle (touching this target) whose 3 corners carry 3
// pairwise-distinct labels, the triple-junction tangent direction
// (T = ga x gb + gb x gc + gc x ga, the cross product of the two interface
// planes' normals, from the 3 labels' own confidence-field gradients within
// ONE tet -- see TripleJunctionTangent below) is computed independently in
// each of the interface's two tets and the difference is minimized via a
// Gauss-Newton fold-in into the same grad/diag accumulator Term 1 uses --
// see the design notes (soft-stargazing-biscuit.md) for the full derivation
// (tet-tet interface counts verified computationally against this file's own
// GatherIncidentTets/GetFaceAdjacentPartner tables: 36 ring1-ring1 pairs + 24
// ring1-ring2 pairs per target, all provably reachable within the existing
// halo -- no HALO_DIM increase needed).
//
// A third term floors the potential field's own gradient magnitude at 1
// (an Eikonal-style regularizer) -- the junction term's cross(Ta,Tb)==0 is
// also trivially satisfied by the underlying confidence gradients collapsing
// to zero, and nothing else here resists that collapse. For each of a
// target's 24 ring-1 incident tets where the target is that tet's OWN
// STRICT local maximum of its own label's confidence field (i.e. this
// target is genuinely the "peak" there, not one of the other 3 corners --
// see the wid<24 block below), a one-sided Gauss-Newton penalty pushes
// |grad(confidence field)| up toward 1 if it's currently below that. Not a
// per-edge distance-matching term (an earlier design, rejected: forcing
// every edge's potential difference toward that edge's real length is only
// correct for edges parallel to the true gradient -- edges tangent to a
// level set should have ~zero difference, not +-edgeLength) -- this instead
// reconstructs the real gradient VECTOR per tet (same TetShapeGradients-
// based confidence-field construction the junction term above already uses)
// and floors its magnitude directly, sidestepping the edge-angle problem
// entirely. See the design notes (soft-stargazing-biscuit.md) for the full
// derivation, including why this uses Gauss-Newton (diag provably >=0)
// rather than the "freeze w=1-1/|grad|, solve a reweighted diffusion"
// approach a literal Euler-Lagrange treatment would suggest -- that weight
// is negative throughout this term's entire active region (|grad|<1),
// i.e. always anti-diffusion under this one-sided floor, a real
// oscillation risk under Jacobi that Gauss-Newton's squared diag avoids.
//
// (A fourth term, per-halo volume conservation against each label's
// ground-truth A-node footprint, was tried and then removed entirely after
// 3 rounds of increasingly complex weighting schemes -- see git history for
// the removed NodeSyntheticVolume/NodeSensitivity/NodeVolumeAlignment
// machinery if that's ever worth resurrecting.)
//
// Buffers are the SAME multi-candidate ones (NodeCandidateLabel/
// NodePotential/NodePotentialScratch), reinterpreted: byte 0 of
// NodeCandidateLabel[node*2+0] is the current label, byte 1 of that SAME
// uint is the scratch (next) label (avoids needing a whole new buffer just
// for double-buffering one byte), and slot 0 of NodePotential/
// NodePotentialScratch is the current/next potential. Bytes 2-3 and slots
// 1-7 are simply never touched in this mode.
// Profiling switches: define as 0 (via a /D compiler flag, or by editing the
// default here) to strip a term ENTIRELY, not just weight it to 0 -- a
// weight of 0 in the GUI still pays this term's full tet-walk/ALU cost every
// sweep, since the shader itself is unchanged; these let the HLSL compiler
// discard that work outright when isolating what the tile/groupshared/
// wave-op machinery costs vs. what each energy term itself costs. See
// smoothnessJacobiSyntheticSimpleCS.hlsl for the complementary comparison:
// the same Term-1-only (smoothing) math with no tile/shared mem/wave ops at
// all, one thread per target.
//#ifndef ENABLE_JUNCTION_TERM
//#define ENABLE_JUNCTION_TERM 1
//#endif
//#ifndef ENABLE_EIKONAL_TERM
//#define ENABLE_EIKONAL_TERM 1
//#endif

#define SmoothnessSyntheticSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u4)," \
    "RootConstants(num32BitConstants=2, b1)," \
    "CBV(b2)"

        cbuffer SyntheticConsts : register(b1)
        {
            uint UseLabelVote; // 1 = SyntheticVote8 over the 8 own A-corners, 0 = dumb binary flip (1-oldLabel), see the relabel block below
            // 0 = a B-node whose potential goes negative just reflects it
            // (sign-flip) and keeps its current label, exactly like an
            // A-node -- no relabel, no SyntheticEpsilon head-start. Lets you
            // isolate the smoothing/junction terms' effect on POTENTIALS
            // alone, without label churn as a confound. 1 = normal behavior
            // (the vote/flip + head-start below).
            uint AllowBFlips;
        };

        RWStructuredBuffer<uint> NodeCandidateLabel : register(u0);
        RWStructuredBuffer<float> NodePotential : register(u1);
        RWStructuredBuffer<float> NodePotentialScratch : register(u2);
        // Written DIRECTLY here (no scratch/commit indirection needed --
        // this shader never READS beta, so there's no within-dispatch hazard
        // to guard against) -- see the beta-tracking write below.
        RWStructuredBuffer<float> NodeAlienPotential : register(u4);

#define HALO_DIM 4u
// HALO_NODES = 135 = HALO_BBASE(71) + 4*HALO_DIM*HALO_DIM(64) -- 7 words of
// pure padding between the A block [0,63] and the B block [71,134]. This
// (and the B-block base itself, see inHaloPosToInHaloIdxB below) is chosen
// to break a shared-memory bank-conflict alias in the wid<27 kernel below:
// with the original base of 64 (a clean multiple of the 32-wide bank
// period), the 8 cross-sublattice ("cross", weight -48) taps -- encoded as
// HALO_BBASE-k for small k -- landed on the exact same banks as several
// same-sublattice taps' negated mirrors, driving a verified worst-case
// 3-way bank conflict on every warp's gPot[]/gLabel[] read here (Nsight
// Compute: ~48% "Short Scoreboard" stall on this block). Retuning ONLY the
// base (not HALO_DIM*HALO_DIM's 16-wide z-stride, which the halo LOAD phase
// below relies on staying exactly 16 -- two consecutive z-slices per warp
// are only conflict-free because they differ by exactly half the 32-bank
// period) cuts the worst case to 2-way (verified: 144 -> 96 conflicting-
// bank instances across the 16 real per-tile targets) with the Load phase
// itself mathematically unaffected. A handful of alternate designs (an
// independent z-stride, fully free per-slice placement, isolating the one
// remaining conflicting lane via a branch) were tried and rejected -- see
// the design notes (soft-stargazing-biscuit.md) -- 2-way appears to be a
// hard structural floor for this offset family, not just an artifact of
// under-searching.
#define HALO_BBASE 71u
#define HALO_NODES 135u // 64 A + 7 padding + 64 B
#define TARGET_NODES 16u // 8 A + 8 B

        groupshared uint gLabel[HALO_NODES];
        groupshared float gPot[HALO_NODES];
        groupshared float gTotal[TARGET_NODES];
        // This sweep's junction-straightness grad/diag contribution per
        // target -- see the wid<32 block below.
        groupshared float gJunctionGrad[TARGET_NODES];
        groupshared float gJunctionDiag[TARGET_NODES];
        // This sweep's Eikonal-floor grad/diag contribution per target --
        // see the wid<24 block below.
        groupshared float gEikonalGrad[TARGET_NODES];
        groupshared float gEikonalDiag[TARGET_NODES];

        // Edge (w8) and face (w16/self) magnitudes are UNCHANGED from the
        // original 64-base table -- only the 8 cross-sublattice (w-48)
        // magnitudes differ, recomputed as HALO_BBASE-{0,1,4,5,16,17,20,21}
        // instead of 64-{...} (see the HALO_BBASE comment above). Verified
        // by direct recomputation, not hand-derived -- see the design notes.
        static const uint4 kernelbits = uint4(
    3 | (5 << 5) | (12 << 10) | (15 << 15) | (17 << 20) | (20 << 25), // w 8
    0x00100401u, // w 192 | w 16
    0x47464337u, // w -48
    0x42363332u);

        uint posToIdxA(uint3 l)
        {
            return AIdx(l.x, l.y, l.z);
        }
        uint posToIdxB(uint3 l)
        {
            return BIdx(l.x, l.y, l.z);
        }

        uint inHaloPosToInHaloIdxA(uint3 l)
        {
            return l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM;
        }
        uint inHaloPosToInHaloIdxB(uint3 l)
        {
            return HALO_BBASE + l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM;
        }

        // Halo-local index of a q-space point within THIS tile's halo, or -1
        // if it falls outside it -- only possible at the true domain
        // boundary (interior tiles are proven, see the design notes, to
        // always stay within Chebyshev radius 1 of the target, well inside
        // this halo). Same "skip at the edge" convention
        // CubeLinearIndex/GetFaceAdjacentPartner already use elsewhere in
        // this file -- NOT the general-shader virtual-background-node
        // convention (this file never treats a SENTINEL_LABEL corner as a
        // real background node).
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

        // Local corner index (0..3) equal to `key` within a resolved
        // halo-index array, or -1 if absent.
        int FindHaloCorner(int halo[4], int key)
        {
            for (int c = 0; c < 4; c++) if (halo[c] == key) return c;
            return -1;
        }

[RootSignature(SmoothnessSyntheticSig)]
[numthreads(128, 1, 1)]
void smoothnessJacobiSyntheticCS(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex)
{
    uint3 haloOriginA = gid * 2;

    // Load: one thread per halo node, direct single-label/potential read --
    // no packed-candidate masking, no authority vote (nothing plays that
    // role here, every node's own single slot is unconditionally valid).
    {
        bool isBHalo = tid >= 64u;
        uint tidLocal = tid & 63u;
        uint3 inHaloPos = uint3(tidLocal % HALO_DIM, (tidLocal / HALO_DIM) % HALO_DIM, tidLocal / (HALO_DIM * HALO_DIM));
        uint3 pos = haloOriginA + inHaloPos;
        // B's valid per-axis range is [0,BDim-1] = [0,GridRes-2], one short of
        // A's [0,GridRes-1] -- but this halo's outermost corner (pos, computed
        // above) can reach GridRes-1 on any axis for the last tile of a
        // dispatch. Passed unclamped into posToIdxB/BIdx, that produced a
        // flat index up to ~BDim+BDim^2 past NodeCount's end -- an
        // out-of-bounds StructuredBuffer read landing in genuinely unmapped
        // GPU VA space, confirmed via an Nsight Aftermath crash dump
        // (Error_DMA_PageFault / MMU Fault Error, GPU PC inside this shader)
        // after the GridRes-change "hang" turned out to be this page fault
        // triggering an engine reset, not an actual infinite loop. Clamp to
        // the nearest valid B node instead -- a harmless duplicated read at
        // the halo's outer seam, same as any other edge-of-domain halo case.
        uint3 bPos = min(pos, uint3(BDim - 1u, BDim - 1u, BDim - 1u));
        uint idx = isBHalo ? posToIdxB(bPos) : posToIdxA(pos);
        uint inHaloIdx = isBHalo ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
        gLabel[inHaloIdx] = GetCandidateLabelAt(NodeCandidateLabel, idx, 0u);
        gPot[inHaloIdx] = NodePotential[idx * MAX_CANDIDATES + 0u];
    }
    GroupMemoryBarrierWithGroupSync();

    // Four warps, targets split in four groups -- same partition as
    // smoothnessJacobiBlockCS.hlsl, but only 1 output per target now (not 8
    // slots), so only ONE lane (wid==0) needs to perform the write.
            uint warpId = tid / 32u;
            uint wid = tid % 32u;
            if (wid < 27u)
            {
                for (int iTarget = warpId; iTarget < 16; iTarget += 4)
                {
                    uint local = iTarget & 7u;
                    uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
                    bool isB = iTarget >= 8u;
                    uint3 inHaloPos = inTilePos + 1u;
                    uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
                    uint myLabelAtTarget = gLabel[centerIdx];

                    uint wid18 = wid % 18u;
                    int offset;
                    float weight;
                    if (wid18 < 6u)
                    {
                        offset = kernelbits.x >> (wid18 * 5u) & 0x1Fu;
                        weight = 8.0;
                    }
                    else
                    {
                        offset = (kernelbits[(wid18 - 2u) / 4u] >> ((wid18 + 2u) % 4u) * 8u) & 0xFFu;
                        weight = 16.0;
                    }
                    bool negate = (wid >= 18u) || (isB && wid18 >= 10u);
                    if (negate)
                        offset = -offset;
                    if (wid18 >= 9u)
                        weight = 192.0;
                    if (wid18 >= 10u)
                        weight = -48.0;

                    uint neighborIdx = centerIdx + offset;
                    float signedVal = gPot[neighborIdx] * ((gLabel[neighborIdx] == myLabelAtTarget) ? 1.0 : -1.0);
                    float contrib = signedVal * weight;
                    gTotal[iTarget] = WaveActiveSum(contrib);
                }
            }

            // Junction-straightness: for every one of a target's 24 ring-1
            // incident tets (GatherIncidentTets) crossed with its 4
            // face-adjacent relations (GetFaceAdjacentPartner) -- 96 raw
            // (tet,relation) discoveries, all 32 lanes of the warp cover
            // them in 3 sub-passes (32*3=96) -- find the 3-label interface
            // triangle (if any) and fold its Gauss-Newton contribution in.
            // Every discovery is resolved to HALO-LOCAL indices only (see
            // ResolveHaloLocalIndex) -- proven sufficient for every interior
            // tile, see the design notes -- so this never touches a global
            // buffer beyond gLabel/gPot already loaded above.
            //
            // A ring1-ring1 pair (both tets touch the target -- 36 distinct
            // pairs per target) is discovered TWICE while walking the
            // target's own 24 tets (once from each member tet's side), so
            // its contribution is halved (pairWeight=0.5) to avoid double-
            // counting; a ring1-ring2 pair (partner tet doesn't touch the
            // target -- 24 distinct pairs) is discovered once (pairWeight=1).
#if ENABLE_JUNCTION_TERM
            {
                for (int iTarget = warpId; iTarget < 16; iTarget += 4)
                {
                    uint local = iTarget & 7u;
                    uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
                    bool isB = iTarget >= 8u;
                    uint3 inHaloPos = inTilePos + 1u;
                    uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
                    uint myLabelAtTarget = gLabel[centerIdx];
                    uint targetGlobalIdx = isB ? posToIdxB(haloOriginA + inHaloPos) : posToIdxA(haloOriginA + inHaloPos);

                    uint ring1Tets[MAX_INCIDENT_TETS];
                    uint ring1Count = GatherIncidentTets(targetGlobalIdx, ring1Tets);

                    float gradAccum = 0.0;
                    float diagAccum = 0.0;

                    [unroll]
                    for (uint k = 0; k < 3u; k++)
                    {
                        uint flatIdx = wid + 32u * k;
                        if (flatIdx >= 96u) continue;
                        uint tetLocalIdx = flatIdx / 4u;
                        uint relation = flatIdx % 4u;
                        if (tetLocalIdx >= ring1Count) continue;

                        uint tetA = ring1Tets[tetLocalIdx];
                        int3 qA0, qA1, qA2, qA3;
                        GetTetCornerQs(tetA, qA0, qA1, qA2, qA3);
                        int haloA[4] = {
                            ResolveHaloLocalIndex(qA0, haloOriginA),
                            ResolveHaloLocalIndex(qA1, haloOriginA),
                            ResolveHaloLocalIndex(qA2, haloOriginA),
                            ResolveHaloLocalIndex(qA3, haloOriginA)
                        };
                        if (haloA[0] < 0 || haloA[1] < 0 || haloA[2] < 0 || haloA[3] < 0) continue;

                        int myIdxA = FindHaloCorner(haloA, (int)centerIdx);
                        if (myIdxA < 0) continue;

                        uint tetB;
                        if (!GetFaceAdjacentPartner(tetA, relation, tetB)) continue;

                        int3 qB0, qB1, qB2, qB3;
                        GetTetCornerQs(tetB, qB0, qB1, qB2, qB3);
                        int haloB[4] = {
                            ResolveHaloLocalIndex(qB0, haloOriginA),
                            ResolveHaloLocalIndex(qB1, haloOriginA),
                            ResolveHaloLocalIndex(qB2, haloOriginA),
                            ResolveHaloLocalIndex(qB3, haloOriginA)
                        };
                        if (haloB[0] < 0 || haloB[1] < 0 || haloB[2] < 0 || haloB[3] < 0) continue;

                        int myIdxB = FindHaloCorner(haloB, (int)centerIdx);
                        bool ring1Ring1 = (myIdxB >= 0);

                        int sharedA[3];
                        uint nShared = 0;
                        for (uint ca = 0; ca < 4u; ca++)
                        {
                            if (FindHaloCorner(haloB, haloA[ca]) >= 0)
                            {
                                if (nShared < 3u) sharedA[nShared] = haloA[ca];
                                nShared++;
                            }
                        }
                        if (nShared != 3u) continue;

                        uint l0 = gLabel[sharedA[0]];
                        uint l1 = gLabel[sharedA[1]];
                        uint l2 = gLabel[sharedA[2]];
                        if (l0 == l1 || l1 == l2 || l0 == l2) continue;

                        // Canonicalize to ascending label value. T is built
                        // from a cyclic-symmetric sum (ga x gb + gb x gc +
                        // gc x ga) that's invariant under a CYCLIC rotation
                        // of (l0,l1,l2) but flips SIGN under a single swap
                        // (transposition) of any two -- and which of those
                        // this discovery's (l0,l1,l2) happens to be is an
                        // accident of iteration order (sharedA[] is built by
                        // scanning tetA's corners for membership in tetB),
                        // which generally differs depending on which tet is
                        // "tetA". For a ring1-ring2 pair that's harmless (each
                        // such pair is only ever discovered once). But a
                        // ring1-ring1 pair IS discovered twice -- once from
                        // each member tet's own 24-tet ring -- and relies on
                        // BOTH halves computing the exact same sign of T for
                        // their pairWeight=0.5 contributions to reinforce
                        // into one full contribution rather than partially or
                        // fully CANCEL if the two discoveries' iteration
                        // orders happen to differ by an odd permutation.
                        // Sorting to a fixed canonical order makes (l0,l1,l2)
                        // -- and therefore T's sign -- depend only on the
                        // (unordered) set of 3 shared labels, identical
                        // regardless of which tet initiated the discovery.
                        if (l0 > l1) { uint t = l0; l0 = l1; l1 = t; }
                        if (l1 > l2) { uint t = l1; l1 = l2; l2 = t; }
                        if (l0 > l1) { uint t = l0; l0 = l1; l1 = t; }

                        float3 PA[4] = { QWorldPos(qA0), QWorldPos(qA1), QWorldPos(qA2), QWorldPos(qA3) };
                        float3 PB[4] = { QWorldPos(qB0), QWorldPos(qB1), QWorldPos(qB2), QWorldPos(qB3) };
                        float3 wA0, wA1, wA2, wA3;
                        TetShapeGradients(PA[0], PA[1], PA[2], PA[3], wA0, wA1, wA2, wA3);
                        float3 wB0, wB1, wB2, wB3;
                        TetShapeGradients(PB[0], PB[1], PB[2], PB[3], wB0, wB1, wB2, wB3);
                        float3 wA[4] = { wA0, wA1, wA2, wA3 };
                        float3 wB[4] = { wB0, wB1, wB2, wB3 };

                        // Per-label confidence-field gradient within one tet
                        // -- same convention as raymarchLatticePS.hlsl: every
                        // corner contributes its OWN real potential, sign-
                        // flipped by whether it carries this specific label.
                        float3 gaA = float3(0, 0, 0), gbA = float3(0, 0, 0), gcA = float3(0, 0, 0);
                        for (uint c1 = 0; c1 < 4u; c1++)
                        {
                            float pot = gPot[haloA[c1]];
                            uint lab = gLabel[haloA[c1]];
                            gaA += pot * ((lab == l0) ? 1.0 : -1.0) * wA[c1];
                            gbA += pot * ((lab == l1) ? 1.0 : -1.0) * wA[c1];
                            gcA += pot * ((lab == l2) ? 1.0 : -1.0) * wA[c1];
                        }
                        float3 gaB = float3(0, 0, 0), gbB = float3(0, 0, 0), gcB = float3(0, 0, 0);
                        for (uint c2 = 0; c2 < 4u; c2++)
                        {
                            float pot = gPot[haloB[c2]];
                            uint lab = gLabel[haloB[c2]];
                            gaB += pot * ((lab == l0) ? 1.0 : -1.0) * wB[c2];
                            gbB += pot * ((lab == l1) ? 1.0 : -1.0) * wB[c2];
                            gcB += pot * ((lab == l2) ? 1.0 : -1.0) * wB[c2];
                        }

                        // Triple-junction tangent: the cross product of the
                        // two interface planes' normals (a=b and a=c),
                        // expanded into the cyclic-symmetric form -- see the
                        // design notes for the algebraic derivation.
                        float3 Ta = cross(gaA, gbA) + cross(gbA, gcA) + cross(gcA, gaA);
                        float3 Tb = cross(gaB, gbB) + cross(gbB, gcB) + cross(gcB, gaB);

                        // Gauss-Newton: d(T^X)/d(myPot) = w_myIdxX^X x
                        // [(sc-sb)*ga^X + (sa-sc)*gb^X + (sb-sa)*gc^X],
                        // target only ever a corner of tetB when ring1-ring1.
                        float sa = (myLabelAtTarget == l0) ? 1.0 : -1.0;
                        float sb = (myLabelAtTarget == l1) ? 1.0 : -1.0;
                        float sc = (myLabelAtTarget == l2) ? 1.0 : -1.0;

                        float3 coeffA = (sc - sb) * gaA + (sa - sc) * gbA + (sb - sa) * gcA;
                        float3 dTa = cross(wA[myIdxA], coeffA); // d(Ta)/d(myPot)
                        float3 dTb = float3(0, 0, 0);           // d(Tb)/d(myPot) -- only nonzero when target is also a corner of tetB
                        if (ring1Ring1)
                        {
                            float3 coeffB = (sc - sb) * gaB + (sa - sc) * gbB + (sb - sa) * gcB;
                            dTb = cross(wB[myIdxB], coeffB);
                        }

                        // Residual C = cross(Ta,Tb), NOT Ta-Tb: zero exactly
                        // when Ta and Tb are parallel OR antiparallel (a
                        // straight junction line has no intrinsic "forward"
                        // direction, so antiparallel should count as aligned
                        // too), and it does NOT penalize a pure magnitude
                        // difference between the two tets' own tangent
                        // estimates (their magnitudes depend on tet size/
                        // shape/potential scale, not on how straight the
                        // junction actually is -- Ta-Tb would wrongly charge
                        // for that). Bonus: cross(sigma*Ta,sigma*Tb) =
                        // sigma^2*cross(Ta,Tb) = cross(Ta,Tb) for either sign
                        // of sigma, so this residual (and its derivative
                        // below) is automatically invariant to the same
                        // label-permutation sign ambiguity the canonical
                        // sort above already guards against -- an extra
                        // margin, not a replacement for that fix.
                        float3 C = cross(Ta, Tb);
                        float3 dC = cross(dTa, Tb) + cross(Ta, dTb); // d(C)/d(myPot), product rule

                        float pairWeight = ring1Ring1 ? 0.5 : 1.0;
                        gradAccum += pairWeight * 2.0 * dot(C, dC);
                        diagAccum += pairWeight * 2.0 * dot(dC, dC);
                    }

                    gJunctionGrad[iTarget] = WaveActiveSum(gradAccum);
                    gJunctionDiag[iTarget] = WaveActiveSum(diagAccum);
                }
            }
#else
            {
                for (int iTarget = warpId; iTarget < 16; iTarget += 4)
                {
                    gJunctionGrad[iTarget] = 0.0;
                    gJunctionDiag[iTarget] = 0.0;
                }
            }
#endif

            // Eikonal floor: one of a target's 24 ring-1 incident tets per
            // lane (wid<24). For each, treat this target's own label as the
            // field of interest (G[c]=+-pot, sign-flipped by same/different
            // label, same convention as everywhere else in this file -- NOT
            // a same-label gate/exclusion) and check whether the target is
            // that tet's OWN strict local maximum of G -- i.e. every other
            // corner is genuinely lower, so this target is locally
            // responsible for maintaining downhill steepness there (if some
            // OTHER corner is higher instead, THAT corner gets its own turn
            // to enforce this when it's processed as the target). The
            // strict-max test is softened into a narrow ramp (gateWeight),
            // not a hard 0/1 switch, to avoid sweep-to-sweep chatter right
            // at a near-tie -- same reasoning as Term 1's own tap weights
            // and the file's existing epsFloor convention (see e.g. the
            // removed volume term's history, or DistanceCb.hlsli's Term 5
            // comment on why a hard sign flip near an ambiguous zero-
            // crossing is avoided elsewhere too).
#if ENABLE_EIKONAL_TERM
            {
                for (int iTarget = warpId; iTarget < 16; iTarget += 4)
                {
                    uint local = iTarget & 7u;
                    uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
                    bool isB = iTarget >= 8u;
                    uint3 inHaloPos = inTilePos + 1u;
                    uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
                    uint myLabelAtTarget = gLabel[centerIdx];
                    uint targetGlobalIdx = isB ? posToIdxB(haloOriginA + inHaloPos) : posToIdxA(haloOriginA + inHaloPos);

                    uint ring1Tets[MAX_INCIDENT_TETS];
                    uint ring1Count = GatherIncidentTets(targetGlobalIdx, ring1Tets);

                    float eikGradAccum = 0.0;
                    float eikDiagAccum = 0.0;

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
                                float G[4];
                                for (uint c = 0; c < 4u; c++)
                                    G[c] = gPot[haloX[c]] * ((gLabel[haloX[c]] == myLabelAtTarget) ? 1.0 : -1.0);

                                float maxOtherG = -3.402823466e+38F;
                                for (uint c2 = 0; c2 < 4u; c2++)
                                    if ((int)c2 != myIdx) maxOtherG = max(maxOtherG, G[c2]);

                                const float epsFloor = 1.0e-4;
                                float margin = G[myIdx] - maxOtherG;
                                float gateWeight = saturate(margin / epsFloor);

                                if (gateWeight > 0.0)
                                {
                                    float3 PX[4] = { QWorldPos(qX0), QWorldPos(qX1), QWorldPos(qX2), QWorldPos(qX3) };
                                    float3 wX0, wX1, wX2, wX3;
                                    TetShapeGradients(PX[0], PX[1], PX[2], PX[3], wX0, wX1, wX2, wX3);
                                    float3 wX[4] = { wX0, wX1, wX2, wX3 };

                                    float3 gradG = G[0] * wX[0] + G[1] * wX[1] + G[2] * wX[2] + G[3] * wX[3];
                                    float magG = length(gradG);

                                    if (magG < 1.0)
                                    {
                                        // d(G[myIdx])/d(myPot) is always exactly +1 here
                                        // (this target's OWN corner, in ITS OWN label's
                                        // field, always agrees with itself), so
                                        // d(gradG)/d(myPot) = wX[myIdx] directly, no sign
                                        // branch needed.
                                        float r = 1.0 - magG;
                                        float dmag_dmyPot = dot(gradG, wX[myIdx]) / max(magG, epsFloor);
                                        float dr_dmyPot = -dmag_dmyPot;
                                        eikGradAccum = gateWeight * 2.0 * r * dr_dmyPot;
                                        eikDiagAccum = gateWeight * 2.0 * dr_dmyPot * dr_dmyPot;
                                    }
                                }
                            }
                        }
                    }

                    gEikonalGrad[iTarget] = WaveActiveSum(eikGradAccum);
                    gEikonalDiag[iTarget] = WaveActiveSum(eikDiagAccum);
                }
            }
#else
            {
                for (int iTarget = warpId; iTarget < 16; iTarget += 4)
                {
                    gEikonalGrad[iTarget] = 0.0;
                    gEikonalDiag[iTarget] = 0.0;
                }
            }
#endif

            GroupMemoryBarrierWithGroupSync();

            uint iTarget = tid / 8u;
            uint local = iTarget & 7u;
            uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
            bool isB = iTarget >= 8u;
            uint3 inHaloPos = inTilePos + 1u;
            uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);

            uint targetGlobalIdx = isB ? posToIdxB(haloOriginA + inHaloPos) : posToIdxA(haloOriginA + inHaloPos);
            float myPot = gPot[centerIdx];
            uint myLabelAtTarget = gLabel[centerIdx];
            float w = SmoothnessWeight;
            float grad = w * gTotal[iTarget];
            float diag = w * 192.0;

            grad += JunctionWeight * gJunctionGrad[iTarget];
            diag += JunctionWeight * gJunctionDiag[iTarget];

            grad += EikonalWeight * gEikonalGrad[iTarget];
            diag += EikonalWeight * gEikonalDiag[iTarget];

            float step = clamp(-grad / (diag + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
            float newPot = myPot + step;
            uint newLabel = myLabelAtTarget;

            if (newPot < 0.0)
            {
                // A-nodes never relabel, so this reflected magnitude is their
                // final newPot. For a B-node this is only a placeholder,
                // overwritten below with a real head-start once it's actually
                // relabeled -- see SyntheticEpsilon's use after the vote.
                newPot = -newPot;
                if (isB && AllowBFlips != 0u)
                {
                    if (UseLabelVote == 0u)
                    {
                // Dumb binary flip -- isolates the two-label
                // case from the vote formula for testing.
                        newLabel = 1u - myLabelAtTarget;
                    }
                    else
                    {
                        uint c = tid % 8;
                        uint3 cornerHaloPos = inHaloPos + uint3(c & 1u, (c >> 1) & 1u, (c >> 2) & 1u);
                        uint cornerIdx = inHaloPosToInHaloIdxA(cornerHaloPos);
                        uint label8 = gLabel[cornerIdx];
                        // Group by the CANDIDATE's own label (label8), not
                        // myLabelAtTarget -- the latter is identical across all 8
                        // corners of a target, so keying on it collapses every corner
                        // into a single WaveMatch group regardless of which label it
                        // actually holds, and the "vote" degenerates to picking
                        // whichever corner lands in a fixed lane slot instead of
                        // comparing distinct candidate labels.
                        uint label8WithTarget = label8 | (iTarget << 8u);
                        float pot8 = (label8 != myLabelAtTarget) ? gPot[cornerIdx] : 0.0;
                        uint sameLabelSameTargetMask = WaveMatch(label8WithTarget);
                        float totalPot8 = WaveMultiPrefixSum(sameLabelSameTargetMask, pot8) + pot8;
                        uint lastlane = firstbithigh(sameLabelSameTargetMask.x);
                        // Ballot on "am I my subgroup's representative lane"
                        // (wid==lastlane), not on the raw lane index (which is
                        // nonzero for virtually every lane, so WaveActiveBallot(lastlane)
                        // marks almost the whole warp as "representatives"). Then
                        // restrict to this target's own byte-aligned 8-lane slice of
                        // the warp -- shift by BYTES (*8u), not bits, or this mask
                        // pulls in neighboring targets' representative lanes too.
                        uint lastlanesForThisTargetMask = WaveActiveBallot(wid == lastlane).x & (0xffu << ((iTarget % 4) * 8u));
                        bool candidate =    (lastlanesForThisTargetMask & (1u << wid)) != 0;

                        float bestValue = candidate ? totalPot8 : -3.402823466e+38F;
                        uint bestLane = wid;

                        [unroll]
                        for (uint offset = 1; offset <= 4; offset <<= 1)
                        {
                            uint otherIndex = wid ^ offset;

                            float otherValue = WaveReadLaneAt(bestValue, otherIndex);
                            uint otherLane = WaveReadLaneAt(bestLane, otherIndex);

                            if (otherValue > bestValue)
                            {
                                bestValue = otherValue;
                                bestLane = otherLane;
                            }
                        }
                        // Broadcast the winner to all 8 lanes sharing this target,
                        // not just wid==bestLane -- all 8 threads redundantly write
                        // NodeCandidateLabel for this target below (unguarded), so
                        // they must all agree on newLabel or that write is a race
                        // between the correct winner and 7 stale (unchanged) values.
                        newLabel = WaveReadLaneAt(label8, bestLane);
                    }
                    // Head start, not the reflected magnitude above: a
                    // freshly relabeled B-node's potential would otherwise
                    // climb from near-MaxPotentialStep at a rate of at most
                    // +-MaxPotentialStep per sweep, staying far below its
                    // long-settled same-region neighbors' potentials for many
                    // sweeps -- and the closed-form volume formula's
                    // per-tet contribution is a PRODUCT of three
                    // myPot/(myPot+neighborPot) ratios, so that scale gap
                    // gets punished cubically, reporting a near-zero volume
                    // for a node that may genuinely hold real territory.
                    // SyntheticEpsilon (DistanceCb.hlsli) is exactly this
                    // head start, not a floor -- see its comment there.
                    newPot = SyntheticEpsilon;
                }
            }

            NodePotentialScratch[targetGlobalIdx * MAX_CANDIDATES + 0u] = newPot;
            uint word0 = NodeCandidateLabel[targetGlobalIdx * 2u + 0u];
            NodeCandidateLabel[targetGlobalIdx * 2u + 0u] = (word0 & 0xFFFF00FFu) | ((newLabel & 0xFFu) << 8u);

            // Keep EVERY node's beta tracking -phi every Phase-1 sweep --
            // unconditionally, regardless of the discriminator's enabled
            // state -- so a phi-only reconstruction (no alien/Stage-3 step
            // run since the last reset) renders the same smooth surface the
            // pre-alien-potential pipeline always gave, even with "Show
            // Alien Potential In Render" left on. -phi isn't the EXACT value
            // that makes CornerR3WayValue's gamma formula equal -phi (that's
            // the unbounded beta=-ln(2*sinh(phi)), blowing up as phi->0) --
            // it's the deliberately simpler, always-bounded choice, giving
            // gamma=-ln(2*cosh(phi)), which is provably always <= -phi and
            // converges to it for any non-tiny phi. A direct write (no
            // scratch/commit) is safe here -- this shader never READS
            // NodeAlienPotential, so there's no within-dispatch
            // read-during-write hazard to guard against.
            //
            // Deliberate consequence, by design (not a bug): this OVERWRITES
            // any real routed beta Stage 3 built up on a previously-enabled
            // node too -- running Phase-1-only rounds (RunContinue()) after
            // a joint/alien step (RunAlienStep()) resets every node's beta
            // back to -phi, discarding Stage 3's work. Intentional -- the
            // expected workflow is phi-only smoothing from a fresh reset,
            // THEN alien steps, never interleaved the other way.
            NodeAlienPotential[targetGlobalIdx] = -newPot;
        }
