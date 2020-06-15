#include "RootSignatures.hlsli"

struct VSOutput {
	float4 position : SV_Position;
    float3 rayDir : RAYDIR;
	float2 texCoord : TEXCOORD;
};

Texture2D txt : register(t0);
TextureCube env : register(t1);
SamplerState sampl : register(s0);

[RootSignature(RootSig4)]
float4 main(VSOutput vso) : SV_Target{
	//return txt.Sample(sampl, vso.texCoord);
	//return float4(normalize(vso.rayDir), 1);
	return env.Sample(sampl, vso.rayDir);
}
