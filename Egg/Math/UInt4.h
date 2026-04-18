#pragma once

#include "bool4.h"
#include "uint2.h"
#include "uint3.h"

namespace Egg {
    namespace Math {

        class uint2;
        class uint3;
        class bool2;
        class bool3;
        class bool4;

        class uint4 {
        public:
            union {
                struct {
                    unsigned int x;
                    unsigned int y;
                    unsigned int z;
                    unsigned int w;
                };

                uint2Swizzle<uint2, bool2, 4, 0, 0> xx;
                uint2Swizzle<uint2, bool2, 4, 0, 1> xy;
                uint2Swizzle<uint2, bool2, 4, 0, 2> xz;
                uint2Swizzle<uint2, bool2, 4, 0, 3> xw;
                uint2Swizzle<uint2, bool2, 4, 1, 0> yx;
                uint2Swizzle<uint2, bool2, 4, 1, 1> yy;
                uint2Swizzle<uint2, bool2, 4, 1, 2> yz;
                uint2Swizzle<uint2, bool2, 4, 1, 3> yw;
                uint2Swizzle<uint2, bool2, 4, 2, 0> zx;
                uint2Swizzle<uint2, bool2, 4, 2, 1> zy;
                uint2Swizzle<uint2, bool2, 4, 2, 2> zz;
                uint2Swizzle<uint2, bool2, 4, 2, 3> zw;
                uint2Swizzle<uint2, bool2, 4, 3, 0> wx;
                uint2Swizzle<uint2, bool2, 4, 3, 1> wy;
                uint2Swizzle<uint2, bool2, 4, 3, 2> wz;
                uint2Swizzle<uint2, bool2, 4, 3, 3> ww;

