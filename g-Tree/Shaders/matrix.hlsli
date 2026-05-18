#pragma once

// Row-vector convention: transform with mul(v, M.m) or chain as M = A * B * C (A applied first).

struct Mat4 {
    float4x4 m;

    Mat4 operator*(Mat4 rhs) {
        Mat4 r;
        r.m = mul(m, rhs.m);
        return r;
    }

    float4 operator[](int i) { return m[i]; }
};

Mat4 Scale(float sx, float sy, float sz)
{
    Mat4 r;
    r.m = float4x4(
        sx,  0,   0,   0,
        0,   sy,  0,   0,
        0,   0,   sz,  0,
        0,   0,   0,   1
    );
    return r;
}

Mat4 Translation(float tx, float ty, float tz)
{
    Mat4 r;
    r.m = float4x4(
        1,   0,   0,   0,
        0,   1,   0,   0,
        0,   0,   1,   0,
        tx,  ty,  tz,  1
    );
    return r;
}

Mat4 RotationX(float theta)
{
    float c = cos(theta);
    float s = sin(theta);
    Mat4 r;
    r.m = float4x4(
        1,   0,   0,   0,
        0,   c,   s,   0,
        0,  -s,   c,   0,
        0,   0,   0,   1
    );
    return r;
}

Mat4 RotationY(float theta)
{
    float c = cos(theta);
    float s = sin(theta);
    Mat4 r;
    r.m = float4x4(
        c,   0,  -s,   0,
        0,   1,   0,   0,
        s,   0,   c,   0,
        0,   0,   0,   1
    );
    return r;
}

Mat4 RotationZ(float theta)
{
    float c = cos(theta);
    float s = sin(theta);
    Mat4 r;
    r.m = float4x4(
        c,   s,   0,   0,
       -s,   c,   0,   0,
        0,   0,   1,   0,
        0,   0,   0,   1
    );
    return r;
}

Mat4 RotationAxis(float theta, float3 axis)
{
    float3 n = normalize(axis);
    float c = cos(theta);
    float s = sin(theta);
    float t = 1.0 - c;
    float nx = n.x, ny = n.y, nz = n.z;
    Mat4 r;
    r.m = float4x4(
        t*nx*nx + c,       t*ny*nx + nz*s,    t*nz*nx - ny*s,    0,
        t*nx*ny - nz*s,    t*ny*ny + c,       t*nz*ny + nx*s,    0,
        t*nx*nz + ny*s,    t*ny*nz - nx*s,    t*nz*nz + c,       0,
        0,                 0,                 0,                  1
    );
    return r;
}
