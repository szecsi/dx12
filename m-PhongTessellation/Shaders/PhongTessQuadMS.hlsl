#include "PhongTess.hlsli"

// Bilinear Phong tessellation over quads.
// One thread group per input quad; reads 4 indices from indexBuffer.
// (N+1)^2 output vertices, 2*N^2 output triangles.

//#define N 1
//#define VERT_COUNT 4
//#define PRIM_COUNT 2

//#define N 2
//#define VERT_COUNT 9
//#define PRIM_COUNT 8

//#define N 4
//#define VERT_COUNT 25
//#define PRIM_COUNT 32

#define N 7
#define VERT_COUNT 64
#define PRIM_COUNT 98

//#define N 8    // max: THREAD_COUNT == 128
//#define VERT_COUNT 81
//#define PRIM_COUNT 128

#define THREAD_COUNT ((VERT_COUNT) > (PRIM_COUNT) ? (VERT_COUNT) : (PRIM_COUNT))

// Vertex index in the (N+1)x(N+1) grid: u-axis = i (0..N), v-axis = j (0..N)
#define QVID(i, j) ((uint)((i) * (N + 1) + (j)))

[RootSignature(PhongTessRootSig)]
[numthreads(THREAD_COUNT, 1, 1)]
[outputtopology("triangle")]
void main(
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out vertices MeshOutput verts[VERT_COUNT],
    out indices uint3 tris[PRIM_COUNT])
{
    SetMeshOutputCounts(VERT_COUNT, PRIM_COUNT);

    // Load 4 control vertices for this quad face (CCW order: v0, v1, v2, v3)
    // Corners: v0=(u=0,v=0), v1=(u=1,v=0), v2=(u=1,v=1), v3=(u=0,v=1)
    uint i0 = indexBuffer[gid * 4 + 0];
    uint i1 = indexBuffer[gid * 4 + 1];
    uint i2 = indexBuffer[gid * 4 + 2];
    uint i3 = indexBuffer[gid * 4 + 3];
    MeshInput v0 = vertexBuffer[i0];
    MeshInput v1 = vertexBuffer[i1];
    MeshInput v2 = vertexBuffer[i2];
    MeshInput v3 = vertexBuffer[i3];

    if (tid < VERT_COUNT) {
        uint vi = tid / (N + 1);
        uint vj = tid % (N + 1);
        float u = (float)vi / N;
        float v = (float)vj / N;

        float w0 = (1.0f - u) * (1.0f - v);
        float w1 = u           * (1.0f - v);
        float w2 = u           * v;
        float w3 = (1.0f - u) * v;

        // Linear position (object space)
        float3 pos = w0 * v0.position + w1 * v1.position + w2 * v2.position + w3 * v3.position;

        // Phong scalar offsets: ci = dot(pi - pos_linear, ni)
        float c0 = dot(v0.position - pos, v0.normal);
        float c1 = dot(v1.position - pos, v1.normal);
        float c2 = dot(v2.position - pos, v2.normal);
        float c3 = dot(v3.position - pos, v3.normal);

        // Phong-displaced position (alpha = 0.75)
        float alpha = 0.95f;
        pos += alpha * (w0 * c0 * v0.normal + w1 * c1 * v1.normal
                      + w2 * c2 * v2.normal + w3 * c3 * v3.normal);
        
//        pos += alpha * (w0 * w0 * c0 * v0.normal + w1 * w1 * c1 * v1.normal
//                      + w2 * w2 * c2 * v2.normal + w3 * w3 * c3 * v3.normal)
//                / (w0 * w0 + w1 * w1 + w2 * w2 + w3 * w3);
//
        // Analytic normal of the displaced biquadratic surface:
        //   dP/du = dp/du + 0.75*sum_i((dwi/du*ci + wi*dci/du)*ni)
        //   dci/du = -dot(dp/du, ni)  [dp/du depends on v, making dci/du v-dependent too]
        float dw0_du = -(1.0f - v);  float dw0_dv = -(1.0f - u);
        float dw1_du =  (1.0f - v);  float dw1_dv = -u;
        float dw2_du =  v;            float dw2_dv =  u;
        float dw3_du = -v;            float dw3_dv =  (1.0f - u);

        float3 dp_du = dw0_du*v0.position + dw1_du*v1.position + dw2_du*v2.position + dw3_du*v3.position;
        float3 dp_dv = dw0_dv*v0.position + dw1_dv*v1.position + dw2_dv*v2.position + dw3_dv*v3.position;

        float3 dP_du = dp_du + alpha * (
            (dw0_du * c0 - w0 * dot(dp_du, v0.normal)) * v0.normal +
            (dw1_du * c1 - w1 * dot(dp_du, v1.normal)) * v1.normal +
            (dw2_du * c2 - w2 * dot(dp_du, v2.normal)) * v2.normal +
            (dw3_du * c3 - w3 * dot(dp_du, v3.normal)) * v3.normal);

        float3 dP_dv = dp_dv + alpha * (
            (dw0_dv * c0 - w0 * dot(dp_dv, v0.normal)) * v0.normal +
            (dw1_dv * c1 - w1 * dot(dp_dv, v1.normal)) * v1.normal +
            (dw2_dv * c2 - w2 * dot(dp_dv, v2.normal)) * v2.normal +
            (dw3_dv * c3 - w3 * dot(dp_dv, v3.normal)) * v3.normal);

        float4 worldPos = mul(modelMat, float4(pos, 1.0f));
        verts[tid].position = mul(viewProjMat, worldPos);
        verts[tid].worldPos = worldPos;
        verts[tid].normal   = normalize(mul(float4(cross(dP_du, dP_dv), 0.0f), modelMatInverse).xyz);
    }

    // Each pair of tids covers one quad cell; tid 2*k = lower-left tri, tid 2*k+1 = upper-right tri
    if (tid < PRIM_COUNT) {
        uint cell = tid / 2;
        uint ci = cell / N;   // u-cell index
        uint cj = cell % N;   // v-cell index
        if (tid % 2 == 0) {
            tris[tid] = uint3(QVID(ci, cj), QVID(ci + 1, cj), QVID(ci, cj + 1));
        } else {
            tris[tid] = uint3(QVID(ci + 1, cj), QVID(ci + 1, cj + 1), QVID(ci, cj + 1));
        }
    }
}
