#include "float4.h"
#include <cmath>
#include <cfloat>

namespace Egg {
    namespace Math {

        float4::float4(float x, float y, float z, float w) : x { x }, y { y }, z { z }, w { w }{ }

        float4::float4(float x, float y, const float2 & zw) : x { x }, y { y }, z { zw.x }, w { zw.y }{ }

        float4::float4(const float2 & xy, const float2 & zw) : x { xy.x }, y { xy.y }, z { zw.x }, w { zw.y }{ }

        float4::float4(const float2 & xy, float z, float w) : x { xy.x }, y { xy.y }, z { z }, w { w }{ }

        float4::float4(const float3 & xyz, float w) : x { xyz.x }, y { xyz.y }, z { xyz.z }, w { w }{ }

        float4::float4(float x, const float3 & yzw) : x { x }, y { yzw.x }, z { yzw.y }, w { yzw.z }{ }

        float4::float4(const float4 & xyzw) : x { xyzw.x }, y { xyzw.y }, z { xyzw.z }, w { xyzw.w }{ }

        float4::float4() : x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, w{ 0.0f }{ }

        float4 & float4::operator=(const float4 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            this->w = rhs.w;
            return *this;
        }

        float4 & float4::operator=(float rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            this->w = rhs;
            return *this;
        }

        float4 & float4::operator+=(const float4 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            this->z += rhs.z;
            this->w += rhs.w;
            return *this;
        }

        float4 & float4::operator+=(float rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            this->z += rhs;
            this->w += rhs;
            return *this;
        }

        float4 & float4::operator-=(const float4 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            this->z -= rhs.z;
            this->w -= rhs.w;
            return *this;
        }

        float4 & float4::operator-=(float rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            this->z -= rhs;
            this->w -= rhs;
            return *this;
        }

        float4 & float4::operator/=(const float4 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            this->z /= rhs.z;
            this->w /= rhs.w;
            return *this;
        }

        float4 & float4::operator/=(float rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            this->z /= rhs;
            this->w /= rhs;
            return *this;
        }

        float4 & float4::operator*=(const float4 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            this->z *= rhs.z;
            this->w *= rhs.w;
            return *this;
        }

        float4 & float4::operator*=(float rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            this->z *= rhs;
            this->w *= rhs;
            return *this;
        }

        float4 float4::operator*(const float4 & rhs) const noexcept {
            return float4 { this->x * rhs.x, this->y * rhs.y, this->z * rhs.z, this->w * rhs.w };
        }

        float4 float4::operator/(const float4 & rhs) const noexcept {
            return float4 { this->x / rhs.x, this->y / rhs.y, this->z / rhs.z, this->w / rhs.w };
        }

        float4 float4::operator+(const float4 & rhs) const noexcept {
            return float4 { this->x + rhs.x, this->y + rhs.y, this->z + rhs.z, this->w + rhs.w };
        }

        float4 float4::operator-(const float4 & rhs) const noexcept {
            return float4 { this->x - rhs.x, this->y - rhs.y, this->z - rhs.z, this->w - rhs.w };
        }

        float4 float4::Abs() const noexcept {
            return float4 { ::abs(this->x), ::abs(this->y), ::abs(this->z), ::abs(this->w) };
        }

        float4 float4::Acos() const noexcept {
            return float4 { ::acos(this->x), ::acos(this->y), ::acos(this->z), ::acos(this->w) };
        }

        float4 float4::Asin() const noexcept {
            return float4 { ::asin(this->x), ::asin(this->y), ::asin(this->z), ::asin(this->w) };
        }

        float4 float4::Atan() const noexcept {
            return float4 { ::atan(this->x), ::atan(this->y), ::atan(this->z), ::atan(this->w) };
        }

        float4 float4::Cos() const noexcept {
            return float4 { ::cos(this->x), ::cos(this->y), ::cos(this->z), ::cos(this->w) };
        }

        float4 float4::Sin() const noexcept {
            return float4 { ::sin(this->x), ::sin(this->y), ::sin(this->z), ::sin(this->w) };
        }

