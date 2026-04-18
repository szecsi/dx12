#include "int4.h"
#include <cmath>

namespace Egg {
    namespace Math {

        int4::int4(int x, int y, int z, int w) : x { x }, y { y }, z { z }, w { w }{ }

        int4::int4(int x, int y, const int2 & zw) : x { x }, y { y }, z { zw.x }, w { zw.y }{ }

        int4::int4(const int2 & xy, const int2 & zw) : x { xy.x }, y { xy.y }, z { zw.x }, w { zw.y }{ }

        int4::int4(const int2 & xy, int z, int w) : x { xy.x }, y { xy.y }, z { z }, w { w }{ }

        int4::int4(const int3 & xyz, int w) : x { xyz.x }, y { xyz.y }, z { xyz.z }, w { w }{ }

        int4::int4(int x, const int3 & yzw) : x { x }, y { yzw.x }, z { yzw.y }, w { yzw.z }{ }

        int4::int4(const int4 & xyzw) : x { xyzw.x }, y { xyzw.y }, z { xyzw.z }, w { xyzw.w }{ }

        int4::int4() : x{ 0 }, y{ 0 }, z{ 0 }, w{ 0 }{ }

        int4 & int4::operator=(const int4 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            this->w = rhs.w;
            return *this;
        }

        int4 & int4::operator=(int rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            this->w = rhs;
            return *this;
        }

        int4 & int4::operator+=(const int4 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            this->z += rhs.z;
            this->w += rhs.w;
            return *this;
        }

        int4 & int4::operator+=(int rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            this->z += rhs;
            this->w += rhs;
            return *this;
        }

        int4 & int4::operator-=(const int4 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            this->z -= rhs.z;
            this->w -= rhs.w;
            return *this;
        }

        int4 & int4::operator-=(int rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            this->z -= rhs;
            this->w -= rhs;
            return *this;
        }

        int4 & int4::operator/=(const int4 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            this->z /= rhs.z;
            this->w /= rhs.w;
            return *this;
        }

        int4 & int4::operator/=(int rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            this->z /= rhs;
            this->w /= rhs;
            return *this;
        }

        int4 & int4::operator*=(const int4 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            this->z *= rhs.z;
            this->w *= rhs.w;
            return *this;
        }

        int4 & int4::operator*=(int rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            this->z *= rhs;
            this->w *= rhs;
            return *this;
        }

        int4 & int4::operator%=(const int4 & rhs) noexcept {
            this->x %= rhs.x;
            this->y %= rhs.y;
            this->z %= rhs.z;
            this->w %= rhs.w;
            return *this;
        }

        int4 & int4::operator%=(int rhs) noexcept {
            this->x %= rhs;
            this->y %= rhs;
            this->z %= rhs;
            this->w %= rhs;
            return *this;
        }

        int4 & int4::operator|=(const int4 & rhs) noexcept {
            this->x |= rhs.x;
            this->y |= rhs.y;
            this->z |= rhs.z;
            this->w |= rhs.w;
            return *this;
        }

        int4 & int4::operator|=(int rhs) noexcept {
            this->x |= rhs;
            this->y |= rhs;
            this->z |= rhs;
            this->w |= rhs;
            return *this;
        }

        int4 & int4::operator&=(const int4 & rhs) noexcept {
            this->x &= rhs.x;
            this->y &= rhs.y;
            this->z &= rhs.z;
            this->w &= rhs.w;
            return *this;
        }

        int4 & int4::operator&=(int rhs) noexcept {
            this->x &= rhs;
            this->y &= rhs;
            this->z &= rhs;
            this->w &= rhs;
            return *this;
        }

        int4 & int4::operator^=(const int4 & rhs) noexcept {
            this->x ^= rhs.x;
            this->y ^= rhs.y;
            this->z ^= rhs.z;
            this->w ^= rhs.w;
            return *this;
        }

        int4 & int4::operator^=(int rhs) noexcept {
            this->x ^= rhs;
            this->y ^= rhs;
            this->z ^= rhs;
            this->w ^= rhs;
            return *this;
        }

        int4 & int4::operator<<=(const int4 & rhs) noexcept {
            this->x <<= rhs.x;
            this->y <<= rhs.y;
            this->z <<= rhs.z;
            this->w <<= rhs.w;
            return *this;
        }

        int4 & int4::operator<<=(int rhs) noexcept {
            this->x <<= rhs;
            this->y <<= rhs;
            this->z <<= rhs;
            this->w <<= rhs;
            return *this;
        }

        int4 & int4::operator>>=(const int4 & rhs) noexcept {
            this->x >>= rhs.x;
            this->y >>= rhs.y;
            this->z >>= rhs.z;
            this->w >>= rhs.w;
            return *this;
        }

        int4 & int4::operator>>=(int rhs) noexcept {
            this->x >>= rhs;
            this->y >>= rhs;
            this->z >>= rhs;
            this->w >>= rhs;
            return *this;
        }

