#include "Billboard.hlsli"

Texture2D particleTexture : register(t0);
SamplerState sampl : register(s0);


[RootSignature(BillboardRootSig)]
float4 main(GSOutput input) : SV_Target
{
   float4 color = particleTexture.Sample(sampl, input.tex.xy);

	color.rgb = float3(color.a,
		pow(color.a, 4),
		pow(color.a, 10));
	color.a *= input.opacity;

	return color;
}
