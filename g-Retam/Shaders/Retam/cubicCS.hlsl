#define CubicSig "RootFlags( 0 )," \
    "RootConstants(num32BitConstants=1, b0)," \
    "DescriptorTable(UAV(u0, numDescriptors=9))"

RWBuffer<float4>    designs    : register(u2);
RWBuffer<float4>    cubics     : register(u6);
RWByteAddressBuffer strokeList : register(u7);

#define designFloat4Size    5
#define designPageStride    80      // nMaxStrokesPerPage(16) * designFloat4Size(5)
#define CUBIC_GROUP_THREADS 64

[RootSignature(CubicSig)]
[numthreads(CUBIC_GROUP_THREADS, 1, 1)]
void cubicCS(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID)
{
    uint globalStroke = gid.x * CUBIC_GROUP_THREADS + tid.x;

    uint packed  = strokeList.Load(globalStroke * 4);
    uint page    = packed >> 16;
    uint j       = packed & 0xffff;

    uint base     = page * designPageStride + j * designFloat4Size;
    float4 xtp    = designs[base + 0];
    float4 ytp    = designs[base + 1];
    float4 t0_3   = designs[base + 2];
    float4 t3_6   = designs[base + 3];
    float4 extrema = designs[base + 4];

    if (t0_3.x < 32) {
        cubics[globalStroke * 4 + 0] = float4(0, 0, 0, 0);
        cubics[globalStroke * 4 + 1] = float4(0, 0, 0, 0);
        cubics[globalStroke * 4 + 2] = float4(0, 0, 0, 0);
        cubics[globalStroke * 4 + 3] = float4(0, 0, 0, 0);
        return;
    }

    float4x4 des = float4x4(
        float4(t0_3),
        float4(t0_3.yzw, t3_6.y),
        float4(t0_3.zw,  t3_6.yz),
        t3_6
    );

    float4 cubicX = 0;
    {
        float4 r = xtp, p = r;
        for (uint i = 0; i < 4; i++) {
            float4 apk  = mul(des, p);
            float  r2   = dot(r, r);
            float  alpha = r2 / dot(p, apk);
            cubicX += p * alpha;
            r -= alpha * apk;
            p  = r + p * dot(r, r) / r2;
        }
    }

    float4 cubicY = 0;
    {
        float4 r = ytp, p = r;
        for (uint i = 0; i < 4; i++) {
            float4 apk  = mul(des, p);
            float  r2   = dot(r, r);
            float  alpha = r2 / dot(p, apk);
            cubicY += p * alpha;
            r -= alpha * apk;
            p  = r + p * dot(r, r) / r2;
        }
    }

    float4 cubicV = 0;
    {
        float4 tx, tn;
        tx.x = extrema.y;        tx.y = tx.x*tx.x; tx.z = tx.y*tx.x; tx.w = tx.z*tx.x;
        tn.x = 1.0 - extrema.x;  tn.y = tn.x*tn.x; tn.z = tn.y*tn.x; tn.w = tn.z*tn.x;
        float4x4 tix = float4x4(
            tx      - tn,
            tx*tx.x - tn*tn.x,
            tx*tx.y - tn*tn.y,
            tx*tx.z - tn*tn.z
        );
        des += float4x4(
            float4(1.0,   1.0/2, 1.0/3, 1.0/4),
            float4(1.0/2, 1.0/3, 1.0/4, 1.0/5),
            float4(1.0/3, 1.0/4, 1.0/5, 1.0/6),
            float4(1.0/4, 1.0/5, 1.0/6, 1.0/7)
        ) * tix * float(t0_3.x);
        float4 r = t0_3, p = r;
        for (uint i = 0; i < 4; i++) {
            float4 apk  = mul(des, p);
            float  r2   = dot(r, r);
            float  alpha = r2 / dot(p, apk);
            cubicV += p * alpha;
            r -= alpha * apk;
            p  = r + p * dot(r, r) / r2;
        }
    }

    float confidence = saturate((t0_3.x - 16.0) / 16.0);
    cubics[globalStroke * 4 + 0] = cubicX;
    cubics[globalStroke * 4 + 1] = cubicY;
    cubics[globalStroke * 4 + 2] = cubicV;
    cubics[globalStroke * 4 + 3] = float4(1.0-extrema.y, extrema.x, t0_3.x, confidence);
}
