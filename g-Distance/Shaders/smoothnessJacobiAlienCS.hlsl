#include "DistanceCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b2
#include "DistanceLattice.hlsli"

// Phase 2 of the "alien potential" secondary pass. Runs AFTER Phase 1
// (smoothnessJacobiSyntheticCS.hlsl) has fully converged. phi AND beta are
// BOTH live unknowns, jointly relaxed every sweep via the SAME Term-1
// kernel run twice (see CornerR3Way/the tap loop below) -- but LABEL AND
// ROUTING ARE NO LONGER FROZEN:
// this file now also owns B-node label flips and beta's routed-label choice
// for the alien phase, instead of freezing whatever
// gatherAlienDiscriminatorCS.hlsl decided once at the start.
//
// Why: freezing topology for the whole alien phase made beta chase a target
// (whichever label gather picked) that Phase 1 itself is still allowed to
// move out from under it (B-nodes flip there too) -- "getting the betas to
// conform to the topology proved intractable." The fix: let B-nodes flip
// here too, using the exact same relabel mechanism Phase 1 already proves
// works, and keep beta's routing coherent as the owner flips underneath it.
//
// The mechanism (every sweep, after each of phi/beta's own Term-1 step):
// - `phi < beta` is the exact generalization of Phase 1's own `phi<0`
//   relabel trigger (`-phi` was always the implicit stand-in for "everything
//   else" before beta existed as a real tracked value; `phi<0` was always
//   really `phi<-phi`). For a B-node this triggers a SWAP: own<->beta
//   exchange both label and value (the two just-computed numbers trade
//   which slot holds which) -- no arbitrary fresh-relabel epsilon stub
//   needed, since a swap hands the promoted slot a real, already-converged
//   value. For an A-node `phi<beta` must never happen at all (A's label is
//   ground truth) -- enforced by an unconditional ceiling on beta instead of
//   a reactive check.
// - Beta has NO floor at all (an earlier version of this file had one,
//   `beta>=-phi/2` -- removed): at RENDER time (CornerR3WayValue,
//   DistanceLattice.hlsli -- NOT the plain CornerR3Way this file's own
//   smoothing kernel uses, see its own comment below) gamma is now defined
//   via `exp(-phi)+exp(-beta)-exp(-gamma)=0` instead of the old reciprocal
//   identity, which gives the UNCONDITIONAL bound `gamma<=min(phi,beta)` for
//   every real phi,beta -- no pole, no interval where the label-blind
//   fallback could out-vote the true owner, nothing left to floor beta
//   against. Beta is free to be a genuine (possibly negative) signed
//   distance again.
// - Re-vote trigger for an already-enabled node: `beta < -phi*
//   BetaRerouteTolerance` (a small tolerance >1, see its own define) --
//   purely linear, no formula/pole involved at all. Reads as a geometric
//   claim: if we're LESS confident of being inside our own region (phi)
//   than we are confident of being OUTSIDE the routed one (-beta), the
//   routed pick is an implausible "second place" and something else must be
//   closer. REPLACES the old `beta<gamma` trigger, which the new gamma
//   identity makes permanently unreachable (gamma<=beta always, so "beta
//   loses to gamma" can never fire for any beta at all anymore).
// - A currently-DISABLED node (candN<=1 at whatever point it was last
//   evaluated) always attempts a re-vote, unconditionally, regardless of the
//   above. The re-vote itself (both cases) scans this target's 8 cross-
//   sublattice "own corners" -- the SAME neighbor set and scoring formula
//   (SyntheticVote8.hlsli's score = average signed potential across the 8)
//   Phase 1's own B-node relabel vote and gatherAlienDiscriminatorCS.hlsl's
//   routing scan already use, just read from the halo already loaded here
//   instead of fresh global memory. Often reconfirms the same label --
//   expected, not a bug. A successful vote from a disabled state is a fresh
//   disabled->enabled discovery and DOES reseed beta (there's no prior
//   smoothed value worth preserving); an already-enabled re-vote only
//   changes which label beta represents, never its value.
//
// gatherAlienDiscriminatorCS.hlsl still runs once per RunAlienStep as
// before -- a harmless, still-valid initial seed -- but is no longer load-
// bearing: every node this file ever processes, gathered or not, keeps
// re-deriving its own routing every sweep now.
#define SmoothnessAlienSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "CBV(b2)"

