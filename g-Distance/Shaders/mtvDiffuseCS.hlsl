#include "DistanceCb.hlsli"
#include "DistanceLattice.hlsli"

// Outer-round MTV step 2 of 3: for every node X, exchange MTV with each
// CURRENT same-label incident-tet neighbor Y via a single unified, exactly
// mass-conservative pairwise flow:
//
//   flow(X->Y) = MTVDiffusionRate  * (MTV[Y]           - MTV[X])
//              + VolumePushbackRate * (Residual[Y]       - Residual[X])
//              + SmoothPressureRate * (SmoothPressure[Y] - SmoothPressure[X])
//
// where Residual[N] = CurrentVolume[N] - MTV[N] (how much N is over/under-
// achieving its own current target). X accumulates flow(X->Y) SUMMED (not
// averaged) over every same-label neighbor encounter. Because this formula
// only ever reads X's and Y's own scalar fields and is evaluated identically
// regardless of which node's thread is asking, Y's own thread computes
// EXACTLY -flow(X->Y) for this same edge -- so whatever X gains, Y loses,
// for every (node, incident-tet, other-corner) encounter, therefore for the
// whole connected same-label component's total, therefore for the whole
// system (mirroring exactly how AccumulatePair in smoothnessJacobiCS.hlsl
// already achieves symmetric per-tet-pair contributions with no atomics).
// Same non-dedup convention as that gather too: a neighbor sharing multiple
// tets with X is encountered -- and exchanges flow -- once per shared tet,
// not once per distinct identity, weighting by connectivity strength
// exactly like the smoothness term already does.
//
// A node with NO same-label neighbor at all (the isolated-feature case)
// simply never enters the summed branch, so it's automatically untouched --
// no explicit gate needed, unlike the old per-node-pull formulation.
//
// This directly replaces an earlier, non-conservative version where
// VolumePushbackRate/SmoothPressureRate pulled a node's OWN MTV toward its
// OWN target independently of any neighbor -- which is not a transfer at
// all, just N simultaneous unilateral creations/destructions with nothing
// to check a whole connected component (e.g. a thin Line, structurally
// unable to be locally smooth ANYWHERE along its length, unlike a torus)
// from uniformly shrinking to nothing together. It also incidentally fixes
// a smaller pre-existing leak in the plain diffusion term itself: SUMMING
// (not averaging) is exactly conservative regardless of node degree, while
// the old normalized-average version silently leaked mass on any
// irregular-degree graph (e.g. a Line's degree-1 endpoints vs degree-2
// interior nodes).
//
// Also applies any flip-share adjustments from neighbors that changed label
// this round (mtvFlipDetectCS.hlsl, step 1): a flipping neighbor F keeps
// its OWN mtv unchanged (only F's neighbors are adjusted here), but F's
// departure/arrival should shift each GROUP's total, not F's own value --
// so X GAINS a share if it was one of F's OLD-label neighbors (the group
// F is leaving absorbs back what F had been accumulating as a member,
// keeping that group's collective total from shrinking just because a
// member departed) and X LOSES a share if it's one of F's NEW-label
// neighbors (a small compensating draw, since F arrives already carrying
// its own retained value). Also writes NodePrevLabelScratch[X] = X's own
// current label, becoming next round's flip-detection baseline. Same
// gather-not-scatter, read-stable/write-scratch pattern as every other
// relaxation pass in this project.
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
    "UAV(u12)," \
    "UAV(u13)"

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
RWStructuredBuffer<float>  NodeSmoothPressure : register(u13);

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
    float myResidual = NodeCurrentVolume[node] - myMTV;
    float mySmoothPressure = NodeSmoothPressure[node];

    float flowSum = 0.0;
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
                float otherMTV = NodeMTV[other];
                float otherResidual = NodeCurrentVolume[other] - otherMTV;
                float otherSmoothPressure = NodeSmoothPressure[other];

                flowSum += MTVDiffusionRate  * (otherMTV - myMTV)
                         + VolumePushbackRate * (otherResidual - myResidual)
                         + SmoothPressureRate * (otherSmoothPressure - mySmoothPressure);
            }

            if (NodeFlipFlag[other] != 0) {
                if (myLabel == NodePrevLabel[other]) flipDelta += NodeFlipShareOld[other];
                if (myLabel == otherLabel)            flipDelta -= NodeFlipShareNew[other];
            }
        }
    }

    // Summing (not averaging) over up to MAX_INCIDENT_TETS same-label
    // encounters means a well-connected node's effective per-round pull can
    // reach rate*MAX_INCIDENT_TETS -- well past stable for an explicit
    // diffusion step (confirmed via soak: system-wide total MTV exploded
    // within a handful of rounds without this). Dividing the WHOLE flowSum
    // by the SAME fixed, node-independent constant for every node is just a
    // uniform rescaling of all three rates -- it does not reintroduce the
    // per-node-degree conservation leak (that only happens when the divisor
    // itself varies node to node, e.g. dividing by each node's own
    // diffCount, which is exactly the old averaging bug this file replaced).
    // This bounds the worst-case per-round pull to roughly the original
    // rate scale regardless of degree; low-degree nodes (e.g. an isolated
    // point's single same-label neighbor, if it had one, or a Line's
    // endpoints) end up moving slower than before, which is fine -- this
    // whole system already needs many rounds to converge (e.g. the torus
    // needing ~60), so slower-but-stable beats fast-but-explosive.
    flowSum /= (float)MAX_INCIDENT_TETS;

    // MTV is a target VOLUME -- never physically negative. Without this
    // floor, a weakly-connected node whose pushback/pressure terms decay it
    // toward 0 can overshoot slightly negative (confirmed via soak: mtvMin
    // decayed geometrically then crossed zero around round 78 on the Line
    // test shape). Now that flowSum is a true conservative exchange this
    // should be rare, but the floor is cheap insurance and still correct to
    // keep -- MTV must never be negative regardless of mechanism.
    NodeMTVScratch[node] = max(myMTV + flowSum + flipDelta, 0.0);
    NodePrevLabelScratch[node] = myLabel;
}
