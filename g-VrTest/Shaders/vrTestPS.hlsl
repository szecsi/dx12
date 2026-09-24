struct VsOut {
    float4 position : SV_POSITION;
    float3 color    : COLOR;
};

float4 vrTestPS(VsOut i) : SV_TARGET
{
    return float4(i.color, 1.0);
}
