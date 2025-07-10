#include "billboard.hlsli"

struct linkData
{
	uint link;
	float depth;
};

RWByteAddressBuffer offsetBuffer;
RWStructuredBuffer<linkData> linkBuffer;

void psControlMeshA(float4 pos : SV_POSITION)
{
	// Increment and get current pixel count.
	uint uPixelCount = linkBuffer.IncrementCounter();

	// Read and update Start Offset Buffer.
	uint uIndex = (uint)pos.y * (uint)fillWindowWidth + (uint)pos.x;
	uint uStartOffsetAddress = 4 * uIndex;
	uint uOldStartOffset;
	offsetBuffer.InterlockedExchange(uStartOffsetAddress, uPixelCount, uOldStartOffset);

	// Create fragment data.    
	linkData element;
	element.link = uOldStartOffset;
	element.depth = pos.z;

	// Store fragment link.
	linkBuffer[uPixelCount] = element;
}
