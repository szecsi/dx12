#include "RootSignatures.hlsli"
#include "ShadowMap.hlsli"

[RootSignature(RootSig4)]
float4 main(GSOutput input) : SV_Target
{
	return length(lightPos.xyz - input.worldPosition.xyz / input.worldPosition.w).xxxx;
}
