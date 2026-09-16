#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// One-shot (manually triggered, 'F' key / "Extract Junction Footvectors"
// button -- see DistanceApp.h) extraction of a per-node "footvector": the
// vector from this node to the nearest point on the nearest local triple-
// junction line, using ONLY the existing label+phi representation (no beta/
// gamma/discriminator) -- restricted to a narrow band: a node with no
// qualifying 3-distinct-label tet-tet interface anywhere in its own
// incident-tet neighborhood gets an explicit INVALID marker (w=0), never an
// extrapolated guess.
//
// Reuses smoothnessJacobiSyntheticCS.hlsl's (currently disabled,
// ENABLE_JUNCTION_TERM) own geometry: the "3 pairwise-distinct labels on a
// tet-tet interface" test, the per-label confidence-field gradient
// (own=+phi/foreign=-phi via TetShapeGradients), and the
// Ta=cross(ga,gb)+cross(gb,gc)+cross(gc,ga) tangent formula -- algebraically
// exactly cross(ga-gb,ga-gc), the two tie-planes' normals' cross product,
// i.e. the local junction line's direction. What that (disabled) code never
// needed and this shader adds: the triple-junction POINT itself, via a 3x3
// barycentric solve on the interface's 3 shared corners (same cross-product
// reciprocal-basis trick TetShapeGradients itself already uses, see
// SolveTriplePoint below) -- so a node can be projected onto an actual line,
// not just compared against a bare direction.
//
// Unlike that disabled term, this shader never SUMS two tets' independent
// tangent estimates together (it only ever takes whichever single interface
// gives the smallest resulting distance), so the canonical-label-ordering
// sign-cancellation concern that code's own comments describe doesn't apply
// here -- Ta's sign is simply whatever it is, harmless for both projecting
// (symmetric) and the stored direction (step 2 only ever uses it as an axis
// to project onto, also sign-agnostic).
//
// FLAT (one thread per node, plain global-memory reads via ResolveCorner),
// NOT the tiled/groupshared halo scheme the disabled term uses -- a one-shot
// pass has no per-sweep performance pressure, so the extra indirection buys
// nothing here and costs correctly reaching real 3-label interfaces near the
// domain edge that a halo-bounded version would miss.
#define ExtractJunctionFootVectorsSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b0)"

RWStructuredBuffer<uint>   NodeCandidateLabel     : register(u0); // read
RWStructuredBuffer<float>  NodePotential          : register(u1); // read
RWStructuredBuffer<float4> NodeJunctionFootVector : register(u2); // write: xyz=raw footvector, w=validity (1/0)
RWStructuredBuffer<float3> NodeJunctionTangent    : register(u3); // write: normalized local junction-line direction

// Own-label-signed potential at a resolved (real, non-SENTINEL) corner --
// same own=+phi/foreign=-phi convention as smoothnessJacobiSyntheticCS.hlsl's
// (disabled) junction term.
float SignedG(uint cornerRef, uint queryLabel)
{
    uint lab = GetCandidateLabelAt(NodeCandidateLabel, cornerRef, 0u);
    float pot = NodePotential[cornerRef * MAX_CANDIDATES + 0u];
    return (lab == queryLabel) ? pot : -pot;
}

// Barycentric weights w (indexed by corner 0,1,2) solving sum(w)=1,
// dot(w,fieldL0-fieldL1)=0, dot(w,fieldL0-fieldL2)=0 -- the point on the face
// spanned by these 3 corners where all 3 labels' own/foreign fields tie.
// fieldLk.corner is label k's G-value at that corner. Solved via the same
// cross-product reciprocal-basis trick TetShapeGradients uses (w is dual to
// the constant-ones row within the frame {fieldL0-fieldL1,fieldL0-fieldL2}),
// not a general 3x3 matrix inverse -- verified algebraically: for
// w=cross(r1,r2)/dot((1,1,1),cross(r1,r2)), dot(r1,w)=dot(r2,w)=0 always
// (cross(r1,r2) is perpendicular to both by construction) and
// dot((1,1,1),w)=1 by the normalization itself.
bool SolveTriplePoint(float3 fieldL0, float3 fieldL1, float3 fieldL2, out float3 w)
{
    float3 r1 = fieldL0 - fieldL1;
    float3 r2 = fieldL0 - fieldL2;
    float3 c = cross(r1, r2);
    float det = c.x + c.y + c.z; // dot((1,1,1), c)
    if (abs(det) < 1.0e-9)
    {
        w = float3(0, 0, 0);
        return false;
    }
    w = c / det;
    return true;
}

[RootSignature(ExtractJunctionFootVectorsSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void extractJunctionFootVectorsCS(uint3 dtid : SV_DispatchThreadID)
{
    uint node = dtid.x;
    if (node >= NodeCount) return;

    float3 nodePos = NodeWorldPos(node);

    bool found = false;
    float bestDistSq = 3.402823466e+38F;
    float3 bestFoot = float3(0, 0, 0);
    float3 bestTangent = float3(0, 0, 0);

    uint ring1Tets[MAX_INCIDENT_TETS];
    uint ring1Count = GatherIncidentTets(node, ring1Tets);

    for (uint t = 0; t < ring1Count; t++)
    {
        uint tetA = ring1Tets[t];
        int3 qA0, qA1, qA2, qA3;
        GetTetCornerQs(tetA, qA0, qA1, qA2, qA3);
        uint cA0 = ResolveCorner(qA0), cA1 = ResolveCorner(qA1), cA2 = ResolveCorner(qA2), cA3 = ResolveCorner(qA3);
        if (cA0 == SENTINEL_LABEL || cA1 == SENTINEL_LABEL || cA2 == SENTINEL_LABEL || cA3 == SENTINEL_LABEL) continue;
        uint cA[4] = { cA0, cA1, cA2, cA3 };

        for (uint relation = 0; relation < 4; relation++)
        {
            uint tetB;
            if (!GetFaceAdjacentPartner(tetA, relation, tetB)) continue;

            int3 qB0, qB1, qB2, qB3;
            GetTetCornerQs(tetB, qB0, qB1, qB2, qB3);
            uint cB0 = ResolveCorner(qB0), cB1 = ResolveCorner(qB1), cB2 = ResolveCorner(qB2), cB3 = ResolveCorner(qB3);
            if (cB0 == SENTINEL_LABEL || cB1 == SENTINEL_LABEL || cB2 == SENTINEL_LABEL || cB3 == SENTINEL_LABEL) continue;
            uint cB[4] = { cB0, cB1, cB2, cB3 };

            uint sharedC[3];
            uint nShared = 0;
            for (uint ia = 0; ia < 4; ia++)
                for (uint ib = 0; ib < 4; ib++)
                    if (cA[ia] == cB[ib]) { if (nShared < 3) sharedC[nShared] = cA[ia]; nShared++; }
            if (nShared != 3) continue;

            uint l0 = GetCandidateLabelAt(NodeCandidateLabel, sharedC[0], 0u);
            uint l1 = GetCandidateLabelAt(NodeCandidateLabel, sharedC[1], 0u);
            uint l2 = GetCandidateLabelAt(NodeCandidateLabel, sharedC[2], 0u);
            if (l0 == l1 || l1 == l2 || l0 == l2) continue;

            float3 fieldL0 = float3(SignedG(sharedC[0], l0), SignedG(sharedC[1], l0), SignedG(sharedC[2], l0));
            float3 fieldL1 = float3(SignedG(sharedC[0], l1), SignedG(sharedC[1], l1), SignedG(sharedC[2], l1));
            float3 fieldL2 = float3(SignedG(sharedC[0], l2), SignedG(sharedC[1], l2), SignedG(sharedC[2], l2));

            float3 w;
            if (!SolveTriplePoint(fieldL0, fieldL1, fieldL2, w)) continue;

            float3 PS0 = NodeWorldPos(sharedC[0]), PS1 = NodeWorldPos(sharedC[1]), PS2 = NodeWorldPos(sharedC[2]);
            float3 triplePoint = w.x * PS0 + w.y * PS1 + w.z * PS2;

            float3 P0 = QWorldPos(qA0), P1 = QWorldPos(qA1), P2 = QWorldPos(qA2), P3 = QWorldPos(qA3);
            float3 wA0, wA1, wA2, wA3;
            TetShapeGradients(P0, P1, P2, P3, wA0, wA1, wA2, wA3);
            float3 wA[4] = { wA0, wA1, wA2, wA3 };

            float3 ga = float3(0, 0, 0), gb = float3(0, 0, 0), gc = float3(0, 0, 0);
            for (uint ci = 0; ci < 4; ci++)
            {
                float pot = NodePotential[cA[ci] * MAX_CANDIDATES + 0u];
                uint lab = GetCandidateLabelAt(NodeCandidateLabel, cA[ci], 0u);
                ga += pot * ((lab == l0) ? 1.0 : -1.0) * wA[ci];
                gb += pot * ((lab == l1) ? 1.0 : -1.0) * wA[ci];
                gc += pot * ((lab == l2) ? 1.0 : -1.0) * wA[ci];
            }
            float3 Ta = cross(ga, gb) + cross(gb, gc) + cross(gc, ga);
            float taLen = length(Ta);
            if (taLen < 1.0e-8) continue; // degenerate here -- no real tangent direction to project onto
            float3 dirN = Ta / taLen;

            float tProj = dot(nodePos - triplePoint, dirN);
            float3 foot = triplePoint + tProj * dirN;
            float distSq = dot(foot - nodePos, foot - nodePos);

            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestFoot = foot;
                bestTangent = dirN;
                found = true;
            }
        }
    }

    if (found)
    {
        NodeJunctionFootVector[node] = float4(bestFoot - nodePos, 1.0);
        NodeJunctionTangent[node] = bestTangent;
    }
    else
    {
        NodeJunctionFootVector[node] = float4(0, 0, 0, 0);
        NodeJunctionTangent[node] = float3(0, 0, 0);
    }
}
