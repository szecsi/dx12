#include "billboard.hlsli"

cbuffer billboardGSTransCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

cbuffer billboardGSSizeCB {
	float4 billboardSize;
}

[maxvertexcount(4)]
void gsBillboard(point VsosBillboard input[1], inout TriangleStream<GsosBillboard> stream) {
	float4 hndcPos = mul(float4(input[0].pos, 1), modelViewProjMatrix);

	GsosBillboard output;
	output.pos = hndcPos;
	output.pos.x += billboardSize.x;
	output.pos.y += billboardSize.y;
	output.tex = float2(1, 0);
	output.id = input[0].id;
	output.rayDir = input[0].rayDir;
	stream.Append(output);

	output.pos = hndcPos;
	output.pos.x += billboardSize.x;
	output.pos.y -= billboardSize.y;
	output.tex = float2(1, 1);
	output.id = input[0].id;
	output.rayDir = input[0].rayDir;
	stream.Append(output);

	output.pos = hndcPos;
	output.pos.x -= billboardSize.x;
	output.pos.y += billboardSize.y;
	output.tex = float2(0, 0);
	output.id = input[0].id;
	output.rayDir = input[0].rayDir;
	stream.Append(output);

	output.pos = hndcPos;
	output.pos.x -= billboardSize.x;
	output.pos.y -= billboardSize.y;
	output.tex = float2(0, 1);
	output.id = input[0].id;
	output.rayDir = input[0].rayDir;
	stream.Append(output);
}