        float4 float4::Cosh() const noexcept {
            return float4 { ::cosh(this->x), ::cosh(this->y), ::cosh(this->z), ::cosh(this->w) };
        }

        float4 float4::Sinh() const noexcept {
            return float4 { ::sinh(this->x), ::sinh(this->y), ::sinh(this->z), ::sinh(this->w) };
        }

        float4 float4::Tan() const noexcept {
            return float4 { ::tan(this->x), ::tan(this->y), ::tan(this->z), ::tan(this->w) };
        }

        float4 float4::Exp() const noexcept {
            return float4 { ::exp(this->x), ::exp(this->y), ::exp(this->z), ::exp(this->w) };
        }

        float4 float4::Log() const noexcept {
            return float4 { ::log(this->x), ::log(this->y), ::log(this->z), ::log(this->w) };
        }

        float4 float4::Log10() const noexcept {
            return float4 { ::log10(this->x), ::log10(this->y), ::log10(this->z), ::log10(this->w) };
        }

        float4 float4::Fmod(const float4 & rhs) const noexcept {
            return float4 { ::fmod(this->x, rhs.x), ::fmod(this->y, rhs.y), ::fmod(this->z, rhs.z), ::fmod(this->w, rhs.w) };
        }

        float4 float4::Atan2(const float4 & rhs) const noexcept {
            return float4 { ::atan2(this->x, rhs.x), ::atan2(this->y, rhs.y), ::atan2(this->z, rhs.z), ::atan2(this->w, rhs.w) };
        }

        float4 float4::Pow(const float4 & rhs) const noexcept {
            return float4 { ::pow(this->x, rhs.x), ::pow(this->y, rhs.y), ::pow(this->z, rhs.z), ::pow(this->w, rhs.w) };
        }

        float4 float4::Sqrt() const noexcept {
            return float4 { ::sqrtf(this->x), ::sqrtf(this->y), ::sqrtf(this->z), ::sqrtf(this->w) };
        }

        float4 float4::Clamp(const float4 & low, const float4 & high) const noexcept {
            return float4 { (x < low.x) ? low.x: ((x > high.x) ? high.x :x), (y < low.y) ? low.y: ((y > high.y) ? high.y :y), (z < low.z) ? low.z: ((z > high.z) ? high.z :z), (w < low.w) ? low.w: ((w > high.w) ? high.w :w) };
        }

        float float4::Dot(const float4 & rhs) const noexcept {
            return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
        }

        float4 float4::Sign() const noexcept {
            return float4 { (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f), (y > 0.0f) ? 1.0f : ((y < 0.0f) ? -1.0f : 0.0f), (z > 0.0f) ? 1.0f : ((z < 0.0f) ? -1.0f : 0.0f), (w > 0.0f) ? 1.0f : ((w < 0.0f) ? -1.0f : 0.0f) };
        }

        int4 float4::Round() const noexcept {
            return int4 { (int)(x + 0.5f), (int)(y + 0.5f), (int)(z + 0.5f), (int)(w + 0.5f) }; 
        }

        float4 float4::Saturate() const noexcept {
            return Clamp(float4 { 0, 0, 0, 0 }, float4 { 1, 1, 1, 1 });
        }

        float float4::LengthSquared() const noexcept {
            return Dot(*this);
        }

        float float4::Length() const noexcept {
            return ::sqrtf(LengthSquared());
        }

        float4 float4::Normalize() const noexcept {
            float len = Length();
             return float4 { x / len , y / len , z / len , w / len  };
        }

        bool4 float4::IsNan() const noexcept {
            return bool4 { std::isnan(x), std::isnan(y), std::isnan(z), std::isnan(w) };
        }

        bool4 float4::IsFinite() const noexcept {
            return bool4 { std::isfinite(x), std::isfinite(y), std::isfinite(z), std::isfinite(w) };
        }

        bool4 float4::IsInfinite() const noexcept {
            return bool4 { !std::isfinite(x), !std::isfinite(y), !std::isfinite(z), !std::isfinite(w) };
        }

        float4 float4::operator-() const noexcept {
            return float4 { -x, -y, -z, -w };
        }

