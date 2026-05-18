#pragma once

#include "float2.h"
#include "bool3.h"
#include "int3.h"
#include "float2.h"
#include "float4.h"

namespace Egg {
    namespace Math {

        class float2;
        class float4;
        class bool2;
        class bool3;
        class bool4;
        class int2;
        class int3;
        class int4;

        class float3 {
        public:
            union {
                struct {
                    float x;
                    float y;
                    float z;
                };

                float2Swizzle<float2, int2, bool2, 3, 0, 0> xx;
                float2Swizzle<float2, int2, bool2, 3, 0, 1> xy;
                float2Swizzle<float2, int2, bool2, 3, 0, 2> xz;
                float2Swizzle<float2, int2, bool2, 3, 1, 0> yx;
                float2Swizzle<float2, int2, bool2, 3, 1, 1> yy;
                float2Swizzle<float2, int2, bool2, 3, 1, 2> yz;
                float2Swizzle<float2, int2, bool2, 3, 2, 0> zx;
                float2Swizzle<float2, int2, bool2, 3, 2, 1> zy;
                float2Swizzle<float2, int2, bool2, 3, 2, 2> zz;

                float3Swizzle<float3, int3, bool3, 3, 0, 0, 0> xxx;
                float3Swizzle<float3, int3, bool3, 3, 0, 0, 1> xxy;
                float3Swizzle<float3, int3, bool3, 3, 0, 0, 2> xxz;
                float3Swizzle<float3, int3, bool3, 3, 0, 1, 0> xyx;
                float3Swizzle<float3, int3, bool3, 3, 0, 1, 1> xyy;
                float3Swizzle<float3, int3, bool3, 3, 0, 1, 2> xyz;
                float3Swizzle<float3, int3, bool3, 3, 0, 2, 0> xzx;
                float3Swizzle<float3, int3, bool3, 3, 0, 2, 1> xzy;
                float3Swizzle<float3, int3, bool3, 3, 0, 2, 2> xzz;
                float3Swizzle<float3, int3, bool3, 3, 1, 0, 0> yxx;
                float3Swizzle<float3, int3, bool3, 3, 1, 0, 1> yxy;
                float3Swizzle<float3, int3, bool3, 3, 1, 0, 2> yxz;
                float3Swizzle<float3, int3, bool3, 3, 1, 1, 0> yyx;
                float3Swizzle<float3, int3, bool3, 3, 1, 1, 1> yyy;
                float3Swizzle<float3, int3, bool3, 3, 1, 1, 2> yyz;
                float3Swizzle<float3, int3, bool3, 3, 1, 2, 0> yzx;
                float3Swizzle<float3, int3, bool3, 3, 1, 2, 1> yzy;
                float3Swizzle<float3, int3, bool3, 3, 1, 2, 2> yzz;
                float3Swizzle<float3, int3, bool3, 3, 2, 0, 0> zxx;
                float3Swizzle<float3, int3, bool3, 3, 2, 0, 1> zxy;
                float3Swizzle<float3, int3, bool3, 3, 2, 0, 2> zxz;
                float3Swizzle<float3, int3, bool3, 3, 2, 1, 0> zyx;
                float3Swizzle<float3, int3, bool3, 3, 2, 1, 1> zyy;
                float3Swizzle<float3, int3, bool3, 3, 2, 1, 2> zyz;
                float3Swizzle<float3, int3, bool3, 3, 2, 2, 0> zzx;
                float3Swizzle<float3, int3, bool3, 3, 2, 2, 1> zzy;
                float3Swizzle<float3, int3, bool3, 3, 2, 2, 2> zzz;

                float4Swizzle<float4, int4, bool4, 3, 0, 0, 0, 0> xxxx;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 1, 0> xxxy;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 2, 0> xxxz;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 0, 1> xxyx;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 1, 1> xxyy;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 2, 1> xxyz;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 0, 2> xxzx;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 1, 2> xxzy;
                float4Swizzle<float4, int4, bool4, 3, 0, 0, 2, 2> xxzz;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 0, 0> xyxx;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 1, 0> xyxy;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 2, 0> xyxz;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 0, 1> xyyx;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 1, 1> xyyy;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 2, 1> xyyz;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 0, 2> xyzx;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 1, 2> xyzy;
                float4Swizzle<float4, int4, bool4, 3, 0, 1, 2, 2> xyzz;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 0, 0> xzxx;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 1, 0> xzxy;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 2, 0> xzxz;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 0, 1> xzyx;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 1, 1> xzyy;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 2, 1> xzyz;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 0, 2> xzzx;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 1, 2> xzzy;
                float4Swizzle<float4, int4, bool4, 3, 0, 2, 2, 2> xzzz;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 0, 0> yxxx;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 1, 0> yxxy;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 2, 0> yxxz;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 0, 1> yxyx;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 1, 1> yxyy;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 2, 1> yxyz;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 0, 2> yxzx;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 1, 2> yxzy;
                float4Swizzle<float4, int4, bool4, 3, 1, 0, 2, 2> yxzz;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 0, 0> yyxx;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 1, 0> yyxy;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 2, 0> yyxz;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 0, 1> yyyx;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 1, 1> yyyy;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 2, 1> yyyz;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 0, 2> yyzx;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 1, 2> yyzy;
                float4Swizzle<float4, int4, bool4, 3, 1, 1, 2, 2> yyzz;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 0, 0> yzxx;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 1, 0> yzxy;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 2, 0> yzxz;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 0, 1> yzyx;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 1, 1> yzyy;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 2, 1> yzyz;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 0, 2> yzzx;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 1, 2> yzzy;
                float4Swizzle<float4, int4, bool4, 3, 1, 2, 2, 2> yzzz;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 0, 0> zxxx;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 1, 0> zxxy;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 2, 0> zxxz;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 0, 1> zxyx;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 1, 1> zxyy;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 2, 1> zxyz;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 0, 2> zxzx;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 1, 2> zxzy;
                float4Swizzle<float4, int4, bool4, 3, 2, 0, 2, 2> zxzz;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 0, 0> zyxx;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 1, 0> zyxy;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 2, 0> zyxz;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 0, 1> zyyx;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 1, 1> zyyy;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 2, 1> zyyz;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 0, 2> zyzx;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 1, 2> zyzy;
                float4Swizzle<float4, int4, bool4, 3, 2, 1, 2, 2> zyzz;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 0, 0> zzxx;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 1, 0> zzxy;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 2, 0> zzxz;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 0, 1> zzyx;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 1, 1> zzyy;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 2, 1> zzyz;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 0, 2> zzzx;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 1, 2> zzzy;
                float4Swizzle<float4, int4, bool4, 3, 2, 2, 2, 2> zzzz;
            };

