



struct VsosTrafo
{
	float4 pos 		: SV_POSITION;
};

float4 psIdle(VsosTrafo input, out float outDepth : SV_Depth) : SV_Target
{
	outDepth = 0.001;
	return float4 (1.0, 0.0, 0.0, 1.0);
}