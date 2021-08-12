#include "metaball.hlsli"

StructuredBuffer<uint2> linkBuffer;

bool MetaBallTest_ABuffer(float3 p, float4 pos, IMetaballTester metaballTester)
{
	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;
	uint offset = offsetBuffer[uIndex];

	float acc = 0.0;
	while (offset != 0)
	{
		uint2 element = linkBuffer[offset];
		offset = element.x;
		int i = element.y;

		if (metaballTester.testFunction(p, particles[i].position, acc, acc) == true)
		{
			return true;
		}
	}

	return false;
}

float3 Grad_ABuffer(float3 p, float4 pos)
{
	float3 grad;

	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;

	uint offset = offsetBuffer[uIndex];

	while (offset != 0)
	{
		uint2 element = linkBuffer[offset];
		offset = element.x;
		int i = element.y;

		grad = calculateGrad(p, particles[i].position, grad);
	}

	return grad;
}

class ABufferMetaballVisualizer : IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p, float4 pos)
	{
		if (functionType == 2)
		{
			WyvillMetaballTester wyvillMetaballTester;
			return MetaBallTest_ABuffer(p, pos, wyvillMetaballTester);
		}
		if (functionType == 3)
		{
			NishimuraMetaballTester nishimuraMetaballTester;
			return MetaBallTest_ABuffer(p, pos, nishimuraMetaballTester);
		}
		if (functionType == 4)
		{
			MurakamiMetaballTester murakamiMetaballTester;
			return MetaBallTest_ABuffer(p, pos, murakamiMetaballTester);
		}
		SimpleMetaballTester simpleMetaballTester;
		return MetaBallTest_ABuffer(p, pos, simpleMetaballTester);
	}

	float3 callGradientCalculator(float3 p, float4 pos)
	{
		return Grad_ABuffer(p, pos);
	}

	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos, float4 pos)
	{
		ABufferMetaballVisualizer aBufferMetaballVisualizer;

		return BinarySearch(startInside, startPos, endInside, endPos, pos, aBufferMetaballVisualizer);
	}
};