#include "Billboard.hlsli"

[RootSignature(BillboardRootSig)]
VSOutput vsBillboard(IAOutput input)
{
	return input;
}
