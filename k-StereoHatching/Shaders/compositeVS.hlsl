#include "CompositeCb.hlsli"

// PS needs CBV(b0) + the SRV table (t0=leftStrokeMask, t1=rightStrokeMask,
// t2=leftLuminance, t3=rightLuminance, t4=leftCrossMask, t5=rightCrossMask)
// even though this VS body never touches them -- only the VS carries
// [RootSignature(...)], matching this codebase's convention (see
// raymarchVS.hlsl).
#define CompositeSig "RootFlags(0)," \
    "CBV(b0)," \
    "DescriptorTable(SRV(t0, numDescriptors=6))"

struct VsOut {
    float4 pos : SV_POSITION;
};

// Full-screen triangle from SV_VertexID alone -- no vertex/index buffers.
[RootSignature(CompositeSig)]
VsOut compositeVS(uint vid : SV_VertexID)
{
    VsOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    float2 ndc = uv * 2.0f - 1.0f;
    o.pos = float4(ndc, 0, 1);
    return o;
}
