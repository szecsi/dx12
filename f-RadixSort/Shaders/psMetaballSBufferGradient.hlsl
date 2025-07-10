#include "metaballSBuffer.hlsli"

float4 psMetaballSBufferGradient(VsosQuad input) : SV_Target
{
	SBufferMetaballVisualizer sbufferMetaballVisualizer;

	return CalculateColor_Gradient(input.rayDir, sbufferMetaballVisualizer);
}



