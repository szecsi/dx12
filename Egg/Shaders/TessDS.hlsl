#include "Tess.hlsli"


cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
	float4x4 modelMatInverse;
}

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirMat;
	float4 cameraPos;
	float4 lightPos;
	float4 lightPowerDensity;
	float4 billboardSize;
}

[RootSignature(TessRootSig)]
[domain("tri")]
DSOutput main(
	HSCOutput hsco,
	float3 domain : SV_DomainLocation,
	const OutputPatch<HSOutput, 3> patch)
{
	DSOutput dso;

	//float hx = (3 * domain.x - 2 * domain.x * domain.x) * domain.x;
	//float hy = (3 * domain.y - 2 * domain.y * domain.y) * domain.y;
	//float hz = (3 * domain.z - 2 * domain.z * domain.z) * domain.z;
	//float sh = hx + hy + hz;
	//hx /= sh;
	//hy /= sh;
	//hz /= sh;

	float3 p = 
		patch[0].position*domain.x
		+patch[1].position*domain.y
		+patch[2].position*domain.z;

	float3 off0 = dot(patch[0].position - p, patch[0].normal) * patch[0].normal;
	float3 off1 = dot(patch[1].position - p, patch[1].normal) * patch[1].normal;
	float3 off2 = dot(patch[2].position - p, patch[2].normal) * patch[2].normal;

	float3 off =
		off0 * domain.x
		+ off1 * domain.y
		+ off2 * domain.z;
	p += off * 0.5;

	float3 n = 
		patch[0].normal * domain.x
		+ patch[1].normal * domain.y
		+ patch[2].normal * domain.z;

	dso.worldPos = mul(modelMat, float4(p, 1.0f));
	dso.position = mul(viewProjMat, dso.worldPos);
	dso.normal = normalize(mul(float4(n, 0), modelMatInverse));

	dso.texCoord = 
		patch[0].texCoord * domain.x
		+ patch[1].texCoord * domain.y
		+ patch[2].texCoord * domain.z;

	return dso;
}
