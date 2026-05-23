#include "PhongTess.hlsli"

// Tessellation level: N subdivisions per edge
// (N+1)*(N+2)/2 output vertices, N*N output triangles
// Upward triangles (corner pointing toward patch[2]): N*(N+1)/2 = 28
#define N 7
#define VERT_COUNT 36
#define PRIM_COUNT 49
#define UP_COUNT 28
//
//#define N 1
//#define VERT_COUNT 3
//#define PRIM_COUNT 1
//#define UP_COUNT 1
//
//#define N 2
//#define VERT_COUNT 6
//#define PRIM_COUNT 4
//#define UP_COUNT 3
//
//#define N 3
//#define VERT_COUNT 10
//#define PRIM_COUNT 9
//#define UP_COUNT 6
//
//#define N 4
//#define VERT_COUNT 14
//#define PRIM_COUNT 16
//#define UP_COUNT 10

// Vertex index in the triangular grid at position (i, j)
// where i,j >= 0 and i+j <= N.
// Barycentric: u = i/N (toward patch[0]), v = j/N (toward patch[1]), w = (N-i-j)/N (patch[2])
#define VID(i, j) ((uint)(((i)+(j))*((i)+(j)+1)/2 + (i)))

// Thread count must cover both vertex and triangle slots
#define THREAD_COUNT ((VERT_COUNT) > (PRIM_COUNT) ? (VERT_COUNT) : (PRIM_COUNT))

// Find the diagonal row r from a flat row-major triangle index.
// Row r holds (r+1) vertices/triangles, starting at index r*(r+1)/2.
uint TriRow(uint idx) {
    uint r = 0;
    while ((r + 1) * (r + 2) / 2 <= idx) r++;
    return r;
}

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

    // Load the 3 control vertices for this input triangle
    uint i0 = indexBuffer[gid * 3 + 0];
    uint i1 = indexBuffer[gid * 3 + 1];
    uint i2 = indexBuffer[gid * 3 + 2];
    MeshInput v0 = vertexBuffer[i0];
    MeshInput v1 = vertexBuffer[i1];
    MeshInput v2 = vertexBuffer[i2];

    if (tid < VERT_COUNT) {
        uint r = TriRow(tid);
        uint c = tid - r * (r + 1) / 2;
        float bu = (float)c / N;
        float bv = (float)(r - c) / N;
        float bw = 1.0f - bu - bv;

        // Linear position (object space)
        float3 pos = bu * v0.position + bv * v1.position + bw * v2.position;

        // Phong scalar offsets: ci = dot(pi - pos_linear, ni)
        float c0 = dot(v0.position - pos, v0.normal);
        float c1 = dot(v1.position - pos, v1.normal);
        float c2 = dot(v2.position - pos, v2.normal);

        // Phong-displaced position (alpha = 0.75)
        pos += 1.0f * (bu * c0 * v0.normal + bv * c1 * v1.normal +  bw * c2 * v2.normal);        
        //pos += 1.0f * (bu * bu * c0 * v0.normal + bv * bv * c1 * v1.normal + bw * bw * c2 * v2.normal)
        //        / (bu * bu + bv * bv + bw * bw);

        // Analytic normal of the displaced quadratic surface:
        //   dP/dbu = dp/dbu + 0.75*sum_i((dwi/dbu*ci + wi*dci/dbu)*ni)
        //   dci/dbu = -dot(dp/dbu, ni)  [constant over the patch since pos_linear is linear]
        //   weight derivatives: dw0/dbu=1, dw1/dbu=0, dw2/dbu=-1
        float3 dp_dbu = v0.position - v2.position;
        float3 dp_dbv = v1.position - v2.position;

        float3 dP_dbu = dp_dbu + 1.0f * (
            ( 1.0f * c0 - bu * dot(dp_dbu, v0.normal)) * v0.normal +
            ( 0.0f * c1 - bv * dot(dp_dbu, v1.normal)) * v1.normal +
            (-1.0f * c2 - bw * dot(dp_dbu, v2.normal)) * v2.normal);

        float3 dP_dbv = dp_dbv + 1.0f * (
            ( 0.0f * c0 - bu * dot(dp_dbv, v0.normal)) * v0.normal +
            ( 1.0f * c1 - bv * dot(dp_dbv, v1.normal)) * v1.normal +
            (-1.0f * c2 - bw * dot(dp_dbv, v2.normal)) * v2.normal);

        float4 worldPos = mul(modelMat, float4(pos, 1.0f));
        verts[tid].position = mul(viewProjMat, worldPos);
        verts[tid].worldPos = worldPos;
        verts[tid].normal   = normalize(mul(float4(cross(dP_dbu, dP_dbv), 0.0f), modelMatInverse).xyz);
    }

    // Generate triangle indices (only PRIM_COUNT threads write, extra threads only wrote vertices)
    if (tid < PRIM_COUNT) {
        uint r, c, pi, pj;
        if (tid < UP_COUNT) {
            // Upward triangle at (pi, pj): vertices (pi,pj), (pi+1,pj), (pi,pj+1)
            r = TriRow(tid);
            c = tid - r * (r + 1) / 2;
            pi = c;
            pj = r - c;
            tris[tid] = uint3(VID(pi, pj), VID(pi + 1, pj), VID(pi, pj + 1));
        } else {
            // Downward triangle: at (pi, pj): vertices (pi+1,pj), (pi,pj+1), (pi+1,pj+1)
            uint td = tid - UP_COUNT;
            r = TriRow(td);
            c = td - r * (r + 1) / 2;
            pi = c;
            pj = r - c;
            tris[tid] = uint3(VID(pi + 1, pj), VID(pi, pj + 1), VID(pi + 1, pj + 1));
        }
    }
}

