#pragma once

#include "bool2.h"
#include "bool4.h"

namespace Egg {
    namespace Math {

        class bool2;
        class bool4;

        class bool3 {
        public:
            union {
                struct {
                    bool x;
                    bool y;
                    bool z;
                };

                bool2Swizzle<bool2, 3, 0, 0> xx;
                bool2Swizzle<bool2, 3, 0, 1> xy;
                bool2Swizzle<bool2, 3, 0, 2> xz;
                bool2Swizzle<bool2, 3, 1, 0> yx;
                bool2Swizzle<bool2, 3, 1, 1> yy;
                bool2Swizzle<bool2, 3, 1, 2> yz;
                bool2Swizzle<bool2, 3, 2, 0> zx;
                bool2Swizzle<bool2, 3, 2, 1> zy;
                bool2Swizzle<bool2, 3, 2, 2> zz;

                bool3Swizzle<bool3, 3, 0, 0, 0> xxx;
                bool3Swizzle<bool3, 3, 0, 0, 1> xxy;
                bool3Swizzle<bool3, 3, 0, 0, 2> xxz;
                bool3Swizzle<bool3, 3, 0, 1, 0> xyx;
                bool3Swizzle<bool3, 3, 0, 1, 1> xyy;
                bool3Swizzle<bool3, 3, 0, 1, 2> xyz;
                bool3Swizzle<bool3, 3, 0, 2, 0> xzx;
                bool3Swizzle<bool3, 3, 0, 2, 1> xzy;
                bool3Swizzle<bool3, 3, 0, 2, 2> xzz;
                bool3Swizzle<bool3, 3, 1, 0, 0> yxx;
                bool3Swizzle<bool3, 3, 1, 0, 1> yxy;
                bool3Swizzle<bool3, 3, 1, 0, 2> yxz;
                bool3Swizzle<bool3, 3, 1, 1, 0> yyx;
                bool3Swizzle<bool3, 3, 1, 1, 1> yyy;
                bool3Swizzle<bool3, 3, 1, 1, 2> yyz;
                bool3Swizzle<bool3, 3, 1, 2, 0> yzx;
                bool3Swizzle<bool3, 3, 1, 2, 1> yzy;
                bool3Swizzle<bool3, 3, 1, 2, 2> yzz;
                bool3Swizzle<bool3, 3, 2, 0, 0> zxx;
                bool3Swizzle<bool3, 3, 2, 0, 1> zxy;
                bool3Swizzle<bool3, 3, 2, 0, 2> zxz;
                bool3Swizzle<bool3, 3, 2, 1, 0> zyx;
                bool3Swizzle<bool3, 3, 2, 1, 1> zyy;
                bool3Swizzle<bool3, 3, 2, 1, 2> zyz;
                bool3Swizzle<bool3, 3, 2, 2, 0> zzx;
                bool3Swizzle<bool3, 3, 2, 2, 1> zzy;
                bool3Swizzle<bool3, 3, 2, 2, 2> zzz;

