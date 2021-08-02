#include "NormalMap.hlsli"

Texture2D diffuseTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D bumpTex : register(t2);
TextureCube envTex : register(t3);

SamplerState sampl : register(s0);

[RootSignature(NormalMapRS)]
float4 main(VSOutput vso) : SV_Target
{
	float3 l = normalize(vso.lightDirTS);
	float3 v = normalize(vso.viewDirTS);
	float3 h = normalize(l + v);

	float bumpHeight = bumpTex.Sample(sampl, vso.texCoord) * 0.07f;
	float2 ptex = vso.texCoord + v.xy * bumpHeight;

	float3 n = normalize(normalTex.Sample(sampl, ptex).xyz - float3(0.5f, 0.5f, 0.0f));


	float ndotl = saturate(dot(n, l));
	float ndoth = saturate(dot(n, h));
	ndoth = pow(ndoth, 80);

	float3 kd = diffuseTex.Sample(sampl, ptex).xyz;

	float3x3 tbn = { vso.tangent, vso.binormal, vso.normal };
	float3 worldNormal = normalize(mul(n, tbn));

	return float4(
		(kd * ndotl + float3(10, 10, 10) * ndoth) * 0.9 +
		envTex.Sample(sampl, reflect(-normalize(vso.viewDir), worldNormal)).rgb * 0.1
		, 1);
}
