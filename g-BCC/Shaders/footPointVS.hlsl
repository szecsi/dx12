#include "BccFrameCb.hlsli"
#include "FootVizCb.hlsli"
#include "FootVector.hlsli"

#define FootPointSig "RootFlags(0)," \
    "CBV(b0)," \
    "CBV(b1)," \
    "SRV(t0)"

StructuredBuffer<FootVectorEntry> gEntries : register(t0);

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    nointerpolation float label : TEXCOORD1; // this footpoint's own ("from") label, see footVectorBuildCS.hlsl / footPointPS.hlsl
};

static const float2 QuadCorners[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };

// A billboard quad around the footpoint (e.end -- the interface point a
// footvector points to), shaded as a fake sphere impostor in footPointPS.hlsl
// rather than real sphere geometry: much cheaper, and at this on-screen size
// indistinguishable from an actual mesh. Same degenerate-collapse culling
// trick as footLineVS.hlsl for entries the shared filter rejects.
[RootSignature(FootPointSig)]
VsOut footPointVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;
    FootVectorEntry e = gEntries[iid];
    o.label = e.label;

    if (!FootVectorVisible(e, boxMin, boxMax)) {
        o.pos = float4(0, 0, 0, 0);
        o.uv = float2(0, 0);
        return o;
    }

    float4 clipCenter = mul(float4(e.end, 1), viewProjTransform);
    float2 corner     = QuadCorners[vid];
    float  radiusPx   = viewport.z;
    float2 offsetNdc  = corner * radiusPx * (2.0 / viewport.xy);

    o.pos = clipCenter;
    o.pos.xy += offsetNdc * clipCenter.w;
    o.uv = corner;
    return o;
}
