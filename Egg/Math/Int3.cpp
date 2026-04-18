#include "int3.h"
#include <cmath>

namespace Egg {
    namespace Math {

        int3::int3(int x, int y, int z) : x { x }, y { y }, z { z }{ }

        int3::int3(int x, const int2 & yz) : x { x }, y { yz.x }, z { yz.y }{ }

        int3::int3(const int2 & xy, int z) : x { xy.x }, y { xy.y }, z { z }{ }

        int3::int3(const int3 & xyz) : x { xyz.x }, y { xyz.y }, z { xyz.z }{ }

        int3::int3() : x{ 0 }, y{ 0 }, z{ 0 }{ }

        int3 & int3::operator=(const int3 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            return *this;
        }

        int3 & int3::operator=(int rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            return *this;
        }

        int3 & int3::operator+=(const int3 & rhs) noexcept {
            this->x += rhs.x;
            this->y += rhs.y;
            this->z += rhs.z;
            return *this;
        }

        int3 & int3::operator+=(int rhs) noexcept {
            this->x += rhs;
            this->y += rhs;
            this->z += rhs;
            return *this;
        }

        int3 & int3::operator-=(const int3 & rhs) noexcept {
            this->x -= rhs.x;
            this->y -= rhs.y;
            this->z -= rhs.z;
            return *this;
        }

        int3 & int3::operator-=(int rhs) noexcept {
            this->x -= rhs;
            this->y -= rhs;
            this->z -= rhs;
            return *this;
        }

        int3 & int3::operator/=(const int3 & rhs) noexcept {
            this->x /= rhs.x;
            this->y /= rhs.y;
            this->z /= rhs.z;
            return *this;
        }

        int3 & int3::operator/=(int rhs) noexcept {
            this->x /= rhs;
            this->y /= rhs;
            this->z /= rhs;
            return *this;
        }

        int3 & int3::operator*=(const int3 & rhs) noexcept {
            this->x *= rhs.x;
            this->y *= rhs.y;
            this->z *= rhs.z;
            return *this;
        }

        int3 & int3::operator*=(int rhs) noexcept {
            this->x *= rhs;
            this->y *= rhs;
            this->z *= rhs;
            return *this;
        }

        int3 & int3::operator%=(const int3 & rhs) noexcept {
            this->x %= rhs.x;
            this->y %= rhs.y;
            this->z %= rhs.z;
            return *this;
        }

        int3 & int3::operator%=(int rhs) noexcept {
            this->x %= rhs;
            this->y %= rhs;
            this->z %= rhs;
            return *this;
        }

        int3 & int3::operator|=(const int3 & rhs) noexcept {
            this->x |= rhs.x;
            this->y |= rhs.y;
            this->z |= rhs.z;
            return *this;
        }

        int3 & int3::operator|=(int rhs) noexcept {
            this->x |= rhs;
            this->y |= rhs;
            this->z |= rhs;
            return *this;
        }

        int3 & int3::operator&=(const int3 & rhs) noexcept {
            this->x &= rhs.x;
            this->y &= rhs.y;
            this->z &= rhs.z;
            return *this;
        }

        int3 & int3::operator&=(int rhs) noexcept {
            this->x &= rhs;
            this->y &= rhs;
            this->z &= rhs;
            return *this;
        }

        int3 & int3::operator^=(const int3 & rhs) noexcept {
            this->x ^= rhs.x;
            this->y ^= rhs.y;
            this->z ^= rhs.z;
            return *this;
        }

        int3 & int3::operator^=(int rhs) noexcept {
            this->x ^= rhs;
            this->y ^= rhs;
            this->z ^= rhs;
            return *this;
        }

        int3 & int3::operator<<=(const int3 & rhs) noexcept {
            this->x <<= rhs.x;
            this->y <<= rhs.y;
            this->z <<= rhs.z;
            return *this;
        }

        int3 & int3::operator<<=(int rhs) noexcept {
            this->x <<= rhs;
            this->y <<= rhs;
            this->z <<= rhs;
            return *this;
        }

        int3 & int3::operator>>=(const int3 & rhs) noexcept {
            this->x >>= rhs.x;
            this->y >>= rhs.y;
            this->z >>= rhs.z;
            return *this;
        }

