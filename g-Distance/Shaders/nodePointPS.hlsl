#include "LabelPalette.hlsli"

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    nointerpolation uint label : TEXCOORD1;
    nointerpolation uint isB   : TEXCOORD2;
    nointerpolation float fade : TEXCOORD3;
};

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
