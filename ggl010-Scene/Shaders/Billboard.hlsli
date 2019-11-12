struct IAOutput {
	float3 pos : POSITION;
	float lifespan : LIFESPAN;
	float age : AGE;
};
typedef IAOutput VSOutput;
struct GSOutput {
	float4 pos : SV_Position;
	float2 tex : TEXCOORD;
	float opacity : OPACITY;
};

cbuffer PerObjectCb : register(b0) {
	float4x4 modelMat;
	float4x4 modelMatInverse;
}

cbuffer PerFrameCb : register(b1) {
	float4x4 viewProjMat;
	float4x4 rayDirMat;
	float4 cameraPos;
	float4 lightPos;
	float4 lightPowerDensity;
	float4 billboardSize;
}

#define BillboardRootSig "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT )," \
                 "CBV(b0)," \
                 "CBV(b1)," \
                 "DescriptorTable(SRV(t0, numDescriptors=1)), StaticSampler(s0)"




