#pragma once

#include "float2Swizzle.hpp"
#include "float3Swizzle.hpp"
#include "float4Swizzle.hpp"
#include "bool1.h"
#include "int1.h"
#include "float2.h"
#include "float3.h"
#include "float4.h"

namespace Egg {
    namespace Math {

        class float1 {
        public:
            union {
                struct {
                    float x;
                };
            };

            float1(float x);

            float1();

            float1 & operator=(const float1 & rhs) noexcept;
float1 & operator=(float rhs) noexcept;

            float1 & operator+=(const float1 & rhs) noexcept;
float1 & operator+=(float rhs) noexcept;

            float1 & operator-=(const float1 & rhs) noexcept;
float1 & operator-=(float rhs) noexcept;

            float1 & operator/=(const float1 & rhs) noexcept;
float1 & operator/=(float rhs) noexcept;

            float1 & operator*=(const float1 & rhs) noexcept;
float1 & operator*=(float rhs) noexcept;

            float1 operator*(const float1 & rhs) const noexcept;

            float1 operator/(const float1 & rhs) const noexcept;

            float1 operator+(const float1 & rhs) const noexcept;

            float1 operator-(const float1 & rhs) const noexcept;

            float1 Abs() const noexcept;

            float1 Acos() const noexcept;

            float1 Asin() const noexcept;

            float1 Atan() const noexcept;

            float1 Cos() const noexcept;

            float1 Sin() const noexcept;

            float1 Cosh() const noexcept;

            float1 Sinh() const noexcept;

            float1 Tan() const noexcept;

            float1 Exp() const noexcept;

            float1 Log() const noexcept;

            float1 Log10() const noexcept;

            float1 Fmod(const float1 & rhs) const noexcept;

            float1 Atan2(const float1 & rhs) const noexcept;

            float1 Pow(const float1 & rhs) const noexcept;

            float1 Sqrt() const noexcept;

            float1 Clamp(const float1 & low, const float1 & high) const noexcept;

            float Dot(const float1 & rhs) const noexcept;

            float1 Sign() const noexcept;

            int1 Round() const noexcept;

            float1 Saturate() const noexcept;

            float LengthSquared() const noexcept;

            float Length() const noexcept;

            float1 Normalize() const noexcept;

            bool1 IsNan() const noexcept;

            bool1 IsFinite() const noexcept;

            bool1 IsInfinite() const noexcept;

            float1 operator-() const noexcept;

            float1 operator%(const float1 & rhs) const noexcept;

            float1 & operator%=(const float1 & rhs) noexcept;

            int1 Ceil() const noexcept;

            int1 Floor() const noexcept;

            float1 Exp2() const noexcept;

            int1 Trunc() const noexcept;

            float Distance(const float1 & rhs) const noexcept;

            bool1 operator<(const float1 & rhs) const noexcept;

            bool1 operator>(const float1 & rhs) const noexcept;

            bool1 operator!=(const float1 & rhs) const noexcept;

            bool1 operator==(const float1 & rhs) const noexcept;

            bool1 operator>=(const float1 & rhs) const noexcept;

            bool1 operator<=(const float1 & rhs) const noexcept;

            static float1 Random(float lower = 0.0f, float upper = 1.0f) noexcept;

        };
    }
}

