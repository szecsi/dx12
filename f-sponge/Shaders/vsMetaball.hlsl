#include "metaball.hlsli"

VsosQuad vsMetaball(IaosQuad input) {
	VsosQuad output = (VsosQuad)0;
	output.pos = input.pos;
	output.pos.z = 0.001f;
	float4 hWorldPosMinusEye = mul(output.pos, rayDirMatrix);
	hWorldPosMinusEye /= hWorldPosMinusEye.w;
	output.rayDir = hWorldPosMinusEye.xyz;
	output.tex = input.tex;
	return output;
}


