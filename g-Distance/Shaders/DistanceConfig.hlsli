#ifndef DISTANCE_CONFIG_HLSLI
#define DISTANCE_CONFIG_HLSLI

// Compile-time constants shared by every g-Distance compute/render shader.
// Mirrors g-Aequor's AequorConfig.hlsli / g-BCC's bccCommon.hlsli conventions
// -- see DistanceApp.h for the "must match" C++-side duplicates.

#define THREAD_GROUP_SIZE 256

// A-sublattice corner-grid resolution (B-sublattice cube centers = GRID_RES-1
// per axis). Kept modest for v1 -- see DistanceApp.h's TetCount comment for
// the resulting tet count.
#define GRID_RES 20u
#define CELL_SIZE 1.0f

#define SENTINEL_LABEL 0xFFFFFFFFu
#define MAX_CANDIDATES 8u

// Fixed per-node capacity for the "which tets touch me" reverse-adjacency
// list (buildIncidentCS.hlsl) -- an interior node touches at most 24 tets
// (6 incident cube-faces x 4 tets/face for a B-node; up to 12 incident faces
// x 2 tets/face for an A-node, see buildTetsCS.hlsl's derivation comment).
// 32 leaves headroom without meaningfully growing buffer size.
#define MAX_INCIDENT_TETS 32u

#endif
