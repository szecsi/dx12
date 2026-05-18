#pragma once

#include "int2Swizzle.hpp"
#include "int3Swizzle.hpp"
#include "int4Swizzle.hpp"
#include "bool1.h"

namespace Egg {
    namespace Math {

        class int1 {
        public:
            union {
                struct {
                    int x;
                };
            };

            int1(int x);

            int1();

            int1 & operator=(const int1 & rhs) noexcept;
int1 & operator=(int rhs) noexcept;

            int& operator[](int i) noexcept { return (&x)[i]; }
            const int& operator[](int i) const noexcept { return (&x)[i]; }

            int1 & operator+=(const int1 & rhs) noexcept;
int1 & operator+=(int rhs) noexcept;

            int1 & operator-=(const int1 & rhs) noexcept;
int1 & operator-=(int rhs) noexcept;

            int1 & operator/=(const int1 & rhs) noexcept;
int1 & operator/=(int rhs) noexcept;

            int1 & operator*=(const int1 & rhs) noexcept;
int1 & operator*=(int rhs) noexcept;

            int1 & operator%=(const int1 & rhs) noexcept;
int1 & operator%=(int rhs) noexcept;

            int1 & operator|=(const int1 & rhs) noexcept;
int1 & operator|=(int rhs) noexcept;

            int1 & operator&=(const int1 & rhs) noexcept;
int1 & operator&=(int rhs) noexcept;

            int1 & operator^=(const int1 & rhs) noexcept;
int1 & operator^=(int rhs) noexcept;

            int1 & operator<<=(const int1 & rhs) noexcept;
int1 & operator<<=(int rhs) noexcept;

            int1 & operator>>=(const int1 & rhs) noexcept;
int1 & operator>>=(int rhs) noexcept;

            int1 operator*(const int1 & rhs) const noexcept;

            int1 operator/(const int1 & rhs) const noexcept;

            int1 operator+(const int1 & rhs) const noexcept;

            int1 operator-(const int1 & rhs) const noexcept;

            int1 operator%(const int1 & rhs) const noexcept;

            int1 operator|(const int1 & rhs) const noexcept;

            int1 operator&(const int1 & rhs) const noexcept;

            int1 operator^(const int1 & rhs) const noexcept;

            int1 operator<<(const int1 & rhs) const noexcept;

            int1 operator>>(const int1 & rhs) const noexcept;

            int1 operator||(const int1 & rhs) const noexcept;

            int1 operator&&(const int1 & rhs) const noexcept;

            bool1 operator<(const int1 & rhs) const noexcept;

            bool1 operator>(const int1 & rhs) const noexcept;

            bool1 operator!=(const int1 & rhs) const noexcept;

            bool1 operator==(const int1 & rhs) const noexcept;

            bool1 operator>=(const int1 & rhs) const noexcept;

            bool1 operator<=(const int1 & rhs) const noexcept;

            int1 operator~() const noexcept;

            int1 operator!() const noexcept;

            int1 operator++() noexcept;

            int1 operator++(int) noexcept;

            int1 operator--() noexcept;

            int1 operator--(int) noexcept;

            static int1 Random(int lower = 0, int upper = 6) noexcept;

            int1 operator-() const noexcept;

        };
    }
}