                uint3Swizzle<uint3, bool3, 4, 0, 0, 0> xxx;
                uint3Swizzle<uint3, bool3, 4, 0, 0, 1> xxy;
                uint3Swizzle<uint3, bool3, 4, 0, 0, 2> xxz;
                uint3Swizzle<uint3, bool3, 4, 0, 0, 3> xxw;
                uint3Swizzle<uint3, bool3, 4, 0, 1, 0> xyx;
                uint3Swizzle<uint3, bool3, 4, 0, 1, 1> xyy;
                uint3Swizzle<uint3, bool3, 4, 0, 1, 2> xyz;
                uint3Swizzle<uint3, bool3, 4, 0, 1, 3> xyw;
                uint3Swizzle<uint3, bool3, 4, 0, 2, 0> xzx;
                uint3Swizzle<uint3, bool3, 4, 0, 2, 1> xzy;
                uint3Swizzle<uint3, bool3, 4, 0, 2, 2> xzz;
                uint3Swizzle<uint3, bool3, 4, 0, 2, 3> xzw;
                uint3Swizzle<uint3, bool3, 4, 0, 3, 0> xwx;
                uint3Swizzle<uint3, bool3, 4, 0, 3, 1> xwy;
                uint3Swizzle<uint3, bool3, 4, 0, 3, 2> xwz;
                uint3Swizzle<uint3, bool3, 4, 0, 3, 3> xww;
                uint3Swizzle<uint3, bool3, 4, 1, 0, 0> yxx;
                uint3Swizzle<uint3, bool3, 4, 1, 0, 1> yxy;
                uint3Swizzle<uint3, bool3, 4, 1, 0, 2> yxz;
                uint3Swizzle<uint3, bool3, 4, 1, 0, 3> yxw;
                uint3Swizzle<uint3, bool3, 4, 1, 1, 0> yyx;
                uint3Swizzle<uint3, bool3, 4, 1, 1, 1> yyy;
                uint3Swizzle<uint3, bool3, 4, 1, 1, 2> yyz;
                uint3Swizzle<uint3, bool3, 4, 1, 1, 3> yyw;
                uint3Swizzle<uint3, bool3, 4, 1, 2, 0> yzx;
                uint3Swizzle<uint3, bool3, 4, 1, 2, 1> yzy;
                uint3Swizzle<uint3, bool3, 4, 1, 2, 2> yzz;
                uint3Swizzle<uint3, bool3, 4, 1, 2, 3> yzw;
                uint3Swizzle<uint3, bool3, 4, 1, 3, 0> ywx;
                uint3Swizzle<uint3, bool3, 4, 1, 3, 1> ywy;
                uint3Swizzle<uint3, bool3, 4, 1, 3, 2> ywz;
                uint3Swizzle<uint3, bool3, 4, 1, 3, 3> yww;
                uint3Swizzle<uint3, bool3, 4, 2, 0, 0> zxx;
                uint3Swizzle<uint3, bool3, 4, 2, 0, 1> zxy;
                uint3Swizzle<uint3, bool3, 4, 2, 0, 2> zxz;
                uint3Swizzle<uint3, bool3, 4, 2, 0, 3> zxw;
                uint3Swizzle<uint3, bool3, 4, 2, 1, 0> zyx;
                uint3Swizzle<uint3, bool3, 4, 2, 1, 1> zyy;
                uint3Swizzle<uint3, bool3, 4, 2, 1, 2> zyz;
                uint3Swizzle<uint3, bool3, 4, 2, 1, 3> zyw;
                uint3Swizzle<uint3, bool3, 4, 2, 2, 0> zzx;
                uint3Swizzle<uint3, bool3, 4, 2, 2, 1> zzy;
                uint3Swizzle<uint3, bool3, 4, 2, 2, 2> zzz;
                uint3Swizzle<uint3, bool3, 4, 2, 2, 3> zzw;
                uint3Swizzle<uint3, bool3, 4, 2, 3, 0> zwx;
                uint3Swizzle<uint3, bool3, 4, 2, 3, 1> zwy;
                uint3Swizzle<uint3, bool3, 4, 2, 3, 2> zwz;
                uint3Swizzle<uint3, bool3, 4, 2, 3, 3> zww;
                uint3Swizzle<uint3, bool3, 4, 3, 0, 0> wxx;
                uint3Swizzle<uint3, bool3, 4, 3, 0, 1> wxy;
                uint3Swizzle<uint3, bool3, 4, 3, 0, 2> wxz;
                uint3Swizzle<uint3, bool3, 4, 3, 0, 3> wxw;
                uint3Swizzle<uint3, bool3, 4, 3, 1, 0> wyx;
                uint3Swizzle<uint3, bool3, 4, 3, 1, 1> wyy;
                uint3Swizzle<uint3, bool3, 4, 3, 1, 2> wyz;
                uint3Swizzle<uint3, bool3, 4, 3, 1, 3> wyw;
                uint3Swizzle<uint3, bool3, 4, 3, 2, 0> wzx;
                uint3Swizzle<uint3, bool3, 4, 3, 2, 1> wzy;
                uint3Swizzle<uint3, bool3, 4, 3, 2, 2> wzz;
                uint3Swizzle<uint3, bool3, 4, 3, 2, 3> wzw;
                uint3Swizzle<uint3, bool3, 4, 3, 3, 0> wwx;
                uint3Swizzle<uint3, bool3, 4, 3, 3, 1> wwy;
                uint3Swizzle<uint3, bool3, 4, 3, 3, 2> wwz;
                uint3Swizzle<uint3, bool3, 4, 3, 3, 3> www;

