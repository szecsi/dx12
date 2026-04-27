Buffer<float4> cubic : register(t0);

struct GsisDummy { };

struct GsosExtrude {
    float4 pos    : SV_Position;
    float2 tex    : TEXCOORD;
    float2 weight : WEIGHT;
};

[maxvertexcount(32)]
void extrudeCubicGS(point GsisDummy dummy[1], uint pid : SV_PrimitiveID, inout TriangleStream<GsosExtrude> stream)
{
    float4 cubicX = cubic[pid * 4 + 0];
    float4 cubicY = cubic[pid * 4 + 1];
    float4 cubicV = cubic[pid * 4 + 2];
    float4 meta   = cubic[pid * 4 + 3];  // x=1-tMin, y=tMax, w=confidence

    float confidence = meta.w;
    if (confidence == 0) return;

    float exX = meta.x;  // = 1 - tMin
    float exY = meta.y;  // = tMax

    float4 tMaxPow = float4(1, exY, exY*exY, exY*exY*exY);
    float4 tMinPow = float4(1, 1-exX, (1-exX)*(1-exX), (1-exX)*(1-exX)*(1-exX));
    float dx = dot(cubicX, tMaxPow) - dot(cubicX, tMinPow);
    float dy = dot(cubicY, tMaxPow) - dot(cubicY, tMinPow);
    if (dx*dx + dy*dy > 5000) return;

    confidence *= saturate((exY - (1 - exX)) * 10);
    float overdraw = 1.0 + ((pid * 0x2da78e85) >> 24) / 255.0 * 0.2;

    for (uint i = 0; i < 16; i++) {
        float t = 1 - exX + (exY - (1 - exX)) * (i / 15.0 * overdraw - (overdraw - 1.0) * 0.5);
        float4 tPowers = float4(1, t, t*t, t*t*t);

        GsosExtrude output;
        output.weight.x = confidence * dot(cubicV, tPowers);
        output.weight.y = ((pid * 0x2da78e85) >> 24) / 255.0;
        output.pos.xy = float2(dot(cubicX, tPowers), dot(cubicY, tPowers));
        output.pos.y += 5.15;

        float2 tangent = float2(dot(cubicX.yzw * float3(1, 2, 3), tPowers.xyz),
                                dot(cubicY.yzw * float3(1, 2, 3), tPowers.xyz));
        tangent = normalize(tangent);
        float2 normal = tangent.yx;

        output.pos.xy /= 512.0;
        output.pos.xy -= float2(1, 1) + normal * 0.005 + sin(i) * output.weight.y * 0.02;
        output.pos.y = -output.pos.y;
        output.pos.z = 0.5;
        output.pos.w = 1;

        output.tex = float2(0, i / 15.0);
        stream.Append(output);
        output.pos.xy += normal * 0.01;
        output.tex = float2(1, i / 15.0);
        stream.Append(output);
    }
}
