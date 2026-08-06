struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    nointerpolation uint label : TEXCOORD1;
};

// Small fixed categorical palette by label -- same palette g-BCC's
// footPointPS.hlsl uses, so results are visually comparable across projects.
float3 LabelColor(uint label)
{
    if (label == 0) return float3(0.90, 0.30, 0.30); // red
    if (label == 1) return float3(0.30, 0.85, 0.35); // green
    if (label == 2) return float3(0.30, 0.55, 0.95); // blue
    if (label == 3) return float3(0.95, 0.85, 0.25); // yellow
    return float3(0.6, 0.6, 0.6);
}

float4 particlePointPS(VsOut input) : SV_Target
{
    float r2    = dot(input.uv, input.uv);
    float aa    = fwidth(r2);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, r2);
    if (alpha <= 0.001) discard;

    float  z    = sqrt(saturate(1.0 - r2));
    float3 n    = float3(input.uv, z);
    float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
    float  diff = saturate(dot(n, lightDir)) * 0.7 + 0.3;
    float3 baseColor = LabelColor(input.label);
    float3 color = baseColor * diff;
    return float4(color, alpha);
}
