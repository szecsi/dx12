#include "AequorCb.hlsli"
#include "OccurrenceField.hlsli"

#define AnchorBarrierSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)"

RWStructuredBuffer<float3> Position : register(u0);
RWStructuredBuffer<uint>   Label    : register(u1);
// GRID_DIM^3 * 4 entries -- slice [label*GRID_DIM^3, (label+1)*GRID_DIM^3)
// is that label's converged nearest-same-label-voxel field (occFinalizeCS).
// Written once during init, then read-only for the rest of the relaxation
// loop -- but still bound as UAV (not SRV) like every other particle/grid
// buffer, so no state transition is ever needed for it either.
RWStructuredBuffer<uint2> OccurrenceSeed : register(u2);

RWStructuredBuffer<float3> ScratchVec3 : register(u3);

// One-sided barrier: "is there a voxel of my own label within AnchorRadius
// of my CURRENT position" against the static ground-truth grid -- zero
// effect while true (protects sub-cell drift/sampling-artifact-scale wobble
// from ever showing as a visible protrusion), a pull-back only once a
// particle has drifted past where the ground-truth grid actually supports
// its label existing. Deliberately keyed to ANY voxel of the label, not the
// particle's own spawn origin -- see design discussion in the g-Aequor plan.
[RootSignature(AnchorBarrierSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void anchorBarrierCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint i = dispatchID.x;
    if (i >= numParticles) return;
    float3 pi = Position[i];
    uint myLabel = Label[i];
    if (myLabel == SENTINEL_LABEL) { ScratchVec3[i] = pi; return; }

    int3 cell = posToCell(pi);
    uint ci = cellIndex(cell);
    uint2 occ = OccurrenceSeed[myLabel * (GRID_DIM * GRID_DIM * GRID_DIM) + ci];

    if (occ.x == SENTINEL_LABEL) { ScratchVec3[i] = pi; return; } // no voxel of this label anywhere (shouldn't happen for an active label)

    float3 seedPos = CellSeedWorldPos(occ.x);
    float d = distance(pi, seedPos);

    if (d <= AnchorRadius) {
        ScratchVec3[i] = pi; // true no-op below the radius
        return;
    }

    float3 dir = (d > 1.0e-8) ? (seedPos - pi) / d : float3(0, 0, 1);
    // Clamped like every other position-correcting constraint -- a badly
    // drifted particle walks back over several iterations rather than
    // snapping in one (potentially destabilizing) jump.
    float pull = min(d - AnchorRadius, MaxStep);
    ScratchVec3[i] = pi + pull * dir;
}
