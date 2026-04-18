#include "uint2.h"
#include <cmath>

namespace Egg {
    namespace Math {

        uint2::uint2(unsigned int x, unsigned int y) : x { x }, y { y }{ }

        uint2::uint2(const uint2 & xy) : x { xy.x }, y { xy.y }{ }

        uint2::uint2() : x{ 0U }, y{ 0U }{ }

        uint2 & uint2::operator=(const uint2 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            return *this;
        }

        uint2 & uint2::operator=(unsigned int rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            return *this;
        }

        uint2 & uint2::operator+=(const uint2 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            return *this;
        }

        uint2 & uint2::operator+=(unsigned int rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            return *this;
        }

        uint2 & uint2::operator-=(const uint2 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            return *this;
        }

        uint2 & uint2::operator-=(unsigned int rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            return *this;
        }

        uint2 & uint2::operator/=(const uint2 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            return *this;
        }

        uint2 & uint2::operator/=(unsigned int rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            return *this;
        }

        uint2 & uint2::operator*=(const uint2 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            return *this;
        }

        uint2 & uint2::operator*=(unsigned int rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            return *this;
        }

        uint2 & uint2::operator%=(const uint2 & rhs) noexcept {
            this->x %= rhs.x;
            this->y %= rhs.y;
            return *this;
        }

        uint2 & uint2::operator%=(unsigned int rhs) noexcept {
            this->x %= rhs;
            this->y %= rhs;
            return *this;
        }

        uint2 & uint2::operator|=(const uint2 & rhs) noexcept {
            this->x |= rhs.x;
            this->y |= rhs.y;
            return *this;
        }

        uint2 & uint2::operator|=(unsigned int rhs) noexcept {
            this->x |= rhs;
            this->y |= rhs;
            return *this;
        }

        uint2 & uint2::operator&=(const uint2 & rhs) noexcept {
            this->x &= rhs.x;
            this->y &= rhs.y;
            return *this;
        }

        uint2 & uint2::operator&=(unsigned int rhs) noexcept {
            this->x &= rhs;
            this->y &= rhs;
            return *this;
        }

        uint2 & uint2::operator^=(const uint2 & rhs) noexcept {
            this->x ^= rhs.x;
            this->y ^= rhs.y;
            return *this;
        }

        uint2 & uint2::operator^=(unsigned int rhs) noexcept {
            this->x ^= rhs;
            this->y ^= rhs;
            return *this;
        }

        uint2 & uint2::operator<<=(const uint2 & rhs) noexcept {
            this->x <<= rhs.x;
            this->y <<= rhs.y;
            return *this;
        }

        uint2 & uint2::operator<<=(unsigned int rhs) noexcept {
            this->x <<= rhs;
            this->y <<= rhs;
            return *this;
        }

        uint2 & uint2::operator>>=(const uint2 & rhs) noexcept {
            this->x >>= rhs.x;
            this->y >>= rhs.y;
            return *this;
        }

        uint2 & uint2::operator>>=(unsigned int rhs) noexcept {
            this->x >>= rhs;
            this->y >>= rhs;
            return *this;
        }

        uint2 uint2::operator*(const uint2 & rhs) const noexcept {
            return uint2 { this->x * rhs.x, this->y * rhs.y };
        }

        uint2 uint2::operator/(const uint2 & rhs) const noexcept {
            return uint2 { this->x / rhs.x, this->y / rhs.y };
        }

        uint2 uint2::operator+(const uint2 & rhs) const noexcept {
            return uint2 { this->x + rhs.x, this->y + rhs.y };
        }

        uint2 uint2::operator-(const uint2 & rhs) const noexcept {
            return uint2 { this->x - rhs.x, this->y - rhs.y };
        }

        uint2 uint2::operator%(const uint2 & rhs) const noexcept {
            return uint2 { this->x % rhs.x, this->y % rhs.y };
        }

        uint2 uint2::operator|(const uint2 & rhs) const noexcept {
            return uint2 { this->x | rhs.x, this->y | rhs.y };
        }

        uint2 uint2::operator&(const uint2 & rhs) const noexcept {
            return uint2 { this->x & rhs.x, this->y & rhs.y };
        }

        uint2 uint2::operator^(const uint2 & rhs) const noexcept {
            return uint2 { this->x ^ rhs.x, this->y ^ rhs.y };
        }

        uint2 uint2::operator<<(const uint2 & rhs) const noexcept {
            return uint2 { this->x << rhs.x, this->y << rhs.y };
        }

        uint2 uint2::operator>>(const uint2 & rhs) const noexcept {
            return uint2 { this->x >> rhs.x, this->y >> rhs.y };
        }

        uint2 uint2::operator||(const uint2 & rhs) const noexcept {
            return uint2 { this->x || rhs.x, this->y || rhs.y };
        }

        uint2 uint2::operator&&(const uint2 & rhs) const noexcept {
            return uint2 { this->x && rhs.x, this->y && rhs.y };
        }

        bool2 uint2::operator<(const uint2 & rhs) const noexcept {
            return bool2 { x < rhs.x, y < rhs.y };
        }

        bool2 uint2::operator>(const uint2 & rhs) const noexcept {
            return bool2 { x > rhs.x, y > rhs.y };
        }

        bool2 uint2::operator!=(const uint2 & rhs) const noexcept {
            return bool2 { x != rhs.x, y != rhs.y };
        }

        bool2 uint2::operator==(const uint2 & rhs) const noexcept {
            return bool2 { x == rhs.x, y == rhs.y };
        }

        bool2 uint2::operator>=(const uint2 & rhs) const noexcept {
            return bool2 { x >= rhs.x, y >= rhs.y };
        }

        bool2 uint2::operator<=(const uint2 & rhs) const noexcept {
            return bool2 { x <= rhs.x, y <= rhs.y };
        }

        uint2 uint2::operator~() const noexcept {
            return uint2 { ~x, ~y };
        }

        uint2 uint2::operator!() const noexcept {
            return uint2 { !x, !y };
        }

        uint2 uint2::operator++() noexcept {
            return uint2 { ++x, ++y };
        }

        uint2 uint2::operator++(int) noexcept {
            return uint2 { x++, y++ };
        }

        uint2 uint2::operator--() noexcept {
            return uint2 { --x, --y };
        }

        uint2 uint2::operator--(int) noexcept {
            return uint2 { x--, y-- };
        }

        uint2 uint2::Random(unsigned int lower, unsigned int upper) noexcept {
            unsigned int range = upper - lower + 1;
             return uint2 {  rand() % range + lower,
             rand() % range + lower };
        }

        const uint2 uint2::One { 1, 1 };
        const uint2 uint2::Zero { 0, 0 };
        const uint2 uint2::UnitX { 1, 0 };
        const uint2 uint2::UnitY { 0, 1 };
    }
}

