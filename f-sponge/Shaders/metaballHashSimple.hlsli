#include "metaball.hlsli"
#include "hash.hlsli"

RWByteAddressBuffer hlistBegin;
RWByteAddressBuffer hlistLength;
RWByteAddressBuffer clistBegin;
RWByteAddressBuffer clistLength;

bool MetaBallTest_HashSimple(float3 p, float4 pos, IMetaballTester metaballTester)
{
	bool result = false;
	float acc = 0.0;

	//uint zHash = 0;

	const float displacementStep = 0.08;
	const int displacementDist = 1;

	int xDis;
	int yDis;
	int zDis;
	[loop]
	for (xDis = -displacementDist; xDis <= displacementDist; xDis++) {
		[loop]
		for (yDis = -displacementDist; yDis <= displacementDist; yDis++) {
			[loop]
			for (zDis = -displacementDist; zDis <= displacementDist; zDis++) {
				float3 testp = p.xyz + displacementStep * float3(float(xDis), float(yDis), float(zDis));

				uint zIndex = mortonHash(testp);
				uint zHash = zIndex % hashCount;

				//zHash = 0;


				uint cIdx = hlistBegin.Load(zHash * 4);
				uint cIdxMax = cIdx + hlistLength.Load(zHash * 4);

				[loop]
				for (; cIdx < cIdxMax; cIdx++) {
					uint pIdx = clistBegin.Load(cIdx * 4);
					uint pIdxMax = pIdx + clistLength.Load(cIdx * 4);

					[loop]
					for (; pIdx < pIdxMax; pIdx++) {
						if (particles[pIdx].zindex == zIndex) {
							acc += 0.0001 * (hlistBegin.Load(0) + hlistLength.Load(0) + clistBegin.Load(0) + clistLength.Load(0));
							if (metaballTester.testFunction(p, particles[pIdx].position, acc, acc) == true)
							{
								result = true;
								pIdx = pIdxMax;
								cIdx = cIdxMax;
								xDis = displacementDist;
								yDis = displacementDist;
								zDis = displacementDist;
							}
						}
					}
				}
			}
		}
	}

	return result;
}


float3 Grad_HashSimple(float3 p, float4 pos)
{
	float3 grad = float3 (0.0, 0.0, 0.0);
	/*
	uint zIndex = mortonHash(pos.xyz);
	uint zHash = zIndex % hashCount;

	uint cIdx = hlistBegin.Load(zHash * 4);
	uint cIdxMax = cIdx + hlistLength.Load(zHash * 4);
	for (; cIdx < cIdxMax; cIdx++) {
	uint pIdx = clistBegin.Load(cIdx * 4);
	uint pIdxMax = pIdx + clistLength.Load(cIdx * 4);
	for (; pIdx < pIdxMax; pIdx++) {
	grad = calculateGrad(p, particles[pIdx].position, grad);
	}
	}
	*/
	/*
	const float displacementStep = 0.08;
	int displacementDist = 1;

	int xDis;
	int yDis;
	int zDis;
	[loop]
	for (xDis = -displacementDist; xDis <= displacementDist; xDis++) {
		[loop]
		for (yDis = -displacementDist; yDis <= displacementDist; yDis++) {
			[loop]
			for (zDis = -displacementDist; zDis <= displacementDist; zDis++) {

				float3 testp = p.xyz + displacementStep * float3(float(xDis), float(yDis), float(zDis));

				uint zIndex = mortonHash(testp);
				uint zHash = zIndex % hashCount;


				uint cIdx = hlistBegin.Load(zHash * 4);
				uint cIdxMax = cIdx + hlistLength.Load(zHash * 4);

				[loop]
				for (; cIdx < cIdxMax; cIdx++) {
					uint pIdx = clistBegin.Load(cIdx * 4);
					uint pIdxMax = pIdx + clistLength.Load(cIdx * 4);

					[loop]
					for (; pIdx < pIdxMax; pIdx++) {
						grad.x += 0.0001 * (hlistBegin.Load(0) + hlistLength.Load(0) + clistBegin.Load(0) + clistLength.Load(0));
						grad = calculateGrad(p, particles[pIdx].position, grad);
					}
				}
			}
		}
	}
	*/

	const float displacementStep = 0.08;
	const int displacementDist = 1;

	int xDis;
	int yDis;
	int zDis;
	[loop]
	for (xDis = -displacementDist; xDis <= displacementDist; xDis++) {
		[loop]
		for (yDis = -displacementDist; yDis <= displacementDist; yDis++) {
			[loop]
			for (zDis = -displacementDist; zDis <= displacementDist; zDis++) {
				float3 testp = p.xyz + displacementStep * float3(float(xDis), float(yDis), float(zDis));

				uint zIndex = mortonHash(testp);
				uint zHash = zIndex % hashCount;

				//zHash = 0;


				uint cIdx = hlistBegin.Load(zHash * 4);
				uint cIdxMax = cIdx + hlistLength.Load(zHash * 4);

				[loop]
				for (; cIdx < cIdxMax; cIdx++) {
					uint pIdx = clistBegin.Load(cIdx * 4);
					uint pIdxMax = pIdx + clistLength.Load(cIdx * 4);

					[loop]
					for (; pIdx < pIdxMax; pIdx++) {
						//acc += 0.0001 * (hlistBegin.Load(0) + hlistLength.Load(0) + clistBegin.Load(0) + clistLength.Load(0));
						//if (metaballTester.testFunction(p, particles[pIdx].position, acc, acc) == true)
						if (particles[pIdx].zindex == zIndex) {
							grad = calculateGrad(p, particles[pIdx].position, grad);
						}
							//result = true;
							//pIdx = pIdxMax;
							//cIdx = cIdxMax;
							//xDis = displacementDist;
							//yDis = displacementDist;
							//zDis = displacementDist;

					}
				}
			}
		}
	}

	return grad;
}

class HashSimpleMetaballVisualizer : IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p, float4 pos)
	{
		if (functionType == 2)
		{
			WyvillMetaballTester wyvillMetaballTester;
			return MetaBallTest_HashSimple(p, pos, wyvillMetaballTester);
		}
		if (functionType == 3)
		{
			NishimuraMetaballTester nishimuraMetaballTester;
			return MetaBallTest_HashSimple(p, pos, nishimuraMetaballTester);
		}
		if (functionType == 4)
		{
			MurakamiMetaballTester murakamiMetaballTester;
			return MetaBallTest_HashSimple(p, pos, murakamiMetaballTester);
		}
		SimpleMetaballTester simpleMetaballTester;
		return MetaBallTest_HashSimple(p, pos, simpleMetaballTester);
	}

	float3 callGradientCalculator(float3 p, float4 pos)
	{
		return Grad_HashSimple(p, pos);
	}

	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos, float4 pos)
	{
		HashSimpleMetaballVisualizer hashMetaballVisualizer;

		return BinarySearch(startInside, startPos, endInside, endPos, pos, hashMetaballVisualizer);
	}
};