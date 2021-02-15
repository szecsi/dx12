struct IAOutput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

typedef IAOutput VSOutput;

struct GSOutput {
	uint renderTargetArrayIndex : SV_RenderTargetArrayIndex;
	float4 position : SV_Position;
	float4 worldPosition : WORLDPOS;
};

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirTransform;
	float4 eyePos;
	float4 lightPos;
	float4 lightPowerDensity;
	float4x4 lightViewProjMat;
	float4 lightPos2;
	float4 lightPowerDensity2;
	float4x4 lightViewProjMat2;
}