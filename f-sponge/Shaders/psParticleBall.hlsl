#include "metaball.hlsli"
#include "window.hlsli"

cbuffer debugTypeCB
{
	uint debugType;
	uint3 temp;
};

bool BallTest(float3 p, out float3 grad)
{
	const float r = 0.005;

	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
		{
			grad = normalize(p - float3(particles[i].position));
			return true;
		}
	}

	return false;
}

float ParticleCount(float3 p)
{
	const float r = 0.005;
	const float maxHitCount = 2;

	float hitCount = 0;
	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
		{
			hitCount++;
		}
	}

	return hitCount/ maxHitCount;
}

float Index (float3 p)
{
	const float r = 0.005;

	float hitCount = 0;
	float index = 0;
	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
		{
			hitCount++;
			index += i;
		}
	}

	return index / hitCount / particleCount;
}

float Zindex(float3 p)
{
	const float r = 0.005;
	const float maxZindex = 1073741823;

	float hitCount = 0;
	float index = 0;
	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
		{
			hitCount++;
			index += particles[i].zindex;
		}
	}

	return index / hitCount / (maxZindex / 10.0);
}

float Pressure(float3 p)
{
	const float r = 0.005;
	const float maxPressure = 1000.0;

	float hitCount = 0;
	float pressure = 0;
	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
		{
			hitCount++;
			pressure += particles[i].pressure;
		}
	}

	return pressure / hitCount / maxPressure;
}

float MassDensity(float3 p)
{
	const float r = 0.005;
	const float maxMassDensity = 1000.0;

	float hitCount = 0;
	float massDensity = 0;
	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
		{
			hitCount++;
			massDensity += particles[i].massDensity;
		}
	}

	return massDensity / hitCount / maxMassDensity;
}

float Speed(float3 p)
{
	const float r = 0.005;
	const float maxSpeed = 10.0;

	float hitCount = 0;
	float speed = 0;
	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
		{
			hitCount++;
			speed += length(particles[i].velocity);
		}
	}

	return speed / hitCount / maxSpeed;
}

float3 RandColor(float3 p) {
	const float r = 0.005;

	for (int i = 0; i < particleCount; i++)
	{
		if (length(p - float3(particles[i].position)) < r)
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

	return float3 (0, 0, 0);
}

float4 psParticleBall(VsosQuad input) : SV_Target
{
	const int stepCount = 50;
	const float boundarySideThreshold = boundarySide * 1.1;
	const float boundaryTopThreshold = boundaryTop * 1.1;
	const float boundaryBottomThreshold = boundaryBottom * 1.1;

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

		for (int i = 0; i<stepCount; i++)
		{
			float3 grad;
			if (BallTest(p, grad))
			{
				if (debugType == 0)
				{
					return float4 ((RandColor(p)), 1.0);
					//return float4 ((grad), 1.0);
				}
				if (debugType == 1)
				{
					return float4 (ParticleCount(p), 0.0, 0.0, 1.0);
				}
				else if (debugType == 2)
				{
					return float4 (Index(p), 0.0, 0.0, 1.0);
				}
				else if (debugType == 3)
				{
					return float4 (Zindex(p), 0.0, 0.0, 1.0);
				}
				else if (debugType == 4)
				{
					return float4 (0.0, 0.0, Pressure(p), 1.0);
				}
				else if (debugType == 5)
				{
					return float4 (0.0, 0.0, MassDensity(p), 1.0);
				}
				else if (debugType == 6)
				{
					return float4 (0.0, Speed(p), 0.0, 1.0);
				}
				else
				{
					return float4 (0.0, 0.0, 0.0, 1.0);
				}				
			}

			p += step;
		}
	}
	return envTexture.Sample(ss, d) + float4(1,1,1,0);

}



