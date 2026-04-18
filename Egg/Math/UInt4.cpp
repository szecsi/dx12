#include "uint4.h"
#include <cmath>

namespace Egg {
    namespace Math {

        uint4::uint4(unsigned int x, unsigned int y, unsigned int z, unsigned int w) : x { x }, y { y }, z { z }, w { w }{ }

        uint4::uint4(unsigned int x, unsigned int y, const uint2 & zw) : x { x }, y { y }, z { zw.x }, w { zw.y }{ }

        uint4::uint4(const uint2 & xy, const uint2 & zw) : x { xy.x }, y { xy.y }, z { zw.x }, w { zw.y }{ }

        uint4::uint4(const uint2 & xy, unsigned int z, unsigned int w) : x { xy.x }, y { xy.y }, z { z }, w { w }{ }

        uint4::uint4(const uint3 & xyz, unsigned int w) : x { xyz.x }, y { xyz.y }, z { xyz.z }, w { w }{ }

        uint4::uint4(unsigned int x, const uint3 & yzw) : x { x }, y { yzw.x }, z { yzw.y }, w { yzw.z }{ }

        uint4::uint4(const uint4 & xyzw) : x { xyzw.x }, y { xyzw.y }, z { xyzw.z }, w { xyzw.w }{ }

        uint4::uint4() : x{ 0U }, y{ 0U }, z{ 0U }, w{ 0U }{ }

        uint4 & uint4::operator=(const uint4 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            this->w = rhs.w;
            return *this;
        }

        uint4 & uint4::operator=(unsigned int rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            this->w = rhs;
            return *this;
        }

        uint4 & uint4::operator+=(const uint4 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            this->z += rhs.z;
            this->w += rhs.w;
            return *this;
        }

        uint4 & uint4::operator+=(unsigned int rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            this->z += rhs;
            this->w += rhs;
            return *this;
        }

        uint4 & uint4::operator-=(const uint4 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            this->z -= rhs.z;
            this->w -= rhs.w;
            return *this;
        }

        uint4 & uint4::operator-=(unsigned int rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            this->z -= rhs;
            this->w -= rhs;
            return *this;
        }

        uint4 & uint4::operator/=(const uint4 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            this->z /= rhs.z;
            this->w /= rhs.w;
            return *this;
        }

        uint4 & uint4::operator/=(unsigned int rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            this->z /= rhs;
            this->w /= rhs;
            return *this;
        }

        uint4 & uint4::operator*=(const uint4 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            this->z *= rhs.z;
            this->w *= rhs.w;
            return *this;
        }

        uint4 & uint4::operator*=(unsigned int rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            this->z *= rhs;
            this->w *= rhs;
            return *this;
        }

        uint4 & uint4::operator%=(const uint4 & rhs) noexcept {
            this->x %= rhs.x;
            this->y %= rhs.y;
            this->z %= rhs.z;
            this->w %= rhs.w;
            return *this;
        }

        uint4 & uint4::operator%=(unsigned int rhs) noexcept {
            this->x %= rhs;
            this->y %= rhs;
            this->z %= rhs;
            this->w %= rhs;
            return *this;
        }

        uint4 & uint4::operator|=(const uint4 & rhs) noexcept {
            this->x |= rhs.x;
            this->y |= rhs.y;
            this->z |= rhs.z;
            this->w |= rhs.w;
            return *this;
        }

        uint4 & uint4::operator|=(unsigned int rhs) noexcept {
            this->x |= rhs;
            this->y |= rhs;
            this->z |= rhs;
            this->w |= rhs;
            return *this;
        }

        uint4 & uint4::operator&=(const uint4 & rhs) noexcept {
            this->x &= rhs.x;
            this->y &= rhs.y;
            this->z &= rhs.z;
            this->w &= rhs.w;
            return *this;
        }

        uint4 & uint4::operator&=(unsigned int rhs) noexcept {
            this->x &= rhs;
            this->y &= rhs;
            this->z &= rhs;
            this->w &= rhs;
            return *this;
        }

        uint4 & uint4::operator^=(const uint4 & rhs) noexcept {
            this->x ^= rhs.x;
            this->y ^= rhs.y;
            this->z ^= rhs.z;
            this->w ^= rhs.w;
            return *this;
        }

        uint4 & uint4::operator^=(unsigned int rhs) noexcept {
            this->x ^= rhs;
            this->y ^= rhs;
            this->z ^= rhs;
            this->w ^= rhs;
            return *this;
        }

        uint4 & uint4::operator<<=(const uint4 & rhs) noexcept {
            this->x <<= rhs.x;
            this->y <<= rhs.y;
            this->z <<= rhs.z;
            this->w <<= rhs.w;
            return *this;
        }

        uint4 & uint4::operator<<=(unsigned int rhs) noexcept {
            this->x <<= rhs;
            this->y <<= rhs;
            this->z <<= rhs;
            this->w <<= rhs;
            return *this;
        }

        uint4 & uint4::operator>>=(const uint4 & rhs) noexcept {
            this->x >>= rhs.x;
            this->y >>= rhs.y;
            this->z >>= rhs.z;
            this->w >>= rhs.w;
            return *this;
        }

