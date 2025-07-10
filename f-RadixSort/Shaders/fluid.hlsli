#define pi 3.1415

#define dt 0.01 // s
#define g  9.82 // m/s2

// Water
#define massPerParticle 0.02	// kg
#define restMassDensity 998.29	// kg/m3
#define supportRadius_w 0.0457	// m
#define gasStiffness 3.0		// J
#define viscosity 3.5			// Pa*s
#define surfaceTension 0.0728	// N/m

float defaultSmoothingKernel (float3 deltaPos, float supportRadius)
{
	if (length(deltaPos) > supportRadius)
	{
		return 0.0;
	}
	else
	{
		return (315.0 / (64.0 * pi * pow(supportRadius, 9))) * pow((pow(supportRadius, 2) - dot(deltaPos, deltaPos)), 3);
	}
}

float3 defaultSmoothingKernelGradient (float3 deltaPos, float supportRadius)
{
	if (length(deltaPos) > supportRadius)
	{
		return float3 (0.0, 0.0, 0.0);
	}
	else
	{
		return (-945.0 / (32.0 * pi * pow(supportRadius, 9))) * deltaPos * pow((pow(supportRadius, 2) - dot(deltaPos, deltaPos)), 2);
	}
}

float defaultSmoothingKernelLaplace (float3 deltaPos, float supportRadius)
{
	if (length(deltaPos) > supportRadius)
	{
		return 0.0;
	}
	else
	{
		//return (-945.0 / (32.0 * pi * pow(supportRadius, 9))) * deltaPos * (pow(supportRadius, 2) - dot(deltaPos, deltaPos)) * (3.0 * pow(supportRadius, 2) - 7.0 * dot(deltaPos, deltaPos));
		return (-945.0 / (32.0 * pi * pow(supportRadius, 9))) * (pow(supportRadius, 2) - dot(deltaPos, deltaPos)) * (3.0 * pow(supportRadius, 2) - 7.0 * dot(deltaPos, deltaPos));
	}
}

float pressureSmoothingKernel(float3 deltaPos, float supportRadius)
{
	float lengthOfDeltaPos = length(deltaPos);
	if (lengthOfDeltaPos > supportRadius)
	{
		return 0.0;
	}
	else
	{
		return (15.0 / (pi * pow(supportRadius, 6))) * pow(supportRadius - lengthOfDeltaPos, 3);
	}
}

float3 pressureSmoothingKernelGradient (float3 deltaPos, float supportRadius)
{
	float lengthOfDeltaPos = length(deltaPos);
	if (lengthOfDeltaPos > supportRadius)
	{
		return float3 (0.0, 0.0, 0.0);
	}
	else
	{
		return (-45.0 / (pi * pow(supportRadius, 6))) * (deltaPos/ lengthOfDeltaPos) * pow(supportRadius - lengthOfDeltaPos, 2);
	}
}

float viscositySmoothingKernelLaplace (float3 deltaPos, float supportRadius)
{
	float lengthOfDeltaPos = length(deltaPos);
	if (lengthOfDeltaPos > supportRadius)
	{
		return 0.0;
	}
	else
	{
		return (45.0 / (pi * pow(supportRadius, 6))) * (supportRadius - lengthOfDeltaPos);
	}
}