        float4 float4::operator%(const float4 & rhs) const noexcept {
            return float4 { ::fmodf(x, rhs.x), ::fmodf(y, rhs.y), ::fmodf(z, rhs.z), ::fmodf(w, rhs.w) };
        }

        float4 & float4::operator%=(const float4 & rhs) noexcept {
            x = ::fmodf(x, rhs.x);
            y = ::fmodf(y, rhs.y);
            z = ::fmodf(z, rhs.z);
            w = ::fmodf(w, rhs.w);
            return *this;
        }

        int4 float4::Ceil() const noexcept {
            return int4 { (int)::ceil(x), (int)::ceil(y), (int)::ceil(z), (int)::ceil(w) };
        }

        int4 float4::Floor() const noexcept {
            return int4 { (int)::floor(x), (int)::floor(y), (int)::floor(z), (int)::floor(w) };
        }

        float4 float4::Exp2() const noexcept {
            return float4 { ::pow(2.0f,x), ::pow(2.0f,y), ::pow(2.0f,z), ::pow(2.0f,w) };
        }

        int4 float4::Trunc() const noexcept {
            return int4 { (int)x, (int)y, (int)z, (int)w };
        }

        float float4::Distance(const float4 & rhs) const noexcept {
            return (( float4 )(*this) - rhs).Length();
        }

        bool4 float4::operator<(const float4 & rhs) const noexcept {
            return bool4 { x < rhs.x, y < rhs.y, z < rhs.z, w < rhs.w };
        }

        bool4 float4::operator>(const float4 & rhs) const noexcept {
            return bool4 { x > rhs.x, y > rhs.y, z > rhs.z, w > rhs.w };
        }

        bool4 float4::operator!=(const float4 & rhs) const noexcept {
            return bool4 { x != rhs.x, y != rhs.y, z != rhs.z, w != rhs.w };
        }

        bool4 float4::operator==(const float4 & rhs) const noexcept {
            return bool4 { x == rhs.x, y == rhs.y, z == rhs.z, w == rhs.w };
        }

        bool4 float4::operator>=(const float4 & rhs) const noexcept {
            return bool4 { x >= rhs.x, y >= rhs.y, z >= rhs.z, w >= rhs.w };
        }

        bool4 float4::operator<=(const float4 & rhs) const noexcept {
            return bool4 { x <= rhs.x, y <= rhs.y, z <= rhs.z, w <= rhs.w };
        }

        float4 float4::Random(float lower, float upper) noexcept {
            float range = upper - lower;
            return float4 {  rand() * range / RAND_MAX + lower,
             rand() * range / RAND_MAX + lower,
             rand() * range / RAND_MAX + lower,
             rand() * range / RAND_MAX + lower };
        }

        float4 float4::operator+(float v) const noexcept {
            return float4 { x + v , y + v , z + v , w + v  };
        }

        float4 float4::operator-(float v) const noexcept {
            return float4 { x - v , y - v , z - v , w - v  };
        }

        float4 float4::operator*(float v) const noexcept {
            return float4 { x * v , y * v , z * v , w * v  };
        }

        float4 float4::operator/(float v) const noexcept {
            return float4 { x / v , y / v , z / v , w / v  };
        }

        float4 float4::operator%(float v) const noexcept {
            return float4 { ::fmodf(x, v), ::fmodf(y, v), ::fmodf(z, v), ::fmodf(w, v) };
        }

        float4 float4::operator!() const noexcept {
            return float4 { -x, -y, -z, w }; 
        }

        const float4 float4::UnitX { 1.0f, 0.0f, 0.0f, 0.0f };
        const float4 float4::UnitY { 0.0f, 1.0f, 0.0f, 0.0f };
        const float4 float4::UnitZ { 0.0f, 0.0f, 1.0f, 0.0f };
        const float4 float4::UnitW { 0.0f, 0.0f, 0.0f, 1.0f };
        const float4 float4::Zero { 0.0f, 0.0f, 0.0f, 0.0f };
        const float4 float4::One { 1.0f, 1.0f, 1.0f, 1.0f };
        const float4 float4::Identity { 0.0f, 0.0f, 0.0f, 1.0f };
    }
}

