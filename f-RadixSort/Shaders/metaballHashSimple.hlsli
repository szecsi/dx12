#include "metaball.hlsli"
//to12 #include "hash.hlsli"
#include "hhash.hlsli"

#define displacementDist 1

//to12 StructuredBuffer<uint> hashes;
//to12 RWByteAddressBuffer hlistBegin;
//to12 RWByteAddressBuffer hlistLength;
//to12 RWByteAddressBuffer clistBegin;
//to12 RWByteAddressBuffer clistLength;
ByteAddressBuffer hashes;
RWByteAddressBuffer cellLut : register(u1);
RWByteAddressBuffer hashLut : register(u2);

bool MetaBallTest_HashSimple(float3 p, IMetaballTester metaballTester)
{
	float2 pos = WorldToScreen(p);

	bool result = false;
	float acc = 0.0;

	uint3 cellIdx = getCellIndex (p);

	int xDis;
	int yDis;
	int zDis;
	[loop]
	for (xDis = -displacementDist; xDis <= displacementDist; xDis++) {
		[loop]
		for (yDis = - displacementDist; yDis <= displacementDist; yDis++) {
			[loop]
			for (zDis = -displacementDist; zDis <= displacementDist; zDis++) {
				int3 localCellIndex = cellIdx;
				localCellIndex += int3(xDis, yDis, zDis);
				uint zIndex =
					//mortonHashFromCellIndex(localCellIndex);
					packedIndexFromCellIndex(localCellIndex);
				uint zHash = hhash(zIndex);

				uint hl = hashLut.Load(zHash * 4);
				uint cIdx = hl & 0xffff;
				uint cIdxMax = cIdx + (hl >> 16);

				[loop]
				for (; cIdx < cIdxMax; cIdx++) {
					uint cl = cellLut.Load(cIdx * 4);
					uint pIdx = cl & 0xffff;
					uint pIdxMax = pIdx + (cl >> 16);

					[loop]
					for (; pIdx < pIdxMax; pIdx++) {
						if (hashes.Load(pIdx*4) == zIndex) 
						{
							//acc += 0.0001; //to12 *(hlistBegin.Load(0) + hlistLength.Load(0) + clistBegin.Load(0) + clistLength.Load(0));
							if (metaballTester.testFunction(p, positions[pIdx].xyz, acc, acc) == true)
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


float3 Grad_HashSimple(float3 p)
{
	float2 pos = WorldToScreen(p);

	float3 grad;
	const float r = metaBallRadius;// 0.005;

	uint3 cellIdx = getCellIndex(p);

	int xDis;
	int yDis;
	int zDis;
	[loop]
	for (xDis = -displacementDist; xDis <= displacementDist; xDis++) {
		[loop]
		for (yDis = -displacementDist; yDis <= displacementDist; yDis++) {
			[loop]
			for (zDis = -displacementDist; zDis <= displacementDist; zDis++) {
				int3 localCellIndex = cellIdx;
				localCellIndex += int3 (xDis, yDis, zDis);
				uint zIndex = 
					//mortonHashFromCellIndex(localCellIndex);
					packedIndexFromCellIndex(localCellIndex);
				uint zHash = hhash(zIndex);

				uint hl = hashLut.Load(zHash * 4);
				uint cIdx = hl & 0xffff;
				uint cIdxMax = cIdx + (hl >> 16);

				[loop]
				for (; cIdx < cIdxMax; cIdx++) {
					uint cl = cellLut.Load(cIdx * 4);
					uint pIdx = cl & 0xffff;
					uint pIdxMax = pIdx + (cl >> 16);

					[loop]
					for (; pIdx < pIdxMax; pIdx++) {
						if (hashes.Load(pIdx*4) == zIndex) 
						{
							//acc += 0.0001; //to12 *(hlistBegin.Load(0) + hlistLength.Load(0) + clistBegin.Load(0) + clistLength.Load(0));
							grad = calculateGrad(p, positions[pIdx].xyz, grad);
						}
					}
				}
			}
		}
	}

	return grad;
}

class HashSimpleMetaballVisualizer : IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p)
	{
		if (functionType == 2)
		{
			WyvillMetaballTester wyvillMetaballTester;
			return MetaBallTest_HashSimple(p, wyvillMetaballTester);
		}
		if (functionType == 3)
		{
			NishimuraMetaballTester nishimuraMetaballTester;
			return MetaBallTest_HashSimple(p, nishimuraMetaballTester);
		}
		if (functionType == 4)
		{
			MurakamiMetaballTester murakamiMetaballTester;
			return MetaBallTest_HashSimple(p, murakamiMetaballTester);
		}
		SimpleMetaballTester simpleMetaballTester;
		return MetaBallTest_HashSimple(p, simpleMetaballTester);
	}

	float3 callGradientCalculator(float3 p)
	{
		return Grad_HashSimple(p);
	}

	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos)
	{
		HashSimpleMetaballVisualizer hashMetaballVisualizer;

		return BinarySearch(startInside, startPos, endInside, endPos, hashMetaballVisualizer);
	}
};