        int3 & int3::operator>>=(int rhs) noexcept {
            this->x >>= rhs;
            this->y >>= rhs;
            this->z >>= rhs;
            return *this;
        }

        int3 int3::operator*(const int3 & rhs) const noexcept {
            return int3 { this->x * rhs.x, this->y * rhs.y, this->z * rhs.z };
        }

        int3 int3::operator/(const int3 & rhs) const noexcept {
            return int3 { this->x / rhs.x, this->y / rhs.y, this->z / rhs.z };
        }

        int3 int3::operator+(const int3 & rhs) const noexcept {
            return int3 { this->x + rhs.x, this->y + rhs.y, this->z + rhs.z };
        }

        int3 int3::operator-(const int3 & rhs) const noexcept {
            return int3 { this->x - rhs.x, this->y - rhs.y, this->z - rhs.z };
        }

        int3 int3::operator%(const int3 & rhs) const noexcept {
            return int3 { this->x % rhs.x, this->y % rhs.y, this->z % rhs.z };
        }

        int3 int3::operator|(const int3 & rhs) const noexcept {
            return int3 { this->x | rhs.x, this->y | rhs.y, this->z | rhs.z };
        }

        int3 int3::operator&(const int3 & rhs) const noexcept {
            return int3 { this->x & rhs.x, this->y & rhs.y, this->z & rhs.z };
        }

        int3 int3::operator^(const int3 & rhs) const noexcept {
            return int3 { this->x ^ rhs.x, this->y ^ rhs.y, this->z ^ rhs.z };
        }

        int3 int3::operator<<(const int3 & rhs) const noexcept {
            return int3 { this->x << rhs.x, this->y << rhs.y, this->z << rhs.z };
        }

        int3 int3::operator>>(const int3 & rhs) const noexcept {
            return int3 { this->x >> rhs.x, this->y >> rhs.y, this->z >> rhs.z };
        }

        int3 int3::operator||(const int3 & rhs) const noexcept {
            return int3 { this->x || rhs.x, this->y || rhs.y, this->z || rhs.z };
        }

        int3 int3::operator&&(const int3 & rhs) const noexcept {
            return int3 { this->x && rhs.x, this->y && rhs.y, this->z && rhs.z };
        }

        bool3 int3::operator<(const int3 & rhs) const noexcept {
            return bool3 { x < rhs.x, y < rhs.y, z < rhs.z };
        }

        bool3 int3::operator>(const int3 & rhs) const noexcept {
            return bool3 { x > rhs.x, y > rhs.y, z > rhs.z };
        }

        bool3 int3::operator!=(const int3 & rhs) const noexcept {
            return bool3 { x != rhs.x, y != rhs.y, z != rhs.z };
        }

        bool3 int3::operator==(const int3 & rhs) const noexcept {
            return bool3 { x == rhs.x, y == rhs.y, z == rhs.z };
        }

        bool3 int3::operator>=(const int3 & rhs) const noexcept {
            return bool3 { x >= rhs.x, y >= rhs.y, z >= rhs.z };
        }

        bool3 int3::operator<=(const int3 & rhs) const noexcept {
            return bool3 { x <= rhs.x, y <= rhs.y, z <= rhs.z };
        }

        int3 int3::operator~() const noexcept {
            return int3 { ~x, ~y, ~z };
        }

        int3 int3::operator!() const noexcept {
            return int3 { !x, !y, !z };
        }

        int3 int3::operator++() noexcept {
            return int3 { ++x, ++y, ++z };
        }

        int3 int3::operator++(int) noexcept {
            return int3 { x++, y++, z++ };
        }

        int3 int3::operator--() noexcept {
            return int3 { --x, --y, --z };
        }

        int3 int3::operator--(int) noexcept {
            return int3 { x--, y--, z-- };
        }

        int3 int3::Random(int lower, int upper) noexcept {
            int range = upper - lower + 1;
             return int3 {  rand() % range + lower,
             rand() % range + lower,
             rand() % range + lower };
        }

        int3 int3::operator-() const noexcept {
            return int3 { -x, -y, -z };
        }

        const int3 int3::One { 1, 1, 1 };
        const int3 int3::Zero { 0, 0, 0 };
        const int3 int3::UnitX { 1, 0, 0 };
        const int3 int3::UnitY { 0, 1, 0 };
        const int3 int3::UnitZ { 0, 0, 1 };
    }
}

