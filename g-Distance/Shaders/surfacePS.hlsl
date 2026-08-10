#include "DistanceFrameCb.hlsli"
#include "LabelPalette.hlsli"

struct VsOut {
    float4 pos      : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    nointerpolation uint labelI : TEXCOORD2;
    nointerpolation uint labelJ : TEXCOORD3;
};

// Shaded by the interface-plane normal (grad(phi_i)-grad(phi_j), normalized
// here for shading only -- the optimization itself never normalizes it) and
// colored by whichever of the tet's two active labels is actually being hit
// -- i.e. the FAR side from the camera, the segment the surface encloses,
// not the near side the camera itself sits in. g=phi_i-phi_j increases
// moving deeper into label i's territory, so the normal points toward the i
// side; the camera sits on that same side exactly when dot(n,cameraDir)>0,
// meaning the visible (hit) segment is then j, not i (e.g. looking at a
// solid's outer surface from outside/background, n points outward toward
// the camera's own background side, and what's actually rendered is the
// solid's own material on the far side). nFacing (for lighting) still uses
// whichever normal orientation points toward the camera, independent of
// per-tet winding (no back-face culling).
float4 surfacePS(VsOut input) : SV_Target
{
    float3 n = normalize(input.normal);
    float3 toCam = normalize(cameraPos.xyz - input.worldPos);
    bool towardI = dot(n, toCam) > 0.0;
    float3 nFacing = towardI ? n : -n;
    uint label = towardI ? input.labelJ : input.labelI;

    float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
    float diff = saturate(dot(nFacing, lightDir)) * 0.7 + 0.3;
    float3 baseColor = LabelColorA(label);
    return float4(baseColor * diff, 1.0);
}
