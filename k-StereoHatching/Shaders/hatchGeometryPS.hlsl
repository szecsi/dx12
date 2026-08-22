#include "EyeFrameCb.hlsli"

struct VsOut {
    float4 pos                    : SV_POSITION;
    float3 worldNormal            : TEXCOORD0;
    float3 worldPos               : TEXCOORD1;
    float2 screenDir               : TEXCOORD2;
    float  curvaturePx            : TEXCOORD4;
    nointerpolation uint colorIdx : TEXCOORD3;
};

struct PsOut {
    float4 sceneColor  : SV_TARGET0;
    float  luminance   : SV_TARGET1;
    // rg = screen-space unit hatch direction, b = screen-space signed
    // curvature (1/px) of that direction, a unused.
    float4 directionMap: SV_TARGET2;
};

static const float3 CharacterColors[3] = {
    float3(0.93, 0.80, 0.72), // bunny -- warm cream
    float3(0.78, 0.70, 0.90), // blob-cat -- soft lavender
    float3(0.98, 0.86, 0.35), // chick -- warm yellow
};

// Directional-light shading: ambient + Lambertian diffuse (kd) + Blinn-Phong
// specular (ks, exponent), all GUI-tunable via brdfParams -- this pass's
// output.luminance is the target luminance image Pass B's seed-density
// filtering matches stroke density against (darker = more hatching).
PsOut hatchGeometryPS(VsOut i)
{
    PsOut o;

    float3 n = normalize(i.worldNormal);
    float3 l = normalize(lightDirWorld.xyz);
    float3 v = normalize(cameraPosWorld.xyz - i.worldPos);
    float3 h = normalize(l + v);

    float3 albedo = CharacterColors[min(i.colorIdx, 2u)];
    float ambient = lightColor.w;
    float kd = brdfParams.x;
    float ks = brdfParams.y;
    float specExponent = brdfParams.z;

    float ndotl = max(dot(n, l), 0.0);
    float ndoth = max(dot(n, h), 0.0);

    float3 ambientTerm  = ambient * albedo;
    float3 diffuseTerm  = kd * ndotl * albedo * lightColor.rgb;
    float3 specularTerm = ks * pow(ndoth, specExponent) * lightColor.rgb;

    float3 lit = ambientTerm + diffuseTerm + specularTerm;

    o.sceneColor = float4(lit, 1.0);
    o.luminance = dot(lit, float3(0.2126, 0.7152, 0.0722));

    float2 dir = i.screenDir;
    float len = length(dir);
    float2 unitDir = (len > 1e-5) ? (dir / len) : float2(1, 0);
    o.directionMap = float4(unitDir, i.curvaturePx, 0.0);

    return o;
}
