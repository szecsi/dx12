#include "int2.h"
#include <cmath>

namespace Egg {
    namespace Math {

        int2::int2(int x, int y) : x { x }, y { y }{ }

        int2::int2(const int2 & xy) : x { xy.x }, y { xy.y }{ }

        int2::int2() : x{ 0 }, y{ 0 }{ }

        int2 & int2::operator=(const int2 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            return *this;
        }

        int2 & int2::operator=(int rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            return *this;
        }

        int2 & int2::operator+=(const int2 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            return *this;
        }

        int2 & int2::operator+=(int rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            return *this;
        }

        int2 & int2::operator-=(const int2 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            return *this;
        }

        int2 & int2::operator-=(int rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            return *this;
        }

        int2 & int2::operator/=(const int2 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            return *this;
        }

        int2 & int2::operator/=(int rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            return *this;
        }

        int2 & int2::operator*=(const int2 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            return *this;
        }

        int2 & int2::operator*=(int rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            return *this;
        }

        int2 & int2::operator%=(const int2 & rhs) noexcept {
            this->x %= rhs.x;
            this->y %= rhs.y;
            return *this;
        }

        int2 & int2::operator%=(int rhs) noexcept {
            this->x %= rhs;
            this->y %= rhs;
            return *this;
        }

        int2 & int2::operator|=(const int2 & rhs) noexcept {
            this->x |= rhs.x;
            this->y |= rhs.y;
            return *this;
        }

        int2 & int2::operator|=(int rhs) noexcept {
            this->x |= rhs;
            this->y |= rhs;
            return *this;
        }

        int2 & int2::operator&=(const int2 & rhs) noexcept {
            this->x &= rhs.x;
            this->y &= rhs.y;
            return *this;
        }

        int2 & int2::operator&=(int rhs) noexcept {
            this->x &= rhs;
            this->y &= rhs;
            return *this;
        }

        int2 & int2::operator^=(const int2 & rhs) noexcept {
            this->x ^= rhs.x;
            this->y ^= rhs.y;
            return *this;
        }

        int2 & int2::operator^=(int rhs) noexcept {
            this->x ^= rhs;
            this->y ^= rhs;
            return *this;
        }

        int2 & int2::operator<<=(const int2 & rhs) noexcept {
            this->x <<= rhs.x;
            this->y <<= rhs.y;
            return *this;
        }

        int2 & int2::operator<<=(int rhs) noexcept {
            this->x <<= rhs;
            this->y <<= rhs;
            return *this;
        }

        int2 & int2::operator>>=(const int2 & rhs) noexcept {
            this->x >>= rhs.x;
            this->y >>= rhs.y;
            return *this;
        }

        int2 & int2::operator>>=(int rhs) noexcept {
            this->x >>= rhs;
            this->y >>= rhs;
            return *this;
        }

        int2 int2::operator*(const int2 & rhs) const noexcept {
            return int2 { this->x * rhs.x, this->y * rhs.y };
        }

        int2 int2::operator/(const int2 & rhs) const noexcept {
            return int2 { this->x / rhs.x, this->y / rhs.y };
        }

        int2 int2::operator+(const int2 & rhs) const noexcept {
            return int2 { this->x + rhs.x, this->y + rhs.y };
        }

        int2 int2::operator-(const int2 & rhs) const noexcept {
            return int2 { this->x - rhs.x, this->y - rhs.y };
        }

        int2 int2::operator%(const int2 & rhs) const noexcept {
            return int2 { this->x % rhs.x, this->y % rhs.y };
        }

        int2 int2::operator|(const int2 & rhs) const noexcept {
            return int2 { this->x | rhs.x, this->y | rhs.y };
        }

        int2 int2::operator&(const int2 & rhs) const noexcept {
            return int2 { this->x & rhs.x, this->y & rhs.y };
        }

        int2 int2::operator^(const int2 & rhs) const noexcept {
            return int2 { this->x ^ rhs.x, this->y ^ rhs.y };
        }

        int2 int2::operator<<(const int2 & rhs) const noexcept {
            return int2 { this->x << rhs.x, this->y << rhs.y };
        }

        int2 int2::operator>>(const int2 & rhs) const noexcept {
            return int2 { this->x >> rhs.x, this->y >> rhs.y };
        }

        int2 int2::operator||(const int2 & rhs) const noexcept {
            return int2 { this->x || rhs.x, this->y || rhs.y };
        }

        int2 int2::operator&&(const int2 & rhs) const noexcept {
            return int2 { this->x && rhs.x, this->y && rhs.y };
        }

        bool2 int2::operator<(const int2 & rhs) const noexcept {
            return bool2 { x < rhs.x, y < rhs.y };
        }

        bool2 int2::operator>(const int2 & rhs) const noexcept {
            return bool2 { x > rhs.x, y > rhs.y };
        }

        bool2 int2::operator!=(const int2 & rhs) const noexcept {
            return bool2 { x != rhs.x, y != rhs.y };
        }

        bool2 int2::operator==(const int2 & rhs) const noexcept {
            return bool2 { x == rhs.x, y == rhs.y };
        }

        bool2 int2::operator>=(const int2 & rhs) const noexcept {
            return bool2 { x >= rhs.x, y >= rhs.y };
        }

        bool2 int2::operator<=(const int2 & rhs) const noexcept {
            return bool2 { x <= rhs.x, y <= rhs.y };
        }

        int2 int2::operator~() const noexcept {
            return int2 { ~x, ~y };
        }

        int2 int2::operator!() const noexcept {
            return int2 { !x, !y };
        }

        int2 int2::operator++() noexcept {
            return int2 { ++x, ++y };
        }

        int2 int2::operator++(int) noexcept {
            return int2 { x++, y++ };
        }

        int2 int2::operator--() noexcept {
            return int2 { --x, --y };
        }

        int2 int2::operator--(int) noexcept {
            return int2 { x--, y-- };
        }

        int2 int2::Random(int lower, int upper) noexcept {
            int range = upper - lower + 1;
             return int2 {  rand() % range + lower,
             rand() % range + lower };
        }

        int2 int2::operator-() const noexcept {
            return int2 { -x, -y };
        }

        const int2 int2::One { 1, 1 };
        const int2 int2::Zero { 0, 0 };
        const int2 int2::UnitX { 1, 0 };
        const int2 int2::UnitY { 0, 1 };
    }
}

