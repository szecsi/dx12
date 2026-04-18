#pragma once

#include "int3.h"
#include "bool4.h"
#include "int4.h"

namespace Egg {
    namespace Math {

        class int2;
        class int3;
        class bool2;
        class bool3;
        class bool4;

        class int4 {
        public:
            union {
                struct {
                    int x;
                    int y;
                    int z;
                    int w;
                };

                int2Swizzle<int2, bool2, 4, 0, 0> xx;
                int2Swizzle<int2, bool2, 4, 0, 1> xy;
                int2Swizzle<int2, bool2, 4, 0, 2> xz;
                int2Swizzle<int2, bool2, 4, 0, 3> xw;
                int2Swizzle<int2, bool2, 4, 1, 0> yx;
                int2Swizzle<int2, bool2, 4, 1, 1> yy;
                int2Swizzle<int2, bool2, 4, 1, 2> yz;
                int2Swizzle<int2, bool2, 4, 1, 3> yw;
                int2Swizzle<int2, bool2, 4, 2, 0> zx;
                int2Swizzle<int2, bool2, 4, 2, 1> zy;
                int2Swizzle<int2, bool2, 4, 2, 2> zz;
                int2Swizzle<int2, bool2, 4, 2, 3> zw;
                int2Swizzle<int2, bool2, 4, 3, 0> wx;
                int2Swizzle<int2, bool2, 4, 3, 1> wy;
                int2Swizzle<int2, bool2, 4, 3, 2> wz;
                int2Swizzle<int2, bool2, 4, 3, 3> ww;

                int3Swizzle<int3, bool3, 4, 0, 0, 0> xxx;
                int3Swizzle<int3, bool3, 4, 0, 0, 1> xxy;
                int3Swizzle<int3, bool3, 4, 0, 0, 2> xxz;
                int3Swizzle<int3, bool3, 4, 0, 0, 3> xxw;
                int3Swizzle<int3, bool3, 4, 0, 1, 0> xyx;
                int3Swizzle<int3, bool3, 4, 0, 1, 1> xyy;
                int3Swizzle<int3, bool3, 4, 0, 1, 2> xyz;
                int3Swizzle<int3, bool3, 4, 0, 1, 3> xyw;
                int3Swizzle<int3, bool3, 4, 0, 2, 0> xzx;
                int3Swizzle<int3, bool3, 4, 0, 2, 1> xzy;
                int3Swizzle<int3, bool3, 4, 0, 2, 2> xzz;
                int3Swizzle<int3, bool3, 4, 0, 2, 3> xzw;
                int3Swizzle<int3, bool3, 4, 0, 3, 0> xwx;
                int3Swizzle<int3, bool3, 4, 0, 3, 1> xwy;
                int3Swizzle<int3, bool3, 4, 0, 3, 2> xwz;
                int3Swizzle<int3, bool3, 4, 0, 3, 3> xww;
                int3Swizzle<int3, bool3, 4, 1, 0, 0> yxx;
                int3Swizzle<int3, bool3, 4, 1, 0, 1> yxy;
                int3Swizzle<int3, bool3, 4, 1, 0, 2> yxz;
                int3Swizzle<int3, bool3, 4, 1, 0, 3> yxw;
                int3Swizzle<int3, bool3, 4, 1, 1, 0> yyx;
                int3Swizzle<int3, bool3, 4, 1, 1, 1> yyy;
                int3Swizzle<int3, bool3, 4, 1, 1, 2> yyz;
                int3Swizzle<int3, bool3, 4, 1, 1, 3> yyw;
                int3Swizzle<int3, bool3, 4, 1, 2, 0> yzx;
                int3Swizzle<int3, bool3, 4, 1, 2, 1> yzy;
                int3Swizzle<int3, bool3, 4, 1, 2, 2> yzz;
                int3Swizzle<int3, bool3, 4, 1, 2, 3> yzw;
                int3Swizzle<int3, bool3, 4, 1, 3, 0> ywx;
                int3Swizzle<int3, bool3, 4, 1, 3, 1> ywy;
                int3Swizzle<int3, bool3, 4, 1, 3, 2> ywz;
                int3Swizzle<int3, bool3, 4, 1, 3, 3> yww;
                int3Swizzle<int3, bool3, 4, 2, 0, 0> zxx;
                int3Swizzle<int3, bool3, 4, 2, 0, 1> zxy;
                int3Swizzle<int3, bool3, 4, 2, 0, 2> zxz;
                int3Swizzle<int3, bool3, 4, 2, 0, 3> zxw;
                int3Swizzle<int3, bool3, 4, 2, 1, 0> zyx;
                int3Swizzle<int3, bool3, 4, 2, 1, 1> zyy;
                int3Swizzle<int3, bool3, 4, 2, 1, 2> zyz;
                int3Swizzle<int3, bool3, 4, 2, 1, 3> zyw;
                int3Swizzle<int3, bool3, 4, 2, 2, 0> zzx;
                int3Swizzle<int3, bool3, 4, 2, 2, 1> zzy;
                int3Swizzle<int3, bool3, 4, 2, 2, 2> zzz;
                int3Swizzle<int3, bool3, 4, 2, 2, 3> zzw;
                int3Swizzle<int3, bool3, 4, 2, 3, 0> zwx;
                int3Swizzle<int3, bool3, 4, 2, 3, 1> zwy;
                int3Swizzle<int3, bool3, 4, 2, 3, 2> zwz;
                int3Swizzle<int3, bool3, 4, 2, 3, 3> zww;
                int3Swizzle<int3, bool3, 4, 3, 0, 0> wxx;
                int3Swizzle<int3, bool3, 4, 3, 0, 1> wxy;
                int3Swizzle<int3, bool3, 4, 3, 0, 2> wxz;
                int3Swizzle<int3, bool3, 4, 3, 0, 3> wxw;
                int3Swizzle<int3, bool3, 4, 3, 1, 0> wyx;
                int3Swizzle<int3, bool3, 4, 3, 1, 1> wyy;
                int3Swizzle<int3, bool3, 4, 3, 1, 2> wyz;
                int3Swizzle<int3, bool3, 4, 3, 1, 3> wyw;
                int3Swizzle<int3, bool3, 4, 3, 2, 0> wzx;
                int3Swizzle<int3, bool3, 4, 3, 2, 1> wzy;
                int3Swizzle<int3, bool3, 4, 3, 2, 2> wzz;
                int3Swizzle<int3, bool3, 4, 3, 2, 3> wzw;
                int3Swizzle<int3, bool3, 4, 3, 3, 0> wwx;
                int3Swizzle<int3, bool3, 4, 3, 3, 1> wwy;
                int3Swizzle<int3, bool3, 4, 3, 3, 2> wwz;
                int3Swizzle<int3, bool3, 4, 3, 3, 3> www;

