// Input vertex read from the StructuredBuffer (object-space position and normal)
struct MeshInput {
    float3 position;
    float3 normal;
};

// Vertex output from mesh shader to pixel shader
struct MeshOutput {
    float4 position : SV_Position;
    float4 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
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

// t0: input vertex buffer, t1: flat index buffer (3 uints per triangle), t2: environment cubemap
StructuredBuffer<MeshInput> vertexBuffer : register(t0);
StructuredBuffer<uint>      indexBuffer  : register(t1);
TextureCube                 envMap       : register(t2);
SamplerState                sampl        : register(s0);

#define PhongTessRootSig \
    "CBV(b0)," \
    "CBV(b1)," \
    "DescriptorTable(SRV(t0, numDescriptors=3))," \
    "StaticSampler(s0)"
