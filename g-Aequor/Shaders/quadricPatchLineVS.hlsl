#include "AequorFrameCb.hlsli"
#include "PatchSegment.hlsli"

// PatchSegments bound as UAV, not SRV -- same "stays permanently
// UNORDERED_ACCESS, no transition dance" convention as particlePointVS.hlsl.
#define QuadricPatchLineSig "RootFlags(0)," \
    "CBV(b0)," \
    "UAV(u0)"

RWStructuredBuffer<PatchSegmentEntry> gSegments : register(u0);

struct VsOut {
    float4 pos  : SV_POSITION;
    float  side : TEXCOORD0;
};

// Wireframe rendering of the quadric each selected particle's (normal,
// curvature tensor) defines -- ported from g-BCC's quadricPatchLineVS.hlsl.
// Same screen-space quad-expanded antialiased line technique as
// particlePointVS.hlsl's billboards; validity is already baked into
// gSegments (len<0 marks a degenerate/unselected segment).
[RootSignature(QuadricPatchLineSig)]
VsOut quadricPatchLineVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;
    PatchSegmentEntry e = gSegments[iid];

    if (e.len < 0.0) {
        o.pos = float4(0, 0, 0, 0);
        o.side = 0;
        return o;
    }

    float4 clipStart = mul(float4(e.start, 1), viewProjTransform);
    float4 clipEnd   = mul(float4(e.end,   1), viewProjTransform);

    float2 ndcStart  = clipStart.xy / clipStart.w;
    float2 ndcEnd    = clipEnd.xy   / clipEnd.w;
    float2 dirPixels = (ndcEnd - ndcStart) * pointParams.xy * 0.5;
    float  dirLen    = length(dirPixels);
    float2 perpPixels = (dirLen > 1.0e-5) ? float2(-dirPixels.y, dirPixels.x) / dirLen : float2(1, 0);

    bool  isEnd       = vid >= 2;
    float side        = (vid & 1) ? 1.0 : -1.0;
    float halfWidthPx = 1.0; // thin wireframe line

    float4 basePos   = isEnd ? clipEnd : clipStart;
    float2 offsetNdc = perpPixels * side * halfWidthPx * (2.0 / pointParams.xy);

    o.pos = basePos;
    o.pos.xy += offsetNdc * basePos.w;
    o.side = side;
    return o;
}
