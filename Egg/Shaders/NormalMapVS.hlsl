#include "NormalMap.hlsli"



[RootSignature(NormalMapRS)]
VSOutput main(IAOutput iao)
{
	float4 worldPos = mul(modelMat, float4(iao.position, 1.0f));
	float3 descartesPos = worldPos.xyz / worldPos.w;

	float3 lightDir = normalize(lightPos.xyz - descartesPos * lightPos.w);
	float3 viewDir = normalize(eyePos.xyz - descartesPos);

	float3 t = normalize(mul(float4(iao.tangent, 0.0f), modelMatInv).xyz);
	float3 b = normalize(mul(float4(iao.binormal, 0.0f), modelMatInv).xyz);
	float3 n = normalize(mul(float4(iao.normal, 0.0f), modelMatInv).xyz);
	float3x3 tbn = { t, -b, n };

	VSOutput vso;
	vso.viewDir = viewDir;
	vso.worldPos = worldPos;
	vso.position = mul(viewProjMat, worldPos);
	vso.normal = n;
	vso.tangent = t;
	vso.binormal = -b;
	vso.texCoord = iao.texCoord * 4.0f;
	vso.lightDirTS = mul(tbn, lightDir);
	vso.viewDirTS = mul(tbn, viewDir);

	return vso;
}
