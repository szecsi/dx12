#include "billboard.hlsli"

RWByteAddressBuffer offsetBuffer;
RWByteAddressBuffer countBuffer;
RWStructuredBuffer<uint> idBuffer;

void psBillboardS2(GsosBillboard input)
{
	uint uIndex = (uint)input.pos.y * (uint)windowWidth + (uint)input.pos.x;

	uint startIdx;
	if (uIndex > 0)
	{
		startIdx = offsetBuffer.Load((uIndex - 1) * 4);
	}
	else
	{
		startIdx = 0;
	}

	uint countIdx;
	countBuffer.InterlockedAdd(uIndex * 4, 1, countIdx);
	idBuffer[startIdx + countIdx] = input.id;
}
