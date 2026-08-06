#pragma once

// One line segment of the quadric-patch wireframe overlay (quadricPatchBuildCS.hlsl
// -> quadricPatchLineVS.hlsl/PS.hlsl). len < 0 marks a degenerate/out-of-radius/
// unselected segment -- the VS collapses it to nothing instead of drawing it.
struct PatchSegmentEntry {
    float3 start;
    float3 end;
    float  len;
    float  _pad;
};
