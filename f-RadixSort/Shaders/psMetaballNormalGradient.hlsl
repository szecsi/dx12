#include "metaball.hlsli"

float4 psMetaballNormalGradient(VsosQuad input) : SV_Target
{
	NormalMetaballVisualizer normalMetaballVisualizer;
	
	return CalculateColor_Gradient(input.rayDir, normalMetaballVisualizer);
}



