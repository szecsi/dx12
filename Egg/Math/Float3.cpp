#include "float3.h"
#include <cmath>
#include <cfloat>

namespace Egg {
    namespace Math {

        float3::float3(float x, float y, float z) : x { x }, y { y }, z { z }{ }

        float3::float3(float x, const float2 & yz) : x { x }, y { yz.x }, z { yz.y }{ }

        float3::float3(const float2 & xy, float z) : x { xy.x }, y { xy.y }, z { z }{ }

        float3::float3(const float3 & xyz) : x { xyz.x }, y { xyz.y }, z { xyz.z }{ }

        float3::float3() : x{ 0.0f }, y{ 0.0f }, z{ 0.0f }{ }

        float3 & float3::operator=(const float3 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            return *this;
        }

        float3 & float3::operator=(float rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            return *this;
        }

        float3 & float3::operator+=(const float3 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            this->z += rhs.z;
            return *this;
        }

        float3 & float3::operator+=(float rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            this->z += rhs;
            return *this;
        }

        float3 & float3::operator-=(const float3 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            this->z -= rhs.z;
            return *this;
        }

        float3 & float3::operator-=(float rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            this->z -= rhs;
            return *this;
        }

        float3 & float3::operator/=(const float3 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            this->z /= rhs.z;
            return *this;
        }

        float3 & float3::operator/=(float rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            this->z /= rhs;
            return *this;
        }

        float3 & float3::operator*=(const float3 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            this->z *= rhs.z;
            return *this;
        }

        float3 & float3::operator*=(float rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            this->z *= rhs;
            return *this;
        }

        float3 float3::operator*(const float3 & rhs) const noexcept {
            return float3 { this->x * rhs.x, this->y * rhs.y, this->z * rhs.z };
        }

        float3 float3::operator/(const float3 & rhs) const noexcept {
            return float3 { this->x / rhs.x, this->y / rhs.y, this->z / rhs.z };
        }

        float3 float3::operator+(const float3 & rhs) const noexcept {
            return float3 { this->x + rhs.x, this->y + rhs.y, this->z + rhs.z };
        }

        float3 float3::operator-(const float3 & rhs) const noexcept {
            return float3 { this->x - rhs.x, this->y - rhs.y, this->z - rhs.z };
        }

        float3 float3::Abs() const noexcept {
            return float3 { ::abs(this->x), ::abs(this->y), ::abs(this->z) };
        }

        float3 float3::Acos() const noexcept {
            return float3 { ::acos(this->x), ::acos(this->y), ::acos(this->z) };
        }

        float3 float3::Asin() const noexcept {
            return float3 { ::asin(this->x), ::asin(this->y), ::asin(this->z) };
        }

        float3 float3::Atan() const noexcept {
            return float3 { ::atan(this->x), ::atan(this->y), ::atan(this->z) };
        }

        float3 float3::Cos() const noexcept {
            return float3 { ::cos(this->x), ::cos(this->y), ::cos(this->z) };
        }

        float3 float3::Sin() const noexcept {
            return float3 { ::sin(this->x), ::sin(this->y), ::sin(this->z) };
        }

        float3 float3::Cosh() const noexcept {
            return float3 { ::cosh(this->x), ::cosh(this->y), ::cosh(this->z) };
        }

        float3 float3::Sinh() const noexcept {
            return float3 { ::sinh(this->x), ::sinh(this->y), ::sinh(this->z) };
        }

        float3 float3::Tan() const noexcept {
            return float3 { ::tan(this->x), ::tan(this->y), ::tan(this->z) };
        }

        float3 float3::Exp() const noexcept {
            return float3 { ::exp(this->x), ::exp(this->y), ::exp(this->z) };
        }

        float3 float3::Log() const noexcept {
            return float3 { ::log(this->x), ::log(this->y), ::log(this->z) };
        }

        float3 float3::Log10() const noexcept {
            return float3 { ::log10(this->x), ::log10(this->y), ::log10(this->z) };
        }