        int4 int4::operator*(const int4 & rhs) const noexcept {
            return int4 { this->x * rhs.x, this->y * rhs.y, this->z * rhs.z, this->w * rhs.w };
        }

        int4 int4::operator/(const int4 & rhs) const noexcept {
            return int4 { this->x / rhs.x, this->y / rhs.y, this->z / rhs.z, this->w / rhs.w };
        }

        int4 int4::operator+(const int4 & rhs) const noexcept {
            return int4 { this->x + rhs.x, this->y + rhs.y, this->z + rhs.z, this->w + rhs.w };
        }

        int4 int4::operator-(const int4 & rhs) const noexcept {
            return int4 { this->x - rhs.x, this->y - rhs.y, this->z - rhs.z, this->w - rhs.w };
        }

        int4 int4::operator%(const int4 & rhs) const noexcept {
            return int4 { this->x % rhs.x, this->y % rhs.y, this->z % rhs.z, this->w % rhs.w };
        }

        int4 int4::operator|(const int4 & rhs) const noexcept {
            return int4 { this->x | rhs.x, this->y | rhs.y, this->z | rhs.z, this->w | rhs.w };
        }

        int4 int4::operator&(const int4 & rhs) const noexcept {
            return int4 { this->x & rhs.x, this->y & rhs.y, this->z & rhs.z, this->w & rhs.w };
        }

        int4 int4::operator^(const int4 & rhs) const noexcept {
            return int4 { this->x ^ rhs.x, this->y ^ rhs.y, this->z ^ rhs.z, this->w ^ rhs.w };
        }

        int4 int4::operator<<(const int4 & rhs) const noexcept {
            return int4 { this->x << rhs.x, this->y << rhs.y, this->z << rhs.z, this->w << rhs.w };
        }

        int4 int4::operator>>(const int4 & rhs) const noexcept {
            return int4 { this->x >> rhs.x, this->y >> rhs.y, this->z >> rhs.z, this->w >> rhs.w };
        }

        int4 int4::operator||(const int4 & rhs) const noexcept {
            return int4 { this->x || rhs.x, this->y || rhs.y, this->z || rhs.z, this->w || rhs.w };
        }

        int4 int4::operator&&(const int4 & rhs) const noexcept {
            return int4 { this->x && rhs.x, this->y && rhs.y, this->z && rhs.z, this->w && rhs.w };
        }

        bool4 int4::operator<(const int4 & rhs) const noexcept {
            return bool4 { x < rhs.x, y < rhs.y, z < rhs.z, w < rhs.w };
        }

        bool4 int4::operator>(const int4 & rhs) const noexcept {
            return bool4 { x > rhs.x, y > rhs.y, z > rhs.z, w > rhs.w };
        }

        bool4 int4::operator!=(const int4 & rhs) const noexcept {
            return bool4 { x != rhs.x, y != rhs.y, z != rhs.z, w != rhs.w };
        }

        bool4 int4::operator==(const int4 & rhs) const noexcept {
            return bool4 { x == rhs.x, y == rhs.y, z == rhs.z, w == rhs.w };
        }

        bool4 int4::operator>=(const int4 & rhs) const noexcept {
            return bool4 { x >= rhs.x, y >= rhs.y, z >= rhs.z, w >= rhs.w };
        }

        bool4 int4::operator<=(const int4 & rhs) const noexcept {
            return bool4 { x <= rhs.x, y <= rhs.y, z <= rhs.z, w <= rhs.w };
        }

        int4 int4::operator~() const noexcept {
            return int4 { ~x, ~y, ~z, ~w };
        }

        int4 int4::operator!() const noexcept {
            return int4 { !x, !y, !z, !w };
        }

        int4 int4::operator++() noexcept {
            return int4 { ++x, ++y, ++z, ++w };
        }

        int4 int4::operator++(int) noexcept {
            return int4 { x++, y++, z++, w++ };
        }

        int4 int4::operator--() noexcept {
            return int4 { --x, --y, --z, --w };
        }

        int4 int4::operator--(int) noexcept {
            return int4 { x--, y--, z--, w-- };
        }

        int4 int4::Random(int lower, int upper) noexcept {
            int range = upper - lower + 1;
             return int4 {  rand() % range + lower,
             rand() % range + lower,
             rand() % range + lower,
             rand() % range + lower };
        }

        int4 int4::operator-() const noexcept {
            return int4 { -x, -y, -z, -w };
        }

        const int4 int4::One { 1, 1, 1, 1 };
        const int4 int4::Zero { 0, 0, 0, 0 };
        const int4 int4::UnitX { 1, 0, 0, 0 };
        const int4 int4::UnitY { 0, 1, 0, 0 };
        const int4 int4::UnitZ { 0, 0, 1, 0 };
        const int4 int4::UnitW { 0, 0, 0, 1 };
    }
}

