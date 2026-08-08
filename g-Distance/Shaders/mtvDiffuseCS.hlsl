#include "DistanceCb.hlsli"
#include "DistanceLattice.hlsli"

// Outer-round MTV step 2 of 3: for every node X, (a) diffuse MTV toward the
// average of its CURRENT same-label incident-tet neighbors (equalizing
// within a connected same-label region -- a node with no same-label
// neighbor at all has nothing to average with, so it simply keeps its full
// MTV, which is exactly the mechanism that protects an isolated feature
// from shrinking), (b) let X's own ACTUAL current reconstructed volume
// (NodeCurrentVolume, VolumePushbackRate) pull the target toward whatever
// is already being achieved -- same diffCount>0 gate as (a), so an isolated
// node is untouched by this too, but a well-connected region's MTV can
// relax toward its natural (possibly non-uniform) smoothness-driven volume
// distribution instead of fighting to match a flat diffused average
// everywhere it doesn't need to, and (c) apply any flip-share adjustments
// from neighbors that changed label this round (mtvFlipDetectCS.hlsl, step
// 1): X loses a share if it was one of a flipping neighbor's OLD-label
// neighbors, gains a share if it's one of that neighbor's NEW-label
// neighbors. Also writes NodePrevLabelScratch[X] = X's own current label,
// becoming next round's flip-detection baseline. Same gather-not-scatter,
// read-stable/write-scratch pattern as every other relaxation pass in this
// project.
#define MtvDiffuseSig "RootFlags(0)," \
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
    "UAV(u10)," \
    "UAV(u11)," \
    "UAV(u12)"

RWStructuredBuffer<uint4>  Tets : register(u0);
RWStructuredBuffer<uint>   NodeIncidentCount : register(u1);
RWStructuredBuffer<uint>   NodeIncidentTets : register(u2);
RWStructuredBuffer<uint>   NodeCandidateLabel : register(u3);
RWStructuredBuffer<float>  NodePotential : register(u4);
RWStructuredBuffer<uint>   NodePrevLabel : register(u5);
RWStructuredBuffer<float>  NodeMTV : register(u6);
RWStructuredBuffer<uint>   NodeFlipFlag : register(u7);
RWStructuredBuffer<float>  NodeFlipShareOld : register(u8);
RWStructuredBuffer<float>  NodeFlipShareNew : register(u9);
RWStructuredBuffer<float>  NodeMTVScratch : register(u10);
RWStructuredBuffer<uint>   NodePrevLabelScratch : register(u11);
RWStructuredBuffer<float>  NodeCurrentVolume : register(u12);

uint TopLabelOf(uint node)
{
    uint bestLabel = SENTINEL_LABEL;
    float bestPot = -1.0e30;
    for (uint s = 0; s < 8; s++) {
        uint l = NodeCandidateLabel[node * 8 + s];
        if (l == SENTINEL_LABEL) continue;
        float p = NodePotential[node * 8 + s];
        if (p > bestPot) { bestPot = p; bestLabel = l; }
    }
    return bestLabel;
}

[RootSignature(MtvDiffuseSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void mtvDiffuseCS(uint3 tid : SV_DispatchThreadID)
{
    uint node = tid.x;
    if (node >= NodeCount) return;

    uint myLabel = TopLabelOf(node);
    float myMTV = NodeMTV[node];

    float diffSum = 0.0;
    uint diffCount = 0;
    float flipDelta = 0.0;

    uint incCount = min(NodeIncidentCount[node], MAX_INCIDENT_TETS);
    for (uint e = 0; e < incCount; e++) {
        uint tetIdx = NodeIncidentTets[node * MAX_INCIDENT_TETS + e];
        uint4 t = Tets[tetIdx];
        uint verts[4] = { t.x, t.y, t.z, t.w };
        for (uint c = 0; c < 4; c++) {
            uint other = verts[c];
            if (other == node) continue;

            uint otherLabel = TopLabelOf(other);
            if (otherLabel == myLabel) {
                diffSum += NodeMTV[other];
                diffCount++;
            }

            if (NodeFlipFlag[other] != 0) {
                if (myLabel == NodePrevLabel[other]) flipDelta -= NodeFlipShareOld[other];
                if (myLabel == otherLabel)            flipDelta += NodeFlipShareNew[other];
            }
        }
    }

    float diffusionDelta = 0.0;
    if (diffCount > 0) {
        float avg = diffSum / (float)diffCount;
        diffusionDelta = MTVDiffusionRate * (avg - myMTV)
                        + VolumePushbackRate * (NodeCurrentVolume[node] - myMTV);
    }

    NodeMTVScratch[node] = myMTV + diffusionDelta + flipDelta;
    NodePrevLabelScratch[node] = myLabel;
}
