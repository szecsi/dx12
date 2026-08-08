#pragma once

// Flat-shaded triangle vertex written by extractSurfaceCS.hlsl and read by
// surfaceVS.hlsl -- position + the tet's single interface-plane normal
// (same value repeated on every vertex of a tet's surface fragment, since
// the plane is exactly flat within a tet).
struct SurfaceVertex {
    float3 pos;
    float3 normal;
};
