#pragma once

#include "float2.h"
#include "float3.h"
#include "bool4.h"
#include "int4.h"

namespace Egg {
    namespace Math {

        class float2;
        class float3;
        class bool2;
        class bool3;
        class bool4;
        class int2;
        class int3;
        class int4;

        class float4 {
        public:
            union {
                struct {
                    float x;
                    float y;
                    float z;
                    float w;
                };

                float2Swizzle<float2, int2, bool2, 4, 0, 0> xx;
                float2Swizzle<float2, int2, bool2, 4, 0, 1> xy;
                float2Swizzle<float2, int2, bool2, 4, 0, 2> xz;
                float2Swizzle<float2, int2, bool2, 4, 0, 3> xw;
                float2Swizzle<float2, int2, bool2, 4, 1, 0> yx;
                float2Swizzle<float2, int2, bool2, 4, 1, 1> yy;
                float2Swizzle<float2, int2, bool2, 4, 1, 2> yz;
                float2Swizzle<float2, int2, bool2, 4, 1, 3> yw;
                float2Swizzle<float2, int2, bool2, 4, 2, 0> zx;
                float2Swizzle<float2, int2, bool2, 4, 2, 1> zy;
                float2Swizzle<float2, int2, bool2, 4, 2, 2> zz;
                float2Swizzle<float2, int2, bool2, 4, 2, 3> zw;
                float2Swizzle<float2, int2, bool2, 4, 3, 0> wx;
                float2Swizzle<float2, int2, bool2, 4, 3, 1> wy;
                float2Swizzle<float2, int2, bool2, 4, 3, 2> wz;
                float2Swizzle<float2, int2, bool2, 4, 3, 3> ww;

