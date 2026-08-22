#include "StrokeFrameCb.hlsli"

// SeedBuffer/SeedCounter are plain structured buffers, so they bind as root
// SRVs directly (no descriptor table needed -- only Texture2D SRVs require
// one). Both were transitioned UAV->SRV by the caller after Pass B.
#define StrokeVsSig "RootFlags(0)," \
    "CBV(b0)," \
    "SRV(t0)," \
    "SRV(t1)"

struct SeedGpu {
    float2 screenPosPx;
    float2 hatchDirScreen;
    float  curvaturePx; // signed, screen-space (1/px), from hatchGeometryPS's directionMap.b
    float  _pad0;
};

StructuredBuffer<SeedGpu> SeedBuffer  : register(t0);
StructuredBuffer<uint>    SeedCounter : register(t1);

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0; // x = along stroke length, y = across stroke width, both in [-1,1]
};

// A stroke rendered as a single quad only ever has its two ENDPOINTS
// evaluated -- the rasterizer linearly interpolates the straight line
// between them no matter what curve the endpoints were computed from, so
// bending only the endpoint positions is invisible. Bending must be
// tessellated: the ribbon is built from STROKE_SEGMENTS straight sub-quads
// chained along the arc, each short enough that its own straight edges
// approximate the local curve. Must match StereoHatchingApp.h's
// DrawInstanced vertex count (6 * STROKE_SEGMENTS).
#define STROKE_SEGMENTS 8

// 2 triangles (6 verts) forming a quad, signs in (along-length, across-width)
// local to ONE segment -- x in {0,1} selects the segment's near/far edge,
// not the whole stroke's endpoints.
static const float2 QuadSign[6] = {
    float2(0, -0.5), float2(1, -0.5), float2(0, 0.5),
    float2(0,  0.5), float2(1, -0.5), float2(1,  0.5)
};

// One oriented, curved ribbon per accepted seed, built directly from the
// seed buffer (no separate compute-builds-a-VB pass -- the seed buffer
// already is usable per-instance data). Draw call is a fixed
// DrawInstanced(6*STROKE_SEGMENTS, 8000, 0, 0); instances past the accepted
// count collapse to a degenerate vertex, the same idiom g-Aequor's
// particlePointVS.hlsl uses for its sentinel-collapse.
[RootSignature(StrokeVsSig)]
VsOut strokeVS(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VsOut o;

    uint validCount = min(SeedCounter[0], (uint)(strokeParams.z + 0.5));
    if (iid >= validCount) {
        o.pos = float4(0, 0, 0, 0);
        o.uv = float2(0, 0);
        return o;
    }

    SeedGpu s = SeedBuffer[iid];
    float2 dir = s.hatchDirScreen;
    float2 perp = float2(-dir.y, dir.x);

    uint segIdx = vid / 6;
    float2 local = QuadSign[vid % 6]; // local.x in {0,1} = segment's near/far edge, local.y in {-0.5,0.5} = across-width

    // Global arc-length parameter t in [-0.5, 0.5] across the WHOLE stroke
    // (not just this segment) -- this is what varies smoothly from segment
    // to segment so the ribbon reads as one continuous curve, and what the
    // width taper in strokePS.hlsl keys off via uv.x below.
    float t = -0.5 + (segIdx + local.x) / (float)STROKE_SEGMENTS;

    // Bend the stroke into a circular arc using the surface's own screen-
    // space curvature at the seed (see hatchGeometryVS.hlsl/PS.hlsl).
    // Parametrize by ARC LENGTH s (matching Stroke Length's original
    // straight-line meaning exactly when curvature is ~0): for a curve with
    // signed curvature c, starting at screenPosPx with unit tangent `dir`,
    //   pos(s)     = screenPosPx + (sin(c*s)/c)*dir + ((1-cos(c*s))/c)*perp
    //   tangent(s) = cos(c*s)*dir + sin(c*s)*perp
    // which is the standard Frenet-frame solution (dT/ds = c*N) and reduces
    // exactly to straight-line motion as c -> 0. The width offset then rides
    // the LOCAL tangent's perpendicular at this segment, not the seed's
    // original perp, so the ribbon keeps a constant width along the arc.
    float L = strokeParams.x;
    float maxC = 3.0 / max(L, 1e-3); // cap total bend to ~172 deg over the stroke's length -- avoids spiraling/self-overlap in tightly-curved regions
    float c = clamp(s.curvaturePx * strokeParams.w, -maxC, maxC); // strokeParams.w = GUI "Curvature Bend Scale"
    float sArc = t * L;
    float cs = c * sArc;

    float sinTerm, cosTerm, tCos, tSin;
    if (abs(c) > 1e-5) {
        sinTerm = sin(cs) / c;
        cosTerm = (1.0 - cos(cs)) / c;
        tCos = cos(cs);
        tSin = sin(cs);
    } else {
        // Taylor-limit as c -> 0 -- avoids amplifying float error from
        // dividing by a near-zero c even though the closed form is finite.
        sinTerm = sArc;
        cosTerm = 0.5 * c * sArc * sArc;
        tCos = 1.0;
        tSin = cs;
    }

    float2 centerLinePos = s.screenPosPx + sinTerm * dir + cosTerm * perp;
    float2 localPerp = -tSin * dir + tCos * perp;

    float2 posPx = centerLinePos + local.y * strokeParams.y * localPerp;

    float2 ndc = (posPx / viewportParams.xy) * 2.0 - 1.0;
    ndc.y = -ndc.y; // D3D screen-space y is flipped vs. NDC up

    o.pos = float4(ndc, 0, 1);
    o.uv = float2(t * 2.0, local.y * 2.0);
    return o;
}
