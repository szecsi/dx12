#include "NormalMap.hlsli"

Texture2D diffuseTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D bumpTex : register(t2);
TextureCube envTex : register(t3);

SamplerState sampl : register(s0);

[RootSignature(NormalMapRS)]
float4 main(VSOutput vso) : SV_Target
{
	//return float4(vso.normal, 1);
	float3 n = normalize(normalTex.Sample(sampl, vso.texCoord).xyz - float3(0.5f, 0.5f, 0.0f));
	//float3 n = normalTex.Sample(sampl, vso.texCoord).xyz;
//	return float4(abs(n), 1);
	float3 l = normalize(vso.lightDirTS);
	float3 v = normalize(vso.viewDirTS);
	float3 h = normalize(l + v);

	float ndotl = saturate(dot(n, l));
	float ndoth = saturate(dot(n, h));
	ndoth = pow(ndoth, 80);

	float3 kd = diffuseTex.Sample(sampl, vso.texCoord).xyz;

	float3x3 tbn = { vso.tangent, vso.binormal, vso.normal };
	float3 worldNormal = normalize(mul(n, tbn));

	return float4(
		(kd * ndotl + float3(10, 10, 10) * ndoth) * 0.9 +
		envTex.Sample(sampl, reflect(-normalize(vso.viewDir), worldNormal)).rgb * 0.1
		, 1);
}
