#pragma once

#include "bool3.h"
#include "bool2.h"

namespace Egg {
    namespace Math {

        class bool2;
        class bool3;

        class bool4 {
        public:
            union {
                struct {
                    bool x;
                    bool y;
                    bool z;
                    bool w;
                };

                bool2Swizzle<bool2, 4, 0, 0> xx;
                bool2Swizzle<bool2, 4, 0, 1> xy;
                bool2Swizzle<bool2, 4, 0, 2> xz;
                bool2Swizzle<bool2, 4, 0, 3> xw;
                bool2Swizzle<bool2, 4, 1, 0> yx;
                bool2Swizzle<bool2, 4, 1, 1> yy;
                bool2Swizzle<bool2, 4, 1, 2> yz;
                bool2Swizzle<bool2, 4, 1, 3> yw;
                bool2Swizzle<bool2, 4, 2, 0> zx;
                bool2Swizzle<bool2, 4, 2, 1> zy;
                bool2Swizzle<bool2, 4, 2, 2> zz;
                bool2Swizzle<bool2, 4, 2, 3> zw;
                bool2Swizzle<bool2, 4, 3, 0> wx;
                bool2Swizzle<bool2, 4, 3, 1> wy;
                bool2Swizzle<bool2, 4, 3, 2> wz;
                bool2Swizzle<bool2, 4, 3, 3> ww;

                bool3Swizzle<bool3, 4, 0, 0, 0> xxx;
                bool3Swizzle<bool3, 4, 0, 0, 1> xxy;
                bool3Swizzle<bool3, 4, 0, 0, 2> xxz;
                bool3Swizzle<bool3, 4, 0, 0, 3> xxw;
                bool3Swizzle<bool3, 4, 0, 1, 0> xyx;
                bool3Swizzle<bool3, 4, 0, 1, 1> xyy;
                bool3Swizzle<bool3, 4, 0, 1, 2> xyz;
                bool3Swizzle<bool3, 4, 0, 1, 3> xyw;
                bool3Swizzle<bool3, 4, 0, 2, 0> xzx;
                bool3Swizzle<bool3, 4, 0, 2, 1> xzy;
                bool3Swizzle<bool3, 4, 0, 2, 2> xzz;
                bool3Swizzle<bool3, 4, 0, 2, 3> xzw;
                bool3Swizzle<bool3, 4, 0, 3, 0> xwx;
                bool3Swizzle<bool3, 4, 0, 3, 1> xwy;
                bool3Swizzle<bool3, 4, 0, 3, 2> xwz;
                bool3Swizzle<bool3, 4, 0, 3, 3> xww;
                bool3Swizzle<bool3, 4, 1, 0, 0> yxx;
                bool3Swizzle<bool3, 4, 1, 0, 1> yxy;
                bool3Swizzle<bool3, 4, 1, 0, 2> yxz;
                bool3Swizzle<bool3, 4, 1, 0, 3> yxw;
                bool3Swizzle<bool3, 4, 1, 1, 0> yyx;
                bool3Swizzle<bool3, 4, 1, 1, 1> yyy;
                bool3Swizzle<bool3, 4, 1, 1, 2> yyz;
                bool3Swizzle<bool3, 4, 1, 1, 3> yyw;
                bool3Swizzle<bool3, 4, 1, 2, 0> yzx;
                bool3Swizzle<bool3, 4, 1, 2, 1> yzy;
                bool3Swizzle<bool3, 4, 1, 2, 2> yzz;
                bool3Swizzle<bool3, 4, 1, 2, 3> yzw;
                bool3Swizzle<bool3, 4, 1, 3, 0> ywx;
                bool3Swizzle<bool3, 4, 1, 3, 1> ywy;
                bool3Swizzle<bool3, 4, 1, 3, 2> ywz;
                bool3Swizzle<bool3, 4, 1, 3, 3> yww;
                bool3Swizzle<bool3, 4, 2, 0, 0> zxx;
                bool3Swizzle<bool3, 4, 2, 0, 1> zxy;
                bool3Swizzle<bool3, 4, 2, 0, 2> zxz;
                bool3Swizzle<bool3, 4, 2, 0, 3> zxw;
                bool3Swizzle<bool3, 4, 2, 1, 0> zyx;
                bool3Swizzle<bool3, 4, 2, 1, 1> zyy;
                bool3Swizzle<bool3, 4, 2, 1, 2> zyz;
                bool3Swizzle<bool3, 4, 2, 1, 3> zyw;
                bool3Swizzle<bool3, 4, 2, 2, 0> zzx;
                bool3Swizzle<bool3, 4, 2, 2, 1> zzy;
                bool3Swizzle<bool3, 4, 2, 2, 2> zzz;
                bool3Swizzle<bool3, 4, 2, 2, 3> zzw;
                bool3Swizzle<bool3, 4, 2, 3, 0> zwx;
                bool3Swizzle<bool3, 4, 2, 3, 1> zwy;
                bool3Swizzle<bool3, 4, 2, 3, 2> zwz;
                bool3Swizzle<bool3, 4, 2, 3, 3> zww;
                bool3Swizzle<bool3, 4, 3, 0, 0> wxx;
                bool3Swizzle<bool3, 4, 3, 0, 1> wxy;
                bool3Swizzle<bool3, 4, 3, 0, 2> wxz;
                bool3Swizzle<bool3, 4, 3, 0, 3> wxw;
                bool3Swizzle<bool3, 4, 3, 1, 0> wyx;
                bool3Swizzle<bool3, 4, 3, 1, 1> wyy;
                bool3Swizzle<bool3, 4, 3, 1, 2> wyz;
                bool3Swizzle<bool3, 4, 3, 1, 3> wyw;
                bool3Swizzle<bool3, 4, 3, 2, 0> wzx;
                bool3Swizzle<bool3, 4, 3, 2, 1> wzy;
                bool3Swizzle<bool3, 4, 3, 2, 2> wzz;
                bool3Swizzle<bool3, 4, 3, 2, 3> wzw;
                bool3Swizzle<bool3, 4, 3, 3, 0> wwx;
                bool3Swizzle<bool3, 4, 3, 3, 1> wwy;
                bool3Swizzle<bool3, 4, 3, 3, 2> wwz;
                bool3Swizzle<bool3, 4, 3, 3, 3> www;

