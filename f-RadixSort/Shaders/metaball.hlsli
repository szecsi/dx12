#include "window.hlsli"
#include "particle.hlsli"
#include "mortonHash.hlsli"

//#define metaBallRadius 1.0 / 0.005
#define metaBallRadius 1.0 / 0.005
//#define marchCount 25
//#define binaryStepCount 3

SamplerState ss;
TextureCube envTexture;

Texture2D solidRenderTarget;

cbuffer metaballPSEyePosCB
{
	float4 eyePos;
};

cbuffer metaballVSTransCB {
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
	row_major float4x4 rayDirMatrix;
};

cbuffer shadingCB {
	float4 ambientIntensity;
	float4 lightDir;
	float4 surfaceColor;
	float4 lightColor;
	float4 eta;
	float4 kappa;
	float4 metaBallMinToHit;
	float radius;
};

cbuffer shadingTypeCB {
	int type;
	int deepWater;
	int binaryStepCount;
	int maxRecursion;
	int marchCount;
};

cbuffer metaballFunctionCB {
	int functionType;
};

StructuredBuffer<float4> positions;
Buffer<uint> offsetBuffer;

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

struct RayMarchHit
{
	float3 position;
	float3 direction;
	uint recursionDepth;
	float alfa;
};

interface IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p);
	float3 callGradientCalculator(float3 p);
	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos);
};

interface IMetaballTester
{
	bool testFunction(float3 p, float3 position, float acc, out float accOut);
};

class SimpleMetaballTester : IMetaballTester
{
	bool testFunction(float3 p, float3 position, float acc, out float accOut)
	{
		float3 diff = p - position;
		float r = sqrt(dot(diff, diff)) * radius;
		float res = (1.0 / (r * r * metaBallRadius * metaBallRadius));
		if (r < 0.16)
			accOut = acc + 1.1 * res;
		if (accOut > metaBallMinToHit.x)
		{
			return true;
		}
		return false;
	}
};

class WyvillMetaballTester : IMetaballTester
{
	bool testFunction(float3 p, float3 position, float acc, out float accOut)
	{
		float r = 0.0;
		float b = 0.04;
		float a = 1.1;

		float3 diff = p - position;
		r = sqrt(dot(diff, diff)) * radius;

		float res = (-4.0 / 9.0) * pow(r / b, 6) + (17.0 / 9.0) * pow(r / b, 4) - (22.0 / 9.0) * pow(r / b, 2) + 1;

		if (r < b) {
			accOut = acc + a * res;
		}

		if (accOut > metaBallMinToHit.x)
		{
			return true;
		}

		return false;
	}
};

class NishimuraMetaballTester : IMetaballTester
{
	bool testFunction(float3 p, float3 position, float acc, out float accOut)
	{
		float r = 0.0;
		float b = 0.04;
		float a = 1.1;

		float3 diff = p - position;
		r = sqrt(dot(diff, diff)) * radius;

		float res = 0.0;

		if (r <= b / 3.0 && r >= 0.0)
		{
			res = 1.0 - (3.0 * pow(r / b, 2));
		}
		if (r >= b / 3.0 && r <= b)
		{
			res = 1.5 * pow(1.0 - (r / b), 2);
		}

		if (r < b) {
			accOut = acc + a * res;
		}

		if (accOut > metaBallMinToHit.x)
		{
			return true;
		}

		return false;
	}
};

class MurakamiMetaballTester : IMetaballTester
{
	bool testFunction(float3 p, float3 position, float acc, out float accOut)
	{
		float r = 0.0;
		float b = 0.04;
		float a = 1.1;

		float3 diff = p - position;
		r = sqrt(dot(diff, diff)) * radius;

		float res = pow(1.0 - pow(r / b, 2), 2);

		if (r < b) {
			accOut = acc + a * res;
		}

		if (accOut > metaBallMinToHit.x)
		{
			return true;
		}

		return false;
	}
};

