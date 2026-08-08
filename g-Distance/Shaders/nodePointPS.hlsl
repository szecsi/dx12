struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    nointerpolation uint label : TEXCOORD1;
    nointerpolation uint isB   : TEXCOORD2;
    nointerpolation float fade : TEXCOORD3;
};

// A-nodes (ground truth, should never flip) use the same red/green palette
// g-BCC/g-Aequor use; B-nodes (solved, no ground truth) use a visually
// distinct yellow/blue palette -- so an A-node showing the wrong color, or a
// B-node's color disagreeing with its A-node neighbors, stands out at a
// glance instead of blending into the same red/green field.
float3 LabelColorA(uint label)
{
    if (label == 0) return float3(0.90, 0.30, 0.30); // red
    if (label == 1) return float3(0.30, 0.85, 0.35); // green
    if (label == 2) return float3(0.30, 0.55, 0.95); // blue
    if (label == 3) return float3(0.95, 0.85, 0.25); // yellow
    return float3(0.6, 0.6, 0.6);
}

float3 LabelColorB(uint label)
{
    if (label == 0) return float3(0.95, 0.85, 0.25); // yellow
    if (label == 1) return float3(0.30, 0.55, 0.95); // blue
    if (label == 2) return float3(0.90, 0.30, 0.30); // red
    if (label == 3) return float3(0.30, 0.85, 0.35); // green
    return float3(0.6, 0.6, 0.6);
}

float4 nodePointPS(VsOut input) : SV_Target
{
    float r2 = dot(input.uv, input.uv);
    float aa = fwidth(r2);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, r2);
    if (alpha <= 0.001) discard;

    float z = sqrt(saturate(1.0 - r2));
    float3 n = float3(input.uv, z);
    float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
    float diff = saturate(dot(n, lightDir)) * 0.7 + 0.3;
    float3 baseColor = (input.isB != 0) ? LabelColorB(input.label) : LabelColorA(input.label);
    float3 color = baseColor * diff;
    return float4(color, alpha * input.fade);
}
