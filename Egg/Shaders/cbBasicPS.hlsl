#include "cbBasic.hlsli"

float4 main(VSOutput input) : SV_Target
{
	return float4(input.texCoord, 0, 1);
}

