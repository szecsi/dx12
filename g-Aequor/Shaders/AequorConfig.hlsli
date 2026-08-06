#ifndef AEQUOR_CONFIG_HLSLI
#define AEQUOR_CONFIG_HLSLI

// Compile-time constants shared by every g-Aequor compute/render shader.
// Mirrors pbf's SharedConfig.hlsli / g-BCC's bccCommon.hlsli conventions, but
// scoped to exactly what this project needs -- see AequorApp.h for the
// "must match" C++-side duplicates (same convention g-BCC's GridRes/CellSize
// used, rather than a dual-mode shared header, since these are pure
// preprocessor constants with no runtime-tunable fields).

#define THREAD_GROUP_SIZE 256
#define CELL_PER_H 1

// Spatial/rasterization grid: single simple-cubic grid, GRID_DIM^3 cells,
// reused for BOTH the particle proximity search (SpatialGrid) and the
// ground-truth label rasterization + occurrence-distance field. Centered at
// the origin (unlike g-BCC's positive-only lattice-index space -- there is
// no lattice here, so there's no reason to avoid negative coordinates).
#define GRID_DIM 64
#define H 1.0f
#define CELL_SIZE (H / (float)CELL_PER_H)
#define BOX_HALF_EXTENT (GRID_DIM * CELL_SIZE / 2.0f)

#define SENTINEL_LABEL 0xFFFFFFFFu

// Fixed particle capacity -- spawnCS fills the first activeParticleCount
// slots via an atomic counter; the rest default to label = SENTINEL_LABEL
// and are skipped by every constraint kernel.
#define MAX_PARTICLES 16384

// Surface particle spacing / SPH kernel radius for the same-label spacing
// (density) constraint -- same role as pbf's PARTICLE_SPACING/H, tuned
// independently since these particles live on a 2D surface, not in a 3D
// fluid volume.
#define PARTICLE_SPACING 1.0f
#define RHO0 (1.0f / (PARTICLE_SPACING * PARTICLE_SPACING * PARTICLE_SPACING))

#define PBF_PI 3.14159265358979323846f
#define POLY6_COEFF       (315.0f / (64.0f * PBF_PI * H*H*H*H*H*H*H*H*H))
#define SPIKY_GRAD_COEFF  (45.0f  / (PBF_PI * H*H*H*H*H*H))

#endif
