#include "Retam.hlsli"
#include "RetamCb.hlsli"

cbuffer PerFrameCb : register(b1) {
    float4x4 viewProjMat : packoffset(c0);
    float4 cameraPos : packoffset(c8);
    float4 ahead : packoffset(c22);
}

struct VSOutput {
    float4 position : SV_Position;
    float4 texCoord : TEXCOORD;
    float4 worldNormal : NORMAL;
    float4 worldTangent : TANGENT;
    float4 worldPosition : WORLDPOS;
};

struct PSOutput {
    uint4 qid0 : SV_Target0;
    uint4 qid1 : SV_Target1;
    uint4 qid2 : SV_Target2;
    uint4 qid3 : SV_Target3;
};
    
[RootSignature(RootSigRetam)]
PSOutput qid256PS(VSOutput input) {
    uint4 quids = uint4(0, 0, 0, 0);
        
    float3 normal = normalize(input.worldNormal.xyz);
    float3 bitangent = cross(normal, input.worldTangent.xyz);
    float3 tangent = cross(bitangent, normal);

    float3 x = input.worldPosition.xyz / input.worldPosition.w;
    float3 viewDiff = cameraPos.xyz - x;
    float3 viewDir = normalize(viewDiff);
    float3 lightDir = normalize(float3(1, 1, 1));
    float tone = clamp(dot(normal, lightDir), 0.0, 1.0) * 4.2;

    float4 fragmentColor = float4(normal, 1);

    for (uint j = 4u; j > (uint) tone && j > 0u; j--){
        float rang = crossAngle[j - 1u] * 6.28;            
        float2 tex = input.texCoord.xy / input.texCoord.w / texScale[j - 1u] * 0.5;
        float3 h = tangent * cos(rang) + bitangent * sin(rang);
        float geom = length(cross(h, viewDir)) / dot(viewDiff, -ahead.xyz) * 10.0;
        float lod = -log2(geom) - 3.0;
        float ilod = floor(lod + 1000.0) - 1000.0;
        if(ilod < 0.5){
            quids[j] = 0;
        } else {
            float flod = frac(lod + 1000.0);
            float2 stex = tex / exp2(ilod);
            uint2 qid2d = uint2(stex);
            qid2d = (qid2d | (qid2d << 16u)) & 0x030000FFu;
            qid2d = (qid2d | (qid2d <<  8u)) & 0x0300F00Fu;
            qid2d = (qid2d | (qid2d <<  4u)) & 0x030C30C3u;
            qid2d = (qid2d | (qid2d <<  2u)) & 0x09249249u;
            quids[j] = (qid2d.x | (qid2d.y << 1u)) + (0x55555555u >> (32u - (uint(ilod) * 2u)));
        }
    }
    PSOutput output;
    output.qid0 = uint4(quids.x, quids.x+1, quids.x+2, quids.x+3);
    output.qid1 = uint4(quids.y, quids.y+1, quids.y+2, quids.y+3);
    output.qid2 = uint4(quids.z, quids.z+1, quids.z+2, quids.z+3);
    output.qid3 = uint4(quids.w, quids.w+1, quids.w+2, quids.w+3);
    return output;
}
