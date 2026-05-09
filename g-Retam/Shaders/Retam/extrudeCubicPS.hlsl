Texture2D    carrotTex : register(t1);
SamplerState sampl     : register(s0);

struct GsosExtrude {
    float4 pos    : SV_Position;
    float2 tex    : TEXCOORD;
    float2 weight : WEIGHT;
};

float4 extrudeCubicPS(GsosExtrude input) : SV_Target0
{
//     return float4(0, 0, input.weight.y, 1);
   
    float4 c = carrotTex.SampleLevel(sampl, input.tex, 0);
    c.a = (1.0 - c.b) * saturate(input.weight.x);
    c.a *= 0.2;
    return c;//float4(c.rgb, 1);//float4(0, 0, input.weight.y, c.a);
}