void BoxIntersect(float3 rayOrigin, float3 rayDir, float3 minBox, float3 maxBox, out bool intersect, out float tStart, out float tEnd)
{
	float3 invDirection = rcp(rayDir);
	float3 t0 = float3 (minBox - rayOrigin) * invDirection;
	float3 t1 = float3 (maxBox - rayOrigin) * invDirection;
	float3 tMin = min(t0, t1);
	float3 tMax = max(t0, t1);
	float tMinMax = max(max(tMin.x, tMin.y), tMin.z);
	float tMaxMin = min(min(tMax.x, tMax.y), tMax.z);

	const float floatMax = 1000.0;
	intersect = (tMinMax <= tMaxMin) & (tMaxMin >= 0.0f) & (tMinMax <= floatMax);
	if (tMinMax < 0.0)
	{
		tMinMax = 0.0;
	}

	tStart = tMinMax;
	tEnd = tMaxMin;
}

float Fresnel(float3 inDir, float3 normal, float n)
{
	float cosa = abs(dot(-inDir, normal));	// 1
	float sina = sqrt(1 - cosa * cosa);		// 0
	float disc = 1 - sina * sina / (n * n);	// 1
	if (disc < 0) return 1;
	float cosd = sqrt(disc);				// 1
	float Rs = (cosa - n * cosd) / (cosa + n * cosd); // -0.2/2.2
	Rs *= Rs;
	float Rp = (cosd - n * cosa) / (cosd + n * cosa);
	Rp *= Rp;
	float fresnel = (Rs + Rp) / 2.0f;
	return saturate(fresnel);
}

float3 FresnelForMetals(float3 inDir, float3 normal, float3 n, float3 k)
{
	float cosTheta = abs(dot(-inDir, normal));
	float3 one = float3(1.0, 1.0, 1.0);

	return ((n - one * n - one) + 4.0 * n * pow(1.0 - cosTheta, 5.0) + k * k) / ((n + one) * (n + one) + k * k);
}

float2 WorldToNDC(float3 wp) // ???
{
	float4 worldPos = mul(float4(wp, 1.0), modelViewProjMatrix);
	worldPos /= worldPos.w;
	float2 screenPos = float2(0.0, 0.0);
	screenPos.x = ((worldPos.x + 1.0) * windowWidth) / 2.0;
	screenPos.y = ((worldPos.y - 1.0) * windowHeight) / -2.0;
	return screenPos;
}

float2 WorldToScreen(float3 wp)
{
	float4 worldPos = mul(float4(wp, 1.0), modelViewProjMatrix);
	worldPos /= worldPos.w;
	float2 screenPos = float2(0.0, 0.0);
	screenPos.x = ((worldPos.x + 1.0) * windowWidth) / 2.0;
	screenPos.y = ((worldPos.y - 1.0) * windowHeight) / -2.0;
	return screenPos;
}

float2 ScreenToUV (float2 sp)
{
	return float2 (sp.x / windowWidth, sp.y / windowHeight);
}


// Metaball test functions

bool MetaBallTest(float3 p, IMetaballTester metaballTester)
{
	float acc = 0.0;
	for (int i = 0; i < particleCount; i++) {
		if (metaballTester.testFunction(p, positions[i].xyz, acc, acc) == true)
		{
			return true;
		}
	}

	return false;
}

// Gradient calculator funtions

float3 calculateGrad(float3 p, float3 position, float3 grad)
{
	float3 gradOut = grad;
	float weight = (pow((-2.0 * metaBallRadius), 2.0) / pow(length(p - float3(position)), 3.0)) * ((-1.0) / (2.0 * length(p - float3(position))));
	gradOut.x += (weight * (p.x - position.x));
	gradOut.y += (weight * (p.y - position.y));
	gradOut.z += (weight * (p.z - position.z));

	return gradOut;
}

float3 Grad(float3 p) {
	float3 grad;

	for (int i = 0; i < particleCount; i++) {
		grad = calculateGrad(p, positions[i].xyz, grad);
	}

	return grad;
}

