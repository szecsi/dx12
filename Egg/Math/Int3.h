#pragma once

#include "int2.h"
#include "bool3.h"
#include "int4.h"

namespace Egg {
    namespace Math {

        class int2;
        class int4;
        class bool2;
        class bool3;
        class bool4;

        class int3 {
        public:
            union {
                struct {
                    int x;
                    int y;
                    int z;
                };

                int2Swizzle<int2, bool2, 3, 0, 0> xx;
                int2Swizzle<int2, bool2, 3, 0, 1> xy;
                int2Swizzle<int2, bool2, 3, 0, 2> xz;
                int2Swizzle<int2, bool2, 3, 1, 0> yx;
                int2Swizzle<int2, bool2, 3, 1, 1> yy;
                int2Swizzle<int2, bool2, 3, 1, 2> yz;
                int2Swizzle<int2, bool2, 3, 2, 0> zx;
                int2Swizzle<int2, bool2, 3, 2, 1> zy;
                int2Swizzle<int2, bool2, 3, 2, 2> zz;

                int3Swizzle<int3, bool3, 3, 0, 0, 0> xxx;
                int3Swizzle<int3, bool3, 3, 0, 0, 1> xxy;
                int3Swizzle<int3, bool3, 3, 0, 0, 2> xxz;
                int3Swizzle<int3, bool3, 3, 0, 1, 0> xyx;
                int3Swizzle<int3, bool3, 3, 0, 1, 1> xyy;
                int3Swizzle<int3, bool3, 3, 0, 1, 2> xyz;
                int3Swizzle<int3, bool3, 3, 0, 2, 0> xzx;
                int3Swizzle<int3, bool3, 3, 0, 2, 1> xzy;
                int3Swizzle<int3, bool3, 3, 0, 2, 2> xzz;
                int3Swizzle<int3, bool3, 3, 1, 0, 0> yxx;
                int3Swizzle<int3, bool3, 3, 1, 0, 1> yxy;
                int3Swizzle<int3, bool3, 3, 1, 0, 2> yxz;
                int3Swizzle<int3, bool3, 3, 1, 1, 0> yyx;
                int3Swizzle<int3, bool3, 3, 1, 1, 1> yyy;
                int3Swizzle<int3, bool3, 3, 1, 1, 2> yyz;
                int3Swizzle<int3, bool3, 3, 1, 2, 0> yzx;
                int3Swizzle<int3, bool3, 3, 1, 2, 1> yzy;
                int3Swizzle<int3, bool3, 3, 1, 2, 2> yzz;
                int3Swizzle<int3, bool3, 3, 2, 0, 0> zxx;
                int3Swizzle<int3, bool3, 3, 2, 0, 1> zxy;
                int3Swizzle<int3, bool3, 3, 2, 0, 2> zxz;
                int3Swizzle<int3, bool3, 3, 2, 1, 0> zyx;
                int3Swizzle<int3, bool3, 3, 2, 1, 1> zyy;
                int3Swizzle<int3, bool3, 3, 2, 1, 2> zyz;
                int3Swizzle<int3, bool3, 3, 2, 2, 0> zzx;
                int3Swizzle<int3, bool3, 3, 2, 2, 1> zzy;
                int3Swizzle<int3, bool3, 3, 2, 2, 2> zzz;

