#pragma once

#include "uint2Swizzle.hpp"
#include "uint3Swizzle.hpp"
#include "uint4Swizzle.hpp"
#include "bool2.h"
#include "uint3.h"
#include "uint4.h"

namespace Egg {
    namespace Math {

        class uint3;
        class uint4;
        class bool2;
        class bool3;
        class bool4;

        class uint2 {
        public:
            union {
                struct {
                    unsigned int x;
                    unsigned int y;
                };

                uint2Swizzle<uint2, bool2, 2, 0, 0> xx;
                uint2Swizzle<uint2, bool2, 2, 0, 1> xy;
                uint2Swizzle<uint2, bool2, 2, 1, 0> yx;
                uint2Swizzle<uint2, bool2, 2, 1, 1> yy;

                uint3Swizzle<uint3, bool3, 2, 0, 0, 0> xxx;
                uint3Swizzle<uint3, bool3, 2, 0, 0, 1> xxy;
                uint3Swizzle<uint3, bool3, 2, 0, 1, 0> xyx;
                uint3Swizzle<uint3, bool3, 2, 0, 1, 1> xyy;
                uint3Swizzle<uint3, bool3, 2, 1, 0, 0> yxx;
                uint3Swizzle<uint3, bool3, 2, 1, 0, 1> yxy;
                uint3Swizzle<uint3, bool3, 2, 1, 1, 0> yyx;
                uint3Swizzle<uint3, bool3, 2, 1, 1, 1> yyy;

                uint4Swizzle<uint4, bool4, 2, 0, 0, 0, 0> xxxx;
                uint4Swizzle<uint4, bool4, 2, 0, 0, 1, 0> xxxy;
                uint4Swizzle<uint4, bool4, 2, 0, 0, 0, 1> xxyx;
                uint4Swizzle<uint4, bool4, 2, 0, 0, 1, 1> xxyy;
                uint4Swizzle<uint4, bool4, 2, 0, 1, 0, 0> xyxx;
                uint4Swizzle<uint4, bool4, 2, 0, 1, 1, 0> xyxy;
                uint4Swizzle<uint4, bool4, 2, 0, 1, 0, 1> xyyx;
                uint4Swizzle<uint4, bool4, 2, 0, 1, 1, 1> xyyy;
                uint4Swizzle<uint4, bool4, 2, 1, 0, 0, 0> yxxx;
                uint4Swizzle<uint4, bool4, 2, 1, 0, 1, 0> yxxy;
                uint4Swizzle<uint4, bool4, 2, 1, 0, 0, 1> yxyx;
                uint4Swizzle<uint4, bool4, 2, 1, 0, 1, 1> yxyy;
                uint4Swizzle<uint4, bool4, 2, 1, 1, 0, 0> yyxx;
                uint4Swizzle<uint4, bool4, 2, 1, 1, 1, 0> yyxy;
                uint4Swizzle<uint4, bool4, 2, 1, 1, 0, 1> yyyx;
                uint4Swizzle<uint4, bool4, 2, 1, 1, 1, 1> yyyy;
            };

            uint2(unsigned int x, unsigned int y);

            uint2(const uint2 & xy);

            uint2();

            uint2 & operator=(const uint2 & rhs) noexcept;
uint2 & operator=(unsigned int rhs) noexcept;

            uint2 & operator+=(const uint2 & rhs) noexcept;
uint2 & operator+=(unsigned int rhs) noexcept;

            uint2 & operator-=(const uint2 & rhs) noexcept;
uint2 & operator-=(unsigned int rhs) noexcept;

            uint2 & operator/=(const uint2 & rhs) noexcept;
uint2 & operator/=(unsigned int rhs) noexcept;

            uint2 & operator*=(const uint2 & rhs) noexcept;
uint2 & operator*=(unsigned int rhs) noexcept;

            uint2 & operator%=(const uint2 & rhs) noexcept;
uint2 & operator%=(unsigned int rhs) noexcept;

            uint2 & operator|=(const uint2 & rhs) noexcept;
uint2 & operator|=(unsigned int rhs) noexcept;

            uint2 & operator&=(const uint2 & rhs) noexcept;
uint2 & operator&=(unsigned int rhs) noexcept;

            uint2 & operator^=(const uint2 & rhs) noexcept;
uint2 & operator^=(unsigned int rhs) noexcept;

            uint2 & operator<<=(const uint2 & rhs) noexcept;
uint2 & operator<<=(unsigned int rhs) noexcept;

            uint2 & operator>>=(const uint2 & rhs) noexcept;
uint2 & operator>>=(unsigned int rhs) noexcept;

            uint2 operator*(const uint2 & rhs) const noexcept;

            uint2 operator/(const uint2 & rhs) const noexcept;

            uint2 operator+(const uint2 & rhs) const noexcept;

            uint2 operator-(const uint2 & rhs) const noexcept;

            uint2 operator%(const uint2 & rhs) const noexcept;

            uint2 operator|(const uint2 & rhs) const noexcept;

            uint2 operator&(const uint2 & rhs) const noexcept;

            uint2 operator^(const uint2 & rhs) const noexcept;

            uint2 operator<<(const uint2 & rhs) const noexcept;

            uint2 operator>>(const uint2 & rhs) const noexcept;

            uint2 operator||(const uint2 & rhs) const noexcept;

            uint2 operator&&(const uint2 & rhs) const noexcept;

            bool2 operator<(const uint2 & rhs) const noexcept;

            bool2 operator>(const uint2 & rhs) const noexcept;

            bool2 operator!=(const uint2 & rhs) const noexcept;

            bool2 operator==(const uint2 & rhs) const noexcept;

            bool2 operator>=(const uint2 & rhs) const noexcept;

            bool2 operator<=(const uint2 & rhs) const noexcept;

            uint2 operator~() const noexcept;

            uint2 operator!() const noexcept;

            uint2 operator++() noexcept;

            uint2 operator++(int) noexcept;

            uint2 operator--() noexcept;

            uint2 operator--(int) noexcept;

            static uint2 Random(unsigned int lower = 0, unsigned int upper = 6) noexcept;

            static const uint2 One;
            static const uint2 Zero;
            static const uint2 UnitX;
            static const uint2 UnitY;
        };
    }
}

