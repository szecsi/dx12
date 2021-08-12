#include "metaballHashSimple.hlsli"

float4 psMetaballHashSimpleGradient(VsosQuad input) : SV_Target
{
	HashSimpleMetaballVisualizer hashMetaballVisualizer;
	return CalculateColor_Gradient(input.rayDir, input.pos, hashMetaballVisualizer);
}