                float3Swizzle<float3, int3, bool3, 4, 0, 0, 0> xxx;
                float3Swizzle<float3, int3, bool3, 4, 0, 0, 1> xxy;
                float3Swizzle<float3, int3, bool3, 4, 0, 0, 2> xxz;
                float3Swizzle<float3, int3, bool3, 4, 0, 0, 3> xxw;
                float3Swizzle<float3, int3, bool3, 4, 0, 1, 0> xyx;
                float3Swizzle<float3, int3, bool3, 4, 0, 1, 1> xyy;
                float3Swizzle<float3, int3, bool3, 4, 0, 1, 2> xyz;
                float3Swizzle<float3, int3, bool3, 4, 0, 1, 3> xyw;
                float3Swizzle<float3, int3, bool3, 4, 0, 2, 0> xzx;
                float3Swizzle<float3, int3, bool3, 4, 0, 2, 1> xzy;
                float3Swizzle<float3, int3, bool3, 4, 0, 2, 2> xzz;
                float3Swizzle<float3, int3, bool3, 4, 0, 2, 3> xzw;
                float3Swizzle<float3, int3, bool3, 4, 0, 3, 0> xwx;
                float3Swizzle<float3, int3, bool3, 4, 0, 3, 1> xwy;
                float3Swizzle<float3, int3, bool3, 4, 0, 3, 2> xwz;
                float3Swizzle<float3, int3, bool3, 4, 0, 3, 3> xww;
                float3Swizzle<float3, int3, bool3, 4, 1, 0, 0> yxx;
                float3Swizzle<float3, int3, bool3, 4, 1, 0, 1> yxy;
                float3Swizzle<float3, int3, bool3, 4, 1, 0, 2> yxz;
                float3Swizzle<float3, int3, bool3, 4, 1, 0, 3> yxw;
                float3Swizzle<float3, int3, bool3, 4, 1, 1, 0> yyx;
                float3Swizzle<float3, int3, bool3, 4, 1, 1, 1> yyy;
                float3Swizzle<float3, int3, bool3, 4, 1, 1, 2> yyz;
                float3Swizzle<float3, int3, bool3, 4, 1, 1, 3> yyw;
                float3Swizzle<float3, int3, bool3, 4, 1, 2, 0> yzx;
                float3Swizzle<float3, int3, bool3, 4, 1, 2, 1> yzy;
                float3Swizzle<float3, int3, bool3, 4, 1, 2, 2> yzz;
                float3Swizzle<float3, int3, bool3, 4, 1, 2, 3> yzw;
                float3Swizzle<float3, int3, bool3, 4, 1, 3, 0> ywx;
                float3Swizzle<float3, int3, bool3, 4, 1, 3, 1> ywy;
                float3Swizzle<float3, int3, bool3, 4, 1, 3, 2> ywz;
                float3Swizzle<float3, int3, bool3, 4, 1, 3, 3> yww;
                float3Swizzle<float3, int3, bool3, 4, 2, 0, 0> zxx;
                float3Swizzle<float3, int3, bool3, 4, 2, 0, 1> zxy;
                float3Swizzle<float3, int3, bool3, 4, 2, 0, 2> zxz;
                float3Swizzle<float3, int3, bool3, 4, 2, 0, 3> zxw;
                float3Swizzle<float3, int3, bool3, 4, 2, 1, 0> zyx;
                float3Swizzle<float3, int3, bool3, 4, 2, 1, 1> zyy;
                float3Swizzle<float3, int3, bool3, 4, 2, 1, 2> zyz;
                float3Swizzle<float3, int3, bool3, 4, 2, 1, 3> zyw;
                float3Swizzle<float3, int3, bool3, 4, 2, 2, 0> zzx;
                float3Swizzle<float3, int3, bool3, 4, 2, 2, 1> zzy;
                float3Swizzle<float3, int3, bool3, 4, 2, 2, 2> zzz;
                float3Swizzle<float3, int3, bool3, 4, 2, 2, 3> zzw;
                float3Swizzle<float3, int3, bool3, 4, 2, 3, 0> zwx;
                float3Swizzle<float3, int3, bool3, 4, 2, 3, 1> zwy;
                float3Swizzle<float3, int3, bool3, 4, 2, 3, 2> zwz;
                float3Swizzle<float3, int3, bool3, 4, 2, 3, 3> zww;
                float3Swizzle<float3, int3, bool3, 4, 3, 0, 0> wxx;
                float3Swizzle<float3, int3, bool3, 4, 3, 0, 1> wxy;
                float3Swizzle<float3, int3, bool3, 4, 3, 0, 2> wxz;
                float3Swizzle<float3, int3, bool3, 4, 3, 0, 3> wxw;
                float3Swizzle<float3, int3, bool3, 4, 3, 1, 0> wyx;
                float3Swizzle<float3, int3, bool3, 4, 3, 1, 1> wyy;
                float3Swizzle<float3, int3, bool3, 4, 3, 1, 2> wyz;
                float3Swizzle<float3, int3, bool3, 4, 3, 1, 3> wyw;
                float3Swizzle<float3, int3, bool3, 4, 3, 2, 0> wzx;
                float3Swizzle<float3, int3, bool3, 4, 3, 2, 1> wzy;
                float3Swizzle<float3, int3, bool3, 4, 3, 2, 2> wzz;
                float3Swizzle<float3, int3, bool3, 4, 3, 2, 3> wzw;
                float3Swizzle<float3, int3, bool3, 4, 3, 3, 0> wwx;
                float3Swizzle<float3, int3, bool3, 4, 3, 3, 1> wwy;
                float3Swizzle<float3, int3, bool3, 4, 3, 3, 2> wwz;
                float3Swizzle<float3, int3, bool3, 4, 3, 3, 3> www;

