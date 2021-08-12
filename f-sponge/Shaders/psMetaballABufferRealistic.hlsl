#include "metaballABuffer.hlsli"

float4 psMetaballABufferRealistic(VsosQuad input) : SV_Target
{
	ABufferMetaballVisualizer abufferMetaballVisualizer;

	return CalculateColor_Realistic(input.rayDir, input.pos, abufferMetaballVisualizer);
}



