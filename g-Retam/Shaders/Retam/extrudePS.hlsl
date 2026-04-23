#include "Retam.hlsli"

Texture2D strokeTexture : register(t0);
SamplerState strokeSampler : register(s0);

struct GsosExtrude
{
	float4 pos			: SV_Position;
	float2 tex			: TEXCOORD;
	float2 weight		: WEIGHT;
};


float4 extrudePS(
	GsosExtrude input
	) : SV_Target
{
//	float2 sc;
//	sincos(input.weight.y, sc.y, sc.x);
//	return float4(sc, 0, 1);
//	return input.tex.xyyy;
	return 
		lerp(
			float4(1, 1, 1, 1),
			float4(strokeTexture.SampleGrad(strokeSampler, input.tex.yx, float2(0.1, 0), float2(0, 0) ).xyz, 1),
			input.weight.x);
//	float4 c = strokeTexture.SampleLevel(strokeSampler, strokeTexPos.yx + float2(0.5,0.5), 0 );
//	return float4(input.tex.x, 0, 1-input.tex.x, 1);
}

