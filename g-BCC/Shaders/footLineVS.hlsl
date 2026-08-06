#include "BccFrameCb.hlsli"
#include "FootVizCb.hlsli"
#include "FootVector.hlsli"

#define FootLineSig "RootFlags(0)," \
    "CBV(b0)," \
    "CBV(b1)," \
    "SRV(t0)"

StructuredBuffer<FootVectorEntry> gEntries : register(t0);

struct VsOut {
    float4 pos  : SV_POSITION;
    float  side : TEXCOORD0;
};

// Each footvector becomes a camera-facing, constant-pixel-width quad (a
// 4-vertex triangle strip) rather than a raw line-list primitive --
// D3D12_PRIMITIVE_TOPOLOGY_LINELIST has no antialiasing without MSAA, which
// this app's backbuffer doesn't use. The quad's width is built in clip space
// from a screen-space (pixel-space) perpendicular so it stays a constant
// pixel width regardless of distance or viewport aspect ratio; footLinePS.hlsl
// then antialiases the two long edges with a distance-from-centerline
// falloff. Entries that fail the shared start/length filter collapse all 4
// corners to the same degenerate (0,0,0,0) clip position -- a zero-area
// primitive the rasterizer discards outright, so no branch is needed at draw
// time to skip them.
[RootSignature(FootLineSig)]
VsOut footLineVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;
    FootVectorEntry e = gEntries[iid];

    if (!FootVectorVisible(e, boxMin, boxMax)) {
        o.pos = float4(0, 0, 0, 0);
        o.side = 0;
        return o;
    }

    float4 clipStart = mul(float4(e.start, 1), viewProjTransform);
    float4 clipEnd   = mul(float4(e.end,   1), viewProjTransform);

    float2 ndcStart  = clipStart.xy / clipStart.w;
    float2 ndcEnd    = clipEnd.xy   / clipEnd.w;
    float2 dirPixels = (ndcEnd - ndcStart) * viewport.xy * 0.5;
    float  dirLen    = length(dirPixels);
    float2 perpPixels = (dirLen > 1.0e-5) ? float2(-dirPixels.y, dirPixels.x) / dirLen : float2(1, 0);

    bool  isEnd       = vid >= 2;
    float side        = (vid & 1) ? 1.0 : -1.0;
    float halfWidthPx = boxMax.w;

    float4 basePos   = isEnd ? clipEnd : clipStart;
    float2 offsetNdc = perpPixels * side * halfWidthPx * (2.0 / viewport.xy);

    o.pos = basePos;
    o.pos.xy += offsetNdc * basePos.w; // scale by w: xy gets divided by w again in the rasterizer's own perspective divide
    o.side = side;
    return o;
}