                uint4Swizzle<uint4, bool4, 4, 0, 0, 0, 0> xxxx;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 1, 0> xxxy;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 2, 0> xxxz;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 3, 0> xxxw;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 0, 1> xxyx;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 1, 1> xxyy;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 2, 1> xxyz;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 3, 1> xxyw;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 0, 2> xxzx;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 1, 2> xxzy;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 2, 2> xxzz;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 3, 2> xxzw;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 0, 3> xxwx;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 1, 3> xxwy;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 2, 3> xxwz;
                uint4Swizzle<uint4, bool4, 4, 0, 0, 3, 3> xxww;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 0, 0> xyxx;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 1, 0> xyxy;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 2, 0> xyxz;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 3, 0> xyxw;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 0, 1> xyyx;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 1, 1> xyyy;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 2, 1> xyyz;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 3, 1> xyyw;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 0, 2> xyzx;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 1, 2> xyzy;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 2, 2> xyzz;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 3, 2> xyzw;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 0, 3> xywx;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 1, 3> xywy;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 2, 3> xywz;
                uint4Swizzle<uint4, bool4, 4, 0, 1, 3, 3> xyww;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 0, 0> xzxx;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 1, 0> xzxy;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 2, 0> xzxz;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 3, 0> xzxw;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 0, 1> xzyx;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 1, 1> xzyy;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 2, 1> xzyz;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 3, 1> xzyw;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 0, 2> xzzx;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 1, 2> xzzy;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 2, 2> xzzz;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 3, 2> xzzw;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 0, 3> xzwx;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 1, 3> xzwy;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 2, 3> xzwz;
                uint4Swizzle<uint4, bool4, 4, 0, 2, 3, 3> xzww;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 0, 0> xwxx;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 1, 0> xwxy;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 2, 0> xwxz;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 3, 0> xwxw;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 0, 1> xwyx;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 1, 1> xwyy;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 2, 1> xwyz;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 3, 1> xwyw;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 0, 2> xwzx;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 1, 2> xwzy;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 2, 2> xwzz;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 3, 2> xwzw;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 0, 3> xwwx;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 1, 3> xwwy;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 2, 3> xwwz;
                uint4Swizzle<uint4, bool4, 4, 0, 3, 3, 3> xwww;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 0, 0> yxxx;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 1, 0> yxxy;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 2, 0> yxxz;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 3, 0> yxxw;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 0, 1> yxyx;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 1, 1> yxyy;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 2, 1> yxyz;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 3, 1> yxyw;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 0, 2> yxzx;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 1, 2> yxzy;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 2, 2> yxzz;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 3, 2> yxzw;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 0, 3> yxwx;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 1, 3> yxwy;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 2, 3> yxwz;
                uint4Swizzle<uint4, bool4, 4, 1, 0, 3, 3> yxww;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 0, 0> yyxx;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 1, 0> yyxy;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 2, 0> yyxz;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 3, 0> yyxw;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 0, 1> yyyx;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 1, 1> yyyy;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 2, 1> yyyz;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 3, 1> yyyw;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 0, 2> yyzx;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 1, 2> yyzy;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 2, 2> yyzz;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 3, 2> yyzw;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 0, 3> yywx;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 1, 3> yywy;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 2, 3> yywz;
                uint4Swizzle<uint4, bool4, 4, 1, 1, 3, 3> yyww;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 0, 0> yzxx;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 1, 0> yzxy;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 2, 0> yzxz;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 3, 0> yzxw;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 0, 1> yzyx;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 1, 1> yzyy;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 2, 1> yzyz;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 3, 1> yzyw;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 0, 2> yzzx;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 1, 2> yzzy;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 2, 2> yzzz;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 3, 2> yzzw;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 0, 3> yzwx;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 1, 3> yzwy;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 2, 3> yzwz;
                uint4Swizzle<uint4, bool4, 4, 1, 2, 3, 3> yzww;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 0, 0> ywxx;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 1, 0> ywxy;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 2, 0> ywxz;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 3, 0> ywxw;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 0, 1> ywyx;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 1, 1> ywyy;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 2, 1> ywyz;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 3, 1> ywyw;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 0, 2> ywzx;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 1, 2> ywzy;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 2, 2> ywzz;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 3, 2> ywzw;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 0, 3> ywwx;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 1, 3> ywwy;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 2, 3> ywwz;
                uint4Swizzle<uint4, bool4, 4, 1, 3, 3, 3> ywww;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 0, 0> zxxx;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 1, 0> zxxy;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 2, 0> zxxz;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 3, 0> zxxw;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 0, 1> zxyx;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 1, 1> zxyy;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 2, 1> zxyz;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 3, 1> zxyw;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 0, 2> zxzx;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 1, 2> zxzy;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 2, 2> zxzz;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 3, 2> zxzw;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 0, 3> zxwx;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 1, 3> zxwy;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 2, 3> zxwz;
                uint4Swizzle<uint4, bool4, 4, 2, 0, 3, 3> zxww;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 0, 0> zyxx;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 1, 0> zyxy;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 2, 0> zyxz;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 3, 0> zyxw;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 0, 1> zyyx;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 1, 1> zyyy;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 2, 1> zyyz;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 3, 1> zyyw;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 0, 2> zyzx;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 1, 2> zyzy;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 2, 2> zyzz;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 3, 2> zyzw;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 0, 3> zywx;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 1, 3> zywy;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 2, 3> zywz;
                uint4Swizzle<uint4, bool4, 4, 2, 1, 3, 3> zyww;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 0, 0> zzxx;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 1, 0> zzxy;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 2, 0> zzxz;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 3, 0> zzxw;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 0, 1> zzyx;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 1, 1> zzyy;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 2, 1> zzyz;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 3, 1> zzyw;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 0, 2> zzzx;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 1, 2> zzzy;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 2, 2> zzzz;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 3, 2> zzzw;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 0, 3> zzwx;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 1, 3> zzwy;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 2, 3> zzwz;
                uint4Swizzle<uint4, bool4, 4, 2, 2, 3, 3> zzww;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 0, 0> zwxx;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 1, 0> zwxy;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 2, 0> zwxz;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 3, 0> zwxw;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 0, 1> zwyx;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 1, 1> zwyy;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 2, 1> zwyz;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 3, 1> zwyw;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 0, 2> zwzx;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 1, 2> zwzy;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 2, 2> zwzz;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 3, 2> zwzw;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 0, 3> zwwx;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 1, 3> zwwy;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 2, 3> zwwz;
                uint4Swizzle<uint4, bool4, 4, 2, 3, 3, 3> zwww;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 0, 0> wxxx;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 1, 0> wxxy;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 2, 0> wxxz;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 3, 0> wxxw;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 0, 1> wxyx;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 1, 1> wxyy;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 2, 1> wxyz;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 3, 1> wxyw;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 0, 2> wxzx;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 1, 2> wxzy;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 2, 2> wxzz;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 3, 2> wxzw;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 0, 3> wxwx;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 1, 3> wxwy;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 2, 3> wxwz;
                uint4Swizzle<uint4, bool4, 4, 3, 0, 3, 3> wxww;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 0, 0> wyxx;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 1, 0> wyxy;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 2, 0> wyxz;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 3, 0> wyxw;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 0, 1> wyyx;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 1, 1> wyyy;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 2, 1> wyyz;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 3, 1> wyyw;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 0, 2> wyzx;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 1, 2> wyzy;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 2, 2> wyzz;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 3, 2> wyzw;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 0, 3> wywx;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 1, 3> wywy;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 2, 3> wywz;
                uint4Swizzle<uint4, bool4, 4, 3, 1, 3, 3> wyww;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 0, 0> wzxx;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 1, 0> wzxy;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 2, 0> wzxz;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 3, 0> wzxw;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 0, 1> wzyx;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 1, 1> wzyy;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 2, 1> wzyz;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 3, 1> wzyw;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 0, 2> wzzx;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 1, 2> wzzy;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 2, 2> wzzz;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 3, 2> wzzw;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 0, 3> wzwx;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 1, 3> wzwy;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 2, 3> wzwz;
                uint4Swizzle<uint4, bool4, 4, 3, 2, 3, 3> wzww;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 0, 0> wwxx;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 1, 0> wwxy;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 2, 0> wwxz;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 3, 0> wwxw;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 0, 1> wwyx;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 1, 1> wwyy;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 2, 1> wwyz;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 3, 1> wwyw;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 0, 2> wwzx;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 1, 2> wwzy;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 2, 2> wwzz;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 3, 2> wwzw;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 0, 3> wwwx;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 1, 3> wwwy;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 2, 3> wwwz;
                uint4Swizzle<uint4, bool4, 4, 3, 3, 3, 3> wwww;
            };

