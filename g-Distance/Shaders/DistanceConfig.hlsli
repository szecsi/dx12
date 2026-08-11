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

// Candidate-label domain: up to 255 real labels (0 = background/outside),
// value 255 reserved as "no candidate here" -- distinct from SENTINEL_LABEL
// above, which sentinels NODE INDICES (a different, much wider domain: e.g.
// ResolveCorner's "virtual/out-of-grid corner" result). 8 bits per label
// packs MAX_CANDIDATES=8 of them into TWO uints (4 per word, 4*8=32 bits
// each) -- nodeCandidateLabelBuffer is now 2 uints per node, word = slot/4,
// shift = (slot%4)*8. See nodeCandidateLabelBuffer (DistanceApp.h) /
// GetCandidateLabelAt (DistanceLattice.hlsli).
#define SENTINEL_CANDIDATE 255u
#define MAX_CANDIDATES 8u

// Fixed per-node capacity for the on-the-fly incident-tet gather
// (DistanceLattice.hlsli's GatherIncidentTets) -- a node touches at most 24
// tets (6 tets each as D0 or D1 of its own cube, 2 tets each for its 6 ring
// positions -- verified exactly, every real node hits this bound exactly).
// 32 leaves headroom without meaningfully growing the local array size.
#define MAX_INCIDENT_TETS 32u

// Capacity for GatherIncidentTets2 (DistanceLattice.hlsli): a node's own
// incident tets (ring 1, <=MAX_INCIDENT_TETS) plus every tet FACE-ADJACENT
// to a ring-1 tet (ring 2, via GetFaceAdjacentPartner) not already in ring
// 1, each tet listed once. Since 3 of a ring-1 tet's 4 face-adjacent
// partners already lie in ring 1 (they share the face containing `node`
// itself), ring 2 contributes at most ~1 new tet per ring-1 tet -- so
// ring1+ring2 lands around 2*MAX_INCIDENT_TETS in the worst case; 48 is a
// practical cap, not a proven exact bound -- GatherIncidentTets2 stops
// early (silently truncated) rather than overflowing if a config ever
// exceeds it.
#define MAX_INCIDENT_TETS2 48u

#endif
