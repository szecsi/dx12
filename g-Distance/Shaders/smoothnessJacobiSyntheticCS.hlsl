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
// same-region neighbors' scale, which would otherwise starve the closed-form
// volume formula's per-tet product of potential ratios.
//
// Buffers are the SAME multi-candidate ones (NodeCandidateLabel/
// NodePotential/NodePotentialScratch), reinterpreted: byte 0 of
// NodeCandidateLabel[node*2+0] is the current label, byte 1 of that SAME
// uint is the scratch (next) label (avoids needing a whole new buffer just
// for double-buffering one byte), and slot 0 of NodePotential/
// NodePotentialScratch is the current/next potential. Bytes 2-3 and slots
// 1-7 are simply never touched in this mode.
#define SmoothnessSyntheticSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "UAV(u4)," \
    "UAV(u5)," \
    "RootConstants(num32BitConstants=1, b1)," \
    "CBV(b2)"

        cbuffer SyntheticConsts : register(b1)
        {
            uint UseLabelVote; // 1 = SyntheticVote8 over the 8 own A-corners, 0 = dumb binary flip (1-oldLabel), see the relabel block below
        };

        RWStructuredBuffer<uint> NodeCandidateLabel : register(u0);
        RWStructuredBuffer<float> NodePotential : register(u1);
        RWStructuredBuffer<float> NodePotentialScratch : register(u2);
        // Volume-conservation buffers (see the correction block below and
        // DistanceCb.hlsli's VolumeRatioWeight) -- NodeSyntheticVolume is
        // read for the WHOLE halo (last round's committed values, same
        // one-round-stale staggering as NodeCandidateLabel/NodePotential
        // above), NodeSyntheticVolumeScratch is written only for this
        // tile's 16 targets and picked up by commitSyntheticCS.hlsl next.
        RWStructuredBuffer<float> NodeSyntheticVolume : register(u3);
        RWStructuredBuffer<float> NodeSyntheticVolumeScratch : register(u4);
        // Ground-truth per-A-node label (ACount-sized, same indexing as
        // posToIdxA/AIdx) -- gives each halo's local "original volume"
        // baseline for the volume-conservation correction.
        RWStructuredBuffer<uint> RasterLabel : register(u5);

