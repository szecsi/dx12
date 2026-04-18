#pragma once

#include "int2Swizzle.hpp"
#include "int3Swizzle.hpp"
#include "int4Swizzle.hpp"
#include "bool2.h"
#include "int3.h"
#include "int4.h"

namespace Egg {
    namespace Math {

        class int3;
        class int4;
        class bool2;
        class bool3;
        class bool4;

        class int2 {
        public:
            union {
                struct {
                    int x;
                    int y;
                };

                int2Swizzle<int2, bool2, 2, 0, 0> xx;
                int2Swizzle<int2, bool2, 2, 0, 1> xy;
                int2Swizzle<int2, bool2, 2, 1, 0> yx;
                int2Swizzle<int2, bool2, 2, 1, 1> yy;

                int3Swizzle<int3, bool3, 2, 0, 0, 0> xxx;
                int3Swizzle<int3, bool3, 2, 0, 0, 1> xxy;
                int3Swizzle<int3, bool3, 2, 0, 1, 0> xyx;
                int3Swizzle<int3, bool3, 2, 0, 1, 1> xyy;
                int3Swizzle<int3, bool3, 2, 1, 0, 0> yxx;
                int3Swizzle<int3, bool3, 2, 1, 0, 1> yxy;
                int3Swizzle<int3, bool3, 2, 1, 1, 0> yyx;
                int3Swizzle<int3, bool3, 2, 1, 1, 1> yyy;

                int4Swizzle<int4, bool4, 2, 0, 0, 0, 0> xxxx;
                int4Swizzle<int4, bool4, 2, 0, 0, 1, 0> xxxy;
                int4Swizzle<int4, bool4, 2, 0, 0, 0, 1> xxyx;
                int4Swizzle<int4, bool4, 2, 0, 0, 1, 1> xxyy;
                int4Swizzle<int4, bool4, 2, 0, 1, 0, 0> xyxx;
                int4Swizzle<int4, bool4, 2, 0, 1, 1, 0> xyxy;
                int4Swizzle<int4, bool4, 2, 0, 1, 0, 1> xyyx;
                int4Swizzle<int4, bool4, 2, 0, 1, 1, 1> xyyy;
                int4Swizzle<int4, bool4, 2, 1, 0, 0, 0> yxxx;
                int4Swizzle<int4, bool4, 2, 1, 0, 1, 0> yxxy;
                int4Swizzle<int4, bool4, 2, 1, 0, 0, 1> yxyx;
                int4Swizzle<int4, bool4, 2, 1, 0, 1, 1> yxyy;
                int4Swizzle<int4, bool4, 2, 1, 1, 0, 0> yyxx;
                int4Swizzle<int4, bool4, 2, 1, 1, 1, 0> yyxy;
                int4Swizzle<int4, bool4, 2, 1, 1, 0, 1> yyyx;
                int4Swizzle<int4, bool4, 2, 1, 1, 1, 1> yyyy;
            };

            int2(int x, int y);

            int2(const int2 & xy);

            int2();

            int2 & operator=(const int2 & rhs) noexcept;
int2 & operator=(int rhs) noexcept;

            int2 & operator+=(const int2 & rhs) noexcept;
int2 & operator+=(int rhs) noexcept;

            int2 & operator-=(const int2 & rhs) noexcept;
int2 & operator-=(int rhs) noexcept;

            int2 & operator/=(const int2 & rhs) noexcept;
int2 & operator/=(int rhs) noexcept;

            int2 & operator*=(const int2 & rhs) noexcept;
int2 & operator*=(int rhs) noexcept;

            int2 & operator%=(const int2 & rhs) noexcept;
int2 & operator%=(int rhs) noexcept;

            int2 & operator|=(const int2 & rhs) noexcept;
int2 & operator|=(int rhs) noexcept;

            int2 & operator&=(const int2 & rhs) noexcept;
int2 & operator&=(int rhs) noexcept;

            int2 & operator^=(const int2 & rhs) noexcept;
int2 & operator^=(int rhs) noexcept;

            int2 & operator<<=(const int2 & rhs) noexcept;
int2 & operator<<=(int rhs) noexcept;

            int2 & operator>>=(const int2 & rhs) noexcept;
int2 & operator>>=(int rhs) noexcept;

            int2 operator*(const int2 & rhs) const noexcept;

            int2 operator/(const int2 & rhs) const noexcept;

            int2 operator+(const int2 & rhs) const noexcept;

            int2 operator-(const int2 & rhs) const noexcept;

            int2 operator%(const int2 & rhs) const noexcept;

            int2 operator|(const int2 & rhs) const noexcept;

            int2 operator&(const int2 & rhs) const noexcept;

            int2 operator^(const int2 & rhs) const noexcept;

            int2 operator<<(const int2 & rhs) const noexcept;

            int2 operator>>(const int2 & rhs) const noexcept;

            int2 operator||(const int2 & rhs) const noexcept;

            int2 operator&&(const int2 & rhs) const noexcept;

            bool2 operator<(const int2 & rhs) const noexcept;

            bool2 operator>(const int2 & rhs) const noexcept;

            bool2 operator!=(const int2 & rhs) const noexcept;

            bool2 operator==(const int2 & rhs) const noexcept;

            bool2 operator>=(const int2 & rhs) const noexcept;

            bool2 operator<=(const int2 & rhs) const noexcept;

            int2 operator~() const noexcept;

            int2 operator!() const noexcept;

            int2 operator++() noexcept;

            int2 operator++(int) noexcept;

            int2 operator--() noexcept;

            int2 operator--(int) noexcept;

            static int2 Random(int lower = 0, int upper = 6) noexcept;

            int2 operator-() const noexcept;

            static const int2 One;
            static const int2 Zero;
            static const int2 UnitX;
            static const int2 UnitY;
        };
    }
}