            uint4(unsigned int x, unsigned int y, unsigned int z, unsigned int w);

            uint4(unsigned int x, unsigned int y, const uint2 & zw);

            uint4(const uint2 & xy, const uint2 & zw);

            uint4(const uint2 & xy, unsigned int z, unsigned int w);

            uint4(const uint3 & xyz, unsigned int w);

            uint4(unsigned int x, const uint3 & yzw);

            uint4(const uint4 & xyzw);

            uint4();

            uint4 & operator=(const uint4 & rhs) noexcept;
uint4 & operator=(unsigned int rhs) noexcept;

            uint4 & operator+=(const uint4 & rhs) noexcept;
uint4 & operator+=(unsigned int rhs) noexcept;

            uint4 & operator-=(const uint4 & rhs) noexcept;
uint4 & operator-=(unsigned int rhs) noexcept;

            uint4 & operator/=(const uint4 & rhs) noexcept;
uint4 & operator/=(unsigned int rhs) noexcept;

            uint4 & operator*=(const uint4 & rhs) noexcept;
uint4 & operator*=(unsigned int rhs) noexcept;

            uint4 & operator%=(const uint4 & rhs) noexcept;
uint4 & operator%=(unsigned int rhs) noexcept;

            uint4 & operator|=(const uint4 & rhs) noexcept;
uint4 & operator|=(unsigned int rhs) noexcept;

