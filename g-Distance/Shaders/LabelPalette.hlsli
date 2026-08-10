#pragma once

// Shared label->color palette. LabelColorA is the canonical per-segment
// color -- used for A-node points and for the extracted surface (surfacePS
// .hlsl), since a surface's li/lj are true segment labels, not node-type-
// specific. LabelColorB is a second, visually distinct palette reserved for
// B-node points (nodePointPS.hlsl) so a B-node's color disagreeing with its
// A-node neighbors stands out at a glance instead of blending into the same
// red/green field.
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
