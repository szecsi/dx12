#include "DistanceFrameCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b1
#include "DistanceLattice.hlsli"
#include "LabelPalette.hlsli"
#include "JunctionCrossing.hlsli"

// Edge-data-driven variant of raymarchLatticePS.hlsl's junction-aware
// lattice raymarch -- step 1 of the per-edge rendering revamp (see the
// approved plan). Walks the SAME q-space tet-by-tet path, but decides
// "is there anything to render in this tet" purely from the precomputed
// per-edge homo/hetero bits (NodeEdgeData, buildEdgeDataCS.hlsl) instead of
// reading NodeCandidateLabel at every corner of every tet along the way --
// node labels (and potentials) are read ONLY once a non-homogeneous tet is
// actually found, right before shading, exactly deferred as far as the edge
// data allows.
//
// Per step, of the new tet's 6 edges, the 3 connecting its 3 corners
// RETAINED from the previous tet are copied from the previous tet's own
// edge-hetero state (matched by Q-SPACE VALUE, since corner indices get
// reassigned every step -- GetTetCornerQs has no reason to keep a retained
// corner at the same index slot); only the 3 edges touching the newly-
// entered 4th corner need a fresh NodeEdgeData read. The very first tet
// along a ray has no "previous" state, so all 6 are read fresh there.
//
// Once a non-homogeneous tet is found, this defers to JunctionCrossing.hlsli's
// FindJunctionCrossing -- the same per-distinct-label pooled-field
// reconstruction raymarchLatticePS.hlsl itself uses, but here fed the real
// per-corner edge-derivative multipliers (a direct NodeEdgeDerivMult lookup,
// see the approved plan's per-face redesign -- multipliers are a genuinely
// per-NODE quantity, not per-edge-end, so no tet-walk retention is needed
// for them at all, unlike the hetero bit) so the two paths only render
// IDENTICALLY while every multiplier is 1.0 -- raymarchLatticePS.hlsl itself
// has no edge data at all and always passes an all-1.0 array.
//
// v1 scope, same as raymarchLatticePS.hlsl: synthetic-field pipeline only,
// no beta/gamma yet.

RWStructuredBuffer<float4> NodeEdgeData       : register(u0); // read (.x = hetero bit only)
RWStructuredBuffer<uint>   NodeCandidateLabel : register(u1); // read, deferred until a hetero tet is found
RWStructuredBuffer<float>  NodePotential      : register(u2); // read, deferred until a hetero tet is found
RWStructuredBuffer<float>  NodeEdgeDerivMult  : register(u3); // read, deferred until a hetero tet is found (TestShape_ClippedSpheres only -- 1.0 elsewhere)

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

