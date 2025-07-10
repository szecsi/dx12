#include "particle.hlsli"
#include "pbd.hlsli"

StructuredBuffer<float4> controlPositions;

cbuffer modelViewProjCB
{
	row_major float4x4 modelMatrix;
	row_major float4x4 modelViewProjMatrixInverse;
	row_major float4x4 modelMatrixInverse;
	row_major float4x4 modelViewProjMatrix;
};

cbuffer spongeCB
{
	float4 eyePos;
};


struct IaosTrafo
{
	float4 pos : POSITION;
	float2 texCoord : TEXCOORDS;
	uint particleId: PARTICLEID;
	float3 neighbourIds : NEIGHBOUR;
	float2 neighbourTex0: FIRSTTEX;
	float2 neighbourTex1: SECONDTEX;
	float2 neighbourTex2: THIRDTEX;
};

struct VsosTrafo
{
	float3 viewDir: VIEWDIR;
	float4 worldPos: WORLDPOS;
	float4 pos : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 texCoord : TEXCOORDS;
	float3 lightDirTS: LIGHTDIRTS;
	float3 viewDirTS: VIEWDIRTS;
};

float3 calculateNormal(float3 p0, float3 p1, float3 p2) {
	float3 v1 = p1 - p0;
	float3 w1 = p2 - p0;

	float3 normal;
	normal.x = v1.y * w1.z - v1.z * w1.y;
	normal.y = v1.z * w1.x - v1.x * w1.z;
	normal.z = v1.x * w1.y - v1.y * w1.x;

	return normalize(normal);
}

float3 calculateBinormal(float3 p0, float3 p1, float3 p2, float2 t0, float2 t1, float2 t2) {
	float3 edge1 = p1 - p0;
	float3 edge2 = p2 - p0;
	float2 edge1uv = t1 - t0;
	float2 edge2uv = t2 - t0;

	float3 binormal;

	float cp = edge1uv.y * edge2uv.x - edge1uv.x * edge2uv.y;

	if (cp != 0.0f) {
		float mul = 1.0f / cp;
		binormal = (edge1 * -edge2uv.x + edge2 * edge1uv.x) * mul;

		binormal = normalize(binormal);
	}
	return binormal;
}

float3 calculateTangent(float3 p0, float3 p1, float3 p2, float2 t0, float2 t1, float2 t2) {
	float3 edge1 = p1 - p0;
	float3 edge2 = p2 - p0;
	float2 edge1uv = t1 - t0;
	float2 edge2uv = t2 - t0;

	float3 tangent;

	float cp = edge1uv.y * edge2uv.x - edge1uv.x * edge2uv.y;

	if (cp != 0.0f) {
		float mul = 1.0f / cp;
		tangent = (edge1 * -edge2uv.y + edge2 * edge1uv.y) * mul;

		tangent = normalize(tangent);
	}
	return tangent;
}

VsosTrafo vsSponge(IaosTrafo input, uint vid : SV_VertexID)
{
	VsosTrafo output = (VsosTrafo)0;

	float4 worldPos = mul(modelMatrix, float4(controlPositions[input.particleId].xyz, 1.0f));
	float3 descartesPos = worldPos.xyz / worldPos.w;

	float4 lightPos = (50.0, 100, 200, 1.0);


	float3 viewDir = normalize(eyePos.xyz - descartesPos);

	float3 lightDir = viewDir;//float3(-1.0, 1.0, -1.0);// normalize(lightPos.xyz - descartesPos * lightPos.w);

	float3 normal = calculateNormal(controlPositions[input.neighbourIds.x].xyz, controlPositions[input.neighbourIds.y].xyz, controlPositions[input.neighbourIds.z].xyz);
	float3 binormal = calculateBinormal(controlPositions[input.neighbourIds.x].xyz, controlPositions[input.neighbourIds.y].xyz, controlPositions[input.neighbourIds.z].xyz,
		input.neighbourTex0, input.neighbourTex1, input.neighbourTex2);
	float3 tangent = calculateTangent(controlPositions[input.neighbourIds.x].xyz, controlPositions[input.neighbourIds.y].xyz, controlPositions[input.neighbourIds.z].xyz,
		input.neighbourTex0, input.neighbourTex1, input.neighbourTex2);

	//normal = normal.xyz / 2.0 + float3(0.5, 0.5, 0.5);
	//binormal = binormal.xyz / 2.0 + float3(0.5, 0.5, 0.5);
	//tangent = tangent.xyz / 2.0 + float3(0.5, 0.5, 0.5);

	float3 t = normalize(mul(float4(tangent, 0.0f), modelMatrixInverse).xyz);
	float3 b = normalize(mul(float4(binormal, 0.0f), modelMatrixInverse).xyz);
	float3 n = normalize(mul(float4(normal, 0.0f), modelMatrixInverse).xyz);
	float3x3 tbn = { t, -b, n };

	output.viewDir = viewDir;
	output.worldPos = worldPos;
	output.pos = mul(worldPos, modelViewProjMatrix);
	output.normal = n;
	output.tangent = t;
	output.binormal = b;
	output.texCoord = input.texCoord;
	output.lightDirTS = mul(tbn, lightDir);
	output.viewDirTS = mul(tbn, viewDir);

	return output;
}
