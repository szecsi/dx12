#include "Billboard.hlsli"

Texture2D txt : register(t0);
SamplerState sampl : register(s0);

[RootSignature(BillboardRootSig)]
float4 main(GSOutput input) : SV_Target
{
	float4 color = txt.Sample(sampl, input.tex);

	color.rgb = float3(color.a,
		pow(color.a, 4),
		pow(color.a, 10));
	color.a *= input.opacity * 0.1;
	return color;
}