        uint4 & uint4::operator>>=(unsigned int rhs) noexcept {
            this->x >>= rhs;
            this->y >>= rhs;
            this->z >>= rhs;
            this->w >>= rhs;
            return *this;
        }

        uint4 uint4::operator*(const uint4 & rhs) const noexcept {
            return uint4 { this->x * rhs.x, this->y * rhs.y, this->z * rhs.z, this->w * rhs.w };
        }

        uint4 uint4::operator/(const uint4 & rhs) const noexcept {
            return uint4 { this->x / rhs.x, this->y / rhs.y, this->z / rhs.z, this->w / rhs.w };
        }

        uint4 uint4::operator+(const uint4 & rhs) const noexcept {
            return uint4 { this->x + rhs.x, this->y + rhs.y, this->z + rhs.z, this->w + rhs.w };
        }

        uint4 uint4::operator-(const uint4 & rhs) const noexcept {
            return uint4 { this->x - rhs.x, this->y - rhs.y, this->z - rhs.z, this->w - rhs.w };
        }

        uint4 uint4::operator%(const uint4 & rhs) const noexcept {
            return uint4 { this->x % rhs.x, this->y % rhs.y, this->z % rhs.z, this->w % rhs.w };
        }

        uint4 uint4::operator|(const uint4 & rhs) const noexcept {
            return uint4 { this->x | rhs.x, this->y | rhs.y, this->z | rhs.z, this->w | rhs.w };
        }

        uint4 uint4::operator&(const uint4 & rhs) const noexcept {
            return uint4 { this->x & rhs.x, this->y & rhs.y, this->z & rhs.z, this->w & rhs.w };
        }

        uint4 uint4::operator^(const uint4 & rhs) const noexcept {
            return uint4 { this->x ^ rhs.x, this->y ^ rhs.y, this->z ^ rhs.z, this->w ^ rhs.w };
        }

        uint4 uint4::operator<<(const uint4 & rhs) const noexcept {
            return uint4 { this->x << rhs.x, this->y << rhs.y, this->z << rhs.z, this->w << rhs.w };
        }

        uint4 uint4::operator>>(const uint4 & rhs) const noexcept {
            return uint4 { this->x >> rhs.x, this->y >> rhs.y, this->z >> rhs.z, this->w >> rhs.w };
        }

        uint4 uint4::operator||(const uint4 & rhs) const noexcept {
            return uint4 { this->x || rhs.x, this->y || rhs.y, this->z || rhs.z, this->w || rhs.w };
        }

        uint4 uint4::operator&&(const uint4 & rhs) const noexcept {
            return uint4 { this->x && rhs.x, this->y && rhs.y, this->z && rhs.z, this->w && rhs.w };
        }

        bool4 uint4::operator<(const uint4 & rhs) const noexcept {
            return bool4 { x < rhs.x, y < rhs.y, z < rhs.z, w < rhs.w };
        }

        bool4 uint4::operator>(const uint4 & rhs) const noexcept {
            return bool4 { x > rhs.x, y > rhs.y, z > rhs.z, w > rhs.w };
        }

        bool4 uint4::operator!=(const uint4 & rhs) const noexcept {
            return bool4 { x != rhs.x, y != rhs.y, z != rhs.z, w != rhs.w };
        }

        bool4 uint4::operator==(const uint4 & rhs) const noexcept {
            return bool4 { x == rhs.x, y == rhs.y, z == rhs.z, w == rhs.w };
        }

        bool4 uint4::operator>=(const uint4 & rhs) const noexcept {
            return bool4 { x >= rhs.x, y >= rhs.y, z >= rhs.z, w >= rhs.w };
        }

        bool4 uint4::operator<=(const uint4 & rhs) const noexcept {
            return bool4 { x <= rhs.x, y <= rhs.y, z <= rhs.z, w <= rhs.w };
        }

        uint4 uint4::operator~() const noexcept {
            return uint4 { ~x, ~y, ~z, ~w };
        }

        uint4 uint4::operator!() const noexcept {
            return uint4 { !x, !y, !z, !w };
        }

        uint4 uint4::operator++() noexcept {
            return uint4 { ++x, ++y, ++z, ++w };
        }

        uint4 uint4::operator++(int) noexcept {
            return uint4 { x++, y++, z++, w++ };
        }

        uint4 uint4::operator--() noexcept {
            return uint4 { --x, --y, --z, --w };
        }

        uint4 uint4::operator--(int) noexcept {
            return uint4 { x--, y--, z--, w-- };
        }

        uint4 uint4::Random(unsigned int lower, unsigned int upper) noexcept {
            unsigned int range = upper - lower + 1;
             return uint4 {  rand() % range + lower,
             rand() % range + lower,
             rand() % range + lower,
             rand() % range + lower };
        }

        const uint4 uint4::One { 1, 1, 1, 1 };
        const uint4 uint4::Zero { 0, 0, 0, 0 };
        const uint4 uint4::UnitX { 1, 0, 0, 0 };
        const uint4 uint4::UnitY { 0, 1, 0, 0 };
        const uint4 uint4::UnitZ { 0, 0, 1, 0 };
        const uint4 uint4::UnitW { 0, 0, 0, 1 };
    }
}

