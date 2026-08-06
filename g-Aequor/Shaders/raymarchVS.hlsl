#include "AequorFrameCb.hlsli"

// Declares the root signature for BOTH raymarchVS and raymarchPS -- only the
// VS carries a [RootSignature(...)] attribute (matching this codebase's
// convention, e.g. g-BCC's raymarchVS.hlsl/raymarchPS.hlsl and
// particlePointVS.hlsl/particlePointPS.hlsl): a PS with its own, different
// embedded root signature would conflict with the one actually bound for
// the draw (loaded from the VS blob in AequorApp::CreateResources) and fail
// PSO creation with E_INVALIDARG. CBV(b1) is here because raymarchPS reads
// TorusListCb even though the VS itself never touches it.
#define RaymarchSig "RootFlags(0)," \
    "CBV(b0)," \
    "CBV(b1)"

struct VsOut {
    float4 pos    : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

// Full-screen triangle from SV_VertexID alone -- no vertex/index buffers.
[RootSignature(RaymarchSig)]
VsOut raymarchVS(uint vid : SV_VertexID)
{
    VsOut o;
    float2 uv  = float2((vid << 1) & 2, vid & 2);
    float2 ndc = uv * 2.0f - 1.0f;
    o.pos = float4(ndc, 0, 1);

    float4 dir = mul(float4(ndc, 1, 1), rayDirTransform);
    dir /= dir.w;
    o.rayDir = dir.xyz;
    return o;
}
