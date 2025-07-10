
cbuffer testMeshCB
{
	float4 eyePos;
};

struct VsosTrafo
{
	float4 pos	: SV_POSITION;
	float4 worldPos : WORLDPOS;
	float3 norm	: NORMAL;
};

float4 psTestMesh(VsosTrafo input) : SV_Target
{
	//return float4 (1.0, 0.0, 0.0, 1.0);
	return float4 (input.norm, distance (input.worldPos.xyz, eyePos.xyz));
}