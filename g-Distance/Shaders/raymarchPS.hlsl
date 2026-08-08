#include "DistanceFrameCb.hlsli"
#include "TorusSdf.hlsli"

// No [RootSignature(...)] here -- the root signature is declared once, by
// raymarchVS.hlsl, and reused for both stages of this draw.
struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

struct PsOut {
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

// The scene only ever has one active test shape at a time (BuildShapeList),
// so this sphere-traces the exact analytic SDF directly -- no lattice/JFA
// field reconstruction needed, this is purely a visual reference for the
// node point-cloud to converge onto.
float SceneSd(float3 p)
{
    float d = 1.0e30;
    for (uint i = 0; i < nTorii; i++)
        d = min(d, ShapeSd(p, torii[i]));
    return d;
}

float3 SceneNormal(float3 p)
{
    const float e = 0.001;
    float2 h = float2(e, -e);
    return normalize(
        h.xyy * SceneSd(p + h.xyy) +
        h.yyx * SceneSd(p + h.yyx) +
        h.yxy * SceneSd(p + h.yxy) +
        h.xxx * SceneSd(p + h.xxx));
}

PsOut raymarchPS(VsOut input)
{
    PsOut result;

    float3 ro = cameraPos.xyz;
    float3 rd = normalize(input.rayDir);

    float maxDist  = raymarchParams.y;
    int   maxSteps = (int)raymarchParams.x;

    float t = 0.0;
    bool hit = false;
    for (int i = 0; i < maxSteps; i++) {
        float3 p = ro + rd * t;
        float d = SceneSd(p);
        if (d < 0.002) { hit = true; break; }
        t += d;
        if (t > maxDist) break;
    }

    if (hit) {
        float3 p = ro + rd * t;
        float3 n = SceneNormal(p);

        float4 clipHit = mul(float4(p, 1), viewProjTransform);
        result.depth = clipHit.z / clipHit.w;

        float3 lightDir = normalize(float3(0.4, 0.6, 0.7));
        float diff = saturate(dot(n, lightDir)) * 0.7 + 0.3;
        float3 baseColor = float3(0.35, 0.38, 0.42);
        result.color = float4(baseColor * diff, 1.0);
        return result;
    }

    result.color = float4(0.05, 0.06, 0.08, 1.0);
    result.depth = 1.0;
    return result;
}
