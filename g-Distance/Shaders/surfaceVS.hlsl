#include "DistanceFrameCb.hlsli"
#include "DistanceSurface.hlsli"

// Non-indexed triangle-list draw over extractSurfaceCS.hlsl's fixed-stride
// (TetCount*6) vertex buffer -- degenerate (zero-area) entries simply don't
// rasterize, no separate index/count bookkeeping needed.
#define SurfaceSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)"

RWStructuredBuffer<SurfaceVertex> SurfaceVertices : register(u0);

struct VsOut {
    float4 pos      : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    nointerpolation uint labelI : TEXCOORD2;
    nointerpolation uint labelJ : TEXCOORD3;
};

[RootSignature(SurfaceSig)]
VsOut surfaceVS(uint vid : SV_VertexID)
{
    VsOut o;
    SurfaceVertex v = SurfaceVertices[vid];
    o.pos = mul(float4(v.pos, 1), viewProjTransform);
    o.worldPos = v.pos;
    o.normal = v.normal;
    o.labelI = v.labelI;
    o.labelJ = v.labelJ;
    return o;
}
