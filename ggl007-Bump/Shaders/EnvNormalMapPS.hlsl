#include "RootSignatures.hlsli"




struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 texCoord : TEXCOORD;
    float3 viewDir : VIEWDIR;
    float3 lightDir : LIGHTDIR;
};


Texture2D diffuseTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D bumpTex : register(t2);
TextureCube envTex : register(t3);


SamplerState sampl : register(s0);

cbuffer PerFrameCb : register(b1)
{
    float4x4 viewProj;
    float4 lightPos;
    float4 eyePos;
    float4 lightIntensity;
}

[RootSignature(NormalMapRS)]
float4 main(VSOutput vso) : SV_Target
{
    
    float3 t = normalize(vso.tangent);
    float3 b = normalize(vso.binormal);
    float3 n = normalize(vso.normal);

    float3x3 tbn = { t, b, n };
    tbn = transpose(tbn);

    float3 normalSample = normalize(normalTex.Sample(sampl, vso.texCoord).xyz - 0.5f);

	float3 surfaceNormal = n;
    n = mul(tbn, normalSample);



    float3 l = normalize(vso.lightDir);
    float3 v = normalize(vso.viewDir);

    float3 h = normalize(l + v);
    float ndoth = saturate(dot(n, h));
    ndoth = pow(ndoth, 80);

	if(dot(surfaceNormal, n) < 0.85f) {
		float ndotl = dot(n, l);
		float3 kd = diffuseTex.Sample(sampl, vso.texCoord).xyz;
		return float4((kd + ndoth) * ndotl, 1);
	}

    float3 reflectedRay = reflect(-v, n);
    
    return envTex.Sample(sampl, reflectedRay) + ndoth;
}
