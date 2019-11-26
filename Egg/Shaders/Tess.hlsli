struct IAOutput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

typedef IAOutput VSOutput;
typedef IAOutput HSOutput;

struct DSOutput {
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD;
};

struct HSCOutput
{
	float EdgeTessFactor[3]			: SV_TessFactor;
	float InsideTessFactor : SV_InsideTessFactor;
};

#define TessRootSig "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT )," \
                 "CBV(b0)," \
                 "CBV(b1)," \
                 "DescriptorTable(SRV(t0, numDescriptors=2)), StaticSampler(s0)"