        float3 float3::Fmod(const float3 & rhs) const noexcept {
            return float3 { ::fmod(this->x, rhs.x), ::fmod(this->y, rhs.y), ::fmod(this->z, rhs.z) };
        }

        float3 float3::Atan2(const float3 & rhs) const noexcept {
            return float3 { ::atan2(this->x, rhs.x), ::atan2(this->y, rhs.y), ::atan2(this->z, rhs.z) };
        }

        float3 float3::Pow(const float3 & rhs) const noexcept {
            return float3 { ::pow(this->x, rhs.x), ::pow(this->y, rhs.y), ::pow(this->z, rhs.z) };
        }

        float3 float3::Sqrt() const noexcept {
            return float3 { ::sqrtf(this->x), ::sqrtf(this->y), ::sqrtf(this->z) };
        }

        float3 float3::Clamp(const float3 & low, const float3 & high) const noexcept {
            return float3 { (x < low.x) ? low.x: ((x > high.x) ? high.x :x), (y < low.y) ? low.y: ((y > high.y) ? high.y :y), (z < low.z) ? low.z: ((z > high.z) ? high.z :z) };
        }

        float float3::Dot(const float3 & rhs) const noexcept {
            return x * rhs.x + y * rhs.y + z * rhs.z;
        }

        float3 float3::Sign() const noexcept {
            return float3 { (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f), (y > 0.0f) ? 1.0f : ((y < 0.0f) ? -1.0f : 0.0f), (z > 0.0f) ? 1.0f : ((z < 0.0f) ? -1.0f : 0.0f) };
        }

        int3 float3::Round() const noexcept {
            return int3 { (int)(x + 0.5f), (int)(y + 0.5f), (int)(z + 0.5f) }; 
        }

        float3 float3::Saturate() const noexcept {
            return Clamp(float3 { 0, 0, 0 }, float3 { 1, 1, 1 });
        }

        float float3::LengthSquared() const noexcept {
            return Dot(*this);
        }

        float float3::Length() const noexcept {
            return ::sqrtf(LengthSquared());
        }

        float3 float3::Normalize() const noexcept {
            float len = Length();
             return float3 { x / len , y / len , z / len  };
        }

        bool3 float3::IsNan() const noexcept {
            return bool3 { std::isnan(x), std::isnan(y), std::isnan(z) };
        }

        bool3 float3::IsFinite() const noexcept {
            return bool3 { std::isfinite(x), std::isfinite(y), std::isfinite(z) };
        }

        bool3 float3::IsInfinite() const noexcept {
            return bool3 { !std::isfinite(x), !std::isfinite(y), !std::isfinite(z) };
        }

        float3 float3::operator-() const noexcept {
            return float3 { -x, -y, -z };
        }

        float3 float3::operator%(const float3 & rhs) const noexcept {
            return float3 { ::fmodf(x, rhs.x), ::fmodf(y, rhs.y), ::fmodf(z, rhs.z) };
        }

        float3 & float3::operator%=(const float3 & rhs) noexcept {
            x = ::fmodf(x, rhs.x);
            y = ::fmodf(y, rhs.y);
            z = ::fmodf(z, rhs.z);
            return *this;
        }

        int3 float3::Ceil() const noexcept {
            return int3 { (int)::ceil(x), (int)::ceil(y), (int)::ceil(z) };
        }

        int3 float3::Floor() const noexcept {
            return int3 { (int)::floor(x), (int)::floor(y), (int)::floor(z) };
        }

        float3 float3::Exp2() const noexcept {
            return float3 { ::pow(2.0f,x), ::pow(2.0f,y), ::pow(2.0f,z) };
        }

        int3 float3::Trunc() const noexcept {
            return int3 { (int)x, (int)y, (int)z };
        }

        float float3::Distance(const float3 & rhs) const noexcept {
            return (( float3 )(*this) - rhs).Length();
        }

        bool3 float3::operator<(const float3 & rhs) const noexcept {
            return bool3 { x < rhs.x, y < rhs.y, z < rhs.z };
        }

        bool3 float3::operator>(const float3 & rhs) const noexcept {
            return bool3 { x > rhs.x, y > rhs.y, z > rhs.z };
        }

