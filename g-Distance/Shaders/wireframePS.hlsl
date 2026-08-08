struct VsOut {
    float4 pos : SV_POSITION;
};

// Flat, unlit, bright -- purely a debug marker, not meant to blend in.
float4 wireframePS(VsOut input) : SV_Target
{
    return float4(1.0, 1.0, 0.15, 1.0);
}