                bool4Swizzle<bool4, 4, 0, 0, 0, 0> xxxx;
                bool4Swizzle<bool4, 4, 0, 0, 1, 0> xxxy;
                bool4Swizzle<bool4, 4, 0, 0, 2, 0> xxxz;
                bool4Swizzle<bool4, 4, 0, 0, 3, 0> xxxw;
                bool4Swizzle<bool4, 4, 0, 0, 0, 1> xxyx;
                bool4Swizzle<bool4, 4, 0, 0, 1, 1> xxyy;
                bool4Swizzle<bool4, 4, 0, 0, 2, 1> xxyz;
                bool4Swizzle<bool4, 4, 0, 0, 3, 1> xxyw;
                bool4Swizzle<bool4, 4, 0, 0, 0, 2> xxzx;
                bool4Swizzle<bool4, 4, 0, 0, 1, 2> xxzy;
                bool4Swizzle<bool4, 4, 0, 0, 2, 2> xxzz;
                bool4Swizzle<bool4, 4, 0, 0, 3, 2> xxzw;
                bool4Swizzle<bool4, 4, 0, 0, 0, 3> xxwx;
                bool4Swizzle<bool4, 4, 0, 0, 1, 3> xxwy;
                bool4Swizzle<bool4, 4, 0, 0, 2, 3> xxwz;
                bool4Swizzle<bool4, 4, 0, 0, 3, 3> xxww;
                bool4Swizzle<bool4, 4, 0, 1, 0, 0> xyxx;
                bool4Swizzle<bool4, 4, 0, 1, 1, 0> xyxy;
                bool4Swizzle<bool4, 4, 0, 1, 2, 0> xyxz;
                bool4Swizzle<bool4, 4, 0, 1, 3, 0> xyxw;
                bool4Swizzle<bool4, 4, 0, 1, 0, 1> xyyx;
                bool4Swizzle<bool4, 4, 0, 1, 1, 1> xyyy;
                bool4Swizzle<bool4, 4, 0, 1, 2, 1> xyyz;
                bool4Swizzle<bool4, 4, 0, 1, 3, 1> xyyw;
                bool4Swizzle<bool4, 4, 0, 1, 0, 2> xyzx;
                bool4Swizzle<bool4, 4, 0, 1, 1, 2> xyzy;
                bool4Swizzle<bool4, 4, 0, 1, 2, 2> xyzz;
                bool4Swizzle<bool4, 4, 0, 1, 3, 2> xyzw;
                bool4Swizzle<bool4, 4, 0, 1, 0, 3> xywx;
                bool4Swizzle<bool4, 4, 0, 1, 1, 3> xywy;
                bool4Swizzle<bool4, 4, 0, 1, 2, 3> xywz;
                bool4Swizzle<bool4, 4, 0, 1, 3, 3> xyww;
                bool4Swizzle<bool4, 4, 0, 2, 0, 0> xzxx;
                bool4Swizzle<bool4, 4, 0, 2, 1, 0> xzxy;
                bool4Swizzle<bool4, 4, 0, 2, 2, 0> xzxz;
                bool4Swizzle<bool4, 4, 0, 2, 3, 0> xzxw;
                bool4Swizzle<bool4, 4, 0, 2, 0, 1> xzyx;
                bool4Swizzle<bool4, 4, 0, 2, 1, 1> xzyy;
                bool4Swizzle<bool4, 4, 0, 2, 2, 1> xzyz;
                bool4Swizzle<bool4, 4, 0, 2, 3, 1> xzyw;
                bool4Swizzle<bool4, 4, 0, 2, 0, 2> xzzx;
                bool4Swizzle<bool4, 4, 0, 2, 1, 2> xzzy;
                bool4Swizzle<bool4, 4, 0, 2, 2, 2> xzzz;
                bool4Swizzle<bool4, 4, 0, 2, 3, 2> xzzw;
                bool4Swizzle<bool4, 4, 0, 2, 0, 3> xzwx;
                bool4Swizzle<bool4, 4, 0, 2, 1, 3> xzwy;
                bool4Swizzle<bool4, 4, 0, 2, 2, 3> xzwz;
                bool4Swizzle<bool4, 4, 0, 2, 3, 3> xzww;
                bool4Swizzle<bool4, 4, 0, 3, 0, 0> xwxx;
                bool4Swizzle<bool4, 4, 0, 3, 1, 0> xwxy;
                bool4Swizzle<bool4, 4, 0, 3, 2, 0> xwxz;
                bool4Swizzle<bool4, 4, 0, 3, 3, 0> xwxw;
                bool4Swizzle<bool4, 4, 0, 3, 0, 1> xwyx;
                bool4Swizzle<bool4, 4, 0, 3, 1, 1> xwyy;
                bool4Swizzle<bool4, 4, 0, 3, 2, 1> xwyz;
                bool4Swizzle<bool4, 4, 0, 3, 3, 1> xwyw;
                bool4Swizzle<bool4, 4, 0, 3, 0, 2> xwzx;
                bool4Swizzle<bool4, 4, 0, 3, 1, 2> xwzy;
                bool4Swizzle<bool4, 4, 0, 3, 2, 2> xwzz;
                bool4Swizzle<bool4, 4, 0, 3, 3, 2> xwzw;
                bool4Swizzle<bool4, 4, 0, 3, 0, 3> xwwx;
                bool4Swizzle<bool4, 4, 0, 3, 1, 3> xwwy;
                bool4Swizzle<bool4, 4, 0, 3, 2, 3> xwwz;
                bool4Swizzle<bool4, 4, 0, 3, 3, 3> xwww;
                bool4Swizzle<bool4, 4, 1, 0, 0, 0> yxxx;
                bool4Swizzle<bool4, 4, 1, 0, 1, 0> yxxy;
                bool4Swizzle<bool4, 4, 1, 0, 2, 0> yxxz;
                bool4Swizzle<bool4, 4, 1, 0, 3, 0> yxxw;
                bool4Swizzle<bool4, 4, 1, 0, 0, 1> yxyx;
                bool4Swizzle<bool4, 4, 1, 0, 1, 1> yxyy;
                bool4Swizzle<bool4, 4, 1, 0, 2, 1> yxyz;
                bool4Swizzle<bool4, 4, 1, 0, 3, 1> yxyw;
                bool4Swizzle<bool4, 4, 1, 0, 0, 2> yxzx;
                bool4Swizzle<bool4, 4, 1, 0, 1, 2> yxzy;
                bool4Swizzle<bool4, 4, 1, 0, 2, 2> yxzz;
                bool4Swizzle<bool4, 4, 1, 0, 3, 2> yxzw;
                bool4Swizzle<bool4, 4, 1, 0, 0, 3> yxwx;
                bool4Swizzle<bool4, 4, 1, 0, 1, 3> yxwy;
                bool4Swizzle<bool4, 4, 1, 0, 2, 3> yxwz;
                bool4Swizzle<bool4, 4, 1, 0, 3, 3> yxww;
                bool4Swizzle<bool4, 4, 1, 1, 0, 0> yyxx;
                bool4Swizzle<bool4, 4, 1, 1, 1, 0> yyxy;
                bool4Swizzle<bool4, 4, 1, 1, 2, 0> yyxz;
                bool4Swizzle<bool4, 4, 1, 1, 3, 0> yyxw;
                bool4Swizzle<bool4, 4, 1, 1, 0, 1> yyyx;
                bool4Swizzle<bool4, 4, 1, 1, 1, 1> yyyy;
                bool4Swizzle<bool4, 4, 1, 1, 2, 1> yyyz;
                bool4Swizzle<bool4, 4, 1, 1, 3, 1> yyyw;
                bool4Swizzle<bool4, 4, 1, 1, 0, 2> yyzx;
                bool4Swizzle<bool4, 4, 1, 1, 1, 2> yyzy;
                bool4Swizzle<bool4, 4, 1, 1, 2, 2> yyzz;
                bool4Swizzle<bool4, 4, 1, 1, 3, 2> yyzw;
                bool4Swizzle<bool4, 4, 1, 1, 0, 3> yywx;
                bool4Swizzle<bool4, 4, 1, 1, 1, 3> yywy;
                bool4Swizzle<bool4, 4, 1, 1, 2, 3> yywz;
                bool4Swizzle<bool4, 4, 1, 1, 3, 3> yyww;
                bool4Swizzle<bool4, 4, 1, 2, 0, 0> yzxx;
                bool4Swizzle<bool4, 4, 1, 2, 1, 0> yzxy;
                bool4Swizzle<bool4, 4, 1, 2, 2, 0> yzxz;
                bool4Swizzle<bool4, 4, 1, 2, 3, 0> yzxw;
                bool4Swizzle<bool4, 4, 1, 2, 0, 1> yzyx;
                bool4Swizzle<bool4, 4, 1, 2, 1, 1> yzyy;
                bool4Swizzle<bool4, 4, 1, 2, 2, 1> yzyz;
                bool4Swizzle<bool4, 4, 1, 2, 3, 1> yzyw;
                bool4Swizzle<bool4, 4, 1, 2, 0, 2> yzzx;
                bool4Swizzle<bool4, 4, 1, 2, 1, 2> yzzy;
                bool4Swizzle<bool4, 4, 1, 2, 2, 2> yzzz;
                bool4Swizzle<bool4, 4, 1, 2, 3, 2> yzzw;
                bool4Swizzle<bool4, 4, 1, 2, 0, 3> yzwx;
                bool4Swizzle<bool4, 4, 1, 2, 1, 3> yzwy;
                bool4Swizzle<bool4, 4, 1, 2, 2, 3> yzwz;
                bool4Swizzle<bool4, 4, 1, 2, 3, 3> yzww;
                bool4Swizzle<bool4, 4, 1, 3, 0, 0> ywxx;
                bool4Swizzle<bool4, 4, 1, 3, 1, 0> ywxy;
                bool4Swizzle<bool4, 4, 1, 3, 2, 0> ywxz;
                bool4Swizzle<bool4, 4, 1, 3, 3, 0> ywxw;
                bool4Swizzle<bool4, 4, 1, 3, 0, 1> ywyx;
                bool4Swizzle<bool4, 4, 1, 3, 1, 1> ywyy;
                bool4Swizzle<bool4, 4, 1, 3, 2, 1> ywyz;
                bool4Swizzle<bool4, 4, 1, 3, 3, 1> ywyw;
                bool4Swizzle<bool4, 4, 1, 3, 0, 2> ywzx;
                bool4Swizzle<bool4, 4, 1, 3, 1, 2> ywzy;
                bool4Swizzle<bool4, 4, 1, 3, 2, 2> ywzz;
                bool4Swizzle<bool4, 4, 1, 3, 3, 2> ywzw;
                bool4Swizzle<bool4, 4, 1, 3, 0, 3> ywwx;
                bool4Swizzle<bool4, 4, 1, 3, 1, 3> ywwy;
                bool4Swizzle<bool4, 4, 1, 3, 2, 3> ywwz;
                bool4Swizzle<bool4, 4, 1, 3, 3, 3> ywww;
                bool4Swizzle<bool4, 4, 2, 0, 0, 0> zxxx;
                bool4Swizzle<bool4, 4, 2, 0, 1, 0> zxxy;
                bool4Swizzle<bool4, 4, 2, 0, 2, 0> zxxz;
                bool4Swizzle<bool4, 4, 2, 0, 3, 0> zxxw;
                bool4Swizzle<bool4, 4, 2, 0, 0, 1> zxyx;
                bool4Swizzle<bool4, 4, 2, 0, 1, 1> zxyy;
                bool4Swizzle<bool4, 4, 2, 0, 2, 1> zxyz;
                bool4Swizzle<bool4, 4, 2, 0, 3, 1> zxyw;
                bool4Swizzle<bool4, 4, 2, 0, 0, 2> zxzx;
                bool4Swizzle<bool4, 4, 2, 0, 1, 2> zxzy;
                bool4Swizzle<bool4, 4, 2, 0, 2, 2> zxzz;
                bool4Swizzle<bool4, 4, 2, 0, 3, 2> zxzw;
                bool4Swizzle<bool4, 4, 2, 0, 0, 3> zxwx;
                bool4Swizzle<bool4, 4, 2, 0, 1, 3> zxwy;
                bool4Swizzle<bool4, 4, 2, 0, 2, 3> zxwz;
                bool4Swizzle<bool4, 4, 2, 0, 3, 3> zxww;
                bool4Swizzle<bool4, 4, 2, 1, 0, 0> zyxx;
                bool4Swizzle<bool4, 4, 2, 1, 1, 0> zyxy;
                bool4Swizzle<bool4, 4, 2, 1, 2, 0> zyxz;
                bool4Swizzle<bool4, 4, 2, 1, 3, 0> zyxw;
                bool4Swizzle<bool4, 4, 2, 1, 0, 1> zyyx;
                bool4Swizzle<bool4, 4, 2, 1, 1, 1> zyyy;
                bool4Swizzle<bool4, 4, 2, 1, 2, 1> zyyz;
                bool4Swizzle<bool4, 4, 2, 1, 3, 1> zyyw;
                bool4Swizzle<bool4, 4, 2, 1, 0, 2> zyzx;
                bool4Swizzle<bool4, 4, 2, 1, 1, 2> zyzy;
                bool4Swizzle<bool4, 4, 2, 1, 2, 2> zyzz;
                bool4Swizzle<bool4, 4, 2, 1, 3, 2> zyzw;
                bool4Swizzle<bool4, 4, 2, 1, 0, 3> zywx;
                bool4Swizzle<bool4, 4, 2, 1, 1, 3> zywy;
                bool4Swizzle<bool4, 4, 2, 1, 2, 3> zywz;
                bool4Swizzle<bool4, 4, 2, 1, 3, 3> zyww;
                bool4Swizzle<bool4, 4, 2, 2, 0, 0> zzxx;
                bool4Swizzle<bool4, 4, 2, 2, 1, 0> zzxy;
                bool4Swizzle<bool4, 4, 2, 2, 2, 0> zzxz;
                bool4Swizzle<bool4, 4, 2, 2, 3, 0> zzxw;
                bool4Swizzle<bool4, 4, 2, 2, 0, 1> zzyx;
                bool4Swizzle<bool4, 4, 2, 2, 1, 1> zzyy;
                bool4Swizzle<bool4, 4, 2, 2, 2, 1> zzyz;
                bool4Swizzle<bool4, 4, 2, 2, 3, 1> zzyw;
                bool4Swizzle<bool4, 4, 2, 2, 0, 2> zzzx;
                bool4Swizzle<bool4, 4, 2, 2, 1, 2> zzzy;
                bool4Swizzle<bool4, 4, 2, 2, 2, 2> zzzz;
                bool4Swizzle<bool4, 4, 2, 2, 3, 2> zzzw;
                bool4Swizzle<bool4, 4, 2, 2, 0, 3> zzwx;
                bool4Swizzle<bool4, 4, 2, 2, 1, 3> zzwy;
                bool4Swizzle<bool4, 4, 2, 2, 2, 3> zzwz;
                bool4Swizzle<bool4, 4, 2, 2, 3, 3> zzww;
                bool4Swizzle<bool4, 4, 2, 3, 0, 0> zwxx;
                bool4Swizzle<bool4, 4, 2, 3, 1, 0> zwxy;
                bool4Swizzle<bool4, 4, 2, 3, 2, 0> zwxz;
                bool4Swizzle<bool4, 4, 2, 3, 3, 0> zwxw;
                bool4Swizzle<bool4, 4, 2, 3, 0, 1> zwyx;
                bool4Swizzle<bool4, 4, 2, 3, 1, 1> zwyy;
                bool4Swizzle<bool4, 4, 2, 3, 2, 1> zwyz;
                bool4Swizzle<bool4, 4, 2, 3, 3, 1> zwyw;
                bool4Swizzle<bool4, 4, 2, 3, 0, 2> zwzx;
                bool4Swizzle<bool4, 4, 2, 3, 1, 2> zwzy;
                bool4Swizzle<bool4, 4, 2, 3, 2, 2> zwzz;
                bool4Swizzle<bool4, 4, 2, 3, 3, 2> zwzw;
                bool4Swizzle<bool4, 4, 2, 3, 0, 3> zwwx;
                bool4Swizzle<bool4, 4, 2, 3, 1, 3> zwwy;
                bool4Swizzle<bool4, 4, 2, 3, 2, 3> zwwz;
                bool4Swizzle<bool4, 4, 2, 3, 3, 3> zwww;
                bool4Swizzle<bool4, 4, 3, 0, 0, 0> wxxx;
                bool4Swizzle<bool4, 4, 3, 0, 1, 0> wxxy;
                bool4Swizzle<bool4, 4, 3, 0, 2, 0> wxxz;
                bool4Swizzle<bool4, 4, 3, 0, 3, 0> wxxw;
                bool4Swizzle<bool4, 4, 3, 0, 0, 1> wxyx;
                bool4Swizzle<bool4, 4, 3, 0, 1, 1> wxyy;
                bool4Swizzle<bool4, 4, 3, 0, 2, 1> wxyz;
                bool4Swizzle<bool4, 4, 3, 0, 3, 1> wxyw;
                bool4Swizzle<bool4, 4, 3, 0, 0, 2> wxzx;
                bool4Swizzle<bool4, 4, 3, 0, 1, 2> wxzy;
                bool4Swizzle<bool4, 4, 3, 0, 2, 2> wxzz;
                bool4Swizzle<bool4, 4, 3, 0, 3, 2> wxzw;
                bool4Swizzle<bool4, 4, 3, 0, 0, 3> wxwx;
                bool4Swizzle<bool4, 4, 3, 0, 1, 3> wxwy;
                bool4Swizzle<bool4, 4, 3, 0, 2, 3> wxwz;
                bool4Swizzle<bool4, 4, 3, 0, 3, 3> wxww;
                bool4Swizzle<bool4, 4, 3, 1, 0, 0> wyxx;
                bool4Swizzle<bool4, 4, 3, 1, 1, 0> wyxy;
                bool4Swizzle<bool4, 4, 3, 1, 2, 0> wyxz;
                bool4Swizzle<bool4, 4, 3, 1, 3, 0> wyxw;
                bool4Swizzle<bool4, 4, 3, 1, 0, 1> wyyx;
                bool4Swizzle<bool4, 4, 3, 1, 1, 1> wyyy;
                bool4Swizzle<bool4, 4, 3, 1, 2, 1> wyyz;
                bool4Swizzle<bool4, 4, 3, 1, 3, 1> wyyw;
                bool4Swizzle<bool4, 4, 3, 1, 0, 2> wyzx;
                bool4Swizzle<bool4, 4, 3, 1, 1, 2> wyzy;
                bool4Swizzle<bool4, 4, 3, 1, 2, 2> wyzz;
                bool4Swizzle<bool4, 4, 3, 1, 3, 2> wyzw;
                bool4Swizzle<bool4, 4, 3, 1, 0, 3> wywx;
                bool4Swizzle<bool4, 4, 3, 1, 1, 3> wywy;
                bool4Swizzle<bool4, 4, 3, 1, 2, 3> wywz;
                bool4Swizzle<bool4, 4, 3, 1, 3, 3> wyww;
                bool4Swizzle<bool4, 4, 3, 2, 0, 0> wzxx;
                bool4Swizzle<bool4, 4, 3, 2, 1, 0> wzxy;
                bool4Swizzle<bool4, 4, 3, 2, 2, 0> wzxz;
                bool4Swizzle<bool4, 4, 3, 2, 3, 0> wzxw;
                bool4Swizzle<bool4, 4, 3, 2, 0, 1> wzyx;
                bool4Swizzle<bool4, 4, 3, 2, 1, 1> wzyy;
                bool4Swizzle<bool4, 4, 3, 2, 2, 1> wzyz;
                bool4Swizzle<bool4, 4, 3, 2, 3, 1> wzyw;
                bool4Swizzle<bool4, 4, 3, 2, 0, 2> wzzx;
                bool4Swizzle<bool4, 4, 3, 2, 1, 2> wzzy;
                bool4Swizzle<bool4, 4, 3, 2, 2, 2> wzzz;
                bool4Swizzle<bool4, 4, 3, 2, 3, 2> wzzw;
                bool4Swizzle<bool4, 4, 3, 2, 0, 3> wzwx;
                bool4Swizzle<bool4, 4, 3, 2, 1, 3> wzwy;
                bool4Swizzle<bool4, 4, 3, 2, 2, 3> wzwz;
                bool4Swizzle<bool4, 4, 3, 2, 3, 3> wzww;
                bool4Swizzle<bool4, 4, 3, 3, 0, 0> wwxx;
                bool4Swizzle<bool4, 4, 3, 3, 1, 0> wwxy;
                bool4Swizzle<bool4, 4, 3, 3, 2, 0> wwxz;
                bool4Swizzle<bool4, 4, 3, 3, 3, 0> wwxw;
                bool4Swizzle<bool4, 4, 3, 3, 0, 1> wwyx;
                bool4Swizzle<bool4, 4, 3, 3, 1, 1> wwyy;
                bool4Swizzle<bool4, 4, 3, 3, 2, 1> wwyz;
                bool4Swizzle<bool4, 4, 3, 3, 3, 1> wwyw;
                bool4Swizzle<bool4, 4, 3, 3, 0, 2> wwzx;
                bool4Swizzle<bool4, 4, 3, 3, 1, 2> wwzy;
                bool4Swizzle<bool4, 4, 3, 3, 2, 2> wwzz;
                bool4Swizzle<bool4, 4, 3, 3, 3, 2> wwzw;
                bool4Swizzle<bool4, 4, 3, 3, 0, 3> wwwx;
                bool4Swizzle<bool4, 4, 3, 3, 1, 3> wwwy;
                bool4Swizzle<bool4, 4, 3, 3, 2, 3> wwwz;
                bool4Swizzle<bool4, 4, 3, 3, 3, 3> wwww;
            };

