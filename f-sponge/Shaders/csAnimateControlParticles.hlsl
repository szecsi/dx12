
#include "particle.hlsli"

RWStructuredBuffer<ControlParticle> controlParticles;

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

[numthreads(1, 1, 1)]
void csAnimateControlParticles(uint3 DTid : SV_GroupID)
{	
	unsigned int tid = DTid.x;

	//controlParticles[tid].blendWeights = float4(1.0, 0.0, 0.0, 0.0);
	//controlParticles[tid].blendIndices = uint4(10, 0, 0, 0);

	float4 originalPos = mul(controlParticles[tid].nonAnimatedPos.xyz, modelViewProjMatrixInverse);

	uint4 blendIndices = controlParticles[tid].blendIndices;
	controlParticles[tid].blendWeights.w = 1 - dot(controlParticles[tid].blendWeights.xyz, float3(1, 1, 1));
	float2x4 qe0 = float2x4(boneBuffer[blendIndices.x].orientation, boneBuffer[blendIndices.x].dualTranslation);
	float2x4 qe1 = float2x4(boneBuffer[blendIndices.y].orientation, boneBuffer[blendIndices.y].dualTranslation);
	float2x4 qe2 = float2x4(boneBuffer[blendIndices.z].orientation, boneBuffer[blendIndices.z].dualTranslation);
	float2x4 qe3 = float2x4(boneBuffer[blendIndices.w].orientation, boneBuffer[blendIndices.w].dualTranslation);
	float3 podality = float3(dot(qe0[0], qe1[0]),
		dot(qe0[0], qe2[0]),
		dot(qe0[0], qe3[0]));

	controlParticles[tid].blendWeights.yzw *= (podality >= 0) ? 1 : -1;

	float2x4 qe = controlParticles[tid].blendWeights.x * qe0;
	qe += controlParticles[tid].blendWeights.y * qe1;
	qe += controlParticles[tid].blendWeights.z * qe2;
	qe += controlParticles[tid].blendWeights.w * qe3;

	float len = length(qe[0]);
	qe /= len;

	float3 blendedPos = originalPos.xyz + 2 * cross(qe[0].xyz,
		cross(qe[0].xyz, originalPos.xyz) +
		qe[0].w * originalPos.xyz);
	float3 trans = 2.0*(qe[0].w*qe[1].xyz - qe[1].w*qe[0].xyz + cross(qe[0].xyz, qe[1].xyz));
	blendedPos += trans;

	float4 backTraf = mul(blendedPos, modelViewProjMatrix);
	controlParticles[tid].position.xyz = backTraf.xyz;

	//controlParticles[tid].position.xyz = mul(mul(float4(controlParticles[tid].nonAnimatedPos.xyz, 1), modelViewProjMatrixInverse), modelViewProjMatrix).xyz;
	//controlParticles[tid].position.xyz = controlParticles[tid].nonAnimatedPos.xyz;



	//controlParticles[tid].position.xyz = mul(float4(controlParticles[tid].nonAnimatedPos.xyz, 1), modelViewProjMatrix);
	//controlParticles[tid].position.xyz = blendedPos;
	//controlParticles[tid].position.xyz = mul(float4(blendedPos, 1), modelViewProjMatrix);
	//controlParticles[tid].position.xyz = mul(float4(controlParticles[tid].nonAnimatedPos.xyz, 1), modelViewProjMatrix);
	//controlParticles[tid].position.xyz = controlParticles[tid].nonAnimatedPos.xyz + trans;
	//controlParticles[tid].position.z = 0.0;
}


