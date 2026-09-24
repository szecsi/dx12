#include "DistanceFrameCb.hlsli"

// Full-screen-triangle VS for the edge-data-driven lattice raymarch pass
// (raymarchEdgePS.hlsl) -- body identical to raymarchLatticeVS.hlsl's own
// (this codebase's standard "no vertex buffer" full-screen triangle trick),
// its own root signature since the PS's UAV set differs: NodeEdgeData,
// NodeCandidateLabel, NodePotential, NodeEdgeDerivMult (the per-node
// edge-derivative multiplier buffer, see the approved plan's per-face
// redesign -- TestShape_ClippedSpheres only, reads back as 1.0 everywhere
// else).
#define RaymarchEdgeSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b1)"

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

[RootSignature(RaymarchEdgeSig)]
VsOut raymarchEdgeVS(uint vid : SV_VertexID)
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