// Binary search functions

float3 BinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos, IMetaballVisualizer metaballVisualizer)
{
	float3 newStart = startPos;
	float3 newEnd = endPos;

	int i;
	for (i = 0; i < binaryStepCount; i++)
	{
		float3 mid = (startPos + endPos) / 2.0;
		bool midInside = metaballVisualizer.callMetaballTestFunction(mid);
		if (midInside == startInside)
		{
			newStart = mid;
		}
		if (midInside == endInside)
		{
			newEnd = mid;
		}
	}

	return newEnd;
}

// Visualizers

class NormalMetaballVisualizer : IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p)
	{
		if (functionType == 2)
		{
			WyvillMetaballTester wyvillMetaballTester;
			return MetaBallTest(p, wyvillMetaballTester);
		}
		if (functionType == 3)
		{
			NishimuraMetaballTester nishimuraMetaballTester;
			return MetaBallTest(p, nishimuraMetaballTester);
		}
		if (functionType == 4)
		{
			MurakamiMetaballTester murakamiMetaballTester;
			return MetaBallTest(p, murakamiMetaballTester);
		}
		SimpleMetaballTester simpleMetaballTester;
		return MetaBallTest(p, simpleMetaballTester);
	}

	float3 callGradientCalculator(float3 p)
	{
		return Grad(p);
	}

	float3 phongShading(float3 p, float ambientIntensity, float specularIntensity, float3 lightDir, float3 viewDir, float3 reflectDir, float3 surfaceColor, float3 lightColor, int shininess)
	{
		float3 normal = normalize(Grad(p));
		float3 ambient = ambientIntensity * lightColor;
		float3 diffuse = max(dot(normal, lightDir), 0.0) * lightColor;
		float3 specular = specularIntensity * pow(max(dot(viewDir, reflectDir), 0.0), shininess) * lightColor;
		return (ambient + diffuse + specular) * surfaceColor;
	}

	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos)
	{
		NormalMetaballVisualizer normalMetaballVisualizer;

		return BinarySearch(startInside, startPos, endInside, endPos, normalMetaballVisualizer);
	}
};

float4 CalculateColor_Gradient(float3 rayDir, IMetaballVisualizer metaballVisualizer)
{
	float ambientIntensity2 = 0.7;
	float3 lightDir2 = float3(1.0, 0.0, 0.0);
	float3 surfaceColor2 = float3(0.22745, 0.20000, 0.20000);
	float3 lightColor2 = float3(1.0, 1.0, 1.0);

	float tStart, tEnd;
	float3 p = eyePos;
	float3 d = normalize(rayDir);

	float2 screenPos = WorldToScreen(eyePos + d);
	float eyeDistToSolid = solidRenderTarget.Load(uint3 (screenPos.x, screenPos.y, 0)).w;
	//float2 uv = float2 (screenPos.x / windowWidth, screenPos.y / windowHeight);
	//float eyeDistToSolid = solidRenderTarget.Sample(ss, uv).w;

	const float boundarySideThreshold = boundarySide * 1.1;
	const float boundaryTopThreshold = boundaryTop * 1.1;
	const float boundaryBottomThreshold = boundaryBottom * 1.1;

	bool intersect;
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
		float3 step = d * (tEnd - tStart) / float(marchCount);
		p += d * tStart;

		[loop]
		for (int i = 0; i < marchCount; i++)
		{
			if (eyeDistToSolid != 0.0 && distance(eyePos, p) > eyeDistToSolid)
			{
				return float4 (solidRenderTarget.Load(uint3 (screenPos.x, screenPos.y, 0)).rgb, 1.0);
				//return float4(solidRenderTarget.Sample(ss, uv).xyz, 1.0);
			}
			else
			{
				if (metaballVisualizer.callMetaballTestFunction(p))
				{
					//return float4(1.0, 1.0, 1.0, 1.0);
					p = metaballVisualizer.doBinarySearch(false, p - step, true, p);
					float3 normal = normalize(metaballVisualizer.callGradientCalculator(p));

					float3 ref = reflect(normalize(rayDir), normal);
					return float4(abs(normal), 1.0);

					if ((int)type == 1)
					{
						float specularIntensity = 0.9;
						int shininess = 1;
						float3 ambient = ambientIntensity * lightColor;
						float3 diffuse = max(dot(normal, lightDir), 0.0) * lightColor;
						float3 specular = specularIntensity * pow(max(dot(normalize(rayDir), ref), 0.0), shininess) * lightColor;
						return float4((ambient + diffuse + specular) * surfaceColor, 1.0);
					}
					if ((int)type == 2)
					{
						float3 fresnel = FresnelForMetals(normalize(rayDir), normal, eta.xyz, kappa.xyz);
						float3 envColor = envTexture.SampleLevel(ss, ref, 0);
						return float4(fresnel * envColor, 1.0);
					}
					return float4(normalize(metaballVisualizer.callGradientCalculator(p)), 1.0);
				}

				p += step;
			}
		}
	}
	//if ((int)type == 1) 
	//{
	//	return float4(1.0, 1.0, 1.0, 1.0);
	//}
	return envTexture.Sample(ss, d);
}