                float4Swizzle<float4, int4, bool4, 4, 0, 0, 0, 0> xxxx;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 1, 0> xxxy;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 2, 0> xxxz;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 3, 0> xxxw;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 0, 1> xxyx;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 1, 1> xxyy;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 2, 1> xxyz;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 3, 1> xxyw;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 0, 2> xxzx;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 1, 2> xxzy;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 2, 2> xxzz;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 3, 2> xxzw;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 0, 3> xxwx;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 1, 3> xxwy;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 2, 3> xxwz;
                float4Swizzle<float4, int4, bool4, 4, 0, 0, 3, 3> xxww;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 0, 0> xyxx;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 1, 0> xyxy;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 2, 0> xyxz;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 3, 0> xyxw;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 0, 1> xyyx;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 1, 1> xyyy;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 2, 1> xyyz;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 3, 1> xyyw;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 0, 2> xyzx;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 1, 2> xyzy;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 2, 2> xyzz;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 3, 2> xyzw;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 0, 3> xywx;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 1, 3> xywy;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 2, 3> xywz;
                float4Swizzle<float4, int4, bool4, 4, 0, 1, 3, 3> xyww;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 0, 0> xzxx;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 1, 0> xzxy;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 2, 0> xzxz;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 3, 0> xzxw;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 0, 1> xzyx;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 1, 1> xzyy;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 2, 1> xzyz;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 3, 1> xzyw;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 0, 2> xzzx;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 1, 2> xzzy;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 2, 2> xzzz;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 3, 2> xzzw;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 0, 3> xzwx;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 1, 3> xzwy;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 2, 3> xzwz;
                float4Swizzle<float4, int4, bool4, 4, 0, 2, 3, 3> xzww;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 0, 0> xwxx;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 1, 0> xwxy;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 2, 0> xwxz;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 3, 0> xwxw;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 0, 1> xwyx;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 1, 1> xwyy;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 2, 1> xwyz;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 3, 1> xwyw;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 0, 2> xwzx;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 1, 2> xwzy;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 2, 2> xwzz;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 3, 2> xwzw;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 0, 3> xwwx;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 1, 3> xwwy;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 2, 3> xwwz;
                float4Swizzle<float4, int4, bool4, 4, 0, 3, 3, 3> xwww;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 0, 0> yxxx;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 1, 0> yxxy;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 2, 0> yxxz;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 3, 0> yxxw;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 0, 1> yxyx;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 1, 1> yxyy;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 2, 1> yxyz;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 3, 1> yxyw;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 0, 2> yxzx;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 1, 2> yxzy;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 2, 2> yxzz;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 3, 2> yxzw;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 0, 3> yxwx;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 1, 3> yxwy;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 2, 3> yxwz;
                float4Swizzle<float4, int4, bool4, 4, 1, 0, 3, 3> yxww;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 0, 0> yyxx;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 1, 0> yyxy;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 2, 0> yyxz;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 3, 0> yyxw;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 0, 1> yyyx;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 1, 1> yyyy;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 2, 1> yyyz;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 3, 1> yyyw;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 0, 2> yyzx;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 1, 2> yyzy;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 2, 2> yyzz;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 3, 2> yyzw;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 0, 3> yywx;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 1, 3> yywy;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 2, 3> yywz;
                float4Swizzle<float4, int4, bool4, 4, 1, 1, 3, 3> yyww;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 0, 0> yzxx;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 1, 0> yzxy;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 2, 0> yzxz;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 3, 0> yzxw;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 0, 1> yzyx;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 1, 1> yzyy;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 2, 1> yzyz;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 3, 1> yzyw;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 0, 2> yzzx;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 1, 2> yzzy;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 2, 2> yzzz;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 3, 2> yzzw;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 0, 3> yzwx;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 1, 3> yzwy;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 2, 3> yzwz;
                float4Swizzle<float4, int4, bool4, 4, 1, 2, 3, 3> yzww;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 0, 0> ywxx;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 1, 0> ywxy;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 2, 0> ywxz;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 3, 0> ywxw;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 0, 1> ywyx;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 1, 1> ywyy;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 2, 1> ywyz;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 3, 1> ywyw;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 0, 2> ywzx;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 1, 2> ywzy;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 2, 2> ywzz;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 3, 2> ywzw;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 0, 3> ywwx;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 1, 3> ywwy;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 2, 3> ywwz;
                float4Swizzle<float4, int4, bool4, 4, 1, 3, 3, 3> ywww;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 0, 0> zxxx;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 1, 0> zxxy;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 2, 0> zxxz;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 3, 0> zxxw;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 0, 1> zxyx;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 1, 1> zxyy;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 2, 1> zxyz;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 3, 1> zxyw;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 0, 2> zxzx;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 1, 2> zxzy;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 2, 2> zxzz;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 3, 2> zxzw;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 0, 3> zxwx;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 1, 3> zxwy;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 2, 3> zxwz;
                float4Swizzle<float4, int4, bool4, 4, 2, 0, 3, 3> zxww;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 0, 0> zyxx;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 1, 0> zyxy;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 2, 0> zyxz;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 3, 0> zyxw;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 0, 1> zyyx;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 1, 1> zyyy;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 2, 1> zyyz;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 3, 1> zyyw;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 0, 2> zyzx;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 1, 2> zyzy;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 2, 2> zyzz;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 3, 2> zyzw;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 0, 3> zywx;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 1, 3> zywy;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 2, 3> zywz;
                float4Swizzle<float4, int4, bool4, 4, 2, 1, 3, 3> zyww;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 0, 0> zzxx;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 1, 0> zzxy;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 2, 0> zzxz;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 3, 0> zzxw;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 0, 1> zzyx;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 1, 1> zzyy;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 2, 1> zzyz;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 3, 1> zzyw;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 0, 2> zzzx;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 1, 2> zzzy;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 2, 2> zzzz;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 3, 2> zzzw;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 0, 3> zzwx;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 1, 3> zzwy;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 2, 3> zzwz;
                float4Swizzle<float4, int4, bool4, 4, 2, 2, 3, 3> zzww;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 0, 0> zwxx;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 1, 0> zwxy;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 2, 0> zwxz;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 3, 0> zwxw;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 0, 1> zwyx;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 1, 1> zwyy;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 2, 1> zwyz;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 3, 1> zwyw;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 0, 2> zwzx;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 1, 2> zwzy;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 2, 2> zwzz;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 3, 2> zwzw;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 0, 3> zwwx;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 1, 3> zwwy;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 2, 3> zwwz;
                float4Swizzle<float4, int4, bool4, 4, 2, 3, 3, 3> zwww;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 0, 0> wxxx;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 1, 0> wxxy;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 2, 0> wxxz;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 3, 0> wxxw;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 0, 1> wxyx;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 1, 1> wxyy;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 2, 1> wxyz;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 3, 1> wxyw;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 0, 2> wxzx;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 1, 2> wxzy;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 2, 2> wxzz;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 3, 2> wxzw;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 0, 3> wxwx;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 1, 3> wxwy;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 2, 3> wxwz;
                float4Swizzle<float4, int4, bool4, 4, 3, 0, 3, 3> wxww;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 0, 0> wyxx;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 1, 0> wyxy;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 2, 0> wyxz;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 3, 0> wyxw;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 0, 1> wyyx;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 1, 1> wyyy;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 2, 1> wyyz;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 3, 1> wyyw;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 0, 2> wyzx;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 1, 2> wyzy;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 2, 2> wyzz;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 3, 2> wyzw;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 0, 3> wywx;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 1, 3> wywy;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 2, 3> wywz;
                float4Swizzle<float4, int4, bool4, 4, 3, 1, 3, 3> wyww;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 0, 0> wzxx;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 1, 0> wzxy;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 2, 0> wzxz;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 3, 0> wzxw;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 0, 1> wzyx;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 1, 1> wzyy;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 2, 1> wzyz;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 3, 1> wzyw;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 0, 2> wzzx;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 1, 2> wzzy;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 2, 2> wzzz;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 3, 2> wzzw;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 0, 3> wzwx;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 1, 3> wzwy;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 2, 3> wzwz;
                float4Swizzle<float4, int4, bool4, 4, 3, 2, 3, 3> wzww;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 0, 0> wwxx;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 1, 0> wwxy;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 2, 0> wwxz;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 3, 0> wwxw;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 0, 1> wwyx;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 1, 1> wwyy;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 2, 1> wwyz;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 3, 1> wwyw;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 0, 2> wwzx;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 1, 2> wwzy;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 2, 2> wwzz;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 3, 2> wwzw;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 0, 3> wwwx;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 1, 3> wwwy;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 2, 3> wwwz;
                float4Swizzle<float4, int4, bool4, 4, 3, 3, 3, 3> wwww;
            };

