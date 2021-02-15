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

	float bumpMax = 0.05;
	float3 step = v / v.z * bumpMax;
	step *= 0.5;
	float3 ptex = float3(vso.texCoord, 0) - step;
	for (int i = 0; i < 16; i++) {
		step *= 0.5;
		float bumpHeight = bumpTex.Sample(sampl, ptex).r * bumpMax;
		if (bumpHeight < bumpMax +ptex.z)
			ptex -= step;
		else
			ptex += step;
	}

	if (ptex.z < -bumpMax * v.z * 5.5 )
		discard;

	float3 n = normalize(normalTex.Sample(sampl, ptex).xyz - float3(0.5f, 0.5f, 0.0f));


	float ndotl = saturate(dot(n, l));
	float ndoth = saturate(dot(n, h));
	ndoth = pow(ndoth, 80);

	float3 kd = diffuseTex.Sample(sampl, ptex).xyz;

	float3x3 tbn = { vso.tangent, vso.binormal, vso.normal };
	float3 worldNormal = normalize(mul(n, tbn));

	return float4(
		(kd * ndotl + float3(0.10, 0.10, 0.10) * ndoth) * 0.9
//		+
//		envTex.Sample(sampl, reflect(-normalize(vso.viewDir), worldNormal)).rgb * 0.1
		, 1);
}


