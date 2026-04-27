struct GsosExtrude {
    float4 pos    : SV_Position;
    float2 tex    : TEXCOORD;
    float2 weight : WEIGHT;
};

float4 extrudeCubicPS(GsosExtrude input) : SV_Target0
{
    float a = saturate(input.weight.x);
    return float4(0.05, 0.02, 0.0, a);
}
