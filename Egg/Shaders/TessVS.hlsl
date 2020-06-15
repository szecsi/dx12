#include "Tess.hlsli"

[RootSignature(TessRootSig)]
VSOutput main(IAOutput input)
{
	return input;
}