            float3(float x, float y, float z);

            float3(float x, const float2 & yz);

            float3(const float2 & xy, float z);

            float3(const float3 & xyz);

            float3();

            float3 & operator=(const float3 & rhs) noexcept;
float3 & operator=(float rhs) noexcept;

            float& operator[](int i) noexcept { return (&x)[i]; }
            const float& operator[](int i) const noexcept { return (&x)[i]; }

            float3 & operator+=(const float3 & rhs) noexcept;
float3 & operator+=(float rhs) noexcept;

            float3 & operator-=(const float3 & rhs) noexcept;
float3 & operator-=(float rhs) noexcept;

            float3 & operator/=(const float3 & rhs) noexcept;
float3 & operator/=(float rhs) noexcept;

            float3 & operator*=(const float3 & rhs) noexcept;
float3 & operator*=(float rhs) noexcept;

            float3 operator*(const float3 & rhs) const noexcept;

            float3 operator/(const float3 & rhs) const noexcept;

            float3 operator+(const float3 & rhs) const noexcept;

            float3 operator-(const float3 & rhs) const noexcept;

            float3 Abs() const noexcept;

            float3 Acos() const noexcept;

            float3 Asin() const noexcept;

            float3 Atan() const noexcept;

            float3 Cos() const noexcept;

            float3 Sin() const noexcept;

            float3 Cosh() const noexcept;

            float3 Sinh() const noexcept;

            float3 Tan() const noexcept;

            float3 Exp() const noexcept;

            float3 Log() const noexcept;

            float3 Log10() const noexcept;

            float3 Fmod(const float3 & rhs) const noexcept;

            float3 Atan2(const float3 & rhs) const noexcept;

            float3 Pow(const float3 & rhs) const noexcept;

            float3 Sqrt() const noexcept;

            float3 Clamp(const float3 & low, const float3 & high) const noexcept;

            float Dot(const float3 & rhs) const noexcept;

            float3 Sign() const noexcept;

            int3 Round() const noexcept;

            float3 Saturate() const noexcept;

            float LengthSquared() const noexcept;

            float Length() const noexcept;

            float3 Normalize() const noexcept;

            bool3 IsNan() const noexcept;

            bool3 IsFinite() const noexcept;

            bool3 IsInfinite() const noexcept;

            float3 operator-() const noexcept;

            float3 operator%(const float3 & rhs) const noexcept;

            float3 & operator%=(const float3 & rhs) noexcept;

            int3 Ceil() const noexcept;

            int3 Floor() const noexcept;

            float3 Exp2() const noexcept;

            int3 Trunc() const noexcept;

            float Distance(const float3 & rhs) const noexcept;

            bool3 operator<(const float3 & rhs) const noexcept;

            bool3 operator>(const float3 & rhs) const noexcept;

            bool3 operator!=(const float3 & rhs) const noexcept;

            bool3 operator==(const float3 & rhs) const noexcept;

            bool3 operator>=(const float3 & rhs) const noexcept;

            bool3 operator<=(const float3 & rhs) const noexcept;

            static float3 Random(float lower = 0.0f, float upper = 1.0f) noexcept;

            float3 operator+(float v) const noexcept;

            float3 operator-(float v) const noexcept;

            float3 operator*(float v) const noexcept;

            float3 operator/(float v) const noexcept;

            float3 operator%(float v) const noexcept;

            float3 Cross(const float3 & rhs) const noexcept;

            static const float3 UnitX;
            static const float3 UnitY;
            static const float3 UnitZ;
            static const float3 Zero;
            static const float3 One;
            static const float3 Black;
            static const float3 Navy;
            static const float3 Blue;
            static const float3 DarkGreen;
            static const float3 Teal;
            static const float3 Azure;
            static const float3 Green;
            static const float3 Cyan;
            static const float3 Maroon;
            static const float3 Purple;
            static const float3 SlateBlue;
            static const float3 Olive;
            static const float3 Gray;
            static const float3 Cornflower;
            static const float3 Aquamarine;
            static const float3 Red;
            static const float3 DeepPink;
            static const float3 Magenta;
            static const float3 Orange;
            static const float3 Coral;
            static const float3 Mallow;
            static const float3 Yellow;
            static const float3 Gold;
            static const float3 White;
            static const float3 Silver;
        };
    }
}

