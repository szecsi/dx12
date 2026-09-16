// No [RootSignature(...)] here -- declared once by footVectorLineVS.hlsl,
// this codebase's convention (see e.g. footSlicePS.hlsl's own comment).
struct VsOut {
    float4 pos  : SV_POSITION;
    float  fade : TEXCOORD0;
};

// Fixed accent color, distinct from LabelPalette.hlsli's per-label palette --
// reads clearly as a debug overlay, not scene geometry. Alpha fades from
// transparent at the node end to solid approaching the junction foot (the
// destination the segment is pointing at), via the VS's `fade` varying (0 at
// the node end, 1 at the foot end).
static const float3 JunctionLineColor = float3(1.0, 0.35, 0.75);

float4 footVectorLinePS(VsOut input) : SV_Target
{
    float alpha = lerp(0.3, 1.0, input.fade);
    return float4(JunctionLineColor, alpha);
}
