#include "float2.h"
#include <cmath>
#include <cfloat>

namespace Egg {
    namespace Math {

        float2::float2(float x, float y) : x { x }, y { y }{ }

        float2::float2(const float2 & xy) : x { xy.x }, y { xy.y }{ }

        float2::float2() : x{ 0.0f }, y{ 0.0f }{ }

        float2 & float2::operator=(const float2 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            return *this;
        }

        float2 & float2::operator=(float rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            return *this;
        }

        float2 & float2::operator+=(const float2 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            return *this;
        }

        float2 & float2::operator+=(float rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            return *this;
        }

        float2 & float2::operator-=(const float2 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            return *this;
        }

        float2 & float2::operator-=(float rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            return *this;
        }

        float2 & float2::operator/=(const float2 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            return *this;
        }

        float2 & float2::operator/=(float rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            return *this;
        }

        float2 & float2::operator*=(const float2 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            return *this;
        }

        float2 & float2::operator*=(float rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            return *this;
        }

        float2 float2::operator*(const float2 & rhs) const noexcept {
            return float2 { this->x * rhs.x, this->y * rhs.y };
        }

        float2 float2::operator/(const float2 & rhs) const noexcept {
            return float2 { this->x / rhs.x, this->y / rhs.y };
        }

        float2 float2::operator+(const float2 & rhs) const noexcept {
            return float2 { this->x + rhs.x, this->y + rhs.y };
        }

        float2 float2::operator-(const float2 & rhs) const noexcept {
            return float2 { this->x - rhs.x, this->y - rhs.y };
        }

        float2 float2::Abs() const noexcept {
            return float2 { ::abs(this->x), ::abs(this->y) };
        }

        float2 float2::Acos() const noexcept {
            return float2 { ::acos(this->x), ::acos(this->y) };
        }

        float2 float2::Asin() const noexcept {
            return float2 { ::asin(this->x), ::asin(this->y) };
        }

        float2 float2::Atan() const noexcept {
            return float2 { ::atan(this->x), ::atan(this->y) };
        }

        float2 float2::Cos() const noexcept {
            return float2 { ::cos(this->x), ::cos(this->y) };
        }

        float2 float2::Sin() const noexcept {
            return float2 { ::sin(this->x), ::sin(this->y) };
        }

        float2 float2::Cosh() const noexcept {
            return float2 { ::cosh(this->x), ::cosh(this->y) };
        }

        float2 float2::Sinh() const noexcept {
            return float2 { ::sinh(this->x), ::sinh(this->y) };
        }

        float2 float2::Tan() const noexcept {
            return float2 { ::tan(this->x), ::tan(this->y) };
        }

        float2 float2::Exp() const noexcept {
            return float2 { ::exp(this->x), ::exp(this->y) };
        }

        float2 float2::Log() const noexcept {
            return float2 { ::log(this->x), ::log(this->y) };
        }

        float2 float2::Log10() const noexcept {
            return float2 { ::log10(this->x), ::log10(this->y) };
        }

        float2 float2::Fmod(const float2 & rhs) const noexcept {
            return float2 { ::fmod(this->x, rhs.x), ::fmod(this->y, rhs.y) };
        }

        float2 float2::Atan2(const float2 & rhs) const noexcept {
            return float2 { ::atan2(this->x, rhs.x), ::atan2(this->y, rhs.y) };
        }

        float2 float2::Pow(const float2 & rhs) const noexcept {
            return float2 { ::pow(this->x, rhs.x), ::pow(this->y, rhs.y) };
        }

        float2 float2::Sqrt() const noexcept {
            return float2 { ::sqrtf(this->x), ::sqrtf(this->y) };
        }

        float2 float2::Clamp(const float2 & low, const float2 & high) const noexcept {
            return float2 { (x < low.x) ? low.x: ((x > high.x) ? high.x :x), (y < low.y) ? low.y: ((y > high.y) ? high.y :y) };
        }

        float float2::Dot(const float2 & rhs) const noexcept {
            return x * rhs.x + y * rhs.y;
        }

        float2 float2::Sign() const noexcept {
            return float2 { (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f), (y > 0.0f) ? 1.0f : ((y < 0.0f) ? -1.0f : 0.0f) };
        }

