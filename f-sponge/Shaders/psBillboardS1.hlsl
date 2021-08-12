
#include "billboard.hlsli"
#include "window.hlsli"

RWByteAddressBuffer offsetBuffer;

void psBillboardS1(GsosBillboard input)
{
	uint uIndex = (uint)input.pos.y * (uint)windowWidth + (uint)input.pos.x;
	uint uDest = 4 * uIndex;
	uint uOldValue;
	offsetBuffer.InterlockedAdd(uDest, 1, uOldValue);
}
