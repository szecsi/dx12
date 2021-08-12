#include "metaballSBuffer.hlsli"

float4 psMetaballSBufferRealistic(VsosQuad input) : SV_Target
{
	SBufferMetaballVisualizer sbufferMetaballVisualizer;

	return CalculateColor_Realistic(input.rayDir, input.pos, sbufferMetaballVisualizer);
}



