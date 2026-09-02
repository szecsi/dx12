#define DISTANCE_GRID_CB_REGISTER b0
#include "DistanceLattice.hlsli"

// Scratch -> main copy for the alien-potential secondary pass (see
// smoothnessJacobiAlienCS.hlsl). Mirrors commitSyntheticCS.hlsl's per-tile
// target enumeration (same dispatch grid, BlockSmoothingTilesPerAxis^3).
// Beta (one float per node) always changes here; the discriminator's
// scratch bits (8-15, written by smoothnessJacobiAlienCS.hlsl whenever a
// node's routing is (re)voted or a B-node swaps -- see PackDiscriminatorScratch)
// get copied down into the current bits (0-7) too, same pattern as
// commitSyntheticCS.hlsl's label byte1->byte0 copy -- a no-op on any sweep
// that didn't change this node's routing, since the scratch bits already
// equal the current ones then.
#define CommitAlienSig "RootFlags(0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "CBV(b0)"

RWStructuredBuffer<float> NodeAlienPotentialScratch : register(u0);
RWStructuredBuffer<float> NodeAlienPotential        : register(u1);
RWStructuredBuffer<uint>  NodeDiscriminator          : register(u2);

[RootSignature(CommitAlienSig)]
[numthreads(16, 1, 1)]
void commitAlienCS(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex)
{
    uint iTarget = tid;
    uint3 haloOriginA = gid * 2;
    uint local = iTarget & 7u;
    uint3 inTilePos = uint3(local & 1u, (local >> 1u) & 1u, (local >> 2u) & 1u);
    bool isB = iTarget >= 8u;
    uint3 pos = haloOriginA + inTilePos + 1u;
    uint targetIdx = isB ? BIdx(pos.x, pos.y, pos.z) : AIdx(pos.x, pos.y, pos.z);

    NodeAlienPotential[targetIdx] = NodeAlienPotentialScratch[targetIdx];

    uint word = NodeDiscriminator[targetIdx];
    uint scratchByte = (word >> 8u) & 0xFFu;
    NodeDiscriminator[targetIdx] = (word & 0xFFFFFF00u) | scratchByte;
}
