
#include "particle.hlsli"

RWStructuredBuffer<ControlParticle> controlParticles;

cbuffer bonePositionsCB {
	float4 boneBuffer[53]; //TODO actual number
};

cbuffer modelViewProjCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

[numthreads(1, 1, 1)]
void csRigControlParticles(uint3 DTid : SV_GroupID)
{	
	
	unsigned int tid = DTid.x;

	//float3 pos = mul(float4(controlParticles[tid].position,1.0), modelViewProjMatrixInverse);
	float3 pos = controlParticles[tid].position;

	uint idxOfMinDist = 0;
	float minDist = 100.0;
	for (int i = 0; i < 53; i++)
	{	
		float currentDist = length(pos - boneBuffer[i].xyz);
		if (currentDist < minDist)
		{
			minDist = currentDist;
			idxOfMinDist = i;
		}
	}

	uint idxOfMinDist2 = 0;
	float minDist2 = 100.0;
	for (int i = 0; i < 53; i++)
	{
		float currentDist = length(pos - boneBuffer[i].xyz);
		if (currentDist < minDist2 && idxOfMinDist2 != idxOfMinDist)
		{
			minDist2 = currentDist;
			idxOfMinDist2 = i;
		}
	}

	uint idxOfMinDist3 = 0;
	float minDist3 = 100.0;
	for (int i = 0; i < 53; i++)
	{
		float currentDist = length(pos - boneBuffer[i].xyz);
		if (currentDist < minDist3 && idxOfMinDist3 != idxOfMinDist && idxOfMinDist3 != idxOfMinDist2)
		{
			minDist3 = currentDist;
			idxOfMinDist3 = i;
		}
	}

	uint idxOfMinDist4 = 0;
	float minDist4 = 100.0;
	for (int i = 0; i < 53; i++)
	{
		float currentDist = length(pos - boneBuffer[i].xyz);
		if (currentDist < minDist4 && idxOfMinDist4 != idxOfMinDist && idxOfMinDist4 != idxOfMinDist2)
		{
			minDist4 = currentDist;
			idxOfMinDist4 = i;
		}
	}

	controlParticles[tid].nonAnimatedPos = controlParticles[tid].position;

	float sum = minDist + minDist2;
	controlParticles[tid].blendWeights = float4(minDist / sum, minDist2 / sum, 0.0, 0.0);

	//float sum = minDist + minDist2 + minDist3 + minDist4;
	//controlParticles[tid].blendWeights = float4(minDist / sum, minDist2 / sum, minDist3 / sum, minDist4 / sum);

	//float sum = (1.0 / minDist) + (1.0 / minDist2) + (1.0 / minDist3) + (1.0 / minDist4);
	//controlParticles[tid].blendWeights = float4( (1.0/ minDist) / (sum), (1.0 / minDist2) / (sum), (1.0 / minDist3) / (sum), (1.0 / minDist4) / (sum));

	controlParticles[tid].blendIndices = uint4(idxOfMinDist, idxOfMinDist2, idxOfMinDist3, idxOfMinDist4);
	controlParticles[tid].temp = minDist;
	//controlParticles[tid].blendIndices = uint4(tid%40, 1, 1, 1);
	
}


