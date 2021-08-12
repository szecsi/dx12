
cbuffer modelViewProjCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

struct IaosTrafo
{
	float4 pos : POSITION;
};

struct VsosTrafo
{
	float4 pos : SV_POSITION;
};

VsosTrafo vsTrafo(IaosTrafo input)
{
	VsosTrafo output = (VsosTrafo)0;
	output.pos = mul(input.pos,	modelViewProjMatrix);
	return output;
}
