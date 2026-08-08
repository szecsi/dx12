#include "DistanceFrameCb.hlsli"
#include "PickedTetCb.hlsli"

// Draws the 6 edges (12 vertices, LINELIST) of whichever single disphenoid
// tet was last picked via DistanceApp::PerformPick() -- a debug aid for
// inspecting exactly which tet a given point on the extracted surface came
// from. Collapses to a degenerate point when nothing is picked (Valid==0),
// same pattern as every other "inactive slot" case in this project.
#define WireframeSig "RootFlags(0)," \
    "CBV(b0)," \
    "CBV(b1)"

static const uint2 TetEdges[6] = {
    uint2(0, 1), uint2(0, 2), uint2(0, 3),
    uint2(1, 2), uint2(1, 3), uint2(2, 3),
};

struct VsOut {
    float4 pos : SV_POSITION;
};

[RootSignature(WireframeSig)]
VsOut wireframeVS(uint vid : SV_VertexID)
{
    VsOut o;
    if (Valid == 0) {
        o.pos = float4(0, 0, 0, 0);
        return o;
    }

    uint edgeIdx = vid / 2;
    uint endIdx = vid % 2;
    uint2 e = TetEdges[edgeIdx];
    uint cornerIdx = (endIdx == 0) ? e.x : e.y;

    float3 p;
    if (cornerIdx == 0) p = Corner0.xyz;
    else if (cornerIdx == 1) p = Corner1.xyz;
    else if (cornerIdx == 2) p = Corner2.xyz;
    else p = Corner3.xyz;

    o.pos = mul(float4(p, 1), viewProjTransform);
    return o;
}
