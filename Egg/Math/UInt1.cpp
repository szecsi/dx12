#include "uint1.h"
#include <cmath>

namespace Egg {
    namespace Math {

        uint1::uint1(unsigned int x) : x { x }{ }

        uint1::uint1() : x{ 0U }{ }

        uint1 & uint1::operator=(const uint1 & rhs) noexcept {
            this->x = rhs.x;
            return *this;
        }

        uint1 & uint1::operator=(unsigned int rhs) noexcept {
            this->x = rhs;
            return *this;
        }

        uint1 & uint1::operator+=(const uint1 & rhs) noexcept {
            this->x += rhs.x;
            return *this;
        }

        uint1 & uint1::operator+=(unsigned int rhs) noexcept {
            this->x += rhs;
            return *this;
        }

        uint1 & uint1::operator-=(const uint1 & rhs) noexcept {
            this->x -= rhs.x;
            return *this;
        }

        uint1 & uint1::operator-=(unsigned int rhs) noexcept {
            this->x -= rhs;
            return *this;
        }

        uint1 & uint1::operator/=(const uint1 & rhs) noexcept {
            this->x /= rhs.x;
            return *this;
        }

        uint1 & uint1::operator/=(unsigned int rhs) noexcept {
            this->x /= rhs;
            return *this;
        }

        uint1 & uint1::operator*=(const uint1 & rhs) noexcept {
            this->x *= rhs.x;
            return *this;
        }

        uint1 & uint1::operator*=(unsigned int rhs) noexcept {
            this->x *= rhs;
            return *this;
        }

        uint1 & uint1::operator%=(const uint1 & rhs) noexcept {
            this->x %= rhs.x;
            return *this;
        }

        uint1 & uint1::operator%=(unsigned int rhs) noexcept {
            this->x %= rhs;
            return *this;
        }

        uint1 & uint1::operator|=(const uint1 & rhs) noexcept {
            this->x |= rhs.x;
            return *this;
        }

        uint1 & uint1::operator|=(unsigned int rhs) noexcept {
            this->x |= rhs;
            return *this;
        }

        uint1 & uint1::operator&=(const uint1 & rhs) noexcept {
            this->x &= rhs.x;
            return *this;
        }

        uint1 & uint1::operator&=(unsigned int rhs) noexcept {
            this->x &= rhs;
            return *this;
        }

        uint1 & uint1::operator^=(const uint1 & rhs) noexcept {
            this->x ^= rhs.x;
            return *this;
        }

        uint1 & uint1::operator^=(unsigned int rhs) noexcept {
            this->x ^= rhs;
            return *this;
        }

        uint1 & uint1::operator<<=(const uint1 & rhs) noexcept {
            this->x <<= rhs.x;
            return *this;
        }

        uint1 & uint1::operator<<=(unsigned int rhs) noexcept {
            this->x <<= rhs;
            return *this;
        }

        uint1 & uint1::operator>>=(const uint1 & rhs) noexcept {
            this->x >>= rhs.x;
            return *this;
        }

        uint1 & uint1::operator>>=(unsigned int rhs) noexcept {
            this->x >>= rhs;
            return *this;
        }

        uint1 uint1::operator*(const uint1 & rhs) const noexcept {
            return uint1 { this->x * rhs.x };
        }

        uint1 uint1::operator/(const uint1 & rhs) const noexcept {
            return uint1 { this->x / rhs.x };
        }

        uint1 uint1::operator+(const uint1 & rhs) const noexcept {
            return uint1 { this->x + rhs.x };
        }

        uint1 uint1::operator-(const uint1 & rhs) const noexcept {
            return uint1 { this->x - rhs.x };
        }

        uint1 uint1::operator%(const uint1 & rhs) const noexcept {
            return uint1 { this->x % rhs.x };
        }

        uint1 uint1::operator|(const uint1 & rhs) const noexcept {
            return uint1 { this->x | rhs.x };
        }

        uint1 uint1::operator&(const uint1 & rhs) const noexcept {
            return uint1 { this->x & rhs.x };
        }

        uint1 uint1::operator^(const uint1 & rhs) const noexcept {
            return uint1 { this->x ^ rhs.x };
        }

        uint1 uint1::operator<<(const uint1 & rhs) const noexcept {
            return uint1 { this->x << rhs.x };
        }

        uint1 uint1::operator>>(const uint1 & rhs) const noexcept {
            return uint1 { this->x >> rhs.x };
        }

        uint1 uint1::operator||(const uint1 & rhs) const noexcept {
            return uint1 { this->x || rhs.x };
        }

        uint1 uint1::operator&&(const uint1 & rhs) const noexcept {
            return uint1 { this->x && rhs.x };
        }

        bool1 uint1::operator<(const uint1 & rhs) const noexcept {
            return bool1 { x < rhs.x };
        }

        bool1 uint1::operator>(const uint1 & rhs) const noexcept {
            return bool1 { x > rhs.x };
        }

        bool1 uint1::operator!=(const uint1 & rhs) const noexcept {
            return bool1 { x != rhs.x };
        }

        bool1 uint1::operator==(const uint1 & rhs) const noexcept {
            return bool1 { x == rhs.x };
        }

        bool1 uint1::operator>=(const uint1 & rhs) const noexcept {
            return bool1 { x >= rhs.x };
        }

        bool1 uint1::operator<=(const uint1 & rhs) const noexcept {
            return bool1 { x <= rhs.x };
        }

        uint1 uint1::operator~() const noexcept {
            return uint1 { ~x };
        }

        uint1 uint1::operator!() const noexcept {
            return uint1 { !x };
        }

        uint1 uint1::operator++() noexcept {
            return uint1 { ++x };
        }

        uint1 uint1::operator++(int) noexcept {
            return uint1 { x++ };
        }

        uint1 uint1::operator--() noexcept {
            return uint1 { --x };
        }

        uint1 uint1::operator--(int) noexcept {
            return uint1 { x-- };
        }

        uint1 uint1::Random(unsigned int lower, unsigned int upper) noexcept {
            unsigned int range = upper - lower + 1;
             return uint1 {  rand() % range + lower };
        }

    }
}