struct PsOut {
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

float3 QOf(float3 r) { return float3(r.x + r.y, r.x + r.z, r.y + r.z); }

bool IntersectWorldBox(float3 ro, float3 rd, out float tEnter, out float tExit)
{
    float3 boxMin = float3(-0.5, -0.5, -0.5) * CELL_SIZE;
    float3 boxMax = (float3((float)GridRes, (float)GridRes, (float)GridRes) - 0.5) * CELL_SIZE;
    float3 invD = 1.0 / rd;
    float3 t0 = (boxMin - ro) * invD;
    float3 t1 = (boxMax - ro) * invD;
    float3 tSmall = min(t0, t1);
    float3 tBig = max(t0, t1);
    tEnter = max(max(tSmall.x, tSmall.y), tSmall.z);
    tExit = min(min(tBig.x, tBig.y), tBig.z);
    return tEnter <= tExit;
}

// A tet's 6 edges as fixed corner-index pairs (matches wireframeVS.hlsl's
// own TetEdges convention).
static const uint2 TetEdgePairs[6] = {
    uint2(0, 1), uint2(0, 2), uint2(0, 3), uint2(1, 2), uint2(1, 3), uint2(2, 3)
};

// Which of the 6 fixed pairs above corresponds to corner indices (a,b).
uint EdgePairIndex(uint a, uint b)
{
    if (a > b) { uint t = a; a = b; b = t; }
    if (a == 0) return b - 1; // (0,1)->0 (0,2)->1 (0,3)->2
    if (a == 1) return 1 + b; // (1,2)->3 (1,3)->4
    return 5;                 // (2,3)->5
}

// Fresh read for one tet edge's hetero bit (corners at Q-space qA,qB) from
// NodeEdgeData -- resolves which of the 2 corners "owns" this edge
// (whichever reaches the other via one of the 7 canonical forward offsets)
// and reads that owner's slot. Falls back to "homo" if either corner is
// outside the real domain (SENTINEL) -- matches every other lattice
// shader's virtual-background-node convention.
float ReadEdgeHetero(int3 qA, int3 qB)
{
    int3 d = qB - qA;
    uint slot;
    int3 ownerQ;
    if (ForwardSlotOf(d, slot)) ownerQ = qA;
    else if (ForwardSlotOf(-d, slot)) ownerQ = qB;
    else return 0.0; // not a real lattice edge -- shouldn't happen for an actual tet edge

    uint ownerRef = ResolveCorner(ownerQ);
    if (ownerRef == SENTINEL_LABEL) return 0.0;
    return NodeEdgeData[ownerRef * 7 + slot].x;
}

// Per-corner edge-derivative multiplier -- a direct per-NODE lookup (see the
// approved plan's per-face redesign: the multiplier is a genuinely per-node
// quantity, not per-edge-end, so there's no tet-walk retention/consistency
// bookkeeping needed here at all, unlike the hetero bit above). 1.0
// (identity) for a virtual/SENTINEL corner, matching every other lattice
// shader's background-node convention.
float CornerDerivMult(uint cornerRef)
{
    if (cornerRef == SENTINEL_LABEL) return 1.0;
    return NodeEdgeDerivMult[cornerRef];
}

// No [RootSignature(...)] here -- declared once by raymarchEdgeVS.hlsl,
// reused for both stages of this draw (this codebase's convention).
PsOut raymarchEdgePS(VsOut input)
{
    PsOut result;
    result.color = float4(0.05, 0.06, 0.08, 1.0);
    result.depth = 1.0;

    float3 ro = cameraPos.xyz;
    float3 rd = normalize(input.rayDir);

    float tEnter, tExit;
    if (!IntersectWorldBox(ro, rd, tEnter, tExit) || tExit < 0.0) return result;

    float3 q0 = QOf(ro / CELL_SIZE);
    float3 qd = QOf(rd / CELL_SIZE);

    float tCur = max(tEnter, 0.0);
    float3 qp = q0 + qd * tCur;
    int3 C = int3(floor(qp));
    uint slot = TetSlotFromFrac(qp - (float3)C);

    const float epsT = 1.0e-4;
    const float epsSlope = 1.0e-6;
    uint maxSteps = (uint)max(raymarchParams.w, 1.0);

    int3 qArr[4] = {
        C, C + int3(1, 1, 1),
        C + CubeVertexOffsets[slot][0], C + CubeVertexOffsets[slot][1]
    };
    // Cold start: no previous tet to retain edges from, so all 6 are read
    // fresh here.
    float edgeHetero[6];
    [unroll]
    for (uint e0 = 0; e0 < 6; e0++)
        edgeHetero[e0] = ReadEdgeHetero(qArr[TetEdgePairs[e0].x], qArr[TetEdgePairs[e0].y]);

    for (uint iter = 0; iter < maxSteps; iter++) {
        if (tCur > tExit + epsT) break; // left the visible domain

        float3 P[4];
        for (uint c = 0; c < 4; c++) P[c] = (float3)qArr[c];

        float bestT = 1.0e30;
        int bestExit = -1;
        for (uint i = 0; i < 4; i++) {
            float3 Pa = P[FaceCorners[i][0]], Pb = P[FaceCorners[i][1]], Pc = P[FaceCorners[i][2]];
            float3 N = cross(Pb - Pa, Pc - Pa);
            float denom = dot(N, qd);
            if (abs(denom) < 1.0e-8) continue;
            float ti = dot(N, Pa - q0) / denom;
            if (ti > tCur + epsT && ti < bestT) { bestT = ti; bestExit = (int)i; }
        }
        if (bestExit < 0) break; // degenerate (ray exits exactly along an edge/vertex) -- bail out rather than loop forever

        bool anyHetero = (edgeHetero[0] > 0.5) || (edgeHetero[1] > 0.5) || (edgeHetero[2] > 0.5)
                       || (edgeHetero[3] > 0.5) || (edgeHetero[4] > 0.5) || (edgeHetero[5] > 0.5);

        if (anyHetero) {
            // Only NOW read the actual corner labels/potentials -- deferred
            // exactly as far as the edge data allows.
            uint cornerLabel[4]; float cornerPot[4]; uint cornerRef[4];
            [unroll]
            for (uint c2 = 0; c2 < 4; c2++) {
                cornerRef[c2] = ResolveCorner(qArr[c2]);
                if (cornerRef[c2] == SENTINEL_LABEL) { cornerLabel[c2] = 0u; cornerPot[c2] = 1.0; }
                else {
                    cornerLabel[c2] = GetCandidateLabelAt(NodeCandidateLabel, cornerRef[c2], 0u);
                    cornerPot[c2] = NodePotential[cornerRef[c2] * MAX_CANDIDATES + 0u];
                }
            }
            float3 Pw[4];
            for (uint c3 = 0; c3 < 4; c3++) Pw[c3] = QWorldPos(qArr[c3]);

            float cornerMult[4];
            [unroll]
            for (uint c8 = 0; c8 < 4; c8++) cornerMult[c8] = CornerDerivMult(cornerRef[c8]);

            JunctionHit jh = FindJunctionCrossing(Pw, cornerLabel, cornerPot, cornerMult, ro, rd, viewProjTransform, tCur, bestT, epsT, epsSlope);
            if (jh.hit) {
                result.depth = jh.depthNdc;
                result.color = float4(jh.color, 1.0);
                return result;
            }
        }

        int3 oldQArr[4] = { qArr[0], qArr[1], qArr[2], qArr[3] };
        float oldEdgeHetero[6];
        [unroll]
        for (uint e1 = 0; e1 < 6; e1++) oldEdgeHetero[e1] = edgeHetero[e1];

        if (!AdvanceTetAcrossFace(C, slot, (uint)bestExit)) break;
        tCur = bestT;

        int3 newQArr[4] = {
            C, C + int3(1, 1, 1),
            C + CubeVertexOffsets[slot][0], C + CubeVertexOffsets[slot][1]
        };

        // Match new corners to old ones BY Q-SPACE VALUE (corner indices are
        // not stable across a step) -- exactly 3 of the 4 new corners must
        // match an old one (the 4th is the newly-entered corner) for any
        // real tet-to-tet step.
        int oldIndexOfNewCorner[4];
        [unroll]
        for (uint nc = 0; nc < 4; nc++) {
            oldIndexOfNewCorner[nc] = -1;
            [unroll]
            for (uint oc = 0; oc < 4; oc++) {
                if (all(newQArr[nc] == oldQArr[oc])) { oldIndexOfNewCorner[nc] = (int)oc; break; }
            }
        }

        float newEdgeHetero[6];
        [unroll]
        for (uint e2 = 0; e2 < 6; e2++) {
            uint a = TetEdgePairs[e2].x, b = TetEdgePairs[e2].y;
            int oa = oldIndexOfNewCorner[a], ob = oldIndexOfNewCorner[b];
            if (oa >= 0 && ob >= 0) {
                newEdgeHetero[e2] = oldEdgeHetero[EdgePairIndex((uint)oa, (uint)ob)];
            } else {
                newEdgeHetero[e2] = ReadEdgeHetero(newQArr[a], newQArr[b]);
            }
        }

        qArr[0] = newQArr[0]; qArr[1] = newQArr[1]; qArr[2] = newQArr[2]; qArr[3] = newQArr[3];
        [unroll]
        for (uint e3 = 0; e3 < 6; e3++) edgeHetero[e3] = newEdgeHetero[e3];
    }

    return result;
}
