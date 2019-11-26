#include "Billboard.hlsli"

[RootSignature(BillboardRootSig)][maxvertexcount(4)]
void main(
	point VSOutput input[1],
	inout TriangleStream<GSOutput> stream) {
	float4 hndcPos = mul(viewProjMat,
		mul(modelMat, float4(input[0].pos, 1)
		));
	float s = input[0].age / input[0].lifespan;

	GSOutput output;
	output.opacity = 1 - abs(s * 2 - 1);
	output.opacity *= 0.1;

	output.pos = hndcPos;
	output.pos.x += billboardSize.x * s;
	output.pos.y += billboardSize.y * s;
	output.tex = float2(1, 0);
	stream.Append(output);
	output.pos = hndcPos;
	output.pos.x += billboardSize.x * s;
	output.pos.y -= billboardSize.y * s;
	output.tex = float2(1, 1);
	stream.Append(output);
	output.pos = hndcPos;
	output.pos.x -= billboardSize.x * s;
	output.pos.y += billboardSize.y * s;
	output.tex = float2(0, 0);
	stream.Append(output);
	output.pos = hndcPos;
	output.pos.x -= billboardSize.x * s;
	output.pos.y -= billboardSize.y * s;
	output.tex = float2(0, 1);
	stream.Append(output);
}
