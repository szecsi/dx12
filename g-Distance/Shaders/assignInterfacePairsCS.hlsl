#include "DistanceLattice.hlsli"

// Outer Lloyd-loop step 1: pick the 2 currently-competing labels for an
// entire grid FACE (not per tet -- see below for why), from the top-1
// candidate at each of its 6 distinct nodes (4 A corners + 2 B corners):
// cheap v1 proxy per the plan, deduped by frequency, rather than a full
// argmax over every candidate label at every corner.
//
// One thread per face (dispatched with TotalFaces threads), writing the
// SAME resulting pair to all 4 of that face's tets. This used to be one
// thread per TET voting only over ITS OWN 4 corners -- which let different
// tets around the same face disagree (e.g. a face with 2 background and 2
// shape A-corners could see some of its 4 tets vote "background only" or
// "shape only" depending on which 2 of the 4 A-corners that tet's edge
// happened to use), marking some of the face's tets "no active interface"
// even though the face clearly needs one. Since AccumulatePair in
// smoothnessJacobiCS.hlsl only couples fan-adjacent tets that *agree* on
// the active pair, an inconsistently-voted face could end up with its two
// genuinely-active tets not fan-paired with each other at all (separated by
// inactive ones in the 4-tet ring) -- silently cutting the smoothness
// energy's only path to the shared B0/B1 nodes for that face, since B0 and
// B1 (not any A-corner) are exactly the two nodes every one of the face's 4
// tets has in common. That's what let the B0-B1 crossing (used whenever the
// two B-corners end up on opposite sides of the interface -- the "B-B
// yellow/blue edge") sit essentially unconstrained by anything, A-node
// potentials included, producing a wedge-shaped indent right at that edge.
// Voting once per face and applying the result uniformly guarantees all 4
// of a face's tets always agree, so the fan is always fully coupled.
#define AssignPairsSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)"

RWStructuredBuffer<uint4>  Tets : register(u0);
RWStructuredBuffer<uint>   NodeCandidateLabel : register(u1);
RWStructuredBuffer<float>  NodePotential : register(u2);
RWStructuredBuffer<uint2>  TetInterfacePair : register(u3);

void TopLabel(uint node, out uint label, out float pot)
{
    label = SENTINEL_LABEL;
    pot = -1.0e30;
    for (uint s = 0; s < 8; s++) {
        uint l = NodeCandidateLabel[node * 8 + s];
        if (l == SENTINEL_LABEL) continue;
        float p = NodePotential[node * 8 + s];
        if (p > pot) { pot = p; label = l; }
    }
}

[RootSignature(AssignPairsSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void assignInterfacePairsCS(uint3 tid : SV_DispatchThreadID)
{
    uint faceIndex = tid.x;
    if (faceIndex >= TotalFaces) return;
    uint tetBase = faceIndex * 4;

    // The face's 4 A-corners are the union of tet0's and tet2's edges
    // (opposite edges of the loop -- see buildTetsCS.hlsl); B0/B1 are the
    // same for all 4 tets of the face.
    uint4 t0 = Tets[tetBase + 0];
    uint4 t2 = Tets[tetBase + 2];
    uint nodes[6] = { t0.x, t0.y, t2.x, t2.y, t0.z, t0.w };

    uint topLabels[6];
    float topPots[6];
    for (uint c = 0; c < 6; c++) TopLabel(nodes[c], topLabels[c], topPots[c]);

    // Frequency-count the (<=6) distinct top labels, padding unused slots
    // with freq=0 so a fixed-size bubble sort can rank them uniformly.
    uint uniqueLabels[6] = { SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL, SENTINEL_LABEL };
    int freq[6] = { 0, 0, 0, 0, 0, 0 };
    uint nUnique = 0;
    for (uint c = 0; c < 6; c++) {
        uint l = topLabels[c];
        bool found = false;
        for (uint u = 0; u < nUnique; u++) if (uniqueLabels[u] == l) { freq[u]++; found = true; break; }
        if (!found) { uniqueLabels[nUnique] = l; freq[nUnique] = 1; nUnique++; }
    }

    // Sort all 6 slots descending by frequency (tie-break ascending label);
    // unused slots (freq=0) sink to the bottom.
    for (uint p = 0; p < 6; p++) {
        for (uint q = 0; q < 5; q++) {
            bool doSwap = (freq[q] < freq[q + 1]) ||
                          (freq[q] == freq[q + 1] && uniqueLabels[q] > uniqueLabels[q + 1]);
            if (doSwap) {
                int tf = freq[q]; freq[q] = freq[q + 1]; freq[q + 1] = tf;
                uint tl = uniqueLabels[q]; uniqueLabels[q] = uniqueLabels[q + 1]; uniqueLabels[q + 1] = tl;
            }
        }
    }

    uint labelI = uniqueLabels[0];
    uint labelJ = (freq[1] > 0) ? uniqueLabels[1] : labelI; // labelI==labelJ marks "no active interface"
    if (labelJ < labelI) { uint tmp = labelI; labelI = labelJ; labelJ = tmp; }

    uint2 pair = uint2(labelI, labelJ);
    TetInterfacePair[tetBase + 0] = pair;
    TetInterfacePair[tetBase + 1] = pair;
    TetInterfacePair[tetBase + 2] = pair;
    TetInterfacePair[tetBase + 3] = pair;
}
