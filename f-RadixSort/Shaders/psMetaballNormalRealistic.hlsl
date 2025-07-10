#include "metaball.hlsli"

float4 psMetaballNormalRealistic(VsosQuad input) : SV_Target
{	
	NormalMetaballVisualizer normalMetaballVisualizer;

	return CalculateColor_Realistic(input.rayDir, input.pos, normalMetaballVisualizer);
}



