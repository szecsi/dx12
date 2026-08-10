#pragma once

// Flat-shaded triangle vertex written by extractSurfaceCS.hlsl and read by
// surfaceVS.hlsl -- position + the tet's single interface-plane normal
// (same value repeated on every vertex of a tet's surface fragment, since
// the plane is exactly flat within a tet). labelI/labelJ are the tet's
// active interface pair (also constant per tet) -- surfacePS.hlsl picks
// whichever of the two currently faces the camera to color the segment
// that's actually being looked at.
struct SurfaceVertex {
    float3 pos;
    float3 normal;
    uint labelI;
    uint labelJ;
};
