#include "BccFrameCb.hlsli"
#include "FootVizCb.hlsli"
#include "FootVector.hlsli"

#define QuadricPatchLineSig "RootFlags(0)," \
    "CBV(b0)," \
    "CBV(b1)," \
    "SRV(t0)"

StructuredBuffer<FootVectorEntry> gSegments : register(t0);

struct VsOut {
    float4 pos  : SV_POSITION;
    float  side : TEXCOORD0;
};

// Wireframe rendering of the full quadric each selected node's footvector +
// curvature tensor define (see quadricPatchBuildCS.hlsl for how segments are
// sampled via ray-quadric intersection). Same screen-space quad-expanded
// antialiased line technique as footLineVS.hlsl, reading a separate segments
// buffer this pass's own build shader fills in -- validity is already baked
// in there (e.len<0 marks a degenerate/out-of-radius/unselected segment), so
// there's no shared footvector-overlay filter call here; this is an
// independent overlay with its own stride/offset/radius controls.
[RootSignature(QuadricPatchLineSig)]
VsOut quadricPatchLineVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;
    FootVectorEntry e = gSegments[iid];

    if (e.len < 0.0) {
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
    float halfWidthPx = 1.0; // thin wireframe line, deliberately not tied to FootVizCb's lineHalfWidthPx

    float4 basePos   = isEnd ? clipEnd : clipStart;
    float2 offsetNdc = perpPixels * side * halfWidthPx * (2.0 / viewport.xy);

    o.pos = basePos;
    o.pos.xy += offsetNdc * basePos.w;
    o.side = side;
    return o;
}