                int4Swizzle<int4, bool4, 4, 0, 0, 0, 0> xxxx;
                int4Swizzle<int4, bool4, 4, 0, 0, 1, 0> xxxy;
                int4Swizzle<int4, bool4, 4, 0, 0, 2, 0> xxxz;
                int4Swizzle<int4, bool4, 4, 0, 0, 3, 0> xxxw;
                int4Swizzle<int4, bool4, 4, 0, 0, 0, 1> xxyx;
                int4Swizzle<int4, bool4, 4, 0, 0, 1, 1> xxyy;
                int4Swizzle<int4, bool4, 4, 0, 0, 2, 1> xxyz;
                int4Swizzle<int4, bool4, 4, 0, 0, 3, 1> xxyw;
                int4Swizzle<int4, bool4, 4, 0, 0, 0, 2> xxzx;
                int4Swizzle<int4, bool4, 4, 0, 0, 1, 2> xxzy;
                int4Swizzle<int4, bool4, 4, 0, 0, 2, 2> xxzz;
                int4Swizzle<int4, bool4, 4, 0, 0, 3, 2> xxzw;
                int4Swizzle<int4, bool4, 4, 0, 0, 0, 3> xxwx;
                int4Swizzle<int4, bool4, 4, 0, 0, 1, 3> xxwy;
                int4Swizzle<int4, bool4, 4, 0, 0, 2, 3> xxwz;
                int4Swizzle<int4, bool4, 4, 0, 0, 3, 3> xxww;
                int4Swizzle<int4, bool4, 4, 0, 1, 0, 0> xyxx;
                int4Swizzle<int4, bool4, 4, 0, 1, 1, 0> xyxy;
                int4Swizzle<int4, bool4, 4, 0, 1, 2, 0> xyxz;
                int4Swizzle<int4, bool4, 4, 0, 1, 3, 0> xyxw;
                int4Swizzle<int4, bool4, 4, 0, 1, 0, 1> xyyx;
                int4Swizzle<int4, bool4, 4, 0, 1, 1, 1> xyyy;
                int4Swizzle<int4, bool4, 4, 0, 1, 2, 1> xyyz;
                int4Swizzle<int4, bool4, 4, 0, 1, 3, 1> xyyw;
                int4Swizzle<int4, bool4, 4, 0, 1, 0, 2> xyzx;
                int4Swizzle<int4, bool4, 4, 0, 1, 1, 2> xyzy;
                int4Swizzle<int4, bool4, 4, 0, 1, 2, 2> xyzz;
                int4Swizzle<int4, bool4, 4, 0, 1, 3, 2> xyzw;
                int4Swizzle<int4, bool4, 4, 0, 1, 0, 3> xywx;
                int4Swizzle<int4, bool4, 4, 0, 1, 1, 3> xywy;
                int4Swizzle<int4, bool4, 4, 0, 1, 2, 3> xywz;
                int4Swizzle<int4, bool4, 4, 0, 1, 3, 3> xyww;
                int4Swizzle<int4, bool4, 4, 0, 2, 0, 0> xzxx;
                int4Swizzle<int4, bool4, 4, 0, 2, 1, 0> xzxy;
                int4Swizzle<int4, bool4, 4, 0, 2, 2, 0> xzxz;
                int4Swizzle<int4, bool4, 4, 0, 2, 3, 0> xzxw;
                int4Swizzle<int4, bool4, 4, 0, 2, 0, 1> xzyx;
                int4Swizzle<int4, bool4, 4, 0, 2, 1, 1> xzyy;
                int4Swizzle<int4, bool4, 4, 0, 2, 2, 1> xzyz;
                int4Swizzle<int4, bool4, 4, 0, 2, 3, 1> xzyw;
                int4Swizzle<int4, bool4, 4, 0, 2, 0, 2> xzzx;
                int4Swizzle<int4, bool4, 4, 0, 2, 1, 2> xzzy;
                int4Swizzle<int4, bool4, 4, 0, 2, 2, 2> xzzz;
                int4Swizzle<int4, bool4, 4, 0, 2, 3, 2> xzzw;
                int4Swizzle<int4, bool4, 4, 0, 2, 0, 3> xzwx;
                int4Swizzle<int4, bool4, 4, 0, 2, 1, 3> xzwy;
                int4Swizzle<int4, bool4, 4, 0, 2, 2, 3> xzwz;
                int4Swizzle<int4, bool4, 4, 0, 2, 3, 3> xzww;
                int4Swizzle<int4, bool4, 4, 0, 3, 0, 0> xwxx;
                int4Swizzle<int4, bool4, 4, 0, 3, 1, 0> xwxy;
                int4Swizzle<int4, bool4, 4, 0, 3, 2, 0> xwxz;
                int4Swizzle<int4, bool4, 4, 0, 3, 3, 0> xwxw;
                int4Swizzle<int4, bool4, 4, 0, 3, 0, 1> xwyx;
                int4Swizzle<int4, bool4, 4, 0, 3, 1, 1> xwyy;
                int4Swizzle<int4, bool4, 4, 0, 3, 2, 1> xwyz;
                int4Swizzle<int4, bool4, 4, 0, 3, 3, 1> xwyw;
                int4Swizzle<int4, bool4, 4, 0, 3, 0, 2> xwzx;
                int4Swizzle<int4, bool4, 4, 0, 3, 1, 2> xwzy;
                int4Swizzle<int4, bool4, 4, 0, 3, 2, 2> xwzz;
                int4Swizzle<int4, bool4, 4, 0, 3, 3, 2> xwzw;
                int4Swizzle<int4, bool4, 4, 0, 3, 0, 3> xwwx;
                int4Swizzle<int4, bool4, 4, 0, 3, 1, 3> xwwy;
                int4Swizzle<int4, bool4, 4, 0, 3, 2, 3> xwwz;
                int4Swizzle<int4, bool4, 4, 0, 3, 3, 3> xwww;
                int4Swizzle<int4, bool4, 4, 1, 0, 0, 0> yxxx;
                int4Swizzle<int4, bool4, 4, 1, 0, 1, 0> yxxy;
                int4Swizzle<int4, bool4, 4, 1, 0, 2, 0> yxxz;
                int4Swizzle<int4, bool4, 4, 1, 0, 3, 0> yxxw;
                int4Swizzle<int4, bool4, 4, 1, 0, 0, 1> yxyx;
                int4Swizzle<int4, bool4, 4, 1, 0, 1, 1> yxyy;
                int4Swizzle<int4, bool4, 4, 1, 0, 2, 1> yxyz;
                int4Swizzle<int4, bool4, 4, 1, 0, 3, 1> yxyw;
                int4Swizzle<int4, bool4, 4, 1, 0, 0, 2> yxzx;
                int4Swizzle<int4, bool4, 4, 1, 0, 1, 2> yxzy;
                int4Swizzle<int4, bool4, 4, 1, 0, 2, 2> yxzz;
                int4Swizzle<int4, bool4, 4, 1, 0, 3, 2> yxzw;
                int4Swizzle<int4, bool4, 4, 1, 0, 0, 3> yxwx;
                int4Swizzle<int4, bool4, 4, 1, 0, 1, 3> yxwy;
                int4Swizzle<int4, bool4, 4, 1, 0, 2, 3> yxwz;
                int4Swizzle<int4, bool4, 4, 1, 0, 3, 3> yxww;
                int4Swizzle<int4, bool4, 4, 1, 1, 0, 0> yyxx;
                int4Swizzle<int4, bool4, 4, 1, 1, 1, 0> yyxy;
                int4Swizzle<int4, bool4, 4, 1, 1, 2, 0> yyxz;
                int4Swizzle<int4, bool4, 4, 1, 1, 3, 0> yyxw;
                int4Swizzle<int4, bool4, 4, 1, 1, 0, 1> yyyx;
                int4Swizzle<int4, bool4, 4, 1, 1, 1, 1> yyyy;
                int4Swizzle<int4, bool4, 4, 1, 1, 2, 1> yyyz;
                int4Swizzle<int4, bool4, 4, 1, 1, 3, 1> yyyw;
                int4Swizzle<int4, bool4, 4, 1, 1, 0, 2> yyzx;
                int4Swizzle<int4, bool4, 4, 1, 1, 1, 2> yyzy;
                int4Swizzle<int4, bool4, 4, 1, 1, 2, 2> yyzz;
                int4Swizzle<int4, bool4, 4, 1, 1, 3, 2> yyzw;
                int4Swizzle<int4, bool4, 4, 1, 1, 0, 3> yywx;
                int4Swizzle<int4, bool4, 4, 1, 1, 1, 3> yywy;
                int4Swizzle<int4, bool4, 4, 1, 1, 2, 3> yywz;
                int4Swizzle<int4, bool4, 4, 1, 1, 3, 3> yyww;
                int4Swizzle<int4, bool4, 4, 1, 2, 0, 0> yzxx;
                int4Swizzle<int4, bool4, 4, 1, 2, 1, 0> yzxy;
                int4Swizzle<int4, bool4, 4, 1, 2, 2, 0> yzxz;
                int4Swizzle<int4, bool4, 4, 1, 2, 3, 0> yzxw;
                int4Swizzle<int4, bool4, 4, 1, 2, 0, 1> yzyx;
                int4Swizzle<int4, bool4, 4, 1, 2, 1, 1> yzyy;
                int4Swizzle<int4, bool4, 4, 1, 2, 2, 1> yzyz;
                int4Swizzle<int4, bool4, 4, 1, 2, 3, 1> yzyw;
                int4Swizzle<int4, bool4, 4, 1, 2, 0, 2> yzzx;
                int4Swizzle<int4, bool4, 4, 1, 2, 1, 2> yzzy;
                int4Swizzle<int4, bool4, 4, 1, 2, 2, 2> yzzz;
                int4Swizzle<int4, bool4, 4, 1, 2, 3, 2> yzzw;
                int4Swizzle<int4, bool4, 4, 1, 2, 0, 3> yzwx;
                int4Swizzle<int4, bool4, 4, 1, 2, 1, 3> yzwy;
                int4Swizzle<int4, bool4, 4, 1, 2, 2, 3> yzwz;
                int4Swizzle<int4, bool4, 4, 1, 2, 3, 3> yzww;
                int4Swizzle<int4, bool4, 4, 1, 3, 0, 0> ywxx;
                int4Swizzle<int4, bool4, 4, 1, 3, 1, 0> ywxy;
                int4Swizzle<int4, bool4, 4, 1, 3, 2, 0> ywxz;
                int4Swizzle<int4, bool4, 4, 1, 3, 3, 0> ywxw;
                int4Swizzle<int4, bool4, 4, 1, 3, 0, 1> ywyx;
                int4Swizzle<int4, bool4, 4, 1, 3, 1, 1> ywyy;
                int4Swizzle<int4, bool4, 4, 1, 3, 2, 1> ywyz;
                int4Swizzle<int4, bool4, 4, 1, 3, 3, 1> ywyw;
                int4Swizzle<int4, bool4, 4, 1, 3, 0, 2> ywzx;
                int4Swizzle<int4, bool4, 4, 1, 3, 1, 2> ywzy;
                int4Swizzle<int4, bool4, 4, 1, 3, 2, 2> ywzz;
                int4Swizzle<int4, bool4, 4, 1, 3, 3, 2> ywzw;
                int4Swizzle<int4, bool4, 4, 1, 3, 0, 3> ywwx;
                int4Swizzle<int4, bool4, 4, 1, 3, 1, 3> ywwy;
                int4Swizzle<int4, bool4, 4, 1, 3, 2, 3> ywwz;
                int4Swizzle<int4, bool4, 4, 1, 3, 3, 3> ywww;
                int4Swizzle<int4, bool4, 4, 2, 0, 0, 0> zxxx;
                int4Swizzle<int4, bool4, 4, 2, 0, 1, 0> zxxy;
                int4Swizzle<int4, bool4, 4, 2, 0, 2, 0> zxxz;
                int4Swizzle<int4, bool4, 4, 2, 0, 3, 0> zxxw;
                int4Swizzle<int4, bool4, 4, 2, 0, 0, 1> zxyx;
                int4Swizzle<int4, bool4, 4, 2, 0, 1, 1> zxyy;
                int4Swizzle<int4, bool4, 4, 2, 0, 2, 1> zxyz;
                int4Swizzle<int4, bool4, 4, 2, 0, 3, 1> zxyw;
                int4Swizzle<int4, bool4, 4, 2, 0, 0, 2> zxzx;
                int4Swizzle<int4, bool4, 4, 2, 0, 1, 2> zxzy;
                int4Swizzle<int4, bool4, 4, 2, 0, 2, 2> zxzz;
                int4Swizzle<int4, bool4, 4, 2, 0, 3, 2> zxzw;
                int4Swizzle<int4, bool4, 4, 2, 0, 0, 3> zxwx;
                int4Swizzle<int4, bool4, 4, 2, 0, 1, 3> zxwy;
                int4Swizzle<int4, bool4, 4, 2, 0, 2, 3> zxwz;
                int4Swizzle<int4, bool4, 4, 2, 0, 3, 3> zxww;
                int4Swizzle<int4, bool4, 4, 2, 1, 0, 0> zyxx;
                int4Swizzle<int4, bool4, 4, 2, 1, 1, 0> zyxy;
                int4Swizzle<int4, bool4, 4, 2, 1, 2, 0> zyxz;
                int4Swizzle<int4, bool4, 4, 2, 1, 3, 0> zyxw;
                int4Swizzle<int4, bool4, 4, 2, 1, 0, 1> zyyx;
                int4Swizzle<int4, bool4, 4, 2, 1, 1, 1> zyyy;
                int4Swizzle<int4, bool4, 4, 2, 1, 2, 1> zyyz;
                int4Swizzle<int4, bool4, 4, 2, 1, 3, 1> zyyw;
                int4Swizzle<int4, bool4, 4, 2, 1, 0, 2> zyzx;
                int4Swizzle<int4, bool4, 4, 2, 1, 1, 2> zyzy;
                int4Swizzle<int4, bool4, 4, 2, 1, 2, 2> zyzz;
                int4Swizzle<int4, bool4, 4, 2, 1, 3, 2> zyzw;
                int4Swizzle<int4, bool4, 4, 2, 1, 0, 3> zywx;
                int4Swizzle<int4, bool4, 4, 2, 1, 1, 3> zywy;
                int4Swizzle<int4, bool4, 4, 2, 1, 2, 3> zywz;
                int4Swizzle<int4, bool4, 4, 2, 1, 3, 3> zyww;
                int4Swizzle<int4, bool4, 4, 2, 2, 0, 0> zzxx;
                int4Swizzle<int4, bool4, 4, 2, 2, 1, 0> zzxy;
                int4Swizzle<int4, bool4, 4, 2, 2, 2, 0> zzxz;
                int4Swizzle<int4, bool4, 4, 2, 2, 3, 0> zzxw;
                int4Swizzle<int4, bool4, 4, 2, 2, 0, 1> zzyx;
                int4Swizzle<int4, bool4, 4, 2, 2, 1, 1> zzyy;
                int4Swizzle<int4, bool4, 4, 2, 2, 2, 1> zzyz;
                int4Swizzle<int4, bool4, 4, 2, 2, 3, 1> zzyw;
                int4Swizzle<int4, bool4, 4, 2, 2, 0, 2> zzzx;
                int4Swizzle<int4, bool4, 4, 2, 2, 1, 2> zzzy;
                int4Swizzle<int4, bool4, 4, 2, 2, 2, 2> zzzz;
                int4Swizzle<int4, bool4, 4, 2, 2, 3, 2> zzzw;
                int4Swizzle<int4, bool4, 4, 2, 2, 0, 3> zzwx;
                int4Swizzle<int4, bool4, 4, 2, 2, 1, 3> zzwy;
                int4Swizzle<int4, bool4, 4, 2, 2, 2, 3> zzwz;
                int4Swizzle<int4, bool4, 4, 2, 2, 3, 3> zzww;
                int4Swizzle<int4, bool4, 4, 2, 3, 0, 0> zwxx;
                int4Swizzle<int4, bool4, 4, 2, 3, 1, 0> zwxy;
                int4Swizzle<int4, bool4, 4, 2, 3, 2, 0> zwxz;
                int4Swizzle<int4, bool4, 4, 2, 3, 3, 0> zwxw;
                int4Swizzle<int4, bool4, 4, 2, 3, 0, 1> zwyx;
                int4Swizzle<int4, bool4, 4, 2, 3, 1, 1> zwyy;
                int4Swizzle<int4, bool4, 4, 2, 3, 2, 1> zwyz;
                int4Swizzle<int4, bool4, 4, 2, 3, 3, 1> zwyw;
                int4Swizzle<int4, bool4, 4, 2, 3, 0, 2> zwzx;
                int4Swizzle<int4, bool4, 4, 2, 3, 1, 2> zwzy;
                int4Swizzle<int4, bool4, 4, 2, 3, 2, 2> zwzz;
                int4Swizzle<int4, bool4, 4, 2, 3, 3, 2> zwzw;
                int4Swizzle<int4, bool4, 4, 2, 3, 0, 3> zwwx;
                int4Swizzle<int4, bool4, 4, 2, 3, 1, 3> zwwy;
                int4Swizzle<int4, bool4, 4, 2, 3, 2, 3> zwwz;
                int4Swizzle<int4, bool4, 4, 2, 3, 3, 3> zwww;
                int4Swizzle<int4, bool4, 4, 3, 0, 0, 0> wxxx;
                int4Swizzle<int4, bool4, 4, 3, 0, 1, 0> wxxy;
                int4Swizzle<int4, bool4, 4, 3, 0, 2, 0> wxxz;
                int4Swizzle<int4, bool4, 4, 3, 0, 3, 0> wxxw;
                int4Swizzle<int4, bool4, 4, 3, 0, 0, 1> wxyx;
                int4Swizzle<int4, bool4, 4, 3, 0, 1, 1> wxyy;
                int4Swizzle<int4, bool4, 4, 3, 0, 2, 1> wxyz;
                int4Swizzle<int4, bool4, 4, 3, 0, 3, 1> wxyw;
                int4Swizzle<int4, bool4, 4, 3, 0, 0, 2> wxzx;
                int4Swizzle<int4, bool4, 4, 3, 0, 1, 2> wxzy;
                int4Swizzle<int4, bool4, 4, 3, 0, 2, 2> wxzz;
                int4Swizzle<int4, bool4, 4, 3, 0, 3, 2> wxzw;
                int4Swizzle<int4, bool4, 4, 3, 0, 0, 3> wxwx;
                int4Swizzle<int4, bool4, 4, 3, 0, 1, 3> wxwy;
                int4Swizzle<int4, bool4, 4, 3, 0, 2, 3> wxwz;
                int4Swizzle<int4, bool4, 4, 3, 0, 3, 3> wxww;
                int4Swizzle<int4, bool4, 4, 3, 1, 0, 0> wyxx;
                int4Swizzle<int4, bool4, 4, 3, 1, 1, 0> wyxy;
                int4Swizzle<int4, bool4, 4, 3, 1, 2, 0> wyxz;
                int4Swizzle<int4, bool4, 4, 3, 1, 3, 0> wyxw;
                int4Swizzle<int4, bool4, 4, 3, 1, 0, 1> wyyx;
                int4Swizzle<int4, bool4, 4, 3, 1, 1, 1> wyyy;
                int4Swizzle<int4, bool4, 4, 3, 1, 2, 1> wyyz;
                int4Swizzle<int4, bool4, 4, 3, 1, 3, 1> wyyw;
                int4Swizzle<int4, bool4, 4, 3, 1, 0, 2> wyzx;
                int4Swizzle<int4, bool4, 4, 3, 1, 1, 2> wyzy;
                int4Swizzle<int4, bool4, 4, 3, 1, 2, 2> wyzz;
                int4Swizzle<int4, bool4, 4, 3, 1, 3, 2> wyzw;
                int4Swizzle<int4, bool4, 4, 3, 1, 0, 3> wywx;
                int4Swizzle<int4, bool4, 4, 3, 1, 1, 3> wywy;
                int4Swizzle<int4, bool4, 4, 3, 1, 2, 3> wywz;
                int4Swizzle<int4, bool4, 4, 3, 1, 3, 3> wyww;
                int4Swizzle<int4, bool4, 4, 3, 2, 0, 0> wzxx;
                int4Swizzle<int4, bool4, 4, 3, 2, 1, 0> wzxy;
                int4Swizzle<int4, bool4, 4, 3, 2, 2, 0> wzxz;
                int4Swizzle<int4, bool4, 4, 3, 2, 3, 0> wzxw;
                int4Swizzle<int4, bool4, 4, 3, 2, 0, 1> wzyx;
                int4Swizzle<int4, bool4, 4, 3, 2, 1, 1> wzyy;
                int4Swizzle<int4, bool4, 4, 3, 2, 2, 1> wzyz;
                int4Swizzle<int4, bool4, 4, 3, 2, 3, 1> wzyw;
                int4Swizzle<int4, bool4, 4, 3, 2, 0, 2> wzzx;
                int4Swizzle<int4, bool4, 4, 3, 2, 1, 2> wzzy;
                int4Swizzle<int4, bool4, 4, 3, 2, 2, 2> wzzz;
                int4Swizzle<int4, bool4, 4, 3, 2, 3, 2> wzzw;
                int4Swizzle<int4, bool4, 4, 3, 2, 0, 3> wzwx;
                int4Swizzle<int4, bool4, 4, 3, 2, 1, 3> wzwy;
                int4Swizzle<int4, bool4, 4, 3, 2, 2, 3> wzwz;
                int4Swizzle<int4, bool4, 4, 3, 2, 3, 3> wzww;
                int4Swizzle<int4, bool4, 4, 3, 3, 0, 0> wwxx;
                int4Swizzle<int4, bool4, 4, 3, 3, 1, 0> wwxy;
                int4Swizzle<int4, bool4, 4, 3, 3, 2, 0> wwxz;
                int4Swizzle<int4, bool4, 4, 3, 3, 3, 0> wwxw;
                int4Swizzle<int4, bool4, 4, 3, 3, 0, 1> wwyx;
                int4Swizzle<int4, bool4, 4, 3, 3, 1, 1> wwyy;
                int4Swizzle<int4, bool4, 4, 3, 3, 2, 1> wwyz;
                int4Swizzle<int4, bool4, 4, 3, 3, 3, 1> wwyw;
                int4Swizzle<int4, bool4, 4, 3, 3, 0, 2> wwzx;
                int4Swizzle<int4, bool4, 4, 3, 3, 1, 2> wwzy;
                int4Swizzle<int4, bool4, 4, 3, 3, 2, 2> wwzz;
                int4Swizzle<int4, bool4, 4, 3, 3, 3, 2> wwzw;
                int4Swizzle<int4, bool4, 4, 3, 3, 0, 3> wwwx;
                int4Swizzle<int4, bool4, 4, 3, 3, 1, 3> wwwy;
                int4Swizzle<int4, bool4, 4, 3, 3, 2, 3> wwwz;
                int4Swizzle<int4, bool4, 4, 3, 3, 3, 3> wwww;
            };

