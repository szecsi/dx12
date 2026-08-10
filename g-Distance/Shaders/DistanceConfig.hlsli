#ifndef DISTANCE_CONFIG_HLSLI
#define DISTANCE_CONFIG_HLSLI

// Compile-time constants shared by every g-Distance compute/render shader.
// Mirrors g-Aequor's AequorConfig.hlsli / g-BCC's bccCommon.hlsli conventions
// -- see DistanceApp.h for the "must match" C++-side duplicates.

#define THREAD_GROUP_SIZE 256

// Grid resolution is a runtime value now (GUI slider, applied on
// Reinitialize -- see Shaders/DistanceGridCb.hlsli and DistanceApp.h's
// EnsureGridBuffersSized), not a compile-time constant. CELL_SIZE is a
// physical unit size, independent of resolution, so it stays compile-time.
#define CELL_SIZE 1.0f

#define SENTINEL_LABEL 0xFFFFFFFFu

// Candidate-label domain: up to 31 real labels (0 = background/outside),
// value 31 reserved as "no candidate here" -- distinct from SENTINEL_LABEL
// above, which sentinels NODE INDICES (a different, much wider domain: e.g.
// ResolveCorner's "virtual/out-of-grid corner" result). 5 bits per label
// packs MAX_CANDIDATES=6 of them into a single uint (6*5=30 bits) -- 6 is
// exactly floor(32/5), the largest slot count that packs into one uint at
// all (8 would need 40 bits, spilling into a second word). See
// nodeCandidateLabelBuffer (DistanceApp.h) / GetCandidateLabelAt
// (DistanceLattice.hlsli).
#define SENTINEL_CANDIDATE 31u
#define MAX_CANDIDATES 6u

// Fixed per-node capacity for the on-the-fly incident-tet gather
// (DistanceLattice.hlsli's GatherIncidentTets) -- a node touches at most 24
// tets (6 tets each as D0 or D1 of its own cube, 2 tets each for its 6 ring
// positions -- verified exactly, every real node hits this bound exactly).
// 32 leaves headroom without meaningfully growing the local array size.
#define MAX_INCIDENT_TETS 32u

#endif
