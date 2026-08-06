#pragma once

// Shared root signature and per-frame data for far-away tree billboards.
// b0 is bound to the engine's existing (already-updated-every-frame) PerFrameCb;
// only the leading fields we actually need are declared here, at their real byte offsets.

#define BillboardTreeRootSig \
	"CBV(b0)," \
	"DescriptorTable(SRV(t0, numDescriptors=2))," \
	"StaticSampler(s0, filter=FILTER_MIN_MAG_MIP_LINEAR, addressU=TEXTURE_ADDRESS_CLAMP, addressV=TEXTURE_ADDRESS_CLAMP, addressW=TEXTURE_ADDRESS_CLAMP)"

cbuffer PerFrameCb : register(b0) {
	float4x4 viewProjMat     : packoffset(c0);
	float4x4 rayDirTransform : packoffset(c4);
	float4   cameraPos       : packoffset(c8);
}

struct VSOutput {
	float4 position : SV_Position;
	float2 texCoord  : TEXCOORD;
};
