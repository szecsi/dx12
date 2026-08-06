#pragma once

// One candidate footvector per BCC lattice point (both sublattices, built by
// footVectorBuildCS.hlsl): from the lattice point (`start`) to its nearest
// label-interface seed (`end`, i.e. the "foot" point on the interface). `len`
// is precomputed once here so both the line and footpoint draw passes can
// filter on it without recomputing distance() themselves; invalid entries
// (no interface found -- see SENTINEL_LABEL in bccCommon.hlsli) get
// len = 1.0e6, matching the "huge sentinel" convention raymarchPS.hlsl
// already uses for the same situation, so a single `len > maxLen` test
// rejects them too, without a separate validity flag. `label` is the node's
// own ("from") label -- see footVectorBuildCS.hlsl -- used by
// footPointPS.hlsl to color-code footpoints; quadricPatchBuildCS.hlsl's
// wireframe segments leave it at 0 (unused there).
struct FootVectorEntry {
    float3 start;
    float  len;
    float3 end;
    float  label;
};

// Shared start-point-bounding-box + max-length filter for both the line and
// footpoint passes -- `boxMinPacked`/`boxMaxPacked` are FootVizParamsCb's
// packed boxMin/boxMax fields (xyz = bound, boxMinPacked.w = maxLen).
bool FootVectorVisible(FootVectorEntry e, float4 boxMinPacked, float4 boxMaxPacked)
{
    float maxLen = boxMinPacked.w;
    if (e.len > maxLen) return false;
    if (any(e.start < boxMinPacked.xyz) || any(e.start > boxMaxPacked.xyz)) return false;
    return true;
}
