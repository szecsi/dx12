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
//
// Also seeds NodeSensitivity (d(currentVolume)/d(myPot), see
// smoothnessJacobiSyntheticCS.hlsl's dcontrib) to a flat 0 -- unlike the
// volume seed above, there's no principled ground-truth guess for a
// derivative before any real potentials/labels have settled, but 0 is safe:
// it only affects the very first sweep's halo-wide sumSensitivity, and if
// every halo member of a label happens to still be seed-zero, the volume
// term's own guard (sumSensitivity>epsFloor) just skips the correction for
// that one sweep rather than dividing by ~0.
//
// Also seeds NodeVolumeAlignment (the continuous smoothing-vs-volume
// agreement signal, see smoothnessJacobiSyntheticCS.hlsl's myAlignment) to a
// flat 0 -- same reasoning as NodeSensitivity above: no principled guess
// before anything has settled, and 0 (neutral, "no opinion") is safe since
// it only affects the very first sweep's halo-wide min/max/sumAgreeSensitivity
// scan, which has its own zero-leverage/zero-range fallbacks already.
#define InitSyntheticVolumeSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<float> NodeSyntheticVolume : register(u0);
RWStructuredBuffer<float> NodeSensitivity : register(u1);
RWStructuredBuffer<float> NodeVolumeAlignment : register(u2);

[RootSignature(InitSyntheticVolumeSig)]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void initSyntheticVolumeCS(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= NodeCount) return;
    NodeSyntheticVolume[tid.x] = (tid.x < ACount) ? 0.999 : 0.001;
    NodeSensitivity[tid.x] = 0.0;
    NodeVolumeAlignment[tid.x] = 0.0;
}
