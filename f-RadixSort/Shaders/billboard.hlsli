
#include "window.hlsli"

#define counterSize 3

struct IaosBillboard {
	float3 pos : POSITION;
	uint id : VID;
};

struct VsosBillboard
{
	float3 pos : POSITION;
	uint id : VID;
	float3 rayDir: TEXCOORD1;
};

struct GsosBillboard
{
	float4 pos : SV_Position;
	float2 tex : TEXCOORD;
	uint id : VID;
	float3 rayDir: TEXCOORD2;
};

cbuffer billboardGSMatricesCB {
	row_major float4x4 mM;
	row_major float4x4 mViewProjMI;
	row_major float4x4 mViewProjM;
	row_major float4x4 rayDirM;
};


