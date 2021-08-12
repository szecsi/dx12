#include "metaballHashSimple.hlsli"

float4 psMetaballHashSimpleRealistic(VsosQuad input) : SV_Target
{
	HashSimpleMetaballVisualizer hashMetaballVisualizer;
	return CalculateColor_Realistic(input.rayDir, input.pos, hashMetaballVisualizer);
}



