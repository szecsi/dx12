struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// Tapered ink-coverage pen-stroke silhouette. strokeMask is a SINGLE-CHANNEL
// (R8_UNORM) target, so the coverage value is written directly into .r and
// accumulated via additive blending (PSO: SrcBlend=ONE, DestBlend=ONE) --
// NOT alpha-over compositing, which would need an alpha channel this target
// doesn't have (an .rgb=0/.a=coverage output with SrcAlpha/InvSrcAlpha
// blending silently stores nothing here, since only .r survives format
// conversion and .r was always 0).
float4 strokePS(VsOut i) : SV_TARGET
{
    float alongFade = smoothstep(1.0, 0.6, abs(i.uv.x));
    float acrossFade = smoothstep(1.0, 0.85, abs(i.uv.y));
    float coverage = alongFade * acrossFade;
    return float4(coverage, 0, 0, 0);
}
