#pragma once

// Fixed-capacity, per-node record of the SparseLabelCount (4) labels whose
// boundary is nearest this node -- built once by RunSparseLabelSeedInit's K
// independent per-label boundary-distance JFA sweeps (sparseLabelSeedCS.hlsl/
// sparseLabelHarvestCS.hlsl) and never revisited afterward, regardless of how
// many distinct labels K the scene actually has. 4 is not an arbitrary cap:
// it's the generic maximum junction degree for 3D cellular structures (four
// regions meeting at a point, per Plateau's laws), so it's enough to
// correctly describe even a quadruple point, not just today's K==4 test
// scene. Slot 0 is always the node's nearest (smallest |dist|) label;
// remaining slots are the next-nearest competitors, sorted ascending by
// |dist|. A slot's label is SENTINEL_LABEL if fewer than SparseLabelCount
// distinct labels exist anywhere in the scene (a label with no voxels
// anywhere has no boundary at all, so it never wins any insertion).
// Indexed the same way QuadricFootField.hlsli's flat node index is
// (EncodeIndex/DecodeIndex), so downstream consumers can share the index
// space.
static const uint SparseLabelCount = 4;

struct SparseNodeSeeds
{
    uint4  labels; // labels.x/y/z/w = slot 0..3's label, or SENTINEL_LABEL
    // Signed distance to that label's own boundary (the label/not-label
    // interface) -- positive when this node is on the labeled side, negative
    // when it's on the other side. This is what quadricSeedCS.hlsl turns into
    // each slot's initial (normal, offset): a slot's normal always points
    // OUTWARD (from inside that label's territory, through the boundary,
    // into open space), so two nodes on opposite sides of the SAME label's
    // boundary get honestly aligned normals, not artificially anti-parallel
    // ones.
    float4 dists;
    uint4  seeds;  // slot 0..3's PackSeed()-encoded nearest boundary voxel
};
