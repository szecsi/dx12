#include "RootSignatures.hlsli"

struct VSOutput {
	float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD;
};

Texture2D txt : register(t0);
TextureCube env : register(t1);
SamplerState sampl : register(s0);

[RootSignature(RootSig4)]
float4 main(VSOutput vso) : SV_Target {
    return txt.Sample(sampl, vso.texCoord);
	//return env.Sample(sampl, reflect(-viewDir, normal);
}
