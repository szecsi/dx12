#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// Outer Lloyd-loop step 1: pick the 2 currently-competing labels for an
// entire rhombohedral CUBE (not per tet -- see below for why), from the
// top-1 candidate at each of its up to 8 distinct corners (D0, D1, 6 ring
// vertices, real or virtual -- see GetCornerTopLabel): cheap v1 proxy per
// the plan, deduped by frequency, rather than a full argmax over every
// candidate label at every corner.
//
// One thread per cube (dispatched with TetCount/6 == TotalCubeCandidates
// threads; tetBase = cubeIndex*6 is always a valid, computable tet block,
// see DistanceLattice.hlsli -- no per-cube valid/invalid check needed at
// all, a cube with no real corners nearby just computes an unremarkable
// "everyone agrees on background" result), writing the SAME resulting
// pair to all 6 of that cube's tets. This used to be one thread per FACE
// voting over 4 tets under the old face-based scheme; the underlying
// reasoning carries over unchanged under cube-based indexing: a per-tet
// vote could let sibling tets around the same cube disagree, and since
// AccumulatePair in smoothnessJacobiCS.hlsl only couples fan-adjacent tets
// that *agree* on the active pair, an inconsistently-voted cube could end
// up with its genuinely-active tets not fan-paired with each other at all
// -- silently cutting the smoothness energy's only path to the shared
// D0/D1 nodes, which every one of the cube's 6 tets has in common. Voting
// once per cube and applying the result uniformly guarantees all 6 of a
// cube's tets always agree, so the fan is always fully coupled.
#define AssignPairsSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<uint>   NodeCandidateLabel : register(u0);
RWStructuredBuffer<float>  NodePotential : register(u1);
RWStructuredBuffer<uint2>  TetInterfacePair : register(u2);

[RootSignature(AssignPairsSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void assignInterfacePairsCS(uint3 tid : SV_DispatchThreadID)
{
    uint cubeIndex = tid.x;
    uint tetBase = cubeIndex * 6;
    if (tetBase >= TetCount) return;

    // Directly enumerate the cube's 8 distinct corners (D0, D1, and the 6
    // RingBetween positions -- the same fixed convention GetTetCornerQs
    // already uses per-slot: q0=D0, q1=D1, q2=ring[slot-1], q3=ring[slot],
    // consistent chirality for every slot, cross-verified via the D0Cap/
    // D1Cap target-slot tables being exact inverse permutations). Still
    // deduped by VALUE (not just position) to keep this bit-identical to
    // the old "gather 24 tet-corner mentions and dedupe" approach:
    // SENTINEL_LABEL (a cube's virtual/out-of-grid corners) is a valid,
    // distinct value here too, and multiple virtual corners must still
    // collapse into a single combined "background" vote via GetCornerTopLabel
    // below, not one vote per virtual corner. 8 ResolveCorner calls (via
    // CubeOriginFromLinear+the offset tables) instead of 24 (via 6x
    // GetTetCornerRefs), and an 8x8 dedup instead of 24x8.
    int3 C = CubeOriginFromLinear(cubeIndex);
    int3 cornerQ[8] = {
        C, C + int3(1, 1, 1),
        C + RingBetween[0], C + RingBetween[1], C + RingBetween[2],
        C + RingBetween[3], C + RingBetween[4], C + RingBetween[5]
    };
    uint refs[8] = { SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL };
    uint nNodes = 0;
    for (uint m = 0; m < 8; m++) {
        uint n = ResolveCorner(cornerQ[m]);
        bool found = false;
        for (uint u = 0; u < nNodes; u++) if (refs[u] == n) { found = true; break; }
        if (!found) { refs[nNodes] = n; nNodes++; }
    }

    uint topLabels[8];
    float topPots[8];
    for (uint c = 0; c < nNodes; c++) GetCornerTopLabel(refs[c], NodeCandidateLabel, NodePotential, topLabels[c], topPots[c]);

    // Frequency-count the (<=8) distinct top labels, padding unused slots
    // with freq=0 so a fixed-size bubble sort can rank them uniformly.
    uint uniqueLabels[8] = { SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE };
    int freq[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint nUnique = 0;
    for (uint c2 = 0; c2 < nNodes; c2++) {
        uint l = topLabels[c2];
        bool found2 = false;
        for (uint u2 = 0; u2 < nUnique; u2++) if (uniqueLabels[u2] == l) { freq[u2]++; found2 = true; break; }
        if (!found2) { uniqueLabels[nUnique] = l; freq[nUnique] = 1; nUnique++; }
    }

    // Sort all 8 slots descending by frequency (tie-break ascending label);
    // unused slots (freq=0) sink to the bottom.
    for (uint p = 0; p < 8; p++) {
        for (uint s = 0; s < 7; s++) {
            bool doSwap = (freq[s] < freq[s + 1]) ||
                          (freq[s] == freq[s + 1] && uniqueLabels[s] > uniqueLabels[s + 1]);
            if (doSwap) {
                int tf = freq[s]; freq[s] = freq[s + 1]; freq[s + 1] = tf;
                uint tl = uniqueLabels[s]; uniqueLabels[s] = uniqueLabels[s + 1]; uniqueLabels[s + 1] = tl;
            }
        }
    }

    uint labelI = uniqueLabels[0];
    uint labelJ = (freq[1] > 0) ? uniqueLabels[1] : labelI; // labelI==labelJ marks "no active interface"
    if (labelJ < labelI) { uint tmp = labelI; labelI = labelJ; labelJ = tmp; }

    uint2 pair = uint2(labelI, labelJ);
    for (uint w = 0; w < 6; w++) TetInterfacePair[tetBase + w] = pair;
}
