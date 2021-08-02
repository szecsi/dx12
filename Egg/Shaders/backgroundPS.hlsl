#include "quad.hlsli"

TextureCube env : register(t0);
SamplerState sampl : register(s0);

[RootSignature(RootSig3)]
float4 main(VSOutput input) : SV_Target
{
	return
		env.Sample(sampl, input.rayDir);
}