        bool3 float3::operator!=(const float3 & rhs) const noexcept {
            return bool3 { x != rhs.x, y != rhs.y, z != rhs.z };
        }

        bool3 float3::operator==(const float3 & rhs) const noexcept {
            return bool3 { x == rhs.x, y == rhs.y, z == rhs.z };
        }

        bool3 float3::operator>=(const float3 & rhs) const noexcept {
            return bool3 { x >= rhs.x, y >= rhs.y, z >= rhs.z };
        }

        bool3 float3::operator<=(const float3 & rhs) const noexcept {
            return bool3 { x <= rhs.x, y <= rhs.y, z <= rhs.z };
        }

        float3 float3::Random(float lower, float upper) noexcept {
            float range = upper - lower;
            return float3 {  rand() * range / RAND_MAX + lower,
             rand() * range / RAND_MAX + lower,
             rand() * range / RAND_MAX + lower };
        }

        float3 float3::operator+(float v) const noexcept {
            return float3 { x + v , y + v , z + v  };
        }

        float3 float3::operator-(float v) const noexcept {
            return float3 { x - v , y - v , z - v  };
        }

        float3 float3::operator*(float v) const noexcept {
            return float3 { x * v , y * v , z * v  };
        }

        float3 float3::operator/(float v) const noexcept {
            return float3 { x / v , y / v , z / v  };
        }

        float3 float3::operator%(float v) const noexcept {
            return float3 { ::fmodf(x, v), ::fmodf(y, v), ::fmodf(z, v) };
        }

        float3 float3::Cross(const float3 & rhs) const noexcept {
            return float3 { y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x };
        }

        const float3 float3::UnitX { 1.0f, 0.0f, 0.0f };
        const float3 float3::UnitY { 0.0f, 1.0f, 0.0f };
        const float3 float3::UnitZ { 0.0f, 0.0f, 1.0f };
        const float3 float3::Zero { 0.0f, 0.0f, 0.0f };
        const float3 float3::One { 1.0f, 1.0f, 1.0f };
        const float3 float3::Black { 0.0f, 0.0f, 0.0f };
        const float3 float3::Navy { 0.0f, 0.0f, 0.5f };
        const float3 float3::Blue { 0.0f, 0.0f, 1.0f };
        const float3 float3::DarkGreen { 0.0f, 0.5f, 0.0f };
        const float3 float3::Teal { 0.0f, 0.5f, 0.5f };
        const float3 float3::Azure { 0.0f, 0.5f, 1.0f };
        const float3 float3::Green { 0.0f, 1.0f, 0.0f };
        const float3 float3::Cyan { 0.0f, 1.0f, 1.0f };
        const float3 float3::Maroon { 0.5f, 0.0f, 0.0f };
        const float3 float3::Purple { 0.5f, 0.0f, 0.5f };
        const float3 float3::SlateBlue { 0.5f, 0.0f, 1.0f };
        const float3 float3::Olive { 0.5f, 0.5f, 0.0f };
        const float3 float3::Gray { 0.5f, 0.5f, 0.5f };
        const float3 float3::Cornflower { 0.5f, 0.5f, 1.0f };
        const float3 float3::Aquamarine { 0.5f, 1.0f, 0.75f };
        const float3 float3::Red { 1.0f, 0.0f, 0.0f };
        const float3 float3::DeepPink { 1.0f, 0.0f, 0.5f };
        const float3 float3::Magenta { 1.0f, 0.0f, 1.0f };
        const float3 float3::Orange { 1.0f, 0.5f, 0.0f };
        const float3 float3::Coral { 1.0f, 0.5f, 0.31f };
        const float3 float3::Mallow { 1.0f, 0.5f, 1.0f };
        const float3 float3::Yellow { 1.0f, 1.0f, 0.0f };
        const float3 float3::Gold { 1.0f, 1.0f, 0.5f };
        const float3 float3::White { 1.0f, 1.0f, 1.0f };
        const float3 float3::Silver { 0.75f, 0.75f, 0.75f };
    }
}

