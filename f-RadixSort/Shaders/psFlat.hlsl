



struct VsosTrafo
{
	float4 pos	: SV_POSITION;
	float3 norm	: NORMAL;
};

float4 psFlat(VsosTrafo input, out float outDepth : SV_Depth) : SV_Target
{
	float3 dir = normalize(float3(-1,1,1));
	outDepth = 0.001;
	return float4 (float3(1.0, 0.0, 0.0) * (abs(dot (dir, input.norm)) + 0.2), 1.0);
	//return float4 (input.norm, 1.0);
}