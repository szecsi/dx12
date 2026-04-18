#include "uint3.h"
#include <cmath>

namespace Egg {
    namespace Math {

        uint3::uint3(unsigned int x, unsigned int y, unsigned int z) : x { x }, y { y }, z { z }{ }

        uint3::uint3(unsigned int x, const uint2 & yz) : x { x }, y { yz.x }, z { yz.y }{ }

        uint3::uint3(const uint2 & xy, unsigned int z) : x { xy.x }, y { xy.y }, z { z }{ }

        uint3::uint3(const uint3 & xyz) : x { xyz.x }, y { xyz.y }, z { xyz.z }{ }

        uint3::uint3() : x{ 0U }, y{ 0U }, z{ 0U }{ }

        uint3 & uint3::operator=(const uint3 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            return *this;
        }

        uint3 & uint3::operator=(unsigned int rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            return *this;
        }

        uint3 & uint3::operator+=(const uint3 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            this->z += rhs.z;
            return *this;
        }

        uint3 & uint3::operator+=(unsigned int rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            this->z += rhs;
            return *this;
        }

        uint3 & uint3::operator-=(const uint3 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            this->z -= rhs.z;
            return *this;
        }

        uint3 & uint3::operator-=(unsigned int rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            this->z -= rhs;
            return *this;
        }

        uint3 & uint3::operator/=(const uint3 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            this->z /= rhs.z;
            return *this;
        }

        uint3 & uint3::operator/=(unsigned int rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            this->z /= rhs;
            return *this;
        }

        uint3 & uint3::operator*=(const uint3 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            this->z *= rhs.z;
            return *this;
        }

        uint3 & uint3::operator*=(unsigned int rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            this->z *= rhs;
            return *this;
        }

        uint3 & uint3::operator%=(const uint3 & rhs) noexcept {
            this->x %= rhs.x;
            this->y %= rhs.y;
            this->z %= rhs.z;
            return *this;
        }

        uint3 & uint3::operator%=(unsigned int rhs) noexcept {
            this->x %= rhs;
            this->y %= rhs;
            this->z %= rhs;
            return *this;
        }

        uint3 & uint3::operator|=(const uint3 & rhs) noexcept {
            this->x |= rhs.x;
            this->y |= rhs.y;
            this->z |= rhs.z;
            return *this;
        }

        uint3 & uint3::operator|=(unsigned int rhs) noexcept {
            this->x |= rhs;
            this->y |= rhs;
            this->z |= rhs;
            return *this;
        }

        uint3 & uint3::operator&=(const uint3 & rhs) noexcept {
            this->x &= rhs.x;
            this->y &= rhs.y;
            this->z &= rhs.z;
            return *this;
        }

        uint3 & uint3::operator&=(unsigned int rhs) noexcept {
            this->x &= rhs;
            this->y &= rhs;
            this->z &= rhs;
            return *this;
        }

        uint3 & uint3::operator^=(const uint3 & rhs) noexcept {
            this->x ^= rhs.x;
            this->y ^= rhs.y;
            this->z ^= rhs.z;
            return *this;
        }

        uint3 & uint3::operator^=(unsigned int rhs) noexcept {
            this->x ^= rhs;
            this->y ^= rhs;
            this->z ^= rhs;
            return *this;
        }

        uint3 & uint3::operator<<=(const uint3 & rhs) noexcept {
            this->x <<= rhs.x;
            this->y <<= rhs.y;
            this->z <<= rhs.z;
            return *this;
        }

        uint3 & uint3::operator<<=(unsigned int rhs) noexcept {
            this->x <<= rhs;
            this->y <<= rhs;
            this->z <<= rhs;
            return *this;
        }

        uint3 & uint3::operator>>=(const uint3 & rhs) noexcept {
            this->x >>= rhs.x;
            this->y >>= rhs.y;
            this->z >>= rhs.z;
            return *this;
        }

        uint3 & uint3::operator>>=(unsigned int rhs) noexcept {
            this->x >>= rhs;
            this->y >>= rhs;
            this->z >>= rhs;
            return *this;
        }

        uint3 uint3::operator*(const uint3 & rhs) const noexcept {
            return uint3 { this->x * rhs.x, this->y * rhs.y, this->z * rhs.z };
        }

