#pragma once

#include "uint2Swizzle.hpp"
#include "uint3Swizzle.hpp"
#include "uint4Swizzle.hpp"
#include "bool1.h"

namespace Egg {
    namespace Math {

        class uint1 {
        public:
            union {
                struct {
                    unsigned int x;
                };
            };

            uint1(unsigned int x);

            uint1();

            uint1 & operator=(const uint1 & rhs) noexcept;
uint1 & operator=(unsigned int rhs) noexcept;

            uint1 & operator+=(const uint1 & rhs) noexcept;
uint1 & operator+=(unsigned int rhs) noexcept;

            uint1 & operator-=(const uint1 & rhs) noexcept;
uint1 & operator-=(unsigned int rhs) noexcept;

            uint1 & operator/=(const uint1 & rhs) noexcept;
uint1 & operator/=(unsigned int rhs) noexcept;

            uint1 & operator*=(const uint1 & rhs) noexcept;
uint1 & operator*=(unsigned int rhs) noexcept;

            uint1 & operator%=(const uint1 & rhs) noexcept;
uint1 & operator%=(unsigned int rhs) noexcept;

            uint1 & operator|=(const uint1 & rhs) noexcept;
uint1 & operator|=(unsigned int rhs) noexcept;

            uint1 & operator&=(const uint1 & rhs) noexcept;
uint1 & operator&=(unsigned int rhs) noexcept;

            uint1 & operator^=(const uint1 & rhs) noexcept;
uint1 & operator^=(unsigned int rhs) noexcept;

            uint1 & operator<<=(const uint1 & rhs) noexcept;
uint1 & operator<<=(unsigned int rhs) noexcept;

            uint1 & operator>>=(const uint1 & rhs) noexcept;
uint1 & operator>>=(unsigned int rhs) noexcept;

            uint1 operator*(const uint1 & rhs) const noexcept;

            uint1 operator/(const uint1 & rhs) const noexcept;

            uint1 operator+(const uint1 & rhs) const noexcept;

            uint1 operator-(const uint1 & rhs) const noexcept;

            uint1 operator%(const uint1 & rhs) const noexcept;

            uint1 operator|(const uint1 & rhs) const noexcept;

            uint1 operator&(const uint1 & rhs) const noexcept;

            uint1 operator^(const uint1 & rhs) const noexcept;

            uint1 operator<<(const uint1 & rhs) const noexcept;

            uint1 operator>>(const uint1 & rhs) const noexcept;

            uint1 operator||(const uint1 & rhs) const noexcept;

            uint1 operator&&(const uint1 & rhs) const noexcept;

            bool1 operator<(const uint1 & rhs) const noexcept;

            bool1 operator>(const uint1 & rhs) const noexcept;

            bool1 operator!=(const uint1 & rhs) const noexcept;

            bool1 operator==(const uint1 & rhs) const noexcept;

            bool1 operator>=(const uint1 & rhs) const noexcept;

            bool1 operator<=(const uint1 & rhs) const noexcept;

            uint1 operator~() const noexcept;

            uint1 operator!() const noexcept;

            uint1 operator++() noexcept;

            uint1 operator++(int) noexcept;

            uint1 operator--() noexcept;

            uint1 operator--(int) noexcept;

            static uint1 Random(unsigned int lower = 0, unsigned int upper = 6) noexcept;

        };
    }
}

