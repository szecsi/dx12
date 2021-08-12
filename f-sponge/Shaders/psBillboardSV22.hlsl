#include "billboard.hlsli"

RWByteAddressBuffer offsetBuffer;
RWByteAddressBuffer countBuffer;
RWStructuredBuffer<uint> idBuffer;

void psBillboardSV22(GsosBillboard input)
{
	uint uIndex = (uint)input.pos.y * (uint)windowWidth + (uint)input.pos.x;
	uint h = 4 * (uint)(uIndex % counterSize);

	uint startIdx;
	if (uIndex > counterSize)
	{
		startIdx = offsetBuffer.Load((uIndex - counterSize) * 4);
	}
	else
	{
		startIdx = 0;
	}

	uint countIdx;
	countBuffer.InterlockedAdd((h + uIndex) * 4, 1, countIdx);
	idBuffer[h + startIdx + countIdx] = input.id;
}