#define HALO_DIM 4u
#define HALO_NODES 128u // 64 A + 64 B
#define TARGET_NODES 16u // 8 A + 8 B

        groupshared uint gLabel[HALO_NODES];
        groupshared float gPot[HALO_NODES];
        groupshared float gTotal[TARGET_NODES];
        groupshared float gVolume[HALO_NODES];
        groupshared uint gRasterLabel[64]; // A-halo only -- RasterLabel has no B entries
        // Per-target CURRENT node volume (see the wid<24 fan-tet block below and
        // plan soft-stargazing-biscuit.md) -- distinct from gVolume[HALO_NODES]
        // above, which holds each halo node's own LAST-COMMITTED volume (read
        // for the ratio correction); this one is THIS sweep's freshly computed
        // value for this tile's 16 targets only, written to
        // NodeSyntheticVolumeScratch at the end of Stage 2.
        groupshared float gTetVolume[TARGET_NODES];

        static const uint4 kernelbits = uint4(
    3 | (5 << 5) | (12 << 10) | (15 << 15) | (17 << 20) | (20 << 25), // w 8
    0x00100401u, // w 192 | w 16
    0x403F3C30u, // w -48
    0x3B2F2C2Bu);

        // Closed-form "current volume" tap table -- see plan
        // soft-stargazing-biscuit.md for the full derivation. A node's 24
        // incident tets (GatherIncidentTets, ring-1) decompose into 3 closed
        // 4-tet fans, one per axis, centered on that axis's same-sublattice
        // ("cube face") neighbor; the other 3 fans (opposite face per axis)
        // are the exact mirror. Each uint packs one axis's fan as 5-bit
        // fields [rim0,rim1,rim2,rim3,rim0-repeat] (verified computationally,
        // not hand-derived, given this project's own history of a bug from a
        // similar hand derivation) -- values are each rim corner's flat
        // halo-index delta from the target's own centerIdx, RE-BASED by a
        // fixed per-target-type bias (+43 for A-targets, -64 for B-targets)
        // so they land in [0,21] for BOTH sublattices identically. The
        // mirrored (opposite-face) half of each fan is exactly
        // `21 - storedValue`; centers need no storage at all -- they're just
        // the halo's own flat-index axis strides (+-1,+-4,+-16).
        static const uint3 kFanRim = uint3(
            0u | (16u << 5) | (20u << 10) | (4u << 15) | (0u << 20),  // axis0 (center=1):  rim=[0,16,20,4,0]
            0u | (1u << 5) | (17u << 10) | (16u << 15) | (0u << 20),  // axis1 (center=4):  rim=[0,1,17,16,0]
            0u | (4u << 5) | (5u << 10) | (1u << 15) | (0u << 20));   // axis2 (center=16): rim=[0,4,5,1,0]

        // Every BCC tet in this lattice is a congruent disphenoid (verified
        // computationally across many cube origins/slots) -- volume is a
        // fixed constant, not something that needs QWorldPos/cross-products
        // at runtime. The raw per-tet volume is CELL_SIZE^3/12, but this
        // formula ALWAYS evaluates the 1-3 split (exactly 1 agreeing corner
        // -- itself -- and 3 unconditionally "opposing" ones, since labels
        // aren't consulted at all), even where all 4 corners genuinely agree.
        // The label-aware general construction (smoothnessJacobiCS's Term 4)
        // recognizes that case (countPos==4, VPos=full Vtet, split 4 ways =
        // Vtet/4 per corner); this formula can never see it and always
        // produces the smaller Vtet*0.5^3=Vtet/8 corner-cut share instead --
        // exactly half, a fixed structural gap, not a per-configuration
        // error, verified by comparing both constructions' uniform-potential
        // steady state (CELL_SIZE^3/2 vs CELL_SIZE^3/4 over the 24 incident
        // tets). The *2 below recalibrates against that reference so a
        // uniform node reports the expected 0.5, not 0.25.
        static const float kVtetConst = CELL_SIZE * CELL_SIZE * CELL_SIZE / 12.0 * 2.0;

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
            return 64u + l.x + l.y * HALO_DIM + l.z * HALO_DIM * HALO_DIM;
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
        uint idx = isBHalo ? posToIdxB(pos) : posToIdxA(pos);
        uint inHaloIdx = isBHalo ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
        gLabel[inHaloIdx] = GetCandidateLabelAt(NodeCandidateLabel, idx, 0u);
        gPot[inHaloIdx] = NodePotential[idx * MAX_CANDIDATES + 0u];
        gVolume[inHaloIdx] = NodeSyntheticVolume[idx];
        // A-node global index (idx, via posToIdxA) is exactly RasterLabel's
        // own indexing -- no separate lookup needed.
        if (!isBHalo) gRasterLabel[inHaloIdx] = RasterLabel[idx];
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

            // Current-volume fan-tet evaluation -- one thread per incident
            // tet (24 of them), WaveActiveSum reduces (not Max: the 24 tets
            // tile a neighborhood without overlapping, so their positive-side
            // volumes are strictly additive). See kFanRim's comment above and
            // plan soft-stargazing-biscuit.md for the full derivation.
            //
            // Same-label taps use the TARGET's own potential in place of
            // their own, not their own potential -- treating every neighbor
            // as unconditionally "opposing" (using its own raw potential
            // regardless of label) is wrong even deep inside a single
            // homogeneous region, since potentials are distances, more or
            // less, and vary node to node even where every node agrees on
            // the label; there is no real interface to locate along a
            // same-label edge at all. Substituting myPotFan for a same-label
            // tap's own potential forces that edge's crossing fraction to
            // exactly 0.5 (myPot/(myPot+myPot)) unconditionally -- the
            // correct "no boundary here, split stays at the midpoint"
            // behavior -- while a genuinely differently-labeled tap still
            // uses its own real potential, unchanged.
            if (wid < 24u)
            {
                for (int iTarget = warpId; iTarget < 16; iTarget += 4)
                {
                    uint local = iTarget & 7u;
                    uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
                    bool isB = iTarget >= 8u;
                    uint3 inHaloPos = inTilePos + 1u;
                    uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);

                    uint fanHalf = wid / 12u;   // 0 = stored (-axis) fans, 1 = mirrored (+axis) fans
                    uint localTet = wid % 12u;
                    uint fanIdx = localTet / 4u; // 0,1,2 -> which axis
                    uint tetIdx = localTet % 4u; // which of the 4 tets in that fan

                    uint centerMag = 1u << (fanIdx * 2u); // 1,4,16 -- the halo's own flat-index axis strides
                    int centerD = (fanHalf == 0u) ? -(int)centerMag : (int)centerMag;

                    uint packed = kFanRim[fanIdx];
                    uint r0 = (packed >> (tetIdx * 5u)) & 0x1Fu;
                    uint r1 = (packed >> ((tetIdx + 1u) * 5u)) & 0x1Fu;
                    if (fanHalf == 1u) { r0 = 21u - r0; r1 = 21u - r1; }

                    int bias = isB ? -64 : 43;
                    int offR0 = (int)r0 + bias;
                    int offR1 = (int)r1 + bias;

                    uint idxCenter = centerIdx + centerD;
                    uint idxRimA = centerIdx + offR0;
                    uint idxRimB = centerIdx + offR1;

                    uint myLabelAtTarget = gLabel[centerIdx];
                    float myPotFan = gPot[centerIdx];
                    float potA = (gLabel[idxCenter] == myLabelAtTarget) ? myPotFan : gPot[idxCenter];
                    float potB = (gLabel[idxRimA] == myLabelAtTarget) ? myPotFan : gPot[idxRimA];
                    float potC = (gLabel[idxRimB] == myLabelAtTarget) ? myPotFan : gPot[idxRimB];
                    const float epsFloor = 1.0e-4;
                    float t0 = myPotFan / max(myPotFan + potA, epsFloor);
                    float t1 = myPotFan / max(myPotFan + potB, epsFloor);
                    float t2 = myPotFan / max(myPotFan + potC, epsFloor);
                    float contrib = t0 * t1 * t2;
                    gTetVolume[iTarget] = kVtetConst * WaveActiveSum(contrib);
                }
            }

            GroupMemoryBarrierWithGroupSync();

            uint iTarget = tid / 8u;
            uint local = iTarget & 7u;
            uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
            bool isB = iTarget >= 8u;
            uint3 inHaloPos = inTilePos + 1u;
            uint centerIdx = isB ? inHaloPosToInHaloIdxB(inHaloPos) : inHaloPosToInHaloIdxA(inHaloPos);
 
            uint targetGlobalIdx = isB ? posToIdxB(haloOriginA + inHaloPos) : posToIdxA(haloOriginA + inHaloPos);
            float myPot = gPot[centerIdx];
            float w = SmoothnessWeight;
            float grad = w * gTotal[iTarget];
            float diag = w * 192.0;
            float step = clamp(-grad / (diag + JacobiDiagEpsilon), -MaxPotentialStep, MaxPotentialStep);
            float newPot = myPot + step;
            uint myLabelAtTarget = gLabel[centerIdx];
            uint newLabel = myLabelAtTarget;

            if (newPot < 0.0)
            {
                // A-nodes never relabel, so this reflected magnitude is their
                // final newPot. For a B-node this is only a placeholder,
                // overwritten below with a real head-start once it's actually
                // relabeled -- see SyntheticEpsilon's use after the vote.
                newPot = -newPot;
                if (isB)
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

            // Volume-conservation nudge (DistanceCb.hlsli's VolumeRatioWeight)
            // -- applied AFTER newLabel/newPot are already finalized above, as
            // a confidence-only correction: it does NOT reopen the relabel
            // decision this round, it only pulls this target's potential
            // toward local per-label volume conservation for whichever label
            // it ends up with. Local, per-halo, no tet-walking: "current
            // volume" for a label is just the sum of gVolume (each halo
            // node's own last-committed potential, see the Load stage above)
            // over halo nodes presently carrying that label; "original
            // volume" is the count of this halo's A-nodes whose RasterLabel
            // ground truth carries it. Ratio 1 = locally conserved; >1 means
            // newLabel has locally grown past its original footprint here
            // (push potential down); <1 means it's under-represented (push
            // up). Redundantly computed by all 8 lanes sharing this target
            // (same as the relabel vote above) -- harmless, gLabel/gVolume/
            // gRasterLabel and newLabel are already identical across them.
            {
                float currentVolume = 0.0;
                for (uint hv = 0; hv < HALO_NODES; hv++)
                {
                    if (gLabel[hv] == newLabel) currentVolume += gVolume[hv];
                }
                float originalVolume = 0.0;
                for (uint hr = 0; hr < 64u; hr++)
                {
                    if (gRasterLabel[hr] == newLabel) originalVolume += 1.0;
                }
                // No local baseline for this label in this halo at all --
                // skip the correction rather than divide by ~0 (which would
                // otherwise blow up to +-inf for any nonzero currentVolume).
                if (originalVolume > 0.5)
                {
                    float ratio = currentVolume / originalVolume;
                    float correction = VolumeRatioWeight * (1.0 - ratio);
                    newPot += clamp(correction, -MaxPotentialStep, MaxPotentialStep);
                }
            }

            NodePotentialScratch[targetGlobalIdx * MAX_CANDIDATES + 0u] = newPot;
            // "Node volume" == this target's own closed-form fan-tet volume,
            // computed in the wid<24 block above (gTetVolume) -- see
            // commitSyntheticCS.hlsl for the scratch->main commit and
            // initSyntheticVolumeCS.hlsl for the matching Reinit-time seed.
            NodeSyntheticVolumeScratch[targetGlobalIdx] = gTetVolume[iTarget];
            uint word0 = NodeCandidateLabel[targetGlobalIdx * 2u + 0u];
            NodeCandidateLabel[targetGlobalIdx * 2u + 0u] = (word0 & 0xFFFF00FFu) | ((newLabel & 0xFFu) << 8u);
        }
