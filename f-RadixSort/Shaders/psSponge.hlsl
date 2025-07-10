SamplerState ss : register(s0);

Texture2D diffuseTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D bumpTex : register(t2);


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

cbuffer spongeCB
{
	float4 eyePos;
};

float4 psSponge(VsosTrafo input) : SV_Target
{
	float3 l = normalize(input.lightDirTS);
	float3 v = normalize(input.viewDirTS);
	float3 h = normalize(l + v);

	float bumpMax = 0.05;
	float3 step = v / v.z * bumpMax;
	step *= 0.5;
	float3 ptex = float3(input.texCoord, 0) - step;

	float bumpHeight;
	for (int i = 0; i < 16; i++) {
		step *= 0.5;
		bumpHeight = bumpTex.Sample(ss, ptex).r * bumpMax;
		if (bumpHeight < bumpMax + ptex.z)
			ptex -= step;
		else
			ptex += step;
	}

	if (ptex.z < -bumpMax * v.z * 5.5)
		discard;

	float3 n = normalize(normalTex.Sample(ss, ptex).xyz - float3(0.5f, 0.5f, 0.0f));


	float ndotl = saturate(dot(n, l));
	float ndoth = saturate(dot(n, h));
	ndoth = pow(ndoth, 80);

	float3 kd = diffuseTex.Sample(ss, ptex).xyz;

	float3x3 tbn = { input.tangent, input.binormal, input.normal };
	float3 worldNormal = normalize(mul(n, tbn));

	float3 reliefPos = input.worldPos.xyz + worldNormal * bumpHeight;
	float eyeDistance = distance(reliefPos, eyePos.xyz);

	//return float4(
	//	(kd * ndotl + float3(0.10, 0.10, 0.10) * ndoth) * 0.9
	//	//		+
	//	//		envTex.Sample(sampl, reflect(-normalize(vso.viewDir), worldNormal)).rgb * 0.1
	//			, 1);

	return float4(
		(kd * ndotl + float3(1, 1, 1) * ndoth) * 0.7 + float3(0.1f, 0.1f, 0.1f) * 0.3, eyeDistance);
}