            float4(float x, float y, float z, float w);

            float4(float x, float y, const float2 & zw);

            float4(const float2 & xy, const float2 & zw);

            float4(const float2 & xy, float z, float w);

            float4(const float3 & xyz, float w);

            float4(float x, const float3 & yzw);

            float4(const float4 & xyzw);

            float4();

            float4 & operator=(const float4 & rhs) noexcept;
float4 & operator=(float rhs) noexcept;

            float4 & operator+=(const float4 & rhs) noexcept;
float4 & operator+=(float rhs) noexcept;

            float4 & operator-=(const float4 & rhs) noexcept;
float4 & operator-=(float rhs) noexcept;

            float4 & operator/=(const float4 & rhs) noexcept;
float4 & operator/=(float rhs) noexcept;

            float4 & operator*=(const float4 & rhs) noexcept;
float4 & operator*=(float rhs) noexcept;

            float4 operator*(const float4 & rhs) const noexcept;

            float4 operator/(const float4 & rhs) const noexcept;

            float4 operator+(const float4 & rhs) const noexcept;

            float4 operator-(const float4 & rhs) const noexcept;

            float4 Abs() const noexcept;

            float4 Acos() const noexcept;

            float4 Asin() const noexcept;

            float4 Atan() const noexcept;

            float4 Cos() const noexcept;

