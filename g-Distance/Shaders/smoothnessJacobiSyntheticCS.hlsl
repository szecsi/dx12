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
// supported -- clamp it to SyntheticEpsilon, and for a B-node (no fixed
// ground truth) pick a new label via SyntheticVote8 over its own 8 A-corners
// (SyntheticField.hlsli, the SAME formula buildSyntheticBCS.hlsl uses to
// seed a non-unanimous B-node at init). A-nodes never change label.
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
    "RootConstants(num32BitConstants=1, b1)," \
    "CBV(b2)"

        cbuffer SyntheticConsts : register(b1)
        {
            uint UseLabelVote; // 1 = SyntheticVote8 over the 8 own A-corners, 0 = dumb binary flip (1-oldLabel), see the relabel block below
        };

        RWStructuredBuffer<uint> NodeCandidateLabel : register(u0);
        RWStructuredBuffer<float> NodePotential : register(u1);
        RWStructuredBuffer<float> NodePotentialScratch : register(u2);

#define HALO_DIM 4u
#define HALO_NODES 128u // 64 A + 64 B
#define TARGET_NODES 16u // 8 A + 8 B

        groupshared uint gLabel[HALO_NODES];
        groupshared float gPot[HALO_NODES];
        groupshared float gTotal[TARGET_NODES];

        static const uint4 kernelbits = uint4(
    3 | (5 << 5) | (12 << 10) | (15 << 15) | (17 << 20) | (20 << 25), // w 8
    0x00100401u, // w 192 | w 16
    0x403F3C30u, // w -48
    0x3B2F2C2Bu);

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
        // TEST: instead of resetting to the SyntheticEpsilon
        // floor, carry over the OLD (pre-correction) potential's
        // magnitude, sign-flipped -- was: newPot = SyntheticEpsilon;
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
                        uint label8WithTarget = myLabelAtTarget | (iTarget << 8u);
                        float pot8 = (label8 != myLabelAtTarget) ? gPot[cornerIdx] : 0.0;
                        uint sameLabelSameTargetMask = WaveMatch(label8WithTarget);
                        float totalPot8 = WaveMultiPrefixSum(sameLabelSameTargetMask, pot8) + pot8;
                        uint lastlane = firstbithigh(sameLabelSameTargetMask.x);
                        uint lastlanesForThisTargetMask = WaveActiveBallot(lastlane).x & (0xff << (iTarget % 4));
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
                        if (wid == bestLane)
                        {
                            newLabel = WaveReadLaneAt(label8, wid);
                        }
                    }
                }
            }

            NodePotentialScratch[targetGlobalIdx * MAX_CANDIDATES + 0u] = newPot;
            uint word0 = NodeCandidateLabel[targetGlobalIdx * 2u + 0u];
            NodeCandidateLabel[targetGlobalIdx * 2u + 0u] = (word0 & 0xFFFF00FFu) | ((newLabel & 0xFFu) << 8u);
        }
