#pragma once

#include "float2Swizzle.hpp"
#include "float3Swizzle.hpp"
#include "float4Swizzle.hpp"
#include "bool2.h"
#include "int2.h"
#include "float3.h"
#include "float4.h"

namespace Egg {
    namespace Math {

        class float3;
        class float4;
        class bool2;
        class bool3;
        class bool4;
        class int2;
        class int3;
        class int4;

        class float2 {
        public:
            union {
                struct {
                    float x;
                    float y;
                };

                float2Swizzle<float2, int2, bool2, 2, 0, 0> xx;
                float2Swizzle<float2, int2, bool2, 2, 0, 1> xy;
                float2Swizzle<float2, int2, bool2, 2, 1, 0> yx;
                float2Swizzle<float2, int2, bool2, 2, 1, 1> yy;

                float3Swizzle<float3, int3, bool3, 2, 0, 0, 0> xxx;
                float3Swizzle<float3, int3, bool3, 2, 0, 0, 1> xxy;
                float3Swizzle<float3, int3, bool3, 2, 0, 1, 0> xyx;
                float3Swizzle<float3, int3, bool3, 2, 0, 1, 1> xyy;
                float3Swizzle<float3, int3, bool3, 2, 1, 0, 0> yxx;
                float3Swizzle<float3, int3, bool3, 2, 1, 0, 1> yxy;
                float3Swizzle<float3, int3, bool3, 2, 1, 1, 0> yyx;
                float3Swizzle<float3, int3, bool3, 2, 1, 1, 1> yyy;

                float4Swizzle<float4, int4, bool4, 2, 0, 0, 0, 0> xxxx;
                float4Swizzle<float4, int4, bool4, 2, 0, 0, 1, 0> xxxy;
                float4Swizzle<float4, int4, bool4, 2, 0, 0, 0, 1> xxyx;
                float4Swizzle<float4, int4, bool4, 2, 0, 0, 1, 1> xxyy;
                float4Swizzle<float4, int4, bool4, 2, 0, 1, 0, 0> xyxx;
                float4Swizzle<float4, int4, bool4, 2, 0, 1, 1, 0> xyxy;
                float4Swizzle<float4, int4, bool4, 2, 0, 1, 0, 1> xyyx;
                float4Swizzle<float4, int4, bool4, 2, 0, 1, 1, 1> xyyy;
                float4Swizzle<float4, int4, bool4, 2, 1, 0, 0, 0> yxxx;
                float4Swizzle<float4, int4, bool4, 2, 1, 0, 1, 0> yxxy;
                float4Swizzle<float4, int4, bool4, 2, 1, 0, 0, 1> yxyx;
                float4Swizzle<float4, int4, bool4, 2, 1, 0, 1, 1> yxyy;
                float4Swizzle<float4, int4, bool4, 2, 1, 1, 0, 0> yyxx;
                float4Swizzle<float4, int4, bool4, 2, 1, 1, 1, 0> yyxy;
                float4Swizzle<float4, int4, bool4, 2, 1, 1, 0, 1> yyyx;
                float4Swizzle<float4, int4, bool4, 2, 1, 1, 1, 1> yyyy;
            };

            float2(float x, float y);

            float2(const float2 & xy);

            float2();

            float2 & operator=(const float2 & rhs) noexcept;
float2 & operator=(float rhs) noexcept;

            float& operator[](int i) noexcept { return (&x)[i]; }
            const float& operator[](int i) const noexcept { return (&x)[i]; }

            float2 & operator+=(const float2 & rhs) noexcept;
float2 & operator+=(float rhs) noexcept;

            float2 & operator-=(const float2 & rhs) noexcept;
float2 & operator-=(float rhs) noexcept;

            float2 & operator/=(const float2 & rhs) noexcept;
float2 & operator/=(float rhs) noexcept;

            float2 & operator*=(const float2 & rhs) noexcept;
float2 & operator*=(float rhs) noexcept;

            float2 operator*(const float2 & rhs) const noexcept;

            float2 operator/(const float2 & rhs) const noexcept;

            float2 operator+(const float2 & rhs) const noexcept;

            float2 operator-(const float2 & rhs) const noexcept;

            float2 Abs() const noexcept;

            float2 Acos() const noexcept;

            float2 Asin() const noexcept;

            float2 Atan() const noexcept;

            float2 Cos() const noexcept;

            float2 Sin() const noexcept;

            float2 Cosh() const noexcept;

            float2 Sinh() const noexcept;

            float2 Tan() const noexcept;

            float2 Exp() const noexcept;

            float2 Log() const noexcept;

            float2 Log10() const noexcept;

            float2 Fmod(const float2 & rhs) const noexcept;

            float2 Atan2(const float2 & rhs) const noexcept;

            float2 Pow(const float2 & rhs) const noexcept;

            float2 Sqrt() const noexcept;

            float2 Clamp(const float2 & low, const float2 & high) const noexcept;

            float Dot(const float2 & rhs) const noexcept;

            float2 Sign() const noexcept;

            int2 Round() const noexcept;

            float2 Saturate() const noexcept;

            float LengthSquared() const noexcept;

            float Length() const noexcept;

            float2 Normalize() const noexcept;

            bool2 IsNan() const noexcept;

            bool2 IsFinite() const noexcept;

            bool2 IsInfinite() const noexcept;

            float2 operator-() const noexcept;

            float2 operator%(const float2 & rhs) const noexcept;

            float2 & operator%=(const float2 & rhs) noexcept;

            int2 Ceil() const noexcept;

            int2 Floor() const noexcept;

            float2 Exp2() const noexcept;

            int2 Trunc() const noexcept;

            float Distance(const float2 & rhs) const noexcept;

            bool2 operator<(const float2 & rhs) const noexcept;

            bool2 operator>(const float2 & rhs) const noexcept;

            bool2 operator!=(const float2 & rhs) const noexcept;

            bool2 operator==(const float2 & rhs) const noexcept;

            bool2 operator>=(const float2 & rhs) const noexcept;

            bool2 operator<=(const float2 & rhs) const noexcept;

            static float2 Random(float lower = 0.0f, float upper = 1.0f) noexcept;

            float2 operator+(float v) const noexcept;

            float2 operator-(float v) const noexcept;

            float2 operator*(float v) const noexcept;

            float2 operator/(float v) const noexcept;

            float2 operator%(float v) const noexcept;

            float Arg() const noexcept;

            float2 Polar() const noexcept;

            float2 ComplexMul(const float2 & rhs) const noexcept;

            float2 Cartesian() const noexcept;

            static const float2 One;
            static const float2 Zero;
            static const float2 UnitX;
            static const float2 UnitY;
        };
    }
}

