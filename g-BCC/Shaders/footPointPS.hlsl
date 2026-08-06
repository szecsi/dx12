struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    nointerpolation float label : TEXCOORD1;
};

// Small fixed categorical palette for a footpoint's own ("from") label --
// distinct, saturated hues so which label a footpoint originated from is
// visually obvious at a glance (e.g. to see whether one label's footpoints
// are behaving differently from another's during relaxation). Falls back to
// gray for anything outside the expected 0..3 range.
float3 LabelColor(uint label)
{
    if (label == 0) return float3(0.90, 0.30, 0.30); // red
    if (label == 1) return float3(0.30, 0.85, 0.35); // green
    if (label == 2) return float3(0.30, 0.55, 0.95); // blue
    if (label == 3) return float3(0.95, 0.85, 0.25); // yellow
    return float3(0.6, 0.6, 0.6);
}

float4 footPointPS(VsOut input) : SV_Target
{
    float r2    = dot(input.uv, input.uv);
    float aa    = fwidth(r2);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, r2);
    if (alpha <= 0.001) discard;

    // Fake-sphere impostor: treat uv as the xy of a unit hemisphere normal
    // and light it, rather than drawing real sphere geometry.
    float  z    = sqrt(saturate(1.0 - r2));
    float3 n    = float3(input.uv, z);
    float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
    float  diff = saturate(dot(n, lightDir)) * 0.7 + 0.3;
    float3 baseColor = LabelColor((uint)round(input.label));
    float3 color = baseColor * diff;
    return float4(color, alpha);
}
