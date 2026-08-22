#include "SeedGenCb.hlsli"
#include "Hash.hlsli"

// t0 = luminance (R16_FLOAT, -1 sentinel = background), t1 = directionMap
// (R16G16B16A16_FLOAT: rg = screen-space unit hatch direction, b =
// screen-space signed curvature 1/px) -- both written by hatchGeometryPS.hlsl
// (Pass A). u0/u1 are plain buffer UAVs, bound as root descriptors (only the
// textures need an actual descriptor table).
#define SeedGenSig "RootFlags(0)," \
    "CBV(b0)," \
    "DescriptorTable(SRV(t0, numDescriptors=2))," \
    "UAV(u0)," \
    "UAV(u1)"

Texture2D<float>  LuminanceTex  : register(t0);
Texture2D<float4> DirectionTex  : register(t1);

struct SeedGpu {
    float2 screenPosPx;
    float2 hatchDirScreen;
    float  curvaturePx;
    float  _pad0;
};

RWStructuredBuffer<SeedGpu> SeedBuffer  : register(u0);
RWStructuredBuffer<uint>    SeedCounter : register(u1);

// One thread per jittered-grid cell -- a single dispatch, no ping-pong.
// Independent per eye via eyeSeed (a distinct constant per eye, folded into
// every hash), so the two eyes' accepted seed sets never correlate.
[RootSignature(SeedGenSig)]
[numthreads(8, 8, 1)]
void seedGenerateCS(uint3 tid : SV_DispatchThreadID)
{
    uint cellSize = max(1u, (uint)cellSizePx);
    uint cellsX = (viewportWidthPx + cellSize - 1) / cellSize;
    uint cellsY = (viewportHeightPx + cellSize - 1) / cellSize;
    if (tid.x >= cellsX || tid.y >= cellsY) return;

    uint jh0 = HashCombine(tid.x, tid.y, eyeSeed ^ 0x1u);
    uint jh1 = HashCombine(tid.x, tid.y, eyeSeed ^ 0x2u);
    float2 jitter = float2(PcgFloat01(jh0), PcgFloat01(jh1));
    float2 candidatePx = (float2(tid.xy) + jitter) * cellSizePx;

    if (candidatePx.x >= (float)viewportWidthPx || candidatePx.y >= (float)viewportHeightPx) return;

    int2 px = int2(candidatePx);
    float lum = LuminanceTex.Load(int3(px, 0));
    if (lum < 0.0) return; // background sentinel -- no character here

    // Darker (lower luminance) -> higher accept probability -> denser hatching.
    float pAccept = pow(saturate(1.0 - saturate(lum)), max(densityGamma, 0.0001)) * seedDensityScale;

    uint ah = HashCombine(tid.x, tid.y, eyeSeed ^ 0x3u);
    float u = PcgFloat01(ah);
    if (u >= pAccept) return;

    uint idx;
    InterlockedAdd(SeedCounter[0], 1, idx);
    if (idx >= maxSeeds) return;

    float4 dirC = DirectionTex.Load(int3(px, 0));
    float2 dir = dirC.xy;
    float dlen = length(dir);

    SeedGpu s;
    s.screenPosPx = candidatePx;
    s.hatchDirScreen = (dlen > 1e-5) ? (dir / dlen) : float2(1, 0);
    s.curvaturePx = dirC.z;
    s._pad0 = 0.0;
    SeedBuffer[idx] = s;
}
