#pragma once

#include "bool3.h"
#include "uint2.h"
#include "uint4.h"

namespace Egg {
    namespace Math {

        class uint2;
        class uint4;
        class bool2;
        class bool3;
        class bool4;

        class uint3 {
        public:
            union {
                struct {
                    unsigned int x;
                    unsigned int y;
                    unsigned int z;
                };

                uint2Swizzle<uint2, bool2, 3, 0, 0> xx;
                uint2Swizzle<uint2, bool2, 3, 0, 1> xy;
                uint2Swizzle<uint2, bool2, 3, 0, 2> xz;
                uint2Swizzle<uint2, bool2, 3, 1, 0> yx;
                uint2Swizzle<uint2, bool2, 3, 1, 1> yy;
                uint2Swizzle<uint2, bool2, 3, 1, 2> yz;
                uint2Swizzle<uint2, bool2, 3, 2, 0> zx;
                uint2Swizzle<uint2, bool2, 3, 2, 1> zy;
                uint2Swizzle<uint2, bool2, 3, 2, 2> zz;

                uint3Swizzle<uint3, bool3, 3, 0, 0, 0> xxx;
                uint3Swizzle<uint3, bool3, 3, 0, 0, 1> xxy;
                uint3Swizzle<uint3, bool3, 3, 0, 0, 2> xxz;
                uint3Swizzle<uint3, bool3, 3, 0, 1, 0> xyx;
                uint3Swizzle<uint3, bool3, 3, 0, 1, 1> xyy;
                uint3Swizzle<uint3, bool3, 3, 0, 1, 2> xyz;
                uint3Swizzle<uint3, bool3, 3, 0, 2, 0> xzx;
                uint3Swizzle<uint3, bool3, 3, 0, 2, 1> xzy;
                uint3Swizzle<uint3, bool3, 3, 0, 2, 2> xzz;
                uint3Swizzle<uint3, bool3, 3, 1, 0, 0> yxx;
                uint3Swizzle<uint3, bool3, 3, 1, 0, 1> yxy;
                uint3Swizzle<uint3, bool3, 3, 1, 0, 2> yxz;
                uint3Swizzle<uint3, bool3, 3, 1, 1, 0> yyx;
                uint3Swizzle<uint3, bool3, 3, 1, 1, 1> yyy;
                uint3Swizzle<uint3, bool3, 3, 1, 1, 2> yyz;
                uint3Swizzle<uint3, bool3, 3, 1, 2, 0> yzx;
                uint3Swizzle<uint3, bool3, 3, 1, 2, 1> yzy;
                uint3Swizzle<uint3, bool3, 3, 1, 2, 2> yzz;
                uint3Swizzle<uint3, bool3, 3, 2, 0, 0> zxx;
                uint3Swizzle<uint3, bool3, 3, 2, 0, 1> zxy;
                uint3Swizzle<uint3, bool3, 3, 2, 0, 2> zxz;
                uint3Swizzle<uint3, bool3, 3, 2, 1, 0> zyx;
                uint3Swizzle<uint3, bool3, 3, 2, 1, 1> zyy;
                uint3Swizzle<uint3, bool3, 3, 2, 1, 2> zyz;
                uint3Swizzle<uint3, bool3, 3, 2, 2, 0> zzx;
                uint3Swizzle<uint3, bool3, 3, 2, 2, 1> zzy;
                uint3Swizzle<uint3, bool3, 3, 2, 2, 2> zzz;

