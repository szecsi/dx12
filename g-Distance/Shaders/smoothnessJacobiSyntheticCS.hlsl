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
    "UAV(u6)," \
    "UAV(u7)," \
    "UAV(u8)," \
    "UAV(u9)," \
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
        // Per-node d(currentVolume)/d(myPot) -- see the wid<24 fan-tet block's
        // dcontrib. Same halo-read/scratch-write/one-round-stale pattern as
        // NodeSyntheticVolume above, used to split a label's original volume
        // across its currently same-labeled halo members in PROPORTION TO
        // LEVERAGE rather than equally: a node whose potential genuinely
        // can't move the reconstructed volume (e.g. deep interior, every tap
        // same-label) gets zero sensitivity and therefore zero target share
        // and zero pressure, automatically -- not a heuristic cutoff, a
        // direct consequence of the derivative (see Stage 1 below).
        RWStructuredBuffer<float> NodeSensitivity : register(u6);
        RWStructuredBuffer<float> NodeSensitivityScratch : register(u7);
        // Continuous smoothing-vs-volume alignment (see the Stage 2 volume
        // block) -- same halo-read/scratch/commit/one-sweep-stale pattern as
        // NodeSensitivity above. Lets a node's OWN target share be weighted
        // by how much its smoothing gradient agrees with what volume
        // conservation wants there, instead of a hard per-node gate (which
        // starves thin/isolated features where every interface node
        // structurally disagrees with smoothing -- confirmed: a hard gate
        // fixed the torus but broke thin features entirely).
        RWStructuredBuffer<float> NodeVolumeAlignment : register(u8);
        RWStructuredBuffer<float> NodeVolumeAlignmentScratch : register(u9);

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
        groupshared float gSensitivity[HALO_NODES]; // last-committed d(volume)/d(myPot), halo-wide, see NodeSensitivity above
        groupshared float gVolumeSensitivity[TARGET_NODES]; // THIS sweep's freshly computed sensitivity, this tile's 16 targets only
        groupshared float gAlignment[HALO_NODES]; // last-committed smoothing-vs-volume alignment, halo-wide, see NodeVolumeAlignment above

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
        gVolume[inHaloIdx] = NodeSyntheticVolume[idx];
        gSensitivity[inHaloIdx] = NodeSensitivity[idx];
        gAlignment[inHaloIdx] = NodeVolumeAlignment[idx];
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
                    bool same0 = (gLabel[idxCenter] == myLabelAtTarget);
                    bool same1 = (gLabel[idxRimA] == myLabelAtTarget);
                    bool same2 = (gLabel[idxRimB] == myLabelAtTarget);
                    float potA = same0 ? myPotFan : gPot[idxCenter];
                    float potB = same1 ? myPotFan : gPot[idxRimA];
                    float potC = same2 ? myPotFan : gPot[idxRimB];
                    const float epsFloor = 1.0e-4;
                    float t0 = myPotFan / max(myPotFan + potA, epsFloor);
                    float t1 = myPotFan / max(myPotFan + potB, epsFloor);
                    float t2 = myPotFan / max(myPotFan + potC, epsFloor);
                    float contrib = t0 * t1 * t2;
                    // d(contrib)/d(myPotFan). A same-label tap's "potential"
                    // IS myPotFan (see the substitution above), so its t_i is
                    // pinned at exactly 0.5 regardless of myPotFan's value --
                    // d(t_i)/d(myPotFan)=0 for that tap, it drops out of the
                    // sum entirely. Only genuinely different-label taps have
                    // any real derivative left, via the log-derivative
                    // identity d(t_i)/d(myPotFan) = t_i*(1-t_i)/myPotFan:
                    // d(contrib)/d(myPotFan) = (contrib/myPotFan) * sum of
                    // (1-t_i) over the different-label taps only. A node with
                    // every tap same-label (deep interior) gets exactly 0
                    // here -- its own volume literally cannot respond to its
                    // own potential, so it should never compete for a target
                    // share (see Stage 2 below), which falls out of this
                    // formula automatically rather than needing a cutoff.
                    float dsum = (same0 ? 0.0 : (1.0 - t0)) + (same1 ? 0.0 : (1.0 - t1)) + (same2 ? 0.0 : (1.0 - t2));
                    float dcontrib = contrib * dsum / max(myPotFan, epsFloor);
                    gTetVolume[iTarget] = kVtetConst * WaveActiveSum(contrib);
                    gVolumeSensitivity[iTarget] = kVtetConst * WaveActiveSum(dcontrib);
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
            uint myLabelAtTarget = gLabel[centerIdx];
            float w = SmoothnessWeight;
            float grad = w * gTotal[iTarget];
            float diag = w * 192.0;

            // Volume-conservation term, folded into the SAME grad/diag as
            // smoothness -- a proper joint Jacobi term now, not a separate
            // post-hoc nudge (see plan/discussion). Uses this target's
            // CURRENT (pre-relabel) label, since newLabel isn't decided until
            // after this combined step -- volume pressure can now itself
            // help push a severely over/under-represented node's potential
            // negative and trigger relabeling, a deliberate change from the
            // earlier "confidence nudge only, after relabeling" version.
            // targetShare divides the label's original (RasterLabel) halo
            // footprint across its currently same-labeled halo members in
            // proportion to LEVERAGE (mySensitivity/sumSensitivity), not
            // equally -- a node that cannot move its own reconstructed
            // volume (mySensitivity==0, e.g. deep interior) gets zero share
            // and exerts zero pressure, automatically.
            float mySensitivity = gVolumeSensitivity[iTarget];
            // Committed every sweep regardless of whether the gates below
            // fire, so other tiles' halos always read a sane (0=neutral)
            // value even when this node currently has no volume opinion.
            float myAlignment = 0.0;
            {
                float sumSensitivity = 0.0;
                for (uint hv = 0; hv < HALO_NODES; hv++)
                {
                    if (gLabel[hv] == myLabelAtTarget) sumSensitivity += gSensitivity[hv];
                }
                float originalVolume = 0.0;
                for (uint hr = 0; hr < 64u; hr++)
                {
                    if (gRasterLabel[hr] == myLabelAtTarget) originalVolume += 10.0;
                }
                const float epsFloor = 1.0e-4;
                // No local baseline for this label in this halo, or no member
                // of it (including possibly this target) has any leverage at
                // all -- skip the term rather than divide by ~0.
                if (originalVolume > 0.5 && sumSensitivity > epsFloor)
                {
                    // Provisional target share/violation, using RAW (ungated)
                    // mySensitivity -- exactly what targetShare/violation
                    // would be with no agreement gate at all. Its SIGN is
                    // what desiredDir needs: violation>0 means THIS node's
                    // own assigned share exceeds its OWN current volume
                    // (needs more, so myPot should increase, since
                    // mySensitivity=d(volume)/d(myPot)>=0); violation<0
                    // means the opposite. This MUST be a per-node signal, not
                    // a halo-wide aggregate (originalVolume minus a sum of
                    // other halo members' current volumes) -- an earlier
                    // version used that halo-wide aggregate here and it was
                    // wrong: a node can sit in a halo whose label is
                    // aggregate-deficient while THIS PARTICULAR node already
                    // exceeds its own proportional share (because some other
                    // halo member is carrying most of the deficit), so the
                    // aggregate's sign and this node's own violation sign can
                    // disagree -- silently gating the wrong nodes in and out,
                    // in a way that varies with local sensitivity/volume
                    // distribution (shape-dependent, not a constant sign
                    // flip -- confirmed empirically: flipping the aggregate's
                    // sign helped the torus but broke other test shapes,
                    // rather than fixing everything uniformly).
                    float targetShareProvisional = originalVolume * mySensitivity / sumSensitivity;
                    float violationProvisional = targetShareProvisional - gTetVolume[iTarget];
                    // Direction the volume term ALONE would push myPot, deadbanded.
                    float desiredDir = (violationProvisional > epsFloor) ? 1.0 : ((violationProvisional < -epsFloor) ? -1.0 : 0.0);
                    // Smoothing's own step direction at this target is
                    // sign(-gTotal[iTarget]) (step=-grad/diag, grad =
                    // w*gTotal[iTarget], w>0 -- see grad's assignment
                    // above). Deadbanded so a near-zero gTotal (smoothing
                    // has no real opinion here) doesn't count as either
                    // agreeing or fighting.
                    float smoothDir = (gTotal[iTarget] > epsFloor) ? -1.0 : ((gTotal[iTarget] < -epsFloor) ? 1.0 : 0.0);
                    // Continuous alignment, not a boolean gate: positive =
                    // smoothing agrees with what volume wants here (this
                    // sweep), magnitude = how strongly; negative = fights;
                    // zero = no local need. Algebraically
                    // -gTotal[iTarget]*desiredDir: when desiredDir=+1 this is
                    // -gTotal[iTarget] (positive iff smoothDir=+1, agreement);
                    // when desiredDir=-1 it's +gTotal[iTarget] (positive iff
                    // smoothDir=-1, agreement) -- so sign(myAlignment)>0 is
                    // exactly the old hard agree test, now keeping the
                    // MAGNITUDE the ranking fallback below needs.
                    myAlignment = -gTotal[iTarget] * desiredDir;

                    // Hard per-node agreement gating (the previous version)
                    // starves volume conservation entirely for any thin/
                    // isolated feature where EVERY interface node's smoothing
                    // gradient opposes growing/shrinking it (confirmed: fixed
                    // the torus, broke thin features -- smoothing wants to
                    // erode/homogenize a small isolated feature, volume wants
                    // to preserve it, so every interface node disagrees).
                    // Renormalize and gracefully degrade instead:
                    //   Tier 1: real agreement exists somewhere -- use only
                    //     agreeing members, renormalized against THEIR
                    //     combined leverage (not the whole halo's), so shares
                    //     sum to exactly originalVolume whenever real
                    //     agreement exists (the old version never
                    //     renormalized at all, undershooting even here).
                    //   Tier 2: no real agreement anywhere -- rank same-label
                    //     halo members by RELATIVE alignment and weight by
                    //     rank, so the least-disagreeable members still carry
                    //     the correction instead of nobody carrying it.
                    //   Fallback: ranking can't distinguish anyone usable
                    //     (every candidate tied, or the top-ranked ones have
                    //     zero leverage) -- degrade to the original plain-
                    //     leverage split.
                    // Same numeric value as epsFloor above, named separately
                    // since it's compared against gTotal-scale quantities
                    // (up to ~192x a raw potential) rather than volume/
                    // sensitivity-scale ones -- both are "distinguish real
                    // signal from float noise" tests, not calibrated
                    // thresholds, so reuse is safe; distinct name just saves
                    // a future reader re-deriving that.
                    const float alignEpsFloor = 1.0e-4;
                    float sumAgreeSensitivity = 0.0;
                    float minAlignment = 3.402823466e+38F;
                    float maxAlignment = -3.402823466e+38F;
                    for (uint hv2 = 0; hv2 < HALO_NODES; hv2++)
                    {
                        if (gLabel[hv2] != myLabelAtTarget) continue;
                        // Fresh myAlignment for THIS node's own slot, not the
                        // stale gAlignment[centerIdx] committed last sweep --
                        // using stale-self here could put the fresh
                        // myAlignment used in ranking below OUTSIDE
                        // [minAlignment,maxAlignment], pushing myRank outside
                        // [0,1] and myWeight negative (the exact "invert, not
                        // zero" class of bug an earlier review already
                        // caught once). Every OTHER member still uses its own
                        // one-sweep-stale gAlignment[hv2], same staleness
                        // already accepted for gSensitivity[]/gVolume[].
                        float alignment_hv = (hv2 == centerIdx) ? myAlignment : gAlignment[hv2];
                        if (alignment_hv > alignEpsFloor) sumAgreeSensitivity += gSensitivity[hv2];
                        minAlignment = min(minAlignment, alignment_hv);
                        maxAlignment = max(maxAlignment, alignment_hv);
                    }

                    float myWeight = 0.0;
                    float sumWeight = 0.0;
                    if (sumAgreeSensitivity > alignEpsFloor)
                    {
                        // Tier 1.
                        myWeight = (myAlignment > alignEpsFloor) ? mySensitivity : 0.0;
                        sumWeight = sumAgreeSensitivity;
                    }
                    else
                    {
                        float rangeAlignment = maxAlignment - minAlignment;
                        if (rangeAlignment > alignEpsFloor)
                        {
                            // Tier 2 -- rank by RELATIVE alignment (0 = this
                            // halo's most disagreeable same-label member, 1 =
                            // least). Needs min/maxAlignment finalized first,
                            // hence a second pass -- only reached when Tier 1
                            // found nothing, same plain per-thread-redundant
                            // loop pattern as every other halo scan here.
                            float myRank = (myAlignment - minAlignment) / rangeAlignment;
                            float sumRankSensitivity = 0.0;
                            for (uint hv3 = 0; hv3 < HALO_NODES; hv3++)
                            {
                                if (gLabel[hv3] != myLabelAtTarget) continue;
                                float alignment_hv3 = (hv3 == centerIdx) ? myAlignment : gAlignment[hv3];
                                float rank_hv3 = (alignment_hv3 - minAlignment) / rangeAlignment;
                                sumRankSensitivity += gSensitivity[hv3] * rank_hv3;
                            }
                            // Even with real spread, the top-ranked member(s)
                            // can have zero leverage (alignment depends only
                            // on gTotal/desiredDir, not sensitivity -- a
                            // deep-interior node can rank highest while
                            // having zero ability to actually move volume)
                            // while everyone who CAN move it is tied at the
                            // bottom -- guard against that collapsing
                            // sumRankSensitivity to ~0 instead of dividing by it.
                            if (sumRankSensitivity > alignEpsFloor)
                            {
                                myWeight = mySensitivity * myRank;
                                sumWeight = sumRankSensitivity;
                            }
                        }
                        if (sumWeight <= alignEpsFloor)
                        {
                            // Fallback -- ranking found nothing usable.
                            myWeight = mySensitivity;
                            sumWeight = sumSensitivity;
                        }
                    }

                    float targetShare = originalVolume * myWeight / sumWeight;
                    float violation = targetShare - gTetVolume[iTarget];
                    grad += -2.0 * VolumeRatioWeight * violation * myWeight;
                    diag += 2.0 * VolumeRatioWeight * myWeight * myWeight;
                }
            }

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

            NodePotentialScratch[targetGlobalIdx * MAX_CANDIDATES + 0u] = newPot;
            // "Node volume"/"sensitivity" == this target's own closed-form
            // fan-tet values, computed in the wid<24 block above -- see
            // commitSyntheticCS.hlsl for the scratch->main commit and
            // initSyntheticVolumeCS.hlsl for the matching Reinit-time seed.
            NodeSyntheticVolumeScratch[targetGlobalIdx] = gTetVolume[iTarget];
            NodeSensitivityScratch[targetGlobalIdx] = mySensitivity;
            NodeVolumeAlignmentScratch[targetGlobalIdx] = myAlignment;
            uint word0 = NodeCandidateLabel[targetGlobalIdx * 2u + 0u];
            NodeCandidateLabel[targetGlobalIdx * 2u + 0u] = (word0 & 0xFFFF00FFu) | ((newLabel & 0xFFu) << 8u);
        }