        uint3 uint3::operator/(const uint3 & rhs) const noexcept {
            return uint3 { this->x / rhs.x, this->y / rhs.y, this->z / rhs.z };
        }

        uint3 uint3::operator+(const uint3 & rhs) const noexcept {
            return uint3 { this->x + rhs.x, this->y + rhs.y, this->z + rhs.z };
        }

        uint3 uint3::operator-(const uint3 & rhs) const noexcept {
            return uint3 { this->x - rhs.x, this->y - rhs.y, this->z - rhs.z };
        }

        uint3 uint3::operator%(const uint3 & rhs) const noexcept {
            return uint3 { this->x % rhs.x, this->y % rhs.y, this->z % rhs.z };
        }

        uint3 uint3::operator|(const uint3 & rhs) const noexcept {
            return uint3 { this->x | rhs.x, this->y | rhs.y, this->z | rhs.z };
        }

        uint3 uint3::operator&(const uint3 & rhs) const noexcept {
            return uint3 { this->x & rhs.x, this->y & rhs.y, this->z & rhs.z };
        }

        uint3 uint3::operator^(const uint3 & rhs) const noexcept {
            return uint3 { this->x ^ rhs.x, this->y ^ rhs.y, this->z ^ rhs.z };
        }

        uint3 uint3::operator<<(const uint3 & rhs) const noexcept {
            return uint3 { this->x << rhs.x, this->y << rhs.y, this->z << rhs.z };
        }

        uint3 uint3::operator>>(const uint3 & rhs) const noexcept {
            return uint3 { this->x >> rhs.x, this->y >> rhs.y, this->z >> rhs.z };
        }

        uint3 uint3::operator||(const uint3 & rhs) const noexcept {
            return uint3 { this->x || rhs.x, this->y || rhs.y, this->z || rhs.z };
        }

        uint3 uint3::operator&&(const uint3 & rhs) const noexcept {
            return uint3 { this->x && rhs.x, this->y && rhs.y, this->z && rhs.z };
        }

        bool3 uint3::operator<(const uint3 & rhs) const noexcept {
            return bool3 { x < rhs.x, y < rhs.y, z < rhs.z };
        }

        bool3 uint3::operator>(const uint3 & rhs) const noexcept {
            return bool3 { x > rhs.x, y > rhs.y, z > rhs.z };
        }

        bool3 uint3::operator!=(const uint3 & rhs) const noexcept {
            return bool3 { x != rhs.x, y != rhs.y, z != rhs.z };
        }

        bool3 uint3::operator==(const uint3 & rhs) const noexcept {
            return bool3 { x == rhs.x, y == rhs.y, z == rhs.z };
        }

        bool3 uint3::operator>=(const uint3 & rhs) const noexcept {
            return bool3 { x >= rhs.x, y >= rhs.y, z >= rhs.z };
        }

        bool3 uint3::operator<=(const uint3 & rhs) const noexcept {
            return bool3 { x <= rhs.x, y <= rhs.y, z <= rhs.z };
        }

        uint3 uint3::operator~() const noexcept {
            return uint3 { ~x, ~y, ~z };
        }

        uint3 uint3::operator!() const noexcept {
            return uint3 { !x, !y, !z };
        }

        uint3 uint3::operator++() noexcept {
            return uint3 { ++x, ++y, ++z };
        }

        uint3 uint3::operator++(int) noexcept {
            return uint3 { x++, y++, z++ };
        }

        uint3 uint3::operator--() noexcept {
            return uint3 { --x, --y, --z };
        }

        uint3 uint3::operator--(int) noexcept {
            return uint3 { x--, y--, z-- };
        }

        uint3 uint3::Random(unsigned int lower, unsigned int upper) noexcept {
            unsigned int range = upper - lower + 1;
             return uint3 {  rand() % range + lower,
             rand() % range + lower,
             rand() % range + lower };
        }

        const uint3 uint3::One { 1, 1, 1 };
        const uint3 uint3::Zero { 0, 0, 0 };
        const uint3 uint3::UnitX { 1, 0, 0 };
        const uint3 uint3::UnitY { 0, 1, 0 };
        const uint3 uint3::UnitZ { 0, 0, 1 };
    }
}

