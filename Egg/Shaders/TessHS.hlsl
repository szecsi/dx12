#include "Tess.hlsli"

// Patch Constant Function
HSCOutput CalcHSPatchConstants(
	InputPatch<VSOutput, 3> ip,
	uint PatchID : SV_PrimitiveID)
{
	HSCOutput hsco;

	// Insert code to compute Output here
	hsco.EdgeTessFactor[0] =
		hsco.EdgeTessFactor[1] =
		hsco.EdgeTessFactor[2] =
		hsco.InsideTessFactor = 15; // e.g. could calculate dynamic tessellation factors instead

	return hsco;
}

[RootSignature(TessRootSig)]
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcHSPatchConstants")]
HSOutput main( 
	InputPatch<VSOutput, 3> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
	return ip[i];
}
