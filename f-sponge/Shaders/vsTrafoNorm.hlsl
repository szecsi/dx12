
cbuffer modelViewProjCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

struct IaosTrafo
{
	float4 pos : POSITION;
	float4 norm: TEXCOORD0;
};

struct VsosTrafo
{
	float4 pos : SV_POSITION;
	float3 norm: NORMAL;
};

VsosTrafo vsTrafoNorm(IaosTrafo input)
{
	VsosTrafo output = (VsosTrafo)0;
	output.pos = mul(input.pos, modelViewProjMatrix);
	//output.norm = mul(input.norm, modelViewProjMatrix);
	output.norm = input.norm;
	return output;
}
