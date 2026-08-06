struct VsOut {
    float4 pos  : SV_POSITION;
    float  side : TEXCOORD0;
};

float4 quadricPatchLinePS(VsOut input) : SV_Target
{
    float aa    = fwidth(input.side);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, abs(input.side));
    if (alpha <= 0.001) discard;
    // Pale yellow -- distinct from the footvector overlay's magenta lines /
    // cyan-magenta footpoints. Color coding by curvature etc. left for later.
    return float4(0.95, 0.9, 0.35, alpha);
}
