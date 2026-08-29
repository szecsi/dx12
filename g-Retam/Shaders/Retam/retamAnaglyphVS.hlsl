// PS needs the SRV table (t0=leftEyeColor, t1=rightEyeColor) even though this
// VS body never touches it -- only the VS carries [RootSignature(...)],
// matching this codebase's convention (see extrudeCubicVS.hlsl).
#define AnaglyphSig \
    "RootFlags(0)," \
    "DescriptorTable(SRV(t0, numDescriptors=2))"

struct VsOut {
    float4 pos : SV_POSITION;
};

// Full-screen triangle from SV_VertexID alone -- no vertex/index buffers.
[RootSignature(AnaglyphSig)]
VsOut retamAnaglyphVS(uint vid : SV_VertexID)
{
    VsOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    float2 ndc = uv * 2.0f - 1.0f;
    o.pos = float4(ndc, 0, 1);
    return o;
}
