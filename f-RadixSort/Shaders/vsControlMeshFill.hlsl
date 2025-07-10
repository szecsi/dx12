

cbuffer metaballVSTransCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

struct IaosQuad
{
	float4  pos: POSITION;
	float2  tex: TEXCOORD0;
};

struct VsosQuad
{
	float4 pos: SV_POSITION;
	float2 tex: TEXCOORD0;
	float3 rayDir: TEXCOORD1;
};

VsosQuad vsControlMeshFill(IaosQuad input) {
	VsosQuad output = (VsosQuad)0;
	output.pos = input.pos;
	output.pos.z = 0.001f;
	float4 hWorldPosMinusEye = mul(output.pos, rayDirMatrix);
	hWorldPosMinusEye /= hWorldPosMinusEye.w;
	output.rayDir = hWorldPosMinusEye.xyz;
	output.tex = input.tex;
	return output;
}