                int4Swizzle<int4, bool4, 3, 0, 0, 0, 0> xxxx;
                int4Swizzle<int4, bool4, 3, 0, 0, 1, 0> xxxy;
                int4Swizzle<int4, bool4, 3, 0, 0, 2, 0> xxxz;
                int4Swizzle<int4, bool4, 3, 0, 0, 0, 1> xxyx;
                int4Swizzle<int4, bool4, 3, 0, 0, 1, 1> xxyy;
                int4Swizzle<int4, bool4, 3, 0, 0, 2, 1> xxyz;
                int4Swizzle<int4, bool4, 3, 0, 0, 0, 2> xxzx;
                int4Swizzle<int4, bool4, 3, 0, 0, 1, 2> xxzy;
                int4Swizzle<int4, bool4, 3, 0, 0, 2, 2> xxzz;
                int4Swizzle<int4, bool4, 3, 0, 1, 0, 0> xyxx;
                int4Swizzle<int4, bool4, 3, 0, 1, 1, 0> xyxy;
                int4Swizzle<int4, bool4, 3, 0, 1, 2, 0> xyxz;
                int4Swizzle<int4, bool4, 3, 0, 1, 0, 1> xyyx;
                int4Swizzle<int4, bool4, 3, 0, 1, 1, 1> xyyy;
                int4Swizzle<int4, bool4, 3, 0, 1, 2, 1> xyyz;
                int4Swizzle<int4, bool4, 3, 0, 1, 0, 2> xyzx;
                int4Swizzle<int4, bool4, 3, 0, 1, 1, 2> xyzy;
                int4Swizzle<int4, bool4, 3, 0, 1, 2, 2> xyzz;
                int4Swizzle<int4, bool4, 3, 0, 2, 0, 0> xzxx;
                int4Swizzle<int4, bool4, 3, 0, 2, 1, 0> xzxy;
                int4Swizzle<int4, bool4, 3, 0, 2, 2, 0> xzxz;
                int4Swizzle<int4, bool4, 3, 0, 2, 0, 1> xzyx;
                int4Swizzle<int4, bool4, 3, 0, 2, 1, 1> xzyy;
                int4Swizzle<int4, bool4, 3, 0, 2, 2, 1> xzyz;
                int4Swizzle<int4, bool4, 3, 0, 2, 0, 2> xzzx;
                int4Swizzle<int4, bool4, 3, 0, 2, 1, 2> xzzy;
                int4Swizzle<int4, bool4, 3, 0, 2, 2, 2> xzzz;
                int4Swizzle<int4, bool4, 3, 1, 0, 0, 0> yxxx;
                int4Swizzle<int4, bool4, 3, 1, 0, 1, 0> yxxy;
                int4Swizzle<int4, bool4, 3, 1, 0, 2, 0> yxxz;
                int4Swizzle<int4, bool4, 3, 1, 0, 0, 1> yxyx;
                int4Swizzle<int4, bool4, 3, 1, 0, 1, 1> yxyy;
                int4Swizzle<int4, bool4, 3, 1, 0, 2, 1> yxyz;
                int4Swizzle<int4, bool4, 3, 1, 0, 0, 2> yxzx;
                int4Swizzle<int4, bool4, 3, 1, 0, 1, 2> yxzy;
                int4Swizzle<int4, bool4, 3, 1, 0, 2, 2> yxzz;
                int4Swizzle<int4, bool4, 3, 1, 1, 0, 0> yyxx;
                int4Swizzle<int4, bool4, 3, 1, 1, 1, 0> yyxy;
                int4Swizzle<int4, bool4, 3, 1, 1, 2, 0> yyxz;
                int4Swizzle<int4, bool4, 3, 1, 1, 0, 1> yyyx;
                int4Swizzle<int4, bool4, 3, 1, 1, 1, 1> yyyy;
                int4Swizzle<int4, bool4, 3, 1, 1, 2, 1> yyyz;
                int4Swizzle<int4, bool4, 3, 1, 1, 0, 2> yyzx;
                int4Swizzle<int4, bool4, 3, 1, 1, 1, 2> yyzy;
                int4Swizzle<int4, bool4, 3, 1, 1, 2, 2> yyzz;
                int4Swizzle<int4, bool4, 3, 1, 2, 0, 0> yzxx;
                int4Swizzle<int4, bool4, 3, 1, 2, 1, 0> yzxy;
                int4Swizzle<int4, bool4, 3, 1, 2, 2, 0> yzxz;
                int4Swizzle<int4, bool4, 3, 1, 2, 0, 1> yzyx;
                int4Swizzle<int4, bool4, 3, 1, 2, 1, 1> yzyy;
                int4Swizzle<int4, bool4, 3, 1, 2, 2, 1> yzyz;
                int4Swizzle<int4, bool4, 3, 1, 2, 0, 2> yzzx;
                int4Swizzle<int4, bool4, 3, 1, 2, 1, 2> yzzy;
                int4Swizzle<int4, bool4, 3, 1, 2, 2, 2> yzzz;
                int4Swizzle<int4, bool4, 3, 2, 0, 0, 0> zxxx;
                int4Swizzle<int4, bool4, 3, 2, 0, 1, 0> zxxy;
                int4Swizzle<int4, bool4, 3, 2, 0, 2, 0> zxxz;
                int4Swizzle<int4, bool4, 3, 2, 0, 0, 1> zxyx;
                int4Swizzle<int4, bool4, 3, 2, 0, 1, 1> zxyy;
                int4Swizzle<int4, bool4, 3, 2, 0, 2, 1> zxyz;
                int4Swizzle<int4, bool4, 3, 2, 0, 0, 2> zxzx;
                int4Swizzle<int4, bool4, 3, 2, 0, 1, 2> zxzy;
                int4Swizzle<int4, bool4, 3, 2, 0, 2, 2> zxzz;
                int4Swizzle<int4, bool4, 3, 2, 1, 0, 0> zyxx;
                int4Swizzle<int4, bool4, 3, 2, 1, 1, 0> zyxy;
                int4Swizzle<int4, bool4, 3, 2, 1, 2, 0> zyxz;
                int4Swizzle<int4, bool4, 3, 2, 1, 0, 1> zyyx;
                int4Swizzle<int4, bool4, 3, 2, 1, 1, 1> zyyy;
                int4Swizzle<int4, bool4, 3, 2, 1, 2, 1> zyyz;
                int4Swizzle<int4, bool4, 3, 2, 1, 0, 2> zyzx;
                int4Swizzle<int4, bool4, 3, 2, 1, 1, 2> zyzy;
                int4Swizzle<int4, bool4, 3, 2, 1, 2, 2> zyzz;
                int4Swizzle<int4, bool4, 3, 2, 2, 0, 0> zzxx;
                int4Swizzle<int4, bool4, 3, 2, 2, 1, 0> zzxy;
                int4Swizzle<int4, bool4, 3, 2, 2, 2, 0> zzxz;
                int4Swizzle<int4, bool4, 3, 2, 2, 0, 1> zzyx;
                int4Swizzle<int4, bool4, 3, 2, 2, 1, 1> zzyy;
                int4Swizzle<int4, bool4, 3, 2, 2, 2, 1> zzyz;
                int4Swizzle<int4, bool4, 3, 2, 2, 0, 2> zzzx;
                int4Swizzle<int4, bool4, 3, 2, 2, 1, 2> zzzy;
                int4Swizzle<int4, bool4, 3, 2, 2, 2, 2> zzzz;
            };

