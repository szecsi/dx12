#include "metaball.hlsli"

StructuredBuffer<uint> idBuffer;

bool MetaBallTest_SBuffer(float3 p, IMetaballTester metaballTester)
{
	float2 pos = WorldToScreen(p);

	float acc = 0.0;

	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;

	uint startIdx;
	if (uIndex > 0) {
		startIdx = offsetBuffer[uIndex - 1];
	}
	else
	{
		startIdx = 0;
	}

	uint endIdx = offsetBuffer[uIndex];

	for (uint i = startIdx; i < endIdx; i++) {
		uint j = idBuffer[i];

		if (metaballTester.testFunction(p, positions[j].xyz, acc, acc) == true)
		{
			return true;
		}
	} 

	return false;
}

bool MetaBallTest_SBufferV2(float3 p, IMetaballTester metaballTester)
{
	float2 pos = WorldToScreen(p);

	float acc = 0.0;

	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;
	uint h = 4 * (uint)(uIndex % counterSize);

	uint startIdx;
	if (uIndex > counterSize) {
		startIdx = offsetBuffer[uIndex - counterSize];
	}
	else
	{
		startIdx = 0;
	}

	uint endIdx = offsetBuffer[uIndex];

	//for (uint i = startIdx; i < endIdx; i+= counterSize) {
	//	uint j = idBuffer[i];

	//	if (metaballTester.testFunction(p, positions[j].xyz, acc, acc) == true)
	//	{
	//		return true;
	//	}
	//}
	
	return false;
}

float3 Grad_SBuffer(float3 p) {
	float2 pos = WorldToScreen(p);

	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;

	uint startIdx;
	if (uIndex > 0) {
		startIdx = offsetBuffer[uIndex - 1];
	}
	else
	{
		startIdx = 0;
	}

	uint endIdx = offsetBuffer[uIndex];

	float3 grad;
	const float r = metaBallRadius;// 0.005;

	for (int i = startIdx; i < endIdx; i++) {
		uint j = idBuffer[i];
		grad = calculateGrad(p, positions[j].xyz, grad);
	}

	return grad;
}

float3 Grad_SBufferV2(float3 p) {
	float2 pos = WorldToScreen(p);

	return float3(0.0, 0.0, 0.0);

	uint uIndex = (uint)pos.y * (uint)windowWidth + (uint)pos.x;

	uint startIdx;
	if (uIndex > 0) {
		startIdx = offsetBuffer[uIndex - 1];
	}
	else
	{
		startIdx = 0;
	}

	uint endIdx = offsetBuffer[uIndex];

	float3 grad;
	const float r = metaBallRadius;// 0.005;

	for (int i = startIdx; i < endIdx; i++) {
		uint j = idBuffer[i];
		grad = calculateGrad(p, positions[j].xyz, grad);
	}

	return grad;
}

class SBufferMetaballVisualizer : IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p)
	{
		if (functionType == 2)
		{
			WyvillMetaballTester wyvillMetaballTester;
			return MetaBallTest_SBuffer(p, wyvillMetaballTester);
		}
		if (functionType == 3)
		{
			NishimuraMetaballTester nishimuraMetaballTester;
			return MetaBallTest_SBuffer(p, nishimuraMetaballTester);
		}
		if (functionType == 4)
		{
			MurakamiMetaballTester murakamiMetaballTester;
			return MetaBallTest_SBuffer(p, murakamiMetaballTester);
		}
		SimpleMetaballTester simpleMetaballTester;
		return MetaBallTest_SBuffer(p, simpleMetaballTester);
	}

	float3 callGradientCalculator(float3 p)
	{
		return Grad_SBuffer(p);
	}

	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos)
	{
		SBufferMetaballVisualizer sBufferMetaballVisualizer;

		return BinarySearch(startInside, startPos, endInside, endPos, sBufferMetaballVisualizer);
	}
};

class SBufferV2MetaballVisualizer : IMetaballVisualizer
{
	bool callMetaballTestFunction(float3 p)
	{
		if (functionType == 2)
		{
			WyvillMetaballTester wyvillMetaballTester;
			return MetaBallTest_SBuffer(p, wyvillMetaballTester);
		}
		if (functionType == 3)
		{
			NishimuraMetaballTester nishimuraMetaballTester;
			return MetaBallTest_SBuffer(p, nishimuraMetaballTester);
		}
		if (functionType == 4)
		{
			MurakamiMetaballTester murakamiMetaballTester;
			return MetaBallTest_SBuffer(p, murakamiMetaballTester);
		}
		SimpleMetaballTester simpleMetaballTester;
		return MetaBallTest_SBufferV2(p, simpleMetaballTester);
	}

	float3 callGradientCalculator(float3 p)
	{
		return Grad_SBufferV2(p);
	}

	float3 doBinarySearch(bool startInside, float3 startPos, bool endInside, float3 endPos)
	{
		SBufferV2MetaballVisualizer sBufferMetaballVisualizer;

		return BinarySearch(startInside, startPos, endInside, endPos, sBufferMetaballVisualizer);
	}
};