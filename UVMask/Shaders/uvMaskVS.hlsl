#define UVMaskRootSig "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT )"

struct IAInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
};

[RootSignature(UVMaskRootSig)]
float4 main(IAInput input) : SV_Position
{
    // Map UV [0,1] to NDC [-1,1]; flip Y because UV row 0 is top, NDC Y=+1 is top
    return float4(input.texCoord.x * 2.0 - 1.0, 1.0 - input.texCoord.y * 2.0, 0.0, 1.0);
}
