#include "RootSignatures.hlsli"
#include "cbBasic.hlsli"

Texture2D txt : register(t0);
TextureCube env : register(t1);
Texture2DArray shadowMap : register(t2);
SamplerState sampl : register(s0);
SamplerComparisonState shadowMapSampl : register(s1);

struct LightSource {
	float4 lightPos;
	float4 lightPowerDensity;
	float4x4 lightViewProjMat;
};

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirTransform;
	float4 eyePos;
	LightSource lights[2];
//	float4 lightPos;
//	float4 lightPowerDensity;
//	float4x4 lightViewProjMat;
//	float4 lightPos2;
//	float4 lightPowerDensity2;
//	float4x4 lightViewProjMat2;
}

[RootSignature(RootSigShadowed)]
float4 main(VSOutput input) : SV_Target
{
	float3 normal = normalize(input.normal.xyz);
	float3 viewDir = normalize(eyePos.xyz -
	input.worldPosition.xyz / input.worldPosition.w);

	float3 reflDir = reflect(-viewDir, normal);
	float3 contrib = float3(0, 0, 0);
	for (int iLight = 0; iLight < 2; iLight++) {
		float4 shadowPos = mul(lights[iLight].lightViewProjMat, input.worldPosition);
		float2 shadowUV = shadowPos.xy / shadowPos.w;
		shadowUV.y *= -1;
		shadowUV += float2(1, 1);
		shadowUV *= 0.5;
		//return //float4(shadowUV, 0, 1);
		float shadowDist = shadowMap.SampleLevel(sampl, float3(shadowUV, iLight), 0).y;

		float3 lightDiff = lights[iLight].lightPos.xyz - input.worldPosition.xyz * lights[iLight].lightPos.w;
		float lightDist = length(lightDiff);
		float3 lightDir = lightDiff / lightDist;
		float3 halfway = normalize(viewDir + lightDir);
		float visibility = 1;
		if (shadowDist + 0.1 < lightDist)
			visibility = 0;
		//float visibility = shadowMap.SampleCmpLevelZero(shadowMapSampl, float3(shadowUV, 1), lightDist );
		//return 1.0 - saturate(visibility.xxxx * 0.001);
		contrib += float3(
			visibility * lights[iLight].lightPowerDensity.rgb / (lightDist * lightDist) *
			// pow(dot(lightMain, lightDir), spotExponent)
			(
				txt.Sample(sampl, input.texCoord).rgb
				* saturate(dot(normal, lightDir)) +
				float3(5, 5, 5)
				* pow(saturate(dot(normal, halfway)), 55.0)
			 ));
		}
	return float4(contrib + 
		env.Sample(sampl, reflDir).rgb * 0.1, 1);

}
