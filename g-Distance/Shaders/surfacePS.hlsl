struct VsOut {
    float4 pos    : SV_POSITION;
    float3 normal : TEXCOORD0;
};

// Shaded purely by the interface-plane normal (grad(phi_i)-grad(phi_j),
// normalized here for shading only -- the optimization itself never
// normalizes it). Rasterizer has no back-face culling and per-tet winding
// isn't enforced, so both faces are lit (max of front/back diffuse) rather
// than relying on winding to pick the "outward" side.
float4 surfacePS(VsOut input) : SV_Target
{
    float3 n = normalize(input.normal);
    float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
    float diffFront = saturate(dot(n, lightDir)) * 0.7 + 0.3;
    float diffBack  = saturate(dot(-n, lightDir)) * 0.7 + 0.3;
    float shade = max(diffFront, diffBack);
    float3 baseColor = float3(0.85, 0.82, 0.75);
    return float4(baseColor * shade, 1.0);
}
