#include "Retam.hlsli"
#include "RetamCb.hlsli"

Texture2D strokeTexture : register(t0);
SamplerState sampl : register(s0);

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat   : packoffset(c0);
	float4   cameraPos     : packoffset(c8);
	float4   ahead         : packoffset(c22);
}

struct VSOutput {
	float4 position      : SV_Position;
	float4 texCoord      : TEXCOORD;
	float4 worldNormal   : NORMAL;
	float4 worldTangent  : TANGENT;
	float4 worldPosition : WORLDPOS;
};

static const uint4 bitPatterns[4] = {
	uint4(0x0625DF73, 0xD1B84B45, 0x152AD8D0, 0x8CFD9C7C),
	uint4(0x179FB4DD, 0x612A0719, 0x0EA42B8D, 0x133CB7EC),
	uint4(0x1771BDB7, 0xEC44A092, 0x0F54D9E8, 0x83C88DDD),
	uint4(0x05E97738, 0x48B16C5F, 0x1CBD8666, 0x8A7EAA07)
};

uint4 cycle(uint4 i) {
	uint4 o = i << 1u;
	o.y |= i.x >> 31u;
	o.x |= i.y >> 31u;
	o.w |= i.z >> 31u;
	o.z |= i.w >> 31u;
	return o;
}

[RootSignature(RootSigRetam)]
float4 retam256PS(VSOutput input) : SV_Target
{
	float3 normal = normalize(input.worldNormal.xyz);
	float3 bitangent = cross(normal, input.worldTangent.xyz);
	float3 tangent = cross(bitangent, normal);

	float3 x = input.worldPosition.xyz / input.worldPosition.w;
	float3 viewDiff = cameraPos.xyz - x;
	float3 viewDir = normalize(viewDiff);
	float3 lightDir = normalize(float3(1, 1, 1));
	float tone = clamp(dot(normal, lightDir), 0.0, 1.0) * 4.2;

	float4 fragmentColor = float4(normal, 1);

	for (uint j = 4u; j > (uint)tone && j > 0u; j--) {
		float2 tex = input.texCoord.xy / input.texCoord.w / texScale[j - 1u];
		float rang = crossAngle[j - 1u] * 6.28;

		float3 h = tangent * cos(rang) + bitangent * sin(rang);
		float geom = length(cross(h, viewDir)) / dot(viewDiff, -ahead.xyz) * 5.0 / texScale[j - 1u];
        if (geom < 0.001)
            geom = 0.001;			
		float lod = -log2(geom) - 4.0;
		//fragmentColor += float4(lod-4.0, lod -5.0, lod  - 6.0, 0);
			//return fragmentColor;
		float ilod = floor(lod + 1000.0) - 1000.0;
		float flod = frac(lod + 1000.0);
		float2 stex = tex / exp2(ilod);
		uint4 bits = bitPatterns[j - 1u];
		for (uint i = 0u; i < 64u; i++) {
			float2 seedUvPos = float2(bits.xz >> 0u) / float(0xffffffff);

			float2 fromSeed = stex - seedUvPos + float2(100.5, 100.5);
			float2 strokeTexPos = frac(fromSeed) - float2(0.5, 0.5);
			uint2 quadrant = uint2(
				(frac(fromSeed.x * 0.5) > 0.5) ? 1u : 0u,
				(frac(fromSeed.y * 0.5) > 0.5) ? 1u : 0u
			);
			//fragmentColor = float4(quadrant.x, quadrant.y, 0.3, 1);
			//return fragmentColor;

            float alpha = 
                smoothstep(
					lerp(0.0, float(i) / 64.0, fading.x),
					lerp(1.0, float(i + 1u) / 64.0, fading.x),
					float(j) - tone
				);

			if (((bits.y & 1u) != quadrant.x) ||
				((bits.w & 1u) != quadrant.y)) {
                    alpha *=
                    1.0 - smoothstep(
					lerp(0.0, float(i) / 64.0, fading.y),
					lerp(1.0, float(i + 1u) / 64.0, fading.y),
					sqrt(sqrt(flod))
				);
			}
				
			strokeTexPos = float2(
				strokeTexPos.x * cos(rang) + strokeTexPos.y * sin(rang),
				-strokeTexPos.x * sin(rang) + strokeTexPos.y * cos(rang));
			strokeTexPos *= float2(1.5, 20.0) / lineSize.xy * texScale[j - 1u] * texScale[j - 1u] / exp2(flod);
			if (strokeTexPos.x > -0.5 &&
				strokeTexPos.y > -0.5 &&
				strokeTexPos.x <  0.5 &&
				strokeTexPos.y <  0.5)
			{
				float4 c = 
						//float4(1, 1, 0.9, 1); 
					strokeTexture.Sample(sampl, strokeTexPos.y + float2(0.5, 0.5));
				c.a = 1.0 - c.b;
				c.a *= alpha;
				fragmentColor = fragmentColor * (1.0 - c.a) + c * c.a;
			}
			bits = cycle(bits);
		}
	}
	return fragmentColor;
}
