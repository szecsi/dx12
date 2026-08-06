#include "tree.hlsli"
#include "treeCB.hlsli"
#include "matrix.hlsli"

// Renders a single tree (model 0, instances 0..255) centered at the origin,
// with no forest layout offset/scale, for baking into a billboard texture.

ByteAddressBuffer pieces  : register(t0);
Buffer<float4>    bones   : register(t1);
ByteAddressBuffer twists  : register(t2);

struct IAOutput {
	float3 position     : POSITION;
	float3 normal       : NORMAL;
	float3 tangent      : TANGENT;
	float2 texCoord     : TEXCOORD;
	uint4  blendIndices : BLENDINDICES;
	float4 blendWeights : BLENDWEIGHT;
};

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat : packoffset(c0);
}

cbuffer PerObjectCb : register(b2) {
	float4x4 modelMat;
	float4x4 modelMatInv;
}

struct VSOutput {
	float4 position      : SV_Position;
	float4 texCoord      : TEXCOORD;
	float4 worldNormal   : NORMAL;
	float4 worldTangent  : TANGENT;
	float4 worldPosition : WORLDPOS;
};

[RootSignature(RootSigTree)]
VSOutput billboardBakeVS(IAOutput iao, uint instanceId : SV_InstanceID) {
	float twist = twists.Load(instanceId << 2) * 1.0471975511965977461542144610932;

	uint bbs[3];
	bbs[0] = pieces.Load((instanceId * 4 + 0) << 2);
	bbs[1] = pieces.Load((instanceId * 4 + 1) << 2);
	bbs[2] = pieces.Load((instanceId * 4 + 2) << 2);

	float4x4 bbx = float4x4(
		bones[bbs[0] * 4 + 0],
		bones[bbs[0] * 4 + 1],
		bones[bbs[0] * 4 + 2],
		bones[bbs[0] * 4 + 3]
	);
	bbx = mul(RotationZ(twist).m, bbx);
	bbx = mul(rigging[0], bbx);

	float4x4 bby = float4x4(
		bones[bbs[1] * 4 + 0],
		bones[bbs[1] * 4 + 1],
		bones[bbs[1] * 4 + 2],
		bones[bbs[1] * 4 + 3]
	);
	bby = mul(rigging[1], bby);

	float4x4 bbz = float4x4(
		bones[bbs[2] * 4 + 0],
		bones[bbs[2] * 4 + 1],
		bones[bbs[2] * 4 + 2],
		bones[bbs[2] * 4 + 3]
	);
	bbz = mul(rigging[2], bbz);

	float4x4 skinMat =
		iao.blendWeights.x * bbx +
		iao.blendWeights.y * bby +
		iao.blendWeights.z * bbz;

	float4 skinnedPos  = mul(float4(iao.position, 1.0f), skinMat);
	float4 skinnedNorm = mul(float4(iao.normal,   0.0f), skinMat);
	float4 skinnedTang = mul(float4(iao.tangent,  0.0f), skinMat);

	VSOutput vso;
	vso.worldPosition = mul(modelMat,    skinnedPos.yzxw);
	vso.position      = mul(viewProjMat, vso.worldPosition);
	vso.worldNormal   = mul(float4(skinnedNorm.xyz, 0.0f), modelMatInv);
	vso.worldTangent  = mul(modelMat,    float4(skinnedTang.xyz, 0.0f));
	vso.texCoord      = float4(iao.blendWeights.rgb, 1);
	if (instanceId == 255) vso.position.w = 0.0;
	return vso;
}
