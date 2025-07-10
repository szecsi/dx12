#include "metaballABuffer.hlsli"

float4 psMetaballABufferGradient(VsosQuad input) : SV_Target
{
	ABufferMetaballVisualizer abufferMetaballVisualizer;

	return CalculateColor_Gradient(input.rayDir, abufferMetaballVisualizer);
}



