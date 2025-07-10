#include "metaball.hlsli"

StructuredBuffer<uint2> linkBuffer;

bool MetaBallTest_ABuffer(float3 p, IMetaballTester metaballTester)
{
	float2 pos = WorldToScreen(p);
	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;
	uint offset = offsetBuffer[uIndex];

	float acc = 0.0;
	while (offset != 0)
	{
		uint2 element = linkBuffer[offset];
		offset = element.x;
		int i = element.y;

		if (metaballTester.testFunction(p, positions[i].xyz, acc, acc) == true)
		{
			return true;
		}
	}

	return false;
}

float3 Grad_ABuffer(float3 p)
{
	float3 grad;

	float2 pos = WorldToScreen(p);
	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;

	uint offset = offsetBuffer[uIndex];

	while (offset != 0)
	{
		uint2 element = linkBuffer[offset];
		offset = element.x;
		int i = element.y;

		grad = calculateGrad(p, positions[i].xyz, grad);
	}

	return grad;
}

class ABufferMetaballVisualizer : IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p)
	{
		if (functionType == 2)
		{
			WyvillMetaballTester wyvillMetaballTester;
			return MetaBallTest_ABuffer(p, wyvillMetaballTester);
		}
		if (functionType == 3)
		{
			NishimuraMetaballTester nishimuraMetaballTester;
			return MetaBallTest_ABuffer(p, nishimuraMetaballTester);
		}
		if (functionType == 4)
		{
			MurakamiMetaballTester murakamiMetaballTester;
			return MetaBallTest_ABuffer(p, murakamiMetaballTester);
		}
		SimpleMetaballTester simpleMetaballTester;
		return MetaBallTest_ABuffer(p, simpleMetaballTester);
	}

	float3 callGradientCalculator(float3 p)
	{
		return Grad_ABuffer(p);
	}

	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos)
	{
		ABufferMetaballVisualizer aBufferMetaballVisualizer;

		return BinarySearch(startInside, startPos, endInside, endPos, aBufferMetaballVisualizer);
	}
};