            bool4(bool x, bool y, bool z, bool w);

            bool4(bool x, bool y, const bool2 & zw);

            bool4(const bool2 & xy, const bool2 & zw);

            bool4(const bool2 & xy, bool z, bool w);

            bool4(const bool3 & xyz, bool w);

            bool4(bool x, const bool3 & yzw);

            bool4(const bool4 & xyzw);

            bool4();

            bool4 & operator=(const bool4 & rhs) noexcept;
bool4 & operator=(bool rhs) noexcept;

            bool& operator[](int i) noexcept { return (&x)[i]; }
            const bool& operator[](int i) const noexcept { return (&x)[i]; }

            bool4 operator||(const bool4 & rhs) const noexcept;

            bool4 operator&&(const bool4 & rhs) const noexcept;

            bool4 operator==(const bool4 & rhs) const noexcept;

            bool4 operator!=(const bool4 & rhs) const noexcept;

            static bool4 Random() noexcept;

            bool Any() const noexcept;

            bool All() const noexcept;

            bool4 operator!() const noexcept;

            bool4 & operator|=(const bool4 & rhs) noexcept;

            bool4 & operator&=(const bool4 & rhs) noexcept;

            static const bool4 Zero;
            static const bool4 UnitX;
            static const bool4 UnitY;
            static const bool4 UnitZ;
            static const bool4 UnitW;
            static const bool4 One;
        };
    }
}

