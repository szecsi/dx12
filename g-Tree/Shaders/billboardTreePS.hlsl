#include "billboardTree.hlsli"

Texture2D atlasTex : register(t1);
SamplerState samp : register(s0);

[RootSignature(BillboardTreeRootSig)]
float4 billboardTreePS(VSOutput input) : SV_Target {
	// alpha drives the blend state's alpha-to-coverage dithering; no manual discard/blend here.
	return atlasTex.Sample(samp, input.texCoord);
}
