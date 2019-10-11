#include "RootSignatures.hlsli"

struct VSOutput {
	float4 position : SV_Position;
    float2 texCoord : TEXCOORD;
};

Texture2D txt : register(t0);
TextureCube env : register(t1);
SamplerState sampl : register(s0);

[RootSignature(RootSig4)]
float4 main(VSOutput vso) : SV_Target {
    return txt.Sample(sampl, vso.texCoord);
	//return env.Sample(sampl, float3(1, vso.texCoord.x,vso.texCoord.y));
}
