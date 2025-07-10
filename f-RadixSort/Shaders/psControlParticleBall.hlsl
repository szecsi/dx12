#include "metaball.hlsli"
#include "window.hlsli"

//SamplerState ss;
//TextureCube envTexture;

StructuredBuffer<float4> controlPositions;
Buffer<uint> controlParticleCounter;

bool BallTest(float3 p)
{
	const float r = 0.01;

	for (int i = 0; i < controlParticleCounter[0]; i++)
	{
		if (length(p - float3(controlPositions[i].xyz)) < r)
		{
			return true;
		}
	}

	return false;
}

float3 RandColor(float3 p) {
	const float r = 0.01;

	for (int i = 0; i < controlParticleCounter[0]; i++)
	{
		if (length(p - float3(controlPositions[i].xyz)) < r)
		{
			int div = i % 6;
			if (div == 0)
			{
				return float3 (1, 0, 0);
			}
			else if (div == 1)
			{
				return float3 (0, 1, 0);
			}
			else if (div == 2)
			{
				return float3 (0, 0, 1);
			}
			else if (div == 3)
			{
				return float3 (1, 1, 0);
			}
			else if (div == 4)
			{
				return float3 (1, 0, 1);
			}
			else if (div == 5)
			{
				return float3 (0, 1, 1);
			}
		}
	}

	return float3 (0,0,0);
}

float4 psControlParticleBall(VsosQuad input) : SV_Target
{
	const int stepCount = 50;
	const float boundarySideThreshold = boundarySide * 1.3;
	const float boundaryTopThreshold = boundaryTop * 1.3;
	const float boundaryBottomThreshold = boundaryBottom * 1.3;

	float3 d = normalize(input.rayDir);
	float3 p = eyePos;

	bool intersect;
	float tStart;
	float tEnd;
	BoxIntersect
	(
		p,
		d,
		float3 (-boundarySideThreshold, boundaryBottomThreshold, -boundarySideThreshold),
		float3 (boundarySideThreshold, boundaryTopThreshold, boundarySideThreshold),
		intersect,
		tStart,
		tEnd
	);

	if (intersect)
	{
		float3 step = d * (tEnd - tStart) / float(stepCount);
		p += d * tStart;

		for (int i = -3; i<stepCount + 3; i++)
		{
			if (BallTest(p))
			{
				return float4 ((RandColor(p)), 1.0);
			}

			p += step;
		}
	}
	return envTexture.Sample(ss, d) + float4(1, 1, 1, 0);

}