            float4 Sin() const noexcept;

            float4 Cosh() const noexcept;

            float4 Sinh() const noexcept;

            float4 Tan() const noexcept;

            float4 Exp() const noexcept;

            float4 Log() const noexcept;

            float4 Log10() const noexcept;

            float4 Fmod(const float4 & rhs) const noexcept;

            float4 Atan2(const float4 & rhs) const noexcept;

            float4 Pow(const float4 & rhs) const noexcept;

            float4 Sqrt() const noexcept;

            float4 Clamp(const float4 & low, const float4 & high) const noexcept;

            float Dot(const float4 & rhs) const noexcept;

            float4 Sign() const noexcept;

            int4 Round() const noexcept;

            float4 Saturate() const noexcept;

            float LengthSquared() const noexcept;

            float Length() const noexcept;

            float4 Normalize() const noexcept;

            bool4 IsNan() const noexcept;

            bool4 IsFinite() const noexcept;

            bool4 IsInfinite() const noexcept;

            float4 operator-() const noexcept;

            float4 operator%(const float4 & rhs) const noexcept;

            float4 & operator%=(const float4 & rhs) noexcept;

            int4 Ceil() const noexcept;

            int4 Floor() const noexcept;

            float4 Exp2() const noexcept;

            int4 Trunc() const noexcept;

            float Distance(const float4 & rhs) const noexcept;

            bool4 operator<(const float4 & rhs) const noexcept;

            bool4 operator>(const float4 & rhs) const noexcept;

            bool4 operator!=(const float4 & rhs) const noexcept;

            bool4 operator==(const float4 & rhs) const noexcept;

            bool4 operator>=(const float4 & rhs) const noexcept;

            bool4 operator<=(const float4 & rhs) const noexcept;

            static float4 Random(float lower = 0.0f, float upper = 1.0f) noexcept;

            float4 operator+(float v) const noexcept;

            float4 operator-(float v) const noexcept;

            float4 operator*(float v) const noexcept;

            float4 operator/(float v) const noexcept;

            float4 operator%(float v) const noexcept;

            float4 operator!() const noexcept;

            static const float4 UnitX;
            static const float4 UnitY;
            static const float4 UnitZ;
            static const float4 UnitW;
            static const float4 Zero;
            static const float4 One;
            static const float4 Identity;
        };
    }
}

