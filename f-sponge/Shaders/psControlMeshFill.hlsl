
#include "particle.hlsli"
#include "window.hlsli"

SamplerState ss;
TextureCube envTexture;

cbuffer metaballVSTransCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse; // MVP Inverse
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

struct VsosQuad
{
	float4 pos: SV_POSITION;
	float2 tex: TEXCOORD0;
	float3 rayDir: TEXCOORD1;
};


struct LinkData
{
	uint link;
	float depth;
};

RWStructuredBuffer<ControlParticle> controlParticles;
RWByteAddressBuffer controlParticleCounter;

Buffer<uint> offsetBuffer;
StructuredBuffer<LinkData> linkBuffer;

void AddControlParticle (float3 pos)
{
	uint newControlParticleIdx;
	controlParticleCounter.InterlockedAdd(0, 1, newControlParticleIdx);

	ControlParticle cp;
	cp.controlPressureRatio = 1.0;
	cp.position = pos;
	cp.nonAnimatedPos = pos;
	cp.temp = 0.0;
	cp.blendWeights = float4 (0.0, 0.0, 0.0, 0.0);
	cp.blendIndices = uint4 (0, 0, 0, 0);
	controlParticles[newControlParticleIdx] = cp;
}

void AddControlParticle(float3 pos, float pressure)
{
	uint newControlParticleIdx;
	controlParticleCounter.InterlockedAdd(0, 1, newControlParticleIdx);

	ControlParticle cp;
	cp.controlPressureRatio = pressure;
	cp.position = pos;
	controlParticles[newControlParticleIdx] = cp;
}

float3 NDCToWorld(float2 screenPos, float depth)
{
	//float4 worldPos = mul(float4(screenPos.x / windowWidth * 2.0 - 1.0, screenPos.y / windowHeight * -2.0 + 1.0, depth, 1.0), modelViewProjMatrixInverse);
	float4 worldPos = mul(float4(screenPos.x / fillWindowWidth * 2.0 - 1.0, screenPos.y / fillWindowHeight * -2.0 + 1.0, depth, 1.0), modelViewProjMatrixInverse);
	worldPos /= worldPos.w;
	return worldPos.xyz;
}


void psControlMeshFill(VsosQuad input)
{
	
	const float placementDistance = 0.02;

	uint uIndex = (uint)input.pos.y * (uint)fillWindowWidth + (uint)input.pos.x;
	uint offset = offsetBuffer[uIndex];

	while (offset != 0 /*frontface*/ && linkBuffer[offset].link != 0 /*backface*/)
	{
		float startDepth = linkBuffer[offset].depth;
		float endDepth = linkBuffer[linkBuffer[offset].link].depth;

		float3 startWorldPos = NDCToWorld (input.pos.xy, startDepth);
		float3 endWorldPos = NDCToWorld (input.pos.xy, endDepth);		
		
		float3 step = endWorldPos - startWorldPos;
		float dist = length(step);
		step *= (placementDistance / dist);
		
		//AddControlParticle(startWorldPos);
		//return;

		if (dist < placementDistance)
		{
			AddControlParticle(startWorldPos.xyz);
			AddControlParticle((startWorldPos.xyz + endWorldPos.xyz) / 2.0);
			AddControlParticle(endWorldPos.xyz);
		}
		else
		{
			float3 marchPos = startWorldPos.xyz;

			while (dist > placementDistance)
			{
				AddControlParticle(marchPos);
				marchPos += step;
				dist -= placementDistance;
			}

			AddControlParticle(endWorldPos.xyz);
		}

		offset = linkBuffer[linkBuffer[offset].link].link;
	}
}



