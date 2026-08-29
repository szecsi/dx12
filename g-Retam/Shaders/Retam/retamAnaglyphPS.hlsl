Texture2D<float4> LeftEyeColor  : register(t0);
Texture2D<float4> RightEyeColor : register(t1);

struct VsOut {
    float4 pos : SV_POSITION;
};

// Red/cyan (half-color) anaglyph combine: each eye's fully-shaded retam
// render (already cleared to white paper before its strokes were drawn, see
// RetamApp::RecordAndSubmitFinalDrawPass) is reduced to luminance, then the
// left eye's luminance drives the red channel and the right eye's drives
// green+blue (cyan) together -- a white background in both eyes therefore
// stays white in the composite. Both eye textures match this pass's
// resolution 1:1, so plain SV_POSITION-indexed Loads need no sampler.
float4 retamAnaglyphPS(VsOut i) : SV_TARGET
{
    int2 px = int2(i.pos.xy);

    float3 colorL = LeftEyeColor.Load(int3(px, 0)).rgb;
    float3 colorR = RightEyeColor.Load(int3(px, 0)).rgb;

    float lumL = dot(colorL, float3(0.299, 0.587, 0.114));
    float lumR = dot(colorR, float3(0.299, 0.587, 0.114));

    return float4(saturate(lumL), saturate(lumR), saturate(lumR), 1.0);
}
