#include "DistanceFrameCb.hlsli"
#define DISTANCE_GRID_CB_REGISTER b1
#include "DistanceLattice.hlsli"

// One line segment per node with a valid junction footvector (see
// extractJunctionFootVectorsCS.hlsl / smoothJunctionFootVectorsCS.hlsl,
// DistanceApp.h picks which of the two buffers to bind here) -- endpoint 0
// is the node itself, endpoint 1 is the nearest point on the nearest
// junction line. An invalid node (w==0) collapses both vertices to the same
// position -- a zero-length, invisible segment, the same "inactive slot"
// convention used elsewhere in this project (wireframeVS's Valid==0 case,
// nodePointVS's hidden-node case) rather than a separate compaction pass for
// what's a sparse narrow-band instance set.
//
// `fade` is an ordinary (non-nointerpolation) varying -- exactly
// nodePointVS.hlsl's own mechanism -- 0 at the node end, 1 at the foot end,
// so the rasterizer's own linear interpolation across LINELIST gives the
// requested color/alpha gradient along the segment for free.
#define FootVectorLineSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)," \
    "CBV(b1)"

RWStructuredBuffer<float4> NodeJunctionFootVector : register(u0);

struct VsOut {
    float4 pos  : SV_POSITION;
    float  fade : TEXCOORD0;
};

[RootSignature(FootVectorLineSig)]
VsOut footVectorLineVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;
    uint node = iid;
    float4 fv = NodeJunctionFootVector[node];

    if (fv.w < 0.5)
    {
        o.pos = float4(0, 0, 0, 0);
        o.fade = 0.0;
        return o;
    }

    float3 nodePos = NodeWorldPos(node);
    float3 footPos = nodePos + fv.xyz;
    float3 worldPos = (vid == 0) ? nodePos : footPos;

    o.pos = mul(float4(worldPos, 1), viewProjTransform);
    o.fade = (vid == 0) ? 0.0 : 1.0;
    return o;
}