            int4(int x, int y, int z, int w);

            int4(int x, int y, const int2 & zw);

            int4(const int2 & xy, const int2 & zw);

            int4(const int2 & xy, int z, int w);

            int4(const int3 & xyz, int w);

            int4(int x, const int3 & yzw);

            int4(const int4 & xyzw);

            int4();

            int4 & operator=(const int4 & rhs) noexcept;
int4 & operator=(int rhs) noexcept;

            int4 & operator+=(const int4 & rhs) noexcept;
int4 & operator+=(int rhs) noexcept;

            int4 & operator-=(const int4 & rhs) noexcept;
int4 & operator-=(int rhs) noexcept;

            int4 & operator/=(const int4 & rhs) noexcept;
int4 & operator/=(int rhs) noexcept;

            int4 & operator*=(const int4 & rhs) noexcept;
int4 & operator*=(int rhs) noexcept;

            int4 & operator%=(const int4 & rhs) noexcept;
int4 & operator%=(int rhs) noexcept;

            int4 & operator|=(const int4 & rhs) noexcept;
int4 & operator|=(int rhs) noexcept;

            int4 & operator&=(const int4 & rhs) noexcept;
int4 & operator&=(int rhs) noexcept;

            int4 & operator^=(const int4 & rhs) noexcept;
int4 & operator^=(int rhs) noexcept;

