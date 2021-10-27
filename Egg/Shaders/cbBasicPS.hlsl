#include "cbBasic.hlsli"

float4 main(VSOutput input) : SV_Target
{
	return float4(1, 1, /*input.texCoord,*/ 0, 1);
}

