#include "tree.hlsli"
#include "treeCB.hlsli"
#include "matrix.hlsli"

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
VSOutput treeVS(IAOutput iao, uint instanceId : SV_InstanceID) {
/*	static const float B = 6.0f * nMinPiecesPerTree - 1.0f;
	uint modelIndex = (uint)((-B + sqrt(B * B + 24.0f * (float)instanceId)) * 0.5f);
	if (((modelIndex + 1u) * (2u * nMinPiecesPerTree + modelIndex / 3u)) / 2u <= instanceId)
		modelIndex++;
		
    uint modelOffset =
        (
        modelIndex * 
        (2 * nMinPiecesPerTree + (modelIndex - 1) / 3 )
    ) / 2;
	while(instanceId < modelOffset){
        modelIndex--;
		modelOffset =
			(
			modelIndex * 
			(2 * nMinPiecesPerTree + (modelIndex - 1) / 3 )
			) / 2;			
    }
    uint nextModelOffset =
        (
        (modelIndex + 1) * 
        (2 * nMinPiecesPerTree + modelIndex / 3 )
    ) / 2;		
	while(instanceId >= nextModelOffset){
        modelIndex++;
		nextModelOffset =
			(
			(modelIndex + 1) * 
			(2 * nMinPiecesPerTree + modelIndex / 3 )
		) / 2;		
    }	*/

	uint treeIndex = instanceId / 256;
	uint modelIndex = treeIndex%(32*27);
    uint modelOffset = modelIndex * 256;
	uint boneOffset = modelOffset * 2;

    float twist = twists.Load(instanceId << 2) * 1.0471975511965977461542144610932;//(6.28318530718 / 6.0);
		
	uint bbs[3];
	bbs[0] = pieces.Load((instanceId * 4 + 0) << 2);
	bbs[1] = pieces.Load((instanceId * 4 + 1) << 2);
	bbs[2] = pieces.Load((instanceId * 4 + 2) << 2);

	float4x4 bbx = float4x4(
		bones[(boneOffset + bbs[0/*iao.blendIndices.x*/])*4 + 0],
		bones[(boneOffset + bbs[0/*iao.blendIndices.x*/])*4 + 1],
		bones[(boneOffset + bbs[0/*iao.blendIndices.x*/])*4 + 2],
		bones[(boneOffset + bbs[0/*iao.blendIndices.x*/])*4 + 3]
	);
	//bbx = rigging[0];
    bbx = mul(RotationZ(twist).m, bbx);
	bbx = mul(rigging[0], bbx);		
	float4x4 bby = float4x4(
		bones[(boneOffset + bbs[1/*iao.blendIndices.y*/])*4 + 0],
		bones[(boneOffset + bbs[1/*iao.blendIndices.y*/])*4 + 1],
		bones[(boneOffset + bbs[1/*iao.blendIndices.y*/])*4 + 2],
		bones[(boneOffset + bbs[1/*iao.blendIndices.y*/])*4 + 3]
	);
	//bby = rigging[1];
	bby = mul(rigging[1], bby);		

	float4x4 bbz = float4x4(
		bones[(boneOffset + bbs[2/*iao.blendIndices.z*/])*4 + 0],
		bones[(boneOffset + bbs[2/*iao.blendIndices.z*/])*4 + 1],
		bones[(boneOffset + bbs[2/*iao.blendIndices.z*/])*4 + 2],
		bones[(boneOffset + bbs[2/*iao.blendIndices.z*/])*4 + 3]
	);	
	//bbz = rigging[2];
	bbz = mul(rigging[2], bbz);	

        float4x4 skinMat = //float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
		iao.blendWeights.x * bbx +
		iao.blendWeights.y * bby +
		iao.blendWeights.z * bbz;

	float haltonX = 0.0f, haltonZ = 0.0f;
	uint hn = treeIndex + 1u;
	float hf = 1.0f;
	[loop] while (hn > 0u) { hf *= 0.5f;  haltonX += (hn & 1u) ? hf : 0.0f; hn >>= 1u; }
	hn = treeIndex + 1u; hf = 1.0f;
	[loop] while (hn > 0u) { hf /= 3.0f;  haltonZ += (hn % 3u) * hf; hn /= 3u; }

	float4 skinnedPos  = mul(float4(iao.position, 1.0f), skinMat);
	skinnedPos.xyz *= (treeIndex % 5) + 1;
	skinnedPos.x += (haltonX - 0.5f) * 1000.0;
	skinnedPos.y += (haltonZ - 0.5f) * 1000.0;
	float4 skinnedNorm = mul(float4(iao.normal,   0.0f), skinMat);
//		skinnedNorm = float4(iao.normal,   0.0f);
	float4 skinnedTang = mul(float4(iao.tangent,  0.0f), skinMat);

	//if(instanceId == 165888)
	//	skinnedPos.z -= 100.0;
		
	VSOutput vso;
	vso.worldPosition = mul(modelMat,    skinnedPos.yzxw);
	vso.position      = mul(viewProjMat, vso.worldPosition);
	vso.worldNormal   = mul(float4(skinnedNorm.xyz, 0.0f), modelMatInv);
	vso.worldTangent  = mul(modelMat,    float4(skinnedTang.xyz, 0.0f));
	vso.texCoord      =  //float4(iao.texCoord, 0.0f, 1.0f);
			float4(iao.blendWeights.rgb,
				//iao.blendWeights[iao.blendIndices.x],
				//iao.blendWeights[iao.blendIndices.y],
				//iao.blendWeights[iao.blendIndices.z],
		1);
	if(instanceId % 256 == 255) vso.position.w = 0.0;
	return vso;
}