            int4 & operator<<=(const int4 & rhs) noexcept;
int4 & operator<<=(int rhs) noexcept;

            int4 & operator>>=(const int4 & rhs) noexcept;
int4 & operator>>=(int rhs) noexcept;

            int4 operator*(const int4 & rhs) const noexcept;

            int4 operator/(const int4 & rhs) const noexcept;

            int4 operator+(const int4 & rhs) const noexcept;

            int4 operator-(const int4 & rhs) const noexcept;

            int4 operator%(const int4 & rhs) const noexcept;

            int4 operator|(const int4 & rhs) const noexcept;

            int4 operator&(const int4 & rhs) const noexcept;

            int4 operator^(const int4 & rhs) const noexcept;

            int4 operator<<(const int4 & rhs) const noexcept;

            int4 operator>>(const int4 & rhs) const noexcept;

            int4 operator||(const int4 & rhs) const noexcept;

            int4 operator&&(const int4 & rhs) const noexcept;

            bool4 operator<(const int4 & rhs) const noexcept;

            bool4 operator>(const int4 & rhs) const noexcept;

            bool4 operator!=(const int4 & rhs) const noexcept;

            bool4 operator==(const int4 & rhs) const noexcept;

            bool4 operator>=(const int4 & rhs) const noexcept;

            bool4 operator<=(const int4 & rhs) const noexcept;

            int4 operator~() const noexcept;

            int4 operator!() const noexcept;

            int4 operator++() noexcept;

            int4 operator++(int) noexcept;

            int4 operator--() noexcept;

            int4 operator--(int) noexcept;

            static int4 Random(int lower = 0, int upper = 6) noexcept;

            int4 operator-() const noexcept;

            static const int4 One;
            static const int4 Zero;
            static const int4 UnitX;
            static const int4 UnitY;
            static const int4 UnitZ;
            static const int4 UnitW;
        };
    }
}

