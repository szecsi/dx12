#include "billboard.hlsli"

RWByteAddressBuffer offsetBuffer;
RWStructuredBuffer<uint2> linkBuffer;

void psBillboardA(GsosBillboard input)
{
	// Increment and get current pixel count.
	uint uPixelCount = linkBuffer.IncrementCounter();

	// Read and update Start Offset Buffer.
	uint uIndex = (uint)input.pos.y * (uint)windowWidth + (uint)input.pos.x;
	uint uStartOffsetAddress = 4 * uIndex;
	uint uOldStartOffset;
	offsetBuffer.InterlockedExchange(uStartOffsetAddress, uPixelCount, uOldStartOffset);

	// Create fragment data.    
	uint2 element;
	element.x = uOldStartOffset;
	element.y = input.id;

	// Store fragment link.
	linkBuffer[uPixelCount] = element;
}
