#include "bccCommon.hlsli"
#include "LabelPotential.hlsli"

#define LabelPotentialInitSig "RootFlags(0)," \
    "DescriptorTable(UAV(u0, numDescriptors=6))"

RWTexture3D<uint4> gB0 : register(u2);
RWTexture3D<uint4> gB1 : register(u3);

// Seeds B's per-label potentials from B0's current JFA-format texel --
// see labelPotentialDiffuseCS.hlsl for the iterative relaxation that follows,
// and LabelPotential.hlsli for TexelToPotentials/PackPotentials4. Writes into
// B1, which JFA no longer needs once converged (its own ping-pong is done and
// finalized into A0/B0) -- repurposed here as filter scratch, no new
// resource needed. A0 is read-only from here on for the whole filter pass.
[RootSignature(LabelPotentialInitSig)]
[numthreads(4, 4, 4)]
void labelPotentialInitCS(uint3 tid : SV_DispatchThreadID)
{
    if (any(tid >= GridRes)) return;

    float4 phi = TexelToPotentials(gB0[tid]);
    gB1[tid] = uint4(PackPotentials4(phi), 0, 0, 0);
}
