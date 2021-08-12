#include "metaballSBuffer.hlsli"

float4 psMetaballS2BufferGradient(VsosQuad input) : SV_Target
{
	SBufferV2MetaballVisualizer sbufferMetaballVisualizer;

	return CalculateColor_Gradient(input.rayDir, input.pos, sbufferMetaballVisualizer);
}



