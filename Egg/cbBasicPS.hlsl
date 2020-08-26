struct VSOutput {
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

float4 main(VSOutput input) : SV_Target
{
	return float4(input.texCoord, 0, 1);
}
