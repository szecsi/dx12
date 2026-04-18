#include "float1.h"
#include <cmath>
#include <cfloat>

namespace Egg {
    namespace Math {

        float1::float1(float x) : x { x }{ }

        float1::float1() : x{ 0.0f }{ }

        float1 & float1::operator=(const float1 & rhs) noexcept {
            this->x = rhs.x;
            return *this;
        }

        float1 & float1::operator=(float rhs) noexcept {
            this->x = rhs;
            return *this;
        }

        float1 & float1::operator+=(const float1 & rhs) noexcept {
            this->x += rhs.x;
            return *this;
        }

        float1 & float1::operator+=(float rhs) noexcept {
            this->x += rhs;
            return *this;
        }

        float1 & float1::operator-=(const float1 & rhs) noexcept {
            this->x -= rhs.x;
            return *this;
        }

        float1 & float1::operator-=(float rhs) noexcept {
            this->x -= rhs;
            return *this;
        }

        float1 & float1::operator/=(const float1 & rhs) noexcept {
            this->x /= rhs.x;
            return *this;
        }

        float1 & float1::operator/=(float rhs) noexcept {
            this->x /= rhs;
            return *this;
        }

        float1 & float1::operator*=(const float1 & rhs) noexcept {
            this->x *= rhs.x;
            return *this;
        }

        float1 & float1::operator*=(float rhs) noexcept {
            this->x *= rhs;
            return *this;
        }

        float1 float1::operator*(const float1 & rhs) const noexcept {
            return float1 { this->x * rhs.x };
        }

        float1 float1::operator/(const float1 & rhs) const noexcept {
            return float1 { this->x / rhs.x };
        }

        float1 float1::operator+(const float1 & rhs) const noexcept {
            return float1 { this->x + rhs.x };
        }

        float1 float1::operator-(const float1 & rhs) const noexcept {
            return float1 { this->x - rhs.x };
        }

        float1 float1::Abs() const noexcept {
            return float1 { ::abs(this->x) };
        }

        float1 float1::Acos() const noexcept {
            return float1 { ::acos(this->x) };
        }

        float1 float1::Asin() const noexcept {
            return float1 { ::asin(this->x) };
        }

        float1 float1::Atan() const noexcept {
            return float1 { ::atan(this->x) };
        }

        float1 float1::Cos() const noexcept {
            return float1 { ::cos(this->x) };
        }

        float1 float1::Sin() const noexcept {
            return float1 { ::sin(this->x) };
        }

        float1 float1::Cosh() const noexcept {
            return float1 { ::cosh(this->x) };
        }

        float1 float1::Sinh() const noexcept {
            return float1 { ::sinh(this->x) };
        }

        float1 float1::Tan() const noexcept {
            return float1 { ::tan(this->x) };
        }

        float1 float1::Exp() const noexcept {
            return float1 { ::exp(this->x) };
        }

        float1 float1::Log() const noexcept {
            return float1 { ::log(this->x) };
        }

        float1 float1::Log10() const noexcept {
            return float1 { ::log10(this->x) };
        }

        float1 float1::Fmod(const float1 & rhs) const noexcept {
            return float1 { ::fmod(this->x, rhs.x) };
        }

        float1 float1::Atan2(const float1 & rhs) const noexcept {
            return float1 { ::atan2(this->x, rhs.x) };
        }

        float1 float1::Pow(const float1 & rhs) const noexcept {
            return float1 { ::pow(this->x, rhs.x) };
        }

        float1 float1::Sqrt() const noexcept {
            return float1 { ::sqrtf(this->x) };
        }

        float1 float1::Clamp(const float1 & low, const float1 & high) const noexcept {
            return float1 { (x < low.x) ? low.x: ((x > high.x) ? high.x :x) };
        }

        float float1::Dot(const float1 & rhs) const noexcept {
            return x * rhs.x;
        }

        float1 float1::Sign() const noexcept {
            return float1 { (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f) };
        }

        int1 float1::Round() const noexcept {
            return int1 { (int)(x + 0.5f) }; 
        }

        float1 float1::Saturate() const noexcept {
            return Clamp(float1 { 0 }, float1 { 1 });
        }

        float float1::LengthSquared() const noexcept {
            return Dot(*this);
        }

        float float1::Length() const noexcept {
            return ::sqrtf(LengthSquared());
        }

        float1 float1::Normalize() const noexcept {
            float len = Length();
             return float1 { x / len  };
        }

        bool1 float1::IsNan() const noexcept {
            return bool1 { std::isnan(x) };
        }

        bool1 float1::IsFinite() const noexcept {
            return bool1 { std::isfinite(x) };
        }

        bool1 float1::IsInfinite() const noexcept {
            return bool1 { !std::isfinite(x) };
        }

        float1 float1::operator-() const noexcept {
            return float1 { -x };
        }

        float1 float1::operator%(const float1 & rhs) const noexcept {
            return float1 { ::fmodf(x, rhs.x) };
        }

        float1 & float1::operator%=(const float1 & rhs) noexcept {
            x = ::fmodf(x, rhs.x);
            return *this;
        }

        int1 float1::Ceil() const noexcept {
            return int1 { (int)::ceil(x) };
        }

        int1 float1::Floor() const noexcept {
            return int1 { (int)::floor(x) };
        }

        float1 float1::Exp2() const noexcept {
            return float1 { ::pow(2.0f,x) };
        }

        int1 float1::Trunc() const noexcept {
            return int1 { (int)x };
        }

        float float1::Distance(const float1 & rhs) const noexcept {
            return (( float1 )(*this) - rhs).Length();
        }

        bool1 float1::operator<(const float1 & rhs) const noexcept {
            return bool1 { x < rhs.x };
        }

        bool1 float1::operator>(const float1 & rhs) const noexcept {
            return bool1 { x > rhs.x };
        }

        bool1 float1::operator!=(const float1 & rhs) const noexcept {
            return bool1 { x != rhs.x };
        }

        bool1 float1::operator==(const float1 & rhs) const noexcept {
            return bool1 { x == rhs.x };
        }

        bool1 float1::operator>=(const float1 & rhs) const noexcept {
            return bool1 { x >= rhs.x };
        }

        bool1 float1::operator<=(const float1 & rhs) const noexcept {
            return bool1 { x <= rhs.x };
        }

        float1 float1::Random(float lower, float upper) noexcept {
            float range = upper - lower;
            return float1 {  rand() * range / RAND_MAX + lower };
        }

    }
}

