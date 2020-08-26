#include "RootSignatures.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 lightDirTS : TEXCOORD1;
    float3 viewDirTS : TEXCOORD2;
};

Texture2D diffuseTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D bumpTex : register(t2);

SamplerState sampl : register(s0);

cbuffer PerFrameCb : register(b1)
{
    float4x4 viewProj;
    float4 lightPos;
    float4 eyePos;
    float4 lightIntensity;
}

static const float HEIGHT_SCALE = 0.1f;


float DepthAt(float2 tex)
{
    return bumpTex.Sample(sampl, tex).r;
}

float2 simple_parallax(float2 tex, float3 viewDirTS)
{
    float height = DepthAt(tex) * HEIGHT_SCALE;
    float2 dxy = (viewDirTS.xy / viewDirTS.z) * height;
	return tex - dxy;
}

float2 offset_limited_parallax(float2 tex, float3 viewDirTS)
{
	float height = DepthAt(tex) * HEIGHT_SCALE;
	float2 dxy = (viewDirTS.xy) * height;
	return tex - dxy;
}

float2 layered_parallax(float2 tex, float3 viewDirTS)
{
	const float minLayers = 8.0;
	const float maxLayers = 32.0;
	float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDirTS)));
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    float2 P = (viewDirTS.xy / viewDirTS.z) * HEIGHT_SCALE;
    float2 deltaTexCoords = P / float(numLayers);

    // get initial values
    float2 currentTexCoords = tex;
    float currentDepthMapValue = DepthAt(currentTexCoords);
  
    for (int i = 0; i < (32) && currentLayerDepth < currentDepthMapValue; ++i)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = DepthAt(tex);
        currentLayerDepth += layerDepth;
    }

    return currentTexCoords;
}

float2 occlusion_parallax(float2 tex, float3 viewDirTS) {
	const float minLayers = 8.0;
	const float maxLayers = 32.0;
	float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDirTS)));
	float layerDepth = 1.0 / numLayers;
	// depth of current layer
	float currentLayerDepth = 0.0;
	// the amount to shift the texture coordinates per layer (from vector P)
	float2 P = (viewDirTS.xy) * HEIGHT_SCALE;
	float2 deltaTexCoords = P / float(numLayers);

	// get initial values
	float2 currentTexCoords = tex;
	float currentDepthMapValue = DepthAt(currentTexCoords);

	for(int i = 0; i < (64) && currentLayerDepth < currentDepthMapValue; ++i)
	{
		currentTexCoords -= deltaTexCoords;
		currentDepthMapValue = DepthAt(tex);
		currentLayerDepth += layerDepth;
	}

	float2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = DepthAt(prevTexCoords) - currentLayerDepth + layerDepth;

	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;
}

[RootSignature(NormalMapRS)]
float4 main(VSOutput vso) : SV_Target
{

    float3 v = normalize(vso.viewDirTS);
    float3 l = normalize(vso.lightDirTS);
    float2 tex = layered_parallax(vso.texCoord, v);

	/*
	if(tex.x < 0.0f || tex.x > 1.0f || tex.y < 0.0f || tex.y > 1.0f) {
		discard;
	}*/

    float3 h = normalize(l + v);
    float3 n = normalize(normalTex.Sample(sampl, tex).xyz - 0.5f);

    float ndotl = saturate(dot(n, l));
    float ndoth = saturate(dot(n, h));
    ndoth = pow(ndoth, 80);

    float3 kd = diffuseTex.Sample(sampl, tex).xyz;
    
	return float4((kd + ndoth) * ndotl, 1);
	return float4(v, 1);
}