float4 CalculateColor_Realistic(float3 rayDir, float4 screenPosDef, IMetaballVisualizer metaballVisualizer)
{
	const float boundarySideThreshold = boundarySide * 1.1;
	const float boundaryTopThreshold = boundaryTop * 1.1;
	const float boundaryBottomThreshold = boundaryBottom * 1.1;

	//float2 screenPos = WorldToScreen(eyePos + normalize(rayDir));
	//float2 uv = float2 (screenPos.x / windowWidth, screenPos.y / windowHeight);
	//float eyeDistToSolid = solidRenderTarget.Sample(ss, uv).w;

	RayMarchHit firstElem;
	firstElem.position = eyePos;
	firstElem.direction = normalize(rayDir);
	firstElem.recursionDepth = 0;
	firstElem.alfa = 1.0;

	RayMarchHit stack[16];
	uint stackSize = 1;
	stack[0] = firstElem;

	float3 color = float3(0.0, 0.0, 0.0);

	uint killer = 10;
	[loop]
	while (stackSize > 0 && killer > 0)
	{
		killer--;
		stackSize--;
		float3 marchPos = stack[stackSize].position;
		float3 marchDir = stack[stackSize].direction;
		uint marchRecursionDepth = stack[stackSize].recursionDepth;
		float marchAlfa = stack[stackSize].alfa;

		float tStart, tEnd;
		bool intersect;
		BoxIntersect
		(
			marchPos,
			marchDir,
			float3 (-boundarySideThreshold, boundaryBottomThreshold, -boundarySideThreshold),
			float3 (boundarySideThreshold, boundaryTopThreshold, boundarySideThreshold),
			intersect,
			tStart,
			tEnd
		);

		if (intersect && marchRecursionDepth < maxRecursion)
		{
			bool startedInside = metaballVisualizer.callMetaballTestFunction(marchPos);
			float3 start = marchPos;
			float3 marchStep = marchDir * (tEnd - tStart) / float(marchCount);
			marchPos += marchDir * tStart;

			bool marchHit = false;
			bool solidHit = false;
			for (int i = 0; i < marchCount && !marchHit && !solidHit; i++)
			{
				float2 screenPos = WorldToScreen(marchPos);
				float eyeDistToSolid = solidRenderTarget.Load(uint3 (screenPos.x, screenPos.y, 0)).w;
				if (eyeDistToSolid != 0.0 && distance (eyePos, marchPos) > eyeDistToSolid)
				{
					solidHit = true;
				}
				else
				{
					bool inside = metaballVisualizer.callMetaballTestFunction(marchPos);
					if (inside && !startedInside || !inside && startedInside)
					{
						marchHit = true;
						marchPos = metaballVisualizer.doBinarySearch(startedInside, start, inside, marchPos);

						float distance = length(marchPos - stack[stackSize].position);
						float i0 = 1.0f;
						if ((int)deepWater == 1)
						{
							i0 = startedInside ? 1.0f * exp(-distance * 13.0) : 1.0;
						}

						float3 normal = normalize(-metaballVisualizer.callGradientCalculator(marchPos));
						float refractiveIndex = 1.4;
						if (dot(normal, marchDir) > 0) {
							normal = -normal;
							refractiveIndex = 1.0 / refractiveIndex;
						}
						float fresnelAlfa = Fresnel(normalize(marchDir), normalize(normal), refractiveIndex);
						float reflectAlfa = fresnelAlfa * marchAlfa * (i0);
						float refractAlfa = (1.0 - fresnelAlfa) * marchAlfa * (i0);

						float3 refractDir = refract(marchDir, normal, 1.0 / refractiveIndex);

						if (reflectAlfa > 0.01)
						{
							RayMarchHit reflectElem;
							reflectElem.direction = reflect(marchDir, normal);
							reflectElem.position = marchPos + normal * 0.1;
							reflectElem.recursionDepth = marchRecursionDepth + 1;
							reflectElem.alfa = reflectAlfa;

							stack[stackSize] = reflectElem;
							stackSize++;
						}

						if (refractAlfa > 0.01)
						{
							RayMarchHit refractElem;
							refractElem.direction = refractDir;
							refractElem.position = marchPos;
							refractElem.recursionDepth = marchRecursionDepth + 1;
							refractElem.alfa = refractAlfa;

							stack[stackSize] = refractElem;
							stackSize++;
						}
					}

					marchPos += marchStep;
				}
			}

			if (solidHit)
			{
				float2 screenPos = WorldToScreen(marchPos);
				color += solidRenderTarget.Load(uint3 (screenPos.x, screenPos.y, 0)).rgb * marchAlfa;
				//color += solidRenderTarget.Sample(ss, uv).rgb * marchAlfa;
			}
			else if (!marchHit)
			{
				//if (eyeDistToSolid == 0.0)
				//{
					color += envTexture.SampleLevel(ss, marchDir, 0) * marchAlfa;
				//}
				//else
				//{
				//	color += solidRenderTarget.Sample(ss, uv).rgb * marchAlfa;
				//}
				//color += float4(1, 1, 1, 1)/*envTexture.Sample(ss, marchDir)*/ * marchAlfa;
				//color += envTexture.SampleLevel(ss, marchDir, 0) * marchAlfa * 0.1;
				//color += solidRenderTarget.Sample(ss, uv) * marchAlfa * 0.9;
				//float w_tex = solidRenderTarget.Sample(ss, uv).w;
				//color += float4 (w_tex, w_tex, w_tex, 1.0) * marchAlfa;
			}
		}
		else
		{
			float2 screenPos = WorldToScreen(marchPos);
			float eyeDistToSolid = solidRenderTarget.Load(uint3 (screenPos.x, screenPos.y, 0)).w;
			if (!intersect || eyeDistToSolid == 0.0)
			{
				color += envTexture.SampleLevel(ss, marchDir, 0) * marchAlfa;
			}
			else
			{
				color += solidRenderTarget.Load(uint3 (screenPos.x, screenPos.y, 0)).rgb * marchAlfa;
				//color += solidRenderTarget.Sample(ss, uv).rgb * marchAlfa;
			}

			//color += solidRenderTarget.Sample(ss, uv) * marchAlfa * 0.9;
			//float w_tex = solidRenderTarget.Sample(ss, uv).w;
			//color += float4 (w_tex, w_tex, w_tex, 1.0) * marchAlfa;
		}
	}

	if (killer == 0)
	{
		//return float4 (1.0, 0.0, 0.0, 1.0);
	}

	return float4 (color, 1.0);

}