RWStructuredBuffer<uint>  NodeCandidateLabel       : register(u0); // label: byte0 current (read), byte1 scratch (written on a B-node swap, committed by commitSyntheticCS.hlsl)
RWStructuredBuffer<float> NodePotential            : register(u1); // phi, "current" (Jacobi read buffer)
RWStructuredBuffer<uint>  NodeDiscriminator        : register(u2); // discriminator: bits0-7 current (read), bits8-15 scratch (written every sweep, committed by commitAlienCS.hlsl)
RWStructuredBuffer<float> NodeAlienPotential       : register(u3); // beta, "current" (Jacobi read buffer)
RWStructuredBuffer<float> NodeAlienPotentialScratch: register(u4); // beta, Jacobi write buffer
RWStructuredBuffer<float> NodePotentialScratch     : register(u5); // phi, Jacobi write buffer (commitSyntheticCS.hlsl commits this -- see DistanceApp.h)

#define HALO_DIM 4u
// Same constants/bank-conflict tuning as smoothnessJacobiSyntheticCS.hlsl --
// see that file's own comment; kept identical so the tile/halo geometry
// (and the kernelbits table below) stays directly comparable/reusable.
#define HALO_BBASE 71u
#define HALO_NODES 135u // 64 A + 7 padding + 64 B
#define TARGET_NODES 16u // 8 A + 8 B

groupshared uint gLabel[HALO_NODES];
groupshared float gPot[HALO_NODES];
groupshared float gAlienPot[HALO_NODES];
groupshared uint gDiscrim[HALO_NODES];
// This sweep's Term-1 weighted-sum per target, phi pass (x) and beta pass
// (y) -- exactly Phase 1's `gTotal`, just computed twice with two different
// query labels and packed into one float2 to halve the WaveActiveSum count.
groupshared float2 gTotal[TARGET_NODES];

// Same closed-form 27-tap kernel table as smoothnessJacobiSyntheticCS.hlsl --
// copied verbatim, not re-derived, so the two stay directly comparable.
static const uint4 kernelbits = uint4(
    3 | (5 << 5) | (12 << 10) | (15 << 15) | (17 << 20) | (20 << 25), // w 8
    0x00100401u, // w 192 | w 16
    0x47464337u, // w -48
    0x42363332u);

uint posToIdxA(uint3 l) { return AIdx(l.x, l.y, l.z); }
uint posToIdxB(uint3 l) { return BIdx(l.x, l.y, l.z); }

uint inHaloPosToInHaloIdxA(uint3 l) { return l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }
uint inHaloPosToInHaloIdxB(uint3 l) { return HALO_BBASE + l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM; }

// The corner rule used WITHIN the 27-tap smoothing kernel -- deliberately
// simpler than the full psi/beta/gamma scheme CornerR3WayValue
// (DistanceLattice.hlsli) evaluates at render time: own label -> phi;
// discriminator-routed label (IsAlienRoute, exact match only) -> beta;
// ANY OTHER query -> plain -phi, never the reciprocal-derived gamma value.
//
// Why not reuse the full scheme here too: each tap only ever asks a neighbor
// about ONE of two specific labels -- the target's own, or the target's
// routed label. Whenever a neighbor's own label AND routed label both differ
// from the one actually being asked about, that neighbor's beta describes
// some completely unrelated third label with no bearing on this tap's
// question at all -- asking the reciprocal formula to fold that in anyway
// is too much to ask of it inside a 27-tap smoothing sum (unlike at render
// time, where gamma's role is to reconstruct a single specific missing
// corner value for a face that's already known to involve exactly 3
// labels). Plain -phi is the same "everything else disagrees" signal this
// codebase already used everywhere before the alien-potential pass existed.
float CornerR3Way(uint haloIdx, uint queryLabel)
{
    uint lab = gLabel[haloIdx];
    if (lab == queryLabel) return gPot[haloIdx];
    if (IsAlienRoute(gDiscrim[haloIdx], queryLabel)) return gAlienPot[haloIdx];
    return -gPot[haloIdx];
}

// Small numerical safety margin -- not a real design knob (unlike
// SmoothnessWeight/MaxPotentialStep etc.), just enough to keep phi>beta (for
// A) a strict inequality rather than an exact tie, same spirit as phi's own
// 1.0e-4 floor above.
#define BetaCeilEpsilon 1.0e-4

// Re-route trigger tolerance for an already-enabled node (see the re-vote
// block below): reroute when beta < -phi*BetaRerouteTolerance, i.e. we're
// LESS confident of being inside our own region (phi) than we are confident
// of being OUTSIDE the routed one (-beta) -- the routed pick is an
// implausible "second place" and something else must be closer. >1 so an
// exact tie (beta==-phi, the disabled-node default) does NOT retrigger --
// that case is already covered by the separate unconditional disabled-node
// path below.
#define BetaRerouteTolerance 1.1

