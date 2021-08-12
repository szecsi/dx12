
struct DualQuat {
	float4 orientation;
	float4 dualTranslation;
};

cbuffer boneCB {
	DualQuat boneBuffer[46]; //TODO actual number
};

cbuffer modelViewProjCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

struct IaosSkinning
{
	float4 pos : POSITION;
	float4 blendWeights	: BLENDWEIGHTS;
	uint4 blendIndices	: BLENDINDICES;
	float3 normal	: NORMAL;
	float3 tex		: TEXCOORD0;
};

struct VsosTrafo
{
	float4 pos : SV_POSITION;
	float3 normal : NORMAL;
	float4 worldPos : WORLD;
	float2 tex : TEXCOORD;
};

VsosTrafo vsSkinning(IaosSkinning input) {
	VsosTrafo output = (VsosTrafo)0;
	uint4 blendIndices = input.blendIndices;//D3DCOLORtoUBYTE4(input.blendIndices);
	input.blendWeights.w = 1 - dot(input.blendWeights.xyz, float3(1, 1, 1));
	float2x4 qe0 = float2x4(boneBuffer[blendIndices.x].orientation, boneBuffer[blendIndices.x].dualTranslation);
	float2x4 qe1 = float2x4(boneBuffer[blendIndices.y].orientation, boneBuffer[blendIndices.y].dualTranslation);
	float2x4 qe2 = float2x4(boneBuffer[blendIndices.z].orientation, boneBuffer[blendIndices.z].dualTranslation);
	float2x4 qe3 = float2x4(boneBuffer[blendIndices.w].orientation, boneBuffer[blendIndices.w].dualTranslation);
	float3 podality = float3(dot(qe0[0], qe1[0]),
		dot(qe0[0], qe2[0]),
		dot(qe0[0], qe3[0]));

	input.blendWeights.yzw *= (podality >= 0) ? 1 : -1;

	float2x4 qe = input.blendWeights.x * qe0;
	qe += input.blendWeights.y * qe1;
	qe += input.blendWeights.z * qe2;
	qe += input.blendWeights.w * qe3;

	float len = length(qe[0]);
	qe /= len;

	float3 blendedPos = input.pos.xyz + 2 * cross(qe[0].xyz,
		cross(qe[0].xyz, input.pos.xyz) +
		qe[0].w * input.pos.xyz);
	float3 trans = 2.0*(qe[0].w*qe[1].xyz - qe[1].w*qe[0].xyz + cross(qe[0].xyz, qe[1].xyz));
	blendedPos += trans;
	//blendedPos = input.pos.xyz; ///// TEST
	output.pos = mul(float4(blendedPos, 1), modelViewProjMatrix);
	//output.pos = mul(float4(input.pos.xyz, 1), modelViewProjMatrix);
	output.normal = normalize(input.normal.xyz + 2.0*cross(qe[0].xyz, cross(qe[0].xyz, input.normal.xyz) + qe[0].w*input.normal.xyz));
	output.worldPos = float4(blendedPos, 1);
	output.tex = input.tex.xy;
	return output;
}