            int3(int x, int y, int z);

            int3(int x, const int2 & yz);

            int3(const int2 & xy, int z);

            int3(const int3 & xyz);

            int3();

            int3 & operator=(const int3 & rhs) noexcept;
int3 & operator=(int rhs) noexcept;

            int& operator[](int i) noexcept { return (&x)[i]; }
            const int& operator[](int i) const noexcept { return (&x)[i]; }

            int3 & operator+=(const int3 & rhs) noexcept;
int3 & operator+=(int rhs) noexcept;

            int3 & operator-=(const int3 & rhs) noexcept;
int3 & operator-=(int rhs) noexcept;

            int3 & operator/=(const int3 & rhs) noexcept;
int3 & operator/=(int rhs) noexcept;

            int3 & operator*=(const int3 & rhs) noexcept;
int3 & operator*=(int rhs) noexcept;

            int3 & operator%=(const int3 & rhs) noexcept;
int3 & operator%=(int rhs) noexcept;

            int3 & operator|=(const int3 & rhs) noexcept;
int3 & operator|=(int rhs) noexcept;

            int3 & operator&=(const int3 & rhs) noexcept;
int3 & operator&=(int rhs) noexcept;

            int3 & operator^=(const int3 & rhs) noexcept;
int3 & operator^=(int rhs) noexcept;

            int3 & operator<<=(const int3 & rhs) noexcept;
int3 & operator<<=(int rhs) noexcept;

            int3 & operator>>=(const int3 & rhs) noexcept;
int3 & operator>>=(int rhs) noexcept;

            int3 operator*(const int3 & rhs) const noexcept;

            int3 operator/(const int3 & rhs) const noexcept;

            int3 operator+(const int3 & rhs) const noexcept;

            int3 operator-(const int3 & rhs) const noexcept;

            int3 operator%(const int3 & rhs) const noexcept;

            int3 operator|(const int3 & rhs) const noexcept;

            int3 operator&(const int3 & rhs) const noexcept;

            int3 operator^(const int3 & rhs) const noexcept;

            int3 operator<<(const int3 & rhs) const noexcept;

            int3 operator>>(const int3 & rhs) const noexcept;

            int3 operator||(const int3 & rhs) const noexcept;

            int3 operator&&(const int3 & rhs) const noexcept;

            bool3 operator<(const int3 & rhs) const noexcept;

            bool3 operator>(const int3 & rhs) const noexcept;

            bool3 operator!=(const int3 & rhs) const noexcept;

            bool3 operator==(const int3 & rhs) const noexcept;

            bool3 operator>=(const int3 & rhs) const noexcept;

            bool3 operator<=(const int3 & rhs) const noexcept;

            int3 operator~() const noexcept;

            int3 operator!() const noexcept;

            int3 operator++() noexcept;

            int3 operator++(int) noexcept;

            int3 operator--() noexcept;

            int3 operator--(int) noexcept;

            static int3 Random(int lower = 0, int upper = 6) noexcept;

            int3 operator-() const noexcept;

            static const int3 One;
            static const int3 Zero;
            static const int3 UnitX;
            static const int3 UnitY;
            static const int3 UnitZ;
        };
    }
}

