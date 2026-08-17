#pragma once

// Shared label->color palette. LabelColorA is the canonical per-segment
// color -- used for A-node points and for the extracted surface (surfacePS
// .hlsl), since a surface's li/lj are true segment labels, not node-type-
// specific. LabelColorB is a second, visually distinct palette reserved for
// B-node points (nodePointPS.hlsl) so a B-node's color disagreeing with its
// A-node neighbors stands out at a glance instead of blending into the same
// red/green field.
// Distinct, deterministic hues for labels past the four hand-picked ones,
// which the ported multi-label Marschner-Lobb scene reaches easily (up to 16
// Voronoi materials, see SyntheticScenes.hlsli) -- one flat gray for all of
// them would make every interior material junction invisible. Golden-ratio hue
// rotation, so any prefix of the sequence is well spread rather than only the
// full set. Same construction the Vulkan renderer colors that scene with
// (voxel_data_component.cpp's syntheticMaterialColor).
float3 GeneratedLabelColor(uint label)
{
    float h = frac((float)label * 0.61803399) * 6.0;
    float x = 1.0 - abs(fmod(h, 2.0) - 1.0);
    float3 rgb = h < 1 ? float3(1, x, 0) : h < 2 ? float3(x, 1, 0) : h < 3 ? float3(0, 1, x) :
                 h < 4 ? float3(0, x, 1) : h < 5 ? float3(x, 0, 1) : float3(1, 0, x);
    return rgb * 0.85 + 0.1;
}

float3 LabelColorA(uint label)
{
    if (label == 0) return float3(0.90, 0.30, 0.30); // red
    if (label == 1) return float3(0.30, 0.85, 0.35); // green
    if (label == 2) return float3(0.30, 0.55, 0.95); // blue
    if (label == 3) return float3(0.95, 0.85, 0.25); // yellow
    return GeneratedLabelColor(label);
}

float3 LabelColorB(uint label)
{
    if (label == 0) return float3(0.95, 0.85, 0.25); // yellow
    if (label == 1) return float3(0.30, 0.55, 0.95); // blue
    if (label == 2) return float3(0.90, 0.30, 0.30); // red
    if (label == 3) return float3(0.30, 0.85, 0.35); // green
    // Offset so a B-node still reads as disagreeing with an A-node carrying the
    // same label, the same way the four hand-picked rows above are permuted.
    return GeneratedLabelColor(label + 8u);
}
