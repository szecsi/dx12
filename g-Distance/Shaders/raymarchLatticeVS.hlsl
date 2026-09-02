#include "DistanceFrameCb.hlsli"

// Full-screen-triangle VS for the junction-aware lattice raymarch pass
// (raymarchLatticePS.hlsl) -- body identical to raymarchVS.hlsl/
// footSliceVS.hlsl (this codebase's standard "no vertex buffer" full-screen
// triangle trick), but with its own root signature since the PS needs UAV
// access to NodeCandidateLabel/NodePotential/NodeAlienPotential/
// NodeDiscriminator plus DistanceGridCb/DistanceCb, which RaymarchSig
// doesn't declare -- this codebase's convention is a dedicated VS/root-
// signature per full-screen pass, not sharing one across PSOs (see
// footSliceVS.hlsl declaring its own FootSliceSig for the same reason).
// u2/u3 and the trailing DistanceCb CBV (b2) are the alien-potential
// secondary pass's read side, see the approved plan -- CornerR degenerates
// to byte-identical output to before this addition whenever
// DistanceCb.UseAlienPotential<=0.5 (the default).
#define RaymarchLatticeSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b1)," \
    "CBV(b2)"

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

[RootSignature(RaymarchLatticeSig)]
VsOut raymarchLatticeVS(uint vid : SV_VertexID)
{
    VsOut o;
    float2 uv  = float2((vid << 1) & 2, vid & 2);
    float2 ndc = uv * 2.0f - 1.0f;
    o.pos = float4(ndc, 0, 1);

    float4 dir = mul(float4(ndc, 1, 1), rayDirTransform);
    dir /= dir.w;
    o.rayDir = dir.xyz;
    return o;
}