// This target's 8 cross-sublattice "own corners" at HALO-LOCAL indices --
// the SAME 8-corner neighbor set Phase 1's B-node relabel vote and
// gatherAlienDiscriminatorCS.hlsl's CrossNeighborLabelPot both already use
// (a B-node's 8 surrounding A-corners, or symmetrically an A-node's 8
// surrounding B-corners), just resolved against this file's own halo
// instead of fresh global-memory reads. Safe within HALO_DIM=4: a target
// always sits at halo-local position 1 or 2 per axis (inTilePos+1), and
// these offsets move it by at most {-1,0,+1} per axis, staying inside
// [0,3] -- same reach the Term-1 kernel's own cross-sublattice taps already
// rely on being sufficient.
uint CrossHaloIdx(uint3 targetHaloPos, bool targetIsB, uint c)
{
    int3 d = int3(c & 1u, (c >> 1u) & 1u, (c >> 2u) & 1u);
    int3 p = targetIsB ? ((int3)targetHaloPos + d) : ((int3)targetHaloPos + d - 1);
    uint3 hp = (uint3)p;
    return targetIsB ? inHaloPosToInHaloIdxA(hp) : inHaloPosToInHaloIdxB(hp);
}

// Best FOREIGN candidate (i.e. excluding myLabel) among 8 (label,potential)
// pairs -- same scoring formula as SyntheticField.hlsli's SyntheticVote8
// (candidate score = average potential across the 8, signed by whether each
// corner's own label agrees with the candidate), restricted to candidates
// other than myLabel since beta must route to a foreign label, never repeat
// the target's own. `found=false` iff all 8 corners share myLabel (no real
// second candidate nearby) -- the same "candN<=1" case
// gatherAlienDiscriminatorCS.hlsl's original one-shot scan already had, just
// reached every sweep now instead of once.
void VoteForeignAmong8(uint labels[8], float pots[8], uint myLabel, out uint bestLabel, out float bestScore, out bool found)
{
    bestLabel = 0u;
    bestScore = -3.402823466e+38F;
    found = false;
    for (uint c = 0; c < 8u; c++)
    {
        uint candidate = labels[c];
        if (candidate == myLabel) continue;
        float sum = 0.0;
        for (uint m = 0; m < 8u; m++)
            sum += pots[m] * ((labels[m] == candidate) ? 1.0 : -1.0);
        float score = sum * 0.125;
        if (score > bestScore) { bestScore = score; bestLabel = candidate; found = true; }
    }
}

