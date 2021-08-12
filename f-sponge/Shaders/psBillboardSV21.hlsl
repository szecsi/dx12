
#include "billboard.hlsli"
#include "window.hlsli"

RWByteAddressBuffer offsetBuffer;
RWByteAddressBuffer counterBuffer;

void psBillboardSV21(GsosBillboard input)
{
	uint uIndex = (uint)input.pos.y * (uint)windowWidth + (uint)input.pos.x;
	uint h = 4 * (uint)(uIndex % counterSize);
	uint uDest = 4 * uIndex;
	uint uOldValueOffset, uOldValueCounter;
	offsetBuffer.InterlockedAdd(uDest, 1, uOldValueOffset);
	counterBuffer.InterlockedAdd(h, 1, uOldValueCounter);
}
