#include "RootSignatures.hlsli"

struct IAOutput {
	float3 position : POSITION;
	float2 tex : TEXCOORD;
};

struct VSOutput {
	float4 position : SV_Position;
	float3 rayDir : RAYDIR;
	float2 tex : TEXCOORD;
};