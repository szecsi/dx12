#include "int1.h"
#include <cmath>

namespace Egg {
    namespace Math {

        int1::int1(int x) : x { x }{ }

        int1::int1() : x{ 0 }{ }

        int1 & int1::operator=(const int1 & rhs) noexcept {
            this->x = rhs.x;
            return *this;
        }

        int1 & int1::operator=(int rhs) noexcept {
            this->x = rhs;
            return *this;
        }

        int1 & int1::operator+=(const int1 & rhs) noexcept {
            this->x += rhs.x;
            return *this;
        }

        int1 & int1::operator+=(int rhs) noexcept {
            this->x += rhs;
            return *this;
        }

        int1 & int1::operator-=(const int1 & rhs) noexcept {
            this->x -= rhs.x;
            return *this;
        }

        int1 & int1::operator-=(int rhs) noexcept {
            this->x -= rhs;
            return *this;
        }

        int1 & int1::operator/=(const int1 & rhs) noexcept {
            this->x /= rhs.x;
            return *this;
        }

        int1 & int1::operator/=(int rhs) noexcept {
            this->x /= rhs;
            return *this;
        }

        int1 & int1::operator*=(const int1 & rhs) noexcept {
            this->x *= rhs.x;
            return *this;
        }

        int1 & int1::operator*=(int rhs) noexcept {
            this->x *= rhs;
            return *this;
        }

        int1 & int1::operator%=(const int1 & rhs) noexcept {
            this->x %= rhs.x;
            return *this;
        }

        int1 & int1::operator%=(int rhs) noexcept {
            this->x %= rhs;
            return *this;
        }

        int1 & int1::operator|=(const int1 & rhs) noexcept {
            this->x |= rhs.x;
            return *this;
        }

        int1 & int1::operator|=(int rhs) noexcept {
            this->x |= rhs;
            return *this;
        }

        int1 & int1::operator&=(const int1 & rhs) noexcept {
            this->x &= rhs.x;
            return *this;
        }

        int1 & int1::operator&=(int rhs) noexcept {
            this->x &= rhs;
            return *this;
        }

        int1 & int1::operator^=(const int1 & rhs) noexcept {
            this->x ^= rhs.x;
            return *this;
        }

        int1 & int1::operator^=(int rhs) noexcept {
            this->x ^= rhs;
            return *this;
        }

        int1 & int1::operator<<=(const int1 & rhs) noexcept {
            this->x <<= rhs.x;
            return *this;
        }

        int1 & int1::operator<<=(int rhs) noexcept {
            this->x <<= rhs;
            return *this;
        }

        int1 & int1::operator>>=(const int1 & rhs) noexcept {
            this->x >>= rhs.x;
            return *this;
        }

        int1 & int1::operator>>=(int rhs) noexcept {
            this->x >>= rhs;
            return *this;
        }

        int1 int1::operator*(const int1 & rhs) const noexcept {
            return int1 { this->x * rhs.x };
        }

        int1 int1::operator/(const int1 & rhs) const noexcept {
            return int1 { this->x / rhs.x };
        }

        int1 int1::operator+(const int1 & rhs) const noexcept {
            return int1 { this->x + rhs.x };
        }

        int1 int1::operator-(const int1 & rhs) const noexcept {
            return int1 { this->x - rhs.x };
        }

        int1 int1::operator%(const int1 & rhs) const noexcept {
            return int1 { this->x % rhs.x };
        }

        int1 int1::operator|(const int1 & rhs) const noexcept {
            return int1 { this->x | rhs.x };
        }

        int1 int1::operator&(const int1 & rhs) const noexcept {
            return int1 { this->x & rhs.x };
        }

        int1 int1::operator^(const int1 & rhs) const noexcept {
            return int1 { this->x ^ rhs.x };
        }

        int1 int1::operator<<(const int1 & rhs) const noexcept {
            return int1 { this->x << rhs.x };
        }

        int1 int1::operator>>(const int1 & rhs) const noexcept {
            return int1 { this->x >> rhs.x };
        }

        int1 int1::operator||(const int1 & rhs) const noexcept {
            return int1 { this->x || rhs.x };
        }

        int1 int1::operator&&(const int1 & rhs) const noexcept {
            return int1 { this->x && rhs.x };
        }

        bool1 int1::operator<(const int1 & rhs) const noexcept {
            return bool1 { x < rhs.x };
        }

        bool1 int1::operator>(const int1 & rhs) const noexcept {
            return bool1 { x > rhs.x };
        }

        bool1 int1::operator!=(const int1 & rhs) const noexcept {
            return bool1 { x != rhs.x };
        }

        bool1 int1::operator==(const int1 & rhs) const noexcept {
            return bool1 { x == rhs.x };
        }

        bool1 int1::operator>=(const int1 & rhs) const noexcept {
            return bool1 { x >= rhs.x };
        }

        bool1 int1::operator<=(const int1 & rhs) const noexcept {
            return bool1 { x <= rhs.x };
        }

        int1 int1::operator~() const noexcept {
            return int1 { ~x };
        }

        int1 int1::operator!() const noexcept {
            return int1 { !x };
        }

        int1 int1::operator++() noexcept {
            return int1 { ++x };
        }

        int1 int1::operator++(int) noexcept {
            return int1 { x++ };
        }

        int1 int1::operator--() noexcept {
            return int1 { --x };
        }

        int1 int1::operator--(int) noexcept {
            return int1 { x-- };
        }

        int1 int1::Random(int lower, int upper) noexcept {
            int range = upper - lower + 1;
             return int1 {  rand() % range + lower };
        }

        int1 int1::operator-() const noexcept {
            return int1 { -x };
        }

    }
}

