#include "PhongTess.hlsli"

[RootSignature(PhongTessRootSig)]
float4 main(MeshOutput input) : SV_Target
{
    float3 viewDir  = normalize(cameraPos.xyz - input.worldPos.xyz);
    float3 normal   = normalize(input.normal);
    float3 reflDir  = reflect(-viewDir, normal);
    
    float3 rl = sin( atan2(reflDir, reflDir.yzx) * 10.0 /* 3.14159  */);
    
    return envMap.Sample(sampl, reflDir) * 0.01 + float4(rl, 1.0);
}
