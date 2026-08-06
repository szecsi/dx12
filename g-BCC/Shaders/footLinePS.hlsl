struct VsOut {
    float4 pos  : SV_POSITION;
    float  side : TEXCOORD0;
};

float4 footLinePS(VsOut input) : SV_Target
{
    float aa    = fwidth(input.side);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, abs(input.side));
    if (alpha <= 0.001) discard;
    return float4(1.0, 0.35, 0.85, alpha);
}