            uint4 & operator&=(const uint4 & rhs) noexcept;
uint4 & operator&=(unsigned int rhs) noexcept;

            uint4 & operator^=(const uint4 & rhs) noexcept;
uint4 & operator^=(unsigned int rhs) noexcept;

            uint4 & operator<<=(const uint4 & rhs) noexcept;
uint4 & operator<<=(unsigned int rhs) noexcept;

            uint4 & operator>>=(const uint4 & rhs) noexcept;
uint4 & operator>>=(unsigned int rhs) noexcept;

            uint4 operator*(const uint4 & rhs) const noexcept;

            uint4 operator/(const uint4 & rhs) const noexcept;

            uint4 operator+(const uint4 & rhs) const noexcept;

            uint4 operator-(const uint4 & rhs) const noexcept;

            uint4 operator%(const uint4 & rhs) const noexcept;

            uint4 operator|(const uint4 & rhs) const noexcept;

            uint4 operator&(const uint4 & rhs) const noexcept;

            uint4 operator^(const uint4 & rhs) const noexcept;

            uint4 operator<<(const uint4 & rhs) const noexcept;

            uint4 operator>>(const uint4 & rhs) const noexcept;

            uint4 operator||(const uint4 & rhs) const noexcept;

            uint4 operator&&(const uint4 & rhs) const noexcept;

            bool4 operator<(const uint4 & rhs) const noexcept;

            bool4 operator>(const uint4 & rhs) const noexcept;

            bool4 operator!=(const uint4 & rhs) const noexcept;

            bool4 operator==(const uint4 & rhs) const noexcept;

            bool4 operator>=(const uint4 & rhs) const noexcept;

            bool4 operator<=(const uint4 & rhs) const noexcept;

            uint4 operator~() const noexcept;

            uint4 operator!() const noexcept;

            uint4 operator++() noexcept;

            uint4 operator++(int) noexcept;

            uint4 operator--() noexcept;

            uint4 operator--(int) noexcept;

            static uint4 Random(unsigned int lower = 0, unsigned int upper = 6) noexcept;

            static const uint4 One;
            static const uint4 Zero;
            static const uint4 UnitX;
            static const uint4 UnitY;
            static const uint4 UnitZ;
            static const uint4 UnitW;
        };
    }
}

