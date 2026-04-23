#include "Retam.hlsli"

Buffer<float4> design : register(t0);

struct GsisDummy
{
};

struct GsosExtrude
{
	float4 pos			: SV_Position;
	float2 tex			: TEXCOORD;
	float2 weight		: WEIGHT;
};

[maxvertexcount(32)]
void extrudeGS(point GsisDummy dummy[1], uint pid : SV_PrimitiveID, inout TriangleStream<GsosExtrude> stream)
{
	float4 xtp  =		design[pid *5 + 0];
	float4 ytp  =		design[pid *5 + 1];
	float4 t0_3 =		design[pid *5 + 2];
	float4 t3_6 =		design[pid *5 + 3];
	float4 extrema =	design[pid *5 + 4];

	if(t0_3.x < 16)
		return;

	float confidence = saturate((t0_3.x - 16) / 16);

	float4x4 des = float4x4(float4(t0_3), float4(t0_3.yzw, t3_6.y), float4(t0_3.zw, t3_6.yz), t3_6);

//	float detr = determinant(des);
//	if( detr < 0.05 )
//		return;
//	confidence *= saturate((detr - 1.5 / 3);

	float4 cubicX = 0;
	{
		float4 r = xtp;
		float4 p = r;	
		for(uint i=0; i<4; i++)
		{
			float4 apk = mul(des, p);
			float r2 = dot(r, r);
			float alpha =  r2 / dot(p, apk);
//			if(alpha > 100)
//				return;
			cubicX += p * alpha;
			r -= alpha * apk;
			p = r + p * dot(r, r) / r2;
		}
	}

	float4 cubicY = 0;
	{
		float4 r = ytp;
		float4 p = r;
		for(uint i=0; i<4; i++)
		{
			float4 apk = mul(des, p);
			float r2 = dot(r, r);
			float alpha =  r2 / dot(p, apk);
//			if(alpha > 100)
//				return;
			cubicY += p * alpha;
			r -= alpha * apk;
			p = r + p * dot(r, r) / r2;
		}
	}

	float4 cubicV = 0;
	{
		float4 tx, tn;
		tx.x = extrema.y;
		tx.y = tx.x * tx.x;
		tx.z = tx.y * tx.x;
		tx.w = tx.z * tx.x;
		tn.x = 1-extrema.x;
		tn.y = tn.x * tn.x;
		tn.z = tn.y * tn.x;
		tn.w = tn.z * tn.x;
		float4x4 tix = 
			float4x4 ( 
			tx - tn,
			tx * tx.x - tn * tn.x,
			tx * tx.y - tn * tn.y,
			tx * tx.z - tn * tn.z
			);
		des += float4x4 ( 
			float4(1.0,   1.0/2, 1.0/3, 1.0/4),
			float4(1.0/2, 1.0/3, 1.0/4, 1.0/5),
			float4(1.0/3, 1.0/4, 1.0/5, 1.0/6),
			float4(1.0/4, 1.0/5, 1.0/6, 1.0/7)
			) * tix * t0_3.x;
		float4 r = t0_3;
		float4 p = r;
		for(uint i=0; i<4; i++)
		{
			float4 apk = mul(des, p);
			float r2 = dot(r, r);
			float alpha =  r2 / dot(p, apk);
//			if(alpha > 100)
//				return;
			cubicV += p * alpha;
			r -= alpha * apk;
			p = r + p * dot(r, r) / r2;
		}
	}
	float dx = dot(cubicX, float4(1, extrema.y, extrema.y*extrema.y, extrema.y*extrema.y*extrema.y))
		- dot(cubicX, float4(1, (1 - extrema.x), (1 - extrema.x)*(1 - extrema.x), (1 - extrema.x)*(1 - extrema.x)*(1 - extrema.x)));
	float dy = dot(cubicY, float4(1, extrema.y, extrema.y*extrema.y, extrema.y*extrema.y*extrema.y))
		- dot(cubicY, float4(1, (1 - extrema.x), (1 - extrema.x)*(1 - extrema.x), (1 - extrema.x)*(1 - extrema.x)*(1 - extrema.x)));
	if (dx*dx + dy*dy > 5000)
		return;

	confidence *= saturate((extrema.y - (1 - extrema.x)) * 10);
	float overdraw = 1.0 + ((pid * 0x2da78e85) >> 24) / 255.0 * 0.2;
	for(uint i=0; i<16; i++)
	{
		float t = 1-extrema.x + (extrema.y - (1-extrema.x)) * (i /15.0 * overdraw - (overdraw - 1.0) * 0.5);
//		float t = i/15.0;
		float4 tPowers = float4(1, t, t*t, t*t*t);
		GsosExtrude output;

		output.weight = confidence * dot(cubicV, tPowers);
		output.weight.y = ((pid * 0x2da78e85) >> 24) / 255.0;
		output.tex.x = ((pid & (0x3)) ^ 0x3)?0:1;
		output.tex.y = dot(cubicV, tPowers);
		output.pos.xy = float2(dot(cubicX, tPowers), dot(cubicY, tPowers));
		output.pos.y += 5.15;
		float2 tangent = float2(dot(cubicX.yzw * float3(1,2,3), tPowers.xyz), dot(cubicY.yzw * float3(1, 2, 3), tPowers));

		tangent = normalize(tangent);
		float2 normal = tangent.yx;

		output.pos.xy /= 512.0;
		output.pos.xy -= float2(1, 1) + normal * 0.005 + sin(i) * output.weight.y * 0.02;
		output.pos.y = -output.pos.y;
		output.pos.z = 0.5;
		output.pos.w = 1;
		output.tex = float2(0, i/15.0);
		stream.Append(output);
		output.pos.xy += normal * 0.01;
		output.tex = float2(1, i/15.0);
		stream.Append(output);
	}
}
