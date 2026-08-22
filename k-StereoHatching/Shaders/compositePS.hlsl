#include "CompositeCb.hlsli"

Texture2D<float> LeftStrokeMask  : register(t0);
Texture2D<float> RightStrokeMask : register(t1);
Texture2D<float> LeftLuminance   : register(t2);
Texture2D<float> RightLuminance  : register(t3);
Texture2D<float> LeftCrossMask   : register(t4);
Texture2D<float> RightCrossMask  : register(t5);

struct VsOut {
    float4 pos : SV_POSITION;
};

// Red/cyan anaglyph combine: left eye's ink darkens only the red channel,
// right eye's ink darkens green+blue (cyan) together. The base tone under
// the ink is normally flat paperColor; when inkTint.z (the "Show Luminance
// Map" checkbox) is set, it's instead that eye's own Phong-shaded luminance
// image (hatchGeometryPS.hlsl's target-luminance output), so the hatching
// can be visually checked against the shading it's supposed to approximate.
// Luminance's -1 background sentinel falls back to paperColor there too.
//
// Ink itself is either this eye's OWN hatching (LeftStrokeMask/
// RightStrokeMask, "unprojected", the Unsynced-mode default) and/or the
// OPPOSING eye's hatching reprojected onto this eye's own surface
// (LeftCrossMask/RightCrossMask, crossProjectPS.hlsl) drawn in THIS eye's
// own color -- e.g. a stroke seeded for the right eye can show up in the
// LEFT (red) channel. modeParams selects which; in Unsynced mode the cross
// textures were never rendered this frame (may hold stale data), so they're
// unconditionally excluded regardless of the checkboxes.
//
// All six textures match this pass's resolution 1:1, so plain
// SV_POSITION-indexed Loads need no sampler.
float4 compositePS(VsOut i) : SV_TARGET
{
    int2 px = int2(i.pos.xy);

    bool crossProjectedMode = modeParams.x > 0.5;
    bool showUnprojected    = modeParams.y > 0.5;
    bool showCrossProjected = modeParams.z > 0.5;

    float inkL = 0.0, inkR = 0.0;
    if (!crossProjectedMode || showUnprojected) {
        inkL += LeftStrokeMask.Load(int3(px, 0));
        inkR += RightStrokeMask.Load(int3(px, 0));
    }
    if (crossProjectedMode && showCrossProjected) {
        inkL += LeftCrossMask.Load(int3(px, 0));
        inkR += RightCrossMask.Load(int3(px, 0));
    }
    inkL = saturate(inkL);
    inkR = saturate(inkR);

    float baseL = paperColor.r;
    float baseR = paperColor.g;
    if (inkTint.z > 0.5) {
        float lumL = LeftLuminance.Load(int3(px, 0));
        float lumR = RightLuminance.Load(int3(px, 0));
        baseL = (lumL < 0.0) ? paperColor.r : saturate(lumL);
        baseR = (lumR < 0.0) ? paperColor.g : saturate(lumR);
    }

    float red  = saturate(baseL - inkL * inkTint.x);
    float cyan = saturate(baseR - inkR * inkTint.y);

    return float4(red, cyan, cyan, 1.0);
}