                bool4Swizzle<bool4, 3, 0, 0, 0, 0> xxxx;
                bool4Swizzle<bool4, 3, 0, 0, 1, 0> xxxy;
                bool4Swizzle<bool4, 3, 0, 0, 2, 0> xxxz;
                bool4Swizzle<bool4, 3, 0, 0, 0, 1> xxyx;
                bool4Swizzle<bool4, 3, 0, 0, 1, 1> xxyy;
                bool4Swizzle<bool4, 3, 0, 0, 2, 1> xxyz;
                bool4Swizzle<bool4, 3, 0, 0, 0, 2> xxzx;
                bool4Swizzle<bool4, 3, 0, 0, 1, 2> xxzy;
                bool4Swizzle<bool4, 3, 0, 0, 2, 2> xxzz;
                bool4Swizzle<bool4, 3, 0, 1, 0, 0> xyxx;
                bool4Swizzle<bool4, 3, 0, 1, 1, 0> xyxy;
                bool4Swizzle<bool4, 3, 0, 1, 2, 0> xyxz;
                bool4Swizzle<bool4, 3, 0, 1, 0, 1> xyyx;
                bool4Swizzle<bool4, 3, 0, 1, 1, 1> xyyy;
                bool4Swizzle<bool4, 3, 0, 1, 2, 1> xyyz;
                bool4Swizzle<bool4, 3, 0, 1, 0, 2> xyzx;
                bool4Swizzle<bool4, 3, 0, 1, 1, 2> xyzy;
                bool4Swizzle<bool4, 3, 0, 1, 2, 2> xyzz;
                bool4Swizzle<bool4, 3, 0, 2, 0, 0> xzxx;
                bool4Swizzle<bool4, 3, 0, 2, 1, 0> xzxy;
                bool4Swizzle<bool4, 3, 0, 2, 2, 0> xzxz;
                bool4Swizzle<bool4, 3, 0, 2, 0, 1> xzyx;
                bool4Swizzle<bool4, 3, 0, 2, 1, 1> xzyy;
                bool4Swizzle<bool4, 3, 0, 2, 2, 1> xzyz;
                bool4Swizzle<bool4, 3, 0, 2, 0, 2> xzzx;
                bool4Swizzle<bool4, 3, 0, 2, 1, 2> xzzy;
                bool4Swizzle<bool4, 3, 0, 2, 2, 2> xzzz;
                bool4Swizzle<bool4, 3, 1, 0, 0, 0> yxxx;
                bool4Swizzle<bool4, 3, 1, 0, 1, 0> yxxy;
                bool4Swizzle<bool4, 3, 1, 0, 2, 0> yxxz;
                bool4Swizzle<bool4, 3, 1, 0, 0, 1> yxyx;
                bool4Swizzle<bool4, 3, 1, 0, 1, 1> yxyy;
                bool4Swizzle<bool4, 3, 1, 0, 2, 1> yxyz;
                bool4Swizzle<bool4, 3, 1, 0, 0, 2> yxzx;
                bool4Swizzle<bool4, 3, 1, 0, 1, 2> yxzy;
                bool4Swizzle<bool4, 3, 1, 0, 2, 2> yxzz;
                bool4Swizzle<bool4, 3, 1, 1, 0, 0> yyxx;
                bool4Swizzle<bool4, 3, 1, 1, 1, 0> yyxy;
                bool4Swizzle<bool4, 3, 1, 1, 2, 0> yyxz;
                bool4Swizzle<bool4, 3, 1, 1, 0, 1> yyyx;
                bool4Swizzle<bool4, 3, 1, 1, 1, 1> yyyy;
                bool4Swizzle<bool4, 3, 1, 1, 2, 1> yyyz;
                bool4Swizzle<bool4, 3, 1, 1, 0, 2> yyzx;
                bool4Swizzle<bool4, 3, 1, 1, 1, 2> yyzy;
                bool4Swizzle<bool4, 3, 1, 1, 2, 2> yyzz;
                bool4Swizzle<bool4, 3, 1, 2, 0, 0> yzxx;
                bool4Swizzle<bool4, 3, 1, 2, 1, 0> yzxy;
                bool4Swizzle<bool4, 3, 1, 2, 2, 0> yzxz;
                bool4Swizzle<bool4, 3, 1, 2, 0, 1> yzyx;
                bool4Swizzle<bool4, 3, 1, 2, 1, 1> yzyy;
                bool4Swizzle<bool4, 3, 1, 2, 2, 1> yzyz;
                bool4Swizzle<bool4, 3, 1, 2, 0, 2> yzzx;
                bool4Swizzle<bool4, 3, 1, 2, 1, 2> yzzy;
                bool4Swizzle<bool4, 3, 1, 2, 2, 2> yzzz;
                bool4Swizzle<bool4, 3, 2, 0, 0, 0> zxxx;
                bool4Swizzle<bool4, 3, 2, 0, 1, 0> zxxy;
                bool4Swizzle<bool4, 3, 2, 0, 2, 0> zxxz;
                bool4Swizzle<bool4, 3, 2, 0, 0, 1> zxyx;
                bool4Swizzle<bool4, 3, 2, 0, 1, 1> zxyy;
                bool4Swizzle<bool4, 3, 2, 0, 2, 1> zxyz;
                bool4Swizzle<bool4, 3, 2, 0, 0, 2> zxzx;
                bool4Swizzle<bool4, 3, 2, 0, 1, 2> zxzy;
                bool4Swizzle<bool4, 3, 2, 0, 2, 2> zxzz;
                bool4Swizzle<bool4, 3, 2, 1, 0, 0> zyxx;
                bool4Swizzle<bool4, 3, 2, 1, 1, 0> zyxy;
                bool4Swizzle<bool4, 3, 2, 1, 2, 0> zyxz;
                bool4Swizzle<bool4, 3, 2, 1, 0, 1> zyyx;
                bool4Swizzle<bool4, 3, 2, 1, 1, 1> zyyy;
                bool4Swizzle<bool4, 3, 2, 1, 2, 1> zyyz;
                bool4Swizzle<bool4, 3, 2, 1, 0, 2> zyzx;
                bool4Swizzle<bool4, 3, 2, 1, 1, 2> zyzy;
                bool4Swizzle<bool4, 3, 2, 1, 2, 2> zyzz;
                bool4Swizzle<bool4, 3, 2, 2, 0, 0> zzxx;
                bool4Swizzle<bool4, 3, 2, 2, 1, 0> zzxy;
                bool4Swizzle<bool4, 3, 2, 2, 2, 0> zzxz;
                bool4Swizzle<bool4, 3, 2, 2, 0, 1> zzyx;
                bool4Swizzle<bool4, 3, 2, 2, 1, 1> zzyy;
                bool4Swizzle<bool4, 3, 2, 2, 2, 1> zzyz;
                bool4Swizzle<bool4, 3, 2, 2, 0, 2> zzzx;
                bool4Swizzle<bool4, 3, 2, 2, 1, 2> zzzy;
                bool4Swizzle<bool4, 3, 2, 2, 2, 2> zzzz;
            };

            bool3(bool x, bool y, bool z);

            bool3(bool x, const bool2 & yz);

            bool3(const bool2 & xy, bool z);

            bool3(const bool3 & xyz);

            bool3();

            bool3 & operator=(const bool3 & rhs) noexcept;
bool3 & operator=(bool rhs) noexcept;

            bool& operator[](int i) noexcept { return (&x)[i]; }
            const bool& operator[](int i) const noexcept { return (&x)[i]; }

            bool3 operator||(const bool3 & rhs) const noexcept;

            bool3 operator&&(const bool3 & rhs) const noexcept;

            bool3 operator==(const bool3 & rhs) const noexcept;

            bool3 operator!=(const bool3 & rhs) const noexcept;

            static bool3 Random() noexcept;

            bool Any() const noexcept;

            bool All() const noexcept;

            bool3 operator!() const noexcept;

            bool3 & operator|=(const bool3 & rhs) noexcept;

            bool3 & operator&=(const bool3 & rhs) noexcept;

            static const bool3 One;
            static const bool3 Zero;
            static const bool3 UnitX;
            static const bool3 UnitY;
            static const bool3 UnitZ;
        };
    }
}