                uint4Swizzle<uint4, bool4, 3, 0, 0, 0, 0> xxxx;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 1, 0> xxxy;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 2, 0> xxxz;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 0, 1> xxyx;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 1, 1> xxyy;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 2, 1> xxyz;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 0, 2> xxzx;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 1, 2> xxzy;
                uint4Swizzle<uint4, bool4, 3, 0, 0, 2, 2> xxzz;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 0, 0> xyxx;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 1, 0> xyxy;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 2, 0> xyxz;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 0, 1> xyyx;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 1, 1> xyyy;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 2, 1> xyyz;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 0, 2> xyzx;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 1, 2> xyzy;
                uint4Swizzle<uint4, bool4, 3, 0, 1, 2, 2> xyzz;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 0, 0> xzxx;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 1, 0> xzxy;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 2, 0> xzxz;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 0, 1> xzyx;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 1, 1> xzyy;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 2, 1> xzyz;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 0, 2> xzzx;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 1, 2> xzzy;
                uint4Swizzle<uint4, bool4, 3, 0, 2, 2, 2> xzzz;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 0, 0> yxxx;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 1, 0> yxxy;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 2, 0> yxxz;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 0, 1> yxyx;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 1, 1> yxyy;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 2, 1> yxyz;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 0, 2> yxzx;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 1, 2> yxzy;
                uint4Swizzle<uint4, bool4, 3, 1, 0, 2, 2> yxzz;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 0, 0> yyxx;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 1, 0> yyxy;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 2, 0> yyxz;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 0, 1> yyyx;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 1, 1> yyyy;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 2, 1> yyyz;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 0, 2> yyzx;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 1, 2> yyzy;
                uint4Swizzle<uint4, bool4, 3, 1, 1, 2, 2> yyzz;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 0, 0> yzxx;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 1, 0> yzxy;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 2, 0> yzxz;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 0, 1> yzyx;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 1, 1> yzyy;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 2, 1> yzyz;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 0, 2> yzzx;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 1, 2> yzzy;
                uint4Swizzle<uint4, bool4, 3, 1, 2, 2, 2> yzzz;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 0, 0> zxxx;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 1, 0> zxxy;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 2, 0> zxxz;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 0, 1> zxyx;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 1, 1> zxyy;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 2, 1> zxyz;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 0, 2> zxzx;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 1, 2> zxzy;
                uint4Swizzle<uint4, bool4, 3, 2, 0, 2, 2> zxzz;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 0, 0> zyxx;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 1, 0> zyxy;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 2, 0> zyxz;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 0, 1> zyyx;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 1, 1> zyyy;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 2, 1> zyyz;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 0, 2> zyzx;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 1, 2> zyzy;
                uint4Swizzle<uint4, bool4, 3, 2, 1, 2, 2> zyzz;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 0, 0> zzxx;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 1, 0> zzxy;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 2, 0> zzxz;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 0, 1> zzyx;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 1, 1> zzyy;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 2, 1> zzyz;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 0, 2> zzzx;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 1, 2> zzzy;
                uint4Swizzle<uint4, bool4, 3, 2, 2, 2, 2> zzzz;
            };

            uint3(unsigned int x, unsigned int y, unsigned int z);

            uint3(unsigned int x, const uint2 & yz);

            uint3(const uint2 & xy, unsigned int z);

            uint3(const uint3 & xyz);

            uint3();

            uint3 & operator=(const uint3 & rhs) noexcept;
uint3 & operator=(unsigned int rhs) noexcept;

            uint3 & operator+=(const uint3 & rhs) noexcept;
uint3 & operator+=(unsigned int rhs) noexcept;

            uint3 & operator-=(const uint3 & rhs) noexcept;
uint3 & operator-=(unsigned int rhs) noexcept;

            uint3 & operator/=(const uint3 & rhs) noexcept;
uint3 & operator/=(unsigned int rhs) noexcept;

            uint3 & operator*=(const uint3 & rhs) noexcept;
uint3 & operator*=(unsigned int rhs) noexcept;

            uint3 & operator%=(const uint3 & rhs) noexcept;
uint3 & operator%=(unsigned int rhs) noexcept;

            uint3 & operator|=(const uint3 & rhs) noexcept;
uint3 & operator|=(unsigned int rhs) noexcept;

            uint3 & operator&=(const uint3 & rhs) noexcept;
uint3 & operator&=(unsigned int rhs) noexcept;

            uint3 & operator^=(const uint3 & rhs) noexcept;
uint3 & operator^=(unsigned int rhs) noexcept;

            uint3 & operator<<=(const uint3 & rhs) noexcept;
uint3 & operator<<=(unsigned int rhs) noexcept;

            uint3 & operator>>=(const uint3 & rhs) noexcept;
uint3 & operator>>=(unsigned int rhs) noexcept;

            uint3 operator*(const uint3 & rhs) const noexcept;

            uint3 operator/(const uint3 & rhs) const noexcept;

            uint3 operator+(const uint3 & rhs) const noexcept;

            uint3 operator-(const uint3 & rhs) const noexcept;

            uint3 operator%(const uint3 & rhs) const noexcept;

            uint3 operator|(const uint3 & rhs) const noexcept;

            uint3 operator&(const uint3 & rhs) const noexcept;

            uint3 operator^(const uint3 & rhs) const noexcept;

            uint3 operator<<(const uint3 & rhs) const noexcept;

            uint3 operator>>(const uint3 & rhs) const noexcept;

            uint3 operator||(const uint3 & rhs) const noexcept;

            uint3 operator&&(const uint3 & rhs) const noexcept;

            bool3 operator<(const uint3 & rhs) const noexcept;

            bool3 operator>(const uint3 & rhs) const noexcept;

            bool3 operator!=(const uint3 & rhs) const noexcept;

            bool3 operator==(const uint3 & rhs) const noexcept;

            bool3 operator>=(const uint3 & rhs) const noexcept;

            bool3 operator<=(const uint3 & rhs) const noexcept;

            uint3 operator~() const noexcept;

            uint3 operator!() const noexcept;

            uint3 operator++() noexcept;

            uint3 operator++(int) noexcept;

            uint3 operator--() noexcept;

            uint3 operator--(int) noexcept;

            static uint3 Random(unsigned int lower = 0, unsigned int upper = 6) noexcept;

            static const uint3 One;
            static const uint3 Zero;
            static const uint3 UnitX;
            static const uint3 UnitY;
            static const uint3 UnitZ;
        };
    }
}

