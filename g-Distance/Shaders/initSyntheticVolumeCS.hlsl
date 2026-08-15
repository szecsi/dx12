#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// One-time (per Reinit) seed for the synthetic-field volume buffer -- see
// smoothnessJacobiSyntheticCS.hlsl's per-sweep NodeSyntheticVolume write
// (the closed-form tet-fan "current volume"). NodePotential is NOT a volume
// -- it's derived from JFA footvector length, a distance, not a geometric
// quantity -- so it must not be used as a stand-in here even transiently.
// Instead seed directly from ground truth: A-nodes start fully confident in
// their own (fixed, correct) label, B-nodes start with essentially none,
// mirroring the same A-vs-B confidence asymmetry buildCandidatesCS/
// buildSyntheticBCS already use for potentials. Nudged off exact 1/0 to
// avoid ever multiplying by a literal zero downstream. This only matters
// for the first sweep's halo reads (every node gets a real, tet-fan-computed
// value once it's first been somebody's target) but should still be a
// sensible guess, not an arbitrary placeholder -- and, regardless, leaving
// this buffer truly uninitialized would risk NaN (0*NaN is NaN, not 0, so
// even VolumeRatioWeight=0 would not mask it).
#define InitSyntheticVolumeSig "RootFlags(0)," \
    "UAV(u0)," \
    "CBV(b0)"

RWStructuredBuffer<float> NodeSyntheticVolume : register(u0);

[RootSignature(InitSyntheticVolumeSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void initSyntheticVolumeCS(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= NodeCount) return;
    NodeSyntheticVolume[tid.x] = (tid.x < ACount) ? 0.999 : 0.001;
}