[RootSignature(SmoothnessAlienSig)]
[numthreads(128, 1, 1)]
void smoothnessJacobiAlienCS(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex)
{
    uint3 haloOriginA = gid * 2;

    // Load: one thread per halo node, direct read of all 4 arrays -- mirrors
    // smoothnessJacobiSyntheticCS.hlsl's Load phase, extended with 2 more
    // per-node reads (beta, discriminator).
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

    // Term 1, run twice per target -- same tap/weight decoding as
    // smoothnessJacobiSyntheticCS.hlsl's wid<27 block, verbatim, just
    // evaluating CornerR3Way for two different query labels per tap instead
    // of the old inline 2-way sign flip.
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
            bool myEnabled; uint myRoutedLabel;
            DecodeDiscriminator(gDiscrim[centerIdx], myEnabled, myRoutedLabel);

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
            float contribPhi = CornerR3Way(neighborIdx, myLabelAtTarget) * weight;
            // Beta pass is meaningless (and myRoutedLabel undefined) when
            // this target's discriminator is disabled -- still compute
            // SOMETHING uniform across the wave (harmless; ignored in the
            // final combine below) rather than diverge the wave here.
            float contribBeta = myEnabled ? CornerR3Way(neighborIdx, myRoutedLabel) * weight : 0.0;
            gTotal[iTarget] = WaveActiveSum(float2(contribPhi, contribBeta));
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

    float myPhi = gPot[centerIdx2];
    float myBeta = gAlienPot[centerIdx2];
    uint myLabelAtTarget = gLabel[centerIdx2];
    bool enabled; uint routedLabel;
    DecodeDiscriminator(gDiscrim[centerIdx2], enabled, routedLabel);

    // Phi step -- same weight/clamp constants as Phase 1's own Term 1.
    float gradPhi = SmoothnessWeight * gTotal[iTarget].x;
    float diagPhi = SmoothnessWeight * 192.0;
    float stepPhi = clamp(-gradPhi / (diagPhi + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
    // Phi is a magnitude -- floor it at a small positive epsilon, same as
    // Phase 1's own A-node reflect (label itself is handled separately below).
    float newPhi = max(myPhi + stepPhi, 1.0e-4);

    // Beta step -- only when this node's discriminator is actually enabled;
    // otherwise nothing reads beta through the routed branch, so just pass
    // it through unchanged. NO floor here anymore: under the exponential
    // gamma identity (CornerR3WayValue, DistanceLattice.hlsli) gamma can
    // NEVER exceed phi or beta regardless of beta's sign or magnitude, so
    // there's nothing left to protect beta's lower end against -- beta is
    // free to be a genuine (possibly negative) signed distance again.
    float newBeta = myBeta;
    if (enabled)
    {
        float gradBeta = SmoothnessWeight * gTotal[iTarget].y;
        float diagBeta = SmoothnessWeight * 192.0;
        float stepBeta = clamp(-gradBeta / (diagBeta + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
        newBeta = myBeta + stepBeta;
    }

    uint newLabel = myLabelAtTarget;
    uint newRoutedLabel = routedLabel;
    bool newEnabled = enabled;

    if (!isB2)
    {
        // A-nodes: phi<beta must never happen at all (A's label is ground
        // truth, corner-rule semantics assume the owner always dominates) --
        // an unconditional ceiling, not a reactive check, same pattern as
        // phi's own epsilon floor just tied to phi instead of zero. A never
        // swaps.
        newBeta = min(newBeta, newPhi - BetaCeilEpsilon);
    }
    else if (newPhi < newBeta)
    {
        // B-nodes: phi<beta is the real, expected trigger -- the exact
        // generalization of Phase 1's own `phi<0` relabel condition now
        // that beta is a real tracked value instead of its old implicit
        // "-phi" stand-in. Swap: own<->beta exchange BOTH label and value.
        newLabel = routedLabel;
        newRoutedLabel = myLabelAtTarget;
        float tmp = newPhi; newPhi = newBeta; newBeta = tmp;
    }

    // Re-vote / discover beta's routed label among this target's 8 cross-
    // sublattice own-corners. Disabled nodes always attempt this
    // (unconditional, cheap, no comparison needed at all); enabled nodes
    // re-vote when `beta < -phi*BetaRerouteTolerance` (see the define's own
    // comment) -- a purely linear signed-distance check, no formula/pole
    // involved, REPLACING the old `beta<gamma` trigger (which the new
    // exponential gamma identity makes permanently unreachable: gamma<=beta
    // always holds now, so "beta loses to gamma" can never fire for any
    // beta, disabled or not).
    bool tryVote = !newEnabled;
    if (newEnabled)
    {
        tryVote = newBeta < -newPhi * BetaRerouteTolerance;
    }
    if (tryVote)
    {
        uint labels8[8];
        float pots8[8];
        [unroll]
        for (uint c = 0; c < 8u; c++)
        {
            uint h = CrossHaloIdx(inHaloPos2, isB2, c);
            labels8[c] = gLabel[h];
            pots8[c] = gPot[h];
        }
        uint bestLabel; float bestScore; bool found;
        VoteForeignAmong8(labels8, pots8, newLabel, bestLabel, bestScore, found);
        bool wasEnabled = newEnabled;
        newEnabled = found;
        newRoutedLabel = found ? bestLabel : 0u;
        if (found && !wasEnabled)
        {
            // Fresh disabled->enabled discovery: seed beta with a real
            // value (same convention gatherAlienDiscriminatorCS.hlsl uses),
            // rather than leaving it at whatever the inert default was.
            newBeta = bestScore;
            if (!isB2) newBeta = min(newBeta, newPhi - BetaCeilEpsilon);
        }
        // else: an already-enabled re-vote only changes (or reconfirms)
        // WHICH label beta represents -- its smoothed VALUE keeps evolving
        // continuously via its own Jacobi step above, never reseeded here.
    }

    NodePotentialScratch[targetGlobalIdx2 * MAX_CANDIDATES + 0u] = newPhi;
    NodeAlienPotentialScratch[targetGlobalIdx2] = newBeta;
    NodeDiscriminator[targetGlobalIdx2] = PackDiscriminatorScratch(gDiscrim[centerIdx2], newEnabled, newRoutedLabel);
    if (isB2 && newLabel != myLabelAtTarget)
    {
        uint word0 = NodeCandidateLabel[targetGlobalIdx2 * 2u + 0u];
        NodeCandidateLabel[targetGlobalIdx2 * 2u + 0u] = (word0 & 0xFFFF00FFu) | ((newLabel & 0xFFu) << 8u);
    }
}
