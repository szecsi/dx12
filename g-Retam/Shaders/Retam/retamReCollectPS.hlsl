#include "Retam.hlsli"
#include "RetamCb.hlsli"

RWByteAddressBuffer counters : register(u0);

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat   : packoffset(c0);
	float4   cameraPos     : packoffset(c8);
	float4   ahead         : packoffset(c22);
	float4   time          : packoffset(c23);
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

uint4 cyclen(uint4 i, uint n)
{
	if(n & 0xffffffe0) {
		i = i.yxwz;
		n &= 0x1f;
	}
	if(n == 0)
		return i;
	uint4 o = i << n;
	n = (~n & 0x1f) + 1;
	o.y |= i.x >> n;
	o.x |= i.y >> n;
	o.w |= i.z >> n;
	o.z |= i.w >> n;
	return o;
}

[earlydepthstencil]
[RootSignature(RootSigRetamCollect)]
float4 retamReCollectPS(VSOutput input) : SV_Target{
	float3 normal = normalize(input.worldNormal.xyz);
	float3 bitangent = cross(normal, input.worldTangent.xyz);
	float3 tangent = cross(bitangent, normal);

	float3 x = input.worldPosition.xyz / input.worldPosition.w;
	float3 viewDiff = cameraPos.xyz - x;
	float3 viewDir = normalize(viewDiff);
	float3 lightDir = normalize(float3(1, 1, 1));
	float tone = clamp(dot(normal, lightDir), 0.0, 1.0) * 4.2;

	uint maxCount = 0;

	for (uint j = 4u; j > (uint)tone && j > 0u; j--) {
		float2 tex = input.texCoord.xy / input.texCoord.w / texScale[j - 1u];
		float rang = crossAngle[j - 1u] * 6.28;

		float3 h = tangent * cos(rang) + bitangent * sin(rang);
		float geom = length(cross(h, viewDir)) / dot(viewDiff, -ahead.xyz) * 5.0 / texScale[j - 1u];
        if (geom < 0.001)
            geom = 0.001;			
		float lod = -log2(geom) - 4.0;
		float ilod = floor(lod + 1000.0) - 1000.0;
		float flod = frac(lod + 1000.0);
		float2 stex = tex / exp2(ilod);
		uint level = uint(ilod + 19.0);
		uint4 bits = bitPatterns[j - 1u];
		for (uint i = 0u; i < 64u; i++) {
			float2 seedUvPos = float2(bits.xz >> 0u) / float(0xffffffff);
			float2 fromSeed = stex - seedUvPos + float2(0.5, 0.5);
			uint2 quadId = uint2(fromSeed);
			quadId <<= level;
			uint4 bs = cyclen(bits, (level + 96) % 64);
			quadId |= bs.xz & (0xffffffff >> (32 - level));
            uint2 hash2 = quadId ^ (quadId >> 6) ^ (quadId >> 12) ^ (quadId >> 18) ^ (quadId >> 24);
			uint sid = ((quadId.x & 0xffff0) << 12)  | ((quadId.y & 0xffff0) >> 4);
			uint hash = ((j-1u) << 12) | ((hash2.x & 0x3f) << 6)  | (hash2.y &  0x3f);

			float2 strokeTexPos = frac(fromSeed) - float2(0.5, 0.5);
			uint2 quadrant = uint2(
				(frac(fromSeed.x * 0.5) > 0.5) ? 1u : 0u,
				(frac(fromSeed.y * 0.5) > 0.5) ? 1u : 0u
			);

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
			if (	strokeTexPos.x > -0.5 &&
					strokeTexPos.y > -0.5 &&
					strokeTexPos.x <  0.5 &&
					strokeTexPos.y <  0.5) {
				
                //    if ((hash /*& 0xfff*/) == 0x2ff0)
                //    {
				//		return float4((level + 100) % 2, ((level + 100) % 3) * 0.5, ((level + 100) % 5) * 0.25, 1);
                //    }
				//hash = quadId >> 26;
				if(alpha < 0.05)
					return float4(1, 0, 0, 1);
				if(any(quadId.xy == uint2(0, 0))){
					return float4(1, 0, 1, 1);
				}
				//if(level > 14)
				//	return float4(1, 0, 0, 1);
                uint bithigh = firstbithigh(quadId.xy);
				if(bithigh > 32)
					return float4(1, 1, 1, 1);					
				if(bithigh > 19)
					return float4(0, 0, 1, 1);
				return float4(firstbithigh(quadId.xy) / 32.0, 0, 1);
				return float4(
					float((hash >> 0) & 0xf) / 15.0,
					float((hash >> 4) & 0xf) / 15.0,
					float((hash >> 8) & 0xf) / 15.0, 1);
					
				if(alpha > 0.05){
					uint count = counters.Load(hash << 2);
					if (count > maxCount)
						maxCount = count;
				}
			}
			bits = cycle(bits);
		}
	}

	uint y = input.position.y;
    return float4(0, float((y/4) % 2)  * 0.2, 0, 1);
		
	// heat map: black -> blue -> cyan -> green -> yellow -> red -> white
	float t = saturate((float)maxCount / 1024.0);
	float3 col;
	if (maxCount == 0)
		col = float3(0, 0, 0);
	else if (t < 0.25)
		col = lerp(float3(0, 0, 0.5), float3(0, 0.5, 1), t * 4.0);
	else if (t < 0.5)
		col = lerp(float3(0, 0.5, 1), float3(0, 1, 0), (t - 0.25) * 4.0);
	else if (t < 0.75)
		col = lerp(float3(0, 1, 0), float3(1, 1, 0), (t - 0.5) * 4.0);
	else
		col = lerp(float3(1, 1, 0), float3(1, 0, 0), (t - 0.75) * 4.0);

	// white for overflow (>= 1024)
	if (maxCount >= 1024)
		col = float3(1, 1, 1);

	return float4(col, 1);
}
