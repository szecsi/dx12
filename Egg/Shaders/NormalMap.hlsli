#define NormalMapRS "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT ), CBV(b0), CBV(b1), DescriptorTable(SRV(t0, numDescriptors = 4)), StaticSampler(s0)"

struct IAOutput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
	float3 lightDirTS : TEXCOORD1;
	float3 viewDirTS : TEXCOORD2;
	float4 worldPos : WORLD;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float3 viewDir : VIEWDIR;
};

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
	float4x4 modelMatInv;
}

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirTransform;
	float4 eyePos;
	float4 lightPos;
	float4 lightPowerDensity;
}