        int2 float2::Round() const noexcept {
            return int2 { (int)(x + 0.5f), (int)(y + 0.5f) }; 
        }

        float2 float2::Saturate() const noexcept {
            return Clamp(float2 { 0, 0 }, float2 { 1, 1 });
        }

        float float2::LengthSquared() const noexcept {
            return Dot(*this);
        }

        float float2::Length() const noexcept {
            return ::sqrtf(LengthSquared());
        }

        float2 float2::Normalize() const noexcept {
            float len = Length();
             return float2 { x / len , y / len  };
        }

        bool2 float2::IsNan() const noexcept {
            return bool2 { std::isnan(x), std::isnan(y) };
        }

        bool2 float2::IsFinite() const noexcept {
            return bool2 { std::isfinite(x), std::isfinite(y) };
        }

        bool2 float2::IsInfinite() const noexcept {
            return bool2 { !std::isfinite(x), !std::isfinite(y) };
        }

        float2 float2::operator-() const noexcept {
            return float2 { -x, -y };
        }

        float2 float2::operator%(const float2 & rhs) const noexcept {
            return float2 { ::fmodf(x, rhs.x), ::fmodf(y, rhs.y) };
        }

        float2 & float2::operator%=(const float2 & rhs) noexcept {
            x = ::fmodf(x, rhs.x);
            y = ::fmodf(y, rhs.y);
            return *this;
        }

        int2 float2::Ceil() const noexcept {
            return int2 { (int)::ceil(x), (int)::ceil(y) };
        }

        int2 float2::Floor() const noexcept {
            return int2 { (int)::floor(x), (int)::floor(y) };
        }

        float2 float2::Exp2() const noexcept {
            return float2 { ::pow(2.0f,x), ::pow(2.0f,y) };
        }

        int2 float2::Trunc() const noexcept {
            return int2 { (int)x, (int)y };
        }

        float float2::Distance(const float2 & rhs) const noexcept {
            return (( float2 )(*this) - rhs).Length();
        }

        bool2 float2::operator<(const float2 & rhs) const noexcept {
            return bool2 { x < rhs.x, y < rhs.y };
        }

        bool2 float2::operator>(const float2 & rhs) const noexcept {
            return bool2 { x > rhs.x, y > rhs.y };
        }

        bool2 float2::operator!=(const float2 & rhs) const noexcept {
            return bool2 { x != rhs.x, y != rhs.y };
        }

        bool2 float2::operator==(const float2 & rhs) const noexcept {
            return bool2 { x == rhs.x, y == rhs.y };
        }

        bool2 float2::operator>=(const float2 & rhs) const noexcept {
            return bool2 { x >= rhs.x, y >= rhs.y };
        }

        bool2 float2::operator<=(const float2 & rhs) const noexcept {
            return bool2 { x <= rhs.x, y <= rhs.y };
        }

        float2 float2::Random(float lower, float upper) noexcept {
            float range = upper - lower;
            return float2 {  rand() * range / RAND_MAX + lower,
             rand() * range / RAND_MAX + lower };
        }

        float2 float2::operator+(float v) const noexcept {
            return float2 { x + v , y + v  };
        }

        float2 float2::operator-(float v) const noexcept {
            return float2 { x - v , y - v  };
        }

        float2 float2::operator*(float v) const noexcept {
            return float2 { x * v , y * v  };
        }

        float2 float2::operator/(float v) const noexcept {
            return float2 { x / v , y / v  };
        }

        float2 float2::operator%(float v) const noexcept {
            return float2 { ::fmodf(x, v), ::fmodf(y, v) };
        }

        float float2::Arg() const noexcept {
            return ::atan2(y, x);
        }

        float2 float2::Polar() const noexcept {
            return float2 { Length(), Arg() };
        }

        float2 float2::ComplexMul(const float2 & rhs) const noexcept {
            return float2 { x * rhs.x - y * rhs.y, x * rhs.y + y * rhs.x };
        }

        float2 float2::Cartesian() const noexcept {
            return float2 { ::cosf(y), ::sinf(y) } * x;
        }

        const float2 float2::One { 1.0f, 1.0f };
        const float2 float2::Zero { 0.0f, 0.0f };
        const float2 float2::UnitX { 1.0f, 0.0f };
        const float2 float2::UnitY { 0.0f, 1.0f };
    }
}

